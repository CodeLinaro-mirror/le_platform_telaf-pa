/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#include "taf_pa_net.hpp"
#include "taf_pa_socks.hpp"
#include "taf_pa_vlan.hpp"
#include "taf_pa_l2tp.hpp"

#include "taf_ns_net.hpp"

#include <telux/tel/PhoneFactory.hpp>

/* Implementation */

class taf_NetAdaptor
{
        public:
            taf_NetAdaptor() {};
            ~taf_NetAdaptor() {};

            void Init(void);

            static taf_NetAdaptor &getInstance();
            void onInitComplete(telux::common::ServiceStatus status);

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
    //  Check if telephony subsystem is ready
    bool PhSubSystemStatus = phoneManager->isSubsystemReady();

    if (!PhSubSystemStatus) {
        PA_INFO("Wait telephony subsystem  to be ready...");
        std::future<bool> f = phoneManager->onSubsystemReady();
        //  Wait until the subsystem is ready.
        PhSubSystemStatus = f.get();
    }

    PA_INFO("-------waiting result is OK");
    if(!PhSubSystemStatus)
    {
        PA_ERROR("Failed to init telephony subsystem");
        return PA_FAULT;
    }
    return PA_OK;
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
