/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <chrono>
#include <mutex>
#include <future>
#include <map>
#include <algorithm>
#include <thread>
#include <bitset>

#include "telux/sensor/SensorManager.hpp"
#include "telux/common/CommonDefines.hpp"
#include "telux/sensor/SensorDefines.hpp"
#include "telux/sensor/SensorClient.hpp"
#include "telux/sensor/SensorFactory.hpp"

#include "tafSensorPa.hpp"

#define MAX_INIT_TIMEOUT 5
#define SEC_TO_NANOS 1000000000

using namespace telux::sensor;
using namespace telux::common;
using namespace tafpa::sensor;

class ReusableIdGenerator {
public:
    static constexpr size_t MAX_SENSOR_IDS = 64;
    static constexpr taf_pa_sensor_SensorId INVALID_SENSOR_ID = 0;

    ReusableIdGenerator() : idMask_() {
    }

    taf_pa_sensor_SensorId acquireId() {
        std::lock_guard<std::mutex> lock(mtx_);
        for (size_t i = 0; i < MAX_SENSOR_IDS; ++i) {
            if (!idMask_.test(i)) {
                idMask_.set(i);
                return static_cast<taf_pa_sensor_SensorId>(i + 1);
            }
        }
        PA_ERROR("No free sensor IDs available. Maximum %zu IDs reached.", MAX_SENSOR_IDS);
        return INVALID_SENSOR_ID;
    }

    void releaseId(taf_pa_sensor_SensorId id) {
        if (id == INVALID_SENSOR_ID || id > MAX_SENSOR_IDS) {
            PA_ERROR("Attempted to release invalid sensor ID: %llu", id);
            return;
        }
        std::lock_guard<std::mutex> lock(mtx_);
        size_t index = static_cast<size_t>(id - 1);
        if (!idMask_.test(index)) {
            PA_DEBUG("Sensor ID %llu was already released or not in use.", id);
        }
        idMask_.reset(index);
        PA_DEBUG("Sensor ID %llu released.", id);
    }

private:
    std::mutex mtx_;
    std::bitset<MAX_SENSOR_IDS> idMask_;
};

class SensorPAController {
public:
    class PASensorClient : public telux::sensor::ISensorEventListener,
                           public std::enable_shared_from_this<PASensorClient> {
    public:
        PASensorClient(std::shared_ptr<telux::sensor::ISensorClient> sensorClient_, bool isCalibrated_)
            : sensorClient_(sensorClient_), isCalibrated_(isCalibrated_), isActivated_(false),eventListenerContext_(),sensorId_(0) {}

        void onEvent(std::shared_ptr<std::vector<SensorEvent>> events) override;
        void onConfigurationUpdate(SensorConfiguration configuration) override;
        void onSelfTestFailed() override;

        telux::common::Status Activate();
        telux::common::Status Deactivate();
        telux::common::Status Configure(SensorConfiguration config);
        telux::common::Status SelfTest(taf_pa_sensor_SensorId sensorId,
                                      SelfTestType type,
                                      taf_pa_sensor_SelfTestResultCb callback,
                                      std::any context);
        void Init();
        void CleanUp();

        pa_result_t RegisterEventListener(
            taf_pa_sensor_SensorId sensorId,
            taf_pa_sensor_EventListener* listener,
            std::any context);

        pa_result_t RegisterConfigUpdateHandler(
            taf_pa_sensor_SensorId sensorId,
            taf_pa_sensor_ConfigUpdateCb callback,
            std::any context);

        pa_result_t UnregisterConfigUpdateHandler(
            taf_pa_sensor_SensorId sensorId);

        pa_result_t RegisterCapabilityHandler(
            taf_pa_sensor_SensorId sensorId,
            taf_pa_sensor_CapabilityCb callback,
            std::any context);

        pa_result_t UnregisterCapabilityHandler(
            taf_pa_sensor_SensorId sensorId);

        bool isActivated() {
            return isActivated_;
        }

    private:
        std::shared_ptr<telux::sensor::ISensorClient> sensorClient_;
        bool isCalibrated_;
        bool isActivated_;
        std::mutex mtx_;
        taf_pa_sensor_EventListener* eventListener_;
        taf_pa_sensor_SensorId sensorId_;
        std::any eventListenerContext_;
        taf_pa_sensor_ConfigUpdateCb configUpdateCallback_;
        std::any configUpdateContext_;
        taf_pa_sensor_CapabilityCb capabilityCallback_;
        std::any capabilityContext_;
    };

    static std::shared_ptr<SensorPAController> getInstance() {
        static std::shared_ptr<SensorPAController> instance(new SensorPAController());
        return instance;
    }

    std::shared_ptr<telux::sensor::ISensorManager> getSensorManager() {
        return sensorManager_;
    }

    size_t getSensorListSize() {
        return sList.size();
    }

    telux::sensor::SensorInfo getSensorInfo(int index) {
        return sList[index];
    }

    pa_result_t initialize();
    pa_result_t deinitialize();
    pa_result_t MapStatus(telux::common::Status status);
    pa_result_t MapErrorCode(telux::common::ErrorCode errorCode);

    std::shared_ptr<PASensorClient> GetClientPtr(taf_pa_sensor_SensorId id);
    taf_pa_sensor_SensorId CreateSensorClient(const std::string& sensorName);
    pa_result_t ReleaseSensorClient(taf_pa_sensor_SensorId id);
    SensorPAController() = default;
    ~SensorPAController() = default;
private:
    SensorPAController(const SensorPAController&) = delete;
    SensorPAController& operator=(const SensorPAController&) = delete;

    std::shared_ptr<telux::sensor::ISensorManager> sensorManager_;
    std::vector<telux::sensor::SensorInfo> sList;

    std::map<taf_pa_sensor_SensorId, std::shared_ptr<PASensorClient>> sensorClients_;
    ReusableIdGenerator id_generator_;
};

pa_result_t SensorPAController::initialize() {
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;
    startTime = std::chrono::system_clock::now();
    std::promise<telux::common::ServiceStatus> prom;

    auto &sensorFactory = telux::sensor::SensorFactory::getInstance();
    sensorManager_ = sensorFactory.getSensorManager(
        [&prom](telux::common::ServiceStatus status) { prom.set_value(status); });

    if (!sensorManager_) {
        PA_CRIT("Failed to get SensorManager");
        return PA_FAULT;
    }

    telux::common::ServiceStatus managerStatus = sensorManager_->getServiceStatus();
    if (managerStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("Sensor subsystem is not ready, Please wait ...");
    }

    auto initFuture = prom.get_future();
    auto waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    if (waitStatus == std::future_status::timeout) {
        PA_CRIT("*** ERROR - Timeout to get sensor manager ready");
        return PA_TIMEOUT;
    } else {
        managerStatus = initFuture.get();
        if (managerStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            endTime = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsedTime = endTime - startTime;
            PA_INFO("Elapsed Time for Sensor Subsystems to ready : %lf", elapsedTime.count());
        } else {
            PA_CRIT("ERROR - Unable to initialize sensor subsystem");
            return PA_FAULT;
        }
    }

    if (sensorManager_) {
        telux::common::Status status = sensorManager_->getAvailableSensorInfo(sList);
        if (status != telux::common::Status::SUCCESS) {
            PA_ERROR("Unable to get available sensors list");
            return MapStatus(status);
        }
    }
    return PA_OK;
}

pa_result_t SensorPAController::deinitialize() {
    PA_INFO("Starting sensor platform adaptor deinitialization...");

    // Step 1: Release all active sensor clients. Each ReleaseSensorClient() call
    // deactivates the sensor (if active) and deregisters the SDK event listener.
    PA_INFO("Releasing all active sensor clients...");
    {
        // Collect IDs first to avoid iterator invalidation during erasure.
        std::vector<taf_pa_sensor_SensorId> clientIds;
        clientIds.reserve(sensorClients_.size());
        for (auto& entry : sensorClients_) {
            clientIds.push_back(entry.first);
        }
        for (auto id : clientIds) {
            ReleaseSensorClient(id);
        }
    }

    // Step 2: Clear the cached sensor info list so stale entries are not
    // visible after a subsequent re-initialization.
    PA_INFO("Clearing sensor info list");
    sList.clear();

    // Step 3: Reset the sensor manager shared pointer so the underlying SDK
    // object is released once no other owners remain.
    PA_INFO("Resetting sensorManager_");
    sensorManager_.reset();

    PA_INFO("Sensor platform adaptor deinitialization complete.");
    return PA_OK;
}

// register sensor for notifications
void SensorPAController::PASensorClient::Init() {
    sensorClient_->registerListener(shared_from_this());
}

// deregister sensor for notifications
void SensorPAController::PASensorClient::CleanUp() {
    sensorClient_->deregisterListener(shared_from_this());
}

pa_result_t SensorPAController::MapStatus(telux::common::Status status) {
    switch (status) {
        case telux::common::Status::SUCCESS:
            PA_INFO("Operation processed successfully");
            return PA_OK;
        case telux::common::Status::FAILED:
            PA_INFO("Operation processing failed");
            return PA_FAULT;
        case telux::common::Status::INVALIDPARAM:
            PA_INFO("Input parameters are invalid");
            return PA_BAD_PARAMETER;
        case telux::common::Status::NOTALLOWED:
            PA_INFO("Operation not allowed");
            return PA_NOT_PERMITTED;
        case telux::common::Status::NOTIMPLEMENTED:
            PA_INFO("Feature not supported");
            return PA_NOT_IMPLEMENTED;
        case telux::common::Status::CONNECTIONLOST:
            PA_INFO("Connection to Socket server lost");
            return PA_COMM_ERROR;
        case telux::common::Status::EXPIRED:
            PA_INFO("Operation has expired");
            return PA_TIMEOUT;
        case telux::common::Status::NOTSUPPORTED:
            PA_INFO("Not supported on target platform");
            return PA_UNSUPPORTED;
        default:
            return PA_FAULT;
    }
}

pa_result_t SensorPAController::MapErrorCode(telux::common::ErrorCode errorCode) {
    switch (errorCode) {
        case telux::common::ErrorCode::SUCCESS:
            PA_INFO("Operation processed successfully");
            return PA_OK;
        case telux::common::ErrorCode::GENERIC_FAILURE:
            PA_ERROR("Operation processing failed");
            return PA_FAULT;
        case telux::common::ErrorCode::INVALID_ARGUMENTS:
            PA_ERROR("Input parameters are invalid");
            return PA_BAD_PARAMETER;
        case telux::common::ErrorCode::OPERATION_NOT_ALLOWED:
            PA_ERROR("Operation not allowed");
            return PA_NOT_PERMITTED;
        case telux::common::ErrorCode::TIMEOUT_ERROR:
            PA_ERROR("TimeOut Error");
            return PA_TIMEOUT;
        case telux::common::ErrorCode::INFO_UNAVAILABLE:
            PA_ERROR("Information not available");
            return PA_UNAVAILABLE;
        case telux::common::ErrorCode::SUBSYSTEM_UNAVAILABLE:
            PA_ERROR("Subsystem Not Available");
            return PA_UNAVAILABLE;
        case telux::common::ErrorCode::REQUEST_NOT_SUPPORTED:
            PA_ERROR("Request Not supported");
            return PA_UNSUPPORTED;
        default:
            return PA_FAULT;
    }
}

// Create a client for a sensor and return a unique ID
taf_pa_sensor_SensorId SensorPAController::CreateSensorClient(const std::string& sensorName) {
    std::shared_ptr<telux::sensor::ISensorClient> sdkSensorClient;
    auto sensorMngr = getSensorManager();
    telux::common::Status status = sensorMngr->getSensorClient(sdkSensorClient, sensorName);
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("Unable to create client for given sensor %s", sensorName.c_str());
        return ReusableIdGenerator::INVALID_SENSOR_ID;
    }

    SensorType type = sdkSensorClient->getSensorInfo().type;
    bool isCalibrated = true;
    if (type == telux::sensor::SensorType::GYROSCOPE_UNCALIBRATED ||
        type == telux::sensor::SensorType::ACCELEROMETER_UNCALIBRATED) {
        isCalibrated = false;
    }

    taf_pa_sensor_SensorId newId = id_generator_.acquireId();
    if (newId == ReusableIdGenerator::INVALID_SENSOR_ID) {
        PA_ERROR("Failed to acquire a new sensor ID for %s. Max IDs reached.", sensorName.c_str());
        return ReusableIdGenerator::INVALID_SENSOR_ID;
    }

    std::shared_ptr<PASensorClient> paSensorClient =
        std::make_shared<PASensorClient>(sdkSensorClient, isCalibrated);

    paSensorClient->Init();

    sensorClients_[newId] = paSensorClient;
    PA_DEBUG("Created sensor client for %s with ID %llu", sensorName.c_str(), newId);
    return newId;
}

pa_result_t SensorPAController::ReleaseSensorClient(taf_pa_sensor_SensorId id) {
    PA_DEBUG("Release Sensor Client with ID %llu", id);
    auto it = sensorClients_.find(id);
    if (it == sensorClients_.end()) {
        PA_ERROR("Invalid sensor ID (%llu) provided for release!", id);
        return PA_BAD_PARAMETER;
    }

    if (it->second) {
        if (it->second->isActivated()) {
            it->second->Deactivate();
        }
        it->second->CleanUp();
    }
    sensorClients_.erase(it);

    id_generator_.releaseId(id);

    PA_INFO("Sensor client with ID %llu released.", id);
    return PA_OK;
}

std::shared_ptr<SensorPAController::PASensorClient>
SensorPAController::GetClientPtr(taf_pa_sensor_SensorId id) {
    auto it = sensorClients_.find(id);
    if (it != sensorClients_.end()) {
        return it->second;
    }
    PA_ERROR("No sensor client found for ID %llu", id);
    return nullptr;
}

telux::common::Status SensorPAController::PASensorClient::Activate() {
    telux::common::Status status = sensorClient_->activate();
    if (status == telux::common::Status::SUCCESS) {
        isActivated_ = true;
    }
    return status;
}

telux::common::Status SensorPAController::PASensorClient::Deactivate() {
    telux::common::Status status = sensorClient_->deactivate();
    if (status == telux::common::Status::SUCCESS) {
        isActivated_ = false;
    }
    return status;
}

telux::common::Status SensorPAController::PASensorClient::Configure(SensorConfiguration config) {
    telux::common::Status status = sensorClient_->configure(config);
    return status;
}

telux::common::Status SensorPAController::PASensorClient::SelfTest(
    taf_pa_sensor_SensorId sensorId,
    SelfTestType type,
    taf_pa_sensor_SelfTestResultCb callback,
    std::any context)
{
    auto paCtrl = SensorPAController::getInstance();
    auto cb = [sensorId,callback,context,paCtrl](telux::common::ErrorCode error,
        SelfTestResultParams selfTestResultParams){
            pa_result_t result;
            uint64_t timestamp=0;
            if(error == telux::common::ErrorCode::SUCCESS) {
                timestamp = selfTestResultParams.timestamp_;
                if(selfTestResultParams.sensorResultType_ == SensorResultType::CURRENT){
                    result = PA_OK;
                }
                else{
                    result = PA_BUSY;
                }
            }
            else{
                result = paCtrl->MapErrorCode(error);
            }
            if(callback){
                callback(sensorId,result,timestamp,context);
            }
        };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  sensorClient_->selfTest(type,cb);
    return status;
}

taf_pa_sensor_SensorId tafpa::sensor::taf_pa_sensor_GetSensorClient(const std::string& sensorName) {
    auto paCtrl = SensorPAController::getInstance();
    return paCtrl->CreateSensorClient(sensorName);
}

pa_result_t tafpa::sensor::taf_pa_sensor_ReleaseSensorClient(taf_pa_sensor_SensorId sensorId) {
    auto paCtrl = SensorPAController::getInstance();
    return paCtrl->ReleaseSensorClient(sensorId);
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_GetSensorInfo(
    int8_t index,
    taf_pa_sensor_BasicInfo& basicInfo,
    taf_pa_sensor_ConfigInfo& configInfo,
    taf_pa_sensor_Capabilities& Capabilities) {

    auto paCtrl = SensorPAController::getInstance();
    if (index < 0 || static_cast<size_t>(index) >= paCtrl->getSensorListSize()) {
        PA_ERROR("Sensor index %d is out of range (max %zu)", index, paCtrl->getSensorListSize());
        return PA_OUT_OF_RANGE;
    }

    telux::sensor::SensorInfo sInfo = paCtrl->getSensorInfo(index);
    basicInfo.id = sInfo.id;
    basicInfo.version = sInfo.version;
    basicInfo.sensorName = sInfo.name;
    basicInfo.vendorName = sInfo.vendor;

    configInfo.samplingRateList = sInfo.samplingRates;
    configInfo.maxSamplingRate = sInfo.maxSamplingRate;
    configInfo.minBatchCount = sInfo.minBatchCountSupported;
    configInfo.maxBatchCount = sInfo.maxBatchCountSupported;

    Capabilities.range = sInfo.range;
    Capabilities.resolution = sInfo.resolution;
    Capabilities.maxRange = sInfo.maxRange;

    telux::sensor::SensorType type = sInfo.type;
    if (type == telux::sensor::SensorType::ACCELEROMETER ||
        type == telux::sensor::SensorType::ACCELEROMETER_UNCALIBRATED) {
        basicInfo.sensorType = taf_pa_sensor_SensorType::ACCELEROMETER;
    } else if (type == telux::sensor::SensorType::GYROSCOPE ||
               type == telux::sensor::SensorType::GYROSCOPE_UNCALIBRATED) {
        basicInfo.sensorType = taf_pa_sensor_SensorType::GYROSCOPE;
    } else {
        basicInfo.sensorType = taf_pa_sensor_SensorType::INVALID;
    }
    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_SetEulerAngle(
    taf_pa_sensor_SensorId sensorId,
    double pitch,
    double roll,
    double yaw) {

    auto paCtrl = SensorPAController::getInstance();
    auto sensorMngr = paCtrl->getSensorManager();

    EulerAngleConfig eulerAngleConfig;
    eulerAngleConfig.pitch = pitch;
    eulerAngleConfig.roll = roll;
    eulerAngleConfig.yaw = yaw;

    Status status = sensorMngr->setEulerAngleConfig(eulerAngleConfig);
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("Not able to set euler angle with status code %d", static_cast<int>(status));
        return paCtrl->MapStatus(status);
    }
    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_SetConfig(
    taf_pa_sensor_SensorId sensorId,
    double samplingRate,
    uint32_t batchCount) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    SensorConfiguration s;
    s.samplingRate = samplingRate;
    s.batchCount = batchCount;
    s.isRotated = false;
    s.validityMask.set(SensorConfigParams::SAMPLING_RATE);
    s.validityMask.set(SensorConfigParams::BATCH_COUNT);
    s.validityMask.set(SensorConfigParams::ROTATE);

    telux::common::Status status = clientPtr->Configure(s);
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("Sensor Configuration failed for ID %llu with status code %d", sensorId, static_cast<int>(status));
        return paCtrl->MapStatus(status);
    }
    PA_INFO("Sensor Configuration done for ID %llu", sensorId);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_Activate(taf_pa_sensor_SensorId sensorId) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    if (clientPtr->isActivated()) {
        PA_ERROR("Sensor with ID %llu already activated!", sensorId);
        return PA_UNAVAILABLE;
    }

    telux::common::Status status = clientPtr->Activate();
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("Sensor activation failed for ID %llu with status code %d", sensorId, static_cast<int>(status));
        return paCtrl->MapStatus(status);
    }
    PA_INFO("Sensor Activation done for ID %llu", sensorId);
    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_Deactivate(taf_pa_sensor_SensorId sensorId) {
    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    if (!clientPtr->isActivated()) {
        PA_ERROR("Sensor with ID %llu already deactivated!", sensorId);
        return PA_UNAVAILABLE;
    }

    telux::common::Status status = clientPtr->Deactivate();
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("Sensor Deactivation failed for ID %llu with status code %d", sensorId, static_cast<int>(status));
        return paCtrl->MapStatus(status);
    }
    PA_INFO("Sensor Deactivation done for ID %llu", sensorId);
    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_SelfTestAsync(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_SelfTestMode mode,
    taf_pa_sensor_SelfTestResultCb callback,
    std::any context) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    SelfTestType type;
    switch (mode) {
        case taf_pa_sensor_SelfTestMode::POSITIVE:
            type = SelfTestType::POSITIVE;
            break;
        case taf_pa_sensor_SelfTestMode::NEGATIVE:
            type = SelfTestType::NEGATIVE;
            break;
        case taf_pa_sensor_SelfTestMode::BOTH:
        default:
            type = SelfTestType::ALL;
            break;
    }

    telux::common::Status status = clientPtr->SelfTest(sensorId, type, callback, context);
    return paCtrl->MapStatus(status);
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_AddListener(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_EventListener* eventListener,
    std::any context) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    pa_result_t result = clientPtr->RegisterEventListener(sensorId, eventListener, context);
    if (result == PA_OK) {
        PA_INFO("Add Event Listener successfully for ID %llu", sensorId);
        return PA_OK;
    }
    PA_ERROR("Failed to add event listener for ID %llu", sensorId);
    return PA_FAULT;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_RemoveListener(
    taf_pa_sensor_SensorId sensorId) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    // Clear the event listener by setting it to nullptr
    pa_result_t result = clientPtr->RegisterEventListener(sensorId, nullptr, std::any());
    if (result == PA_OK) {
        PA_INFO("Remove Event Listener successfully for ID %llu", sensorId);
        return PA_OK;
    }
    PA_ERROR("Failed to remove event listener for ID %llu", sensorId);
    return PA_FAULT;
}
PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_Init(int8_t& listSize) {
    auto paCtrl = SensorPAController::getInstance();
    pa_result_t res = paCtrl->initialize();
    if (res == PA_OK) {
        PA_INFO("Sensor Platform adapter initialization done.");
        listSize = static_cast<int8_t>(paCtrl->getSensorListSize());
    } else {
        PA_CRIT("Sensor Platform adapter initialization failed.");
    }
    return res;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_Deinit() {
    auto paCtrl = SensorPAController::getInstance();
    pa_result_t res = paCtrl->deinitialize();
    if (res == PA_OK) {
        PA_INFO("Sensor Platform adapter deinitialization done.");
    } else {
        PA_ERROR("Sensor Platform adapter deinitialization failed.");
    }
    return res;
}

void SensorPAController::PASensorClient::onEvent(std::shared_ptr<std::vector<SensorEvent>> events) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (!eventListener_ || !eventListener_->onEvent) {
        PA_ERROR("Unable to find event listener for onEvent for ID %llu", sensorId_);
        return;
    }

    auto paEvents = std::make_shared<std::vector<taf_pa_sensor_Event>>();
    paEvents->reserve(events->size());

    for (const auto& sdkEvent : *events) {
        taf_pa_sensor_Event paEvent;
        paEvent.timestamp = sdkEvent.timestamp;
        if (!isCalibrated_) {
            paEvent.x = sdkEvent.uncalibrated.data.x;
            paEvent.y = sdkEvent.uncalibrated.data.y;
            paEvent.z = sdkEvent.uncalibrated.data.z;
            paEvent.xb = sdkEvent.uncalibrated.bias.x;
            paEvent.yb = sdkEvent.uncalibrated.bias.y;
            paEvent.zb = sdkEvent.uncalibrated.bias.z;
        } else {
            paEvent.x = sdkEvent.calibrated.x;
            paEvent.y = sdkEvent.calibrated.y;
            paEvent.z = sdkEvent.calibrated.z;
            paEvent.xb = 0;
            paEvent.yb = 0;
            paEvent.zb = 0;
        }
        paEvents->push_back(paEvent);
    }

    eventListener_->onEvent(sensorId_, paEvents, eventListenerContext_);
}

void SensorPAController::PASensorClient::onConfigurationUpdate(SensorConfiguration configuration) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (sensorClient_) {
        PA_INFO("%s is configured for :[%f %d %d]", sensorClient_->getSensorInfo().name.c_str(),
                    configuration.samplingRate, configuration.batchCount,
                    static_cast<int>(configuration.isRotated));

        // Call the registered configuration update callback if available
        if (configUpdateCallback_) {
            configUpdateCallback_(sensorId_, configuration.samplingRate,
                                configuration.batchCount, configuration.isRotated,
                                configUpdateContext_);
        }
    } else {
        PA_ERROR("SensorClient null in onConfigurationUpdate");
    }
}

void SensorPAController::PASensorClient::onSelfTestFailed() {
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::high_resolution_clock::now().time_since_epoch())
                             .count();

    if (eventListener_ && eventListener_->onSelfTestFailed) {
        eventListener_->onSelfTestFailed(sensorId_, timestamp, eventListenerContext_);
    } else {
        PA_ERROR("Not able to find event Listener for onSelfTestFailed for ID %llu", sensorId_);
    }
}

pa_result_t SensorPAController::PASensorClient::RegisterEventListener(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_EventListener* listener,
    std::any context) {

    sensorId_ = sensorId;
    // Allow nullptr to clear the listener (used by RemoveListener)
    eventListener_ = listener;

    if (context.has_value())
    {
        eventListenerContext_ = std::move(context);
    }

    return PA_OK;
}

pa_result_t SensorPAController::PASensorClient::RegisterConfigUpdateHandler(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_ConfigUpdateCb callback,
    std::any context) {

    std::lock_guard<std::mutex> lock(mtx_);
    sensorId_ = sensorId;

    if (callback) {
        configUpdateCallback_ = callback;
        if (context.has_value()) {
            configUpdateContext_ = std::move(context);
        }
        PA_INFO("Configuration update handler registered for sensor ID %llu", sensorId_);
        return PA_OK;
    } else {
        PA_ERROR("Configuration update callback is NULL for sensor ID %llu", sensorId_);
        return PA_BAD_PARAMETER;
    }
}

pa_result_t SensorPAController::PASensorClient::UnregisterConfigUpdateHandler(
    taf_pa_sensor_SensorId sensorId) {

    std::lock_guard<std::mutex> lock(mtx_);
    configUpdateCallback_ = nullptr;
    configUpdateContext_ = std::any();
    PA_INFO("Configuration update handler unregistered for sensor ID %llu", sensorId);
    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_AddConfigUpdateHandler(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_ConfigUpdateCb callback,
    std::any context) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    pa_result_t result = clientPtr->RegisterConfigUpdateHandler(sensorId, callback, context);
    if (result == PA_OK) {
        PA_INFO("Add configuration update handler successfully for ID %llu", sensorId);
        return PA_OK;
    }
    PA_ERROR("Failed to add configuration update handler for ID %llu", sensorId);
    return PA_FAULT;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_RemoveConfigUpdateHandler(
    taf_pa_sensor_SensorId sensorId) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    pa_result_t result = clientPtr->UnregisterConfigUpdateHandler(sensorId);
    if (result == PA_OK) {
        PA_INFO("Remove configuration update handler successfully for ID %llu", sensorId);
        return PA_OK;
    }
    PA_ERROR("Failed to remove configuration update handler for ID %llu", sensorId);
    return PA_FAULT;
}

pa_result_t SensorPAController::PASensorClient::RegisterCapabilityHandler(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_CapabilityCb callback,
    std::any context) {

    std::lock_guard<std::mutex> lock(mtx_);
    sensorId_ = sensorId;

    if (callback) {
        capabilityCallback_ = callback;
        if (context.has_value()) {
            capabilityContext_ = std::move(context);
        }
        PA_INFO("Capability handler registered for sensor ID %llu", sensorId_);
        return PA_OK;
    } else {
        PA_ERROR("Capability callback is NULL for sensor ID %llu", sensorId_);
        return PA_BAD_PARAMETER;
    }
}

pa_result_t SensorPAController::PASensorClient::UnregisterCapabilityHandler(
    taf_pa_sensor_SensorId sensorId) {

    std::lock_guard<std::mutex> lock(mtx_);
    capabilityCallback_ = nullptr;
    capabilityContext_ = std::any();
    PA_INFO("Capability handler unregistered for sensor ID %llu", sensorId);
    return PA_OK;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_AddCapabilityHandler(
    taf_pa_sensor_SensorId sensorId,
    taf_pa_sensor_CapabilityCb callback,
    std::any context) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    pa_result_t result = clientPtr->RegisterCapabilityHandler(sensorId, callback, context);
    if (result == PA_OK) {
        PA_INFO("Add capability handler successfully for ID %llu", sensorId);
        return PA_OK;
    }
    PA_ERROR("Failed to add capability handler for ID %llu", sensorId);
    return PA_FAULT;
}

PA_SHARED PA_WEAK pa_result_t tafpa::sensor::taf_pa_sensor_RemoveCapabilityHandler(
    taf_pa_sensor_SensorId sensorId) {

    auto paCtrl = SensorPAController::getInstance();
    std::shared_ptr<SensorPAController::PASensorClient> clientPtr = paCtrl->GetClientPtr(sensorId);
    if (!clientPtr) {
        PA_ERROR("Invalid sensor ID (%llu) provided!", sensorId);
        return PA_BAD_PARAMETER;
    }

    pa_result_t result = clientPtr->UnregisterCapabilityHandler(sensorId);
    if (result == PA_OK) {
        PA_INFO("Remove capability handler successfully for ID %llu", sensorId);
        return PA_OK;
    }
    PA_ERROR("Failed to remove capability handler for ID %llu", sensorId);
    return PA_FAULT;
}
