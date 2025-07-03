/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "taf_pa_time.hpp"

//For GNSS time
#include <telux/platform/PlatformFactory.hpp>
#include <telux/platform/TimeManager.hpp>
#include <telux/platform/TimeListener.hpp>
#include <condition_variable>

//For network time
#include <telux/common/CommonDefines.hpp>
#include <telux/common/DeviceConfig.hpp>
#include <telux/tel/Phone.hpp>
#include <telux/tel/PhoneDefines.hpp>
#include <telux/tel/PhoneFactory.hpp>
#include <telux/tel/PhoneListener.hpp>
#include <telux/data/ServingSystemManager.hpp>

//For GNSS time
using namespace telux::platform;

//For GNSS + network
using namespace telux::common;

//For GNSS time
std::shared_ptr<ITimeListener> gnssTimeListener = nullptr;
std::shared_ptr<ITimeManager> timeManager = nullptr;

//For c++ standard
using namespace std;

//--------------------------------------------------------------------------------------------------
/**
 * Globle pointer for phone manager.
 */
//--------------------------------------------------------------------------------------------------
std::shared_ptr<telux::tel::IPhoneManager> phoneManager = nullptr;


//--------------------------------------------------------------------------------------------------
/**
 * Lock and condition used for time manager.
 */
//--------------------------------------------------------------------------------------------------
std::mutex mtx;
std::condition_variable cv;


//--------------------------------------------------------------------------------------------------
/**
 * Prototype for network time handler.
 */
//--------------------------------------------------------------------------------------------------
static taf_pa_time_NetworkChangeHandler_t NetworkChangeHandlerPtr = nullptr;
static taf_pa_time_NetworkInfoHandler_t NetworkRespHandlerPtrs[NETWORK_SLOT_NUM_MAX + 1]
                                        = { nullptr };

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for GNSS time handler.
 */
//--------------------------------------------------------------------------------------------------
static taf_pa_time_GnssUtcTimeUpdateHandler_t GnssUtcTimeUpdateHandlerPtr = nullptr;


//--------------------------------------------------------------------------------------------------
/**
 * Mask for GNSS/CV2X time selection.
 */
//--------------------------------------------------------------------------------------------------
TimeTypeMask SupportTimeMask;

class taf_TimeGnssListener : public telux::platform::ITimeListener
{
    public:
        void onGnssUtcTimeUpdate(const uint64_t utc) override;
};

class taf_TimeServingSystemListener : public telux::tel::IServingSystemListener
{
    public:
        uint8_t phone = NETWORK_PHONE_1;
        taf_TimeServingSystemListener(uint8_t phone);
        void onNetworkTimeChanged(telux::tel::NetworkTimeInfo info) override;
};

std::shared_ptr<telux::tel::IServingSystemManager>
                        servingSystemManagers[NETWORK_SLOT_NUM_MAX + 1] = { nullptr };
std::shared_ptr<taf_TimeServingSystemListener>
                             servSysListeners[NETWORK_SLOT_NUM_MAX + 1] = { nullptr };

//--------------------------------------------------------------------------------------------------
/**
 * GNSS UTC time notification for local GNSS time update.
 */
//--------------------------------------------------------------------------------------------------
void taf_TimeGnssListener::onGnssUtcTimeUpdate
(
    const uint64_t utc
)
{
    if (GnssUtcTimeUpdateHandlerPtr != nullptr)
    {
        GnssUtcTimeUpdateHandlerPtr(utc);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * GNSS UTC time initialization.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_gnss_Init(void)
{
    auto &platformFactory = PlatformFactory::getInstance();
    bool statusUpdated = false;
    auto servicStatus = telux::common::ServiceStatus::SERVICE_UNAVAILABLE;
    auto statusCb = [&statusUpdated, &servicStatus](telux::common::ServiceStatus status)
    {
        std::lock_guard<std::mutex> lock(mtx);
        statusUpdated = true;
        servicStatus = status;
        cv.notify_all();
    };

    timeManager = platformFactory.getTimeManager(statusCb);
    if (timeManager)
    {
        // Wait for time manager to be ready
        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [&statusUpdated] { return statusUpdated; });
    }

    if (servicStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_INFO("Time manager is ready");
    }
    else
    {
        PA_ERROR("Unable to initialize time manager");
        return PA_UNAVAILABLE;
    }

    SupportTimeMask.set(SupportedTimeType::GNSS_UTC_TIME);
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register the GNSS time listener.
 *
 * @return
 *     - PA_OK -- Succeeded.
 *     - PA_UNAVAILABLE -- If any error occurs.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_RegGnssTimeListener(void)
{
    if (gnssTimeListener == nullptr)
    {
        gnssTimeListener = std::make_shared<taf_TimeGnssListener>();
        auto myStatus = timeManager->registerListener(gnssTimeListener, SupportTimeMask);
        if (myStatus != Status::SUCCESS)
        {
            PA_ERROR("Failed to register time listener");
            gnssTimeListener = nullptr;
            return PA_UNAVAILABLE;
        }
        PA_DEBUG("gnssTimeListener was successfully registered");
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * De-Register the GNSS time listener.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_DeregGnssTimeListener(void)
{
    telux::common::Status status;
    if (timeManager && gnssTimeListener != nullptr)
    {
        status = timeManager->deregisterListener(gnssTimeListener, SupportTimeMask);
        if (status == telux::common::Status::SUCCESS)
        {
            PA_DEBUG("GnssTimeListener was successfully deregistered");
            gnssTimeListener = nullptr;
            return PA_OK;
        }
        PA_ERROR("Deregister the GnssTimeListener failed");
        return PA_FAULT;
    }
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register GNSS UTC time update callback handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_time_RegGnssUtcTimeUpdateHandler
(
    taf_pa_time_GnssUtcTimeUpdateHandler_t handlerFunc
)
{
    if (handlerFunc == nullptr)
    {
        PA_ERROR("Parameter is NULL");
        return PA_BAD_PARAMETER;
    }

    if (GnssUtcTimeUpdateHandlerPtr != nullptr)
    {
        PA_ERROR("GNSS UTC time update handler already registered.");
        return PA_FAULT;
    }

    GnssUtcTimeUpdateHandlerPtr = handlerFunc;
    return PA_OK;
}


pa_result_t IsPhoneSubSystemReady()
{
    if (phoneManager == nullptr)
    {
        auto &phoneFactory = telux::tel::PhoneFactory::getInstance();
        phoneManager = phoneFactory.getPhoneManager();
    }

    if (phoneManager != nullptr)
    {
        // Check if telephony subsystem is ready
        bool subSystemStatus = phoneManager->isSubsystemReady();
        if (!subSystemStatus)
        {
            PA_INFO("Telephony subsystem wait to be ready...");
            std::future<bool> f = phoneManager->onSubsystemReady();
            //  Wait until the subsystem is ready.
            subSystemStatus = f.get();
        }
        return PA_OK;
    }

    PA_ERROR("Telephony subsystem is not ready");
    return PA_UNAVAILABLE;
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the network time.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_network_Init
(
    int slotId
)
{

    if (slotId < 1 || slotId > NETWORK_SLOT_NUM_MAX) {
        PA_ERROR("Invalid network slot id %d", slotId);
        return PA_BAD_PARAMETER;
    }

    if (IsPhoneSubSystemReady() != PA_OK)
    {
        PA_ERROR("Telephony manager is not ready");
        return PA_UNAVAILABLE;
    }

    if (servingSystemManagers[slotId] == nullptr)
    {
        servingSystemManagers[slotId] =
             telux::tel::PhoneFactory::getInstance().getServingSystemManager(slotId);
    }

    if (servingSystemManagers[slotId] != nullptr)
    {
        // Check if serving subsystem is ready
        bool servingSystemStatus = servingSystemManagers[slotId]->isSubsystemReady();
        if (!servingSystemStatus)
        {
            PA_INFO("Serving subsystem wait to be ready...");
            std::future<bool> f = servingSystemManagers[slotId]->onSubsystemReady();
            //  Wait until the subsystem is ready.
            servingSystemStatus = f.get();
        }

        if (servingSystemStatus)
        {
            PA_INFO("Serving subsystem is ready");
            return PA_OK;
        }
    }

    PA_ERROR("Serving manager system is not ready");
    return PA_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register network time Listener.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_time_RegNetworkTimeListener
(
    int slotId
)
{
    PA_DEBUG("Tring to register servSysListeners for slotId %d", slotId);
    telux::common::Status status;
    if ((slotId > NETWORK_SLOT_NUM_MAX)|| slotId < 1 || (servingSystemManagers[slotId] == nullptr))
    {
        PA_ERROR("Invalid network ID or NULL handler.");
        return PA_BAD_PARAMETER;
    }

    auto servSysListener = std::make_shared<taf_TimeServingSystemListener>(slotId);
    status = servingSystemManagers[slotId]->registerListener(servSysListener);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to register serving system listener for slot %d", slotId);
        servSysListeners[slotId] = nullptr;
        return PA_UNAVAILABLE;
    }
    servSysListeners[slotId] = servSysListener;

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * De-Register network time Listener.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_time_DeregNetworkTimeListener
(
    int slotId
)
{
    telux::common::Status status;
    PA_DEBUG("Tring to deregister servSysListeners for slot %d", slotId);

    if (servingSystemManagers[slotId] != nullptr
        && servSysListeners[slotId] != nullptr)
    {
        status = servingSystemManagers[slotId]->deregisterListener(servSysListeners[slotId]);
        if (status == telux::common::Status::SUCCESS)
        {
            servSysListeners[slotId] = nullptr;
            return PA_OK;
        }
        PA_ERROR("Deregister the servSysListeners failed");
        return PA_FAULT;
    }
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Respone network time info.
 */
//--------------------------------------------------------------------------------------------------

void NetworkDateInfoRespone
(
    int slotId,
    telux::tel::NetworkTimeInfo info, ///< [IN] Network time information.
    telux::common::ErrorCode error    ///< [IN] Error code.
)
{
    pa_result_t result = PA_FAULT;
    taf_time_NetTimeInfo_t dateInfo;


    if (NetworkRespHandlerPtrs[slotId] == nullptr)
    {
        PA_ERROR("Date info pointer for slot %d is NULL", slotId);
        return;
    }

    if (error == telux::common::ErrorCode::SUCCESS)
    {
        PA_DEBUG("Respone success: Slot: %d, NITZ:%s", slotId, info.nitzTime.c_str());

        dateInfo.year = info.year;
        dateInfo.month = info.month;
        dateInfo.day = info.day;
        dateInfo.hour = info.hour;
        dateInfo.minute = info.minute;
        dateInfo.second = info.second;
        dateInfo.dayOfWeek = info.dayOfWeek;
        dateInfo.timeZone = info.timeZone;
        dateInfo.dstAdj = info.dstAdj;
        snprintf(dateInfo.nitzTime, sizeof(dateInfo.nitzTime), "%s" , info.nitzTime.c_str());

        result = PA_OK;
    }
    NetworkRespHandlerPtrs[slotId](dateInfo, slotId, result);
}


void SyncPhoneTimeResponse
(
    telux::tel::NetworkTimeInfo info, ///< [IN] Network time information.
    telux::common::ErrorCode error,   ///< [IN] Error code.
    int phoneId
)
{
    if (phoneManager == nullptr)
    {
        PA_ERROR("Pointer phoneManager is nullptr");
        return;
    }

    int slotId = phoneManager->getSlotIdFromPhoneId(phoneId);
    if (slotId < 1 || slotId > NETWORK_SLOT_NUM_MAX)
    {
        PA_ERROR("Invalid slotId %d derived from phoneId %d", slotId, phoneId);
        return;
    }
    NetworkDateInfoRespone(slotId, info, error);
}

//--------------------------------------------------------------------------------------------------
/**
 * Request network time.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_time_RequestNetworkTime
(
    int slotId,
    taf_pa_time_NetworkInfoHandler_t handlerFunc
)
{
    PA_DEBUG("Request network time for slot %d", slotId);

    if (slotId < 1 || slotId > NETWORK_SLOT_NUM_MAX
        || handlerFunc == nullptr
        || servingSystemManagers[slotId] == nullptr)
    {
        PA_ERROR("Invalid parameter.");
        return PA_BAD_PARAMETER;
    }

    //Save the callback function for this 'slotid'
    NetworkRespHandlerPtrs[slotId] = handlerFunc;

    if (servSysListeners[slotId] == nullptr)
    {
        PA_ERROR("No active servSysListeners for slot %d", slotId);
        return PA_UNAVAILABLE;
    }

    telux::common::Status ret;
    if (slotId == NETWORK_SLOT_1)
    {
        ret = servingSystemManagers[slotId]->requestNetworkTime(
            [](auto info, auto error) { SyncPhoneTimeResponse(info, error, NETWORK_PHONE_1); });
    }
    else if (slotId == NETWORK_SLOT_2)
    {
        ret = servingSystemManagers[slotId]->requestNetworkTime(
            [](auto info, auto error) { SyncPhoneTimeResponse(info, error, NETWORK_PHONE_2); });
    }
    else
    {
        PA_ERROR("Unsupported slot ID %d", slotId);
        return PA_BAD_PARAMETER;
    }

    if (ret != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Request time from slot %d failed, ret:%d",slotId, static_cast<int>(ret));
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * PhoneId update from serving system.
 */
//--------------------------------------------------------------------------------------------------
taf_TimeServingSystemListener::taf_TimeServingSystemListener
(
    uint8_t phoneId ///< [IN] Phone ID.
) : phone(phoneId)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * Listener for network time changes.
 */
//--------------------------------------------------------------------------------------------------
void taf_TimeServingSystemListener::onNetworkTimeChanged
(
    telux::tel::NetworkTimeInfo info ///< [IN] Network time information.
)
{
    if (NetworkChangeHandlerPtr != nullptr)
    {
        int slotId = phoneManager->getPhoneIdFromSlotId(phone);

        taf_time_NetTimeInfo_t dateInfo;
        dateInfo.year = info.year;
        dateInfo.month = info.month;
        dateInfo.day = info.day;
        dateInfo.hour = info.hour;
        dateInfo.minute = info.minute;
        dateInfo.second = info.second;
        dateInfo.dayOfWeek = info.dayOfWeek;
        dateInfo.timeZone = info.timeZone;
        dateInfo.dstAdj = info.dstAdj;
        snprintf(dateInfo.nitzTime, sizeof(dateInfo.nitzTime), "%s" , info.nitzTime.c_str());

        PA_DEBUG("SlotId %d, Phone %d, NITZ:%s", slotId, phone, info.nitzTime.c_str());
        NetworkChangeHandlerPtr(dateInfo, slotId);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Register network time change callback handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_time_RegNetworkTimeChangeHandler
(
    taf_pa_time_NetworkChangeHandler_t handlerFunc
)
{
    if ((handlerFunc != nullptr) && (NetworkChangeHandlerPtr == nullptr))
    {
        NetworkChangeHandlerPtr = handlerFunc;
        PA_INFO("Network time change handler registered.");
        return PA_OK;
    }

    PA_ERROR("Network time change handler register failed.");
    return PA_FAULT;
}
