/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "legato.h"
#include "interfaces.h"
#include <chrono>
#include <mutex>
#include <future>
#include <memory>
#include <time.h>
#include "telux/sensor/SensorManager.hpp"
#include "telux/common/CommonDefines.hpp"
#include "telux/sensor/SensorDefines.hpp"
#include "telux/sensor/SensorClient.hpp"
#include "telux/sensor/SensorFactory.hpp"
#include "taf_pa_sensor.hpp"
#include "tafSvcIF.hpp"

#define MAX_INIT_TIMEOUT 5
#define SEC_TO_NANOS 1000000000

using namespace telux::sensor;
using namespace telux::common;

class SensorPAController{
public:
    static std::shared_ptr<SensorPAController> getInstance()
    {
        static std::shared_ptr<SensorPAController> instance(new SensorPAController());
        return instance;
    }

    std::shared_ptr<telux::sensor::ISensorManager> getSensorManager()
    {
        return sensorManager_;
    }

    size_t getSensorListSize(){
        return sList.size();
    }

    telux::sensor::SensorInfo getSensorInfo(int index){
        return sList[index];
    }

    le_result_t initialize();
    le_result_t MapStatus(telux::common::Status status);
    le_result_t MapErrorCode(telux::common::ErrorCode errorCode);

    class PASensorClient : public telux::sensor::ISensorEventListener,
        public std::enable_shared_from_this<PASensorClient> {
    public:
        PASensorClient(
            std::shared_ptr<telux::sensor::ISensorClient> sensorClient_,bool isCalibrated_)
            : sensorClient_(sensorClient_),isCalibrated_(isCalibrated_),isActivated_(false){}
        void onEvent(std::shared_ptr<std::vector<SensorEvent>> events) override;
        void onConfigurationUpdate(SensorConfiguration configuration) override;
        void onSelfTestFailed();
        telux::common::Status Activate();
        telux::common::Status Deactivate();
        telux::common::Status Configure(SensorConfiguration config);
        telux::common::Status SelfTest(taf_pa_sensor_Ref_t ref,
            SelfTestType type,taf_pa_sensor_SelfTestResultCb callback,void* contextPtr);
        void Init();
        void CleanUp();

        le_result_t RegisterEventListener(taf_pa_sensor_Ref_t reference,
            taf_pa_sensor_EventListener* listener,void *contextPtr)
        {
            reference_ = reference;
            if (listener != nullptr)
            {
                eventListener_ = listener;
            }
            else
            {
                LE_ERROR("Listener is NULL");
                return LE_NOT_FOUND;
            }

            if (contextPtr != nullptr)
            {
                eventListenerContext = contextPtr;
            }

            return LE_OK;
        }

        bool isActivated(){
            return isActivated_;
        }

    private:
        std::shared_ptr<telux::sensor::ISensorClient> sensorClient_;
        bool isCalibrated_;
        bool isActivated_ ;
        std::mutex mtx_ ;
        taf_pa_sensor_EventListener* eventListener_ ;
        taf_pa_sensor_Ref_t reference_;
        void* eventListenerContext;
    };

    struct taf_pa_sensor_client_t
    {
        std::shared_ptr<SensorPAController::PASensorClient> paSensorClient;
        taf_pa_sensor_Ref_t reference;
    };

    taf_pa_sensor_client_t* GetClientPtr(taf_pa_sensor_Ref_t ref);
    taf_pa_sensor_Ref_t CreateReference(const char* sensorName);
    le_result_t DeleteReference(taf_pa_sensor_Ref_t ref);
    SensorPAController() = default;
    ~SensorPAController() = default;
private:
    SensorPAController(const SensorPAController&) = delete;
    SensorPAController& operator=(const SensorPAController&) = delete;
    std::shared_ptr<telux::sensor::ISensorManager> sensorManager_;
    std::vector<telux::sensor::SensorInfo> sList;
    static std::shared_ptr<SensorPAController> instance;
    le_mem_PoolRef_t sensorClientPool_ = nullptr;
    le_ref_MapRef_t sensorClientRefMap_ = nullptr;
};



LE_MEM_DEFINE_STATIC_POOL(taf_pa_sensor_Client_pool, 20,
    sizeof(SensorPAController::taf_pa_sensor_client_t));

le_result_t SensorPAController::initialize()
{
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;
    startTime = std::chrono::system_clock::now();
    std::promise<telux::common::ServiceStatus> prom;
    //  Get the SensorFactory and SensorManager instances.
    auto &sensorFactory = telux::sensor::SensorFactory::getInstance();
    sensorManager_ = sensorFactory.getSensorManager(
        [&prom](telux::common::ServiceStatus status) { prom.set_value(status); });

    if (!sensorManager_) {
        LE_CRIT("Failed to get SensorManager");
        return LE_FAULT;
    }

    //  Check if sensor subsystem is ready
    //  If sensor subsystem is not ready, wait for it to be ready
    telux::common::ServiceStatus managerStatus = sensorManager_->getServiceStatus();
    if (managerStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        LE_INFO("Sensor subsystem is not ready, Please wait ...");
    }

    auto initFuture = prom.get_future();
    auto waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    //  Exit the application, if SDK is unable to initialize sensor subsystems
    if (waitStatus == std::future_status::timeout) {
        LE_CRIT("*** ERROR - Timeout to get sensor manager ready");
        return LE_TIMEOUT;
    }
    else{
        managerStatus = initFuture.get();
        if (managerStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            endTime = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsedTime = endTime - startTime;
            LE_INFO("Elapsed Time for Sensor Subsystems to ready : %lf",elapsedTime.count());
        } else {
            LE_CRIT("ERROR - Unable to initialize sensor subsystem");
            return LE_FAULT;
        }
    }

    if(sensorManager_){
        telux::common::Status status = sensorManager_->getAvailableSensorInfo(sList);
        if(status != telux::common::Status::SUCCESS){
            LE_ERROR("Unable to get available sensors list");
        }
    }
    sensorClientPool_ = le_mem_InitStaticPool(taf_pa_sensor_Client_pool, 20, sizeof(taf_pa_sensor_client_t));
    sensorClientRefMap_ = le_ref_CreateMap("taf_pa_sensor_client_map", 20);
    return LE_OK;
}

// register sensor for notifications
void SensorPAController::PASensorClient::Init(){
    sensorClient_->registerListener(shared_from_this());
}

// deregister sensor for notifications
void SensorPAController::PASensorClient::CleanUp(){
    sensorClient_->deregisterListener(shared_from_this());
}

le_result_t SensorPAController::MapStatus(telux::common::Status status){
    switch(status)
    {
        case telux::common::Status::SUCCESS:
            LE_INFO("Operation processed successfully");
            return LE_OK;
        case telux::common::Status::FAILED:
            LE_INFO("Operation processing failed");
            return LE_FAULT;
        case telux::common::Status::INVALIDPARAM:
            LE_INFO("Input parameters are invalid");
            return LE_BAD_PARAMETER;
        case telux::common::Status::NOTALLOWED:
            LE_INFO("Operation not allowed");
            return LE_NOT_PERMITTED;
        case telux::common::Status::NOTIMPLEMENTED:
            LE_INFO("Feature not supported");
            return LE_NOT_IMPLEMENTED ;
        case telux::common::Status::CONNECTIONLOST:
            LE_INFO("Connection to Socket server lost");
            return LE_NOT_FOUND;
        case telux::common::Status::EXPIRED:
            LE_INFO("Operation has expired");
            return LE_TERMINATED;
        case telux::common::Status::NOTSUPPORTED:
            LE_INFO("Not supported on target platform");
            return LE_UNSUPPORTED;
        default:
           return LE_FAULT;
    }
}

le_result_t SensorPAController::MapErrorCode(telux::common::ErrorCode errorCode)
{
    switch(errorCode)
    {
        case telux::common::ErrorCode::SUCCESS:
            LE_INFO("Operation processed successfully");
            return LE_OK;
        case telux::common::ErrorCode::GENERIC_FAILURE:
            LE_ERROR("Operation processing failed");
            return LE_FAULT;
        case telux::common::ErrorCode::INVALID_ARGUMENTS:
            LE_ERROR("Input parameters are invalid");
            return LE_BAD_PARAMETER;
        case telux::common::ErrorCode::OPERATION_NOT_ALLOWED:
            LE_ERROR("Operation not allowed");
            return LE_NOT_PERMITTED;
        case telux::common::ErrorCode::TIMEOUT_ERROR:
            LE_ERROR("TimeOut Error");
            return LE_TIMEOUT;
        case telux::common::ErrorCode::INFO_UNAVAILABLE:
            LE_ERROR("Information not available");
            return LE_UNAVAILABLE;
        case telux::common::ErrorCode::SUBSYSTEM_UNAVAILABLE:
            LE_ERROR("Subsystem Not Available");
            return LE_UNAVAILABLE;
        case telux::common::ErrorCode::REQUEST_NOT_SUPPORTED:
            LE_ERROR("Request Not supported");
            return LE_UNSUPPORTED;
        default:
           return LE_FAULT;
    }
}

//Create a reference for sensor
taf_pa_sensor_Ref_t SensorPAController::CreateReference(const char* sensorName)
{
    auto paCtrl = SensorPAController::getInstance();
    taf_pa_sensor_client_t *clientInfo = nullptr;
    std::shared_ptr<telux::sensor::ISensorClient> sdkSensorClient;
    auto sensorMngr = paCtrl->getSensorManager();
    telux::common::Status status = sensorMngr->getSensorClient(sdkSensorClient,sensorName);
    if(status != telux::common::Status::SUCCESS){
        LE_ERROR("Unable to create client for given sensor %s",sensorName);
        return NULL;
    }
    SensorType type  = sdkSensorClient->getSensorInfo().type;
    bool isCalibrated = true;
    if(type == telux::sensor::SensorType::GYROSCOPE_UNCALIBRATED ||
        type == telux::sensor::SensorType::ACCELEROMETER_UNCALIBRATED){
        isCalibrated = false;
    }
    clientInfo =  (taf_pa_sensor_client_t*)le_mem_ForceAlloc(paCtrl->sensorClientPool_);
    clientInfo->paSensorClient =
        std::make_shared<SensorPAController::PASensorClient>(sdkSensorClient,isCalibrated);
    clientInfo->reference =
        (taf_pa_sensor_Ref_t)le_ref_CreateRef(paCtrl->sensorClientRefMap_ , (void*)clientInfo);
    clientInfo->paSensorClient->Init();
    return clientInfo->reference;
}

le_result_t SensorPAController::DeleteReference(taf_pa_sensor_Ref_t reference)
{
    LE_DEBUG("Delete Reference %p",reference);
    TAF_ERROR_IF_RET_VAL(reference == NULL,LE_BAD_PARAMETER,"Null reference");
    taf_pa_sensor_client_t* ptr =
        (taf_pa_sensor_client_t*)le_ref_Lookup(sensorClientRefMap_ , reference);
    TAF_ERROR_IF_RET_VAL(ptr == NULL, LE_BAD_PARAMETER, "Invalid para(null reference ptr)");
    if(ptr->paSensorClient){
        ptr->paSensorClient->CleanUp();
    }
    le_ref_DeleteRef(sensorClientRefMap_,reference);
    le_mem_Release(ptr);
    return LE_OK;
}

SensorPAController::taf_pa_sensor_client_t*
    SensorPAController::GetClientPtr(taf_pa_sensor_Ref_t ref)
{
    taf_pa_sensor_client_t* clientPtr =
        (taf_pa_sensor_client_t*)le_ref_Lookup(sensorClientRefMap_ , ref);
    return clientPtr;
}

telux::common::Status SensorPAController::PASensorClient::Activate(){
    telux::common::Status status = sensorClient_->activate();
    if (status == telux::common::Status::SUCCESS) {
        isActivated_ = true;
    }
    return status;
}

telux::common::Status SensorPAController::PASensorClient::Deactivate(){
    telux::common::Status status = sensorClient_->deactivate();
    if (status == telux::common::Status::SUCCESS) {
        isActivated_ = false;
    }
    return status;
}

telux::common::Status SensorPAController::PASensorClient::Configure(SensorConfiguration config)
{
    telux::common::Status status = sensorClient_->configure(config);
    return status;
}

telux::common::Status SensorPAController::PASensorClient::SelfTest(taf_pa_sensor_Ref_t ref,
    SelfTestType type,taf_pa_sensor_SelfTestResultCb callback,void* contextPtr){
    auto promisePtr = std::make_shared<std::promise<le_result_t>>();
    auto paCtrl = SensorPAController::getInstance();
    auto timestamp = std::make_shared<uint64_t>(0);
    //Sdk Callback
    auto cb1 = [promisePtr,timestamp,&paCtrl](telux::common::ErrorCode error,
        SelfTestResultParams selfTestResultParams) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                *timestamp = selfTestResultParams.timestamp_;
                if(selfTestResultParams.sensorResultType_ == SensorResultType::CURRENT){
                    promisePtr->set_value(LE_OK);
                }
                else{
                    promisePtr->set_value(LE_BUSY);
                }
            }
            else{
                le_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            LE_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            LE_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            LE_ERROR("Unknown error in self test callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  sensorClient_->selfTest(type,cb1);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            le_result_t selfResult = futResult.get();
            if(callback){
                callback(ref,selfResult,*timestamp,contextPtr);
            }
        }else{
            if(callback){
                callback(ref,LE_TIMEOUT,0,contextPtr);
            }
            LE_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

//Create a reference for sensor
taf_pa_sensor_Ref_t taf_pa_sensor_CreateReference(const char* sensorName)
{
    auto paCtrl = SensorPAController::getInstance();
    return paCtrl->CreateReference(sensorName);
}

//Return the sensor Information
le_result_t taf_pa_sensor_GetSensorInfo(int8_t index,taf_pa_sensor_basicInfo_t& basicInfo,
    taf_pa_sensor_configInfo_t& configInfo,taf_pa_sensor_capabilities_t& Capabilities)
{
    auto paCtrl = SensorPAController::getInstance();
    if(index >= static_cast<int8_t>(paCtrl->getSensorListSize())){
        return LE_OUT_OF_RANGE;
    }
    telux::sensor::SensorInfo sInfo = paCtrl->getSensorInfo(index);
    basicInfo.id = sInfo.id;
    basicInfo.version = sInfo.version;
    snprintf(basicInfo.sensorName,sizeof(basicInfo.sensorName),
        "%s",sInfo.name.c_str());
    snprintf(basicInfo.vendorName,sizeof(basicInfo.vendorName),
        "%s",sInfo.vendor.c_str());
    // Add bounds checking for sampling rates
    size_t maxRates = sizeof(configInfo.samplingRate) / sizeof(configInfo.samplingRate[0]);
    size_t ratesToCopy = std::min(sInfo.samplingRates.size(), maxRates);
    for(size_t j=0; j < ratesToCopy; j++){
        configInfo.samplingRate[j] = sInfo.samplingRates[j];
    }
    configInfo.sampleRateListSize = ratesToCopy;
    configInfo.maxSamplingRate = sInfo.maxSamplingRate;
    configInfo.minBatchCount = sInfo.minBatchCountSupported;
    configInfo.maxBatchCount = sInfo.maxBatchCountSupported;
    Capabilities.range = sInfo.range;
    Capabilities.resolution = sInfo.resolution;
    Capabilities.maxRange = sInfo.maxRange;
    telux::sensor::SensorType type = sInfo.type;
    if(type == telux::sensor::SensorType::ACCELEROMETER ||
        type == telux::sensor::SensorType::ACCELEROMETER_UNCALIBRATED){
        basicInfo.sensorType = taf_pa_sensor_type_t::TAF_PA_SENSOR_ACCELEROMETER;
    } else if(type == telux::sensor::SensorType::GYROSCOPE ||
        type == telux::sensor::SensorType::GYROSCOPE_UNCALIBRATED) {
        basicInfo.sensorType = taf_pa_sensor_type_t::TAF_PA_SENSOR_GYROSCOPE;
    } else {
        basicInfo.sensorType = taf_pa_sensor_type_t::TAF_PA_SENSOR_INVALID;
    }
    return LE_OK;
}

//Set the mounting angle of IMU
le_result_t taf_pa_sensor_SetEulerAngle(taf_pa_sensor_Ref_t reference,
    double pitch,double roll, double yaw){
    auto paCtrl = SensorPAController::getInstance();
    auto sensorMngr =  paCtrl->getSensorManager();
    Status status = telux::common::Status::FAILED;
    EulerAngleConfig eulerAngleConfig;
    eulerAngleConfig.pitch = pitch;
    eulerAngleConfig.roll = roll;
    eulerAngleConfig.yaw = yaw;
    status =  sensorMngr->setEulerAngleConfig(eulerAngleConfig);
    if(status != telux::common::Status::SUCCESS){
        LE_ERROR("Not able to set euler angle with status code %d",static_cast<int>(status));
        return LE_FAULT;
    }
    return LE_OK;
}

//Activate the sensor for given configuration
le_result_t taf_pa_sensor_Activate(taf_pa_sensor_Ref_t reference,double sampleRate,
    uint32_t batchCount,bool isRotated)
{
    auto paCtrl = SensorPAController::getInstance();
    SensorPAController::taf_pa_sensor_client_t* clientPtr = paCtrl->GetClientPtr(reference);
    TAF_ERROR_IF_RET_VAL(clientPtr == NULL, LE_FAULT,
        "Invalid reference (%p) provided!",clientPtr);
    TAF_ERROR_IF_RET_VAL(clientPtr->paSensorClient == NULL, LE_FAULT,
        "Invalid reference for sensor Client");
    if(clientPtr->paSensorClient->isActivated()){
        LE_ERROR("Sensor Already activated!!");
        return LE_UNAVAILABLE;
    }
    SensorConfiguration s;
    s.samplingRate = sampleRate;
    s.batchCount = batchCount;
    s.isRotated = isRotated;
    s.validityMask.set(SensorConfigParams::SAMPLING_RATE);
    s.validityMask.set(SensorConfigParams::BATCH_COUNT);
    s.validityMask.set(SensorConfigParams::ROTATE);
    telux::common::Status status = clientPtr->paSensorClient->Configure(s);
    if(status != telux::common::Status::SUCCESS){
        LE_DEBUG("Sensor Configuration failed with status code %d",static_cast<int>(status));
        return LE_FAULT;
    }
    LE_INFO("Sensor Configuration done for %p",clientPtr);
    le_thread_Sleep(1);
    status = clientPtr->paSensorClient->Activate();
    if(status != telux::common::Status::SUCCESS){
        LE_DEBUG("Sensor activation failed with status code %d",static_cast<int>(status));
        return LE_FAULT;
    }
    LE_INFO("Sensor Activation done for %p",clientPtr);
    return LE_OK;
}

//Deactivate the sensor
le_result_t taf_pa_sensor_Deactivate(taf_pa_sensor_Ref_t reference)
{
    auto paCtrl = SensorPAController::getInstance();
    SensorPAController::taf_pa_sensor_client_t* clientPtr = paCtrl->GetClientPtr(reference);
    TAF_ERROR_IF_RET_VAL(clientPtr == NULL, LE_FAULT,
        "Invalid reference (%p) provided!",clientPtr);
    TAF_ERROR_IF_RET_VAL(clientPtr->paSensorClient == NULL, LE_FAULT,
        "Invalid reference for sensor Client");

    if(!clientPtr->paSensorClient->isActivated()){
        LE_ERROR("Sensor Already deactivated!!");
        return LE_UNAVAILABLE;
    }
    telux::common::Status status = clientPtr->paSensorClient->Deactivate();
    if(status != telux::common::Status::SUCCESS){
        LE_DEBUG("Sensor Deactivation failed with status code %d",static_cast<int>(status));
        return LE_FAULT;
    }
    LE_INFO("Sensor Deactivation done for %p",clientPtr);
    return LE_OK;
}

//Perform Self Test for given Sensor Ref
le_result_t taf_pa_sensor_SelfTest(taf_pa_sensor_Ref_t reference,taf_pa_sensor_testmode_t mode,
    taf_pa_sensor_SelfTestResultCb callback,void* contextPtr)
{
    auto paCtrl = SensorPAController::getInstance();
    SensorPAController::taf_pa_sensor_client_t* clientPtr = paCtrl->GetClientPtr(reference);
    TAF_ERROR_IF_RET_VAL(clientPtr == NULL, LE_FAULT,
        "Invalid reference (%p) provided!",clientPtr);
    TAF_ERROR_IF_RET_VAL(clientPtr->paSensorClient == NULL, LE_FAULT,
        "Invalid reference for sensor Client");
    SelfTestType type;
    if(mode == TAF_PA_SENSOR_POSITIVE){
        type  = SelfTestType::POSITIVE;
    }
    else if(mode == TAF_PA_SENSOR_NEGATIVE){
        type  = SelfTestType::NEGATIVE;
    }
    else{
        type = SelfTestType::ALL;
    }
    telux::common::Status status =
        clientPtr->paSensorClient->SelfTest(reference,type,callback,contextPtr);
    return paCtrl->MapStatus(status);
}

//Registered the Event Listener
le_result_t taf_pa_sensor_RegisterListener(taf_pa_sensor_Ref_t reference,
    taf_pa_sensor_EventListener* eventListener,void *contextPtr)
{
    auto paCtrl = SensorPAController::getInstance();
    SensorPAController::taf_pa_sensor_client_t* clientPtr = paCtrl->GetClientPtr(reference);
    TAF_ERROR_IF_RET_VAL(clientPtr == NULL, LE_FAULT,
        "Invalid reference (%p) provided!",clientPtr);
    TAF_ERROR_IF_RET_VAL(clientPtr->paSensorClient == NULL, LE_FAULT,
        "Invalid reference for sensor Client");
    if(clientPtr->paSensorClient->RegisterEventListener(reference,eventListener,contextPtr) == LE_OK){
        LE_INFO("Register Event Listenr successfully");
        return LE_OK;
    }
    LE_ERROR("Failed to register event listener");
    return LE_FAULT;
}

//Report Sensor Event
void SensorPAController::PASensorClient::onEvent(std::shared_ptr<std::vector<SensorEvent>> events)
{
    std::lock_guard<std::mutex> lock(mtx_);
    int n = events->size();
    taf_pa_sensor_event_t* paEvent = new taf_pa_sensor_event_t[n];
    for(int i=0;i<n;i++){
        paEvent[i].timestamp = (*events)[i].timestamp;
        if(!isCalibrated_){
            paEvent[i].x = (*events)[i].uncalibrated.data.x;
            paEvent[i].y = (*events)[i].uncalibrated.data.y;
            paEvent[i].z = (*events)[i].uncalibrated.data.z;
            paEvent[i].xb = (*events)[i].uncalibrated.bias.x;
            paEvent[i].yb = (*events)[i].uncalibrated.bias.y;
            paEvent[i].zb = (*events)[i].uncalibrated.bias.z;
        }
        else{
            paEvent[i].x = (*events)[i].calibrated.x;
            paEvent[i].y = (*events)[i].calibrated.y;
            paEvent[i].z = (*events)[i].calibrated.z;
            paEvent[i].xb = 0;
            paEvent[i].yb = 0;
            paEvent[i].zb = 0;
        }
    }

    if(eventListener_ && eventListener_->onEvent){
        eventListener_->onEvent(reference_,paEvent,n,eventListenerContext);
    }
    else{
        LE_ERROR("unable to find event Listener for onEvent");
    }
}

//Report configuration for which sensor is configured.
void SensorPAController::PASensorClient::onConfigurationUpdate(SensorConfiguration configuration)
{
    if(sensorClient_){
        LE_INFO("%s is configured for :[%f %d %d]",sensorClient_->getSensorInfo().name.c_str(),
        configuration.samplingRate,configuration.batchCount,
        static_cast<int>(configuration.isRotated));
    }
    else{
        LE_ERROR("SensorClient null");
    }
}

//Report if voluntary self test failed on suspend
void SensorPAController::PASensorClient::onSelfTestFailed()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t timestamp = (uint64_t)ts.tv_sec * SEC_TO_NANOS + (uint64_t)ts.tv_nsec;
    if(eventListener_ && eventListener_->onSelfTestFailed){
        eventListener_->onSelfTestFailed(reference_,timestamp,eventListenerContext);
    }
    else{
        LE_ERROR("Not able to find event Listener for onSelfTestFailed");
    }
}

le_result_t taf_pa_sensor_DeleteReference(taf_pa_sensor_Ref_t reference)
{
    auto paCtrl = SensorPAController::getInstance();
    le_result_t res = paCtrl->DeleteReference(reference);
    if(res == LE_OK){
        LE_INFO("Reference deleted.");
    }
    else{
        LE_ERROR("Failed to delete reference");
    }
    return res;
}

//Initialize the sensor subsystem
le_result_t taf_pa_sensor_Init(int8_t& listSize)
{
    auto paCtrl = SensorPAController::getInstance();
    le_result_t res = paCtrl->initialize();
    if(res == LE_OK){
        LE_INFO("Sensor Platform adapter intialization done.");
        listSize = static_cast<int8_t>(paCtrl->getSensorListSize());
    }
    else{
        LE_CRIT("Sensor Platform adapter intialization failed.");
    }
    return res;
}

COMPONENT_INIT{
    LE_INFO("Sensor platform adaptor component_init done");
}