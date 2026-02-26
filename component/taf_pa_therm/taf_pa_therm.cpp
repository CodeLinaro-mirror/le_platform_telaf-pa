/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_therm.cpp
 * @brief Telux-based Platform Adapter implementation for Thermal Service
 */

#include "taf_pa_therm.hpp"
#include <telux/common/CommonDefines.hpp>
#include <telux/therm/ThermalDefines.hpp>
#include <telux/therm/ThermalFactory.hpp>
#include <telux/therm/ThermalListener.hpp>
#include <telux/therm/ThermalManager.hpp>
#include <future>
#include <chrono>
#include <map>

#define TAF_PA_THERM_MANAGER_TIMEOUT 30

// Global thermal manager and listener
static std::shared_ptr<telux::therm::IThermalManager> g_thermalManager = nullptr;
static taf_pa_therm_TripEventHandler_t g_tripEventHandler = nullptr;
static taf_pa_therm_CoolingLevelChangeHandler_t g_coolingLevelChangeHandler = nullptr;
static std::map<std::string, uint32_t> g_zoneNameToIdMap;
static std::map<std::string, uint32_t> g_deviceNameToIdMap;

//--------------------------------------------------------------------------------------------------
/**
 * Thermal listener class for handling Telux callbacks
 */
//--------------------------------------------------------------------------------------------------
class TafPaThermalListener : public telux::therm::IThermalListener
{
public:
    void onServiceStatusChange(telux::common::ServiceStatus serviceStatus) override
    {
        PA_INFO("Thermal service status changed: %d", static_cast<int>(serviceStatus));
    }

    void onTripEvent(std::shared_ptr<telux::therm::ITripPoint> tripPoint,
                     telux::therm::TripEvent tripEvent) override
    {
        if (g_tripEventHandler && tripPoint)
        {
            taf_pa_therm_TripEventInfo eventInfo;

            // Convert trip type
            switch (tripPoint->getType())
            {
                case telux::therm::TripType::CRITICAL:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::CRITICAL;
                    break;
                case telux::therm::TripType::HOT:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::HOT;
                    break;
                case telux::therm::TripType::PASSIVE:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::PASSIVE;
                    break;
                case telux::therm::TripType::ACTIVE:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::ACTIVE;
                    break;
                case telux::therm::TripType::CONFIGURABLE_HIGH:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::CONFIGURABLE_HIGH;
                    break;
                case telux::therm::TripType::CONFIGURABLE_LOW:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::CONFIGURABLE_LOW;
                    break;
                default:
                    eventInfo.tripPoint.tripType = taf_pa_therm_TripType::UNKNOWN;
                    break;
            }

            eventInfo.tripPoint.threshold = tripPoint->getThresholdTemp();
            eventInfo.tripPoint.hysteresis = tripPoint->getHysteresis();
            eventInfo.tripPoint.tripId = tripPoint->getTripId();
            eventInfo.tripPoint.thermalZoneId = tripPoint->getTZoneId();

            // Convert trip event
            switch (tripEvent)
            {
                case telux::therm::TripEvent::CROSSED_UNDER:
                    eventInfo.tripEvent = taf_pa_therm_TripEvent::CROSSED_UNDER;
                    break;
                case telux::therm::TripEvent::CROSSED_OVER:
                    eventInfo.tripEvent = taf_pa_therm_TripEvent::CROSSED_OVER;
                    break;
                default:
                    eventInfo.tripEvent = taf_pa_therm_TripEvent::NONE;
                    break;
            }
            g_tripEventHandler(eventInfo);
        }
    }

    void onCoolingDeviceLevelChange(std::shared_ptr<telux::therm::ICoolingDevice> coolingDevice) override
    {
        if (g_coolingLevelChangeHandler && coolingDevice)
        {
            taf_pa_therm_CoolingLevelChangeInfo changeInfo;
            changeInfo.coolingDevice.deviceId = coolingDevice->getId();
            changeInfo.coolingDevice.maxCoolingLevel = coolingDevice->getMaxCoolingLevel();
            changeInfo.coolingDevice.currentCoolingLevel = coolingDevice->getCurrentCoolingLevel();
            changeInfo.coolingDevice.description = coolingDevice->getDescription();

            g_coolingLevelChangeHandler(changeInfo);
        }
    }
};

static std::shared_ptr<TafPaThermalListener> g_thermalListener = nullptr;

//--------------------------------------------------------------------------------------------------
/**
 * Helper function to build zone name to ID map
 */
//--------------------------------------------------------------------------------------------------
static void BuildZoneNameToIdMap()
{
    if (!g_thermalManager)
        return;

    auto zones = g_thermalManager->getThermalZones();
    for (const auto& zone : zones)
    {
        g_zoneNameToIdMap[zone->getDescription()] = zone->getId();
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Helper function to build device name to ID map
 */
//--------------------------------------------------------------------------------------------------
static void BuildDeviceNameToIdMap()
{
    if (!g_thermalManager)
        return;

    auto devices = g_thermalManager->getCoolingDevices();
    for (const auto& device : devices)
    {
        g_deviceNameToIdMap[device->getDescription()] = device->getId();
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the thermal PA layer
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_Init(void)
{
    PA_INFO("Initializing Thermal PA layer");

    try
    {
        auto& thermalFactory = telux::therm::ThermalFactory::getInstance();
        auto promisePtr = std::make_shared<std::promise<telux::common::ServiceStatus>>();

        g_thermalManager = thermalFactory.getThermalManager(
            [promisePtr](telux::common::ServiceStatus status) {
                try {
                    promisePtr->set_value(status);
                } catch (const std::future_error& e) {
                    PA_ERROR("Promise already satisfied: %s", e.what());
                }
            },
            telux::common::ProcType::LOCAL_PROC
        );

        if (!g_thermalManager)
        {
            PA_ERROR("Failed to get thermal manager instance");
            return PA_FAULT;
        }

        auto future = promisePtr->get_future();
        if (future.wait_for(std::chrono::seconds(TAF_PA_THERM_MANAGER_TIMEOUT)) == std::future_status::ready)
        {
            telux::common::ServiceStatus status = future.get();
            if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                PA_ERROR("Thermal service not available");
                return PA_FAULT;
            }
        }
        else
        {
            PA_ERROR("Timeout waiting for thermal service");
            return PA_FAULT;
        }

        // Create and register listener
        g_thermalListener = std::make_shared<TafPaThermalListener>();
        auto result = g_thermalManager->registerListener(g_thermalListener);
        if (result != telux::common::Status::SUCCESS)
        {
            PA_ERROR("Failed to register thermal listener");
            return PA_FAULT;
        }

        // Build name to ID maps
        BuildZoneNameToIdMap();
        BuildDeviceNameToIdMap();

        PA_INFO("Thermal PA layer initialized successfully");
        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception during thermal PA init: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get list of all thermal zones
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetThermalZones(std::vector<taf_pa_therm_ThermalZoneInfo>& thermalZones)
{
    if (!g_thermalManager)
    {
        PA_ERROR("Thermal manager not initialized");
        return PA_FAULT;
    }

    try
    {
        auto zones = g_thermalManager->getThermalZones();

        for (const auto& zone : zones)
        {
            taf_pa_therm_ThermalZoneInfo zoneInfo;
            zoneInfo.zoneId = zone->getId();
            zoneInfo.currentTemp = zone->getCurrentTemp();
            zoneInfo.passiveTemp = zone->getPassiveTemp();
            zoneInfo.description = zone->getDescription();
            thermalZones.push_back(zoneInfo);
        }

        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception getting thermal zones: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get thermal zone by ID
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetThermalZoneById(uint32_t zoneId, taf_pa_therm_ThermalZoneInfo& zoneInfo)
{
    if (!g_thermalManager)
    {
        PA_ERROR("Thermal manager not initialized");
        return PA_FAULT;
    }

    try
    {
        auto zone = g_thermalManager->getThermalZone(zoneId);
        if (!zone)
        {
            PA_ERROR("Thermal zone %u not found", zoneId);
            return PA_FAULT;
        }

        zoneInfo.zoneId = zone->getId();
        zoneInfo.currentTemp = zone->getCurrentTemp();
        zoneInfo.passiveTemp = zone->getPassiveTemp();
        zoneInfo.description = zone->getDescription();

        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception getting thermal zone by ID: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get thermal zone by name
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetThermalZoneByName(const std::string& zoneName, taf_pa_therm_ThermalZoneInfo& zoneInfo)
{
    auto it = g_zoneNameToIdMap.find(zoneName);
    if (it == g_zoneNameToIdMap.end())
    {
        PA_ERROR("Thermal zone '%s' not found", zoneName.c_str());
        return PA_FAULT;
    }

    return taf_pa_therm_GetThermalZoneById(it->second, zoneInfo);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get trip points for a thermal zone
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetTripPoints(uint32_t zoneId, std::vector<taf_pa_therm_TripPointInfo>& tripPoints)
{
    if (!g_thermalManager)
    {
        PA_ERROR("Thermal manager not initialized");
        return PA_FAULT;
    }

    try
    {
        auto zone = g_thermalManager->getThermalZone(zoneId);
        if (!zone)
        {
            PA_ERROR("Thermal zone %u not found", zoneId);
            return PA_FAULT;
        }

        auto teluxTripPoints = zone->getTripPoints();

        for (const auto& tripPoint : teluxTripPoints)
        {
            taf_pa_therm_TripPointInfo info;

            // Convert trip type
            switch (tripPoint->getType())
            {
                case telux::therm::TripType::CRITICAL:
                    info.tripType = taf_pa_therm_TripType::CRITICAL;
                    break;
                case telux::therm::TripType::HOT:
                    info.tripType = taf_pa_therm_TripType::HOT;
                    break;
                case telux::therm::TripType::PASSIVE:
                    info.tripType = taf_pa_therm_TripType::PASSIVE;
                    break;
                case telux::therm::TripType::ACTIVE:
                    info.tripType = taf_pa_therm_TripType::ACTIVE;
                    break;
                case telux::therm::TripType::CONFIGURABLE_HIGH:
                    info.tripType = taf_pa_therm_TripType::CONFIGURABLE_HIGH;
                    break;
                case telux::therm::TripType::CONFIGURABLE_LOW:
                    info.tripType = taf_pa_therm_TripType::CONFIGURABLE_LOW;
                    break;
                default:
                    info.tripType = taf_pa_therm_TripType::UNKNOWN;
                    break;
            }

            info.threshold = tripPoint->getThresholdTemp();
            info.hysteresis = tripPoint->getHysteresis();
            info.tripId = tripPoint->getTripId();
            info.thermalZoneId = tripPoint->getTZoneId();

            tripPoints.push_back(info);
        }

        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception getting trip points: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get bound cooling devices for a thermal zone
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetBoundCoolingDevices(uint32_t zoneId, std::vector<taf_pa_therm_BoundCoolingDevice>& boundDevices)
{
    if (!g_thermalManager)
    {
        PA_ERROR("Thermal manager not initialized");
        return PA_FAULT;
    }

    try
    {
        auto zone = g_thermalManager->getThermalZone(zoneId);
        if (!zone)
        {
            PA_ERROR("Thermal zone %u not found", zoneId);
            return PA_FAULT;
        }

        auto teluxBoundDevices = zone->getBoundCoolingDevices();

        for (const auto& boundDevice : teluxBoundDevices)
        {
            taf_pa_therm_BoundCoolingDevice deviceInfo;
            deviceInfo.coolingDeviceId = boundDevice.coolingDeviceId;

            // Convert binding info
            for (const auto& tripPoint : boundDevice.bindingInfo)
            {
                taf_pa_therm_TripPointInfo info;

                switch (tripPoint->getType())
                {
                    case telux::therm::TripType::CRITICAL:
                        info.tripType = taf_pa_therm_TripType::CRITICAL;
                        break;
                    case telux::therm::TripType::HOT:
                        info.tripType = taf_pa_therm_TripType::HOT;
                        break;
                    case telux::therm::TripType::PASSIVE:
                        info.tripType = taf_pa_therm_TripType::PASSIVE;
                        break;
                    case telux::therm::TripType::ACTIVE:
                        info.tripType = taf_pa_therm_TripType::ACTIVE;
                        break;
                    case telux::therm::TripType::CONFIGURABLE_HIGH:
                        info.tripType = taf_pa_therm_TripType::CONFIGURABLE_HIGH;
                        break;
                    case telux::therm::TripType::CONFIGURABLE_LOW:
                        info.tripType = taf_pa_therm_TripType::CONFIGURABLE_LOW;
                        break;
                    default:
                        info.tripType = taf_pa_therm_TripType::UNKNOWN;
                        break;
                }

                info.threshold = tripPoint->getThresholdTemp();
                info.hysteresis = tripPoint->getHysteresis();
                info.tripId = tripPoint->getTripId();
                info.thermalZoneId = tripPoint->getTZoneId();

                deviceInfo.bindingInfo.push_back(info);
            }

            boundDevices.push_back(deviceInfo);
        }

        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception getting bound cooling devices: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get list of all cooling devices
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetCoolingDevices(std::vector<taf_pa_therm_CoolingDeviceInfo>& coolingDevices)
{
    if (!g_thermalManager)
    {
        PA_ERROR("Thermal manager not initialized");
        return PA_FAULT;
    }

    try
    {
        auto devices = g_thermalManager->getCoolingDevices();

        for (const auto& device : devices)
        {
            taf_pa_therm_CoolingDeviceInfo deviceInfo;
            deviceInfo.deviceId = device->getId();
            deviceInfo.maxCoolingLevel = device->getMaxCoolingLevel();
            deviceInfo.currentCoolingLevel = device->getCurrentCoolingLevel();
            deviceInfo.description = device->getDescription();
            coolingDevices.push_back(deviceInfo);
        }

        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception getting cooling devices: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get cooling device by ID
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetCoolingDeviceById(uint32_t deviceId, taf_pa_therm_CoolingDeviceInfo& deviceInfo)
{
    if (!g_thermalManager)
    {
        PA_ERROR("Thermal manager not initialized");
        return PA_FAULT;
    }

    try
    {
        auto device = g_thermalManager->getCoolingDevice(deviceId);
        if (!device)
        {
            PA_ERROR("Cooling device %u not found", deviceId);
            return PA_FAULT;
        }

        deviceInfo.deviceId = device->getId();
        deviceInfo.maxCoolingLevel = device->getMaxCoolingLevel();
        deviceInfo.currentCoolingLevel = device->getCurrentCoolingLevel();
        deviceInfo.description = device->getDescription();

        return PA_OK;
    }
    catch (const std::exception& e)
    {
        PA_ERROR("Exception getting cooling device by ID: %s", e.what());
        return PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get cooling device by name
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_GetCoolingDeviceByName(const std::string& deviceName, taf_pa_therm_CoolingDeviceInfo& deviceInfo)
{
    auto it = g_deviceNameToIdMap.find(deviceName);
    if (it == g_deviceNameToIdMap.end())
    {
        PA_ERROR("Cooling device '%s' not found", deviceName.c_str());
        return PA_FAULT;
    }

    return taf_pa_therm_GetCoolingDeviceById(it->second, deviceInfo);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register trip event handler
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_RegisterTripEventHandler(
    taf_pa_therm_TripEventHandler_t handler,
    void* contextPtr
)
{
    if (!handler)
    {
        PA_ERROR("Invalid handler");
        return PA_FAULT;
    }

    g_tripEventHandler = handler;
    PA_INFO("Trip event handler registered");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deregister trip event handler
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_DeregisterTripEventHandler(void)
{
    g_tripEventHandler = nullptr;
    PA_INFO("Trip event handler deregistered");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register cooling level change handler
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_RegisterCoolingLevelChangeHandler(
    taf_pa_therm_CoolingLevelChangeHandler_t handler,
    void* contextPtr
)
{
    if (!handler)
    {
        PA_ERROR("Invalid handler");
        return PA_FAULT;
    }

    g_coolingLevelChangeHandler = handler;
    PA_INFO("Cooling level change handler registered");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deregister cooling level change handler
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_therm_DeregisterCoolingLevelChangeHandler(void)
{
    g_coolingLevelChangeHandler = nullptr;
    PA_INFO("Cooling level change handler deregistered");
    return PA_OK;
}
