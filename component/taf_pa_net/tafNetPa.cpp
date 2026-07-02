/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#include <set>
#include "tafNetPa.hpp"
#include "tafSocksPa.hpp"
#include "tafVlanPa.hpp"
#include "tafL2tpPa.hpp"

#include "taf_prop_net.h"
#include "tafInternalCommonPa.h"

#include <telux/tel/PhoneFactory.hpp>
#include <atomic>

/* Implementation */

class taf_NetAdaptor
{
        public:
            static taf_NetAdaptor &getInstance();

            taf_pa_result_t initialize();

            std::atomic<bool> isInitialized{false};

            std::shared_ptr<telux::tel::IPhoneManager> getPhoneManager()
            {
              return phoneManager;
            }

            std::shared_ptr<telux::tel::IPhoneManager> phoneManager = nullptr;
};

taf_NetAdaptor &taf_NetAdaptor::getInstance
(
    void
)
{
    static taf_NetAdaptor instance;
    return instance;
}

taf_pa_result_t taf_NetAdaptor::initialize()
{
    TAF_PA_INFO("Actual platform adapter implementation");

    auto &phoneFactory = telux::tel::PhoneFactory::getInstance();
    phoneManager = phoneFactory.getPhoneManager();
    if (!phoneManager) {
        TAF_PA_CRIT("Failed to get phone manager");
        return TAF_PA_FAULT;
    }

    telux::common::ServiceStatus phoneMgrStatus = phoneManager->getServiceStatus();

    if (phoneMgrStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        TAF_PA_INFO("Telephony subsystem is not ready, waiting for it to be ready...");

        auto phoneMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        phoneManager = phoneFactory.getPhoneManager(
            [phoneMgrPromPtr](telux::common::ServiceStatus status) {
                TAF_PA_INFO("Getting status:%d from phone manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        phoneMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        phoneMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    TAF_PA_ERROR("Exception setting phone manager promise: %s", e.what());
                } catch (...) {
                    TAF_PA_ERROR("Unknown error setting phone manager promise");
                }
            });

        if (!phoneManager) {
            TAF_PA_CRIT("Failed to get phone manager with init callback");
            return TAF_PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            phoneMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(30));

        if (std::future_status::timeout == waitStatus) {
            TAF_PA_CRIT("Timeout waiting for telephony subsystem");
            return TAF_PA_TIMEOUT;
        } else {
            phoneMgrStatus = initFuture.get();
        }
    }

    if (phoneMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        TAF_PA_INFO("Telephony subsystem is ready.");
        return TAF_PA_OK;
    } else {
        TAF_PA_CRIT("Failed to init telephony subsystem, status=%d",
                static_cast<int>(phoneMgrStatus));
        return TAF_PA_FAULT;
    }
}

taf_pa_result_t taf_pa_net_Init()
{
    TAF_PA_INFO("Actual platform adapter implementation");

    taf_prop_result_t result_ns = taf_prop_net_Init();
    if (result_ns == TAF_PROP_NOT_IMPLEMENTED)
        TAF_PA_INFO("NET proprietary platform adaptor is not implemented.");
    else if (result_ns == TAF_PROP_OK)
        TAF_PA_INFO("NET proprietary platform adaptor initialization is done.");
    else
        TAF_PA_ERROR("NET proprietary platform adaptor initialization failed, ret: %d",
                 static_cast<int>(result_ns));

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();

    taf_pa_result_t result = pNetAdaptor.initialize();
    if (result == TAF_PA_OK)
    {
        TAF_PA_INFO("Net platform adapter initialization is done");
        pNetAdaptor.isInitialized = true;
    }
    else
    {
        TAF_PA_CRIT("Failed to initialize Net platform adapter, ret: %d", result);
        pNetAdaptor.isInitialized = false;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get phone ID from slot ID
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_GetPhoneIdFromSlotId
(
    uint8_t slotId,
    uint8_t *phoneIdPtr
)
{
    int retPhoneId;
    taf_pa_result_t result = TAF_PA_OK;

    if (phoneIdPtr == nullptr) {
        TAF_PA_ERROR("Null ptr(phoneIdPtr)");
        return TAF_PA_BAD_PARAMETER;
    }

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();
    auto phoneMgr = pNetAdaptor.getPhoneManager();

    if(phoneMgr)
    {
        retPhoneId = phoneMgr->getPhoneIdFromSlotId(slotId);
        if(retPhoneId < 0)
        {
            TAF_PA_ERROR("Invalid phone id");
            result = TAF_PA_FAULT;
        }
        else
        {
            *phoneIdPtr = (uint8_t)retPhoneId;
            result = TAF_PA_OK;
        }
    }
    else
    {
        TAF_PA_ERROR("Phone manager is NULL");
        result = TAF_PA_FAULT;
    }

    TAF_PA_DEBUG("result =%d, slotId = %d, phoneId = %d", result, slotId, *phoneIdPtr);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get slot ID from phone ID
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_GetSlotIdFromPhoneId
(
    uint8_t phoneId,
    uint8_t *slotIdPtr
)
{
    int retSlotId;
    taf_pa_result_t result = TAF_PA_OK;

    if (slotIdPtr == nullptr) {
        TAF_PA_ERROR("Null ptr(slotIdPtr)");
        return TAF_PA_BAD_PARAMETER;
    }

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();
    auto phoneMgr = pNetAdaptor.getPhoneManager();

    if(phoneMgr)
    {
        retSlotId = phoneMgr->getSlotIdFromPhoneId(phoneId);
        if(retSlotId < 0)
        {
            TAF_PA_ERROR("Invalid slot id");
            result = TAF_PA_FAULT;
        }
        else
        {
            *slotIdPtr = (uint8_t)retSlotId;
            result = TAF_PA_OK;
        }
    }
    else
    {
        TAF_PA_ERROR("Phone manager is NULL");
        result = TAF_PA_FAULT;
    }

    TAF_PA_DEBUG("result =%d, slotId = %d, phoneId = %d",result, *slotIdPtr, phoneId);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return supported slot IDs
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_GetSupportedSlotIds(std::vector<uint8_t> &slotIds)
{
    slotIds.clear();

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();
    auto phoneMgr = pNetAdaptor.getPhoneManager();
    if (!phoneMgr)
    {
        TAF_PA_ERROR("Phone manager is NULL");
        return TAF_PA_FAULT;
    }

    // Get phone IDs from TelSDK
    std::vector<int> phoneIds;
    telux::common::Status status = phoneMgr->getPhoneIds(phoneIds);
    if (status != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("getPhoneIds failed, status=%d", static_cast<int>(status));
        return TAF_PA_FAULT;
    }

    // Convert to slot IDs and deduplicate
    std::set<int> uniqueSlots;
    for (int phoneId : phoneIds)
    {
        int slot = phoneMgr->getSlotIdFromPhoneId(phoneId);
        if (slot > 0)
        {
            uniqueSlots.insert(slot);
        }
        else
        {
            TAF_PA_WARN("getSlotIdFromPhoneId(%d) returned %d", phoneId, slot);
        }
    }

    for (int slot : uniqueSlots)
    {
        slotIds.push_back(static_cast<uint8_t>(slot));
    }

    TAF_PA_INFO("Supported slots count: %zu", slotIds.size());
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_net_Deinit()
{
    TAF_PA_INFO("Starting Net platform adaptor deinitialization...");
    auto &pNetAdaptor = taf_NetAdaptor::getInstance();

    // Check if Init() was successfully called
    if (!pNetAdaptor.isInitialized)
    {
        TAF_PA_WARN("Net Deinit() called before Init() was successfully called");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Resetting phoneManager");
    pNetAdaptor.phoneManager.reset();

    taf_prop_result_t nsRes = taf_prop_net_Deinit();
    if (nsRes == TAF_PROP_OK)
    {
        TAF_PA_INFO("taf_prop_net_Deinit() completed successfully.");
    }
    else if (nsRes == TAF_PROP_NOT_IMPLEMENTED)
    {
        TAF_PA_INFO("taf_prop_net_Deinit() not implemented (stub).");
    }
    else
    {
        TAF_PA_ERROR("taf_prop_net_Deinit() failed with result %d.", (int)nsRes);
    }

    pNetAdaptor.isInitialized = false;
    TAF_PA_INFO("Net platform adaptor deinitialization complete.");
    return TAF_PA_OK;
}
