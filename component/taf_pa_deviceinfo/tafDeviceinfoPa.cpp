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
#include "tafInternalCommonPa.h"

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

        taf_pa_result_t initialize();
        taf_pa_result_t deinitialize();

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
        TAF_PA_INFO("Service Status : UNAVAILABLE");
    } else if (serviceStatus == ServiceStatus::SERVICE_AVAILABLE) {
        TAF_PA_INFO("Service Status : AVAILABLE");
    }
}

taf_pa_result_t DeviceInfoPAController::initialize()
{
    TAF_PA_INFO("initialize!!");
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
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e) {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...) {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };

    deviceInfoManager_ = platformFactory.getDeviceInfoManager(cb);
    if (deviceInfoManager_ == nullptr) {
        TAF_PA_ERROR("Failed to get Device Info Manager instance");
        return TAF_PA_FAULT;
    }

    startTime = std::chrono::system_clock::now();
    ServiceStatus devInfoMgrStatus = deviceInfoManager_->getServiceStatus();
    if(devInfoMgrStatus != ServiceStatus::SERVICE_AVAILABLE) {
        TAF_PA_INFO( "DevInfoManager subsystem is not ready, Please wait");
    }

    auto initFuture = prom->get_future();
    auto waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    //  Exit the application, if SDK is unable to initialize deviceinfo subsystems
    if (waitStatus == std::future_status::timeout) {
        TAF_PA_CRIT("*** ERROR - Timeout to get device info manager ready");
        return TAF_PA_TIMEOUT;
    }
    else{
        devInfoMgrStatus = initFuture.get();
        if (devInfoMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            endTime = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsedTime = endTime - startTime;
            TAF_PA_INFO("Elapsed Time for DevInfoManager Subsystems to ready : %lf",elapsedTime.count());
        } else {
            TAF_PA_CRIT("ERROR - Unable to initialize DevInfoManager subsystem");
            return TAF_PA_FAULT;
        }
    }
    TAF_PA_INFO("Obtained Platform manager!!");
    // Register for Device information service status change
    devinfoServiceStatusListener_ = std::make_shared<DeviceInfoListener>(this);
    telux::common::Status status
        = deviceInfoManager_->registerListener(devinfoServiceStatusListener_);
    if (status != telux::common::Status::SUCCESS) {
        TAF_PA_ERROR("Failed to register for service state change ");
        return TAF_PA_FAULT;
    }

    // Mark initialization as complete
    gDeviceinfoPaInitialized.store(true, std::memory_order_release);
    TAF_PA_INFO("DeviceInfo PA initialization flag set to true.");

    return TAF_PA_OK;
}

taf_pa_result_t DeviceInfoPAController::deinitialize()
{
    // Check if Init() was called before Deinit()
    if (!gDeviceinfoPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Starting DeviceInfo PA deinitialization...");

    // Step 1: Deregister the service status listener from the device info manager
    // before releasing any state, so no further SDK callbacks arrive.
    if (deviceInfoManager_ && devinfoServiceStatusListener_)
    {
        TAF_PA_INFO("Deregistering devinfoServiceStatusListener_ from deviceInfoManager_");
        telux::common::Status status =
            deviceInfoManager_->deregisterListener(devinfoServiceStatusListener_);
        if (status != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("Failed to deregister device info service status listener. Status: %d",
                     static_cast<int>(status));
        }
        devinfoServiceStatusListener_.reset();
    }

    // Step 2: Reset device info manager shared pointer
    TAF_PA_INFO("Resetting deviceInfoManager_");
    deviceInfoManager_.reset();

    // Reset initialization flag
    gDeviceinfoPaInitialized.store(false, std::memory_order_release);
    TAF_PA_INFO("DeviceInfo PA initialization flag reset to false.");

    TAF_PA_INFO("DeviceInfo PA deinitialization complete");
    return TAF_PA_OK;
}

taf_pa_result_t tafpa::deviceinfo::taf_pa_deviceinfo_GetIMEI(char* imeiPtr, size_t numElements)
{
    if (imeiPtr == nullptr || numElements == 0) {
        TAF_PA_ERROR("Invalid parameters: imeiPtr is null or numElements is 0");
        return TAF_PA_BAD_PARAMETER;
    }

    auto pACtrl = DeviceInfoPAController::getInstance();
    auto deviceInfoMgr = pACtrl->getDeviceInfoManager();

    if (deviceInfoMgr == nullptr) {
        TAF_PA_ERROR("Device Info Manager is not initialized");
        return TAF_PA_FAULT;
    }

    std::string imei = "";
    telux::common::Status status = deviceInfoMgr->getIMEI(imei);

    if(status != Status::SUCCESS){
        TAF_PA_ERROR("request for IMEI failed(status = %d)", static_cast<int>(status));
        return TAF_PA_FAULT;
    }

    taf_pa_memscpy(imeiPtr, numElements + 1, imei.c_str(), imei.size() + 1);
    return TAF_PA_OK;
}

taf_pa_result_t tafpa::deviceinfo::taf_pa_deviceinfo_Init()
{
    auto pACtrl = DeviceInfoPAController::getInstance();

    TAF_PA_INFO("taf_pa_deviceinfo_Init!!");

    taf_pa_result_t result = pACtrl->initialize();
    if (result == TAF_PA_OK)
    {
        TAF_PA_INFO("DeviceInfo platform adapter initialization is done");
    }
    else
    {
        TAF_PA_CRIT("Failed to initialize DeviceInfo platform adapter, ret: %d", result);
    }

    return result;
}

taf_pa_result_t tafpa::deviceinfo::taf_pa_deviceinfo_Deinit()
{
    auto pACtrl = DeviceInfoPAController::getInstance();

    TAF_PA_INFO("taf_pa_deviceinfo_Deinit!!");

    taf_pa_result_t result = pACtrl->deinitialize();
    if (result == TAF_PA_OK)
    {
        TAF_PA_INFO("DeviceInfo platform adapter deinitialization is done");
    }
    else
    {
        TAF_PA_ERROR("Failed to deinitialize DeviceInfo platform adapter, ret: %d", result);
    }

    return result;
}
