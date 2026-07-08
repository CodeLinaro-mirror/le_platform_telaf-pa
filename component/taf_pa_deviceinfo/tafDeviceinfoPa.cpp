/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cstring>
#include <string>
#include <future>
#include <atomic>
#include <unistd.h>

#include <telux/platform/PlatformFactory.hpp>
#include "tafCommonPa.h"
#include "tafDeviceinfoPa.hpp"

// Thread-safe initialization flag
static std::atomic<bool> gDeviceinfoPaInitialized(false);

#define MAX_INIT_TIMEOUT 5

using namespace tafpa::deviceinfo;
using namespace telux::common;
using namespace telux::platform;

class DeviceInfoPAController{
public:
    static std::shared_ptr<DeviceInfoPAController> getInstance()
    {
        static std::shared_ptr<DeviceInfoPAController> instance(new DeviceInfoPAController());
        return instance;
    }

    std::shared_ptr<IDeviceInfoManager> getDeviceInfoManager()
    {
        return deviceInfoManager_;
    }

        pa_result_t initialize();
        pa_result_t deinitialize();

    class DeviceInfoListener : public telux::platform::IDeviceInfoListener
    {
        public:
            DeviceInfoListener(DeviceInfoPAController* controller) : controller_(controller) {}

            void onServiceStatusChange(telux::common::ServiceStatus serviceStatus) override;

            ~DeviceInfoListener() = default;
        private:
            DeviceInfoPAController* controller_;
    };

    DeviceInfoPAController() = default;
    ~DeviceInfoPAController() = default;
private:
    DeviceInfoPAController(const DeviceInfoPAController&) = delete;
    DeviceInfoPAController& operator=(const DeviceInfoPAController&) = delete;
    std::shared_ptr<IDeviceInfoManager> deviceInfoManager_ = nullptr;
    std::shared_ptr<DeviceInfoListener> devinfoServiceStatusListener_ = nullptr;
    static std::shared_ptr<DeviceInfoPAController> instance;
};

void DeviceInfoPAController::DeviceInfoListener::onServiceStatusChange(telux::common::ServiceStatus serviceStatus)
{
    if (serviceStatus == ServiceStatus::SERVICE_UNAVAILABLE) {
        PA_INFO("Service Status : UNAVAILABLE");
    } else if (serviceStatus == ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("Service Status : AVAILABLE");
    }
}

pa_result_t DeviceInfoPAController::initialize()
{
    PA_INFO("initialize!!");
    auto& platformFactory = PlatformFactory::getInstance();

    auto prom = std::make_shared<std::promise<ServiceStatus>>();
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;

    auto cb = [prom](ServiceStatus status) {
        try {
            if (status == ServiceStatus::SERVICE_AVAILABLE) {
                prom->set_value(ServiceStatus::SERVICE_AVAILABLE);
            } else {
                prom->set_value(ServiceStatus::SERVICE_UNAVAILABLE);
            }
        }
        catch (const std::future_error& e) {
            PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e) {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...) {
            PA_ERROR("Unknown error in callback.");
        }
    };

    deviceInfoManager_ = platformFactory.getDeviceInfoManager(cb);
    if (deviceInfoManager_ == nullptr) {
        PA_ERROR("Failed to get Device Info Manager instance");
        return PA_FAULT;
    }

    startTime = std::chrono::system_clock::now();
    ServiceStatus devInfoMgrStatus = deviceInfoManager_->getServiceStatus();
    if(devInfoMgrStatus != ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO( "DevInfoManager subsystem is not ready, Please wait");
    }

    auto initFuture = prom->get_future();
    auto waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    //  Exit the application, if SDK is unable to initialize deviceinfo subsystems
    if (waitStatus == std::future_status::timeout) {
        PA_CRIT("*** ERROR - Timeout to get device info manager ready");
        return PA_TIMEOUT;
    }
    else{
        devInfoMgrStatus = initFuture.get();
        if (devInfoMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            endTime = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsedTime = endTime - startTime;
            PA_INFO("Elapsed Time for DevInfoManager Subsystems to ready : %lf",elapsedTime.count());
        } else {
            PA_CRIT("ERROR - Unable to initialize DevInfoManager subsystem");
            return PA_FAULT;
        }
    }
    PA_INFO("Obtained Platform manager!!");
    // Register for Device information service status change
    devinfoServiceStatusListener_ = std::make_shared<DeviceInfoListener>(this);
    telux::common::Status status
        = deviceInfoManager_->registerListener(devinfoServiceStatusListener_);
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("Failed to register for service state change ");
        return PA_FAULT;
    }

    // Mark initialization as complete
    gDeviceinfoPaInitialized.store(true, std::memory_order_release);
    PA_INFO("DeviceInfo PA initialization flag set to true.");

    return PA_OK;
}

pa_result_t DeviceInfoPAController::deinitialize()
{
    // Check if Init() was called before Deinit()
    if (!gDeviceinfoPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return PA_FAULT;
    }

    PA_INFO("Starting DeviceInfo PA deinitialization...");

    // Step 1: Deregister the service status listener from the device info manager
    // before releasing any state, so no further SDK callbacks arrive.
    if (deviceInfoManager_ && devinfoServiceStatusListener_)
    {
        PA_INFO("Deregistering devinfoServiceStatusListener_ from deviceInfoManager_");
        telux::common::Status status =
            deviceInfoManager_->deregisterListener(devinfoServiceStatusListener_);
        if (status != telux::common::Status::SUCCESS)
        {
            PA_ERROR("Failed to deregister device info service status listener. Status: %d",
                     static_cast<int>(status));
        }
        devinfoServiceStatusListener_.reset();
    }

    // Step 2: Reset device info manager shared pointer
    PA_INFO("Resetting deviceInfoManager_");
    deviceInfoManager_.reset();

    // Reset initialization flag
    gDeviceinfoPaInitialized.store(false, std::memory_order_release);
    PA_INFO("DeviceInfo PA initialization flag reset to false.");

    PA_INFO("DeviceInfo PA deinitialization complete");
    return PA_OK;
}

pa_result_t tafpa::deviceinfo::taf_pa_deviceinfo_GetIMEI(char* imeiPtr, size_t numElements)
{
    if (imeiPtr == nullptr || numElements == 0) {
        PA_ERROR("Invalid parameters: imeiPtr is null or numElements is 0");
        return PA_BAD_PARAMETER;
    }

    auto pACtrl = DeviceInfoPAController::getInstance();
    auto deviceInfoMgr = pACtrl->getDeviceInfoManager();

    if (deviceInfoMgr == nullptr) {
        PA_ERROR("Device Info Manager is not initialized");
        return PA_FAULT;
    }

    std::string imei = "";
    telux::common::Status status = deviceInfoMgr->getIMEI(imei);

    if(status != Status::SUCCESS){
        PA_ERROR("request for IMEI failed(status = %d)", static_cast<int>(status));
        return PA_FAULT;
    }

    std::memcpy(imeiPtr, imei.c_str(), numElements + 1);
    return PA_OK;
}

pa_result_t tafpa::deviceinfo::taf_pa_deviceinfo_Init()
{
    auto pACtrl = DeviceInfoPAController::getInstance();

    PA_INFO("taf_pa_deviceinfo_Init!!");

    pa_result_t result = pACtrl->initialize();
    if (result == PA_OK)
    {
        PA_INFO("DeviceInfo platform adapter initialization is done");
    }
    else
    {
        PA_CRIT("Failed to initialize DeviceInfo platform adapter, ret: %d", result);
    }

    return result;
}

pa_result_t tafpa::deviceinfo::taf_pa_deviceinfo_Deinit()
{
    auto pACtrl = DeviceInfoPAController::getInstance();

    PA_INFO("taf_pa_deviceinfo_Deinit!!");

    pa_result_t result = pACtrl->deinitialize();
    if (result == PA_OK)
    {
        PA_INFO("DeviceInfo platform adapter deinitialization is done");
    }
    else
    {
        PA_ERROR("Failed to deinitialize DeviceInfo platform adapter, ret: %d", result);
    }

    return result;
}
