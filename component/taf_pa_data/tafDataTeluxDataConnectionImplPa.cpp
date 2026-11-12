/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataConnectionImpl.cpp
 * @brief Telux Data connection implementation.
 *
 */

#include "tafDataUtilsPa.hpp"
#include "tafDataTeluxDataPa.hpp"
#include "tafDataTeluxDataConnectionPa.hpp"

#include <arpa/inet.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <sstream>
#include <thread>
#include <utility>

using namespace taf::pa::data;

/*
 * TelSDK to PA mapping:
 *   telux::data::TCPKAParams          -> TcpKeepAliveParams_t
 *   telux::data::MonitorHandleType    -> TcpMonitorHandle_t
 *   telux::data::TCPKAOffloadHandle   -> TcpKeepAliveOffloadHandle_t
 *   telux::data::DataRestrictMode     -> FilterModeInfo_t
 *   telux::data::DataRestrictModeType -> FilterMode_e
 *   telux::common::ResponseCallback   -> taf_pa_data_DataRestrictResponseCb
 *   telux::data::DataRestrictModeCb   -> taf_pa_data_DataRestrictModeResponseCb
 *   telux::data::IIpFilter variants   -> IpFilter_t and protocol-specific fields
 *   telux::common::ErrorCode/Status   -> taf_pa_result_t via Utils conversion helpers
 */
namespace
{

constexpr uint32_t IPV6_FLOW_LABEL_MAX = 0xFFFFF;

taf_pa_result_t GetTeluxDataCallStatus
(
    const std::shared_ptr<telux::data::IDataCall> &dataCall,
    telux::data::DataCallStatus &callStatus
)
{
    if (!dataCall)
    {
        return TAF_PA_BAD_PARAMETER;
    }

    try
    {
        callStatus = dataCall->getDataCallStatus();
    }
    catch (const std::exception &error)
    {
        TAF_PA_ERROR("Failed to get TelSDK data-call status: %s", error.what());
        return TAF_PA_FAULT;
    }
    catch (...)
    {
        TAF_PA_ERROR("Unknown exception getting TelSDK data-call status");
        return TAF_PA_FAULT;
    }

    return TAF_PA_OK;
}

taf_pa_result_t ConvertServiceStatus(telux::common::ServiceStatus status)
{
    switch (status)
    {
    case telux::common::ServiceStatus::SERVICE_AVAILABLE:
        return TAF_PA_OK;
    case telux::common::ServiceStatus::SERVICE_UNAVAILABLE:
        return TAF_PA_UNAVAILABLE;
    case telux::common::ServiceStatus::SERVICE_FAILED:
    default:
        return TAF_PA_FAULT;
    }
}

bool IsValidIpAddress(const std::string &address, int &family)
{
    uint8_t buffer[sizeof(struct in6_addr)] = {};
    if (inet_pton(AF_INET, address.c_str(), buffer) == 1)
    {
        family = AF_INET;
        return true;
    }
    if (inet_pton(AF_INET6, address.c_str(), buffer) == 1)
    {
        family = AF_INET6;
        return true;
    }
    return false;
}

bool IsValidAddress(const std::string &address, int family)
{
    uint8_t buffer[sizeof(struct in6_addr)] = {};
    return inet_pton(family, address.c_str(), buffer) == 1;
}

telux::data::DataRestrictModeType ConvertDataRestrictModeType(FilterMode_e mode)
{
    switch (mode)
    {
    case FilterMode_e::DISABLE:
        return telux::data::DataRestrictModeType::DISABLE;
    case FilterMode_e::ENABLE:
        return telux::data::DataRestrictModeType::ENABLE;
    case FilterMode_e::UNKNOWN:
    default:
        return telux::data::DataRestrictModeType::UNKNOWN;
    }
}

FilterMode_e ConvertDataRestrictModeType(telux::data::DataRestrictModeType mode)
{
    switch (mode)
    {
    case telux::data::DataRestrictModeType::DISABLE:
        return FilterMode_e::DISABLE;
    case telux::data::DataRestrictModeType::ENABLE:
        return FilterMode_e::ENABLE;
    case telux::data::DataRestrictModeType::UNKNOWN:
    default:
        return FilterMode_e::UNKNOWN;
    }
}

void AppendOptionalPrefix(std::ostringstream &stream, const char *name, bool hasValue)
{
    stream << name << '=';
    if (!hasValue)
    {
        stream << "<unset>;";
    }
}

} // namespace

taf::pa::data::TafPaTeluxDataConnection &taf::pa::data::TafPaTeluxDataConnection::GetInstance()
{
    static TafPaTeluxDataConnection instance;
    return instance;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetSubsysState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e &sState
)
{
    // Initialize to FAILED state
    sState = taf::pa::data::SubsystemState_e::FAILED;

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    {
        std::shared_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
        sState = dataConnectionManagersSubsysStateMap_[slotId];
    }
    TAF_PA_INFO("Conn init state for slot id[%d]: %d", slotId, TO_INT(sState));
    return TAF_PA_OK;
}

TafPaTeluxKeepAlive &TafPaTeluxKeepAlive::GetInstance()
{
    static TafPaTeluxKeepAlive instance;
    return instance;
}

taf_pa_result_t TafPaTeluxKeepAlive::Init()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_ && !keepAliveManagersMap_.empty())
        {
            // Check if all managers are available
            bool allAvailable = true;
            for (const auto &entry : keepAliveManagersMap_)
            {
                if (!entry.second ||
                    entry.second->getServiceStatus() != telux::common::ServiceStatus::SERVICE_AVAILABLE)
                {
                    allAvailable = false;
                    break;
                }
            }
            if (allAvailable)
            {
                return TAF_PA_OK;
            }
        }
    }

    auto &dataFactory = telux::data::DataFactory::getInstance();

    // Initialize KeepAlive managers for both slots
    std::vector<int> failedSlots;
    std::vector<int> successfulSlots;

    TAF_PA_INFO("Starting KeepAlive manager initialization for slots");

    for (int slotId = 1; slotId <= MAX_SLOT_NUM; slotId++)
    {
        auto initPromise = std::make_shared<std::promise<telux::common::ServiceStatus>>();
        std::future<telux::common::ServiceStatus> initFuture = initPromise->get_future();

        auto manager = dataFactory.getKeepAliveManager(
            (SlotId)slotId,
            [initPromise, slotId](telux::common::ServiceStatus status)
            {
                SET_SDK_THREAD_NAME();
                TAF_PA_INFO("KeepAlive manager callback for slot %d, status: %d", slotId, TO_INT(status));
                try
                {
                    initPromise->set_value(status);
                }
                catch (const std::future_error &error)
                {
                    TAF_PA_WARN("KeepAlive init callback completed more than once for slot %d: %s",
                            slotId, error.what());
                }
            });

        if (!manager)
        {
            TAF_PA_ERROR("Failed to get KeepAlive manager instance for slot %d", slotId);
            failedSlots.push_back(slotId);
            continue;
        }

        telux::common::ServiceStatus status = manager->getServiceStatus();
        if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            std::future_status waitStatus = initFuture.wait_for(
                std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT));
            if (waitStatus == std::future_status::ready)
            {
                status = initFuture.get();
            }
        }

        if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            keepAliveManagersMap_[(SlotId)slotId] = manager;
            successfulSlots.push_back(slotId);
            TAF_PA_INFO("KeepAlive manager initialized successfully for slot %d", slotId);
        }
        else
        {
            TAF_PA_WARN("KeepAlive manager not available for slot %d. Status: %d", slotId, TO_INT(status));
            failedSlots.push_back(slotId);
        }
    }

    // Log summary
    TAF_PA_INFO("=== KeepAlive Manager Initialization Summary ===");
    TAF_PA_INFO("Successfully initialized: %zu slot(s)", successfulSlots.size());
    if (!successfulSlots.empty())
    {
        for (auto slot : successfulSlots)
        {
            TAF_PA_INFO("  - Slot %d: AVAILABLE", slot);
        }
    }
    if (!failedSlots.empty())
    {
        TAF_PA_WARN("Failed to initialize: %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_WARN("  - Slot %d: FAILED", slot);
        }
    }
    TAF_PA_INFO("================================================");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = !keepAliveManagersMap_.empty();
    }

    return keepAliveManagersMap_.empty() ? TAF_PA_UNAVAILABLE : TAF_PA_OK;
}

taf_pa_result_t TafPaTeluxKeepAlive::Deinit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (keepAliveManagersMap_.empty())
    {
        monitorHandles_.clear();
        offloadHandles_.clear();
        initialized_ = false;
        return TAF_PA_OK;
    }

    // Clean up offload handles using the first available manager
    // (all managers should have the same handles since they're global)
    if (!keepAliveManagersMap_.empty())
    {
        auto manager = keepAliveManagersMap_.begin()->second;
        for (const auto &entry : offloadHandles_)
        {
            telux::common::ErrorCode error =
                manager->stopTCPKeepAliveOffload(entry.first);
            if (error != telux::common::ErrorCode::SUCCESS)
            {
                TAF_PA_WARN("Failed to stop keep-alive offload %u during deinit: %d",
                        entry.first, TO_INT(error));
            }
        }
    }
    offloadHandles_.clear();

    // Clean up monitor handles using the first available manager
    if (!keepAliveManagersMap_.empty())
    {
        auto manager = keepAliveManagersMap_.begin()->second;
        for (auto monHandle : monitorHandles_)
        {
            telux::common::ErrorCode error = manager->disableTCPMonitor(monHandle);
            if (error != telux::common::ErrorCode::SUCCESS)
            {
                TAF_PA_WARN("Failed to disable TCP monitor %u during deinit: %d",
                        monHandle, TO_INT(error));
            }
        }
    }
    monitorHandles_.clear();

    keepAliveManagersMap_.clear();
    initialized_ = false;
    return TAF_PA_OK;
}


bool TafPaTeluxKeepAlive::IsValidTcpKaParams(const TcpKeepAliveParams_t &tcpKaParams)
{
    int sourceFamily = AF_UNSPEC;
    int destinationFamily = AF_UNSPEC;

    if (tcpKaParams.sourcePort == 0 || tcpKaParams.destinationPort == 0)
    {
        TAF_PA_ERROR("TCP keep-alive source and destination ports must be non-zero");
        return false;
    }
    if (!IsValidIpAddress(tcpKaParams.sourceAddress, sourceFamily))
    {
        TAF_PA_ERROR("Invalid TCP keep-alive source address: %s", tcpKaParams.sourceAddress.c_str());
        return false;
    }
    if (!IsValidIpAddress(tcpKaParams.destinationAddress, destinationFamily))
    {
        TAF_PA_ERROR("Invalid TCP keep-alive destination address: %s",
                 tcpKaParams.destinationAddress.c_str());
        return false;
    }
    if (sourceFamily != destinationFamily)
    {
        TAF_PA_ERROR("TCP keep-alive source and destination address families differ");
        return false;
    }
    return true;
}

telux::data::TCPKAParams TafPaTeluxKeepAlive::ConvertTcpKaParams(
    const TcpKeepAliveParams_t &tcpKaParams)
{
    telux::data::TCPKAParams sdkParams{};
    sdkParams.srcIp = tcpKaParams.sourceAddress;
    sdkParams.dstIp = tcpKaParams.destinationAddress;
    sdkParams.srcPort = tcpKaParams.sourcePort;
    sdkParams.dstPort = tcpKaParams.destinationPort;
    return sdkParams;
}

taf_pa_result_t TafPaTeluxKeepAlive::PaEnableTCPMonitor(
    taf::pa::data::SlotId_e slotId,
    const TcpKeepAliveParams_t &tcpKaParams,
    TcpMonitorHandle_t &monHandle)
{
    monHandle = INVALID_TCP_MONITOR_HANDLE;
    TAF_PA_ERROR_IF_RET_VAL(!IsValidTcpKaParams(tcpKaParams), TAF_PA_BAD_PARAMETER,
                            "Invalid TCP keep-alive parameters");

    std::lock_guard<std::mutex> lock(mutex_);
    auto managerIt = keepAliveManagersMap_.find((SlotId)slotId);
    if (managerIt == keepAliveManagersMap_.end())
    {
        TAF_PA_ERROR("No KeepAlive manager available for slot %d", TO_INT(slotId));
        return TAF_PA_NOT_FOUND;
    }
    auto manager = managerIt->second;
    telux::data::MonitorHandleType sdkHandle = INVALID_TCP_MONITOR_HANDLE;
    telux::common::ErrorCode error =
        manager->enableTCPMonitor(ConvertTcpKaParams(tcpKaParams), sdkHandle);
    taf_pa_result_t result = Utils::ConvertErrorCode(error);
    if (result != TAF_PA_OK)
    {
        TAF_PA_ERROR("enableTCPMonitor failed. ErrorCode: %d", TO_INT(error));
        return result;
    }
    if (sdkHandle == INVALID_TCP_MONITOR_HANDLE)
    {
        TAF_PA_ERROR("enableTCPMonitor returned an invalid monitor handle");
        return TAF_PA_FAULT;
    }

    monHandle = sdkHandle;
    monitorHandles_.insert(monHandle);
    return TAF_PA_OK;
}

taf_pa_result_t TafPaTeluxKeepAlive::PaDisableTCPMonitor(taf::pa::data::SlotId_e slotId, TcpMonitorHandle_t monHandle)
{
    TAF_PA_ERROR_IF_RET_VAL(monHandle == INVALID_TCP_MONITOR_HANDLE, TAF_PA_BAD_PARAMETER,
                            "Invalid TCP monitor handle");

    std::lock_guard<std::mutex> lock(mutex_);
    if (monitorHandles_.find(monHandle) == monitorHandles_.end())
    {
        TAF_PA_WARN("Unknown TCP monitor handle: %u", monHandle);
        return TAF_PA_NOT_FOUND;
    }

    auto managerIt = keepAliveManagersMap_.find((SlotId)slotId);
    if (managerIt == keepAliveManagersMap_.end())
    {
        TAF_PA_ERROR("No KeepAlive manager available for slot %d", TO_INT(slotId));
        return TAF_PA_NOT_FOUND;
    }
    auto manager = managerIt->second;
    auto offload = std::find_if(offloadHandles_.begin(), offloadHandles_.end(),
        [monHandle](const auto &entry) { return entry.second == monHandle; });
    if (offload != offloadHandles_.end())
    {
        telux::common::ErrorCode stopError =
            manager->stopTCPKeepAliveOffload(offload->first);
        taf_pa_result_t result = Utils::ConvertErrorCode(stopError);
        if (result != TAF_PA_OK)
        {
            TAF_PA_ERROR("Failed to stop keep-alive offload %u before disabling monitor %u: %d",
                     offload->first, monHandle, TO_INT(stopError));
            return result;
        }
        offloadHandles_.erase(offload);
    }

    telux::common::ErrorCode error = manager->disableTCPMonitor(monHandle);
    taf_pa_result_t result = Utils::ConvertErrorCode(error);
    if (result != TAF_PA_OK)
    {
        TAF_PA_ERROR("disableTCPMonitor failed for handle %u. ErrorCode: %d",
                 monHandle, TO_INT(error));
        return result;
    }

    monitorHandles_.erase(monHandle);
    return TAF_PA_OK;
}

taf_pa_result_t TafPaTeluxKeepAlive::PaStartTCPKeepAliveOffload(
    taf::pa::data::SlotId_e slotId,
    TcpMonitorHandle_t monHandle,
    uint32_t interval,
    TcpKeepAliveOffloadHandle_t &handle)
{
    handle = INVALID_TCP_KEEP_ALIVE_OFFLOAD_HANDLE;
    TAF_PA_ERROR_IF_RET_VAL(monHandle == INVALID_TCP_MONITOR_HANDLE, TAF_PA_BAD_PARAMETER,
                            "Invalid TCP monitor handle");
    TAF_PA_ERROR_IF_RET_VAL(interval == 0, TAF_PA_BAD_PARAMETER,
                            "TCP keep-alive interval must be non-zero");

    std::lock_guard<std::mutex> lock(mutex_);
    if (monitorHandles_.find(monHandle) == monitorHandles_.end())
    {
        TAF_PA_WARN("Unknown TCP monitor handle: %u", monHandle);
        return TAF_PA_NOT_FOUND;
    }

    auto existing = std::find_if(offloadHandles_.begin(), offloadHandles_.end(),
        [monHandle](const auto &entry) { return entry.second == monHandle; });
    if (existing != offloadHandles_.end())
    {
        TAF_PA_WARN("TCP monitor %u already has offload handle %u", monHandle, existing->first);
        return TAF_PA_DUPLICATE;
    }

    auto managerIt = keepAliveManagersMap_.find((SlotId)slotId);
    if (managerIt == keepAliveManagersMap_.end())
    {
        TAF_PA_ERROR("No KeepAlive manager available for slot %d", TO_INT(slotId));
        return TAF_PA_NOT_FOUND;
    }
    auto manager = managerIt->second;
    telux::data::TCPKAOffloadHandle sdkHandle = INVALID_TCP_KEEP_ALIVE_OFFLOAD_HANDLE;
    telux::common::ErrorCode error =
        manager->startTCPKeepAliveOffload(monHandle, interval, sdkHandle);
    taf_pa_result_t result = Utils::ConvertErrorCode(error);
    if (result != TAF_PA_OK)
    {
        TAF_PA_ERROR("startTCPKeepAliveOffload failed for monitor %u. ErrorCode: %d",
                 monHandle, TO_INT(error));
        return result;
    }
    if (sdkHandle == INVALID_TCP_KEEP_ALIVE_OFFLOAD_HANDLE)
    {
        TAF_PA_ERROR("startTCPKeepAliveOffload returned an invalid offload handle");
        return TAF_PA_FAULT;
    }

    handle = sdkHandle;
    offloadHandles_[handle] = monHandle;
    return TAF_PA_OK;
}

taf_pa_result_t TafPaTeluxKeepAlive::PaStopTCPKeepAliveOffload(taf::pa::data::SlotId_e slotId, TcpKeepAliveOffloadHandle_t handle)
{
    TAF_PA_ERROR_IF_RET_VAL(handle == INVALID_TCP_KEEP_ALIVE_OFFLOAD_HANDLE, TAF_PA_BAD_PARAMETER,
                            "Invalid TCP keep-alive offload handle");

    std::lock_guard<std::mutex> lock(mutex_);
    auto offload = offloadHandles_.find(handle);
    if (offload == offloadHandles_.end())
    {
        TAF_PA_WARN("Unknown TCP keep-alive offload handle: %u", handle);
        return TAF_PA_NOT_FOUND;
    }

    auto managerIt = keepAliveManagersMap_.find((SlotId)slotId);
    if (managerIt == keepAliveManagersMap_.end())
    {
        TAF_PA_ERROR("No KeepAlive manager available for slot %d", TO_INT(slotId));
        return TAF_PA_NOT_FOUND;
    }
    auto manager = managerIt->second;
    telux::common::ErrorCode error = manager->stopTCPKeepAliveOffload(handle);
    taf_pa_result_t result = Utils::ConvertErrorCode(error);
    if (result != TAF_PA_OK)
    {
        TAF_PA_ERROR("stopTCPKeepAliveOffload failed for handle %u. ErrorCode: %d",
                 handle, TO_INT(error));
        return result;
    }

    offloadHandles_.erase(offload);
    return TAF_PA_OK;
}

TafPaTeluxDataFilter &TafPaTeluxDataFilter::GetInstance()
{
    static TafPaTeluxDataFilter instance;
    return instance;
}

taf_pa_result_t TafPaTeluxDataFilter::Init()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_ && !dataFilterManagersMap_.empty())
        {
            // Check if all managers are available
            bool allAvailable = true;
            for (const auto &entry : dataFilterManagersMap_)
            {
                if (!entry.second ||
                    entry.second->getServiceStatus() != telux::common::ServiceStatus::SERVICE_AVAILABLE)
                {
                    allAvailable = false;
                    break;
                }
            }
            if (allAvailable)
            {
                return TAF_PA_OK;
            }
        }
    }

    auto &dataFactory = telux::data::DataFactory::getInstance();

    // Initialize DataFilter managers for both slots
    std::vector<int> failedSlots;
    std::vector<int> successfulSlots;

    TAF_PA_INFO("Starting DataFilter manager initialization for slots");

    for (int slotId = 1; slotId <= MAX_SLOT_NUM; slotId++)
    {
        auto initPromise = std::make_shared<std::promise<telux::common::ServiceStatus>>();
        std::future<telux::common::ServiceStatus> initFuture = initPromise->get_future();

        auto manager = dataFactory.getDataFilterManager(
            (SlotId)slotId,
            [initPromise, slotId](telux::common::ServiceStatus status)
            {
                SET_SDK_THREAD_NAME();
                TAF_PA_INFO("DataFilter manager callback for slot %d, status: %d", slotId, TO_INT(status));
                try
                {
                    initPromise->set_value(status);
                }
                catch (const std::future_error &error)
                {
                    TAF_PA_WARN("DataFilter init callback completed more than once for slot %d: %s",
                            slotId, error.what());
                }
            });

        if (!manager)
        {
            TAF_PA_ERROR("Failed to get DataFilter manager instance for slot %d", slotId);
            failedSlots.push_back(slotId);
            continue;
        }

        telux::common::ServiceStatus status = manager->getServiceStatus();
        if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            std::future_status waitStatus = initFuture.wait_for(
                std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT));
            if (waitStatus == std::future_status::ready)
            {
                status = initFuture.get();
            }
        }

        if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            dataFilterManagersMap_[(SlotId)slotId] = manager;
            successfulSlots.push_back(slotId);
            TAF_PA_INFO("DataFilter manager initialized successfully for slot %d", slotId);
        }
        else
        {
            TAF_PA_WARN("DataFilter manager not available for slot %d. Status: %d", slotId, TO_INT(status));
            failedSlots.push_back(slotId);
        }
    }

    // Log summary
    TAF_PA_INFO("=== DataFilter Manager Initialization Summary ===");
    TAF_PA_INFO("Successfully initialized: %zu slot(s)", successfulSlots.size());
    if (!successfulSlots.empty())
    {
        for (auto slot : successfulSlots)
        {
            TAF_PA_INFO("  - Slot %d: AVAILABLE", slot);
        }
    }
    if (!failedSlots.empty())
    {
        TAF_PA_WARN("Failed to initialize: %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_WARN("  - Slot %d: FAILED", slot);
        }
    }
    TAF_PA_INFO("================================================");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = !dataFilterManagersMap_.empty();
        callbacksEnabled_.store(initialized_);
    }

    return dataFilterManagersMap_.empty() ? TAF_PA_UNAVAILABLE : TAF_PA_OK;
}

taf_pa_result_t TafPaTeluxDataFilter::Deinit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    callbacksEnabled_.store(false);

    taf_pa_result_t result = TAF_PA_OK;
    if (!dataFilterManagersMap_.empty())
    {
        // Clean up using the first available manager
        // (all managers should have the same global rules since they're global)
        auto manager = dataFilterManagersMap_.begin()->second;

        // Disable the modem-side mode before releasing the manager. The TelSDK manager does not
        // document filter lifetime across a manager/client restart, so explicitly remove the
        // global rules as well.
        telux::data::DataRestrictMode disableMode{};
        disableMode.filterMode = telux::data::DataRestrictModeType::DISABLE;
        disableMode.filterAutoExit = telux::data::DataRestrictModeType::DISABLE;

        telux::common::Status status = manager->setDataRestrictMode(disableMode);
        if (status != telux::common::Status::SUCCESS)
        {
            result = Utils::ConvertStatus(status);
            TAF_PA_WARN("Failed to disable data restrict mode during deinit. Status: %d",
                    TO_INT(status));
        }

        status = manager->removeAllDataRestrictFilters();
        if (status != telux::common::Status::SUCCESS)
        {
            if (result == TAF_PA_OK)
            {
                result = Utils::ConvertStatus(status);
            }
            TAF_PA_WARN("Failed to remove data restrict filters during deinit. Status: %d",
                    TO_INT(status));
        }
    }

    configuredFilterKeys_.clear();
    dataFilterManagersMap_.clear();
    initialized_ = false;
    return result;
}


bool TafPaTeluxDataFilter::IsValidMode(FilterMode_e mode)
{
    return mode == FilterMode_e::DISABLE || mode == FilterMode_e::ENABLE;
}

bool TafPaTeluxDataFilter::IsValidDataRestrictMode(const FilterModeInfo_t &mode)
{
    if (!IsValidMode(mode.filterMode))
    {
        TAF_PA_ERROR("Invalid data restrict filterMode: %d", TO_INT(mode.filterMode));
        return false;
    }
    if (!IsValidMode(mode.filterAutoExit))
    {
        TAF_PA_ERROR("Invalid data restrict filterAutoExit: %d", TO_INT(mode.filterAutoExit));
        return false;
    }
    return true;
}

bool TafPaTeluxDataFilter::IsValidPortInfo(const PortInfo_t &portInfo)
{
    if (portInfo.port == 0)
    {
        TAF_PA_ERROR("Port must be non-zero when a port filter is supplied");
        return false;
    }
    if (static_cast<uint32_t>(portInfo.port) + portInfo.range > UINT16_MAX)
    {
        TAF_PA_ERROR("Port range exceeds maximum TCP/UDP port value");
        return false;
    }
    return true;
}

bool TafPaTeluxDataFilter::IsValidFilter(const IpFilter_t &filter)
{
    switch (filter.protocol)
    {
    case IpProtocol_e::TCP:
    case IpProtocol_e::UDP:
        if (filter.icmp || filter.esp)
        {
            TAF_PA_ERROR("ICMP/ESP fields cannot be used with TCP/UDP data restrict filters");
            return false;
        }
        break;
    case IpProtocol_e::ICMP:
        if (filter.sourcePort || filter.destinationPort || filter.esp)
        {
            TAF_PA_ERROR("Port/ESP fields cannot be used with ICMP data restrict filters");
            return false;
        }
        break;
    case IpProtocol_e::ESP:
        if (filter.sourcePort || filter.destinationPort || filter.icmp)
        {
            TAF_PA_ERROR("Port/ICMP fields cannot be used with ESP data restrict filters");
            return false;
        }
        if (filter.esp && filter.esp->spi == 0)
        {
            TAF_PA_ERROR("ESP SPI must be non-zero when supplied");
            return false;
        }
        break;
    case IpProtocol_e::UNKNOWN:
    default:
        TAF_PA_ERROR("Invalid data restrict IP protocol: %d", TO_INT(filter.protocol));
        return false;
    }

    if (filter.profileId && *filter.profileId == ProfileId_e::INVALID)
    {
        TAF_PA_ERROR("Invalid data restrict profile ID");
        return false;
    }
    if (filter.ipFamilyType &&
        (*filter.ipFamilyType == IpType_e::UNKNOWN ||
         (*filter.ipFamilyType == IpType_e::IPV4 && filter.ipv6) ||
         (*filter.ipFamilyType == IpType_e::IPV6 && filter.ipv4)))
    {
        TAF_PA_ERROR("Invalid data restrict IP family combination");
        return false;
    }
    if (filter.sourcePort && !IsValidPortInfo(*filter.sourcePort))
    {
        return false;
    }
    if (filter.destinationPort && !IsValidPortInfo(*filter.destinationPort))
    {
        return false;
    }

    if (filter.ipv4)
    {
        if (filter.ipv4->source)
        {
            if (!IsValidAddress(filter.ipv4->source->address, AF_INET))
            {
                TAF_PA_ERROR("Invalid source IPv4 address: %s",
                         filter.ipv4->source->address.c_str());
                return false;
            }
            if (!filter.ipv4->source->subnetMask.empty() &&
                !IsValidAddress(filter.ipv4->source->subnetMask, AF_INET))
            {
                TAF_PA_ERROR("Invalid source IPv4 subnet mask: %s",
                         filter.ipv4->source->subnetMask.c_str());
                return false;
            }
        }
        if (filter.ipv4->destination)
        {
            if (!IsValidAddress(filter.ipv4->destination->address, AF_INET))
            {
                TAF_PA_ERROR("Invalid destination IPv4 address: %s",
                         filter.ipv4->destination->address.c_str());
                return false;
            }
            if (!filter.ipv4->destination->subnetMask.empty() &&
                !IsValidAddress(filter.ipv4->destination->subnetMask, AF_INET))
            {
                TAF_PA_ERROR("Invalid destination IPv4 subnet mask: %s",
                         filter.ipv4->destination->subnetMask.c_str());
                return false;
            }
        }
    }

    if (filter.ipv6)
    {
        if (filter.ipv6->source)
        {
            if (!IsValidAddress(filter.ipv6->source->address, AF_INET6))
            {
                TAF_PA_ERROR("Invalid source IPv6 address: %s",
                         filter.ipv6->source->address.c_str());
                return false;
            }
            if (filter.ipv6->source->prefixLength > 128)
            {
                TAF_PA_ERROR("Invalid source IPv6 prefix length: %u",
                         filter.ipv6->source->prefixLength);
                return false;
            }
        }
        if (filter.ipv6->destination)
        {
            if (!IsValidAddress(filter.ipv6->destination->address, AF_INET6))
            {
                TAF_PA_ERROR("Invalid destination IPv6 address: %s",
                         filter.ipv6->destination->address.c_str());
                return false;
            }
            if (filter.ipv6->destination->prefixLength > 128)
            {
                TAF_PA_ERROR("Invalid destination IPv6 prefix length: %u",
                         filter.ipv6->destination->prefixLength);
                return false;
            }
        }
        if (filter.ipv6->flowLabel && *filter.ipv6->flowLabel > IPV6_FLOW_LABEL_MAX)
        {
            TAF_PA_ERROR("Invalid IPv6 flow label: %u", *filter.ipv6->flowLabel);
            return false;
        }
        if (filter.ipv6->natEnabled && *filter.ipv6->natEnabled > 1)
        {
            TAF_PA_ERROR("Invalid IPv6 NAT enabled value: %u", *filter.ipv6->natEnabled);
            return false;
        }
    }

    return true;
}

std::string TafPaTeluxDataFilter::BuildFilterKey(const IpFilter_t &filter)
{
    std::ostringstream key;
    key << "profile=";
    if (filter.profileId)
    {
        key << TO_INT(*filter.profileId);
    }
    else
    {
        key << "<unset>";
    }
    key << ";family=";
    if (filter.ipFamilyType)
    {
        key << TO_INT(*filter.ipFamilyType);
    }
    else
    {
        key << "<unset>";
    }
    key << ";proto=" << TO_INT(filter.protocol);

    AppendOptionalPrefix(key, ";v4src", filter.ipv4 && filter.ipv4->source);
    if (filter.ipv4 && filter.ipv4->source)
    {
        key << filter.ipv4->source->address << '/' << filter.ipv4->source->subnetMask << ';';
    }
    AppendOptionalPrefix(key, "v4dst", filter.ipv4 && filter.ipv4->destination);
    if (filter.ipv4 && filter.ipv4->destination)
    {
        key << filter.ipv4->destination->address << '/' << filter.ipv4->destination->subnetMask
            << ';';
    }
    AppendOptionalPrefix(key, "v4tos", filter.ipv4 && filter.ipv4->typeOfServiceValue);
    if (filter.ipv4 && filter.ipv4->typeOfServiceValue)
    {
        key << TO_INT(*filter.ipv4->typeOfServiceValue) << ';';
    }
    AppendOptionalPrefix(key, "v4tosmask", filter.ipv4 && filter.ipv4->typeOfServiceMask);
    if (filter.ipv4 && filter.ipv4->typeOfServiceMask)
    {
        key << TO_INT(*filter.ipv4->typeOfServiceMask) << ';';
    }

    AppendOptionalPrefix(key, "v6src", filter.ipv6 && filter.ipv6->source);
    if (filter.ipv6 && filter.ipv6->source)
    {
        key << filter.ipv6->source->address << '/' << TO_INT(filter.ipv6->source->prefixLength)
            << ';';
    }
    AppendOptionalPrefix(key, "v6dst", filter.ipv6 && filter.ipv6->destination);
    if (filter.ipv6 && filter.ipv6->destination)
    {
        key << filter.ipv6->destination->address << '/'
            << TO_INT(filter.ipv6->destination->prefixLength) << ';';
    }
    AppendOptionalPrefix(key, "v6tc", filter.ipv6 && filter.ipv6->trafficClassValue);
    if (filter.ipv6 && filter.ipv6->trafficClassValue)
    {
        key << TO_INT(*filter.ipv6->trafficClassValue) << ';';
    }
    AppendOptionalPrefix(key, "v6tcmask", filter.ipv6 && filter.ipv6->trafficClassMask);
    if (filter.ipv6 && filter.ipv6->trafficClassMask)
    {
        key << TO_INT(*filter.ipv6->trafficClassMask) << ';';
    }
    AppendOptionalPrefix(key, "v6flow", filter.ipv6 && filter.ipv6->flowLabel);
    if (filter.ipv6 && filter.ipv6->flowLabel)
    {
        key << *filter.ipv6->flowLabel << ';';
    }
    AppendOptionalPrefix(key, "v6nat", filter.ipv6 && filter.ipv6->natEnabled);
    if (filter.ipv6 && filter.ipv6->natEnabled)
    {
        key << TO_INT(*filter.ipv6->natEnabled) << ';';
    }

    AppendOptionalPrefix(key, "srcport", filter.sourcePort.has_value());
    if (filter.sourcePort)
    {
        key << filter.sourcePort->port << '/' << filter.sourcePort->range << ';';
    }
    AppendOptionalPrefix(key, "dstport", filter.destinationPort.has_value());
    if (filter.destinationPort)
    {
        key << filter.destinationPort->port << '/' << filter.destinationPort->range << ';';
    }
    AppendOptionalPrefix(key, "icmp", filter.icmp.has_value());
    if (filter.icmp)
    {
        key << TO_INT(filter.icmp->type) << '/' << TO_INT(filter.icmp->code) << ';';
    }
    AppendOptionalPrefix(key, "esp", filter.esp.has_value());
    if (filter.esp)
    {
        key << filter.esp->spi << ';';
    }

    return key.str();
}

telux::data::DataRestrictMode TafPaTeluxDataFilter::ConvertDataRestrictMode(
    const FilterModeInfo_t &mode)
{
    telux::data::DataRestrictMode sdkMode{};
    sdkMode.filterMode = ConvertDataRestrictModeType(mode.filterMode);
    sdkMode.filterAutoExit = ConvertDataRestrictModeType(mode.filterAutoExit);
    return sdkMode;
}

FilterModeInfo_t TafPaTeluxDataFilter::ConvertDataRestrictMode(
    const telux::data::DataRestrictMode &mode)
{
    FilterModeInfo_t paMode{};
    paMode.filterMode = ConvertDataRestrictModeType(mode.filterMode);
    paMode.filterAutoExit = ConvertDataRestrictModeType(mode.filterAutoExit);
    return paMode;
}

taf_pa_result_t TafPaTeluxDataFilter::ConvertFilter(
    const IpFilter_t &filter,
    std::shared_ptr<telux::data::IIpFilter> &sdkFilter) const
{
    auto &dataFactory = telux::data::DataFactory::getInstance();
    sdkFilter = dataFactory.getNewIpFilter(static_cast<telux::data::IpProtocol>(filter.protocol));
    if (!sdkFilter)
    {
        TAF_PA_ERROR("Failed to allocate TelSDK data restrict IP filter");
        return TAF_PA_UNSUPPORTED;
    }

    const auto protocol = static_cast<telux::data::IpProtocol>(filter.protocol);
    telux::common::Status status = telux::common::Status::SUCCESS;

    if (filter.ipv4)
    {
        telux::data::IPv4Info ipv4Info{};
        ipv4Info.nextProtoId = protocol;
        if (filter.ipv4->source)
        {
            ipv4Info.srcAddr = filter.ipv4->source->address;
            ipv4Info.srcSubnetMask = filter.ipv4->source->subnetMask.empty()
                ? "255.255.255.255"
                : filter.ipv4->source->subnetMask;
        }
        if (filter.ipv4->destination)
        {
            ipv4Info.destAddr = filter.ipv4->destination->address;
            ipv4Info.destSubnetMask = filter.ipv4->destination->subnetMask.empty()
                ? "255.255.255.255"
                : filter.ipv4->destination->subnetMask;
        }
        if (filter.ipv4->typeOfServiceValue)
        {
            ipv4Info.value = *filter.ipv4->typeOfServiceValue;
        }
        if (filter.ipv4->typeOfServiceMask)
        {
            ipv4Info.mask = *filter.ipv4->typeOfServiceMask;
        }
        status = sdkFilter->setIPv4Info(ipv4Info);
        if (status != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("setIPv4Info failed. Status: %d", TO_INT(status));
            return Utils::ConvertStatus(status);
        }
    }

    if (filter.ipv6)
    {
        telux::data::IPv6Info ipv6Info{};
        ipv6Info.nextProtoId = protocol;
        if (filter.ipv6->source)
        {
            ipv6Info.srcAddr = filter.ipv6->source->address;
            ipv6Info.srcPrefixLen = filter.ipv6->source->prefixLength;
        }
        if (filter.ipv6->destination)
        {
            ipv6Info.destAddr = filter.ipv6->destination->address;
            ipv6Info.dstPrefixLen = filter.ipv6->destination->prefixLength;
        }
        if (filter.ipv6->trafficClassValue)
        {
            ipv6Info.val = *filter.ipv6->trafficClassValue;
        }
        if (filter.ipv6->trafficClassMask)
        {
            ipv6Info.mask = *filter.ipv6->trafficClassMask;
        }
        if (filter.ipv6->flowLabel)
        {
            ipv6Info.flowLabel = *filter.ipv6->flowLabel;
        }
        if (filter.ipv6->natEnabled)
        {
            ipv6Info.natEnabled = *filter.ipv6->natEnabled;
        }
        status = sdkFilter->setIPv6Info(ipv6Info);
        if (status != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("setIPv6Info failed. Status: %d", TO_INT(status));
            return Utils::ConvertStatus(status);
        }
    }

    switch (filter.protocol)
    {
    case IpProtocol_e::TCP:
    {
        auto tcpFilter = std::dynamic_pointer_cast<telux::data::ITcpFilter>(sdkFilter);
        if (!tcpFilter)
        {
            TAF_PA_ERROR("TelSDK returned a non-TCP filter for TCP protocol");
            return TAF_PA_FAULT;
        }
        telux::data::TcpInfo tcpInfo{};
        if (filter.sourcePort)
        {
            tcpInfo.src.port = filter.sourcePort->port;
            tcpInfo.src.range = filter.sourcePort->range;
        }
        if (filter.destinationPort)
        {
            tcpInfo.dest.port = filter.destinationPort->port;
            tcpInfo.dest.range = filter.destinationPort->range;
        }
        status = tcpFilter->setTcpInfo(tcpInfo);
        break;
    }
    case IpProtocol_e::UDP:
    {
        auto udpFilter = std::dynamic_pointer_cast<telux::data::IUdpFilter>(sdkFilter);
        if (!udpFilter)
        {
            TAF_PA_ERROR("TelSDK returned a non-UDP filter for UDP protocol");
            return TAF_PA_FAULT;
        }
        telux::data::UdpInfo udpInfo{};
        if (filter.sourcePort)
        {
            udpInfo.src.port = filter.sourcePort->port;
            udpInfo.src.range = filter.sourcePort->range;
        }
        if (filter.destinationPort)
        {
            udpInfo.dest.port = filter.destinationPort->port;
            udpInfo.dest.range = filter.destinationPort->range;
        }
        status = udpFilter->setUdpInfo(udpInfo);
        break;
    }
    case IpProtocol_e::ICMP:
    {
        if (filter.icmp)
        {
            auto icmpFilter = std::dynamic_pointer_cast<telux::data::IIcmpFilter>(sdkFilter);
            if (!icmpFilter)
            {
                TAF_PA_ERROR("TelSDK returned a non-ICMP filter for ICMP protocol");
                return TAF_PA_FAULT;
            }
            telux::data::IcmpInfo icmpInfo{};
            icmpInfo.type = filter.icmp->type;
            icmpInfo.code = filter.icmp->code;
            status = icmpFilter->setIcmpInfo(icmpInfo);
        }
        break;
    }
    case IpProtocol_e::ESP:
    {
        if (filter.esp)
        {
            auto espFilter = std::dynamic_pointer_cast<telux::data::IEspFilter>(sdkFilter);
            if (!espFilter)
            {
                TAF_PA_ERROR("TelSDK returned a non-ESP filter for ESP protocol");
                return TAF_PA_FAULT;
            }
            telux::data::EspInfo espInfo{};
            espInfo.spi = filter.esp->spi;
            status = espFilter->setEspInfo(espInfo);
        }
        break;
    }
    default:
        return TAF_PA_BAD_PARAMETER;
    }

    if (status != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Setting protocol-specific filter info failed. Status: %d", TO_INT(status));
        return Utils::ConvertStatus(status);
    }

    return TAF_PA_OK;
}

void TafPaTeluxDataFilter::RemoveFilterKey(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(configuredFilterKeys_.begin(), configuredFilterKeys_.end(), key);
    if (it != configuredFilterKeys_.end())
    {
        configuredFilterKeys_.erase(it);
    }
}

taf_pa_result_t TafPaTeluxDataFilter::PaSetDataRestrictMode(
    taf::pa::data::SlotId_e slotId,
    FilterModeInfo_t mode)
{
    if (!IsValidDataRestrictMode(mode))
    {
        return TAF_PA_BAD_PARAMETER;
    }

    std::shared_ptr<telux::data::IDataFilterManager> manager;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = dataFilterManagersMap_.find((SlotId)slotId);
        if (it == dataFilterManagersMap_.end())
        {
            TAF_PA_ERROR("No DataFilter manager available for slot %d", TO_INT(slotId));
            return TAF_PA_NOT_FOUND;
        }
        manager = it->second;
    }

    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future<telux::common::ErrorCode> future = promisePtr->get_future();

    telux::common::ResponseCallback sdkCallback =
        [promisePtr](telux::common::ErrorCode error)
        {
            SET_SDK_THREAD_NAME();
            try
            {
                promisePtr->set_value(error);
            }
            catch (const std::future_error &e)
            {
                TAF_PA_WARN("Future error in setDataRestrictMode callback: %s", e.what());
            }
        };

    telux::common::Status status =
        manager->setDataRestrictMode(ConvertDataRestrictMode(mode), sdkCallback);
    taf_pa_result_t result = Utils::ConvertStatus(status);
    if (result != TAF_PA_OK)
    {
        return result;
    }

    std::future_status waitStatus = future.wait_for(
        std::chrono::seconds(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT));
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("setDataRestrictMode callback timed out");
        return TAF_PA_TIMEOUT;
    }

    telux::common::ErrorCode error;
    FUTURE_GET_RET_VAL(future, error, TAF_PA_FAULT);
    return Utils::ConvertErrorCode(error);
}

taf_pa_result_t TafPaTeluxDataFilter::PaRequestDataRestrictMode(taf::pa::data::SlotId_e slotId, FilterModeInfo_t &mode)
{
    std::shared_ptr<telux::data::IDataFilterManager> manager;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = dataFilterManagersMap_.find((SlotId)slotId);
        if (it == dataFilterManagersMap_.end())
        {
            TAF_PA_ERROR("No DataFilter manager available for slot %d", TO_INT(slotId));
            return TAF_PA_NOT_FOUND;
        }
        manager = it->second;
    }

    auto promisePtr = std::make_shared<
        std::promise<std::pair<telux::data::DataRestrictMode, telux::common::ErrorCode>>>();
    std::future<std::pair<telux::data::DataRestrictMode, telux::common::ErrorCode>> future =
        promisePtr->get_future();

    telux::data::DataRestrictModeCb sdkCallback =
        [promisePtr](telux::data::DataRestrictMode sdkMode, telux::common::ErrorCode error)
        {
            SET_SDK_THREAD_NAME();
            try
            {
                promisePtr->set_value(std::make_pair(sdkMode, error));
            }
            catch (const std::future_error &e)
            {
                TAF_PA_WARN("Future error in requestDataRestrictMode callback: %s", e.what());
            }
        };

    telux::common::Status status = manager->requestDataRestrictMode(sdkCallback);
    taf_pa_result_t result = Utils::ConvertStatus(status);
    if (result != TAF_PA_OK)
    {
        return result;
    }

    std::future_status waitStatus = future.wait_for(
        std::chrono::seconds(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT));
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("requestDataRestrictMode callback timed out");
        return TAF_PA_TIMEOUT;
    }

    std::pair<telux::data::DataRestrictMode, telux::common::ErrorCode> resultPair;
    FUTURE_GET_RET_VAL(future, resultPair, TAF_PA_FAULT);
    result = Utils::ConvertErrorCode(resultPair.second);
    if (result == TAF_PA_OK)
    {
        mode  =
            ConvertDataRestrictMode(resultPair.first);
    }
    return result;
}

taf_pa_result_t TafPaTeluxDataFilter::PaAddDataRestrictFilter(
    taf::pa::data::SlotId_e slotId,
    const IpFilter_t &filter)
{
    if (!IsValidFilter(filter))
    {
        return TAF_PA_BAD_PARAMETER;
    }

    // The current TelSDK NAO filter API is global. Its legacy overload accepts profile and IP
    // family arguments but is explicitly deprecated because those arguments do not scope the
    // resulting filter. Do not silently turn a scoped PA request into a global modem rule.
    if (filter.profileId || filter.ipFamilyType)
    {
        TAF_PA_WARN("Profile-specific or IP-family-specific data restrict filters are unsupported");
        return TAF_PA_UNSUPPORTED;
    }

    std::shared_ptr<telux::data::IIpFilter> sdkFilter;
    taf_pa_result_t result = ConvertFilter(filter, sdkFilter);
    if (result != TAF_PA_OK)
    {
        return result;
    }

    std::shared_ptr<telux::data::IDataFilterManager> manager;
    std::string filterKey = BuildFilterKey(filter);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = dataFilterManagersMap_.find((SlotId)slotId);
        if (it == dataFilterManagersMap_.end())
        {
            TAF_PA_ERROR("No DataFilter manager available for slot %d", TO_INT(slotId));
            return TAF_PA_NOT_FOUND;
        }
        if (std::find(configuredFilterKeys_.begin(), configuredFilterKeys_.end(), filterKey) !=
            configuredFilterKeys_.end())
        {
            TAF_PA_WARN("Duplicate data restrict filter rejected");
            return TAF_PA_DUPLICATE;
        }
        configuredFilterKeys_.push_back(filterKey);
        manager = it->second;
    }

    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future<telux::common::ErrorCode> future = promisePtr->get_future();

    telux::common::ResponseCallback sdkCallback =
        [promisePtr, this, filterKey](telux::common::ErrorCode error)
        {
            SET_SDK_THREAD_NAME();
            taf_pa_result_t callbackResult = Utils::ConvertErrorCode(error);
            if (callbackResult != TAF_PA_OK)
            {
                RemoveFilterKey(filterKey);
            }
            try
            {
                promisePtr->set_value(error);
            }
            catch (const std::future_error &e)
            {
                TAF_PA_WARN("Future error in addDataRestrictFilter callback: %s", e.what());
            }
        };

    telux::common::Status status = manager->addDataRestrictFilter(sdkFilter, sdkCallback);

    result = Utils::ConvertStatus(status);
    if (result != TAF_PA_OK)
    {
        RemoveFilterKey(filterKey);
        return result;
    }

    std::future_status waitStatus = future.wait_for(
        std::chrono::seconds(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT));
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("addDataRestrictFilter callback timed out");
        return TAF_PA_TIMEOUT;
    }

    telux::common::ErrorCode error;
    FUTURE_GET_RET_VAL(future, error, TAF_PA_FAULT);
    result = Utils::ConvertErrorCode(error);
    if (result != TAF_PA_OK)
    {
        return result;
    }

    return TAF_PA_OK;
}

taf_pa_result_t TafPaTeluxDataFilter::PaRemoveAllDataRestrictFilters(taf::pa::data::SlotId_e slotId)
{
    std::shared_ptr<telux::data::IDataFilterManager> manager;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = dataFilterManagersMap_.find((SlotId)slotId);
        if (it == dataFilterManagersMap_.end())
        {
            TAF_PA_ERROR("No DataFilter manager available for slot %d", TO_INT(slotId));
            return TAF_PA_NOT_FOUND;
        }
        manager = it->second;
    }

    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future<telux::common::ErrorCode> future = promisePtr->get_future();

    telux::common::ResponseCallback sdkCallback =
        [promisePtr, this](telux::common::ErrorCode error)
        {
            SET_SDK_THREAD_NAME();
            taf_pa_result_t callbackResult = Utils::ConvertErrorCode(error);
            if (callbackResult == TAF_PA_OK)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                configuredFilterKeys_.clear();
            }
            try
            {
                promisePtr->set_value(error);
            }
            catch (const std::future_error &e)
            {
                TAF_PA_WARN("Future error in removeAllDataRestrictFilters callback: %s", e.what());
            }
        };

    telux::common::Status status = manager->removeAllDataRestrictFilters(sdkCallback);
    taf_pa_result_t result = Utils::ConvertStatus(status);
    if (result != TAF_PA_OK)
    {
        return result;
    }

    std::future_status waitStatus = future.wait_for(
        std::chrono::seconds(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT));
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("removeAllDataRestrictFilters callback timed out");
        return TAF_PA_TIMEOUT;
    }

    telux::common::ErrorCode error;
    FUTURE_GET_RET_VAL(future, error, TAF_PA_FAULT);
    return Utils::ConvertErrorCode(error);
}


taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::SetSubsysState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e sState,
    bool bSendEvent
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    {
        std::unique_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
        dataConnectionManagersSubsysStateMap_[slotId] = sState;
    }
    TAF_PA_INFO("Conn init state for slot id[%d]: %d", slotId, TO_INT(sState));

    // Send the state change event to clients.
    if (bSendEvent)
    {
        TAF_PA_INFO("Send event to clients.");
        auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
        SubsystemEvent_t event;
        taf_pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(slotId, event.phoneId);
        TAF_PA_ERROR_IF_RET_VAL(TAF_PA_OK != result, result, "PaGetPhoneIdFromSlotId err: %d", result);
        event.subsystem      = Subsystem_e::DATACALL_MANAGER;
        event.subsystemState = sState;
        teluxPaData.SendSubsystemEventToClients(event);
    }
    return TAF_PA_OK;
}

void taf::pa::data::TafPaTeluxDataConnection::Init(taf::pa::data::SlotCount_e slotCount)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    slotCount_ = slotCount;
    initDataConnectionManagers();
    PaRegisterDataConnCallbacks();
}

void taf::pa::data::TafPaTeluxDataConnection::Deinit()
{
    deInitDataConnectionManagers();
}

void taf::pa::data::TafPaTeluxDataConnection::LogDataCallInfo
(
    const std::shared_ptr<telux::data::IDataCall> &dataCall,
    const char *fromPtr
)
{
    int32_t profileId = dataCall->getProfileId();
    uint8_t slotId = (uint8_t)dataCall->getSlotId();
    telux::data::DataCallStatus callStatus;

    TAF_PA_DEBUG("data callback details from: %s", fromPtr);
    TAF_PA_DEBUG("profile id:           %d", profileId);
    TAF_PA_DEBUG("slot id:              %d", slotId);
    TAF_PA_DEBUG("interface name:       %s", dataCall->getInterfaceName().c_str());
    callStatus = dataCall->getDataCallStatus();
    TAF_PA_DEBUG("call status:          %s", taf::pa::data::Utils::CallStatusToString(callStatus));
    TAF_PA_DEBUG("ip type:              %s", taf::pa::data::Utils::IpFamilyTypeToString(
                                             dataCall->getIpFamilyType()));
    TAF_PA_DEBUG("ipv4 status:          %s", taf::pa::data::Utils::CallStatusToString(
                                             dataCall->getIpv4Info().status));
    if (telux::data::DataCallStatus::NET_CONNECTED == dataCall->getIpv4Info().status)
    {
        unsigned int mask = 0;
        TAF_PA_DEBUG("IPv4 Address Info:");
        TAF_PA_DEBUG("  Interface addr : %s", dataCall->getIpv4Info().addr.ifAddress.c_str());
        mask = dataCall->getIpv4Info().addr.ifMask;
        TAF_PA_DEBUG("  Interface mask : 0x%X(%u.%u.%u.%u)", mask,
                                                        (mask >> 24) & 0xFF,
                                                        (mask >> 16) & 0xFF,
                                                        (mask >> 8)  & 0xFF,
                                                         mask        & 0xFF);
        TAF_PA_DEBUG("  Gateway addr   : %s", dataCall->getIpv4Info().addr.gwAddress.c_str());
        mask = dataCall->getIpv4Info().addr.gwMask;
        TAF_PA_DEBUG("  Gateway mask   : 0x%X(%u.%u.%u.%u)", mask,
                                                        (mask >> 24) & 0xFF,
                                                        (mask >> 16) & 0xFF,
                                                        (mask >> 8)  & 0xFF,
                                                         mask        & 0xFF);
        TAF_PA_DEBUG("  Pri DNS addr   : %s", dataCall->getIpv4Info().addr.primaryDnsAddress.c_str());
        TAF_PA_DEBUG("  Sec DNS addr   : %s", dataCall->getIpv4Info().addr.secondaryDnsAddress.c_str());
    }
    TAF_PA_DEBUG("ipv6 status:          %s", taf::pa::data::Utils::CallStatusToString(
                                             dataCall->getIpv6Info().status));
    if (telux::data::DataCallStatus::NET_CONNECTED == dataCall->getIpv6Info().status)
    {
        TAF_PA_DEBUG("IPv6 Address Info:");
        TAF_PA_DEBUG("  Interface addr : %s", dataCall->getIpv6Info().addr.ifAddress.c_str());
        TAF_PA_DEBUG("  Interface mask : 0x%X", dataCall->getIpv6Info().addr.ifMask);
        TAF_PA_DEBUG("  Gateway addr   : %s", dataCall->getIpv6Info().addr.gwAddress.c_str());
        TAF_PA_DEBUG("  Gateway mask   : 0x%X", dataCall->getIpv6Info().addr.gwMask);
        TAF_PA_DEBUG("  Pri DNS addr   : %s", dataCall->getIpv6Info().addr.primaryDnsAddress.c_str());
        TAF_PA_DEBUG("  Sec DNS addr   : %s", dataCall->getIpv6Info().addr.secondaryDnsAddress.c_str());
    }

    telux::common::DataCallEndReason reason = dataCall->getDataCallEndReason();
    TAF_PA_DEBUG("call end reason type:   %s",
                                     taf::pa::data::Utils::CallEndReasonTypeToString(reason.type));
    if (telux::data::DataCallStatus::NET_NO_NET == callStatus ||
        telux::data::DataCallStatus::NET_DISCONNECTING == callStatus)
    {
        switch (reason.type)
        {
        case telux::data::EndReasonType::CE_MOBILE_IP:
            TAF_PA_DEBUG("call end MIP reason code: %d", TO_INT(reason.IpCode));
            break;
        case telux::data::EndReasonType::CE_INTERNAL:
            TAF_PA_DEBUG("call end internal reason code: %d", TO_INT(reason.internalCode));
            break;
        case telux::data::EndReasonType::CE_CALL_MANAGER_DEFINED:
            TAF_PA_DEBUG("call end CM reason code: %d", TO_INT(reason.cmCode));
            break;
        case telux::data::EndReasonType::CE_3GPP_SPEC_DEFINED:
            TAF_PA_DEBUG("call end 3GPP spec reason code: %d", TO_INT(reason.specCode));
            break;
        case telux::data::EndReasonType::CE_PPP:
            TAF_PA_DEBUG("call end PPP reason code: %d", TO_INT(reason.pppCode));
            break;
        case telux::data::EndReasonType::CE_EHRPD:
            TAF_PA_DEBUG("call end EHRPD reason code: %d", TO_INT(reason.ehrpdCode));
            break;
        case telux::data::EndReasonType::CE_IPV6:
            TAF_PA_DEBUG("call end IPv6 reason code: %d", TO_INT(reason.ipv6Code));
            break;
        case telux::data::EndReasonType::CE_HANDOFF:
            TAF_PA_DEBUG("call end handoff reason code: %d", TO_INT(reason.handOffCode));
            break;
        default:
            TAF_PA_DEBUG("Invalid Reason code: %d", TO_INT(reason.type));
            break;
        }
    }

    TAF_PA_DEBUG("tech preference:      %s", taf::pa::data::Utils::TechPreferenceToString(
                                             dataCall->getTechPreference()));
    {
        auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
        taf::pa::data::SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(
                                                                         dataCall->getSlotId());
        telux::data::ServiceStatus svcStatus{};
        if (TAF_PA_OK == teluxPaData.PaGetServiceStatus(slotIdPa, svcStatus))
        {
            TAF_PA_DEBUG("NetworkRat (bearer tech): %s",
                     taf::pa::data::Utils::NetworkRatToString(svcStatus.networkRat));
        }
        else
        {
            TAF_PA_DEBUG("NetworkRat (bearer tech): unavailable");
        }
    }

    return;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetDefaultProfile
(
    const taf::pa::data::PhoneId_e phoneId,
    taf::pa::data::ProfileId_e &profileId
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    TAF_PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        TAF_PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr =
                std::make_shared<std::promise<std::tuple<int, SlotId, telux::common::ErrorCode>>>();
    std::future<std::tuple<int, SlotId, telux::common::ErrorCode>> future =
                promisePtr->get_future();

    // Define a lambda function to handle the callback and set the promise
    status = dataConnectionManagersMap_[slotId]->getDefaultProfile
    (
        telux::data::OperationType::DATA_LOCAL,
        // Lambda callback - captures promisePtr by value (shared ownership)
        [promisePtr]
        (
            int profileId, SlotId slotId, telux::common::ErrorCode error
        )
        {
            SET_SDK_THREAD_NAME();
            try
            {
                promisePtr->set_value(std::make_tuple(profileId, slotId, error));
            }
            catch (const std::future_error& e)
            {
                TAF_PA_ERROR("Future error in callback: %s", e.what());
                // Try to set promise to unblock waiting thread
                try { promisePtr->set_value(std::make_tuple(-1, INVALID_SLOT_ID,
                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
            }
            catch (const std::exception& e)
            {
                TAF_PA_ERROR("Exception in callback: %s", e.what());
                try { promisePtr->set_value(std::make_tuple(-1, INVALID_SLOT_ID,
                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
            }
            catch (...)
            {
                TAF_PA_ERROR("Unknown error in getDefaultProfile callback.");
                try { promisePtr->set_value(std::make_tuple(-1, INVALID_SLOT_ID,
                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
            }
        }
    );
    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("getDefaultProfile failed. Status: %d", TO_INT(status));
        return TAF_PA_FAULT;
    }

    // Wait for the callback to complete and capture the results
    TAF_PA_DEBUG("Wait for callback..");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = future.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("getDefaultProfile promise timeout");
        return TAF_PA_TIMEOUT;
    }

    std::tuple<int, SlotId, telux::common::ErrorCode> futResult;
    FUTURE_GET_RET_VAL(future, futResult, TAF_PA_FAULT);
    int futProfileId = std::get<0>(futResult);
    SlotId futSlotId = std::get<1>(futResult);
    telux::common::ErrorCode futError = std::get<2>(futResult);

    if (telux::common::ErrorCode::SUCCESS != futError)
    {
        TAF_PA_ERROR("getDefaultProfile failed in callback. Error: %d", TO_INT(futError));
        return TAF_PA_FAULT;
    }
    TAF_PA_DEBUG("Slot Id: %d", TO_INT(futSlotId));
    TAF_PA_DEBUG("Profile: %d", futProfileId);

    if (futSlotId != slotId)
    {
        TAF_PA_ERROR("Slot ID mismatch. Expected: %d, Actual: %d", TO_INT(slotId), TO_INT(futSlotId));
        return TAF_PA_FAULT;
    }
    profileId = static_cast<taf::pa::data::ProfileId_e>(futProfileId);

    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaSetDefaultProfile
(
    const taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileId_e profileId
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    TAF_PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        TAF_PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future<telux::common::ErrorCode> fut = promisePtr->get_future();

    TAF_PA_INFO("Profile ID to set: %d", TO_INT(profileId));
    status = dataConnectionManagersMap_[slotId]->setDefaultProfile
                (
                    telux::data::OperationType::DATA_LOCAL,
                    static_cast<uint8_t>(profileId),
                    // Lambda callback - captures promisePtr by value (shared ownership)
                    [promisePtr](telux::common::ErrorCode error)
                    {
                        SET_SDK_THREAD_NAME();
                        try
                        {
                            promisePtr->set_value(error);
                        }
                        catch (const std::future_error& e)
                        {
                            TAF_PA_ERROR("Future error in callback: %s", e.what());
                            // Try to set promise to unblock waiting thread
                            try { promisePtr->set_value
                                (telux::common::ErrorCode::INTERNAL_ERROR); } catch(...) {}
                        }
                        catch (const std::exception& e)
                        {
                            TAF_PA_ERROR("Exception in callback: %s", e.what());
                            try { promisePtr->set_value
                                (telux::common::ErrorCode::INTERNAL_ERROR); } catch(...) {}
                        }
                        catch (...)
                        {
                            TAF_PA_ERROR("Unknown error in setDefaultProfile callback.");
                            try { promisePtr->set_value
                                (telux::common::ErrorCode::INTERNAL_ERROR); } catch(...) {}
                        }
                    }
                );
    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("setDefaultProfile failed. Status: %d", TO_INT(status));
        return TAF_PA_FAULT;
    }

    // Wait for the callback to complete and capture the results
    TAF_PA_DEBUG("Wait for callback..");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("setDefaultProfile promise timeout");
        return TAF_PA_TIMEOUT;
    }

    telux::common::ErrorCode futError;
    FUTURE_GET_RET_VAL(fut, futError, TAF_PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futError)
    {
        TAF_PA_ERROR("setDefaultProfile failed in callback. Error: %d", TO_INT(futError));
        return TAF_PA_FAULT;
    }
    TAF_PA_DEBUG("setDefaultProfile done.");
    return TAF_PA_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Private functions
 */
////////////////////////////////////////////////////////////////////////////////////////////////////

void taf::pa::data::TafPaTeluxDataConnection::initDataConnectionManagers()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    // Get the data factory
    auto &dataFactory = telux::data::DataFactory::getInstance();

    // Track slots that failed or are unavailable for summary reporting
    std::vector<int> failedSlots;
    std::vector<int> unavailableSlots;
    std::vector<int> successfulSlots;

    TAF_PA_INFO("Starting data connection manager initialization for %d slot(s)", TO_INT(slotCount_));

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        taf::pa::data::SlotId_e paSlotId = static_cast<taf::pa::data::SlotId_e>(slotId);

        // Check if already initialized
        SubsystemState_e connInitCheckState;
        {
            std::shared_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
            connInitCheckState = dataConnectionManagersSubsysStateMap_[paSlotId];
        }
        if (taf::pa::data::SubsystemState_e::AVAILABLE == connInitCheckState)
        {
            TAF_PA_INFO("Data connection manager already initialized for slot id: %d.", TO_INT(slotId));
            successfulSlots.push_back(slotId);
            continue;
        }

        TAF_PA_INFO("Initializing data connection manager for slot %d...", slotId);

        // Initialize the data connection manager for each slot
        // Create shared state for synchronization using condition variable
        // Use heap-allocated shared_ptr to ensure state outlives this function scope
        struct InitState {
            std::mutex mtx;
            std::condition_variable cv;
            bool callbackReceived = false;
            telux::common::ServiceStatus status;
        };
        auto state = std::make_shared<InitState>();

        auto dataProfMngr = dataFactory.getDataConnectionManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [state, slotId](telux::common::ServiceStatus svcStatus)
            {
                SET_SDK_THREAD_NAME();
                TAF_PA_INFO("getDataConnectionManager callback for slot ID: %d", TO_INT(slotId));
                TAF_PA_INFO("Data connection manager status: %d", TO_INT(svcStatus));

                {
                    std::lock_guard<std::mutex> lock(state->mtx);
                    state->status = svcStatus;
                    state->callbackReceived = true;
                }
                state->cv.notify_one();
            });

        if (!dataProfMngr)
        {
            TAF_PA_ERROR("Failed to get Data connection manager for slot %d", slotId);
            SetSubsysState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }

        // Wait for the callback with timeout
        telux::common::ServiceStatus dataServSysMgrStatus;
        TAF_PA_INFO("Waiting for Data connection manager to be ready for slot %d...", slotId);

        {
            std::unique_lock<std::mutex> lock(state->mtx);
            bool success = state->cv.wait_for(
                lock,
                std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT),
                [&state]() { return state->callbackReceived; }
            );

            if (!success)
            {
                TAF_PA_ERROR("Timeout waiting for Data connection manager for slot %d", slotId);
                SetSubsysState(paSlotId, SubsystemState_e::FAILED);
                failedSlots.push_back(slotId);
                // Continue processing other slots instead of returning
                continue;
            }

            dataServSysMgrStatus = state->status;
        }

        TAF_PA_INFO("dataConnMngr for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));

        // Handle different service status outcomes
        if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
        {
            TAF_PA_INFO("dataConnMngr for slot %d: AVAILABLE", slotId);
            // Store the manager in the map with slot Id as index
            dataConnectionManagersMap_.emplace((SlotId)slotId, dataProfMngr);
            // Keep the listener alive for TelSDK state-change callbacks and later registration.
            dataConnectionListenersMap_[(SlotId)slotId] =
                std::make_shared<TafPaTeluxDataConnectionListener>((SlotId)slotId);
            // Mark listener as not registered (under lock for consistency with
            // PaRegisterDataConnCallbacks / PaDeregisterDataConnCallbacks).
            {
                std::unique_lock lock(dataConnectionCbksMtx_);
                bDataConnectionListenersRegistered_[slotId-1] = false;
            }
            // Update that the data connection manager is initialized.
            SetSubsysState(paSlotId, SubsystemState_e::AVAILABLE);
            successfulSlots.push_back(slotId);
            TAF_PA_INFO("Data connection manager initialization for slot %d complete", slotId);
        }
        else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
        {
            TAF_PA_WARN("dataConnMngr for slot %d: UNAVAILABLE (temporary)", slotId);
            // Mark as unavailable but don't set FAILED state - this is temporary
            // The service may become available later
            unavailableSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
        else
        {
            // Unknown/error status
            TAF_PA_ERROR("Failed to init Data connection manager for slot %d with status: %d",
                     slotId, TO_INT(dataServSysMgrStatus));
            SetSubsysState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
    }

    // Log summary of initialization results
    TAF_PA_INFO("=== Data Connection Manager Initialization Summary ===");
    TAF_PA_INFO("Total slots: %d", TO_INT(slotCount_));
    TAF_PA_INFO("Successfully initialized: %zu slot(s)", successfulSlots.size());
    if (!successfulSlots.empty())
    {
        for (auto slot : successfulSlots)
        {
            TAF_PA_INFO("  - Slot %d: AVAILABLE", slot);
        }
    }

    if (!unavailableSlots.empty())
    {
        TAF_PA_WARN("Temporarily unavailable: %zu slot(s)", unavailableSlots.size());
        for (auto slot : unavailableSlots)
        {
            TAF_PA_WARN("  - Slot %d: UNAVAILABLE (may retry later)", slot);
        }
    }

    if (!failedSlots.empty())
    {
        TAF_PA_ERROR("Failed to initialize: %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_ERROR("  - Slot %d: FAILED", slot);
        }
    }
    TAF_PA_INFO("====================================================");

    return;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::deInitDataConnectionManagers()
{
    TAF_PA_INFO("Starting data connection managers deinitialization...");

    // Deregister callbacks with error checking.
    taf_pa_result_t result = PaDeregisterDataConnCallbacks();
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("CRITICAL: Failed to deregister data connection callbacks!");
        TAF_PA_ERROR("Cannot safely proceed with cleanup - callbacks may still be active");
        TAF_PA_ERROR("This could lead to crashes if SDK invokes callbacks after cleanup");
        // Reset the flag to prevent further event forwarding attempts even though
        // deregistration failed - this stops the PA layer from forwarding any
        // incoming SDK throughput events to a partially cleaned-up state.
        bThroughputEventsEnabled_.store(false);
        return TAF_PA_FAULT; // Do NOT clear maps if deregistration failed
    }

    TAF_PA_INFO("Callbacks successfully deregistered, proceeding with cleanup...");

    // Phase 3: Ordered cleanup with mutex protection
    {
        std::unique_lock lock(dataConnectionCbksMtx_);

        // Clear callback vectors first to prevent new callbacks from being invoked
        TAF_PA_INFO("Clearing callback vectors...");
        dataCallEventsCallbacks_.clear();
        throttledApnEventsCallbacks_.clear();
        qosTftEventsCallbacks_.clear();
        hwAccelerationEventsCallbacks_.clear();
        throughputEventsCallbacks_.clear();
        // Explicitly reset the throughput events gate flag to ensure a consistent
        // state. During deinit the callbacks are cleared directly (not via
        // PaRemoveThroughputEventsCallback), so the flag must be reset here.
        bThroughputEventsEnabled_.store(false);

        TAF_PA_INFO("Cleared all callback vectors");
    }

    // Clear listener and manager maps in proper order
    TAF_PA_INFO("Clear dataConnectionListenersMap_");
    dataConnectionListenersMap_.clear();

    TAF_PA_INFO("Clear dataConnectionManagersMap_");
    dataConnectionManagersMap_.clear();

    // Update subsystem states to reflect deinitialized state
    {
        std::unique_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
        dataConnectionManagersSubsysStateMap_[SlotId_e::SLOT_1] = SubsystemState_e::FAILED;
        dataConnectionManagersSubsysStateMap_[SlotId_e::SLOT_2] = SubsystemState_e::FAILED;
    }

    TAF_PA_INFO("Data connection managers deinitialization complete");
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRegisterDataConnCallbacks()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    bool allSuccess = true;
    std::vector<int> failedSlots;
    // Protect the critical section
    std::unique_lock lock(dataConnectionCbksMtx_);

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Register serving system listener for each slot it
        if (bDataConnectionListenersRegistered_[slotId - 1])
        {
            TAF_PA_INFO("Data connection listener already registered for slot ID: %d.", slotId);
        }
        else
        {
            auto managerIt = dataConnectionManagersMap_.find((SlotId)slotId);
            if (managerIt != dataConnectionManagersMap_.end())
            {
                SubsystemState_e subsysState;
                PaGetSubsysState(static_cast<SlotId_e>(slotId), subsysState);
                if (SubsystemState_e::AVAILABLE != subsysState)
                {
                    TAF_PA_ERROR("Subsystem not initialized for slot id: %d", slotId);
                    allSuccess = false;
                    failedSlots.push_back(slotId);
                    // Go to the next slot if available
                    continue;
                }
                auto listenerIt = dataConnectionListenersMap_.find((SlotId)slotId);
                if (listenerIt != dataConnectionListenersMap_.end())
                {
                    // Register for DEFAULT indications only.
                    // The THROUGHPUT indication is registered separately in
                    // PaAddThroughputEventsCallback() when the first throughput callback is
                    // added, and deregistered in PaRemoveThroughputEventsCallback() when the
                    // last throughput callback is removed.
                    if (managerIt->second->registerListener(listenerIt->second,
                        telux::data::DEFAULT_INDICATIONS) == telux::common::Status::SUCCESS)
                    {
                        TAF_PA_INFO("Data connection listener for slot ID %d registered.", slotId);
                        bDataConnectionListenersRegistered_[slotId - 1] = true;
                    }
                    else
                    {
                        TAF_PA_ERROR("Failed to register data connection listener for slot ID %d.",
                                                                                           slotId);
                        allSuccess = false;
                        failedSlots.push_back(slotId);
                    }
                }
            }
        }
    }
    if (!allSuccess)
    {
        TAF_PA_ERROR("=== Registration Failed ===");
        TAF_PA_ERROR("Failed to register listeners for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_ERROR("  - Slot %d: REGISTRATION FAILED", slot);
        }
        TAF_PA_ERROR("============================");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("All data connection listeners successfully registered");
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaDeregisterDataConnCallbacks()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    // Protect the critical section
    std::unique_lock lock(dataConnectionCbksMtx_);

    bool allSuccess = true;
    std::vector<int> failedSlots;

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Deregister serving system listener for each slot
        if (bDataConnectionListenersRegistered_[slotId - 1])
        {
            if (dataConnectionManagersMap_.find((SlotId)slotId) != dataConnectionManagersMap_.end())
            {
                // Deregister from DEFAULT indications only.
                // The THROUGHPUT indication is managed independently by
                // PaAddThroughputEventsCallback() and PaRemoveThroughputEventsCallback().
                telux::common::Status status = dataConnectionManagersMap_[(SlotId)slotId]->
                    deregisterListener(dataConnectionListenersMap_[(SlotId)slotId],
                                      telux::data::DEFAULT_INDICATIONS);

                if (telux::common::Status::SUCCESS == status)
                {
                    TAF_PA_INFO("Data connection listener for slot ID %d deregistered.", slotId);
                    bDataConnectionListenersRegistered_[slotId - 1] = false;
                }
                else
                {
                    TAF_PA_ERROR("FAILED to deregister listener for slot ID %d. Status: %d",
                             slotId, TO_INT(status));
                    failedSlots.push_back(slotId);
                    allSuccess = false;
                }
            }
        }
        else
        {
            TAF_PA_INFO("Serving System listener already deregistered for slot ID: %d.", slotId);
        }
    }

    if (!allSuccess)
    {
        TAF_PA_ERROR("=== Deregistration Failed ===");
        TAF_PA_ERROR("Failed to deregister callbacks for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_ERROR("  - Slot %d: DEREGISTRATION FAILED", slot);
        }
        TAF_PA_ERROR("============================");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("All data connection callbacks successfully deregistered");
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaStartDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t &params,
    taf_pa_data_OperationCb callback,
    std::shared_ptr<void> context
)
{
    telux::data::DataCallParams teluxParams;
    taf::pa::data::SlotId_e slotIDpa;

    teluxParams.profileId = static_cast<int>(params.profileId);
    teluxParams.ipFamilyType = taf::pa::data::Utils::ConvertIpType(params.ipType);
    teluxParams.interfaceName = params.interfaceName;

    if (!callback)
    {
        return TAF_PA_BAD_PARAMETER;
    }

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    if (taf::pa::data::SubsystemState_e::AVAILABLE != phoneMngrState)
    {
        TAF_PA_ERROR("PA phone manager not initialized for Start data-call operation");
        return TAF_PA_UNAVAILABLE;
    }

    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(params.phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d", TO_INT(params.phoneId));
        return result;
    }
    TAF_PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);
    auto managerIt = dataConnectionManagersMap_.find(slotId);
    if (managerIt == dataConnectionManagersMap_.end() || !managerIt->second)
    {
        TAF_PA_ERROR("Connection manager is not initialized for slot %d", TO_INT(slotId));
        return TAF_PA_UNAVAILABLE;
    }

    // Forward only the TelSDK operation response. Data-call state indications still
    // flow through TafPaTeluxDataConnectionListener::onDataCallInfoChanged().
    telux::common::Status status = managerIt->second->startDataCall(
        teluxParams,
        [callback, context, requestedPhoneId = params.phoneId, requestedSlotId = slotIDpa](
            const std::shared_ptr<telux::data::IDataCall> &dataCall,
            telux::common::ErrorCode errorCode)
        {
            SET_SDK_THREAD_NAME();
            taf::pa::data::DataCallOperationResult_t operationResult{};
            operationResult.operationType = taf::pa::data::DataCallOperationType::START;
            operationResult.result =
                telux::common::ErrorCode::SUCCESS == errorCode ? TAF_PA_OK : TAF_PA_FAULT;

            auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
            if (!dataCall)
            {
                TAF_PA_WARN("TelSDK returned a null IDataCall for Start data-call operation, "
                            "error=%d", TO_INT(errorCode));
                operationResult.result = TAF_PA_FAULT;
                teluxPaDataConn.invokeDataCallOperationCallback(
                    callback, context, operationResult);
                return;
            }

            try
            {
                LogDataCallInfo(dataCall, __func__);
            }
            catch (const std::exception &error)
            {
                TAF_PA_WARN("Failed to log Start data-call info: %s", error.what());
            }
            catch (...)
            {
                TAF_PA_WARN("Unknown exception while logging Start data-call info");
            }

            taf::pa::data::DataCallEventInfo_t parsedDataCallInfo{};
            telux::data::DataCallStatus callStatus = telux::data::DataCallStatus::INVALID;
            const taf_pa_result_t statusResult = GetTeluxDataCallStatus(dataCall, callStatus);
            if (TAF_PA_OK != statusResult)
            {
                TAF_PA_ERROR("Failed to get Start data-call status: %d", statusResult);
                if (telux::common::ErrorCode::SUCCESS == errorCode)
                {
                    operationResult.result = statusResult;
                }
                teluxPaDataConn.invokeDataCallOperationCallback(
                    callback, context, operationResult);
                return;
            }
            parsedDataCallInfo.callStatus =
                taf::pa::data::Utils::ConvertCallStatus(callStatus);

            if (taf::pa::data::DataCallStatus_e::CONNECTED == parsedDataCallInfo.callStatus)
            {
                // Parse the remaining operation-response fields only for a connected call.
                const taf_pa_result_t parseResult = taf::pa::data::Utils::ParseDataCallInfo(
                    dataCall, requestedPhoneId, parsedDataCallInfo, &callStatus);
                if (TAF_PA_OK != parseResult)
                {
                    TAF_PA_ERROR("Failed to parse Start data-call info: %d", parseResult);
                    if (telux::common::ErrorCode::SUCCESS == errorCode)
                    {
                        operationResult.result = parseResult;
                    }
                    teluxPaDataConn.invokeDataCallOperationCallback(
                        callback, context, operationResult);
                    return;
                }
            }

            if (telux::common::ErrorCode::SUCCESS != errorCode)
            {
                TAF_PA_ERROR("Starting data session failed with error code: %d",
                             TO_INT(errorCode));
                parsedDataCallInfo.callStatus =
                    taf::pa::data::DataCallStatus_e::DISCONNECTED;
                parsedDataCallInfo.ipv4DataCallInfo.callStatus =
                    taf::pa::data::DataCallStatus_e::DISCONNECTED;
                parsedDataCallInfo.ipv6DataCallInfo.callStatus =
                    taf::pa::data::DataCallStatus_e::DISCONNECTED;
            }
            else if (telux::data::DataCallStatus::NET_NO_NET == callStatus)
            {
                // Preserve legacy Start behavior: TelSDK can report SUCCESS while the
                // IDataCall still shows NET_NO_NET during bring-up. Treat that operation
                // result as CONNECTING; the listener indication will report the final state.
                TAF_PA_WARN("TelSDK returned NET_NO_NET for successful Start data-call operation");
                parsedDataCallInfo.callStatus =
                    taf::pa::data::DataCallStatus_e::CONNECTING;
                parsedDataCallInfo.ipv4DataCallInfo.callStatus =
                    taf::pa::data::DataCallStatus_e::CONNECTING;
                parsedDataCallInfo.ipv6DataCallInfo.callStatus =
                    taf::pa::data::DataCallStatus_e::CONNECTING;
            }

            if (taf::pa::data::DataCallStatus_e::CONNECTED == parsedDataCallInfo.callStatus)
            {
                // Prefer the identifiers from the accepted request for this connected snapshot.
                parsedDataCallInfo.phoneId = requestedPhoneId;
                parsedDataCallInfo.slotId = requestedSlotId;
                operationResult.dataCallInfo = std::move(parsedDataCallInfo);
            }

            teluxPaDataConn.invokeDataCallOperationCallback(callback, context, operationResult);
        });

    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("TelSDK Start data-call dispatch failed. Status: %d", TO_INT(status));
        return TAF_PA_FAULT;
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaStopDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t &params,
    taf_pa_data_OperationCb callback,
    std::shared_ptr<void> context
)
{
    telux::data::DataCallParams teluxParams;
    taf::pa::data::SlotId_e slotIDpa;

    teluxParams.profileId = static_cast<int>(params.profileId);
    teluxParams.ipFamilyType = taf::pa::data::Utils::ConvertIpType(params.ipType);
    teluxParams.interfaceName = params.interfaceName;

    if (!callback)
    {
        return TAF_PA_BAD_PARAMETER;
    }

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    if (taf::pa::data::SubsystemState_e::AVAILABLE != phoneMngrState)
    {
        TAF_PA_ERROR("PA phone manager not initialized for Stop data-call operation");
        return TAF_PA_UNAVAILABLE;
    }

    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(params.phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d", TO_INT(params.phoneId));
        return result;
    }
    TAF_PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);
    auto managerIt = dataConnectionManagersMap_.find(slotId);
    if (managerIt == dataConnectionManagersMap_.end() || !managerIt->second)
    {
        TAF_PA_ERROR("Connection manager is not initialized for slot %d", TO_INT(slotId));
        return TAF_PA_UNAVAILABLE;
    }

    // Forward only the TelSDK operation response. Data-call state indications still
    // flow through TafPaTeluxDataConnectionListener::onDataCallInfoChanged().
    telux::common::Status status = managerIt->second->stopDataCall(
        teluxParams,
        [callback, context, requestedPhoneId = params.phoneId, requestedSlotId = slotIDpa](
            const std::shared_ptr<telux::data::IDataCall> &dataCall,
            telux::common::ErrorCode errorCode)
        {
            SET_SDK_THREAD_NAME();
            taf::pa::data::DataCallOperationResult_t operationResult{};
            operationResult.operationType = taf::pa::data::DataCallOperationType::STOP;
            operationResult.result =
                telux::common::ErrorCode::SUCCESS == errorCode ? TAF_PA_OK : TAF_PA_FAULT;

            auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
            if (!dataCall)
            {
                TAF_PA_WARN("TelSDK returned a null IDataCall for Stop data-call operation, "
                            "error=%d", TO_INT(errorCode));
                operationResult.result = TAF_PA_FAULT;
                teluxPaDataConn.invokeDataCallOperationCallback(
                    callback, context, operationResult);
                return;
            }

            try
            {
                LogDataCallInfo(dataCall, __func__);
            }
            catch (const std::exception &error)
            {
                TAF_PA_WARN("Failed to log Stop data-call info: %s", error.what());
            }
            catch (...)
            {
                TAF_PA_WARN("Unknown exception while logging Stop data-call info");
            }

            // Return the operation-response status and available IP-family details. Listener state
            // bookkeeping and its bit-rate request remain independent of this callback.
            taf::pa::data::DataCallEventInfo_t parsedDataCallInfo{};
            telux::data::DataCallStatus callStatus = telux::data::DataCallStatus::INVALID;
            const taf_pa_result_t statusResult = GetTeluxDataCallStatus(dataCall, callStatus);
            if (TAF_PA_OK != statusResult)
            {
                TAF_PA_ERROR("Failed to get Stop data-call status: %d", statusResult);
                if (telux::common::ErrorCode::SUCCESS == errorCode)
                {
                    operationResult.result = statusResult;
                }
                teluxPaDataConn.invokeDataCallOperationCallback(
                    callback, context, operationResult);
                return;
            }
            parsedDataCallInfo.callStatus =
                taf::pa::data::Utils::ConvertCallStatus(callStatus);

            if (taf::pa::data::DataCallStatus_e::CONNECTED == parsedDataCallInfo.callStatus)
            {
                const taf_pa_result_t parseResult = taf::pa::data::Utils::ParseDataCallInfo(
                    dataCall, requestedPhoneId, parsedDataCallInfo, &callStatus);
                if (TAF_PA_OK != parseResult)
                {
                    TAF_PA_ERROR("Failed to parse Stop data-call info: %d", parseResult);
                    if (telux::common::ErrorCode::SUCCESS == errorCode)
                    {
                        operationResult.result = parseResult;
                    }
                    teluxPaDataConn.invokeDataCallOperationCallback(
                        callback, context, operationResult);
                    return;
                }
            }

            if (taf::pa::data::DataCallStatus_e::CONNECTED == parsedDataCallInfo.callStatus)
            {
                parsedDataCallInfo.phoneId = requestedPhoneId;
                parsedDataCallInfo.slotId = requestedSlotId;
                operationResult.dataCallInfo = std::move(parsedDataCallInfo);
            }

            teluxPaDataConn.invokeDataCallOperationCallback(callback, context, operationResult);
        });

    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("TelSDK Stop data-call dispatch failed. Status: %d", TO_INT(status));
        return TAF_PA_FAULT;
    }

    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Invoke the caller-provided operation callback with exception protection.
 *
 * Client callbacks are external code from the PA's perspective. Catching exceptions here prevents a
 * bad callback from unwinding through PA/TelSDK callback handling.
 */
//--------------------------------------------------------------------------------------------------
void taf::pa::data::TafPaTeluxDataConnection::invokeDataCallOperationCallback
(
    taf_pa_data_OperationCb callback,
    std::shared_ptr<void> context,
    const DataCallOperationResult_t &result
)
{
    if (!callback)
    {
        return;
    }

    try
    {
        callback(result, context);
    }
    catch (const std::exception &error)
    {
        TAF_PA_ERROR("Exception in data-call operation callback: %s", error.what());
    }
    catch (...)
    {
        TAF_PA_ERROR("Unknown exception in data-call operation callback");
    }
}

void taf::pa::data::TafPaTeluxDataConnection::onRequestDataCallList
(
    const std::shared_ptr<DataCallListRequestContext_t> &request,
    const std::vector<std::shared_ptr<telux::data::IDataCall>> &iDataCalls,
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();
    if (!request)
    {
        TAF_PA_ERROR("Ignoring data-call-list response without request context");
        return;
    }
    // This completion guard belongs to this request alone. It prevents duplicate/concurrent
    // TelSDK responses from invoking the same client callback more than once.
    if (request->completed.exchange(true))
    {
        TAF_PA_INFO("Ignoring duplicate data-call-list response");
        return;
    }

    TAF_PA_INFO("Data-call-list response contains %zu calls", iDataCalls.size());

    taf_pa_result_t result = Utils::ConvertErrorCode(error);
    std::vector<DataCallEventInfo_t> callList;
    if (telux::common::ErrorCode::SUCCESS != error)
    {
        TAF_PA_ERROR("onRequestDataCallList failed with TelSDK error code: %d", TO_INT(error));
    }
    else
    {
        // Return a complete snapshot or a failure, never a partial list if one entry is invalid.
        try
        {
            result = TAF_PA_OK;
            for (const auto &iDataCall : iDataCalls)
            {
                if (!iDataCall)
                {
                    TAF_PA_ERROR("TelSDK returned a null IDataCall in the call list");
                    result = TAF_PA_FAULT;
                    callList.clear();
                    break;
                }

                const SlotId slotId = iDataCall->getSlotId();
                TAF_PA_INFO("Slot ID: %d", TO_INT(slotId));
                DataCallEventInfo_t eventInfo{};
                const taf_pa_result_t parseResult =
                    Utils::ParseDataCallInfo(iDataCall, request->phoneId, eventInfo);
                if (TAF_PA_OK != parseResult)
                {
                    TAF_PA_ERROR("Failed to parse call-list entry for slot %d: %d",
                                 TO_INT(slotId), parseResult);
                    result = parseResult;
                    callList.clear();
                    break;
                }
                callList.push_back(std::move(eventInfo));
            }
        }
        catch (const std::exception &exception)
        {
            TAF_PA_ERROR("Exception while parsing data-call list: %s", exception.what());
            result = TAF_PA_FAULT;
            callList.clear();
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception while parsing data-call list");
            result = TAF_PA_FAULT;
            callList.clear();
        }
    }

    if (TAF_PA_OK != result)
    {
        callList.clear();
    }

    TAF_PA_INFO("Data-call-list request completed with result %d and %zu calls",
                result, callList.size());
    if (!request->callback)
    {
        TAF_PA_WARN("Cannot invoke an empty data-call-list callback");
        return;
    }

    try
    {
        request->callback(result, callList, std::move(request->context));
    }
    catch (const std::exception &exception)
    {
        TAF_PA_ERROR("Exception in data-call-list callback: %s", exception.what());
    }
    catch (...)
    {
        TAF_PA_ERROR("Unknown exception in data-call-list callback");
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRequestDataCallsListAsync
(
    taf::pa::data::PhoneId_e phoneId,
    taf::pa::data::taf_pa_data_RequestCallListCb callback,
    std::shared_ptr<void> context
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callback, TAF_PA_BAD_PARAMETER, "callback is null");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");

    taf::pa::data::SlotId_e slotIdPa;
    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIdPa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    TAF_PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIdPa));
    const SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIdPa);
    if (INVALID_SLOT_ID == slotId)
    {
        TAF_PA_ERROR("Invalid Slot ID");
        return TAF_PA_BAD_PARAMETER;
    }

    std::shared_ptr<DataCallListRequestContext_t> request;
    try
    {
        request = std::make_shared<DataCallListRequestContext_t>();
    }
    catch (const std::exception &error)
    {
        TAF_PA_ERROR("Failed to allocate data-call-list request context: %s", error.what());
        return TAF_PA_NO_MEMORY;
    }

    request->callback = std::move(callback);
    request->context = std::move(context);
    request->phoneId = phoneId;

    const auto managerIt = dataConnectionManagersMap_.find(slotId);
    if (managerIt == dataConnectionManagersMap_.end() || !managerIt->second)
    {
        TAF_PA_ERROR("Connection manager is not initialized for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    telux::common::Status status = telux::common::Status::FAILED;
    try
    {
        status = managerIt->second->requestDataCallList(
            telux::data::OperationType::DATA_LOCAL,
            // Keep this request's callback/context with its TelSDK response. This avoids a shared
            // "current request" slot, so concurrent requests can complete independently and out
            // of order.
            [request](
                const std::vector<std::shared_ptr<telux::data::IDataCall>> &dataCalls,
                telux::common::ErrorCode error)
            {
                TafPaTeluxDataConnection::onRequestDataCallList(
                    request, dataCalls, error);
            });
    }
    catch (const std::exception &error)
    {
        TAF_PA_ERROR("requestDataCallList threw an exception: %s", error.what());
    }
    catch (...)
    {
        TAF_PA_ERROR("requestDataCallList threw an unknown exception");
    }

    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("requestDataCallList failed for phone id[%d]: %d",
                     TO_INT(phoneId), TO_INT(status));
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("requestDataCallList accepted for phone id %d", TO_INT(phoneId));
    return TAF_PA_OK;
}



taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::paGetThrottledApnInfo
(
    const taf::pa::data::PhoneId_e        phoneId,
    std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    TAF_PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        TAF_PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr = std::make_shared<std::promise<
            std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode>>>();
    std::future<std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode>> fut
            = promisePtr->get_future();

    // Single SDK call — the callback only fulfils the promise, nothing else.
    status = dataConnectionManagersMap_[slotId]->requestThrottledApnInfo(
    [promisePtr]
    (
        const std::vector<telux::data::APNThrottleInfo> &throttleInfoList,
        telux::common::ErrorCode error
    )
    {
        SET_SDK_THREAD_NAME();
        try
        {
            promisePtr->set_value(std::make_pair(throttleInfoList, error));
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
            // Try to set promise to unblock waiting thread
            try { promisePtr->set_value(std::make_pair(std::vector<telux::data::APNThrottleInfo>(),
                telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
            try { promisePtr->set_value(std::make_pair(std::vector<telux::data::APNThrottleInfo>(),
                telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in requestThrottledApnInfo callback.");
            try { promisePtr->set_value(std::make_pair(std::vector<telux::data::APNThrottleInfo>(),
                telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
        }
    });
    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("requestThrottledApnInfo failed. Status: %d", TO_INT(status));
        return TAF_PA_FAULT;
    }
    TAF_PA_DEBUG("Wait for callback ...");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("requestThrottledApnInfo promise timeout");
        return TAF_PA_TIMEOUT;
    }

    std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode> futResult;
    FUTURE_GET_RET_VAL(fut, futResult, TAF_PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futResult.second)
    {
        TAF_PA_ERROR("requestThrottledApnInfo error: %d", TO_INT(futResult.second));
        return TAF_PA_FAULT;
    }

    std::vector<telux::data::APNThrottleInfo> throttleInfoList = futResult.first;
    size_t numThrottledApn = throttleInfoList.size();
    TAF_PA_DEBUG("Number of throttled APN(s): %zu", numThrottledApn);
    if (numThrottledApn > 0)
    {
        for (auto &throttleInfo : throttleInfoList)
        {
            ThrottledApnEventInfo_t throttledApnEventInfo;
            Utils::ConvertThrottledApnEvent(throttleInfo, throttledApnEventInfo);
            // Add the phone ID
            throttledApnEventInfo.phoneId = phoneId;
            // Add to vector
            throttledApnEventInfoList.push_back(throttledApnEventInfo);
        }
    }

    return TAF_PA_OK;
}

/**
 * SDK to PA event callbacks
 */
void taf::pa::data::TafPaTeluxDataConnection::PaSendHwAccelerationEventInfoToClients
(
    const HwAccelerationChangeEvent_t &hwAccelerationEventInfo
)
{
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<HwAccelerationEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = hwAccelerationEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(hwAccelerationEventInfo, cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddHwAccelerationChangeEventsCallback
(
    taf_pa_data_HwAccelerationEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");

    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    HwAccelerationEventsCallbackEntry_t entry = {hwAccelerationEventsCallbackId_, callBack,
                                                                                        context};
    hwAccelerationEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = hwAccelerationEventsCallbackId_;
    // Increment the ID.
    hwAccelerationEventsCallbackId_++;
    TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    TAF_PA_INFO("Number of registered callbacks: %zu", hwAccelerationEventsCallbacks_.size());
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveHwAccelerationChangeEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = hwAccelerationEventsCallbacks_.begin();
        cbk != hwAccelerationEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            hwAccelerationEventsCallbacks_.erase(cbk);
            return TAF_PA_OK;
        }
    }
    TAF_PA_WARN("Callback not found. Id: %d", id);
    return TAF_PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendQosTftEventInfoToClients
(
    const QosTftEventInfo_t &qosTftEventsList
)
{
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<QosTftEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = qosTftEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(qosTftEventsList, cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddQosTftEventsCallback
(
    taf_pa_data_QosTftEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    QosTftEventsCallbackEntry_t entry = {qosTftEventsCallbackId_, callBack, context};
    qosTftEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = qosTftEventsCallbackId_;
    // Increment the ID.
    qosTftEventsCallbackId_++;
    TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    TAF_PA_INFO("Number of registered callbacks: %zu", qosTftEventsCallbacks_.size());
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveQosTftEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = qosTftEventsCallbacks_.begin();
        cbk != qosTftEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            qosTftEventsCallbacks_.erase(cbk);
            return TAF_PA_OK;
        }
    }
    TAF_PA_WARN("Callback not found. Id: %d", id);
    return TAF_PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendThrottledApnEventInfoToClients
(
    const std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfo
)
{
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<ThrottledApnEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = throttledApnEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(throttledApnEventInfo, cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddThrottledApnEventsCallback
(
    taf_pa_data_ThrottledApnEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    ThrottledApnEventsCallbackEntry_t entry = {throttledApnEventsCallbackId_, callBack, context};
    throttledApnEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = throttledApnEventsCallbackId_;
    // Increment the ID.
    throttledApnEventsCallbackId_++;
    TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    TAF_PA_INFO("Number of registered callbacks: %zu", throttledApnEventsCallbacks_.size());
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveThrottledApnEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for
    (
        auto cbk = throttledApnEventsCallbacks_.begin();
        cbk != throttledApnEventsCallbacks_.end(); ++cbk
    )
    {
        if (cbk->id == id)
        {
            TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            throttledApnEventsCallbacks_.erase(cbk);
            return TAF_PA_OK;
        }
    }
    TAF_PA_WARN("Callback not found. Id: %d", id);
    return TAF_PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendDataCallEventInfoToClients
(
    const taf::pa::data::DataCallEventInfo_t &dataCallEventInfo
)
{
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<DataCallEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = dataCallEventsCallbacks_;
    }
    // Call the service callback(s)
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(dataCallEventInfo, cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddDataCallEventsCallback
(
    taf_pa_data_CallEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");

    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    DataCallEventsCallbackEntry_t entry = {dataCallEventsCallbackId_, callBack, context};
    dataCallEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = dataCallEventsCallbackId_;
    // Increment the ID.
    dataCallEventsCallbackId_++;
    TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    TAF_PA_INFO("Number of registered callbacks: %zu", dataCallEventsCallbacks_.size());
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveDataCallEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = dataCallEventsCallbacks_.begin(); cbk != dataCallEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            dataCallEventsCallbacks_.erase(cbk);
            return TAF_PA_OK;
        }
    }
    TAF_PA_WARN("Callback not found. Id: %d", id);
    return TAF_PA_NOT_FOUND;
}

/*
 * Throughput Events Implementation
*/

void taf::pa::data::TafPaTeluxDataConnection::PaSendThroughputEventInfoToClients
(
    const std::vector<ThroughputInfo_t> &throughputInfoList
)
{
    // Gate: suppress SDK throughput events until at least one PA-level callback is registered.
    // This keeps the gating logic exclusively in the PA layer.
    if (!bThroughputEventsEnabled_.load())
    {
        TAF_PA_DEBUG("Throughput events gated: no callbacks registered yet, dropping event.");
        return;
    }
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<ThroughputEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = throughputEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(throughputInfoList, cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddThroughputEventsCallback
(
    taf_pa_data_ThroughputEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");

    bool needRegister = false;
    {
        std::unique_lock lock(dataConnectionCbksMtx_);
        // Add the callback
        ThroughputEventsCallbackEntry_t entry = {throughputEventsCallbackId_, callBack, context};
        throughputEventsCallbacks_.push_back(entry);
        // Give ID back to app
        id = throughputEventsCallbackId_;
        // Increment the ID.
        throughputEventsCallbackId_++;
        TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
        TAF_PA_INFO("Number of registered callbacks: %zu", throughputEventsCallbacks_.size());

        // Decide first-registration under the lock, but do NOT call TelSDK while holding it.
        needRegister = (throughputEventsCallbacks_.size() == 1);
    } // Lock released here before any TelSDK call

    if (needRegister)
    {
        TAF_PA_INFO("First throughput callback added: registering THROUGHPUT indication with SDK.");
        telux::data::DataConnectionIndications throughputIndication;
        throughputIndication.set(
            telux::data::DataConnectionIndicationsType::THROUGHPUT);

        for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
        {
            auto managerIt = dataConnectionManagersMap_.find((SlotId)slotId);
            auto listenerIt = dataConnectionListenersMap_.find((SlotId)slotId);
            if (managerIt != dataConnectionManagersMap_.end() &&
                listenerIt != dataConnectionListenersMap_.end())
            {
                telux::common::Status status = managerIt->second->registerListener(
                    listenerIt->second, throughputIndication);
                if (telux::common::Status::SUCCESS == status)
                {
                    TAF_PA_INFO("THROUGHPUT indication registered for slot ID %d.", slotId);
                }
                else
                {
                    TAF_PA_ERROR("Failed to register THROUGHPUT indication for slot ID %d.", slotId);
                }
            }
        }
        bThroughputEventsEnabled_.store(true);
        TAF_PA_INFO("Throughput events gate opened: SDK events will now be forwarded to clients.");
    }
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveThroughputEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    bool needDeregister = false;
    bool found = false;
    {
        std::unique_lock lock(dataConnectionCbksMtx_);
        // Iterate over the vector and remove the one with the provided id.
        for (
            auto cbk = throughputEventsCallbacks_.begin();
            cbk != throughputEventsCallbacks_.end();
            ++cbk)
        {
            if (cbk->id == id)
            {
                TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
                throughputEventsCallbacks_.erase(cbk);
                found = true;
                needDeregister = throughputEventsCallbacks_.empty();
                // Close the gate immediately under the lock so no further events are forwarded
                // while we are about to deregister from TelSDK.
                if (needDeregister)
                    bThroughputEventsEnabled_.store(false);
                break;
            }
        }
    } // Lock released here before any TelSDK call

    if (!found)
    {
        TAF_PA_WARN("Callback not found. Id: %d", id);
        return TAF_PA_NOT_FOUND;
    }

    if (needDeregister)
    {
        TAF_PA_INFO("Last throughput callback removed: deregistering THROUGHPUT indication "
                "from SDK.");
        telux::data::DataConnectionIndications throughputIndication;
        throughputIndication.set(
            telux::data::DataConnectionIndicationsType::THROUGHPUT);

        for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
        {
            auto managerIt = dataConnectionManagersMap_.find((SlotId)slotId);
            auto listenerIt = dataConnectionListenersMap_.find((SlotId)slotId);
            if (managerIt != dataConnectionManagersMap_.end() &&
                listenerIt != dataConnectionListenersMap_.end())
            {
                telux::common::Status status = managerIt->second->deregisterListener(
                    listenerIt->second, throughputIndication);
                if (telux::common::Status::SUCCESS == status)
                {
                    TAF_PA_INFO("THROUGHPUT indication deregistered for slot ID %d.", slotId);
                }
                else
                {
                    TAF_PA_ERROR("Failed to deregister THROUGHPUT indication for slot ID %d.",
                             slotId);
                }
            }
        }
        TAF_PA_INFO("Throughput events gate closed: no remaining callbacks.");
    }
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaSetThroughputReportInterval
(
    PhoneId_e phoneId,
    uint32_t reportInterval
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::ErrorCode errorCode;

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    TAF_PA_INFO("Phone Id: %d, Slot Id: %d, Interval: %u ms", TO_INT(phoneId), TO_INT(slotIDpa),
            reportInterval);

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        TAF_PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    errorCode = dataConnectionManagersMap_[slotId]->setThroughputInterval(reportInterval);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        TAF_PA_ERROR("setThroughputInterval failed. ErrorCode: %d", TO_INT(errorCode));
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("setThroughputInterval succeeded");
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetLastThroughputInfo
(
    PhoneId_e phoneId,
    std::vector<ThroughputInfo_t> &throughputInfoList
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::ErrorCode errorCode;

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, TAF_PA_FAULT,
                                                             "PA phone manager not initialized.");
    taf_pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    TAF_PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        TAF_PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    std::vector<telux::data::ThroughputInfo> sdkThroughputInfoList;
    errorCode = dataConnectionManagersMap_[slotId]->getLastThroughputInfo(sdkThroughputInfoList);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        TAF_PA_ERROR("getLastThroughputInfo failed. ErrorCode: %d", TO_INT(errorCode));
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("getLastThroughputInfo succeeded. Count: %zu", sdkThroughputInfoList.size());

    // Convert SDK throughput info to PA throughput info
    throughputInfoList.clear();
    for (const auto &sdkInfo : sdkThroughputInfoList)
    {
        ThroughputInfo_t paInfo;
        taf::pa::data::Utils::ConvertThroughputInfo(sdkInfo, paInfo);
        // Add phone ID to the info
        paInfo.phoneId = phoneId;
        throughputInfoList.push_back(paInfo);
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetMtuByInterfaceName
(
    const std::string& interfaceName,
    int32_t& mtu
)
{
    if (interfaceName.empty())
    {
        TAF_PA_ERROR("Interface name is empty");
        return TAF_PA_BAD_PARAMETER;
    }

    // Iterate through all slot listeners to find the data call with the matching interface name.
    // The MTU is read directly from IpAddrInfo (SDK-provided) via the active IDataCall object.
    for (auto& entry : dataConnectionListenersMap_)
    {
        // The map stores the SDK listener interface; the MTU lookup lives on our listener class.
        auto listener = std::dynamic_pointer_cast<TafPaTeluxDataConnectionListener>(entry.second);
        if (!listener)
        {
            continue;
        }
        taf_pa_result_t result = listener->GetMtuByInterfaceName(interfaceName, mtu);
        if (TAF_PA_OK == result)
        {
            return TAF_PA_OK;
        }
        else if (TAF_PA_NOT_FOUND != result)
        {
            // Interface found but MTU not available
            return result;
        }
        // TAF_PA_NOT_FOUND: interface not in this slot's listener, try next slot
    }

    TAF_PA_WARN("Interface %s not found in any active data calls", interfaceName.c_str());
    return TAF_PA_NOT_FOUND;
}
