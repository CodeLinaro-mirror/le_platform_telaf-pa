/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <atomic>


#include "tafSocksPa.hpp"
#include "taf_prop_net.hpp"
#include "tafInternalCommonPa.h"

#include <telux/data/DataFactory.hpp>
#include <telux/data/net/SocksManager.hpp>

#define ENABLE_SOCKS_TIMEOUT 30

class taf_SocksAdaptor
{
        public:
            static taf_SocksAdaptor &getInstance();

            pa_result_t initialize();

            std::atomic<bool> isInitialized{false};

            std::shared_ptr<telux::data::net::ISocksManager> getSocksManager()
            {
              return socksManager;
            }

            std::shared_ptr<telux::data::net::ISocksManager> socksManager = nullptr;

            taf_pa_socks_CallCb callCbEnableAsync;
            taf_pa_socks_CallCb callCbDisableAsync;

            /*
            * @brief A callback class must be provided when invoke telsdk API.
            */
            class tafSocksCallback
            {
              public:
                static void enableSocksAsyncResponse(telux::common::ErrorCode error);
                static void disableSocksAsyncResponse(telux::common::ErrorCode error);
            };
};


/*======================================================================

 FUNCTION        tafSocksCallback::enableSocksAsyncResponse

 DESCRIPTION     Call back function for enabling SOCKS asynchronously.

 DEPENDENCIES    The initialization of Socks.

 PARAMETERS      [IN] telux::common::ErrorCode error: The error code.

 RETURN VALUE    None.

======================================================================*/
void taf_SocksAdaptor::tafSocksCallback::enableSocksAsyncResponse(telux::common::ErrorCode error)
{
    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    pa_result_t result = PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        result = PA_FAULT;
    }

    if (pSocksAdaptor.callCbEnableAsync)
    {
      pSocksAdaptor.callCbEnableAsync(result,nullptr);
    }
}

/*======================================================================

 FUNCTION        tafSocksCallback::disableSocksAsyncResponse

 DESCRIPTION     Call back function for disabling SOCKS asynchronously.

 DEPENDENCIES    The initialization of Socks.

 PARAMETERS      [IN] telux::common::ErrorCode error: The error code.

 RETURN VALUE    None.

======================================================================*/
void taf_SocksAdaptor::tafSocksCallback::disableSocksAsyncResponse(telux::common::ErrorCode error)
{

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    pa_result_t result = PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        result = PA_FAULT;
    }

    if (pSocksAdaptor.callCbDisableAsync)
    {
      pSocksAdaptor.callCbDisableAsync(result,nullptr);
    }
}


taf_SocksAdaptor &taf_SocksAdaptor::getInstance
(
    void
)
{
    static taf_SocksAdaptor instance;
    return instance;
}

pa_result_t taf_SocksAdaptor::initialize()
{
    PA_INFO("Initializing SOCKS adaptor");
    auto &dataFactory = telux::data::DataFactory::getInstance();

    if (socksManager == nullptr)
    {
        socksManager = dataFactory.getSocksManager(
            telux::data::OperationType::DATA_LOCAL);
    }

    if (socksManager == nullptr)
    {
        PA_INFO("Socks manager initialize error...");
        return PA_FAULT;
    }

    telux::common::ServiceStatus subSystemStatus =
        socksManager->getServiceStatus();

    if (subSystemStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("Socks subsystem is not ready, waiting for it to be ready...");

        auto socksMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        socksManager = dataFactory.getSocksManager(
            telux::data::OperationType::DATA_LOCAL,
            [socksMgrPromPtr](telux::common::ServiceStatus status) {
                PA_INFO("Getting status:%d from socks manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        socksMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        socksMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    PA_ERROR("Exception setting socks manager promise: %s", e.what());
                } catch (...) {
                    PA_ERROR("Unknown error setting socks manager promise");
                }
            });

        if (socksManager == nullptr) {
            PA_ERROR("Failed to get socks manager with init callback");
            return PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            socksMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(ENABLE_SOCKS_TIMEOUT));

        if (std::future_status::timeout == waitStatus) {
            PA_ERROR("Timeout waiting for socks subsystem");
            return PA_TIMEOUT;
        } else {
            subSystemStatus = initFuture.get();
        }
    }

    if (subSystemStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("socksManager component is ready...");
        return PA_OK;
    } else {
        PA_CRIT("unable to init socksManager component, status=%d",
                static_cast<int>(subSystemStatus));
        socksManager = nullptr;
        return PA_FAULT;
    }
}

/* Implementation */

pa_result_t taf_pa_socks_Init()
{
    PA_INFO("Default platform adatper implementation");

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    pa_result_t result = pSocksAdaptor.initialize();
    if (result == PA_OK)
    {
        PA_INFO("Socks platform adapter initialization is done");
        pSocksAdaptor.isInitialized = true;
    }
    else
    {
        PA_CRIT("Failed to initialize Socks platform adapter, ret: %d", result);
        pSocksAdaptor.isInitialized = false;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 *
 * @return PA_FAULT                      Failed
 *         PA_BAD_PARAMETER              Invalid deviceMode
 *         PA_OK                         Succeeded
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_SetDeviceMode
(
    taf_pa_net_DeviceMode_t deviceMode  ///< [IN] Device mode
)
{
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 *
 * @return taf_net_DeviceMode_t          Device mode
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_GetDeviceMode
(
    taf_pa_net_DeviceMode_t* deviceModePtr
)
{
    if (deviceModePtr) *deviceModePtr = TAF_PA_NET_DEVICE_NONE;
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_SetSocksAuthMethod
(
    taf_pa_net_AuthMethod_t authMethod
)
{
    PA_INFO("Actual taf_pa_net_SetSocksAuthMethod implementation");

    taf_prop_net_AuthMethod_t auth = TAF_PROP_NET_SOCKS_UNKNOWN;

    auth = static_cast<taf_prop_net_AuthMethod_t>(authMethod);

    if(auth != TAF_PROP_NET_SOCKS_NONE && auth != TAF_PROP_NET_SOCKS_USER_PASSWD)
        return PA_FAULT;

    return taf_prop_net_SetSocksAuthMethod(auth);

}

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_GetSocksAuthMethod
(
    taf_pa_net_AuthMethod_t* authMethodPtr
)
{
    PA_INFO("Actual taf_pa_net_GetSocksAuthMethod implementation");

    taf_prop_net_AuthMethod_t auth = taf_prop_net_GetSocksAuthMethod();
    if (authMethodPtr) *authMethodPtr = static_cast<taf_pa_net_AuthMethod_t>(auth);
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_SetSocksLanInterface
(
    const char* ifName
)
{
    PA_INFO("Actual taf_pa_net_SetSocksLanInterface implementation");

    return taf_prop_net_SetSocksLanInterface(ifName);
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_GetSocksLanInterface
(
    char* ifName,
    size_t ifNameSize
)
{
    PA_INFO("Actual taf_pa_net_GetSocksLanInterface implementation");

    return taf_prop_net_GetSocksLanInterface(ifName, ifNameSize);
}

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_AddSocksAssociation
(
    const char* userName,
    uint32_t profileId
)
{
    PA_INFO("Actual taf_pa_net_AddSocksAssociation implementation");

    return taf_prop_net_AddSocksAssociation(userName, profileId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_RemoveSocksAssociation
(
    const char* userName
)
{
    PA_INFO("Actual taf_pa_net_RemoveSocksAssociation implementation");

    return taf_prop_net_RemoveSocksAssociation(userName);
}

/*======================================================================

 FUNCTION        taf_Socks::EnableSocksCmdSync

======================================================================*/
pa_result_t taf_pa_net_EnableSocksCmdSync()
{
    PA_INFO("Actual taf_pa_net_EnableSocksCmdSync implementation");

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    pa_result_t result;
    std::chrono::seconds span(ENABLE_SOCKS_TIMEOUT);
    if (socksMgr == NULL) {
        PA_ERROR("socksMgr is null");
        return PA_NOT_FOUND;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto enableSocksRespCb = [promisePtr](telux::common::ErrorCode error)
    {
        try
        {
          if (error != telux::common::ErrorCode::SUCCESS)
          {
              PA_ERROR( "Request failed with errorCode: %d " , static_cast<int>(error));
              promisePtr->set_value(PA_FAULT);
          }
          else
          {
               PA_DEBUG("Request processed successfully \n");
               promisePtr->set_value(PA_OK);
          }
      }
      catch (const std::future_error& e)
      {
          PA_ERROR("Future error in callback: %s", e.what());
      }
      catch (const std::exception& e)
      {
         PA_ERROR("Exception in callback: %s", e.what());
      }
      catch (...)
      {
         PA_ERROR("Unknown error in enable socks callback.");
      }
   };

    telux::common::Status status = socksMgr->enableSocks(true, enableSocksRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Enable SOCKS timeout for %d seconds", ENABLE_SOCKS_TIMEOUT);
            result = PA_TIMEOUT;
        }
        else
        {
            result = futureResult.get();
        }

        return result;
    }
    else
    {
        PA_ERROR( "ERROR - Failed to enable SOCKS, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
    return PA_OK;
}

/*======================================================================

 FUNCTION        taf_Socks::DisableSocksCmdSync

======================================================================*/
pa_result_t taf_pa_net_DisableSocksCmdSync
(

)
{
    PA_INFO("Actual taf_pa_net_DisableSocksCmdSync implementation");

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    pa_result_t result;
    std::chrono::seconds span(ENABLE_SOCKS_TIMEOUT);
    if (socksMgr == NULL) {
        PA_ERROR("socksMgr is null");
        return PA_NOT_FOUND;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto disbleSocksRespCb = [promisePtr](telux::common::ErrorCode error)
    {
        try
        {
          if (error != telux::common::ErrorCode::SUCCESS)
          {
              PA_ERROR( "Request failed with errorCode: %d " , static_cast<int>(error));
              promisePtr->set_value(PA_FAULT);
          }
          else
          {
               PA_DEBUG("Request processed successfully \n");
               promisePtr->set_value(PA_OK);
          }
      }
      catch (const std::future_error& e)
      {
          PA_ERROR("Future error in callback: %s", e.what());
      }
      catch (const std::exception& e)
      {
         PA_ERROR("Exception in callback: %s", e.what());
      }
      catch (...)
      {
         PA_ERROR("Unknown error in enable socks callback.");
      }
   };


    telux::common::Status status = socksMgr->enableSocks(false, disbleSocksRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Disable SOCKS timeout for %d seconds", ENABLE_SOCKS_TIMEOUT);
            result = PA_TIMEOUT;
        }
        else
        {
            result = futureResult.get();
        }

        return result;
    }
    else
    {
        PA_ERROR( "ERROR - Failed to disable SOCKS, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }

    return PA_OK;
}

/*======================================================================

 FUNCTION        EnableSocksCmdSync

 DESCRIPTION     ASynchronously enable socks.

 DEPENDENCIES    The initialization of socks.

 PARAMETERS      None

 RETURN VALUE    pa_result_t
                     PA_OK:                      Success.
                     PA_FAULT                    Failure.

======================================================================*/
pa_result_t taf_pa_net_EnableSocksCmdASync(taf_pa_socks_CallCb callback,void *contextPtr)
{

    PA_INFO("Actual taf_pa_net_EnableSocksCmdASync implementation");
    PA_UNUSED(contextPtr);

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    pSocksAdaptor.callCbEnableAsync = callback;

    if (socksMgr == NULL) {
        PA_ERROR("socksMgr is null");
        return PA_NOT_FOUND;
    }

    telux::common::Status status = socksMgr->enableSocks(true, taf_SocksAdaptor::tafSocksCallback::enableSocksAsyncResponse);

    if (status == telux::common::Status::SUCCESS)
    {
       return PA_OK;
    }
    return PA_FAULT;

}


/*======================================================================

 FUNCTION        DisableSocksCmdSync

 DESCRIPTION     ASynchronously disable socks.

 DEPENDENCIES    The initialization of socks.

 PARAMETERS      None

 RETURN VALUE    pa_result_t
                     PA_OK:                      Success.
                     PA_FAULT                    Failure.

======================================================================*/
pa_result_t taf_pa_net_DisableSocksCmdASync(taf_pa_socks_CallCb callback,void *contextPtr)
{

    PA_INFO("Actual taf_pa_net_DisableSocksCmdASync implementation");
    PA_UNUSED(contextPtr);

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    pSocksAdaptor.callCbDisableAsync = callback;

    if (socksMgr == NULL) {
        PA_ERROR("socksMgr is null");
        return PA_NOT_FOUND;
    }

    telux::common::Status status = socksMgr->enableSocks(false, taf_SocksAdaptor::tafSocksCallback::disableSocksAsyncResponse);

    if (status == telux::common::Status::SUCCESS)
    {
       return PA_OK;
    }
    return PA_FAULT;
}



pa_result_t taf_pa_socks_Deinit()
{
    PA_INFO("Starting SOCKS platform adaptor deinitialization...");
    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    // Check if Init() was successfully called
    if (!pSocksAdaptor.isInitialized)
    {
        PA_WARN("SOCKS Deinit() called before Init() was successfully called");
        return PA_FAULT;
    }

    PA_INFO("Clearing SOCKS callbacks");
    pSocksAdaptor.callCbEnableAsync = nullptr;
    pSocksAdaptor.callCbDisableAsync = nullptr;
    PA_INFO("Resetting socksManager");
    pSocksAdaptor.socksManager.reset();
    pSocksAdaptor.isInitialized = false;
    PA_INFO("SOCKS platform adaptor deinitialization complete.");
    return PA_OK;
}

