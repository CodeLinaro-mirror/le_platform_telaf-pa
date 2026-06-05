/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#include "tafNatPa.hpp"

#include <telux/data/DataFactory.hpp>
#include <telux/data/net/NatManager.hpp>
#include <glib.h>
#include <atomic>

using namespace telux::data;
using namespace telux::common;

#define OPERATION_TIMEOUT 30

/* Implementation */

class taf_NatAdaptor
{
        public:
            static taf_NatAdaptor &getInstance();

            pa_result_t initialize();
            
            std::atomic<bool> isInitialized{false};

            std::shared_ptr<telux::data::net::INatManager> getNatManager()
            {
              return staticNatManager;
            }

            std::shared_ptr<telux::data::net::INatManager> staticNatManager = nullptr;
};

taf_NatAdaptor &taf_NatAdaptor::getInstance
(
    void
)
{
    static taf_NatAdaptor instance;
    return instance;
}

pa_result_t taf_NatAdaptor::initialize()
{
    PA_INFO("Initializing NAT adaptor");
    auto &dataFactory = telux::data::DataFactory::getInstance();

    if (staticNatManager == nullptr) {
        staticNatManager = dataFactory.getNatManager(
            telux::data::OperationType::DATA_LOCAL);
    }

    if (staticNatManager == nullptr) {
        PA_INFO("Nat manager initialize error...");
        return PA_FAULT;
    }

    telux::common::ServiceStatus subSystemStatus =
        staticNatManager->getServiceStatus();

    if (subSystemStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("NAT subsystem is not ready, waiting for it to be ready...");

        auto natMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        staticNatManager = dataFactory.getNatManager(
            telux::data::OperationType::DATA_LOCAL,
            [natMgrPromPtr](telux::common::ServiceStatus status) {
                PA_INFO("Getting status:%d from NAT manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        natMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        natMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    PA_ERROR("Exception setting NAT manager promise: %s", e.what());
                } catch (...) {
                    PA_ERROR("Unknown error setting NAT manager promise");
                }
            });

        if (staticNatManager == nullptr) {
            PA_ERROR("Failed to get NAT manager with init callback");
            return PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            natMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(OPERATION_TIMEOUT));

        if (std::future_status::timeout == waitStatus) {
            PA_ERROR("Timeout waiting for NAT subsystem");
            return PA_TIMEOUT;
        } else {
            subSystemStatus = initFuture.get();
        }
    }

    if (subSystemStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("NAT component is ready...");
        return PA_OK;
    } else {
        PA_CRIT("Unable to init NAT component, status=%d",
                static_cast<int>(subSystemStatus));
        staticNatManager = nullptr;
        return PA_FAULT;
    }
}

pa_result_t taf_pa_nat_Init()
{
    PA_INFO("Actual platform adatper implementation");
    
    auto &pNatAdaptor = taf_NatAdaptor::getInstance();

    pa_result_t result = pNatAdaptor.initialize();
    if (result == PA_OK)
    {
        PA_INFO("NAT platform adapter initialization is done");
        pNatAdaptor.isInitialized = true;
    }
    else
    {
        PA_CRIT("Failed to initialize NAT platform adapter, ret: %d", result);
        pNatAdaptor.isInitialized = false;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add a destination NAT entry
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_nat_AddDestNatEntry(
    uint32_t profileId,
    uint8_t slotId,
    const taf_pa_net_NatConfig_t *natConfig
)
{
    auto &natAdaptor = taf_NatAdaptor::getInstance();
    auto staticNatManager = natAdaptor.getNatManager();
    std::chrono::seconds span(OPERATION_TIMEOUT);
    pa_result_t result;

    if (staticNatManager == nullptr)
    {
        PA_ERROR("staticNatManager is null");
        return PA_FAULT;
    }
    
    if (natConfig == nullptr)
    {
        PA_ERROR("natConfig is null");
        return PA_BAD_PARAMETER;
    }

    // Convert PA NAT config to telux NAT config
    telux::data::net::NatConfig teluxNatConfig;
    teluxNatConfig.addr = natConfig->addr;
    teluxNatConfig.port = natConfig->port;
    teluxNatConfig.globalPort = natConfig->globalPort;
    teluxNatConfig.proto = natConfig->proto;

    PA_DEBUG("AddDestNatEntry: profileId=%u, slotId=%u, addr=%s, port=%u, globalPort=%u, proto=%u",
             profileId, slotId, natConfig->addr, natConfig->port, 
             natConfig->globalPort, natConfig->proto);

    // Create promise for synchronous operation
    auto natSyncPromise = std::make_shared<std::promise<pa_result_t>>();

    auto natCallback = [natSyncPromise](telux::common::ErrorCode error)
    {
        try
        {
          if (error != telux::common::ErrorCode::SUCCESS)
          {
              PA_ERROR( "Request failed with errorCode: %d " , static_cast<int>(error));
              natSyncPromise->set_value(PA_FAULT);
          }
          else
          {
               PA_DEBUG("Request processed successfully \n");
               natSyncPromise->set_value(PA_OK);
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
         PA_ERROR("Unknown error in NAT callback.");
      }
   };

    // Call telux SDK
    SlotId slot = static_cast<SlotId>(slotId);
    Status status = staticNatManager->addStaticNatEntry(profileId, teluxNatConfig,
                                                        natCallback, slot);

    if (status == Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = natSyncPromise->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Add vlan interface timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR("Failed to send add static entry request, Status: %d", static_cast<int>(status));
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove a destination NAT entry
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_nat_RemoveDestNatEntry(
    uint32_t profileId,
    uint8_t slotId,
    const taf_pa_net_NatConfig_t *natConfig
)
{
    auto &natAdaptor = taf_NatAdaptor::getInstance();
    auto staticNatManager = natAdaptor.getNatManager();
    std::chrono::seconds span(OPERATION_TIMEOUT);
    pa_result_t result;
    
    if (staticNatManager == nullptr)
    {
        PA_ERROR("staticNatManager is null");
        return PA_FAULT;
    }
    
    if (natConfig == nullptr)
    {
        PA_ERROR("natConfig is null");
        return PA_BAD_PARAMETER;
    }
    
    // Convert PA NAT config to telux NAT config
    telux::data::net::NatConfig teluxNatConfig;
    teluxNatConfig.addr = natConfig->addr;
    teluxNatConfig.port = natConfig->port;
    teluxNatConfig.globalPort = natConfig->globalPort;
    teluxNatConfig.proto = natConfig->proto;
    
    PA_DEBUG("RemoveDestNatEntry: profileId=%u, slotId=%u, addr=%s, port=%u, globalPort=%u, proto=%u",
             profileId, slotId, natConfig->addr, natConfig->port, 
             natConfig->globalPort, natConfig->proto);

    // Create promise for synchronous operation
    auto natSyncPromise = std::make_shared<std::promise<pa_result_t>>();

    auto natCallback = [natSyncPromise](telux::common::ErrorCode error)
    {
        try
        {
          if (error != telux::common::ErrorCode::SUCCESS)
          {
              PA_ERROR( "Request failed with errorCode: %d " , static_cast<int>(error));
              natSyncPromise->set_value(PA_FAULT);
          }
          else
          {
               PA_DEBUG("Request processed successfully \n");
               natSyncPromise->set_value(PA_OK);
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
         PA_ERROR("Unknown error in NAT callback.");
      }
   };
    
    // Call telux SDK
    SlotId slot = static_cast<SlotId>(slotId);
    Status status = staticNatManager->removeStaticNatEntry(profileId, teluxNatConfig, 
                                                           natCallback, slot);
    if (status == Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = natSyncPromise->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Add vlan interface timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR("Failed to send add static entry request, Status: %d", static_cast<int>(status));
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Query destination NAT entry list for a profile
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_nat_QueryDestNatEntryList(
    uint32_t profileId,
    uint8_t slotId,
    std::vector<taf_pa_net_NatConfig_t> &natEntryInfo
)
{
    auto &natAdaptor = taf_NatAdaptor::getInstance();
    auto staticNatManager = natAdaptor.getNatManager();
    
    if (staticNatManager == nullptr)
    {
        PA_ERROR("staticNatManager is null");
        return PA_FAULT;
    }
    
    std::chrono::seconds span(30);  // 30 second timeout
    
    std::promise<telux::common::ErrorCode> errorPromise;
    std::promise<const std::vector<telux::data::net::NatConfig>> dataPromise;
    
    auto queryCallback = [&errorPromise, &dataPromise](
        const std::vector<telux::data::net::NatConfig> &natConfigs,
        telux::common::ErrorCode error)
    {
        PA_DEBUG("QueryDestNatEntryList callback: error=%d, size=%zu", 
                 static_cast<int>(error), natConfigs.size());
        errorPromise.set_value(error);
        dataPromise.set_value(natConfigs);
    };
    
    SlotId slot = static_cast<SlotId>(slotId);
    telux::common::Status status = staticNatManager->requestStaticNatEntries(
        profileId, queryCallback, slot);
    
    if (status == telux::common::Status::SUCCESS)
    {
        std::future<telux::common::ErrorCode> futureError = errorPromise.get_future();
        std::future_status waitStatus = futureError.wait_for(span);
        
        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Query NAT entry list timeout for 30 seconds");
            return PA_TIMEOUT;
        }
        else
        {
            telux::common::ErrorCode error = futureError.get();
            PA_INFO("QueryDestNatEntryList response error code: %d", static_cast<int>(error));
            
            if (error == telux::common::ErrorCode::SUCCESS)
            {
                const std::vector<telux::data::net::NatConfig> teluxNatConfigs = 
                    dataPromise.get_future().get();
                PA_INFO("Size of NAT entry list: %zu", teluxNatConfigs.size());

                // Convert telux NAT configs to PA NAT configs
                for (const auto &teluxConfig : teluxNatConfigs)
                {
                    taf_pa_net_NatConfig_t paConfig;

                    gsize src_len = g_strlcpy(paConfig.addr,teluxConfig.addr.c_str(),
                                              sizeof(paConfig.addr));
                    if (src_len >= sizeof(paConfig.addr))
                    {
                       PA_ERROR("destId truncated: src_len=%zu, buf_size=%u, value='%s'",
                       static_cast<size_t>(src_len),
                       static_cast<unsigned>(sizeof(paConfig.addr)),
                       teluxConfig.addr.c_str());
                    }

                    paConfig.port = teluxConfig.port;
                    paConfig.globalPort = teluxConfig.globalPort;
                    paConfig.proto = teluxConfig.proto;
                    natEntryInfo.push_back(paConfig);
                }
                
                return PA_OK;
            }
            else
            {
                PA_ERROR("Failed to query NAT entry list, error: %d", static_cast<int>(error));
                return PA_FAULT;
            }
        }
    }
    else
    {
        PA_ERROR("Failed to request NAT entry list, status: %d", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_nat_Deinit()
{
    PA_INFO("Starting NAT platform adaptor deinitialization...");
    auto &pNatAdaptor = taf_NatAdaptor::getInstance();

    // Check if Init() was successfully called
    if (!pNatAdaptor.isInitialized)
    {
        PA_WARN("NAT Deinit() called before Init() was successfully called");
        return PA_FAULT;
    }

    PA_INFO("Resetting staticNatManager");
    pNatAdaptor.staticNatManager.reset();
    pNatAdaptor.isInitialized = false;
    PA_INFO("NAT platform adaptor deinitialization complete.");
    return PA_OK;
}
