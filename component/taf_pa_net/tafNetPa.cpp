/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#include <set>
#include "tafNetPa.hpp"
#include "tafSocksPa.hpp"
#include "tafVlanPa.hpp"
#include "tafL2tpPa.hpp"

#include "taf_ns_net.hpp"

#include <telux/tel/PhoneFactory.hpp>

/* Implementation */

class taf_NetAdaptor
{
        public:
            static taf_NetAdaptor &getInstance();

            pa_result_t initialize();

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

pa_result_t taf_NetAdaptor::initialize()
{
    PA_INFO("Actual platform adatper implementation");

    auto &phoneFactory = telux::tel::PhoneFactory::getInstance();
    phoneManager = phoneFactory.getPhoneManager();
    if (!phoneManager) {
        PA_CRIT("Failed to get phone manager");
        return PA_FAULT;
    }

    telux::common::ServiceStatus phoneMgrStatus = phoneManager->getServiceStatus();

    if (phoneMgrStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("Telephony subsystem is not ready, waiting for it to be ready...");

        auto phoneMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        phoneManager = phoneFactory.getPhoneManager(
            [phoneMgrPromPtr](telux::common::ServiceStatus status) {
                PA_INFO("Getting status:%d from phone manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        phoneMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        phoneMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    PA_ERROR("Exception setting phone manager promise: %s", e.what());
                } catch (...) {
                    PA_ERROR("Unknown error setting phone manager promise");
                }
            });

        if (!phoneManager) {
            PA_CRIT("Failed to get phone manager with init callback");
            return PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            phoneMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(30));

        if (std::future_status::timeout == waitStatus) {
            PA_CRIT("Timeout waiting for telephony subsystem");
            return PA_TIMEOUT;
        } else {
            phoneMgrStatus = initFuture.get();
        }
    }

    if (phoneMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("Telephony subsystem is ready.");
        return PA_OK;
    } else {
        PA_CRIT("Failed to init telephony subsystem, status=%d",
                static_cast<int>(phoneMgrStatus));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_Init()
{
    PA_INFO("Actual platform adatper implementation");

    int32_t result_ns = taf_ns_net_Init();
    if (result_ns == TAF_NS_NET_RESULT_NOT_IMPLEMENTED)
        PA_INFO("NET proprietary platform adaptor is not implemented.");
    else if (result_ns == TAF_NS_NET_RESULT_OK)
        PA_INFO("NET proprietary platform adaptor initialization is done.");

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();

    pa_result_t result = pNetAdaptor.initialize();
    if (result == PA_OK)
    {
        PA_INFO("Net platform adapter initialization is done");
    }
    else
    {
        PA_CRIT("Failed to initialize Net platform adapter, ret: %d", result);
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get phone ID from slot ID
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_GetPhoneIdFromSlotId
(
    uint8_t slotId,
    uint8_t *phoneIdPtr
)
{
    int retPhoneId;
    pa_result_t result = PA_OK;

    if (phoneIdPtr == nullptr) {
        PA_ERROR("Null ptr(phoneIdPtr)");
        return PA_BAD_PARAMETER;
    }

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();
    auto phoneMgr = pNetAdaptor.getPhoneManager();

    if(phoneMgr)
    {
        retPhoneId = phoneMgr->getPhoneIdFromSlotId(slotId);
        if(retPhoneId < 0)
        {
            PA_ERROR("Invalid phone id");
            result = PA_FAULT;
        }
        else
        {
            *phoneIdPtr = (uint8_t)retPhoneId;
            result = PA_OK;
        }
    }
    else
    {
        PA_ERROR("Phone manager is NULL");
        result = PA_FAULT;
    }

    PA_DEBUG("result =%d, slotId = %d, phoneId = %d", result, slotId, *phoneIdPtr);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get slot ID from phone ID
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_GetSlotIdFromPhoneId
(
    uint8_t phoneId,
    uint8_t *slotIdPtr
)
{
    int retSlotId;
    pa_result_t result = PA_OK;

    if (slotIdPtr == nullptr) {
        PA_ERROR("Null ptr(slotIdPtr)");
        return PA_BAD_PARAMETER;
    }

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();
    auto phoneMgr = pNetAdaptor.getPhoneManager();

    if(phoneMgr)
    {
        retSlotId = phoneMgr->getSlotIdFromPhoneId(phoneId);
        if(retSlotId < 0)
        {
            PA_ERROR("Invalid slot id");
            result = PA_FAULT;
        }
        else
        {
            *slotIdPtr = (uint8_t)retSlotId;
            result = PA_OK;
        }
    }
    else
    {
        PA_ERROR("Phone manager is NULL");
        result = PA_FAULT;
    }

    PA_DEBUG("result =%d, slotId = %d, phoneId = %d",result, *slotIdPtr, phoneId);

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Return supported slot IDs
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_GetSupportedSlotIds(std::vector<uint8_t> &slotIds)
{
    slotIds.clear();

    auto &pNetAdaptor = taf_NetAdaptor::getInstance();
    auto phoneMgr = pNetAdaptor.getPhoneManager();
    if (!phoneMgr)
    {
        PA_ERROR("Phone manager is NULL");
        return PA_FAULT;
    }

    // Get phone IDs from TelSDK
    std::vector<int> phoneIds;
    telux::common::Status status = phoneMgr->getPhoneIds(phoneIds);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_ERROR("getPhoneIds failed, status=%d", static_cast<int>(status));
        return PA_FAULT;
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
            PA_WARN("getSlotIdFromPhoneId(%d) returned %d", phoneId, slot);
        }
    }

    for (int slot : uniqueSlots)
    {
        slotIds.push_back(static_cast<uint8_t>(slot));
    }

    PA_INFO("Supported slots count: %zu", slotIds.size());
    return PA_OK;
}
