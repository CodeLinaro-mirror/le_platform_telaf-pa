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
#include "taf_prop_net.h"
#include "tafInternalCommonPa.h"

#include <telux/data/DataFactory.hpp>
#include <telux/data/net/SocksManager.hpp>

#define ENABLE_SOCKS_TIMEOUT 30

class taf_SocksListener : public telux::data::net::ISocksListener
{
    public:
        taf_SocksListener(){};
        void onServiceStatusChange(telux::common::ServiceStatus status) override;
};

class taf_SocksAdaptor
{
        public:
            static taf_SocksAdaptor &getInstance();

            taf_pa_result_t initialize();

            std::atomic<bool> isInitialized{false};

            std::shared_ptr<telux::data::net::ISocksManager> getSocksManager()
            {
              return socksManager;
            }

            std::shared_ptr<telux::data::net::ISocksManager> socksManager = nullptr;
            std::shared_ptr<taf_SocksListener> socksListener = nullptr;

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

    taf_pa_result_t result = TAF_PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        result = TAF_PA_FAULT;
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

    taf_pa_result_t result = TAF_PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        result = TAF_PA_FAULT;
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

taf_pa_result_t taf_SocksAdaptor::initialize()
{
    TAF_PA_INFO("Initializing SOCKS adaptor");
    auto &dataFactory = telux::data::DataFactory::getInstance();

    if (socksManager == nullptr)
    {
        socksManager = dataFactory.getSocksManager(
            telux::data::OperationType::DATA_LOCAL);
    }

    if (socksManager == nullptr)
    {
        TAF_PA_INFO("Socks manager initialize error...");
        return TAF_PA_FAULT;
    }

    telux::common::ServiceStatus subSystemStatus =
        socksManager->getServiceStatus();

    if (subSystemStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        TAF_PA_INFO("Socks subsystem is not ready, waiting for it to be ready...");

        auto socksMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        socksManager = dataFactory.getSocksManager(
            telux::data::OperationType::DATA_LOCAL,
            [socksMgrPromPtr](telux::common::ServiceStatus status) {
                TAF_PA_INFO("Getting status:%d from socks manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        socksMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        socksMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    TAF_PA_ERROR("Exception setting socks manager promise: %s", e.what());
                } catch (...) {
                    TAF_PA_ERROR("Unknown error setting socks manager promise");
                }
            });

        if (socksManager == nullptr) {
            TAF_PA_ERROR("Failed to get socks manager with init callback");
            return TAF_PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            socksMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(ENABLE_SOCKS_TIMEOUT));

        if (std::future_status::timeout == waitStatus) {
            TAF_PA_ERROR("Timeout waiting for socks subsystem");
            return TAF_PA_TIMEOUT;
        } else {
            subSystemStatus = initFuture.get();
        }
    }

    if (subSystemStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        TAF_PA_INFO("socksManager component is ready...");
        return TAF_PA_OK;
    } else {
        TAF_PA_CRIT("unable to init socksManager component, status=%d",
                static_cast<int>(subSystemStatus));
        socksManager = nullptr;
        return TAF_PA_FAULT;
    }
}

/* Implementation */

void taf_SocksListener::onServiceStatusChange
(
    telux::common::ServiceStatus status
)
{
    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("SocksManager service status changed to available. Re-initializing.");

        taf_prop_result_t nsRes = taf_prop_net_Init();
        if (nsRes == TAF_PROP_OK)
        {
            TAF_PA_INFO("taf_prop_net_Init() completed successfully after service recovery.");
        }
        else if (nsRes == TAF_PROP_NOT_IMPLEMENTED)
        {
            TAF_PA_INFO("taf_prop_net_Init() not implemented (stub).");
        }
        else
        {
            TAF_PA_ERROR("taf_prop_net_Init() failed with result %d after service recovery.",
                     (int)nsRes);
        }

        pSocksAdaptor.isInitialized = true;
        return;
    }

    TAF_PA_WARN("SocksManager service status changed to unavailable. Calling deinit.");

    if (!pSocksAdaptor.isInitialized)
    {
        TAF_PA_INFO("Skipping deinit because Socks was not initialized.");
        return;
    }

    pSocksAdaptor.callCbEnableAsync = nullptr;
    pSocksAdaptor.callCbDisableAsync = nullptr;
    pSocksAdaptor.isInitialized = false;

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
}

taf_pa_result_t taf_pa_socks_Init()
{
    TAF_PA_INFO("Default platform adatper implementation");

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    taf_pa_result_t result = pSocksAdaptor.initialize();
    if (result == TAF_PA_OK)
    {
        TAF_PA_INFO("Socks platform adapter initialization is done");
        pSocksAdaptor.isInitialized = true;

        pSocksAdaptor.socksListener = std::make_shared<taf_SocksListener>();
        if (pSocksAdaptor.socksManager->registerListener(pSocksAdaptor.socksListener) ==
            telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("Socks service status listener registered.");
        }
        else
        {
            TAF_PA_ERROR("Failed to register socks service status listener.");
        }
    }
    else
    {
        TAF_PA_CRIT("Failed to initialize Socks platform adapter, ret: %d", result);
        pSocksAdaptor.isInitialized = false;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 *
 * @return TAF_PA_FAULT                      Failed
 *         TAF_PA_BAD_PARAMETER              Invalid deviceMode
 *         TAF_PA_OK                         Succeeded
 *
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_SetDeviceMode
(
    taf_pa_net_DeviceMode_t deviceMode  ///< [IN] Device mode
)
{
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 *
 * @return taf_net_DeviceMode_t          Device mode
 *
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_GetDeviceMode
(
    taf_pa_net_DeviceMode_t* deviceModePtr
)
{
    if (deviceModePtr) *deviceModePtr = TAF_PA_NET_DEVICE_NONE;
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_SetSocksAuthMethod
(
    taf_pa_net_AuthMethod_t authMethod
)
{
    TAF_PA_INFO("Actual taf_pa_net_SetSocksAuthMethod implementation");

    taf_prop_net_AuthMethod_t auth = TAF_PROP_NET_SOCKS_UNKNOWN;

    auth = static_cast<taf_prop_net_AuthMethod_t>(authMethod);

    if(auth != TAF_PROP_NET_SOCKS_NONE && auth != TAF_PROP_NET_SOCKS_USER_PASSWD)
        return TAF_PA_FAULT;

    taf_prop_result_t rc = taf_prop_net_SetSocksAuthMethod(auth);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);

}

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_GetSocksAuthMethod
(
    taf_pa_net_AuthMethod_t* authMethodPtr
)
{
    TAF_PA_INFO("Actual taf_pa_net_GetSocksAuthMethod implementation");

    taf_prop_net_AuthMethod_t auth;
    taf_prop_net_GetSocksAuthMethod(&auth);
    if (authMethodPtr) *authMethodPtr = static_cast<taf_pa_net_AuthMethod_t>(auth);
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_SetSocksLanInterface
(
    const char* ifName
)
{
    TAF_PA_INFO("Actual taf_pa_net_SetSocksLanInterface implementation");

    taf_prop_result_t rc = taf_prop_net_SetSocksLanInterface(ifName);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_GetSocksLanInterface
(
    char* ifName,
    size_t ifNameSize
)
{
    TAF_PA_INFO("Actual taf_pa_net_GetSocksLanInterface implementation");

    taf_prop_result_t rc = taf_prop_net_GetSocksLanInterface(ifName, ifNameSize);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_AddSocksAssociation
(
    const char* userName,
    uint32_t profileId
)
{
    TAF_PA_INFO("Actual taf_pa_net_AddSocksAssociation implementation");

    taf_prop_result_t rc = taf_prop_net_AddSocksAssociation(userName, profileId);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_net_RemoveSocksAssociation
(
    const char* userName
)
{
    TAF_PA_INFO("Actual taf_pa_net_RemoveSocksAssociation implementation");

    taf_prop_result_t rc = taf_prop_net_RemoveSocksAssociation(userName);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

/*======================================================================

 FUNCTION        taf_Socks::EnableSocksCmdSync

======================================================================*/
taf_pa_result_t taf_pa_net_EnableSocksCmdSync()
{
    TAF_PA_INFO("Actual taf_pa_net_EnableSocksCmdSync implementation");

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    taf_pa_result_t result;
    std::chrono::seconds span(ENABLE_SOCKS_TIMEOUT);
    if (socksMgr == NULL) {
        TAF_PA_ERROR("socksMgr is null");
        return TAF_PA_NOT_FOUND;
    }

    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();

    auto enableSocksRespCb = [promisePtr](telux::common::ErrorCode error)
    {
        try
        {
          if (error != telux::common::ErrorCode::SUCCESS)
          {
              TAF_PA_ERROR( "Request failed with errorCode: %d " , static_cast<int>(error));
              promisePtr->set_value(TAF_PA_FAULT);
          }
          else
          {
               TAF_PA_DEBUG("Request processed successfully \n");
               promisePtr->set_value(TAF_PA_OK);
          }
      }
      catch (const std::future_error& e)
      {
          TAF_PA_ERROR("Future error in callback: %s", e.what());
      }
      catch (const std::exception& e)
      {
         TAF_PA_ERROR("Exception in callback: %s", e.what());
      }
      catch (...)
      {
         TAF_PA_ERROR("Unknown error in enable socks callback.");
      }
   };

    telux::common::Status status = socksMgr->enableSocks(true, enableSocksRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<taf_pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            TAF_PA_ERROR("Enable SOCKS timeout for %d seconds", ENABLE_SOCKS_TIMEOUT);
            result = TAF_PA_TIMEOUT;
        }
        else
        {
            result = futureResult.get();
        }

        return result;
    }
    else
    {
        TAF_PA_ERROR( "ERROR - Failed to enable SOCKS, Status:%d ", static_cast<int>(status));
        return TAF_PA_FAULT;
    }
    return TAF_PA_OK;
}

/*======================================================================

 FUNCTION        taf_Socks::DisableSocksCmdSync

======================================================================*/
taf_pa_result_t taf_pa_net_DisableSocksCmdSync
(

)
{
    TAF_PA_INFO("Actual taf_pa_net_DisableSocksCmdSync implementation");

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    taf_pa_result_t result;
    std::chrono::seconds span(ENABLE_SOCKS_TIMEOUT);
    if (socksMgr == NULL) {
        TAF_PA_ERROR("socksMgr is null");
        return TAF_PA_NOT_FOUND;
    }

    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();

    auto disbleSocksRespCb = [promisePtr](telux::common::ErrorCode error)
    {
        try
        {
          if (error != telux::common::ErrorCode::SUCCESS)
          {
              TAF_PA_ERROR( "Request failed with errorCode: %d " , static_cast<int>(error));
              promisePtr->set_value(TAF_PA_FAULT);
          }
          else
          {
               TAF_PA_DEBUG("Request processed successfully \n");
               promisePtr->set_value(TAF_PA_OK);
          }
      }
      catch (const std::future_error& e)
      {
          TAF_PA_ERROR("Future error in callback: %s", e.what());
      }
      catch (const std::exception& e)
      {
         TAF_PA_ERROR("Exception in callback: %s", e.what());
      }
      catch (...)
      {
         TAF_PA_ERROR("Unknown error in enable socks callback.");
      }
   };


    telux::common::Status status = socksMgr->enableSocks(false, disbleSocksRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<taf_pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            TAF_PA_ERROR("Disable SOCKS timeout for %d seconds", ENABLE_SOCKS_TIMEOUT);
            result = TAF_PA_TIMEOUT;
        }
        else
        {
            result = futureResult.get();
        }

        return result;
    }
    else
    {
        TAF_PA_ERROR( "ERROR - Failed to disable SOCKS, Status:%d ", static_cast<int>(status));
        return TAF_PA_FAULT;
    }

    return TAF_PA_OK;
}

/*======================================================================

 FUNCTION        EnableSocksCmdSync

 DESCRIPTION     ASynchronously enable socks.

 DEPENDENCIES    The initialization of socks.

 PARAMETERS      None

 RETURN VALUE    taf_pa_result_t
                     TAF_PA_OK:                      Success.
                     TAF_PA_FAULT                    Failure.

======================================================================*/
taf_pa_result_t taf_pa_net_EnableSocksCmdASync(taf_pa_socks_CallCb callback,void *contextPtr)
{

    TAF_PA_INFO("Actual taf_pa_net_EnableSocksCmdASync implementation");
    PA_UNUSED(contextPtr);

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    pSocksAdaptor.callCbEnableAsync = callback;

    if (socksMgr == NULL) {
        TAF_PA_ERROR("socksMgr is null");
        return TAF_PA_NOT_FOUND;
    }

    telux::common::Status status = socksMgr->enableSocks(true, taf_SocksAdaptor::tafSocksCallback::enableSocksAsyncResponse);

    if (status == telux::common::Status::SUCCESS)
    {
       return TAF_PA_OK;
    }
    return TAF_PA_FAULT;

}


/*======================================================================

 FUNCTION        DisableSocksCmdSync

 DESCRIPTION     ASynchronously disable socks.

 DEPENDENCIES    The initialization of socks.

 PARAMETERS      None

 RETURN VALUE    taf_pa_result_t
                     TAF_PA_OK:                      Success.
                     TAF_PA_FAULT                    Failure.

======================================================================*/
taf_pa_result_t taf_pa_net_DisableSocksCmdASync(taf_pa_socks_CallCb callback,void *contextPtr)
{

    TAF_PA_INFO("Actual taf_pa_net_DisableSocksCmdASync implementation");
    PA_UNUSED(contextPtr);

    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();
    auto socksMgr = pSocksAdaptor.getSocksManager();

    pSocksAdaptor.callCbDisableAsync = callback;

    if (socksMgr == NULL) {
        TAF_PA_ERROR("socksMgr is null");
        return TAF_PA_NOT_FOUND;
    }

    telux::common::Status status = socksMgr->enableSocks(false, taf_SocksAdaptor::tafSocksCallback::disableSocksAsyncResponse);

    if (status == telux::common::Status::SUCCESS)
    {
       return TAF_PA_OK;
    }
    return TAF_PA_FAULT;
}



taf_pa_result_t taf_pa_socks_Deinit()
{
    TAF_PA_INFO("Starting SOCKS platform adaptor deinitialization...");
    auto &pSocksAdaptor = taf_SocksAdaptor::getInstance();

    // Check if Init() was successfully called
    if (!pSocksAdaptor.isInitialized)
    {
        TAF_PA_WARN("SOCKS Deinit() called before Init() was successfully called");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Clearing SOCKS callbacks");
    pSocksAdaptor.callCbEnableAsync = nullptr;
    pSocksAdaptor.callCbDisableAsync = nullptr;

    if (pSocksAdaptor.socksListener && pSocksAdaptor.socksManager)
    {
        if (pSocksAdaptor.socksManager->deregisterListener(pSocksAdaptor.socksListener) ==
            telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("Socks service status listener deregistered.");
        }
        else
        {
            TAF_PA_ERROR("Failed to deregister socks service status listener.");
        }
    }
    pSocksAdaptor.socksListener.reset();

    TAF_PA_INFO("Resetting socksManager");
    pSocksAdaptor.socksManager.reset();
    pSocksAdaptor.isInitialized = false;
    TAF_PA_INFO("SOCKS platform adaptor deinitialization complete.");
    return TAF_PA_OK;
}
