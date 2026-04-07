/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_SENSOR_PA_HPP
#define TAF_SENSOR_PA_HPP

#include <functional>
#include <any>
#include <vector>
#include <memory>
#include <string>
#include "tafCommonPa.h"

namespace tafpa::sensor {

// Alias for sensor instance identifiers.
// This ID uniquely identifies an active sensor instance.
using taf_pa_sensor_SensorId = uint64_t;

// Defines types of sensors.
enum class taf_pa_sensor_SensorType : uint8_t {
    ACCELEROMETER = 1,
    GYROSCOPE = 2,
    INVALID = 0xFF
};

// Basic information about a sensor.
struct taf_pa_sensor_BasicInfo {
    int id;
    int version;
    taf_pa_sensor_SensorType sensorType;
    std::string sensorName;
    std::string vendorName;
};

// Configuration capabilities for a sensor.
struct taf_pa_sensor_ConfigInfo {
    std::vector<float> samplingRateList;
    double maxSamplingRate;
    uint32_t maxBatchCount;
    uint32_t minBatchCount;
};

// Physical capabilities of a sensor.
struct taf_pa_sensor_Capabilities {
    int range;
    double resolution;
    double maxRange;
};

// A single sensor event measurement.
struct taf_pa_sensor_Event {
    uint64_t timestamp;
    double x;
    double y;
    double z;
    double xb;
    double yb;
    double zb;
};

// Defines modes for sensor self-testing.
enum class taf_pa_sensor_SelfTestMode : uint8_t {
    POSITIVE = 1,
    NEGATIVE = 2,
    BOTH = 3
};

// Callback for sensor event notifications.
using taf_pa_sensor_OnEventCb = std::function<void(
    taf_pa_sensor_SensorId sensorId,
    std::shared_ptr<const std::vector<taf_pa_sensor_Event>> events,
    std::any context
)>;

// Callback for voluntary self-test failures.
using taf_pa_sensor_SelfTestFailedCb = std::function<void(
    taf_pa_sensor_SensorId sensorId,
    uint64_t timestamp,
    std::any context
)>;

// Callback for self-test operation results.
using taf_pa_sensor_SelfTestResultCb = std::function<void(
    taf_pa_sensor_SensorId sensorId,
    pa_result_t result,
    uint64_t timestamp,
    std::any context
)>;

// Callback for configuration update notifications.
using taf_pa_sensor_ConfigUpdateCb = std::function<void(
    taf_pa_sensor_SensorId sensorId,
    double samplingRate,
    uint32_t batchCount,
    bool isRotated,
    std::any context
)>;

// Sensor capability/service state information.
struct taf_pa_sensor_CapabilityInfo {
    // True if the sensor/service is present.
    bool isAvailable;
    // True if the sensor/service can be used
    bool isEnabled;
    uint32_t capabilityMask;
};

// Callback for sensor capability/service state change notifications.
using taf_pa_sensor_CapabilityCb = std::function<void(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_CapabilityInfo capabilityInfo,
    std::any context
)>;

// Collection of event callbacks for a sensor.
struct taf_pa_sensor_EventListener {
    taf_pa_sensor_OnEventCb onEvent;
    taf_pa_sensor_SelfTestFailedCb onSelfTestFailed;
};

// Initializes the Sensor subsystem.
PA_SHARED pa_result_t taf_pa_sensor_Init(int8_t& listSize);

PA_SHARED pa_result_t taf_pa_sensor_Deinit();

// Retrieves information about a specific sensor.
PA_SHARED pa_result_t taf_pa_sensor_GetSensorInfo(
    int8_t index,
    taf_pa_sensor_BasicInfo &basicInfo,
    taf_pa_sensor_ConfigInfo &configInfo,
    taf_pa_sensor_Capabilities &Capabilities
);

// Gets a client ID for a sensor instance by its name.
PA_SHARED taf_pa_sensor_SensorId taf_pa_sensor_GetSensorClient(const std::string& sensorName);

// Releases a sensor client associated with the given ID.
PA_SHARED pa_result_t taf_pa_sensor_ReleaseSensorClient(taf_pa_sensor_SensorId sensorId);

// Sets the configuration for a sensor.
PA_SHARED pa_result_t taf_pa_sensor_SetConfig(
    taf_pa_sensor_SensorId sensorId,
    double samplingRate,
    uint32_t batchCount
);

// Activates a sensor.
PA_SHARED pa_result_t taf_pa_sensor_Activate(taf_pa_sensor_SensorId sensorId);

// Sets a new reference coordinate system using Euler angles.
PA_SHARED pa_result_t taf_pa_sensor_SetEulerAngle(
    taf_pa_sensor_SensorId sensorId,
    double pitch,
    double roll,
    double yaw
);

// Deactivates a sensor.
PA_SHARED pa_result_t taf_pa_sensor_Deactivate(taf_pa_sensor_SensorId sensorId);

// Performs a self-test on the specified sensor.
PA_SHARED pa_result_t taf_pa_sensor_SelfTestAsync(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_SelfTestMode mode,
    taf_pa_sensor_SelfTestResultCb callback,
    std::any context
);

// Adds an event listener.
PA_SHARED pa_result_t taf_pa_sensor_AddListener(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_EventListener* eventListener,
    std::any context
);

// Removes an event listener.
PA_SHARED pa_result_t taf_pa_sensor_RemoveListener(
    taf_pa_sensor_SensorId sensorId
);

// Adds a configuration update handler to receive notifications when sensor configuration changes.
PA_SHARED pa_result_t taf_pa_sensor_AddConfigUpdateHandler(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_ConfigUpdateCb callback,
    std::any context
);

// Removes a configuration update handler.
PA_SHARED pa_result_t taf_pa_sensor_RemoveConfigUpdateHandler(
    taf_pa_sensor_SensorId sensorId
);

// Adds a capability handler to receive notifications when sensor service state changes.
PA_SHARED pa_result_t taf_pa_sensor_AddCapabilityHandler(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_CapabilityCb callback,
    std::any context
);

// Removes a capability handler.
PA_SHARED pa_result_t taf_pa_sensor_RemoveCapabilityHandler(
    taf_pa_sensor_SensorId sensorId
);

} // End namespace tafpa::sensor

#endif // TAF_SENSOR_PA_HPP
