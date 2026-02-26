/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */




#include "taf_pa_vlan.hpp"

#include <string>
#include <iostream>
#include <shared_mutex>
#include <glib.h>

#include <telux/data/DataFactory.hpp>
#include <telux/data/net/VlanManager.hpp>

#define OPERATION_DATA_SETTINGS 60
#define OPERATION_TIMEOUT 30

static bool bVlanListenerRegistered = {false};

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for hardware acceleration handler.
 */
//--------------------------------------------------------------------------------------------------
static taf_pa_vlan_HardwareAccelerationHandler_t HwAccelarationHandlerPtr = nullptr;

class taf_VlanListener : public  telux::data::net::IVlanListener
{
        public:
            taf_VlanListener(){};
            void onHwAccelerationChanged(const telux::data::ServiceState state) override;
};

//--------------------------------------------------------------------------------------------------
/**
 * Backhaul preference.
 */
//--------------------------------------------------------------------------------------------------

class taf_VlanAdaptor
{
        public:
            static taf_VlanAdaptor &getInstance();

            pa_result_t initialize();

            std::shared_ptr<telux::data::net::IVlanManager> getVlanManager()
            {
              return vlanManager;
            }

            std::shared_ptr<telux::data::IDataSettingsManager> getDataSettingsManager()
            {
              return dataSettingsManager;
            }

            static pa_result_t ConvertVlanInfo(const telux::data::VlanConfig &telVlanConfig,
                                              taf_pa_Vlan_t &vlanConfig);
            static pa_result_t ConvertVlanBindInfo(const telux::data::net::VlanBindConfig &telVlanConfig,
                                              taf_pa_VlanBindConfig_t &vlanbindConfig);

            std::shared_ptr<telux::data::net::IVlanListener>   vlanListener;
            std::shared_ptr<taf_VlanListener>   tafVlanListener;
            std::shared_mutex regVlanListenerMutex_;
            std::shared_mutex deregVlanListenerMutex_;

        private:
            std::shared_ptr<telux::data::net::IVlanManager> vlanManager = nullptr;
            std::shared_ptr<telux::data::IDataSettingsManager> dataSettingsManager = nullptr;
};

pa_result_t  taf_VlanAdaptor::ConvertVlanInfo(const telux::data::VlanConfig &telVlanConfig,
                                            taf_pa_Vlan_t &vlanConfig)
{

    vlanConfig.vlanId = telVlanConfig.vlanId;
    vlanConfig.isAccelerated = telVlanConfig.isAccelerated;
    vlanConfig.priority = telVlanConfig.priority;
    //vlanConfig.iface = (taf_pa_Vlan_t)telVlanConfig.iface;
    
    switch (telVlanConfig.iface)
    {
        case telux::data::InterfaceType::UNKNOWN:
            vlanConfig.iface = TAF_PA_VLAN_IFACE_UNKNOWN;
            break;
        case telux::data::InterfaceType::WLAN:
            vlanConfig.iface = TAF_PA_VLAN_IFACE_WLAN;
            break;
        case telux::data::InterfaceType::ETH:
            vlanConfig.iface = TAF_PA_VLAN_IFACE_ETH;
            break;
        case telux::data::InterfaceType::ECM:
            vlanConfig.iface = TAF_PA_VLAN_IFACE_ECM;
            break;
        case telux::data::InterfaceType::RNDIS:
            vlanConfig.iface = TAF_PA_VLAN_IFACE_RNDIS;
            break;
        case telux::data::InterfaceType::MHI:
            vlanConfig.iface = TAF_PA_VLAN_IFACE_MHI;
            break;
        default:
            PA_ERROR("Invalid iface type (%d).", static_cast<int>(telVlanConfig.iface));
            return PA_BAD_PARAMETER;
    }

    if(telVlanConfig.nwType == telux::data::NetworkType::LAN)
    {
        vlanConfig.nwType = TAF_PA_VLAN_NETWORK_LAN;
    }
    else if(telVlanConfig.nwType == telux::data::NetworkType::WAN)
    {
        vlanConfig.nwType = TAF_PA_VLAN_NETWORK_WAN;
    }
    else
    {
       vlanConfig.nwType = TAF_PA_VLAN_NETWORK_UNKNOWN;
    }

    vlanConfig.isBridgeEnabled = telVlanConfig.createBridge;

    return PA_OK;
}

pa_result_t taf_VlanAdaptor::ConvertVlanBindInfo(const telux::data::net::VlanBindConfig &telVlanConfig,
                                                 taf_pa_VlanBindConfig_t &vlanbindConfig)
{
    vlanbindConfig.vlanId = telVlanConfig.vlanId;
    // for WWAN backhauls only
    vlanbindConfig.slotId = telVlanConfig.bhInfo.slotId;
    vlanbindConfig.profileId = telVlanConfig.bhInfo.profileId;
    // for non-WWAN backhaul only
    vlanbindConfig.vlanIdBackhaul = telVlanConfig.bhInfo.vlanId;

    switch (telVlanConfig.bhInfo.backhaul)
    {
        case telux::data::BackhaulType::ETH:
            vlanbindConfig.backhaulType = TAF_PA_VLAN_BH_ETH;
            break;
        case telux::data::BackhaulType::USB:
            vlanbindConfig.backhaulType = TAF_PA_VLAN_BH_USB;
            break;
        case telux::data::BackhaulType::WLAN:
            vlanbindConfig.backhaulType = TAF_PA_VLAN_BH_WLAN;
            break;
        case telux::data::BackhaulType::WWAN:
            vlanbindConfig.backhaulType = TAF_PA_VLAN_BH_WWAN;
            break;
        case telux::data::BackhaulType::BLE:
            vlanbindConfig.backhaulType = TAF_PA_VLAN_BH_BLE;
            break;
        default:
            PA_ERROR("Invalid backhaul type (%d).", static_cast<int>(telVlanConfig.bhInfo.backhaul));
            return PA_BAD_PARAMETER;

    }

    return PA_OK;
}

void taf_VlanListener::onHwAccelerationChanged(const telux::data::ServiceState state)
{
    if (HwAccelarationHandlerPtr != nullptr)
    {
        taf_pa_vlan_hwacc_state_t stateHdlr = TAF_PA_VLAN_HW_ACC_STATE_INACTIVE;
        // If active, return TAF_NET_VLAN_HW_ACC_STATE_ACTIVE
        if (telux::data::ServiceState::ACTIVE   == state)
        {
            stateHdlr = TAF_PA_VLAN_HW_ACC_STATE_ACTIVE;
        } //  TAF_PA_VLAN_HW_ACC_STATE_INACTIVE in all other cases
        HwAccelarationHandlerPtr(stateHdlr);
    }
}

taf_VlanAdaptor &taf_VlanAdaptor::getInstance
(
    void
)
{
    static taf_VlanAdaptor instance;
    return instance;
}


/* Implementation */


pa_result_t taf_VlanAdaptor::initialize()
{
    PA_INFO("Initializing VLAN adaptor");

    auto &dataFactory = telux::data::DataFactory::getInstance();

    if (vlanManager == nullptr)
    {
        vlanManager = dataFactory.getVlanManager(telux::data::OperationType::DATA_LOCAL);
    }

    if (vlanManager == nullptr)
    {
        PA_INFO("Vlan manager initialize error...");
        return PA_FAULT;
    }

    telux::common::ServiceStatus subSystemStatus = vlanManager->getServiceStatus();

    if (subSystemStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_INFO("Vlan subsystem is not ready, waiting for it to be ready...");

        auto vlanMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        vlanManager = dataFactory.getVlanManager(
            telux::data::OperationType::DATA_LOCAL,
            [vlanMgrPromPtr](telux::common::ServiceStatus status)
            {
                PA_INFO("Getting status:%d from VLAN manager", static_cast<int>(status));
                try
                {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                    {
                        vlanMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    }
                    else
                    {
                        vlanMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                }
                catch (const std::exception &e)
                {
                    PA_ERROR("Exception setting VLAN manager promise: %s", e.what());
                }
                catch (...)
                {
                    PA_ERROR("Unknown error setting VLAN manager promise");
                }
            });

        if (vlanManager == nullptr)
        {
            PA_ERROR("Failed to get VLAN manager with init callback");
            return PA_FAULT;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            vlanMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(OPERATION_TIMEOUT));

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Timeout waiting for VLAN subsystem");
            return PA_TIMEOUT;
        }
        else
        {
            subSystemStatus = initFuture.get();
        }
    }

    if (subSystemStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_ERROR("Vlan Manager initialization failed, status=%d",
                 static_cast<int>(subSystemStatus));
        vlanManager = nullptr;
        return PA_FAULT;
    }

    PA_INFO("vlanManager component is ready...");

    if (dataSettingsManager == nullptr)
    {
        dataSettingsManager = dataFactory.getDataSettingsManager(
            telux::data::OperationType::DATA_LOCAL);
        if (!dataSettingsManager)
        {
            PA_CRIT("Failed to get Data Settings instance.");
        }
        else
        {
            telux::common::ServiceStatus dataSettingsManagerStatus =
                dataSettingsManager->getServiceStatus();
            if (dataSettingsManagerStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                auto promSettingPtr =
                    std::make_shared<std::promise<telux::common::ServiceStatus>>();

                dataSettingsManager = dataFactory.getDataSettingsManager(
                    telux::data::OperationType::DATA_LOCAL,
                    [promSettingPtr](telux::common::ServiceStatus svcStatus)
                    {
                        try
                        {
                            if (svcStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                            {
                                PA_INFO("iDataSettingsManager promSetting.set_value AVAILABLE...");
                                promSettingPtr->set_value(
                                    telux::common::ServiceStatus::SERVICE_AVAILABLE);
                            }
                            else
                            {
                                PA_INFO("iDataSettingsManager promSetting.set_value FAILED...");
                                promSettingPtr->set_value(
                                    telux::common::ServiceStatus::SERVICE_FAILED);
                            }
                        }
                        catch (const std::exception &e)
                        {
                            PA_ERROR("Exception setting DataSettingsManager promise: %s",
                                     e.what());
                        }
                        catch (...)
                        {
                            PA_ERROR("Unknown error setting DataSettingsManager promise");
                        }
                    });

                PA_INFO("Data setting subsystem wait to be ready...");
                std::future<telux::common::ServiceStatus> initFuture =
                    promSettingPtr->get_future();
                std::future_status waitStatus = initFuture.wait_for(
                    std::chrono::seconds(OPERATION_DATA_SETTINGS));
                if (std::future_status::timeout == waitStatus)
                {
                    PA_CRIT("Timeout waiting for Data setting susbsytem");
                }
                else
                {
                    dataSettingsManagerStatus = initFuture.get();
                }
            }

            if (dataSettingsManagerStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                PA_INFO("iDataSettingsManager is ready...");
            }
            else
            {
                PA_CRIT("Fail to init Data Setting Manager subsystem");
            }
        }
    }

    return PA_OK;
}

pa_result_t taf_pa_vlan_Init()
{
    PA_INFO("Actual platform adatper implementation");
    
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();

    pa_result_t result = pVlanAdaptor.initialize();
    if (result == PA_OK)
    {
        PA_INFO("VLAN platform adapter initialization is done");
    }
    else
    {
        PA_INFO("Failed to initialize VLAN platform adapter, ret: %d", result);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Register hardware acceleration notification handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_vlan_RegHwAccelarationUpdateHandler
(
    taf_pa_vlan_HardwareAccelerationHandler_t handlerFunc
)

{
    if ((handlerFunc != nullptr) && (HwAccelarationHandlerPtr == nullptr))
    {
        HwAccelarationHandlerPtr = handlerFunc;
        PA_INFO("hardware acceleration Handler registered.");
        return PA_OK;
    }

    PA_ERROR("hardware acceleration handler registered.");
    return PA_FAULT;
}

pa_result_t taf_pa_net_AddVlanInterface
(
     const taf_pa_Vlan_t vlanConfig,
     const taf_pa_vlan_iface_type_t iftype
)
{

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    telux::data::VlanConfig vconfig;
    pa_result_t result;
    std::chrono::seconds span(OPERATION_TIMEOUT);

    vconfig.iface = telux::data::InterfaceType(iftype);
    vconfig.vlanId = vlanConfig.vlanId;
    vconfig.isAccelerated = vlanConfig.isAccelerated;
    vconfig.priority = vlanConfig.priority;

    if(vlanConfig.nwType != TAF_PA_VLAN_NETWORK_UNKNOWN)
    {
        vconfig.nwType = (telux::data::NetworkType)vlanConfig.nwType;
    }

    // Set bridge enabled from vlanConfig
    vconfig.createBridge = vlanConfig.isBridgeEnabled;

    if (vlanConfig.nwType == TAF_PA_VLAN_NETWORK_WAN)
    { // bridge is not supported for WAN network type
        vconfig.createBridge = false;
        PA_WARN("bridge is not supported for WAN hence setting to false");
    }

    PA_DEBUG("NetworkType %d createBridge %d ", static_cast<int>(vconfig.nwType),
                                                static_cast<bool>(vconfig.createBridge));

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto addVlanRespCb = [promisePtr](bool isAccelerated, telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = vlanManager->createVlan(vconfig, addVlanRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
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
        PA_ERROR( "ERROR - Failed to add vlan interface, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_RemoveVlanInterface
(
     const taf_pa_Vlan_t vlanConfig,
     const taf_pa_vlan_iface_type_t iftype
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    pa_result_t result;
    std::chrono::seconds span(OPERATION_TIMEOUT);

    telux::data::InterfaceType iface = telux::data::InterfaceType(iftype);
    uint32_t vlanId = vlanConfig.vlanId;

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto removeVlanIfRespCb = [promisePtr](telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = vlanManager->removeVlan(vlanId, iface,removeVlanIfRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Remove vlan interface timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to remove vlan interface, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_BindWithProfile
(
     const taf_pa_Vlan_t vlan,
     const taf_pa_VlanBindConfig_t vlanBindConfig
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    pa_result_t result;
    std::chrono::seconds span(OPERATION_TIMEOUT);
    uint8_t slotId;
    uint32_t profileId;
    uint16_t vlanId=0;

    vlanId = vlan.vlanId;
    slotId = vlanBindConfig.slotId;
    profileId = vlanBindConfig.profileId;

    SlotId slot = (SlotId)slotId;

    PA_INFO("bindwithProfile: vlanid %d slotId %d profileId %d", vlanId,slotId,profileId);

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto bindVlanWithProfileRespCb = [promisePtr](telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = vlanManager->bindWithProfile(
                                              profileId, vlanId, bindVlanWithProfileRespCb,slot);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Bind vlan timeout for %d seconds", OPERATION_TIMEOUT);
            result = PA_TIMEOUT;
        }
        else
        {
            result = futureResult.get();
            PA_INFO("bindwithProfile: result %d ", result);
        }
        return result;
    }
    else
    {
        PA_ERROR( "ERROR - Failed to bind vlan , Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}


pa_result_t taf_pa_net_UnbindWithProfile
(
     const taf_pa_Vlan_t vlan,
     const taf_pa_VlanBindConfig_t vlanBindConfig
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

        if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    pa_result_t result;
    std::chrono::seconds span(OPERATION_TIMEOUT);
    uint8_t slotId;
    uint32_t profileId;
    uint16_t vlanId=0;

    vlanId = vlan.vlanId;
    slotId = vlanBindConfig.slotId;
    profileId = vlanBindConfig.profileId;

    SlotId slot = (SlotId)slotId;

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto unbindVlanWithProfileRespCb = [promisePtr](telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = vlanManager->unbindFromProfile(
                                              profileId, vlanId, unbindVlanWithProfileRespCb,slot);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Unbind vlan timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to Unbind vlan , Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_BindWithBackhaul
(
     const taf_pa_Vlan_t vlanConfig,
     const taf_pa_VlanBindConfig_t vlanBindConfig
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    telux::data::net::VlanBindConfig vlanBind = {};

    vlanBind.vlanId = vlanConfig.vlanId;

    std::chrono::seconds span(OPERATION_TIMEOUT);
    pa_result_t result;

    switch (vlanBindConfig.backhaulType)
    {
        case TAF_PA_VLAN_BH_ETH:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::ETH;
            break;
        case TAF_PA_VLAN_BH_USB:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::USB;
            break;
        case TAF_PA_VLAN_BH_WLAN:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::WLAN;
            break;
        case TAF_PA_VLAN_BH_WWAN:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::WWAN;
            break;
        case TAF_PA_VLAN_BH_BLE:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::BLE;
            break;
        default:
            PA_ERROR("Invalid backhaul type (%d).", vlanBindConfig.backhaulType);
            return PA_BAD_PARAMETER;
    }
    
    if(vlanBindConfig.backhaulType == TAF_PA_VLAN_BH_WWAN)
    {
        vlanBind.bhInfo.slotId = (SlotId)vlanBindConfig.slotId;
        vlanBind.bhInfo.profileId = vlanBindConfig.profileId;
    }
    else  // for ETH and rest where SIM does not exist.
    {
        vlanBind.bhInfo.vlanId = vlanBindConfig.vlanIdBackhaul; // BH is vlan
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto bindVlanWithBHRespCb = [promisePtr](telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = vlanManager->bindToBackhaul(vlanBind, bindVlanWithBHRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Bind vlan timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to bind vlan , Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_UnbindWithBackhaul
(
     const taf_pa_Vlan_t vlanConfig,
     const taf_pa_VlanBindConfig_t vlanBindConfig
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    telux::data::net::VlanBindConfig vlanBind = {};

    vlanBind.vlanId = vlanConfig.vlanId;

    std::chrono::seconds span(OPERATION_TIMEOUT);
    pa_result_t result;

    switch (vlanBindConfig.backhaulType)
    {
        case TAF_PA_VLAN_BH_ETH:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::ETH;
            break;
        case TAF_PA_VLAN_BH_USB:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::USB;
            break;
        case TAF_PA_VLAN_BH_WLAN:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::WLAN;
            break;
        case TAF_PA_VLAN_BH_WWAN:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::WWAN;
            break;
        case TAF_PA_VLAN_BH_BLE:
            vlanBind.bhInfo.backhaul = telux::data::BackhaulType::BLE;
            break;
        default:
            PA_ERROR("Invalid backhaul type (%d).", vlanBindConfig.backhaulType);
            return PA_BAD_PARAMETER;
    }
    
    if(vlanBindConfig.backhaulType == TAF_PA_VLAN_BH_WWAN)
    {
        vlanBind.bhInfo.slotId = (SlotId)vlanBindConfig.slotId;
        vlanBind.bhInfo.profileId = vlanBindConfig.profileId;
    }
    else  // for ETH and rest where SIM does not exist.
    {
        vlanBind.bhInfo.vlanId = vlanBindConfig.vlanIdBackhaul; // BH is vlan
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    auto unbindVlanWithBHRespCb = [promisePtr](telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = vlanManager->unbindFromBackhaul(vlanBind, unbindVlanWithBHRespCb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("unBind vlan timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to bind vlan , Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_QueryVlanInfo
(
    std::vector<taf_pa_Vlan_t> &vlanEntryInfo
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    std::chrono::seconds span(OPERATION_TIMEOUT);

    std::promise<telux::common::ErrorCode> p;
    std::promise<const std::vector<telux::data::VlanConfig>> q;

    telux::data::net::QueryVlanResponseCb cb =
            [&p, &q](const std::vector<telux::data::VlanConfig> configs,
                     telux::common::ErrorCode error) {
                p.set_value(error);
                q.set_value(configs);
            };

    telux::common::Status status = vlanManager->queryVlanInfo(cb);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<telux::common::ErrorCode> futureResult = p.get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("Query vlan timeout for %d seconds", OPERATION_TIMEOUT);
            return PA_TIMEOUT;
        }
        else
        {
            telux::common::ErrorCode error = futureResult.get();
            PA_INFO("queryVlanInfo response error code: %d", (int) error);
            if (error == telux::common::ErrorCode::SUCCESS)
            {
                const std::vector<telux::data::VlanConfig> vlanConfigs = q.get_future().get();
                PA_INFO("Size of vector VlanBindConfig: %d", (int) vlanConfigs.size());
                for (const auto &vlanConfig : vlanConfigs)
                {
                  taf_pa_Vlan_t vlanInfo;
                  taf_VlanAdaptor::ConvertVlanInfo(vlanConfig, vlanInfo);
                  vlanEntryInfo.push_back(vlanInfo);
                }
            }
           else
            {
                PA_ERROR( "ERROR - Failed to query vlan info , Status:%d ", static_cast<int>(status));
                return PA_FAULT;
            }
        }
    }
    else
    {
        PA_ERROR( "ERROR - Failed to bind vlan , Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t taf_pa_net_QueryVlanMappingList
(
     const uint8_t slotId,
     std::list<std::pair<int, int>> &vlanMapping
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    std::chrono::seconds span(OPERATION_TIMEOUT);

    std::promise<telux::common::ErrorCode> p;
    std::promise<const std::list<std::pair<int, int>>> q;
    std::vector<taf_pa_Vlan_t> vlanInfos;
    SlotId slot = SlotId(slotId);

    telux::data::net::VlanMappingResponseCb queryMapcb =
            [&p, &q](const std::list<std::pair<int, int>> &mapping,telux::common::ErrorCode error) 
            {
                PA_INFO("QueryVlanToBackhaulMappingList response error code: %d",
                         static_cast<int>(error));
                p.set_value(error);
                q.set_value(mapping);
            };

    telux::common::Status status = vlanManager->queryVlanMappingList(queryMapcb,slot);

    if (status == telux::common::Status::SUCCESS)
    {
        std::future<telux::common::ErrorCode> futureResult = p.get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("query vlan mapping timeout for %d seconds", OPERATION_TIMEOUT);
            return PA_TIMEOUT;
        }
        else
        {
            telux::common::ErrorCode error = futureResult.get();
            PA_INFO("QueryVlanToBackhaulMappingList response error code: %d", static_cast<int>(error));
            if (error == telux::common::ErrorCode::SUCCESS)
            {
                vlanMapping = q.get_future().get();
                return PA_OK;
            }
            else
            {
                return PA_OK;  //telsdk sends error for no bind list.
            }
        }
    }
    else
    {
        PA_ERROR( "ERROR - Failed to query vlan mapping , Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t taf_pa_net_QueryVlanToBackhaulMappingList
(
    const uint8_t slotID,                          // IN
    const taf_pa_vlan_backhaul_type_t backhaulType,  // IN
    std::vector<taf_pa_VlanBindConfig_t>& vlanEntryInfo  // OUT
)
{
    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto vlanManager = pVlanAdaptor.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

    std::promise<telux::common::ErrorCode> p;
    std::promise<const std::vector<telux::data::net::VlanBindConfig>> q;
    SlotId slot = (SlotId)slotID;

    telux::data::net::VlanBindingsResponseCb cb =
            [&p, &q](const std::vector<telux::data::net::VlanBindConfig> bindings,
                     telux::common::ErrorCode error) {
                p.set_value(error);
                q.set_value(bindings);
            };

    telux::data::BackhaulType telbackhaulType;
    switch (backhaulType)
    {
        case TAF_PA_VLAN_BH_ETH:
            telbackhaulType = telux::data::BackhaulType::ETH;
            break;
        case TAF_PA_VLAN_BH_USB:
            telbackhaulType = telux::data::BackhaulType::USB;
            break;
        case TAF_PA_VLAN_BH_WLAN:
            telbackhaulType = telux::data::BackhaulType::WLAN;
            break;
        case TAF_PA_VLAN_BH_WWAN:
            telbackhaulType = telux::data::BackhaulType::WWAN;
            break;
        case TAF_PA_VLAN_BH_BLE:
            telbackhaulType = telux::data::BackhaulType::BLE;
            break;
        default:
            PA_ERROR("Invalid backhaul type (%d).", backhaulType);
            return PA_BAD_PARAMETER;
    }

    telux::common::Status status = vlanManager->queryVlanToBackhaulBindings(
                                                telbackhaulType, cb, slot);

    if (status == telux::common::Status::SUCCESS)
    {
        PA_INFO("QueryVlanToBackhaulMappingList request status: %d", (int) status);
        telux::common::ErrorCode error = p.get_future().get();
        PA_INFO("QueryVlanToBackhaulMappingList response error code: %d", (int) error);
        if (error == telux::common::ErrorCode::SUCCESS)
        {
            const std::vector<telux::data::net::VlanBindConfig> vlanbindings = q.get_future().get();
            PA_INFO("Size of vector VlanBindConfig: %d", (int) vlanbindings.size());
            for (auto binding:vlanbindings)
            {
                taf_pa_VlanBindConfig_t vlanInfo;
                taf_VlanAdaptor::ConvertVlanBindInfo(binding, vlanInfo);
                vlanEntryInfo.push_back(vlanInfo);
            }
        }
        return PA_OK;
    }
    else
    {
        PA_ERROR("Request vlan backahul binding list failed, status: %d",int(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_RegVlanListener()
{

    PA_INFO("Registering listeners.");
    auto &tafVlan = taf_VlanAdaptor::getInstance();
    auto vlanManager = tafVlan.getVlanManager();

    if (!vlanManager)
    {
        PA_ERROR("VLAN Manager not available");
        return PA_FAULT;
    }

        std::shared_lock<std::shared_mutex> lock(tafVlan.regVlanListenerMutex_);

        // Register vlan listener for each slot it
        if (bVlanListenerRegistered)
        {
            PA_INFO("Vlan listener already registered.");
        }
        else
        {
            if (vlanManager)
            {
                tafVlan.tafVlanListener = std::make_shared<taf_VlanListener>();
                tafVlan.vlanListener = tafVlan.tafVlanListener;
                if (vlanManager-> registerListener(tafVlan.vlanListener) ==
                                                                 telux::common::Status::SUCCESS)
                {
                    PA_INFO("Vlan listener registered.");
                    bVlanListenerRegistered = true;
                }
                else
                {
                    PA_ERROR("Fail to register vlan listener.");
                }
            }
        }

    return PA_OK;
}

pa_result_t taf_pa_net_DeregVlanListener()
{
    PA_INFO("Deregistering listeners.");
    auto &tafVlan = taf_VlanAdaptor::getInstance();
    auto vlanManager = tafVlan.getVlanManager();

    std::shared_lock<std::shared_mutex> lock(tafVlan.deregVlanListenerMutex_);

    // Deregister vlan listener for each slot it
    if (!bVlanListenerRegistered)
    {
        PA_INFO("Vlan listeners already deregistered.");
    }
    else
        {
            if (vlanManager)
            {
                if (vlanManager-> deregisterListener(tafVlan.vlanListener) ==
                                                                  telux::common::Status::SUCCESS)
                {
                    PA_INFO("Vlan listener registered.");
                    bVlanListenerRegistered = false;
                }
                else
                {
                    PA_ERROR("Fail to register serving system listener.");
                }
            }
        }
    return PA_OK;
}

//Data Settings API

pa_result_t taf_pa_net_GetBackhaulPreference
(
    std::vector<taf_pa_vlan_backhaul_type_t> &backhaulPref        // OUT
)
{
    PA_INFO("Actual platform adatper implementation");

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();
    std::chrono::seconds span(OPERATION_TIMEOUT);
    pa_result_t result;

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto promisePtrPref = std::make_shared<std::promise<std::vector<telux::data::BackhaulType>>>();
    auto getPrefRespCb = [promisePtr,promisePtrPref](const std::vector<telux::data::BackhaulType> backhaulPref,
                                        telux::common::ErrorCode error)
    {
       try
       {
         if (error != telux::common::ErrorCode::SUCCESS)
         {
            PA_ERROR("Error(%d)", (int)error);
            promisePtr->set_value(PA_FAULT);
          }
         else
         {
            promisePtrPref->set_value(backhaulPref);
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = dataSettingsManager->requestBackhaulPreference(getPrefRespCb);
    
    if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future<std::vector<telux::data::BackhaulType>> futureResultPref =
                                                promisePtrPref->get_future();

        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("set backhaul pref interface timeout for %d seconds", OPERATION_TIMEOUT);
            result = PA_TIMEOUT;
        }
        else
        {
            result = futureResult.get();
            std::vector<telux::data::BackhaulType> backhaulPrefListTel = futureResultPref.get();
            std::vector<taf_pa_vlan_backhaul_type_t> backhaulPrefList;
            for (auto type : backhaulPrefListTel)
            {
              switch (type)
              {
                 case telux::data::BackhaulType::ETH:
                   backhaulPrefList.emplace_back(TAF_PA_VLAN_BH_ETH);
                   break;
                 case telux::data::BackhaulType::USB:
                   backhaulPrefList.emplace_back(TAF_PA_VLAN_BH_USB);
                   break;
                 case telux::data::BackhaulType::WLAN:
                   backhaulPrefList.emplace_back(TAF_PA_VLAN_BH_WLAN);
                   break;
                 case telux::data::BackhaulType::WWAN:
                   backhaulPrefList.emplace_back(TAF_PA_VLAN_BH_WWAN);
                   break;
                 case telux::data::BackhaulType::BLE:
                   backhaulPrefList.emplace_back(TAF_PA_VLAN_BH_BLE);
                   break;
                 default:
               PA_DEBUG("Invalid backhaul preference.");
              }
            }
            // copy backhaulPrefList
            backhaulPref.assign(backhaulPrefList.begin(),backhaulPrefList.end());
        }
        return result;
    }
    else
    {
        PA_ERROR( "ERROR - Failed to get backhaul pref interface, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_SetBackhaulPreference
(
    std::vector<taf_pa_vlan_backhaul_type_t> backhaulPrefIN        // IN
)
{
    PA_INFO("Actual platform adatper implementation");
    
    pa_result_t result;

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();
    
    std::vector<telux::data::BackhaulType> backhaulPref;

    //TAF_ERROR_IF_RET_VAL(backhaulPrefIN.size() == 0, PA_BAD_PARAMETER, "pref list is null");
    if(backhaulPrefIN.size() == 0)
    {
        PA_ERROR("pref list is null");
        return PA_BAD_PARAMETER;
    }

    //assign to telux's vector
    PA_INFO("Size of vector backhaulPrefIN: %d", (int) backhaulPrefIN.size());

    for (const auto &pref : backhaulPrefIN)
        {
            switch (pref)
            {
               case TAF_PA_VLAN_BH_ETH:
                   backhaulPref.emplace_back(telux::data::BackhaulType::ETH);
                   break;
               case TAF_PA_VLAN_BH_USB:
                   backhaulPref.emplace_back(telux::data::BackhaulType::USB);
                   break;
               case TAF_PA_VLAN_BH_WLAN:
                   backhaulPref.emplace_back(telux::data::BackhaulType::WLAN);
                   break;
               case TAF_PA_VLAN_BH_WWAN:
                   backhaulPref.emplace_back(telux::data::BackhaulType::WWAN);
                   break;
               case TAF_PA_VLAN_BH_BLE:
                   backhaulPref.emplace_back(telux::data::BackhaulType::BLE);
                   break;
               default:
               PA_DEBUG("Invalid backhaul preference.");
            }
        }

    std::chrono::seconds span(OPERATION_TIMEOUT);

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto getPrefRespCb = [promisePtr](telux::common::ErrorCode error)
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
         PA_ERROR("Unknown error in VLAN callback.");
      }
   };

    telux::common::Status status = dataSettingsManager->setBackhaulPreference(backhaulPref,
                                             getPrefRespCb);

   if (status == telux::common::Status::SUCCESS)
    {
        std::future<pa_result_t> futureResult = promisePtr->get_future();
        std::future_status waitStatus = futureResult.wait_for(span);

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("set backhaul pref interface timeout for %d seconds", OPERATION_TIMEOUT);
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
        PA_ERROR( "ERROR - Failed to add vlan interface, Status:%d ", static_cast<int>(status));
        return PA_FAULT;
    }
}

pa_result_t taf_pa_net_GetIPPassThroughNatConfig
(
    bool &isEnabledPtr                             // OUT
)
{
    PA_INFO("Actual platform adatper implementation");

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();
    telux::common::ErrorCode error = dataSettingsManager->getIpPassThroughNatConfig(isEnabledPtr);
    if (error != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("ERROR - Failed to get ippt NAT config , error:%d ",static_cast<int>(error));
        return PA_FAULT;
    }
    else
    {
        PA_INFO("get ippt NAT config is success...");
    }
    PA_DEBUG("GetIPPassThroughNatConfig %d", static_cast<int>(isEnabledPtr));
    return PA_OK;
}

pa_result_t taf_pa_net_SetIPPassThroughNatConfig
(
    bool isEnabled                             // IN
)
{
    PA_INFO("Actual platform adatper implementation");

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();

    telux::common::ErrorCode error = dataSettingsManager->setIpPassThroughNatConfig(isEnabled);
    if (error != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("ERROR - Failed to get get ippt NAT config , error:%d ",static_cast<int>(error));
        return PA_FAULT;
    }
    else
    {
        PA_INFO("set ippt NAT config is success...");
    }
    PA_DEBUG("SetIPPassThroughNatConfig %d", static_cast<int>(isEnabled));
    return PA_OK;
}

pa_result_t taf_pa_net_SetIPPassThroughConfig
(
    const taf_pa_IpptConfigIn_t *ipptConfigIn,    // IN
    const taf_pa_IpptConfigOut_t *ipptConfigOut   // IN
)
{
    PA_INFO("Actual platform adapter implementation");

    //TAF_ERROR_IF_RET_VAL(ipptConfigIn == NULL, PA_BAD_PARAMETER, "ipptConfigIn is null");
    if(ipptConfigIn == NULL)
    {
        PA_ERROR("ipptConfigIn is null");
        return PA_BAD_PARAMETER;
    }
    
    //TAF_ERROR_IF_RET_VAL(ipptConfigOut == NULL, PA_BAD_PARAMETER, "ipptConfigOut is null");
    if(ipptConfigOut == NULL)
    {
        PA_ERROR("ipptConfigOut is null");
        return PA_BAD_PARAMETER;
    }

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();

    telux::data::IpptParams ipptParams;
    telux::data::IpptConfig ipptConfigTel;

    ipptParams.profileId = ipptConfigIn->profileId;
    ipptParams.slotId = (SlotId)ipptConfigIn->slotId;
    ipptParams.vlanId = ipptConfigIn->vlanId;

    PA_DEBUG("SetIPPassThroughConfig vlanid(%d)", (int)ipptConfigIn->vlanId);
    PA_DEBUG("SetIPPassThroughConfig slotId(%d)", (int)ipptConfigIn->slotId);
    PA_DEBUG("SetIPPassThroughConfig profileId(%d)", (int)ipptConfigIn->profileId);

    // Convert operation type
    switch (ipptConfigOut->operation)
    {
        case TAF_PA_VLAN_IPPT_DISABLE:
            ipptConfigTel.ipptOpr = telux::data::Operation::DISABLE;
            break;
        case TAF_PA_VLAN_IPPT_ENABLE:
            ipptConfigTel.ipptOpr = telux::data::Operation::ENABLE;
            break;
        default:
            ipptConfigTel.ipptOpr = telux::data::Operation::UNKNOWN;
            break;
    }

    PA_DEBUG("SetIPPassThroughConfig operation(%d)", (int)ipptConfigOut->operation);

    if (ipptConfigOut->operation == TAF_PA_VLAN_IPPT_ENABLE)
    {
        // Convert interface type
        switch (ipptConfigOut->ifType)
        {
            case TAF_PA_VLAN_IFACE_WLAN:
                ipptConfigTel.devConfig.nwInterface = telux::data::InterfaceType::WLAN;
                break;
            case TAF_PA_VLAN_IFACE_ETH:
                ipptConfigTel.devConfig.nwInterface = telux::data::InterfaceType::ETH;
                break;
            case TAF_PA_VLAN_IFACE_ECM:
                ipptConfigTel.devConfig.nwInterface = telux::data::InterfaceType::ECM;
                break;
            case TAF_PA_VLAN_IFACE_RNDIS:
                ipptConfigTel.devConfig.nwInterface = telux::data::InterfaceType::RNDIS;
                break;
            case TAF_PA_VLAN_IFACE_MHI:
                ipptConfigTel.devConfig.nwInterface = telux::data::InterfaceType::MHI;
                break;
            default:
                ipptConfigTel.devConfig.nwInterface = telux::data::InterfaceType::UNKNOWN;
                break;
        }

        std::string mac(ipptConfigOut->macAddr);
        ipptConfigTel.devConfig.macAddr = mac;

        PA_DEBUG("SetIPPassThrough iftype(%d)", (int)ipptConfigOut->ifType);
        PA_DEBUG("SetIPPassThrough Mac in %s", ipptConfigOut->macAddr);
        PA_DEBUG("SetIPPassThrough Mac out %s", ipptConfigTel.devConfig.macAddr.data());
    }
    else
    {
        PA_ERROR("IP Pass through disabled or unknown; not filling device config");
    }

    telux::common::ErrorCode error = dataSettingsManager->setIpPassThroughConfig(ipptParams,
                                                                                 ipptConfigTel);
    if (error != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("ERROR - Failed to set ip pass through, error:%d ", static_cast<int>(error));
        return PA_FAULT;
    }
    else
    {
        PA_INFO("set ip pass through is success...");
    }
    return PA_OK;
}

pa_result_t taf_pa_net_GetIPPassThroughConfig
(
    const taf_pa_IpptConfigIn_t *ipptConfigIn,    // IN
    taf_pa_IpptConfigOut_t *ipptConfigOut         // OUT
)
{
    PA_INFO("Actual platform adapter implementation");

    //TAF_ERROR_IF_RET_VAL(ipptConfigIn == NULL, PA_BAD_PARAMETER, "ipptConfigIn is null");
    if(ipptConfigIn == NULL)
    {
        PA_ERROR("ipptConfigIn is null");
        return PA_BAD_PARAMETER;
    }

    //TAF_ERROR_IF_RET_VAL(ipptConfigOut == NULL, PA_BAD_PARAMETER, "ipptConfigOut is null");
    if(ipptConfigOut == NULL)
    {
        PA_ERROR("ipptConfigOut is null");
        return PA_BAD_PARAMETER;
    }

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();

    telux::data::IpptConfig ipptConfigTel;
    telux::data::IpptParams ipptParams;

    ipptParams.profileId = ipptConfigIn->profileId;
    ipptParams.vlanId = ipptConfigIn->vlanId;
    ipptParams.slotId = static_cast<SlotId>(ipptConfigIn->slotId);

    PA_DEBUG("GetIPPassThroughConfig vlanid(%d)", (int)ipptConfigIn->vlanId);
    PA_DEBUG("GetIPPassThroughConfig slotId(%d)", (int)ipptConfigIn->slotId);
    PA_DEBUG("GetIPPassThroughConfig profileId(%d)", (int)ipptConfigIn->profileId);

    telux::common::ErrorCode error = dataSettingsManager->getIpPassThroughConfig(ipptParams,
                                                                                 ipptConfigTel);
    if (error != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("ERROR - Failed to get ip pass through, error:%d ", static_cast<int>(error));
        return PA_FAULT;
    }
    else
    {
        PA_INFO("get ip pass through is success...");
    }

    // Convert operation type back
    switch (ipptConfigTel.ipptOpr)
    {
        case telux::data::Operation::DISABLE:
            ipptConfigOut->operation = TAF_PA_VLAN_IPPT_DISABLE;
            break;
        case telux::data::Operation::ENABLE:
            ipptConfigOut->operation = TAF_PA_VLAN_IPPT_ENABLE;
            break;
        default:
            ipptConfigOut->operation = TAF_PA_VLAN_IPPT_UNKNOWN;
            break;
    }

    PA_DEBUG("GetIPPassThroughConfig operation(%d)", (int)ipptConfigOut->operation);

    // Convert interface type back
    switch (ipptConfigTel.devConfig.nwInterface)
    {
        case telux::data::InterfaceType::WLAN:
            ipptConfigOut->ifType = TAF_PA_VLAN_IFACE_WLAN;
            break;
        case telux::data::InterfaceType::ETH:
            ipptConfigOut->ifType = TAF_PA_VLAN_IFACE_ETH;
            break;
        case telux::data::InterfaceType::ECM:
            ipptConfigOut->ifType = TAF_PA_VLAN_IFACE_ECM;
            break;
        case telux::data::InterfaceType::RNDIS:
            ipptConfigOut->ifType = TAF_PA_VLAN_IFACE_RNDIS;
            break;
        case telux::data::InterfaceType::MHI:
            ipptConfigOut->ifType = TAF_PA_VLAN_IFACE_MHI;
            break;
        default:
            ipptConfigOut->ifType = TAF_PA_VLAN_IFACE_UNKNOWN;
            break;
    }

    gsize src_len = g_strlcpy(ipptConfigOut->macAddr, ipptConfigTel.devConfig.macAddr.c_str(),
                              TAF_PA_NET_MAC_ADDR_MAX_LEN);

    if (src_len >= TAF_PA_NET_MAC_ADDR_MAX_LEN)
    {
       PA_ERROR("MAC truncated: src_len=%d, buf_size=%d, value='%s'",
             static_cast<size_t>(src_len),
             static_cast<unsigned>(TAF_PA_NET_MAC_ADDR_MAX_LEN),
             ipptConfigTel.devConfig.macAddr.c_str());
    }

    PA_DEBUG("GetIPPassThroughConfig iftype(%d)", (int)ipptConfigOut->ifType);
    PA_DEBUG("GetIPPassThroughConfig Mac %s", ipptConfigOut->macAddr);

    return PA_OK;
}

pa_result_t taf_pa_net_SetIPConfig
(
    const taf_pa_IpConfigParams_t *ipConfigParams,  // IN
    const taf_pa_IpConfig_t *ipConfig               // IN
)
{
    PA_INFO("Actual platform adapter implementation");

    //TAF_ERROR_IF_RET_VAL(ipConfigParams == NULL, PA_BAD_PARAMETER, "ipConfigParams is null");
    //TAF_ERROR_IF_RET_VAL(ipConfig == NULL, PA_BAD_PARAMETER, "ipConfig is null");
    if(ipConfigParams == NULL)
    {
        PA_ERROR("ipConfigParams is null");
        return PA_BAD_PARAMETER;
    }
    if(ipConfig == NULL)
    {
        PA_ERROR("ipConfig is null");
        return PA_BAD_PARAMETER;
    }


    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();

    telux::data::IpConfig ipConfigTel;
    telux::data::IpConfigParams ipConfigParamsTel;

    // Set ipConfigParams
    ipConfigParamsTel.ifType = static_cast<telux::data::InterfaceType>(ipConfigParams->ifType);
    ipConfigParamsTel.vlanId = ipConfigParams->vlanId;

    PA_DEBUG("SetIPConfig vlanid(%d)", (int)ipConfigParams->vlanId);

    // Convert IP family type
    switch (ipConfigParams->ipFamilyType)
    {
        case TAF_PA_VLAN_IPV4:
            ipConfigParamsTel.ipFamilyType = telux::data::IpFamilyType::IPV4;
            break;
        case TAF_PA_VLAN_IPV6:
            ipConfigParamsTel.ipFamilyType = telux::data::IpFamilyType::IPV6;
            break;
        default:
            PA_ERROR("Invalid IP family type (%d).", ipConfigParams->ipFamilyType);
            return PA_BAD_PARAMETER;
    }

    // Convert IP operation type
    switch (ipConfig->ipOpr)
    {
        case TAF_PA_VLAN_IP_OPR_ENABLE:
            ipConfigTel.ipOpr = telux::data::IpAssignOperation::ENABLE;
            break;
        case TAF_PA_VLAN_IP_OPR_DISABLE:
            ipConfigTel.ipOpr = telux::data::IpAssignOperation::DISABLE;
            break;
        case TAF_PA_VLAN_IP_OPR_RECONFIGURE:
            ipConfigTel.ipOpr = telux::data::IpAssignOperation::RECONFIGURE;
            break;
        default:
            ipConfigTel.ipOpr = telux::data::IpAssignOperation::UNKNOWN;
            break;
    }

    // Convert IP assignment type
    switch (ipConfig->ipAssignType)
    {
        case TAF_PA_VLAN_DYNAMIC_IP:
            ipConfigTel.ipType = telux::data::IpAssignType::DYNAMIC_IP;
            break;
        case TAF_PA_VLAN_STATIC_IP:
            ipConfigTel.ipType = telux::data::IpAssignType::STATIC_IP;
            break;
        default:
            ipConfigTel.ipType = telux::data::IpAssignType::UNKNOWN;
            break;
    }

    PA_DEBUG("SetIPConfig ipOpr(%d)", (int)ipConfig->ipOpr);
    PA_DEBUG("SetIPConfig ipAssignType(%d)", (int)ipConfig->ipAssignType);

    // If static IP, copy address information
    if (ipConfig->ipAssignType == TAF_PA_VLAN_STATIC_IP)
    {
        std::string interfaceAddress(ipConfig->ipAddrInfo.interfaceAddress);
        ipConfigTel.ipAddr.ifAddress = interfaceAddress;
        ipConfigTel.ipAddr.ifMask = ipConfig->ipAddrInfo.interfaceMask;

        std::string gwAddress(ipConfig->ipAddrInfo.gwAddress);
        ipConfigTel.ipAddr.gwAddress = gwAddress;

        std::string primaryDnsAddress(ipConfig->ipAddrInfo.primaryDnsAddress);
        ipConfigTel.ipAddr.primaryDnsAddress = primaryDnsAddress;

        std::string secondaryDnsAddress(ipConfig->ipAddrInfo.secondaryDnsAddress);
        ipConfigTel.ipAddr.secondaryDnsAddress = secondaryDnsAddress;

        PA_DEBUG("SetIPConfig if %s", ipConfigTel.ipAddr.ifAddress.data());
        PA_DEBUG("SetIPConfig gw %s", ipConfigTel.ipAddr.gwAddress.data());
        PA_DEBUG("SetIPConfig pDNS %s", ipConfigTel.ipAddr.primaryDnsAddress.data());
        PA_DEBUG("SetIPConfig sDNS %s", ipConfigTel.ipAddr.secondaryDnsAddress.data());
    }

    telux::common::ErrorCode error = dataSettingsManager->setIpConfig(ipConfigParamsTel,
                                                                      ipConfigTel);
    if (error != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("ERROR - Failed to set ip config, error:%d ", static_cast<int>(error));
        return PA_FAULT;
    }
    else
    {
        PA_INFO("set ip config is success...");
    }

    return PA_OK;
}

pa_result_t taf_pa_net_GetIPConfig
(
    const taf_pa_IpConfigParams_t *ipConfigParams,  // IN
    taf_pa_IpConfig_t *ipConfig                     // OUT
)
{
    PA_INFO("Actual platform adapter implementation");

    //TAF_ERROR_IF_RET_VAL(ipConfigParams == NULL, PA_BAD_PARAMETER, "ipConfigParams is null");
    //TAF_ERROR_IF_RET_VAL(ipConfig == NULL, PA_BAD_PARAMETER, "ipConfig is null");
    if(ipConfigParams == NULL)
    {
        PA_ERROR("ipConfigParams is null");
        return PA_BAD_PARAMETER;
    }
    if(ipConfig == NULL)
    {
        PA_ERROR("ipConfig is null");
        return PA_BAD_PARAMETER;
    }

    auto &pVlanAdaptor = taf_VlanAdaptor::getInstance();
    auto dataSettingsManager = pVlanAdaptor.getDataSettingsManager();

    telux::data::IpConfig ipConfigTel;
    telux::data::IpConfigParams ipConfigParamsTel;

    // Set ipConfigParams
    ipConfigParamsTel.ifType = static_cast<telux::data::InterfaceType>(ipConfigParams->ifType);
    ipConfigParamsTel.vlanId = ipConfigParams->vlanId;

    PA_DEBUG("GetIPConfig vlanid(%d)", (int)ipConfigParams->vlanId);

    // Convert IP family type
    switch (ipConfigParams->ipFamilyType)
    {
        case TAF_PA_VLAN_IPV4:
            ipConfigParamsTel.ipFamilyType = telux::data::IpFamilyType::IPV4;
            break;
        case TAF_PA_VLAN_IPV6:
            ipConfigParamsTel.ipFamilyType = telux::data::IpFamilyType::IPV6;
            break;
        default:
            PA_ERROR("Invalid IP family type (%d).", ipConfigParams->ipFamilyType);
            return PA_BAD_PARAMETER;
    }

    telux::common::ErrorCode error = dataSettingsManager->getIpConfig(ipConfigParamsTel,
                                                                      ipConfigTel);
    if (error != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("ERROR - Failed to get ip config, error:%d ", static_cast<int>(error));
        return PA_FAULT;
    }
    else
    {
        PA_INFO("get ip config is success...");
    }

    // Convert IP operation type back
    switch (ipConfigTel.ipOpr)
    {
        case telux::data::IpAssignOperation::ENABLE:
            ipConfig->ipOpr = TAF_PA_VLAN_IP_OPR_ENABLE;
            break;
        case telux::data::IpAssignOperation::DISABLE:
            ipConfig->ipOpr = TAF_PA_VLAN_IP_OPR_DISABLE;
            break;
        case telux::data::IpAssignOperation::RECONFIGURE:
            ipConfig->ipOpr = TAF_PA_VLAN_IP_OPR_RECONFIGURE;
            break;
        default:
            ipConfig->ipOpr = TAF_PA_VLAN_IP_OPR_UNKNOWN;
            break;
    }

    // Convert IP assignment type back
    switch (ipConfigTel.ipType)
    {
        case telux::data::IpAssignType::DYNAMIC_IP:
            ipConfig->ipAssignType = TAF_PA_VLAN_DYNAMIC_IP;
            break;
        case telux::data::IpAssignType::STATIC_IP:
            ipConfig->ipAssignType = TAF_PA_VLAN_STATIC_IP;
            break;
        default:
            ipConfig->ipAssignType = TAF_PA_VLAN_IP_UNKNOWN;
            break;
    }

    PA_DEBUG("GetIPConfig ipOpr(%d)", (int)ipConfig->ipOpr);
    PA_DEBUG("GetIPConfig ipAssignType(%d)", (int)ipConfig->ipAssignType);

    // If static IP, copy address information
    if (ipConfigTel.ipType == telux::data::IpAssignType::STATIC_IP)
    {
        g_strlcpy(ipConfig->ipAddrInfo.interfaceAddress,
                     ipConfigTel.ipAddr.ifAddress.c_str(), IP_PA_NET_ADDR_MAX_LEN);
        ipConfig->ipAddrInfo.interfaceMask = ipConfigTel.ipAddr.ifMask;

        g_strlcpy(ipConfig->ipAddrInfo.gwAddress,
                     ipConfigTel.ipAddr.gwAddress.c_str(), IP_PA_NET_ADDR_MAX_LEN);
        g_strlcpy(ipConfig->ipAddrInfo.primaryDnsAddress,
                     ipConfigTel.ipAddr.primaryDnsAddress.c_str(), IP_PA_NET_ADDR_MAX_LEN);
        g_strlcpy(ipConfig->ipAddrInfo.secondaryDnsAddress,
                     ipConfigTel.ipAddr.secondaryDnsAddress.c_str(), IP_PA_NET_ADDR_MAX_LEN);

        PA_DEBUG("GetIPConfig if %s", ipConfig->ipAddrInfo.interfaceAddress);
        PA_DEBUG("GetIPConfig gw %s", ipConfig->ipAddrInfo.gwAddress);
        PA_DEBUG("GetIPConfig pDNS %s", ipConfig->ipAddrInfo.primaryDnsAddress);
        PA_DEBUG("GetIPConfig sDNS %s", ipConfig->ipAddrInfo.secondaryDnsAddress);
    }
    else
    {
        PA_ERROR("IP config is DYNAMIC or UNKNOWN; dont copy address information");
    }

    return PA_OK;
}
