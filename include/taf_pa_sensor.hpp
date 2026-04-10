/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __TAF_PA_SENSOR_HPP__
#define __TAF_PA_SENSOR_HPP__

#include <functional>
#include <any>
#include <vector>
#include <memory>
#include <string>
#include "taf_pa_common.h"

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

// Collection of event callbacks for a sensor.
struct taf_pa_sensor_EventListener {
    taf_pa_sensor_OnEventCb onEvent;
    taf_pa_sensor_SelfTestFailedCb onSelfTestFailed;
};

// Initializes the Sensor subsystem.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_Init(int8_t& listSize);

// Retrieves information about a specific sensor.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_GetSensorInfo(
    int8_t index,
    taf_pa_sensor_BasicInfo &basicInfo,
    taf_pa_sensor_ConfigInfo &configInfo,
    taf_pa_sensor_Capabilities &Capabilities
);

// Gets a client ID for a sensor instance by its name.
PA_SHARED PA_WEAK taf_pa_sensor_SensorId taf_pa_sensor_GetSensorClient(const std::string& sensorName);

// Releases a sensor client associated with the given ID.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_ReleaseSensorClient(taf_pa_sensor_SensorId sensorId);

// Activates a sensor with specified parameters.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_Activate(
    taf_pa_sensor_SensorId sensorId,
    double sampleRate,
    uint32_t batchCount,
    bool isRotated
);

// Sets a new reference coordinate system using Euler angles.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_SetEulerAngle(
    taf_pa_sensor_SensorId sensorId,
    double pitch,
    double roll,
    double yaw
);

// Deactivates a sensor.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_Deactivate(taf_pa_sensor_SensorId sensorId);

// Performs a self-test on the specified sensor.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_SelfTest(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_SelfTestMode mode,
    taf_pa_sensor_SelfTestResultCb callback,
    std::any context
);

// Registers an event listener.
PA_SHARED PA_WEAK pa_result_t taf_pa_sensor_RegisterListener(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_EventListener* eventListener,
    std::any context
);

} // End namespace tafpa::sensor

#endif // __TAF_PA_SENSOR_HPP__