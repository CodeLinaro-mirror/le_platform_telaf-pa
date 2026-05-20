/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#include "tafL2tpPa.hpp"

#include <telux/data/DataFactory.hpp>
#include <telux/data/net/L2tpManager.hpp>

#include <arpa/inet.h> // Required for inet_pton
#include <string.h> // Required for memset
#include <atomic>

#define L2TP_TIMEOUT 30

// Add a default MTU size if not already defined, as it's used in some calls
#ifndef DEFAULT_MTU_SIZE
#define DEFAULT_MTU_SIZE 1422
#endif

class taf_L2tpAdaptor
{
    public:
        static taf_L2tpAdaptor &getInstance();

        pa_result_t initialize();

        pa_result_t ConvertPaTunnelConfToTelux(const taf_pa_net_L2tpTunnel_t& paL2tpTunnelConfig,
                                         telux::data::net::L2tpTunnelConfig& teluxL2tpTunnelConfig);

        std::atomic<bool> isInitialized{false};


        // Distinct callbacks and contexts per async operation
        taf_pa_l2tp_CallCb cbEnable = nullptr;
        taf_pa_l2tp_CallCb cbDisable = nullptr;
        taf_pa_l2tp_CallCb cbStartTunnel = nullptr;
        taf_pa_l2tp_CallCb cbStopTunnel = nullptr;
        void *ctxEnable = nullptr;
        void *ctxDisable = nullptr;
        void *ctxStartTunnel = nullptr;
        void *ctxStopTunnel = nullptr;

        std::shared_ptr<telux::data::net::IL2tpManager> getL2tpManager()
        {
            return l2tpManager;
        }
        std::shared_ptr<telux::data::net::IL2tpManager> l2tpManager = nullptr;

        /*
        * @brief A callback class must be provided when invoke telsdk API.
        */
        class tafL2tpCallback
        {
            public:
            static void enableL2tpAsyncResponse(telux::common::ErrorCode error);
            static void disableL2tpAsyncResponse(telux::common::ErrorCode error);
            static void startTunnelAsyncResponse(telux::common::ErrorCode error);
            static void stopTunnelAsyncResponse(telux::common::ErrorCode error);
            static taf_pa_net_L2tpConfig_t l2tpConfig;
        };
};

taf_pa_net_L2tpConfig_t taf_L2tpAdaptor::tafL2tpCallback::l2tpConfig;

taf_L2tpAdaptor &taf_L2tpAdaptor::getInstance(void)
{
    static taf_L2tpAdaptor instance;
    return instance;
}

pa_result_t taf_pa_l2tp_Init()
{
    PA_DEBUG("Enter taf_pa_l2tp_Init in PA");
    PA_INFO("Default platform adatper implementation");

    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();

    pa_result_t result = pL2tpAdaptor.initialize();
    if (result == PA_OK)
    {
        PA_INFO("L2tp platform adapter initialization is done");
        pL2tpAdaptor.isInitialized = true;
    }
    else
    {
        PA_CRIT("Failed to initialize L2tp platform adapter, ret: %d", result);
        pL2tpAdaptor.isInitialized = false;
    }

    return result;
}

pa_result_t taf_L2tpAdaptor::initialize()
{
    PA_DEBUG("Enter initialize in PA");

    auto &dataFactory = telux::data::DataFactory::getInstance();

    if (l2tpManager == nullptr)
    {
        l2tpManager = dataFactory.getL2tpManager();
    }

    if (l2tpManager == nullptr)
    {
        PA_INFO("L2tp manager initialize error...");
        return PA_FAULT;
    }

    telux::common::ServiceStatus subSystemStatus =
        l2tpManager->getServiceStatus();

    if (subSystemStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("L2tp subsystem is not ready, waiting for it to be ready...");

        auto l2tpMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        l2tpManager = dataFactory.getL2tpManager(
            [l2tpMgrPromPtr](telux::common::ServiceStatus status) {
                PA_INFO("Getting status:%d from L2TP manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        l2tpMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        l2tpMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    PA_ERROR("Exception setting L2TP manager promise: %s", e.what());
                } catch (...) {
                    PA_ERROR("Unknown error setting L2TP manager promise");
                }
            });

        if (l2tpManager == nullptr) {
            PA_ERROR("Failed to get L2TP manager with init callback");
            return PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            l2tpMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(L2TP_TIMEOUT));

        if (std::future_status::timeout == waitStatus) {
            PA_ERROR("Timeout waiting for L2TP subsystem");
            return PA_FAULT;
        } else {
            subSystemStatus = initFuture.get();
        }
    }

    if (subSystemStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("L2tp manager service is available");
        return PA_OK;
    } else {
        PA_ERROR("L2tp Manager initialization failed, status=%d",
                 static_cast<int>(subSystemStatus));
        l2tpManager = nullptr;
        return PA_FAULT;
    }
}

/*======================================================================

 FUNCTION        taf_L2tpAdaptor::ConvertPaTunnelConfToTelux

 DESCRIPTION     Converts taf_pa_net_L2tpTunnel_t to telux::data::net::L2tpTunnelConfig.

 DEPENDENCIES    None.

 PARAMETERS      [IN] paL2tpTunnelConfig: Source PA tunnel config.
                 [OUT] teluxL2tpTunnelConfig: Destination Telux tunnel config.

 RETURN VALUE    pa_result_t: PA_OK on success, PA_FAULT otherwise.

======================================================================*/
pa_result_t taf_L2tpAdaptor::ConvertPaTunnelConfToTelux(const taf_pa_net_L2tpTunnel_t& paL2tpTunnelConfig,
                                                                   telux::data::net::L2tpTunnelConfig& teluxL2tpTunnelConfig)
{
    PA_DEBUG("Enter ConvertPaTunnelConfToTelux in PA");
    teluxL2tpTunnelConfig.prot = static_cast<telux::data::net::L2tpProtocol>(paL2tpTunnelConfig.prot);
    teluxL2tpTunnelConfig.locId = paL2tpTunnelConfig.locId;
    teluxL2tpTunnelConfig.peerId = paL2tpTunnelConfig.peerId;
    teluxL2tpTunnelConfig.localUdpPort = paL2tpTunnelConfig.localUdpPort;
    teluxL2tpTunnelConfig.peerUdpPort = paL2tpTunnelConfig.peerUdpPort;

    // Convert IP addresses (assuming peerIpv4Addr and peerIpv6Addr are char arrays)
    if (paL2tpTunnelConfig.ipType == TAF_PA_NET_L2TP_IPV4) {
        teluxL2tpTunnelConfig.peerIpv4Addr = paL2tpTunnelConfig.peerIpv4Addr;
        teluxL2tpTunnelConfig.ipType = telux::data::IpFamilyType::IPV4;
    } else if (paL2tpTunnelConfig.ipType == TAF_PA_NET_L2TP_IPV6) {
        teluxL2tpTunnelConfig.peerIpv6Addr = paL2tpTunnelConfig.peerIpv6Addr;
        teluxL2tpTunnelConfig.ipType = telux::data::IpFamilyType::IPV6;
    } else {
        PA_ERROR("Unsupported IP family type: %d", static_cast<int>(paL2tpTunnelConfig.ipType));
        return PA_FAULT;
    }

    teluxL2tpTunnelConfig.locIface = paL2tpTunnelConfig.locIface;

    // Convert session configurations
    teluxL2tpTunnelConfig.sessionConfig.clear();
    for (const auto& session : paL2tpTunnelConfig.sessionConfig) {
        telux::data::net::L2tpSessionConfig teluxSession;
        teluxSession.locId = session.locId;
        teluxSession.peerId = session.peerId;
        teluxL2tpTunnelConfig.sessionConfig.push_back(teluxSession);
    }
    return PA_OK;
}

/*======================================================================

 FUNCTION        tafL2tpCallback::enableL2tpAsyncResponse

 DESCRIPTION     Call back function for enabling L2TP (Telux SDK internal callback).

 DEPENDENCIES    The initialization of L2tp.

 PARAMETERS      [IN] telux::common::ErrorCode error: The error code from Telux SDK.

 RETURN VALUE    None.

======================================================================*/
void taf_L2tpAdaptor::tafL2tpCallback::enableL2tpAsyncResponse(telux::common::ErrorCode error)
{
    PA_DEBUG("Enter enableL2tpAsyncResponse in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    pa_result_t result = PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        PA_ERROR("Telux enableL2tp failed with error: %d", static_cast<int>(error));
        result = PA_FAULT;
    }

    if (pL2tpAdaptor.cbEnable)
        pL2tpAdaptor.cbEnable(result, pL2tpAdaptor.ctxEnable);

}

/*======================================================================

 FUNCTION        tafL2tpCallback::disableL2tpAsyncResponse

 DESCRIPTION     Call back function for disabling L2TP (Telux SDK internal callback).

 DEPENDENCIES    The initialization of L2TP.

 PARAMETERS      [IN] telux::common::ErrorCode error: The error code from Telux SDK.

 RETURN VALUE    None.

======================================================================*/
void taf_L2tpAdaptor::tafL2tpCallback::disableL2tpAsyncResponse(telux::common::ErrorCode error)
{
    PA_DEBUG("Enter disableL2tpAsyncResponse in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    pa_result_t result = PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        PA_ERROR("Telux disableL2tp failed with error: %d", static_cast<int>(error));
        result = PA_FAULT;
    }

    if (pL2tpAdaptor.cbDisable)
        pL2tpAdaptor.cbDisable(result, pL2tpAdaptor.ctxDisable);

}

/*======================================================================

 FUNCTION        tafL2tpCallback::startTunnelAsyncResponse

 DESCRIPTION     Call back function for starting tunnel (Telux SDK internal callback).

 DEPENDENCIES    The initialization of L2TP.

 PARAMETERS      [IN] telux::common::ErrorCode error: The error code from Telux SDK.

 RETURN VALUE    None.

======================================================================*/
void taf_L2tpAdaptor::tafL2tpCallback::startTunnelAsyncResponse(telux::common::ErrorCode error)
{
    PA_DEBUG("Enter startTunnelAsyncResponse in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    pa_result_t result = PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        PA_ERROR("Telux addTunnel failed with error: %d", static_cast<int>(error));
        result = PA_FAULT;
    }

    if (pL2tpAdaptor.cbStartTunnel)
        pL2tpAdaptor.cbStartTunnel(result, pL2tpAdaptor.ctxStartTunnel);

}

/*======================================================================

 FUNCTION        tafL2tpCallback::stopTunnelAsyncResponse

 DESCRIPTION     Call back function for stopping tunnel (Telux SDK internal callback).

 DEPENDENCIES    The initialization of L2TP.

 PARAMETERS      [IN] telux::common::ErrorCode error: The error code from Telux SDK.

 RETURN VALUE    None.

======================================================================*/
void taf_L2tpAdaptor::tafL2tpCallback::stopTunnelAsyncResponse(telux::common::ErrorCode error)
{
    PA_DEBUG("Enter stopTunnelAsyncResponse in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    pa_result_t result = PA_OK;

    if(error != telux::common::ErrorCode::SUCCESS &&
       error != telux::common::ErrorCode::NO_EFFECT)
    {
        PA_ERROR("Telux removeTunnel failed with error: %d", static_cast<int>(error));
        result = PA_FAULT;
    }

    if (pL2tpAdaptor.cbStopTunnel)
        pL2tpAdaptor.cbStopTunnel(result, pL2tpAdaptor.ctxStopTunnel);

}

//--------------------------------------------------------------------------------------------------
/**
 * Request L2TP Configuration
 * This function remains synchronous because its result is immediately needed for subsequent logic.
 * It uses a future-promise to block until the async Telux callback populates the config.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_RequestL2tpConfig
(
    taf_pa_net_L2tpConfig_t& L2tpConfig // OUT parameter
)
{
    PA_DEBUG("Enter taf_pa_net_RequestL2tpConfig in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    auto promisePtr = std::make_shared<std::promise<taf_pa_net_L2tpConfig_t>>();

    telux::common::Status status = l2tpManager->requestConfig(
        [promisePtr](const telux::data::net::L2tpSysConfig &l2tpSysConfig,
                     telux::common::ErrorCode error)
        {
            taf_pa_net_L2tpConfig_t retrievedConfig;

            if (error == telux::common::ErrorCode::SUCCESS) {
                retrievedConfig.enableL2tp   = l2tpSysConfig.enableMtu || l2tpSysConfig.enableTcpMss;
                retrievedConfig.enableMtu    = l2tpSysConfig.enableMtu;
                retrievedConfig.enableTcpMss = l2tpSysConfig.enableTcpMss;
                retrievedConfig.mtuSize      = l2tpSysConfig.mtuSize;

                retrievedConfig.configList.clear(); // Clear existing
                for (const auto& teluxTunnel : l2tpSysConfig.configList) {
                    taf_pa_net_L2tpTunnel_t paTunnel;
                    paTunnel.prot         = static_cast<taf_pa_net_L2tpEncapProtocol_t>(teluxTunnel.prot);
                    paTunnel.locId        = teluxTunnel.locId;
                    paTunnel.peerId       = teluxTunnel.peerId;
                    paTunnel.localUdpPort = teluxTunnel.localUdpPort;
                    paTunnel.peerUdpPort  = teluxTunnel.peerUdpPort;

                    paTunnel.ipType = static_cast<taf_pa_net_IpFamilyType_t>(teluxTunnel.ipType);
                    if (teluxTunnel.ipType == telux::data::IpFamilyType::IPV4) {
                        std::snprintf(paTunnel.peerIpv4Addr, TAF_PA_NET_IPV4_ADDR_MAX_LEN, "%s",
                                      teluxTunnel.peerIpv4Addr.c_str());
                        memset(paTunnel.peerIpv6Addr, 0, TAF_PA_NET_IPV6_ADDR_MAX_LEN);
                    } else if (teluxTunnel.ipType == telux::data::IpFamilyType::IPV6) {
                        std::snprintf(paTunnel.peerIpv6Addr, TAF_PA_NET_IPV6_ADDR_MAX_LEN, "%s",
                                      teluxTunnel.peerIpv6Addr.c_str());
                        memset(paTunnel.peerIpv4Addr, 0, TAF_PA_NET_IPV4_ADDR_MAX_LEN);
                    } else {
                        memset(paTunnel.peerIpv4Addr, 0, TAF_PA_NET_IPV4_ADDR_MAX_LEN);
                        memset(paTunnel.peerIpv6Addr, 0, TAF_PA_NET_IPV6_ADDR_MAX_LEN);
                    }

                    std::snprintf(paTunnel.locIface, TAF_PA_NET_INTERFACE_NAME_MAX_LEN, "%s",
                                  teluxTunnel.locIface.c_str());

                    paTunnel.sessionConfig.clear();
                    for (const auto& teluxSession : teluxTunnel.sessionConfig) {
                        taf_pa_net_L2tpSessionConfig_t paSession;
                        paSession.locId  = teluxSession.locId;
                        paSession.peerId = teluxSession.peerId;
                        paTunnel.sessionConfig.push_back(paSession);
                    }
                    retrievedConfig.configList.push_back(paTunnel);
                }
            } else {
                // ERROR
                PA_ERROR("ErrorCode %d", static_cast<int>(error));
                // NOT_SUPPORTED means that L2TP is disabled
                if (error == telux::common::ErrorCode::NOT_SUPPORTED) {
                    PA_ERROR("L2TP Unmanaged tunnel state is not enabled");
                }
                retrievedConfig.enableL2tp   = false;
                retrievedConfig.enableMtu    = false;
                retrievedConfig.enableTcpMss = false;
                retrievedConfig.mtuSize      = 0;
                retrievedConfig.configList.clear();
            }

            // Keep static snapshot as requested
            taf_L2tpAdaptor::tafL2tpCallback::l2tpConfig = retrievedConfig;

            // Fulfill the promise
            try {
                promisePtr->set_value(retrievedConfig);
            } catch (const std::exception &e) {
                PA_ERROR("Setting L2TP config promise failed: %s", e.what());
            } catch (...) {
                PA_ERROR("Unknown error setting L2TP config promise");
            }
        });

    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("ERROR - Failed to request config, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }

    auto fut = promisePtr->get_future();
    if (fut.wait_for(std::chrono::seconds(L2TP_TIMEOUT)) == std::future_status::timeout) {
        PA_ERROR("Wait for requestConfig response timed out after %d seconds", L2TP_TIMEOUT);
        return PA_TIMEOUT;
    }

    L2tpConfig = fut.get();
    return PA_OK;
}
//--------------------------------------------------------------------------------------------------
/**
 * Set L2TP Configuration Synchronously
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_SetL2tpConfigSync
(
    taf_pa_net_L2tpConfig_t& L2tpConfig  // IN
)
{
    PA_DEBUG("Enter taf_pa_net_SetL2tpConfigSync in PA");
    pa_result_t result = PA_OK;
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    std::chrono::seconds span(L2TP_TIMEOUT);

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto setConfigL2tpRespCb = [promisePtr](telux::common::ErrorCode error)
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
            PA_ERROR("Unknown error in L2TP callback.");
        }
    };

    if(L2tpConfig.mtuSize == 0)
        L2tpConfig.mtuSize=DEFAULT_MTU_SIZE;

    // Log all configuration values before applying
    PA_DEBUG("L2TP config: enableL2tp=%d, enableTcpMss=%d, enableMtu=%d, mtuSize=%u",
             L2tpConfig.enableL2tp,
             L2tpConfig.enableTcpMss,
             L2tpConfig.enableMtu,
             L2tpConfig.mtuSize);

    telux::common::Status status = l2tpManager->setConfig(L2tpConfig.enableL2tp, L2tpConfig.enableTcpMss,
                                    L2tpConfig.enableMtu, setConfigL2tpRespCb, L2tpConfig.mtuSize);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Set config timeout for %d seconds", L2TP_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to set config, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Add Tunnel Synchronously
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_AddTunnelSync(const taf_pa_net_L2tpTunnel_t& addTunnelConfig)
{
    PA_DEBUG("Enter taf_pa_net_AddTunnelSync in PA");
    pa_result_t result = PA_OK;
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    telux::data::net::L2tpTunnelConfig l2tpTunnelConfig;
    if(pL2tpAdaptor.ConvertPaTunnelConfToTelux(addTunnelConfig, l2tpTunnelConfig) != PA_OK) {
        return PA_FAULT;
    }

    std::chrono::seconds span(L2TP_TIMEOUT);

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto addTunnelSyncRespCb = [promisePtr](telux::common::ErrorCode error)
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
            PA_ERROR("Unknown error in L2TP callback.");
        }
    };


    telux::common::Status status = l2tpManager->addTunnel(l2tpTunnelConfig, addTunnelSyncRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Add tunnel timeout for %d seconds", L2TP_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to add tunnel, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }

    return PA_OK;

}

//--------------------------------------------------------------------------------------------------
/**
 * Remove Tunnel Synchronously
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_RemoveTunnelSync( const uint32_t tunnelId)
{
    PA_DEBUG("Enter taf_pa_net_RemoveTunnelSync in PA");
    pa_result_t result = PA_OK;
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    std::chrono::seconds span(L2TP_TIMEOUT);
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto removeTunnelSyncRespCb = [promisePtr](telux::common::ErrorCode error)
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
            PA_ERROR("Unknown error in L2TP callback.");
        }
    };

    telux::common::Status status = l2tpManager->removeTunnel(tunnelId,
                                           removeTunnelSyncRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Remove tunnel timeout for %d seconds", L2TP_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to Remove tunnel, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set Config Asynchronously
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_SetL2tpConfigAsync
(
    const taf_pa_net_L2tpConfig_t& config,
    taf_pa_l2tp_CallCb callback,
    void *contextPtr
)
{
    PA_DEBUG("Enter taf_pa_net_SetL2tpConfigAsync in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    // Store per-operation callback and context
    if (config.enableL2tp) {
        pL2tpAdaptor.cbEnable = callback;
        pL2tpAdaptor.ctxEnable = contextPtr;
    } else {
        pL2tpAdaptor.cbDisable = callback;
        pL2tpAdaptor.ctxDisable = contextPtr;
    }

    uint32_t mtuSizeToUse = config.mtuSize;
    if (mtuSizeToUse == 0) {
        mtuSizeToUse = DEFAULT_MTU_SIZE;
    }

    // Choose the corresponding Telux callback
    auto teluxCb = config.enableL2tp
                ? taf_L2tpAdaptor::tafL2tpCallback::enableL2tpAsyncResponse
                : taf_L2tpAdaptor::tafL2tpCallback::disableL2tpAsyncResponse;
    telux::common::Status status = l2tpManager->setConfig(
        config.enableL2tp,
        config.enableTcpMss,
        config.enableMtu,
        teluxCb,
        mtuSizeToUse);

    if (status == telux::common::Status::SUCCESS)
    {
       return PA_OK;
    }
    return PA_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove Tunnel Asynchronously
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_RemoveTunnelAsync(
    uint32_t tunnelId,
    taf_pa_l2tp_CallCb callback,
    void *contextPtr
)
{
    PA_DEBUG("Enter taf_pa_net_RemoveTunnelAsync in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    pL2tpAdaptor.cbStopTunnel = callback;
    pL2tpAdaptor.ctxStopTunnel = contextPtr;

    telux::common::Status status = l2tpManager->removeTunnel(tunnelId,
                                    taf_L2tpAdaptor::tafL2tpCallback::stopTunnelAsyncResponse);

    if (status == telux::common::Status::SUCCESS)
    {
       return PA_OK;
    }
    return PA_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add tunnel Asynchronously
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_net_AddTunnelAsync(
    const taf_pa_net_L2tpTunnel_t& addTunnelConfig,
    taf_pa_l2tp_CallCb callback,
    void *contextPtr)
{
    PA_DEBUG("Enter taf_pa_net_AddTunnelAsync in PA");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();
    auto l2tpManager = pL2tpAdaptor.getL2tpManager();

    if (l2tpManager == nullptr) {
        PA_ERROR("l2tpManager is null");
        return PA_NOT_FOUND;
    }

    telux::data::net::L2tpTunnelConfig l2tpTunnelConfig;

    if (pL2tpAdaptor.ConvertPaTunnelConfToTelux(addTunnelConfig, l2tpTunnelConfig) != PA_OK) {
        return PA_FAULT;
    }

    // Store per-op callback and context
    pL2tpAdaptor.cbStartTunnel = callback;
    pL2tpAdaptor.ctxStartTunnel = contextPtr;
    auto status = l2tpManager->addTunnel(l2tpTunnelConfig,
                taf_L2tpAdaptor::tafL2tpCallback::startTunnelAsyncResponse);
    return (status == telux::common::Status::SUCCESS) ? PA_OK : PA_FAULT;
}




pa_result_t taf_pa_l2tp_Deinit()
{
    PA_DEBUG("Starting L2TP platform adaptor deinitialization...");
    auto &pL2tpAdaptor = taf_L2tpAdaptor::getInstance();

    // Check if Init() was successfully called
    if (!pL2tpAdaptor.isInitialized)
    {
        PA_WARN("L2TP Deinit() called before Init() was successfully called");
        return PA_FAULT;
    }

    PA_DEBUG("Resetting l2tpManager");
    pL2tpAdaptor.l2tpManager.reset();
    pL2tpAdaptor.isInitialized = false;
    PA_DEBUG("L2TP platform adaptor deinitialization complete.");
    return PA_OK;
}
