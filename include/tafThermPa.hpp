/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_therm.hpp
 * @brief Platform Adapter interface for Thermal Service
 *
 * This file defines the PA layer interface for thermal management operations.
 * It abstracts the underlying Telux thermal SDK APIs.
 */

#ifndef TAF_THERM_PA_HPP
#define TAF_THERM_PA_HPP

#include "tafCommonPa.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

//--------------------------------------------------------------------------------------------------
/**
 * Trip point types
 */
//--------------------------------------------------------------------------------------------------
enum class taf_pa_therm_TripType
{
    UNKNOWN,
    CRITICAL,
    HOT,
    PASSIVE,
    ACTIVE,
    CONFIGURABLE_HIGH,
    CONFIGURABLE_LOW
};

//--------------------------------------------------------------------------------------------------
/**
 * Trip event types
 */
//--------------------------------------------------------------------------------------------------
enum class taf_pa_therm_TripEvent
{
    NONE,
    CROSSED_UNDER,
    CROSSED_OVER
};

//--------------------------------------------------------------------------------------------------
/**
 * Thermal zone information structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t zoneId;
    int32_t currentTemp;
    int32_t passiveTemp;
    std::string description;
} taf_pa_therm_ThermalZoneInfo;

//--------------------------------------------------------------------------------------------------
/**
 * Trip point information structure
 * threshold : specific temperature point at which a thermal zone triggers trip event
 * hysteresis: the difference between current temperature of the device
 *             and the temperature above which certain trip point will be fired.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    taf_pa_therm_TripType tripType;
    int32_t threshold;
    int32_t hysteresis;
    uint32_t tripId;
    uint32_t thermalZoneId;
} taf_pa_therm_TripPointInfo;

//--------------------------------------------------------------------------------------------------
/**
 * Bound cooling device information structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t coolingDeviceId;
    std::vector<taf_pa_therm_TripPointInfo> bindingInfo;
} taf_pa_therm_BoundCoolingDevice;

//--------------------------------------------------------------------------------------------------
/**
 * Cooling device information structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t deviceId;
    uint32_t maxCoolingLevel;
    uint32_t currentCoolingLevel;
    std::string description;
} taf_pa_therm_CoolingDeviceInfo;

//--------------------------------------------------------------------------------------------------
/**
 * Trip event callback information
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    taf_pa_therm_TripPointInfo tripPoint;
    taf_pa_therm_TripEvent tripEvent;
    void* contextPtr;
} taf_pa_therm_TripEventInfo;

//--------------------------------------------------------------------------------------------------
/**
 * Cooling level change callback information
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    taf_pa_therm_CoolingDeviceInfo coolingDevice;
    void* contextPtr;
} taf_pa_therm_CoolingLevelChangeInfo;

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for trip event notification handler
 */
//--------------------------------------------------------------------------------------------------
typedef std::function<void(const taf_pa_therm_TripEventInfo&)> taf_pa_therm_TripEventHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for cooling level change notification handler
 */
//--------------------------------------------------------------------------------------------------
typedef std::function<void(const taf_pa_therm_CoolingLevelChangeInfo&)> taf_pa_therm_CoolingLevelChangeHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the thermal PA layer
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_Init(void);
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_Deinit(void);

//--------------------------------------------------------------------------------------------------
/**
 * Get list of all thermal zones
 *
 * @param[out] thermalZones Vector to store thermal zone information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetThermalZones(
    std::vector<taf_pa_therm_ThermalZoneInfo>& thermalZones
);

//--------------------------------------------------------------------------------------------------
/**
 * Get thermal zone by ID
 *
 * @param[in]  zoneId Thermal zone ID
 * @param[out] zoneInfo Thermal zone information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetThermalZoneById(
    uint32_t zoneId,
    taf_pa_therm_ThermalZoneInfo& zoneInfo
);

//--------------------------------------------------------------------------------------------------
/**
 * Get thermal zone by name
 *
 * @param[in]  zoneName Thermal zone name/description
 * @param[out] zoneInfo Thermal zone information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetThermalZoneByName(
    const std::string& zoneName,
    taf_pa_therm_ThermalZoneInfo& zoneInfo
);

//--------------------------------------------------------------------------------------------------
/**
 * Get trip points for a thermal zone
 *
 * @param[in]  zoneId Thermal zone ID
 * @param[out] tripPoints Vector to store trip point information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetTripPoints(
    uint32_t zoneId,
    std::vector<taf_pa_therm_TripPointInfo>& tripPoints
);

//--------------------------------------------------------------------------------------------------
/**
 * Get bound cooling devices for a thermal zone
 *
 * @param[in]  zoneId Thermal zone ID
 * @param[out] boundDevices Vector to store bound cooling device information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetBoundCoolingDevices(
    uint32_t zoneId,
    std::vector<taf_pa_therm_BoundCoolingDevice>& boundDevices
);

//--------------------------------------------------------------------------------------------------
/**
 * Get list of all cooling devices
 *
 * @param[out] coolingDevices Vector to store cooling device information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetCoolingDevices(
    std::vector<taf_pa_therm_CoolingDeviceInfo>& coolingDevices
);

//--------------------------------------------------------------------------------------------------
/**
 * Get cooling device by ID
 *
 * @param[in]  deviceId Cooling device ID
 * @param[out] deviceInfo Cooling device information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetCoolingDeviceById(
    uint32_t deviceId,
    taf_pa_therm_CoolingDeviceInfo& deviceInfo
);

//--------------------------------------------------------------------------------------------------
/**
 * Get cooling device by name
 *
 * @param[in]  deviceName Cooling device name/description
 * @param[out] deviceInfo Cooling device information
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_GetCoolingDeviceByName(
    const std::string& deviceName,
    taf_pa_therm_CoolingDeviceInfo& deviceInfo
);

//--------------------------------------------------------------------------------------------------
/**
 * Register trip event handler
 *
 * @param[in] handler Callback function for trip events
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_RegisterTripEventHandler(
    taf_pa_therm_TripEventHandler_t handler, void* contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Deregister trip event handler
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_DeregisterTripEventHandler(void);

//--------------------------------------------------------------------------------------------------
/**
 * Register cooling level change handler
 *
 * @param[in] handler Callback function for cooling level changes
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_RegisterCoolingLevelChangeHandler(
    taf_pa_therm_CoolingLevelChangeHandler_t handler, void* contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Deregister cooling level change handler
 *
 * @return TAF_PA_OK on success
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_therm_DeregisterCoolingLevelChangeHandler(void);

#endif // TAF_THERM_PA_HPP
