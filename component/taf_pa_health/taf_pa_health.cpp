/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_pa_health.hpp"

#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>

#include <telux/platform/PlatformFactory.hpp>
#include <condition_variable>
#include <telux/platform/SubsystemFactory.hpp>
#include <telux/platform/SubsystemManager.hpp>
#include <telux/common/CommonDefines.hpp>
#include <telux/data/ServingSystemManager.hpp>
#include <telux/tel/PhoneManager.hpp>
#include <telux/tel/Phone.hpp>
#include <telux/tel/PhoneDefines.hpp>
#include <telux/tel/PhoneFactory.hpp>
#include <telux/tel/PhoneListener.hpp>

using namespace telux::platform;

using namespace telux::common;

using namespace std;

static taf_pa_health_ModemStatusUpdateHandler_t ModemStatusUpdateHandlerPtr = nullptr;
static taf_pa_health_ModemOperatingModeUpdateHandler_t ModemOperationModeUpdateHandlerPtr = nullptr;
std::shared_ptr<telux::platform::ISubsystemManager> subsystemMgr;

class tafHmsListenerPA : public telux::platform::ISubsystemListener {
        public:
        tafHmsListenerPA() {};
        ~tafHmsListenerPA() {};
        static tafHmsListenerPA& GetInstance()
        {
            static tafHmsListenerPA instance;
            return instance;
        }
        void onStateChange(telux::common::SubsystemInfo subsystemInfo,
            telux::common::OperationalStatus newOperationalStatus) override;
    };

class ModemStatusPA : public telux::tel::IOperatingModeCallback,
                        public std::enable_shared_from_this<ModemStatusPA> {
    public:
        ModemStatusPA() {};
        ~ModemStatusPA() {};
        static ModemStatusPA& GetInstance()
        {
            static ModemStatusPA instance;
            return instance;
        }
        void operatingModeResponse(telux::tel::OperatingMode operatingMode,
                                   telux::common::ErrorCode error) override;

        std::shared_ptr<std::promise<bool>> responsePromise_{nullptr};
        std::shared_ptr<telux::tel::IPhoneManager> phoneManager_{nullptr};

};

std::shared_ptr<tafHmsListenerPA> stateListener;
std::shared_ptr<ModemStatusPA> modemStatus = std::make_shared<ModemStatusPA>();


pa_result_t taf_pa_health_PhoneInit(void)
{
    if (!modemStatus->phoneManager_)
    {
        auto prom = std::make_shared<std::promise<telux::common::ServiceStatus>>();
        auto tempManager = telux::tel::PhoneFactory::getInstance().getPhoneManager(
            [prom]( telux::common::ServiceStatus status)
            {
                try
                {
                    prom->set_value(status);
                }
                catch (const std::future_error &e)
                {
                    PA_ERROR("Promise already satisfied: %s", e.what());
                }
            });

        if (!tempManager)
        {
            PA_ERROR("ERROR - Failed to get Phone Manager");
            return PA_FAULT;
        }

        const auto statusNow = tempManager->getServiceStatus();
        if (statusNow != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Phone Manager subsystem is not ready, please wait");
        }

        auto fut = prom->get_future();
        if (fut.wait_for(std::chrono::seconds(TAF_HMS_PHONE_MANAGER_TIMEOUT))
            == std::future_status::ready)
        {
            auto status = fut.get(); // Wait until available
            if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                PA_ERROR("ERROR - Unable to initialize telephony subsystem");
                return PA_FAULT;
            }
            PA_INFO("Phone Manager subsystem is ready.");
            modemStatus->phoneManager_ = tempManager;
        }
        else
        {
            PA_ERROR("Timeout for waiting status callback");
            return PA_FAULT;
        }
    }
    return PA_OK;
}

pa_result_t taf_pa_health_ReqPhoneOperatingMode() {
    if (!modemStatus->phoneManager_) {
        PA_ERROR("reqsOperatingMode called before phoneInit");
        return PA_FAULT;
    }
    modemStatus->phoneManager_->requestOperatingMode(modemStatus->shared_from_this());
    return PA_OK;
}

ModemOperationalStatusPA MapOperationStatus(telux::common::OperationalStatus status) {
    if(status == telux::common::OperationalStatus::UNAVAILABLE)
    {
        return ModemOperationalStatusPA::UNAVAILABLE;
    }

    if(status == telux::common::OperationalStatus::OPERATIONAL)
    {
        return ModemOperationalStatusPA::OPERATIONAL;
    }
    return ModemOperationalStatusPA::UNKNOWN;
}

SubsytemInfoPA MapSubsytemInfo(telux::common::SubsystemInfo subsystemInfo) {
    SubsytemInfoPA subsytemInfoPA;
    subsytemInfoPA.subsystemBitmask = subsystemInfo.subsystems;
    if(subsystemInfo.location == telux::common::ProcType::LOCAL_PROC)
    {
        subsytemInfoPA.procType = ProcTypePA::LOCAL_PROC;
    }
    else if(subsystemInfo.location ==  telux::common::ProcType::REMOTE_PROC)
    {
        subsytemInfoPA.procType = ProcTypePA::REMOTE_PROC;
    }
    else
    {
        subsytemInfoPA.procType = ProcTypePA::LOCAL_PROC;
    }
    return subsytemInfoPA;
}

void tafHmsListenerPA:: onStateChange(telux::common::SubsystemInfo subsystemInfo,
            telux::common::OperationalStatus newOperationalStatus)
{
    taf_health_ModemStatusInfo_t modemStatusInfo;
    modemStatusInfo.subSystemInfo = MapSubsytemInfo(subsystemInfo);
    modemStatusInfo.newOperationalStatus = MapOperationStatus(newOperationalStatus);
    if (ModemStatusUpdateHandlerPtr != nullptr)
    {
        ModemStatusUpdateHandlerPtr(modemStatusInfo);
    }
}

pa_result_t taf_pa_health_ModemNotificationInit(void)
{
    telux::common::ServiceStatus serviceStatus;
    std::promise<telux::common::ServiceStatus> p{};

    auto &subsystemFact = telux::platform::SubsystemFactory::getInstance();

    subsystemMgr = subsystemFact.getSubsystemManager(
            [&p](telux::common::ServiceStatus srvStatus) {
        p.set_value(srvStatus);
    });
    if (!subsystemMgr) {
        PA_ERROR("Couldn't get the subsystemMgr");
        return PA_FAULT;
    }

    auto future = p.get_future();
    if (future.wait_for(std::chrono::seconds(TAF_HMS_SUBSYSTEM_MANAGER_TIMEOUT))
            == std::future_status::ready)
    {
        serviceStatus = future.get();
        PA_INFO("serviceStatus get the callback waiting");
        if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            PA_ERROR("ISubsystemManager unavailable");
            return PA_FAULT;
        }
    }
    else
    {
        PA_ERROR("Timeout waiting for serviceStatus callback");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t taf_pa_health_RegModemStatusUpdateHandler
(
    taf_pa_health_ModemStatusUpdateHandler_t handlerFunc
)
{
    if ((handlerFunc != nullptr) && (ModemStatusUpdateHandlerPtr == nullptr))
    {
        ModemStatusUpdateHandlerPtr = handlerFunc;
        PA_INFO("Modem Status Change Handler registered.");
        return PA_OK;
    }

    PA_ERROR("Modem Status Change handler registered.");
    return PA_FAULT;
}

pa_result_t taf_pa_health_RegModemOperationModeUpdateHandler
(
    taf_pa_health_ModemOperatingModeUpdateHandler_t handlerFunc
)
{
    if ((handlerFunc != nullptr) && (ModemOperationModeUpdateHandlerPtr == nullptr))
    {
        ModemOperationModeUpdateHandlerPtr = handlerFunc;
        PA_INFO("Modem Operation Mode Change Handler registered.");
        return PA_OK;
    }

    PA_ERROR("Modem Operation Mode Change handler registered.");
    return PA_FAULT;
}


pa_result_t taf_pa_health_RegModemListener(void)
{
    telux::common::ErrorCode ec;
    telux::common::SubsystemInfo subsysInfo{};
    std::vector<telux::common::SubsystemInfo> listOfSubsystems;
    stateListener = std::make_shared<tafHmsListenerPA>();

    subsysInfo.location = telux::common::ProcType::LOCAL_PROC;
    subsysInfo.subsystems = telux::common::Subsystem::MPSS;
    listOfSubsystems.push_back(subsysInfo);
    ec = subsystemMgr->registerListener(stateListener, listOfSubsystems);
    if (ec != telux::common::ErrorCode::SUCCESS) {
        PA_ERROR("Can't register listener, err ");
        return PA_FAULT;
    }
    PA_INFO("registerListener ok");

    return PA_OK;
}

static std::string OperatingModeToString(telux::tel::OperatingMode operatingMode)
{
    switch (operatingMode)
    {
        case telux::tel::OperatingMode::ONLINE: return "ONLINE";
        case telux::tel::OperatingMode::AIRPLANE: return "AIRPLANE";
        case telux::tel::OperatingMode::FACTORY_TEST: return "FACTORY_TEST";
        case telux::tel::OperatingMode::OFFLINE: return "OFFLINE";
        case telux::tel::OperatingMode::RESETTING: return "RESETTING";
        case telux::tel::OperatingMode::SHUTTING_DOWN: return "SHUTTING_DOWN";
        case telux::tel::OperatingMode::PERSISTENT_LOW_POWER: return "PERSISTENT_LOW_POWER";
        default: return "Unknown";
    }
}

void ModemStatusPA::operatingModeResponse(telux::tel::OperatingMode operatingMode,
                                        telux::common::ErrorCode error)
{
    pa_result_t status = PA_FAULT;
    if (error == telux::common::ErrorCode::SUCCESS)
    {
        status = PA_OK;
        switch (operatingMode)
        {
            case telux::tel::OperatingMode::ONLINE:
            case telux::tel::OperatingMode::AIRPLANE:
            case telux::tel::OperatingMode::FACTORY_TEST:
            case telux::tel::OperatingMode::PERSISTENT_LOW_POWER:
            case telux::tel::OperatingMode::OFFLINE:
            case telux::tel::OperatingMode::RESETTING:
            case telux::tel::OperatingMode::SHUTTING_DOWN:
            default:
                PA_DEBUG("Operating Mode is: %s", OperatingModeToString(operatingMode).c_str());
                break;
        }
    }
    else
    {
        PA_ERROR("Operating Mode unknown, errorCode: %d", static_cast<int>(error));
    }

    if (ModemOperationModeUpdateHandlerPtr != nullptr)
    {
        //Notify the client the connection was restored.
        ModemOperationModeUpdateHandlerPtr(status);
    }
}