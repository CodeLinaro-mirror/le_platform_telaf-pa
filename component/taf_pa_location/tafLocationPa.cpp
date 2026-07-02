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
#include <unistd.h>
#include <atomic>

#include <telux/loc/LocationConfigurator.hpp>
#include <telux/loc/LocationDefines.hpp>
#include <telux/loc/DgnssManager.hpp>
#include "telux/common/CommonDefines.hpp"
#include <telux/loc/LocationManager.hpp>
#include <telux/loc/LocationListener.hpp>
#include "telux/loc/LocationFactory.hpp"

#include "tafLocationPa.hpp"
#include "tafCommonPa.h"

#define MAX_INIT_TIMEOUT 5
#define CLEANUP_TIMEOUT_SECONDS 5
#define SEC_TO_NANOS 1000000000

using namespace telux::loc;
using namespace telux::common;
using namespace tafpa::location;

// Thread-safe initialization flag
static std::atomic<bool> gLocationPaInitialized(false);

class ReusableIdGenerator {
public:
    static constexpr size_t MAX_LOCATION_IDS = 64;
    static constexpr taf_pa_location_LocationId INVALID_LOCATION_ID = 0;

    ReusableIdGenerator() : idMask_() {
    }

    taf_pa_location_LocationId acquireId() {
        for (size_t i = 0; i < MAX_LOCATION_IDS; ++i) {
            if (!idMask_.test(i)) {
                idMask_.set(i);
                return static_cast<taf_pa_location_LocationId>(i + 1);
            }
        }
        TAF_PA_ERROR("No free location IDs available. Maximum %zu IDs reached.", MAX_LOCATION_IDS);
        return INVALID_LOCATION_ID;
    }

    void releaseId(taf_pa_location_LocationId id) {
        if (id == INVALID_LOCATION_ID || id > MAX_LOCATION_IDS) {
            TAF_PA_ERROR("Attempted to release invalid location ID: %llu", id);
            return;
        }
        size_t index = static_cast<size_t>(id - 1);
        if (!idMask_.test(index)) {
            TAF_PA_INFO("Location ID %llu was already released or not in use.", id);
        }
        idMask_.reset(index);
        TAF_PA_INFO("Location ID %llu released.", id);
    }

private:
    std::mutex mtx_;
    std::bitset<MAX_LOCATION_IDS> idMask_;
};

class LocationPAController:public telux::loc::IDgnssStatusListener,public std::enable_shared_from_this<LocationPAController>{
public:
    static std::shared_ptr<LocationPAController> getInstance()
    {
        static std::shared_ptr<LocationPAController> instance(new LocationPAController());
        return instance;
    }

    taf_pa_result_t initialize();
    taf_pa_result_t deinitialize();
    telux::common::Status RegisterDgnssManager();
    telux::common::Status InitializeDgnss(taf_pa_location_DgnssDataFormat_t dataFormat,taf_pa_location_GeneralCb callback,std::any context);
    telux::common::Status DeInitializeDgnss(taf_pa_location_GeneralCb callback,std::any context);
    taf_pa_result_t MapStatus(telux::common::Status status);
    taf_pa_result_t MapErrorCode(telux::common::ErrorCode errorCode);

    LocationPAController() = default;

    ~LocationPAController()
    {
        TAF_PA_INFO("~LocationPAController - cleaning up");

        // Lock mutex to prevent callback execution during cleanup
        std::lock_guard<std::mutex> lock(mutex_);

        // CRITICAL: Deregister BEFORE destruction completes
        if (mDgnssManager)
        {
            TAF_PA_INFO("Deregistering DGNSS listener in destructor mDgnssManager: %p",mDgnssManager);

            // Clear listener first to prevent callbacks
            dgnssListener_ = nullptr;

           // Deregister synchronously without callbacks
            auto status = mDgnssManager->deRegisterListener();
            if (status == telux::common::Status::SUCCESS)
            {
                TAF_PA_INFO("Successfully deregistered Dgnss manager in destructor");
            }
            else
            {
                TAF_PA_ERROR("Failed to deregister Dgnss in destructor: %d", (int)status);
            }

            // Clear the pointer
            mDgnssManager.reset();
        }
    }
    void onDgnssStatusUpdate(DgnssStatus status) override;
    taf_pa_location_DgnssDataStatus_t convertTeluxToDgnssDataStatus(DgnssStatus dgnssStatus);

    class PALocationClient : public telux::loc::ILocationListener,
        public telux::loc::ILocationSystemInfoListener,
        public telux::loc::ILocationConfigListener,
        public std::enable_shared_from_this<PALocationClient> {
    public:
        PALocationClient(
            std::shared_ptr<telux::loc::ILocationManager> locationManager_)
            : locationManager_(locationManager_){};

        std::shared_ptr<telux::loc::ILocationManager> getLocationManager()
        {
            return locationManager_;
        }

        void onGnssSVInfo(const std::shared_ptr<telux::loc::IGnssSVInfo> &gnssSVInfo) override;
        void onGnssSignalInfo(const std::shared_ptr<telux::loc::IGnssSignalInfo> &gnssDatainfo) override;
        void onGnssNmeaInfo(uint64_t timestamp, const std::string &nmea) override;
        void onDetailedEngineLocationUpdate(const std::vector<std::shared_ptr<telux::loc::ILocationInfoEx>>
                     &locationEngineInfo) override;
        void onGnssMeasurementsInfo(const telux::loc::GnssMeasurements &measurementInfo) override;
        void onLocationSystemInfo(const telux::loc::LocationSystemInfo &locationSystemInfo) override;
        void onCapabilitiesInfo(const telux::loc::LocCapability capabilityInfo) override;
        void onXtraStatusUpdate(const telux::loc::XtraStatus xtraStatus) override;

        void Init();
        void CleanUp();
        taf_pa_location_NavigationSolutionType_t convertTeluxToNavigationSolutionType(telux::loc::NavigationSolution naviSolution);

        telux::common::Status StartDetailedEngineReports(uint32_t optInterval, uint16_t engineType, taf_pa_location_GeneralCb callback, uint32_t reportMask, std::any context);
        telux::common::Status StopReports(taf_pa_location_GeneralCb callback, std::any context);
        uint32_t GetCapabilities();

        taf_pa_result_t RegisterEventListener(
            taf_pa_location_LocationId locationId,
            taf_pa_location_EventListener* listener,
            std::any context);

    private:
        std::mutex mtx_;
        std::shared_ptr<telux::loc::ILocationManager> locationManager_ = nullptr;
        taf_pa_location_EventListener* eventListener_ = nullptr ;
        taf_pa_location_LocationId locationId_;
        std::any eventListenerContext_;
    };

    telux::common::Status ConfigureConstellations(std::vector<telux::loc::SvBlackListInfo> SvBlackList, taf_pa_location_GeneralCb callback, bool deviceReset, std::any context);
    telux::common::Status DeleteAidingData(telux::loc::AidingData AidingData, taf_pa_location_GeneralCb callback,std::any context);
    telux::common::Status DeleteAllAidingData(taf_pa_location_GeneralCb callback,std::any context);
    telux::common::Status ConfigureMinSVElevation(uint8_t minElevation, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status RequestMinSVElevation(taf_pa_location_RequestMinSVElevationCb callback, std::any context);
    telux::common::Status ConfigureNmeaTypes(telux::loc::NmeaSentenceConfig  nmeaType, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status ConfigureDR(telux::loc::DREngineConfiguration drConfig, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status ConfigureEngineState(telux::loc::EngineType engineType, telux::loc::LocationEngineRunState engineState, taf_pa_location_GeneralCb callback, std::any context);
	telux::common::Status ConfigureRobustLocation(bool enableRobustloc, bool enableE911loc, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status RequestRobustLocation(taf_pa_location_RequestRobustLocationCb callback, std::any context);
    telux::common::Status ConfigureSecondaryBand(telux::loc::ConstellationSet constellationSet, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status RequestSecondaryBandConfig(taf_pa_location_RequestSecondaryBandConfigCb callback, std::any context);
    telux::common::Status ConfigureLeverArm(LeverArmConfigInfo configInfo , taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status ConfigureMinGpsWeek(uint16_t minGpsWeek, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status ConfigureNmea(telux::loc::NmeaConfig nmeaConfig, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status RequestMinGpsWeek(taf_pa_location_RequestMinGpsWeekCb callback, std::any context);
    telux::common::Status RequestXtraStatus(taf_pa_location_RequestXtraStatusCb callback, std::any context);
    telux::common::Status InjectMerkleTreeInformation(const std::string merkleTreeInfo, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status ConfigureOsnma(bool enableOsnma, taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status ConfigureEngineIntegrityRisk(telux::loc::EngineType engineType,uint32_t integrityRisk,taf_pa_location_GeneralCb callback, std::any context);
    telux::common::Status InjectCorrectionData(const uint8_t*injectionData,
    uint32_t injectionDataSize, taf_pa_location_GeneralCb callback,std::any context);
    telux::common::Status CreateDgnssSource(telux::loc::DgnssDataFormat dgnssFormat,
    taf_pa_location_GeneralCb callback,std::any context);
    telux::common::Status ReleaseDgnssSource(taf_pa_location_GeneralCb callback,std::any context);
    std::shared_ptr<PALocationClient> GetClientPtr(taf_pa_location_LocationId id);
    taf_pa_location_LocationId CreateLocationClient();
    taf_pa_result_t ReleaseLocationClient(taf_pa_location_LocationId id);
    std::shared_ptr<telux::loc::IDgnssManager> getDgnssManager()
    {
        return mDgnssManager;
    }
    taf_pa_result_t RegisterDgnssEventListener(
        taf_pa_location_DgnssEventListener* listener,
        std::any context);
    taf_pa_result_t DeregisterDgnssEventListener(std::any context);
private:
    LocationPAController(const LocationPAController&) = delete;
    LocationPAController& operator=(const LocationPAController&) = delete;
    std::shared_ptr<telux::loc::ILocationConfigurator> mLocationConfigurator = nullptr;
    std::shared_ptr<telux::loc::IDgnssManager> mDgnssManager = nullptr;
    static std::shared_ptr<LocationPAController> instance;
    std::map<taf_pa_location_LocationId, std::shared_ptr<PALocationClient>> locationClients_;
    ReusableIdGenerator id_generator_;
    std::mutex mutex_;
    taf_pa_location_DgnssEventListener* dgnssListener_ = nullptr;
    std::any dgnssListenerContext_;
};

taf_pa_result_t LocationPAController::PALocationClient::RegisterEventListener(
taf_pa_location_LocationId locationId,
    taf_pa_location_EventListener* listener,
    std::any context) {

    locationId_ = locationId;
    if (listener != nullptr) {
        // write eventListener_ under mtx_ so that concurrent SB callbacks
        // cannot observe a partially-written pointer.
        std::lock_guard<std::mutex> lock(mtx_);
        eventListener_ = listener;
    } else {
        TAF_PA_ERROR("Listener is NULL for location ID %llu", locationId);
        return TAF_PA_BAD_PARAMETER;
    }

    if (context.has_value())
    {
        eventListenerContext_ = std::move(context);
    }

    return TAF_PA_OK;
}

taf_pa_result_t LocationPAController::RegisterDgnssEventListener(
    taf_pa_location_DgnssEventListener* listener,
    std::any context) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (listener != nullptr) {
        dgnssListener_ = listener;
    } else {
        TAF_PA_ERROR("Dgnss Listener is NULL");
        return TAF_PA_BAD_PARAMETER;
    }

    if (context.has_value())
    {
        dgnssListenerContext_ = std::move(context);
    }
    return TAF_PA_OK;
}

taf_pa_result_t LocationPAController::DeregisterDgnssEventListener(std::any context)
{

    std::lock_guard<std::mutex> lock(mutex_);

    if (dgnssListener_ != nullptr)
    {
        dgnssListener_ = nullptr;
        TAF_PA_INFO("dgnssListener_ is NULL");
    }
    else
    {
        TAF_PA_INFO("Dgnss dgnssListener_ is aready deregistered");
    }

    if (context.has_value())
    {
        dgnssListenerContext_ = std::move(context);
    }
    return TAF_PA_OK;
}

taf_pa_result_t LocationPAController::MapStatus(telux::common::Status status){
    switch(status)
    {
        case telux::common::Status::SUCCESS:
            TAF_PA_DEBUG("Operation processed successfully");
            return TAF_PA_OK;
        case telux::common::Status::FAILED:
            TAF_PA_DEBUG("Operation processing failed");
            return TAF_PA_FAULT;
        case telux::common::Status::INVALIDPARAM:
            TAF_PA_DEBUG("Input parameters are invalid");
            return TAF_PA_BAD_PARAMETER;
        case telux::common::Status::NOTALLOWED:
            TAF_PA_DEBUG("Operation not allowed");
            return TAF_PA_NOT_PERMITTED;
        case telux::common::Status::NOTIMPLEMENTED:
            TAF_PA_DEBUG("Feature not implemented");
            return TAF_PA_NOT_IMPLEMENTED ;
        case telux::common::Status::CONNECTIONLOST:
            TAF_PA_DEBUG("Connection to Socket server lost");
            return TAF_PA_NOT_FOUND;
        case telux::common::Status::EXPIRED:
            TAF_PA_DEBUG("Operation has expired");
            return TAF_PA_TERMINATED;
        case telux::common::Status::NOTSUPPORTED:
            TAF_PA_DEBUG("Not supported on target platform");
            return TAF_PA_UNSUPPORTED;
        default:
           return TAF_PA_FAULT;
    }
}

taf_pa_result_t LocationPAController::MapErrorCode(telux::common::ErrorCode errorCode)
{
    switch(errorCode)
    {
        case telux::common::ErrorCode::SUCCESS:
            TAF_PA_DEBUG("Operation processed successfully");
            return TAF_PA_OK;
        case telux::common::ErrorCode::GENERIC_FAILURE:
            TAF_PA_ERROR("Operation processing failed");
            return TAF_PA_FAULT;
        case telux::common::ErrorCode::INVALID_ARGUMENTS:
            TAF_PA_ERROR("Input parameters are invalid");
            return TAF_PA_BAD_PARAMETER;
        case telux::common::ErrorCode::OPERATION_NOT_ALLOWED:
            TAF_PA_ERROR("Operation not allowed");
            return TAF_PA_NOT_PERMITTED;
        case telux::common::ErrorCode::TIMEOUT_ERROR:
            TAF_PA_ERROR("TimeOut Error");
            return TAF_PA_TIMEOUT;
        case telux::common::ErrorCode::INFO_UNAVAILABLE:
            TAF_PA_ERROR("Information not available");
            return TAF_PA_UNAVAILABLE;
        case telux::common::ErrorCode::SUBSYSTEM_UNAVAILABLE:
            TAF_PA_ERROR("Subsystem Not Available");
            return TAF_PA_UNAVAILABLE;
        case telux::common::ErrorCode::REQUEST_NOT_SUPPORTED:
            TAF_PA_ERROR("Request Not supported");
            return TAF_PA_UNSUPPORTED;
        default:
           return TAF_PA_FAULT;
    }
}

void LocationPAController::PALocationClient::Init(){
    if(locationManager_ != nullptr)
    {
        TAF_PA_INFO("locationManager_ --> Init: %p", locationManager_.get());
        auto status = locationManager_->registerForSystemInfoUpdates(shared_from_this());
        if(status == telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("registerForSystemInfoUpdates listener for location system information");
        }
        else
        {
            TAF_PA_ERROR("Failed to register a listener for location system information");
        }
        status = locationManager_->registerListenerEx(shared_from_this());
        if(status == telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("registerListenerEx a listener!!");
        }
        else
        {
            TAF_PA_ERROR("Failed to register a listener");
        }
    }
    else
    {
        TAF_PA_INFO("locationManager_ is null");
    }
}

void LocationPAController::PALocationClient::CleanUp()
{
    TAF_PA_INFO("CleanUp!!");

    if(locationManager_ != nullptr)
    {
        TAF_PA_INFO("locationManager_ --> CleanUp: %p", locationManager_.get());
        auto status = locationManager_->deRegisterListenerEx(weak_from_this());
        if(status == telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("Deregistered a listener!!");
        }
        else
        {
            TAF_PA_ERROR("Failed to deregister a listener");
        }

        // Synchronize deRegister callback to prevent manager teardown overlap
        try {
            TAF_PA_INFO("Deregistering system info updates with callback synchronization");
            auto deregisterPromisePtr = std::make_shared<std::promise<void>>();
            auto deregisterStatus = locationManager_->deRegisterForSystemInfoUpdates(
                weak_from_this(),
                [deregisterPromisePtr](telux::common::ErrorCode error) {
                    try {
                        if(error == telux::common::ErrorCode::SUCCESS) {
                            TAF_PA_INFO("Successfully deregistered system info updates");
                        } else {
                            TAF_PA_ERROR("Failed to deregister system info updates, error: %d",
                                    static_cast<int>(error));
                        }
                        deregisterPromisePtr->set_value();
                    }
                    catch (const std::future_error& e) {
                        TAF_PA_ERROR("Future error in deRegister callback: %s", e.what());
                    }
                    catch (const std::exception& e) {
                        TAF_PA_ERROR("Exception in deRegister callback: %s", e.what());
                    }
                    catch (...) {
                        TAF_PA_ERROR("Unknown error in deRegister callback");
                    }
                });

            if(deregisterStatus == telux::common::Status::SUCCESS) {
                // Wait for deregister to complete with CLEANUP_TIMEOUT_SECONDS timeout
                auto deregisterFuture = deregisterPromisePtr->get_future();
                if(deregisterFuture.wait_for(std::chrono::seconds(CLEANUP_TIMEOUT_SECONDS))
                        == std::future_status::ready) {
                    TAF_PA_INFO("DeRegisterForSystemInfoUpdates completed successfully");
                } else {
                    TAF_PA_ERROR("Timeout waiting for deRegisterForSystemInfoUpdates to complete");
                }
            } else {
                TAF_PA_ERROR("DeRegisterForSystemInfoUpdates failed to initiate with status: %d",
                        static_cast<int>(deregisterStatus));
            }
        }
        catch (const std::exception& e) {
            TAF_PA_ERROR("Exception during deRegisterForSystemInfoUpdates in cleanup: %s", e.what());
        }
    }
    else
    {
        TAF_PA_INFO("locationManager_ is null");
    }
}

taf_pa_location_NavigationSolutionType_t LocationPAController::PALocationClient:: convertTeluxToNavigationSolutionType(telux::loc::NavigationSolution naviSolution)
{
    TAF_PA_INFO("convertTeluxToNavigationSolutionType naviSolution-> %lu",naviSolution.to_ulong());
    taf_pa_location_NavigationSolutionType_t navSolution = (taf_pa_location_NavigationSolutionType_t) 0;

    // Check if naviSolution is empty/invalid
    if(naviSolution.to_ulong() == 0) {
        TAF_PA_ERROR("Invalid navigation solution type: value is 0");
        return navSolution;  // Return 0 safely
    }

    if(naviSolution.to_ulong() & (1 << telux::loc::NAV_SBAS_SOLUTION_IONO))
    {
        TAF_PA_DEBUG("telux::loc::NAV_SBAS_SOLUTION_IONO");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_SBAS_SOLUTION_IONO);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_SBAS_SOLUTION_FAST))
    {
        TAF_PA_DEBUG("telux::loc::NAV_SBAS_SOLUTION_FAST");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_SBAS_SOLUTION_FAST);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_SBAS_SOLUTION_LONG))
    {
        TAF_PA_DEBUG("telux::loc::NAV_SBAS_SOLUTION_LONG");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_SBAS_SOLUTION_LONG);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_SBAS_INTEGRITY))
    {
        TAF_PA_DEBUG("telux::loc::NAV_SBAS_INTEGRITY");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_SBAS_INTEGRITY);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_DGNSS_SOLUTION))
    {
        TAF_PA_DEBUG("telux::loc::NAV_DGNSS_SOLUTION");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_DGNSS_SOLUTION);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_RTK_SOLUTION))
    {
        TAF_PA_DEBUG("telux::loc::NAV_RTK_SOLUTION");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_RTK_SOLUTION);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_PPP_SOLUTION))
    {
        TAF_PA_DEBUG("telux::loc::NAV_PPP_SOLUTION");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_PPP_SOLUTION);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_RTK_FIXED_SOLUTION))
    {
        TAF_PA_DEBUG("telux::loc::NAV_RTK_FIXED_SOLUTION");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_RTK_FIXED_SOLUTION);
    }
    if(naviSolution.to_ulong() & (1<<telux::loc::NAV_ONLY_SBAS_CORRECTED_SV_USED))
    {
        TAF_PA_DEBUG("telux::loc::NAV_ONLY_SBAS_CORRECTED_SV_USED");
        navSolution = static_cast<taf_pa_location_NavigationSolutionType_t>(
        navSolution | TAF_PA_LOCATION_NAV_ONLY_SBAS_CORRECTED_SV_USED);
    }

    // Log if no valid bits were found (but still return the value)
    if(navSolution == 0) {
        TAF_PA_ERROR("Invalid navigation solution type: no recognized bits set in value %lu", naviSolution.to_ulong());
    }
    return navSolution;
}

taf_pa_location_LocationId LocationPAController::CreateLocationClient()
{
    std::shared_ptr<telux::loc::ILocationManager> sdkLocationManager = nullptr;

    auto &locationFactory = LocationFactory::getInstance();

    sdkLocationManager = locationFactory.getLocationManager();

    if(!sdkLocationManager)
    {
        TAF_PA_ERROR("Failed to get location manager instance");
        return ReusableIdGenerator::INVALID_LOCATION_ID;
    }

    // Check service status using getServiceStatus()
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;
    telux::common::ServiceStatus serviceStatus = sdkLocationManager->getServiceStatus();

    if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("Location manager subsystem is not ready (status: %d), waiting...",
               static_cast<int>(serviceStatus));

        // Get with callback to wait for availability
        auto prom = std::make_shared<std::promise<ServiceStatus>>();

        sdkLocationManager = locationFactory.getLocationManager(
             [&prom](ServiceStatus status) {
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
            });

        if (!sdkLocationManager)
        {
            TAF_PA_ERROR("Failed to get location manager with callback");
            return ReusableIdGenerator::INVALID_LOCATION_ID;
        }

        startTime = std::chrono::system_clock::now();
        std::future<telux::common::ServiceStatus> initFuture = prom->get_future();
        std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(5));

        if (std::future_status::timeout == waitStatus)
        {
            TAF_PA_ERROR("Timeout waiting for location manager");
            sdkLocationManager = nullptr;
            return ReusableIdGenerator::INVALID_LOCATION_ID;
        }
        else
        {
            serviceStatus = initFuture.get();
            if (serviceStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                endTime = std::chrono::system_clock::now();
                std::chrono::duration<double> elapsedTime = endTime - startTime;
                TAF_PA_INFO("Elapsed Time for Subsystems to ready: %lf", elapsedTime.count());
            }
            else
            {
                TAF_PA_ERROR("Unable to get location manager - service unavailable");
                sdkLocationManager = nullptr;
                return ReusableIdGenerator::INVALID_LOCATION_ID;
            }
        }
    }
    else
    {
        TAF_PA_INFO("Location manager subsystem is already ready");
    }

    taf_pa_location_LocationId newId = id_generator_.acquireId();
    if (newId == ReusableIdGenerator::INVALID_LOCATION_ID) {
        TAF_PA_ERROR("Failed to acquire a new location ID");
        return ReusableIdGenerator::INVALID_LOCATION_ID;
    }

    std::shared_ptr<LocationPAController::PALocationClient> paLocationClient =
    std::make_shared<LocationPAController::PALocationClient>(sdkLocationManager);

    paLocationClient->Init();

    TAF_PA_INFO("sdkLocationManager: %p", sdkLocationManager.get());

    locationClients_[newId] = paLocationClient;
    TAF_PA_INFO("Created location client for with ID %llu", newId);
    return newId;
}

taf_pa_result_t LocationPAController::ReleaseLocationClient(taf_pa_location_LocationId id)
{
    TAF_PA_INFO("Release location Client with ID %llu", id);

    std::shared_ptr<LocationPAController::PALocationClient> clientPtr;

    try {
        auto it = locationClients_.find(id);
        if (it == locationClients_.end()) {
            TAF_PA_ERROR("Invalid location ID (%llu) provided for release!", id);
            return TAF_PA_BAD_PARAMETER;
        }

        clientPtr = it->second;
        locationClients_.erase(it);

        id_generator_.releaseId(id);

        TAF_PA_INFO("Client removed from map, now calling CleanUp() for client=%p", clientPtr.get());

        if (clientPtr) {
            clientPtr->CleanUp();
        }
    } catch (const std::exception& ex) {
        TAF_PA_ERROR("Exception during CleanUp: %s", ex.what());
    } catch (...) {
        TAF_PA_ERROR("Unknown exception during CleanUp");
    }

    TAF_PA_INFO("Location client with ID %llu released.", id);
    return TAF_PA_OK;
}

std::shared_ptr<LocationPAController::PALocationClient>
LocationPAController::GetClientPtr(taf_pa_location_LocationId id) {
    auto it = locationClients_.find(id);
    if (it != locationClients_.end()) {
        return it->second;
    }
    TAF_PA_ERROR("No Location client found for ID %llu", id);
    return nullptr;
}

taf_pa_result_t tafpa::location::taf_pa_location_CreateClient(taf_pa_location_LocationId* clientIdPtr) {
    auto paCtrl = LocationPAController::getInstance();
    taf_pa_location_LocationId id = paCtrl->CreateLocationClient();
    // CreateLocationClient() returns INVALID_LOCATION_ID (0) on failure
    if (id == ReusableIdGenerator::INVALID_LOCATION_ID) {
        TAF_PA_ERROR("Failed to create location client: CreateLocationClient returned invalid ID");
        return TAF_PA_FAULT;
    }
    if (clientIdPtr) *clientIdPtr = id;
    return TAF_PA_OK;
}

taf_pa_result_t tafpa::location::taf_pa_location_DeleteClient(taf_pa_location_LocationId locationId) {
    TAF_PA_INFO("taf_pa_location_DeleteClient!!");
    auto paCtrl = LocationPAController::getInstance();
    return paCtrl->ReleaseLocationClient(locationId);
}

taf_pa_result_t tafpa::location::taf_pa_location_RegisterListener(
    taf_pa_location_LocationId locationId,
    taf_pa_location_EventListener* eventListener,
    std::any context) {

    auto paCtrl = LocationPAController::getInstance();
    std::shared_ptr<LocationPAController::PALocationClient> clientPtr = paCtrl->GetClientPtr(locationId);
    if (!clientPtr) {
        TAF_PA_ERROR("Invalid location ID (%llu) provided!", locationId);
        return TAF_PA_BAD_PARAMETER;
    }

    taf_pa_result_t result = clientPtr->RegisterEventListener(locationId, eventListener, context);
    if (result == TAF_PA_OK) {
        TAF_PA_INFO("Register Event Listener successfully for ID %llu", locationId);
        return TAF_PA_OK;
    }
    TAF_PA_ERROR("Failed to register event listener for ID %llu", locationId);
    return TAF_PA_FAULT;
}

taf_pa_result_t tafpa::location::taf_pa_location_registerDgnssEventListener(
    taf_pa_location_DgnssEventListener* eventListener,std::any context) {

    auto paCtrl = LocationPAController::getInstance();

    taf_pa_result_t result = paCtrl->RegisterDgnssEventListener(eventListener, context);
    if (result == TAF_PA_OK) {
        TAF_PA_INFO("Register Dgnss Event Listener successfully ");
        return TAF_PA_OK;
    }
    TAF_PA_ERROR("Failed to register Dgnss event listener");
    return TAF_PA_FAULT;
}

taf_pa_result_t tafpa::location::taf_pa_location_deregisterDgnssEventListener(std::any context) {

    auto paCtrl = LocationPAController::getInstance();

    taf_pa_result_t result = paCtrl->DeregisterDgnssEventListener(context);
    if (result == TAF_PA_OK) {
        TAF_PA_INFO("Deregister Dgnss Event Listener successfully ");
        return TAF_PA_OK;
    }
    TAF_PA_ERROR("Failed to deregister Dgnss event listener");
    return TAF_PA_FAULT;
}

taf_pa_result_t LocationPAController::initialize()
{
    TAF_PA_INFO("initialize!!");
    telux::common::Status status;
    status = telux::common::Status::SUCCESS;
    if(mLocationConfigurator == nullptr)
    {
        auto &locationFactory = telux::loc::LocationFactory::getInstance();

        //First, try to get the configurator without callback
        mLocationConfigurator = locationFactory.getLocationConfigurator();

        if (!mLocationConfigurator)
        {
            TAF_PA_CRIT("*** ERROR - mLocationConfigurator is NULL");
            return TAF_PA_FAULT;
        }

        // Check service status using getServiceStatus()
        telux::common::ServiceStatus serviceStatus = mLocationConfigurator->getServiceStatus();

        if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            TAF_PA_INFO("Location configurator subsystem is not ready, waiting for it to be ready...");

            // If not ready, get with callback to wait for availability
            auto prom = std::make_shared<std::promise<ServiceStatus>>();
            std::chrono::time_point<std::chrono::system_clock> startTime, endTime;

            mLocationConfigurator = locationFactory.getLocationConfigurator(
                [&prom](telux::common::ServiceStatus status) {
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
                });

            startTime = std::chrono::system_clock::now();
            std::future<telux::common::ServiceStatus> initFuture = prom->get_future();
            std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(5));

            if (std::future_status::timeout == waitStatus)
            {
                TAF_PA_ERROR("Timeout waiting for location configurator");
                mLocationConfigurator = nullptr;
                return TAF_PA_FAULT;
            }
            else
            {
                serviceStatus = initFuture.get();
                if (serviceStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                    endTime = std::chrono::system_clock::now();
                    std::chrono::duration<double> elapsedTime = endTime - startTime;
                    TAF_PA_INFO("Elapsed Time for configuration subsystems to ready : %lf",
                            elapsedTime.count());
                }
                else
                {
                    TAF_PA_ERROR("ERROR - Unable to initialize Location configuration subsystem");
                    mLocationConfigurator = nullptr;
                    status = telux::common::Status::FAILED;
                }
            }
        }
        else
        {
            TAF_PA_INFO("Location configurator subsystem is already ready.");
        }
    }
    else
    {
        TAF_PA_CRIT("Location configurator is already initialized");
    }

    if (status != telux::common::Status::SUCCESS) {
        TAF_PA_ERROR("LocationConfigurator not available");
        return TAF_PA_FAULT;
    }
    return TAF_PA_OK;
}

telux::common::Status LocationPAController::RegisterDgnssManager()
{
    TAF_PA_INFO("RegisterDgnssManager");
    auto status = telux::common::Status::FAILED;
    auto paCtrl = LocationPAController::getInstance();

    if(!paCtrl) {
        TAF_PA_ERROR("Failed to get LocationPAController instance");
        return status;
    }
    TAF_PA_INFO("RegisterDgnssManager mDgnssManager: %p",mDgnssManager);
    if(mDgnssManager != nullptr)
    {
        status = mDgnssManager->registerListener(shared_from_this());
        if(status == telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("registerListener Dgnss listener!!");
        }
        else
        {
            TAF_PA_ERROR("Failed to register Dgnss listener");
            return status;
        }
    }
    else
    {
        TAF_PA_INFO("paCtrl->mDgnssManager is null");
    }

    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_Init() {
    TAF_PA_DEBUG("PA implementation.");

    // Check if already initialized (idempotent pattern)
    if (gLocationPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Location platform adaptor already initialized");
        return TAF_PA_OK;  // Idempotent - safe to call multiple times
    }

    auto paCtrl = LocationPAController::getInstance();
    taf_pa_result_t res = paCtrl->initialize();
    if (res == TAF_PA_OK) {
        gLocationPaInitialized.store(true, std::memory_order_release);
        TAF_PA_INFO("Location platform adaptor initialization flag set to true.");
        TAF_PA_INFO("Location Platform adapter initialization done.");
    } else {
        TAF_PA_CRIT("Location Platform adapter initialization failed.");
    }
    return res;
}

telux::common::Status LocationPAController::PALocationClient::StartDetailedEngineReports(uint32_t optInterval, uint16_t engineType, taf_pa_location_GeneralCb  callback, uint32_t reportMask, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;

    status =  locationManager_->startDetailedEngineReports(optInterval, engineType, cb, reportMask);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_startDetailedEngineReports(taf_pa_location_LocationId clientId, uint32_t optInterval, uint16_t engineType, taf_pa_location_GeneralCb callback, uint32_t reportMask, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    std::shared_ptr<LocationPAController::PALocationClient> clientPtr = paCtrl->GetClientPtr(clientId);
    if (!clientPtr) {
        TAF_PA_ERROR("Invalid location ID (%llu) provided!", clientId);
        return TAF_PA_BAD_PARAMETER;
    }

    auto status = clientPtr->StartDetailedEngineReports(optInterval, engineType, callback, reportMask, context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("Location engine start failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::PALocationClient::StopReports(taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  locationManager_->stopReports(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_stopReports(taf_pa_location_LocationId clientId, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    std::shared_ptr<LocationPAController::PALocationClient> clientPtr = paCtrl->GetClientPtr(clientId);
    if (!clientPtr) {
        TAF_PA_ERROR("Invalid location ID (%llu) provided!", clientId);
        return TAF_PA_BAD_PARAMETER;
    }

    auto status = clientPtr->StopReports(callback, context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("Location engine start failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }
    return TAF_PA_OK;
}

uint32_t LocationPAController::PALocationClient::GetCapabilities()
{
    auto paCtrl = LocationPAController::getInstance();

    uint32_t sdkCapData;
    sdkCapData =  locationManager_->getCapabilities();
    return sdkCapData;
}

taf_pa_result_t tafpa::location::taf_pa_location_getCapabilities(taf_pa_location_LocationId clientId, uint32_t* capabilitiesPtr, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    std::shared_ptr<LocationPAController::PALocationClient> clientPtr = paCtrl->GetClientPtr(clientId);
    if (!clientPtr) {
        TAF_PA_ERROR("Invalid location ID (%llu) provided!", clientId);
        return TAF_PA_BAD_PARAMETER;
    }

    if (capabilitiesPtr) *capabilitiesPtr = clientPtr->GetCapabilities();
    return TAF_PA_OK;
}

telux::common::Status LocationPAController::ConfigureConstellations(std::vector<telux::loc::SvBlackListInfo> SvBlackList, taf_pa_location_GeneralCb callback, bool deviceReset, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureConstellations(SvBlackList, cb, deviceReset);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureConstellations(const std::vector<taf_pa_location_SvBlackListInfo_t>& svBlackListData, taf_pa_location_GeneralCb callback, bool deviceReset, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    std::vector<telux::loc::SvBlackListInfo> svBlackList;
    for(const auto &data : svBlackListData)
    {
        telux::loc::SvBlackListInfo blackListInfo;

        blackListInfo.constellation =  (telux::loc::GnssConstellationType)data.constellation;
        blackListInfo.svId = data.svId;

        svBlackList.push_back(blackListInfo);
    }

    telux::common::Status status = paCtrl->ConfigureConstellations(svBlackList, callback, deviceReset, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::DeleteAidingData(telux::loc::AidingData AidingData, taf_pa_location_GeneralCb callback,std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->deleteAidingData(AidingData, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_deleteAidingData(taf_pa_location_AidingDataType_t AidingData, taf_pa_location_GeneralCb callback,std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->DeleteAidingData((telux::loc::AidingData)AidingData, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::DeleteAllAidingData(taf_pa_location_GeneralCb callback,std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->deleteAllAidingData(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_deleteAllAidingData(taf_pa_location_GeneralCb callback,std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->DeleteAllAidingData(callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureMinSVElevation(uint8_t minElevation, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureMinSVElevation(minElevation, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureMinSVElevation(uint8_t minElevation, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->ConfigureMinSVElevation(minElevation, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::RequestMinSVElevation(taf_pa_location_RequestMinSVElevationCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();
    uint8_t minSVElevation;
    //Sdk Callback
    auto cb = [promisePtr,&minSVElevation,&paCtrl](uint8_t minSVElevation_, telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                minSVElevation = minSVElevation_;
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->requestMinSVElevation(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,&minSVElevation,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_requestMinSVElevation(taf_pa_location_RequestMinSVElevationCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->RequestMinSVElevation(callback, context);
    return paCtrl->MapStatus(status);
}


telux::common::Status LocationPAController::ConfigureNmeaTypes(telux::loc::NmeaSentenceConfig nmeaType, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureNmeaTypes(nmeaType, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureNmeaTypes(taf_pa_location_NmeaSentenceType_t nmeaType, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->ConfigureNmeaTypes((telux::loc::NmeaSentenceConfig)nmeaType, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureDR(telux::loc::DREngineConfiguration drConfigData, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureDR(drConfigData, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureDR(const taf_pa_location_DREngineConfiguration_t& drConfig, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::loc::DREngineConfiguration drConfigData;
    drConfigData.validMask = static_cast<telux::loc::DRConfigValidity>(0);
    drConfigData.speedFactor = drConfig.speedFactor;
    drConfigData.speedFactorUnc = drConfig.speedFactorUnc;
    drConfigData.gyroFactor = drConfig.gyroFactor;
    drConfigData.gyroFactorUnc = drConfig.gyroFactorUnc;

    drConfigData.mountParam.rollOffset = drConfig.mountParam.rollOffset;
    drConfigData.mountParam.yawOffset = drConfig.mountParam.yawOffset;
    drConfigData.mountParam.pitchOffset = drConfig.mountParam.pitchOffset;
    drConfigData.mountParam.offsetUnc = drConfig.mountParam.offsetUnc;

    telux::common::Status status = paCtrl->ConfigureDR(drConfigData, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureEngineState(telux::loc::EngineType engineType, telux::loc::LocationEngineRunState engineState, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureEngineState(engineType, engineState, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureEngineState(taf_pa_location_EngineType_t engineType, taf_pa_location_LocationEngineRunState_t engineState, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->ConfigureEngineState((telux::loc::EngineType)engineType, (telux::loc::LocationEngineRunState)engineState, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureRobustLocation(bool enableRobustloc, bool enableE911loc, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureRobustLocation(enableRobustloc, enableE911loc, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureRobustLocation(bool enableRobustloc, bool enableE911loc, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->ConfigureRobustLocation(enableRobustloc, enableE911loc, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::RequestRobustLocation(taf_pa_location_RequestRobustLocationCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();
    taf_pa_location_RobustLocationConfiguration_t rLConfig;
    //Sdk Callback
    auto cb = [promisePtr,&rLConfig,&paCtrl](const telux::loc::RobustLocationConfiguration rLConfig_, telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                rLConfig.validMask = rLConfig_.validMask;
                if(rLConfig_.validMask & telux::loc::VALID_ENABLED)
                {
                    rLConfig.enabled = rLConfig_.enabled;
                }
                if(rLConfig_.validMask & telux::loc::VALID_ENABLED_FOR_E911)
                {
                    rLConfig.enabledForE911 = rLConfig_.enabledForE911;
                }
                if(rLConfig_.validMask & telux::loc::VALID_VERSION)
                {
                    rLConfig.version.major = unsigned (rLConfig_.version.major);
                    rLConfig.version.minor = rLConfig_.version.minor;
                }
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->requestRobustLocation(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,rLConfig, context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_requestRobustLocation(taf_pa_location_RequestRobustLocationCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->RequestRobustLocation(callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureSecondaryBand(telux::loc::ConstellationSet constellationSet, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureSecondaryBand(constellationSet, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureSecondaryBand(const std::unordered_set<taf_pa_location_GnssConstellationType_t>& constSet, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::loc::ConstellationSet constellationSet{};

    for(auto i:constSet){
        constellationSet.insert((telux::loc::GnssConstellationType)i);
    }

    telux::common::Status status = paCtrl->ConfigureSecondaryBand(constellationSet, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::RequestSecondaryBandConfig(taf_pa_location_RequestSecondaryBandConfigCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();
    std::set<taf_pa_location_GnssConstellationType_t> constSet;
    //Sdk Callback
    auto cb = [promisePtr,&constSet,&paCtrl](telux::loc::ConstellationSet set, telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                for(auto i:set)
                {
                    constSet.insert((taf_pa_location_GnssConstellationType_t) i);
                }
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->requestSecondaryBandConfig(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,constSet, context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_requestSecondaryBandConfig(taf_pa_location_RequestSecondaryBandConfigCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->RequestSecondaryBandConfig(callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureLeverArm(LeverArmConfigInfo configInfo , taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureLeverArm(configInfo, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureLeverArm(const taf_pa_location_LeverArmParams_t* leverArmConfigInfoPtr, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    LeverArmConfigInfo configInfo;
    telux::loc::LeverArmType leverArmType;
    telux::loc::LeverArmParams leverArmParams;

    if(leverArmConfigInfoPtr->levArmType == TAF_PA_LOCATION_LEVER_ARM_TYPE_GNSS_TO_VRP)
    {
        leverArmType = telux::loc::LEVER_ARM_TYPE_GNSS_TO_VRP;
    }
    else if(leverArmConfigInfoPtr->levArmType == TAF_PA_LOCATION_LEVER_ARM_TYPE_DR_IMU_TO_GNSS)
    {
        leverArmType = telux::loc::LEVER_ARM_TYPE_DR_IMU_TO_GNSS;
    }
    else if(leverArmConfigInfoPtr->levArmType == TAF_PA_LOCATION_LEVER_ARM_TYPE_VPE_IMU_TO_GNSS)
    {
        leverArmType = telux::loc::LEVER_ARM_TYPE_VPE_IMU_TO_GNSS;
    }

    //Filling the Lever Arm Paramters
    leverArmParams.forwardOffset = (float)leverArmConfigInfoPtr->forwardOffset;
    leverArmParams.sidewaysOffset =(float)leverArmConfigInfoPtr->sidewaysOffset;
    leverArmParams.upOffset = (float)leverArmConfigInfoPtr->upOffset;
    configInfo.insert({leverArmType, leverArmParams});

    telux::common::Status status = paCtrl->ConfigureLeverArm(configInfo, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureMinGpsWeek(uint16_t minGpsWeek, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureMinGpsWeek(minGpsWeek, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}


taf_pa_result_t tafpa::location::taf_pa_location_configureMinGpsWeek(uint16_t minGpsWeek, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->ConfigureMinGpsWeek(minGpsWeek, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureNmea(telux::loc::NmeaConfig nmeaConfig, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureNmea(nmeaConfig, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_configureNmea(const taf_pa_location_NmeaConfig_t& nmeaConfigData,  taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::loc::NmeaConfig nmeaConfig;
    nmeaConfig.sentenceConfig = nmeaConfigData.sentenceConfig;
    nmeaConfig.datumType = (telux::loc::GeodeticDatumType) nmeaConfigData.datumType;
    nmeaConfig.engineType = nmeaConfigData.engineType;

    telux::common::Status status = paCtrl->ConfigureNmea(nmeaConfig, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::RequestMinGpsWeek(taf_pa_location_RequestMinGpsWeekCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();
    uint16_t minGpsWeek;
    //Sdk Callback
    auto cb = [promisePtr,&minGpsWeek,&paCtrl](uint16_t minGpsWeek_, telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                minGpsWeek = minGpsWeek_;
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->requestMinGpsWeek(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult,&minGpsWeek, context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_requestMinGpsWeek(taf_pa_location_RequestMinGpsWeekCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->RequestMinGpsWeek(callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::RequestXtraStatus(taf_pa_location_RequestXtraStatusCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();
    taf_pa_location_XtraStatus_t XtraStatusData;
    //Sdk Callback
    auto cb = [promisePtr,&XtraStatusData,&paCtrl](telux::loc::XtraStatus xtraStatus, telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                XtraStatusData.featureEnabled = xtraStatus.featureEnabled;
                XtraStatusData.xtraValidForHours = xtraStatus.xtraValidForHours;
                switch(xtraStatus.xtraDataStatus)
                {
                    case telux::loc::XtraDataStatus::STATUS_UNKNOWN:
                    XtraStatusData.xtraDataStatus = TAF_PA_LOCATION_STATUS_UNKNOWN;
                    break;
                    case telux::loc::XtraDataStatus::STATUS_NOT_AVAIL:
                    XtraStatusData.xtraDataStatus = TAF_PA_LOCATION_STATUS_NOT_AVAIL;
                    break;
                    case telux::loc::XtraDataStatus::STATUS_NOT_VALID:
                    XtraStatusData.xtraDataStatus = TAF_PA_LOCATION_STATUS_NOT_VALID;
                    break;
                    case telux::loc::XtraDataStatus::STATUS_VALID:
                    XtraStatusData.xtraDataStatus = TAF_PA_LOCATION_STATUS_VALID;
                    break;
                }
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->requestXtraStatus(cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready){
            taf_pa_result_t selfResult = futResult.get();
            if(callback){
                callback(selfResult, XtraStatusData, context);
            }
        }else{
            TAF_PA_ERROR("Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_requestXtraStatus(taf_pa_location_RequestXtraStatusCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    telux::common::Status status = paCtrl->RequestXtraStatus(callback, context);
    return paCtrl->MapStatus(status);
}

void LocationPAController::PALocationClient::onGnssNmeaInfo(uint64_t timestamp, const std::string &nmea)
{
    TAF_PA_DEBUG( "**** onGnssNmeaInfo Information --> PA****" );

    auto nmeaEvent = std::make_shared<taf_pa_location_NmeaInfoEvent_t>();

    nmeaEvent->timestamp = timestamp;
    nmeaEvent->nmeaMask = nmea;

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener1 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener1 = eventListener_;
    }
    if(locListener1 && locListener1->onGnssNmeaInfo){
        locListener1->onGnssNmeaInfo(locationId_,nmeaEvent,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onGnssNmeaInfo");
    }
}

telux::common::Status LocationPAController::InjectMerkleTreeInformation(const std::string merkleTreeInfo, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->injectMerkleTreeInformation(merkleTreeInfo, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready)
        {
            taf_pa_result_t selfResult = futResult.get();
            if(callback)
            {
                callback(selfResult,context);
            }
        }
        else
        {
            TAF_PA_ERROR("InjectMerkleTreeInformation Timeout waiting for result..");
        }
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_injectMerkleTreeInformation(const std::string merkleTreeInfo, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();
    TAF_PA_INFO("taf_pa_location_injectMerkleTreeInformation");
    telux::common::Status status = paCtrl->InjectMerkleTreeInformation(merkleTreeInfo, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureOsnma(bool enableOsnma, taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureOsnma(enableOsnma, cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready)
        {
            taf_pa_result_t selfResult = futResult.get();
            if(callback)
            {
                callback(selfResult,context);
            }
        }
        else
        {
            TAF_PA_ERROR("configureOsnma Timeout waiting for result..");
        }
    }
    return status;
}
taf_pa_result_t tafpa::location::taf_pa_location_configureOsnma(bool enableOsnma, taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();
    TAF_PA_INFO("taf_pa_location_configureOsnma");
    telux::common::Status status = paCtrl->ConfigureOsnma(enableOsnma, callback, context);
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ConfigureEngineIntegrityRisk(telux::loc::EngineType engineType,uint32_t integrityRisk,taf_pa_location_GeneralCb callback, std::any context)
{
    auto promisePtr = std::make_shared<std::promise<taf_pa_result_t>>();
    auto paCtrl = LocationPAController::getInstance();

    //Sdk Callback
    auto cb = [promisePtr,&paCtrl](telux::common::ErrorCode error) {
        try{
            if(error == telux::common::ErrorCode::SUCCESS) {
                promisePtr->set_value(TAF_PA_OK);
            }
            else{
                taf_pa_result_t res = paCtrl->MapErrorCode(error);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            TAF_PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            TAF_PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown error in callback.");
        }
    };
    telux::common::Status status = telux::common::Status::FAILED;
    status =  mLocationConfigurator->configureEngineIntegrityRisk(engineType,integrityRisk,cb);
    if(status == telux::common::Status::SUCCESS){
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT))
            == std::future_status::ready)
        {
            taf_pa_result_t selfResult = futResult.get();
            if(callback)
            {
                callback(selfResult,context);
            }
        }
        else
        {
            TAF_PA_ERROR("configureEngineIntegrityRisk Timeout waiting for result..");
        }
    }
    return status;
}
taf_pa_result_t tafpa::location::taf_pa_location_configureEngineIntegrityRisk(taf_pa_location_EngineType_t engineType,uint32_t integrityRisk,taf_pa_location_GeneralCb callback, std::any context)
{
    auto paCtrl = LocationPAController::getInstance();
    TAF_PA_INFO("taf_pa_location_ConfigureEngineIntegrityRisk");
    telux::common::Status status = paCtrl->ConfigureEngineIntegrityRisk((telux::loc::EngineType)engineType,integrityRisk,callback,context);
    return paCtrl->MapStatus(status);
}
//dgns
telux::common::Status LocationPAController::InjectCorrectionData(const uint8_t *injectionData, uint32_t injectionDataSize, taf_pa_location_GeneralCb  callback,std::any context)
{
    telux::common::Status status = telux::common::Status::FAILED;
    if (!mDgnssManager) {
        TAF_PA_ERROR("DGNSS Manager not initialized");
        if(callback) {
            callback(TAF_PA_FAULT, context);
        }
        return status;
    }

    status =  mDgnssManager->injectCorrectionData(injectionData, injectionDataSize);
    if(status == telux::common::Status::SUCCESS)
    {
        if(callback)
        {
            callback(TAF_PA_OK,context);
        }
        TAF_PA_INFO("Injecting correction data is success");
    }
    else
    {
        TAF_PA_INFO("Injecting correction data is failed");
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_injectCorrectionData(const uint8_t *injectionData,
uint32_t injectionDataSize, taf_pa_location_GeneralCb callback,std::any context)
{
    if (!injectionData || injectionDataSize == 0) {
        TAF_PA_ERROR("Invalid injection data: pointer is null or size is zero");
        return TAF_PA_BAD_PARAMETER;
    }
    auto paCtrl = LocationPAController::getInstance();

    auto status = paCtrl->InjectCorrectionData(injectionData, injectionDataSize, callback,context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("InjectCorrectionData failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::CreateDgnssSource(telux::loc::DgnssDataFormat
dgnssFormat, taf_pa_location_GeneralCb  callback,std::any context)
{
    telux::common::Status status = telux::common::Status::FAILED;

    if (!mDgnssManager) {
        TAF_PA_ERROR("DGNSS Manager not initialized");
        if(callback) {
            callback(TAF_PA_FAULT, context);
        }
        return status;
    }

    status =  mDgnssManager->createSource(dgnssFormat);
    if(status == telux::common::Status::SUCCESS)
    {
        if(callback)
        {
            callback(TAF_PA_OK,context);
        }
        TAF_PA_INFO("taf_pa_location_createDgnssSource is success");
    }
    else
    {
        TAF_PA_INFO("taf_pa_location_createDgnssSource is failed");
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_createDgnssSource(taf_pa_location_DgnssDataFormat_t dgnssFormat,taf_pa_location_GeneralCb callback,std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    auto status = paCtrl->CreateDgnssSource((telux::loc::DgnssDataFormat)dgnssFormat,callback,context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("taf_pa_location_createDgnssSource failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::ReleaseDgnssSource(taf_pa_location_GeneralCb
callback,std::any context)
{
    telux::common::Status status = telux::common::Status::FAILED;

    if (!mDgnssManager) {
        TAF_PA_ERROR("DGNSS Manager not initialized");
        if(callback) {
            callback(TAF_PA_FAULT, context);
        }
        return status;
    }

    status =  mDgnssManager->releaseSource();
    if(status == telux::common::Status::SUCCESS)
    {
        if(callback)
        {
            callback(TAF_PA_OK,context);
        }
        TAF_PA_INFO("ReleaseDgnssSource is success");
    }
    else
    {
        TAF_PA_INFO("ReleaseDgnssSource is failed");
    }
    return status;
}

taf_pa_result_t tafpa::location::taf_pa_location_releaseDgnssSource(taf_pa_location_GeneralCb callback,std::any context)
{
    auto paCtrl = LocationPAController::getInstance();

    auto status = paCtrl->ReleaseDgnssSource(callback,context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("taf_pa_location_releaseDgnssSource failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }
    return paCtrl->MapStatus(status);
}

telux::common::Status LocationPAController::InitializeDgnss(taf_pa_location_DgnssDataFormat_t dataFormat,
taf_pa_location_GeneralCb callback,std::any context)
{
    TAF_PA_INFO("InitializeDgnss!!");
    telux::common::Status statusDgnss;
    telux::common::Status status;
    status = telux::common::Status::SUCCESS;
    statusDgnss = telux::common::Status::SUCCESS;
    auto paCtrl = LocationPAController::getInstance();

    TAF_PA_INFO("InitializeDgnss!! mDgnssManager: %p",mDgnssManager);
    if(mDgnssManager == nullptr)
    {
        auto promDgnss = std::make_shared<std::promise<ServiceStatus>>();
        auto &locationFactory = telux::loc::LocationFactory::getInstance();
        mDgnssManager = locationFactory.getDgnssManager((telux::loc::DgnssDataFormat)dataFormat,[&promDgnss]
                (telux::common::ServiceStatus status) {
                try {
                    if (status == ServiceStatus::SERVICE_AVAILABLE) {
                        promDgnss->set_value(ServiceStatus::SERVICE_AVAILABLE);
                        TAF_PA_INFO("SERVICE_AVAILABLE");
                    } else {
                        promDgnss->set_value(ServiceStatus::SERVICE_UNAVAILABLE);
                        TAF_PA_INFO("SERVICE_UNAVAILABLE");
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
        });
        if (!mDgnssManager)
        {
            TAF_PA_CRIT("*** ERROR - dgnssManager is NULL");
            return telux::common::Status::FAILED;
        }

        std::chrono::time_point<std::chrono::system_clock> startTimeDgnss, endTimeDgnss;
        startTimeDgnss = std::chrono::system_clock::now();
        ServiceStatus dgnssMgrStatus = mDgnssManager->getServiceStatus();
        if(dgnssMgrStatus != ServiceStatus::SERVICE_AVAILABLE){
            TAF_PA_INFO("Dgnss subsystem is not ready, Please wait");
        }
        std::future<telux::common::ServiceStatus> initFutureDgnss = promDgnss->get_future();
        std::future_status waitStatusDgnss = initFutureDgnss.wait_for(std::chrono::seconds(5));
        telux::common::ServiceStatus serviceStatusDgnss;
        if (std::future_status::timeout == waitStatusDgnss)
        {
            TAF_PA_ERROR("Timeout waiting for dgnss manager");
            mDgnssManager = nullptr;
            return telux::common::Status::FAILED;
        } else {
            serviceStatusDgnss = initFutureDgnss.get();
            if (serviceStatusDgnss == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                endTimeDgnss = std::chrono::system_clock::now();
                std::chrono::duration<double> elapsedTimeDgnss = endTimeDgnss - startTimeDgnss;
                TAF_PA_INFO("Elapsed Time for dgnss subsystems to ready : %lf",elapsedTimeDgnss.count());
            }else{
                TAF_PA_ERROR("ERROR - Unable to initialize dgnss manager subsystem");
                mDgnssManager = nullptr;
                statusDgnss = telux::common::Status::FAILED;
            }
        }
    }
    else
    {
        TAF_PA_CRIT("dgnss manager is already initialized");
        return telux::common::Status::FAILED;
    }

    if (statusDgnss != telux::common::Status::SUCCESS) {
        TAF_PA_ERROR("dgnss manager not available");
        return telux::common::Status::FAILED;
    }
    if(callback)
    {
        callback(TAF_PA_OK,context);
    }
    return telux::common::Status::SUCCESS;
}

taf_pa_result_t tafpa::location::taf_pa_location_initializeDgnss(taf_pa_location_DgnssDataFormat_t dataFormat,
taf_pa_location_GeneralCb callback,std::any context)
{
    TAF_PA_INFO("taf_pa_location_initializeDgnss");
    auto paCtrl = LocationPAController::getInstance();

    auto status = paCtrl->InitializeDgnss(dataFormat,callback,context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("taf_pa_location_initializeDgnss failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }

    auto resDgnss = paCtrl->RegisterDgnssManager();
    if (resDgnss == telux::common::Status::SUCCESS)
    {
        TAF_PA_INFO("Dgnss manager registration is success.");
    }
    else {
        TAF_PA_ERROR("Dgnss manager registration is failed.");
    }

    return paCtrl->MapStatus(resDgnss);
}

telux::common::Status LocationPAController::DeInitializeDgnss(taf_pa_location_GeneralCb callback,
std::any context)
{
    TAF_PA_INFO("DeInitializeDgnss!!");

    auto paCtrl = LocationPAController::getInstance();
    if (mDgnssManager)
    {
        TAF_PA_INFO("InitializeDgnss!! mDgnssManager: %p",mDgnssManager);

        // Deregister synchronously without callbacks
        auto status = mDgnssManager->deRegisterListener();
        if (status == telux::common::Status::SUCCESS)
        {
            TAF_PA_INFO("Successfully deregistered Dgnss manager");
        }
        else
        {
            TAF_PA_ERROR("Failed to deregister Dgnss %d", (int)status);
            return telux::common::Status::FAILED;
        }
        // Clear the pointer
        mDgnssManager.reset();
    }
    else
    {
        TAF_PA_INFO("DeInitializeDgnss is already done");
    }
    if(callback)
    {
        callback(TAF_PA_OK,context);
    }
    return telux::common::Status::SUCCESS;
}

taf_pa_result_t tafpa::location::taf_pa_location_deInitializeDgnss(taf_pa_location_GeneralCb callback,
std::any context)
{
    TAF_PA_INFO("taf_pa_location_deInitializeDgnss");
    auto paCtrl = LocationPAController::getInstance();

    auto status = paCtrl->DeInitializeDgnss(callback,context);
    if(status != telux::common::Status::SUCCESS){
        TAF_PA_ERROR("taf_pa_location_deInitializeDgnss failed with status code %d",static_cast<int>(status));
        return TAF_PA_FAULT;
    }

    return paCtrl->MapStatus(status);
}

void LocationPAController::PALocationClient::onCapabilitiesInfo(const telux::loc::LocCapability capabilityInfo)
{
    TAF_PA_DEBUG( "**** onCapabilitiesInfo Information-->PA ****" );

    auto capabilityEvent = std::make_shared<taf_pa_location_CapabilityChangeEvent_t>();

    capabilityEvent->locCapability = (taf_pa_location_LocCapabilityType_t) capabilityInfo;

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener2 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener2 = eventListener_;
    }
    if(locListener2 && locListener2->onCapabilitiesInfo){
        locListener2->onCapabilitiesInfo(locationId_,capabilityEvent,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onCapabilitiesInfo");
    }
}

void LocationPAController::PALocationClient::onGnssSVInfo(const std::shared_ptr<telux::loc::IGnssSVInfo> &gnssSVInfo)
{
    TAF_PA_DEBUG( "**** onGnssSVInfo Information -->PA****" );

    std::vector<std::shared_ptr<taf_pa_location_GnssSVInfo_t>> GnssSVInfo;

    for(auto svInfo : gnssSVInfo->getSVInfoList())
    {
        auto svInfodata = std::make_shared<taf_pa_location_GnssSVInfo_t>();

        svInfodata->constType = (taf_pa_location_GnssConstellationType_t)svInfo->getConstellation();
        svInfodata->satId =  svInfo->getId();
        svInfodata->hasFix = (taf_pa_location_SVInfoAvailability_t)svInfo->getHasFix();
        svInfodata->Snr = svInfo->getSnr();
        svInfodata->elevation = svInfo->getElevation();
        svInfodata->azimuth = svInfo->getAzimuth();
        svInfodata->signalType = (uint32_t) svInfo->getSignalType();
        svInfodata->GlonassFcn = svInfo->getGlonassFcn();
        svInfodata->BasebandCnr = svInfo->getBasebandCnr();

        GnssSVInfo.push_back(svInfodata);
    }

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener3 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener3 = eventListener_;
    }
    if(locListener3 && locListener3->onGnssSVInfo){
        locListener3->onGnssSVInfo(locationId_,GnssSVInfo,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onGnssSVInfo");
    }
}

void LocationPAController::PALocationClient::onGnssSignalInfo(const std::shared_ptr<telux::loc::IGnssSignalInfo> &gnssDatainfo)
{
    TAF_PA_DEBUG( "**** onGnssSignalInfo Information -->PA****" );

    std::shared_ptr<taf_pa_location_GnssData_t> GnssDatainfo = std::make_shared<taf_pa_location_GnssData_t>();

    for(auto i=0;i<TAF_PA_GNSS_DATA_ARRAY_SIZE;i++){
        GnssDatainfo->gnssDataMask[i] = gnssDatainfo->getGnssData().gnssDataMask[i];
        GnssDatainfo->jammerInd[i] = gnssDatainfo->getGnssData().jammerInd[i];
        GnssDatainfo->agc[i] = gnssDatainfo->getGnssData().agc[i];
    }

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener4 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener4 = eventListener_;
    }
    if(locListener4 && locListener4->onGnssSignalInfo){
        locListener4->onGnssSignalInfo(locationId_,GnssDatainfo,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onGnssSignalInfo");
    }
}

void LocationPAController::PALocationClient::onXtraStatusUpdate(const telux::loc::XtraStatus xtraStatus)
{
    TAF_PA_DEBUG( "**** onXtraStatusUpdate Information -->PA****" );

    auto onXtraStatusUpdateData = std::make_shared<taf_pa_location_XtraStatus_t>();

    onXtraStatusUpdateData->featureEnabled = xtraStatus.featureEnabled;
    onXtraStatusUpdateData->xtraValidForHours = xtraStatus.xtraValidForHours;
    onXtraStatusUpdateData->xtraDataStatus = (taf_pa_location_XtraDataStatus_t) xtraStatus.xtraDataStatus;

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener5 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener5 = eventListener_;
    }
    if(locListener5 && locListener5->onXtraStatusUpdate){
        locListener5->onXtraStatusUpdate(locationId_,onXtraStatusUpdateData,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onXtraStatusUpdate");
    }
}

taf_pa_location_DgnssDataStatus_t LocationPAController::convertTeluxToDgnssDataStatus(DgnssStatus dgnssStatus)
{
    TAF_PA_INFO("convertTeluxToDgnssDataStatus");
    switch(dgnssStatus)
    {
        case DgnssStatus::DATA_SOURCE_NOT_SUPPORTED:
        {
            TAF_PA_DEBUG("telux::loc::DATA_SOURCE_NOT_SUPPORTED");
            return TAF_PA_LOCATION_DATA_SOURCE_NOT_SUPPORTED;
        }
        break;
        case DgnssStatus::DATA_FORMAT_NOT_SUPPORTED:
        {
            TAF_PA_DEBUG("telux::loc::DATA_FORMAT_NOT_SUPPORTED");
            return TAF_PA_LOCATION_DATA_FORMAT_NOT_SUPPORTED;
        }
        break;
        case DgnssStatus::OTHER_SOURCE_IN_USE:
        {
            TAF_PA_DEBUG("telux::loc::OTHER_SOURCE_IN_USE");
            return TAF_PA_LOCATION_OTHER_SOURCE_IN_USE;
        }
        break;
        case DgnssStatus::MESSAGE_PARSE_ERROR:
        {
            TAF_PA_DEBUG("telux::loc::MESSAGE_PARSE_ERROR");
            return TAF_PA_LOCATION_MESSAGE_PARSE_ERROR;
        }
        break;
        case DgnssStatus::DATA_SOURCE_USABLE:
        {
            TAF_PA_DEBUG("telux::loc::DATA_SOURCE_USABLE");
            return TAF_PA_LOCATION_DATA_SOURCE_USABLE;
        }
        break;
        case DgnssStatus::DATA_SOURCE_NOT_USABLE :
        {
            TAF_PA_DEBUG("telux::loc::DATA_SOURCE_NOT_USABLE");
            return TAF_PA_LOCATION_DATA_SOURCE_NOT_USABLE;
        }
        break;
        case DgnssStatus::CDFW_STOP_SOURCE_INJECT :
        {
            TAF_PA_DEBUG("telux::loc::CDFW_STOP_SOURCE_INJECT");
            return TAF_PA_LOCATION_CDFW_STOP_SOURCE_INJECT;
        }
        break;
        default:
            TAF_PA_INFO("Invalid Dgnss data status");
            return TAF_PA_LOCATION_DATA_SOURCE_NOT_SUPPORTED;
        break;
    }
}

void LocationPAController::onDgnssStatusUpdate(DgnssStatus status)
{
    TAF_PA_DEBUG( "**** onDgnssStatusUpdate Information -->PA****" );

    // Guard against calls during/after deregistration
    std::lock_guard<std::mutex> lock(mutex_);

    // Guard against calls during/after deregistration
    if (!mDgnssManager) {
        TAF_PA_INFO("Ignoring DGNSS status update - manager is null");
        return;
    }

    auto onDgnssStatusUpdateData = std::make_shared<taf_pa_location_DgnssStatus_t>();

    onDgnssStatusUpdateData->status = (taf_pa_location_DgnssDataStatus_t)convertTeluxToDgnssDataStatus(status);

    if(dgnssListener_ && dgnssListener_->onDgnssStatusUpdate){
        dgnssListener_->onDgnssStatusUpdate(onDgnssStatusUpdateData,dgnssListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onDgnssStatusUpdate");
    }
}

void LocationPAController::PALocationClient::onLocationSystemInfo(const telux::loc::LocationSystemInfo &locationSystemInfo)
{
    TAF_PA_DEBUG( "**** Location System Information ****" );
    TAF_PA_DEBUG( "**** Location System Information locationSystemInfoValidity:%d",locationSystemInfo.valid);
    TAF_PA_DEBUG( "**** Location System Information LeapSecondInfoValidity:%d",locationSystemInfo.info.valid);

    std::shared_ptr<taf_pa_location_LocationSystemInfo_t> locSysInfoEvent = std::make_shared<taf_pa_location_LocationSystemInfo_t>();

    locSysInfoEvent->validLocationSystemInfoMask = locationSystemInfo.valid;
    locSysInfoEvent->validLeapSecondSysInfoMask = locationSystemInfo.info.valid;
    locSysInfoEvent->current = locationSystemInfo.info.current;
    locSysInfoEvent->leapSecondsBeforeChange = locationSystemInfo.info.info.leapSecondsBeforeChange;
    locSysInfoEvent->leapSecondsAfterChange = locationSystemInfo.info.info.leapSecondsAfterChange;

    telux::loc::TimeInfo timeInfo = locationSystemInfo.info.info.timeInfo;
    locSysInfoEvent->timeinfo.validityMask = timeInfo.validityMask;
    locSysInfoEvent->timeinfo.systemWeek = timeInfo.systemWeek;
    locSysInfoEvent->timeinfo.systemMsec = timeInfo.systemMsec;
    locSysInfoEvent->timeinfo.systemClkTimeBias = timeInfo.systemClkTimeBias;
    locSysInfoEvent->timeinfo.systemClkTimeUncMs = timeInfo.systemClkTimeUncMs;
    locSysInfoEvent->timeinfo.refFCount = timeInfo.refFCount;
    locSysInfoEvent->timeinfo.numClockResets = timeInfo.numClockResets;

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener6 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener6 = eventListener_;
    }
    if(locListener6 && locListener6->onLocationSystemInfo){
        locListener6->onLocationSystemInfo(locationId_,locSysInfoEvent,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onLocationSystemInfo");
    }
}

void LocationPAController::PALocationClient::onDetailedEngineLocationUpdate(const std::vector<std::shared_ptr<telux::loc::ILocationInfoEx>> &locationEngineInfo)
{
    TAF_PA_DEBUG( "**** onDetailedEngineLocationUpdate Information -->PA****" );

    std::vector<std::shared_ptr<taf_pa_location_LocEngineInfo_t>> LocationEngineInfo;

    for(auto locationInfo:locationEngineInfo)
    {
        auto LocationEngineInfodata = std::make_shared<taf_pa_location_LocEngineInfo_t>();
        LocationEngineInfodata->latitude = locationInfo->getLatitude();
        LocationEngineInfodata->longitude = locationInfo->getLongitude();
        LocationEngineInfodata->altMeanSeaLevel = locationInfo->getAltitudeMeanSeaLevel();
        LocationEngineInfodata->timeStamp = locationInfo->getTimeStamp();
        LocationEngineInfodata->hUncertainity = locationInfo->getHorizontalUncertainty();
        LocationEngineInfodata->altitude = locationInfo->getAltitude();
        LocationEngineInfodata->vUncertainity = locationInfo->getVerticalUncertainty();
        LocationEngineInfodata->hSpeed = locationInfo->getSpeed();
        LocationEngineInfodata->hSpeedUncertainity = locationInfo->getSpeedUncertainty();
        std::vector<float> mVerticalSpeed;
        std::vector<float> mVerticalSpeedAccuracy;
        if(locationInfo->getVelocityEastNorthUp(mVerticalSpeed) == telux::common::Status::SUCCESS)
        {
            TAF_PA_DEBUG("mVerticalSpeed size: %ld", mVerticalSpeed.size());
            LocationEngineInfodata->verticalSpeed = mVerticalSpeed;
        }
        if(locationInfo->getVelocityUncertaintyEastNorthUp(mVerticalSpeedAccuracy) ==
                telux::common::Status::SUCCESS)
        {
            TAF_PA_DEBUG("mVerticalSpeedAccuracy size: %ld", mVerticalSpeedAccuracy.size());
            LocationEngineInfodata->verticalSpeedAccuracy = mVerticalSpeedAccuracy;
        }
        LocationEngineInfodata->magneticDeviation = locationInfo->getMagneticDeviation();
        LocationEngineInfodata->epochTime = locationInfo->getTimeStamp();
        LocationEngineInfodata->horUncEllipseSemiMajor =
                locationInfo->getHorizontalUncertaintySemiMajor();
        LocationEngineInfodata->horUncEllipseSemiMinor =
                locationInfo->getHorizontalUncertaintySemiMinor();
        LocationEngineInfodata->direction = locationInfo->getHeading();
        LocationEngineInfodata->directionAccuracy = locationInfo->getHeadingUncertainty();
        telux::loc::SystemTime sysTime = locationInfo->getGnssSystemTime();
        telux::loc::GnssSystem system = sysTime.gnssSystemTimeSrc;
        telux::loc::SystemTimeInfo sysTimeInfo = sysTime.time;
        if(system == telux::loc::GnssSystem::GNSS_LOC_SV_SYSTEM_GPS) {
            telux::loc::TimeInfo timeInfo = sysTimeInfo.gps;
            LocationEngineInfodata->gpsWeek = timeInfo.systemWeek;
            LocationEngineInfodata->gpsTimeOfWeek = timeInfo.systemMsec;
        }else {
            LocationEngineInfodata->gpsWeek = 0;
            LocationEngineInfodata->gpsTimeOfWeek = 0;
        }
        LocationEngineInfodata->timeAccuracy = locationInfo->getTimeUncMs();
        LocationEngineInfodata->hdop = locationInfo->getHorizontalDop();
        LocationEngineInfodata->vdop = locationInfo->getVerticalDop();
        LocationEngineInfodata->pdop = locationInfo->getPositionDop();
        LocationEngineInfodata->gdop = locationInfo->getGeometricDop();
        LocationEngineInfodata->tdop = locationInfo->getTimeDop();
        uint8_t mLeapSeconds = 0;
        if(locationInfo->getLeapSeconds(mLeapSeconds) ==
                    telux::common::Status::SUCCESS)
        {
            LocationEngineInfodata->leapSeconds = mLeapSeconds;
        } else {
            LocationEngineInfodata->leapSeconds = 0;
        }
        LocationEngineInfodata->leapSecondsUnc = locationInfo->getLeapSecondsUncertainty();
        LocationEngineInfodata->satsUsedCount = locationInfo->getNumSvUsed();
        LocationEngineInfodata->robustConformity = locationInfo->getConformityIndex();
        LocationEngineInfodata->confidencePercent = locationInfo->getCalibrationConfidencePercent();
        telux::loc::DrCalibrationStatus calibrationStatus = locationInfo->getCalibrationStatus();
        TAF_PA_DEBUG("onDetailedEngineLocationUpdate calibrationStatus %d",(int)calibrationStatus);
        LocationEngineInfodata->calibrationStatus = (taf_pa_location_DrCalibrationStatusType_t) calibrationStatus;
        telux::loc::DrSolutionStatus solutionStatus = locationInfo->getSolutionStatus();
        TAF_PA_DEBUG("DR solution status %d", (int)solutionStatus);
        LocationEngineInfodata->drSolutionStatus = (taf_pa_location_DrSolutionStatusType_t) solutionStatus;
        telux::loc::GnssKinematicsData GnssKinData = locationInfo->getBodyFrameData();
        telux::loc::KinematicDataValidity GnssKinDataValidity = GnssKinData.bodyFrameDataMask;
        TAF_PA_DEBUG("onDetailedEngineLocationUpdate GnssKinDataValidity:%0x", GnssKinDataValidity);
        LocationEngineInfodata->GnssKinematicsData.bodyFrameDataMask = (taf_pa_location_KinematicDataValidityType_t) GnssKinData.bodyFrameDataMask;
        LocationEngineInfodata->GnssKinematicsData.longAccel = GnssKinData.longAccel;
        LocationEngineInfodata->GnssKinematicsData.latAccel = GnssKinData.latAccel;
        LocationEngineInfodata->GnssKinematicsData.vertAccel = GnssKinData.vertAccel;
        LocationEngineInfodata->GnssKinematicsData.yawRate = GnssKinData.yawRate;
        LocationEngineInfodata->GnssKinematicsData.pitch = GnssKinData.pitch;
        LocationEngineInfodata->GnssKinematicsData.longAccelUnc = GnssKinData.longAccelUnc;
        LocationEngineInfodata->GnssKinematicsData.latAccelUnc = GnssKinData.latAccelUnc;
        LocationEngineInfodata->GnssKinematicsData.vertAccelUnc = GnssKinData.vertAccelUnc;
        LocationEngineInfodata->GnssKinematicsData.yawRateUnc = GnssKinData.yawRateUnc;
        LocationEngineInfodata->GnssKinematicsData.pitchUnc = GnssKinData.pitchUnc;
        LocationEngineInfodata->GnssKinematicsData.pitchRate = GnssKinData.pitchRate;
        LocationEngineInfodata->GnssKinematicsData.pitchRateUnc = GnssKinData.pitchRateUnc;
        LocationEngineInfodata->GnssKinematicsData.roll = GnssKinData.roll;
        LocationEngineInfodata->GnssKinematicsData.rollUnc = GnssKinData.rollUnc;
        LocationEngineInfodata->GnssKinematicsData.rollRate = GnssKinData.rollRate;
        LocationEngineInfodata->GnssKinematicsData.rollRateUnc = GnssKinData.rollRateUnc;
        LocationEngineInfodata->GnssKinematicsData.yaw = GnssKinData.yaw;
        LocationEngineInfodata->GnssKinematicsData.yawUnc = GnssKinData.yawUnc;

        LocationEngineInfodata->vrpLatitude = locationInfo->getVRPBasedLLA().latitude;
        LocationEngineInfodata->vrpLongitude = locationInfo->getVRPBasedLLA().longitude;
        LocationEngineInfodata->vrpAltitude = locationInfo->getVRPBasedLLA().altitude;
        std::vector<float> velData = locationInfo->getVRPBasedENUVelocity();
        if(velData.size() == 3){
            LocationEngineInfodata->eastVel = velData[0];
            LocationEngineInfodata->northVel = velData[1];
            LocationEngineInfodata->upVel = velData[2];
        }
        LocationEngineInfodata->svData.gps = locationInfo->getSvUsedInPosition().gps;
        LocationEngineInfodata->svData.glo = locationInfo->getSvUsedInPosition().glo;
        LocationEngineInfodata->svData.gal = locationInfo->getSvUsedInPosition().gal;
        LocationEngineInfodata->svData.bds = locationInfo->getSvUsedInPosition().bds;
        LocationEngineInfodata->svData.qzss = locationInfo->getSvUsedInPosition().qzss;
        LocationEngineInfodata->svData.navic = locationInfo->getSvUsedInPosition().navic;

        telux::loc::SbasCorrection correction = locationInfo->getSbasCorrection();
        std::string correctionString = correction.to_string();
        TAF_PA_DEBUG("Bits Count:%d",(int)correction.count());
        TAF_PA_DEBUG("correction:%s",correctionString.c_str());
        LocationEngineInfodata->sbasMask = correctionString;
        telux::loc::LocationInfoValidity validityMask = locationInfo->getLocationInfoValidity();
        TAF_PA_DEBUG("LocationInfoValidity->validityMask: %u ",validityMask);
        LocationEngineInfodata->validityMask = (taf_pa_location_LocationValidityType_t) validityMask;
        telux::loc::LocationInfoExValidity validityExMask = locationInfo->getLocationInfoExValidity();
        TAF_PA_DEBUG("LocationInfoExValidity->validityExMask: %d", (int)validityExMask);
        LocationEngineInfodata->validityExMask = (taf_pa_location_LocationInfoExValidityType_t) validityExMask;
        telux::loc::PositioningEngine posEngineBits = locationInfo->getLocOutputEngMask();
        TAF_PA_DEBUG("posEngineBits: %d", (int)posEngineBits);
        LocationEngineInfodata->engMask = (taf_pa_location_PositioningEngineType_t) posEngineBits;
        telux::loc::LocationAggregationType locEngineType = locationInfo->getLocOutputEngType();
        TAF_PA_DEBUG("locEngineType: %d", (int)locEngineType);
        LocationEngineInfodata->locationEngType = (taf_pa_location_LocationAggregationType_t) locEngineType;
        telux::loc::LocationReliability HorLocReliability = locationInfo->getHorizontalReliability();
        TAF_PA_DEBUG("HorLocReliability: %d",(int)HorLocReliability);
        LocationEngineInfodata->horiReliablity = (taf_pa_location_LocationReliability_t) HorLocReliability;
        telux::loc::LocationReliability vertLocReliability = locationInfo->getVerticalReliability();
        TAF_PA_DEBUG("vertLocReliability: %d",(int)vertLocReliability);
        LocationEngineInfodata->vertReliablity = (taf_pa_location_LocationReliability_t) vertLocReliability;
        LocationEngineInfodata->azimuth = locationInfo->getHorizontalUncertaintyAzimuth();
        LocationEngineInfodata->eastDev = locationInfo->getEastStandardDeviation();
        LocationEngineInfodata->northDev = locationInfo->getNorthStandardDeviation();
        LocationEngineInfodata->realTime = locationInfo->getElapsedRealTime();
        LocationEngineInfodata->realTimeUnc = locationInfo->getElapsedRealTimeUncertainty();
        telux::loc::LocationTechnology techMask = locationInfo->getTechMask();
        TAF_PA_DEBUG("techMask: %d",(int)techMask);
        LocationEngineInfodata->techMask = (taf_pa_location_LocationTechnologyType_t) techMask;
        LocationEngineInfodata->gPtpTime = locationInfo->getElapsedGptpTime();
        LocationEngineInfodata->gPtpTimeUnc = locationInfo->getElapsedGptpTimeUnc();

        std::vector<uint16_t> SVIds;
        locationInfo->getSVIds(SVIds);
        LocationEngineInfodata->SVIdData = SVIds;

        LocationEngineInfodata->posTechnology = (taf_pa_location_GnssPositionTechType_t)locationInfo->getPositionTechnology();
        LocationEngineInfodata->mAltType = (taf_pa_location_AltitudeType_t)locationInfo->getAltitudeType();
        LocationEngineInfodata->reportStatus = (taf_pa_location_ReportStatus_t) locationInfo->getReportStatus();
        telux::loc::NavigationSolution navSol = locationInfo->getNavigationSolution();
        if (navSol.to_ulong() != 0) {
            LocationEngineInfodata->naviSolution = (taf_pa_location_NavigationSolutionType_t)convertTeluxToNavigationSolutionType(navSol);
            TAF_PA_DEBUG("LocationEngineInfodata->naviSolution :%lu",LocationEngineInfodata->naviSolution);
        } else {
            TAF_PA_ERROR("Received invalid navigation solution from locationInfo, setting to 0");
            LocationEngineInfodata->naviSolution = (taf_pa_location_NavigationSolutionType_t)0;
        }
        TAF_PA_DEBUG("LocationEngineInfodata->naviSolution :%lu",LocationEngineInfodata->naviSolution);
        LocationEngineInfodata->dgnssStationIds = locationInfo->getDgnssStationIds();
        TAF_PA_DEBUG("LocationEngineInfodata->dgnssStationIds : %u",
                 LocationEngineInfodata->dgnssStationIds);
        LocationEngineInfodata->integrityRiskUsed = locationInfo->getIntegrityRiskUsed();
        TAF_PA_DEBUG("LocationEngineInfodata->integrityRiskUsed -> %d",
                 LocationEngineInfodata->integrityRiskUsed);
        LocationEngineInfodata->protectionlevelAlongTrack = locationInfo->getProtectionLevelAlongTrack();
        TAF_PA_DEBUG("LocationEngineInfodata->protectionlevelAlongTrackd -> %lf",
                 LocationEngineInfodata->protectionlevelAlongTrack);
        LocationEngineInfodata->protectionlevelCrossTrack = locationInfo->getProtectionLevelCrossTrack();
        TAF_PA_DEBUG(" LocationEngineInfodata->protectionlevelCrossTrack -> %lf", LocationEngineInfodata->protectionlevelCrossTrack);
        LocationEngineInfodata->protectionlevelVertical = locationInfo->getProtectionLevelVertical();
        TAF_PA_DEBUG("LocationEngineInfodata->protectionlevelVertical -> %lf",
                 LocationEngineInfodata->protectionlevelVertical);
        LocationEngineInfodata->ageOfCorrections = locationInfo->getAgeOfCorrections();
        TAF_PA_DEBUG(" LocationEngineInfodata->ageOfCorrections -> %lu",
                 LocationEngineInfodata->ageOfCorrections);
        LocationEngineInfodata->baselineLength = locationInfo->getBaselineLength();
        TAF_PA_DEBUG("LocationEngineInfodata->baselineLength -> %lf",
                 LocationEngineInfodata->baselineLength);
        std::vector<telux::loc::GnssMeasurementInfo> measInfo = locationInfo->getmeasUsageInfo();
        for (const auto &info : measInfo) {
            taf_pa_location_GnssMeasurementInfo_t converted;
            converted.gnssSignalType = static_cast<taf_pa_location_GnssSignalType_t>(info.gnssSignalType);
            converted.gnssConstellation = static_cast<taf_pa_location_GnssSystem_t>(info.gnssConstellation);
            converted.gnssSvId = static_cast<uint16_t>(info.gnssSvId);

            LocationEngineInfodata->measInfoData.push_back(converted);
        }

        LocationEngineInfo.push_back(LocationEngineInfodata);
    }

    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener7 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener7 = eventListener_;
    }
    if(locListener7 && locListener7->onDetailedEngineLocationUpdate){
        locListener7->onDetailedEngineLocationUpdate(locationId_, LocationEngineInfo, eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onEvent");
    }

}

taf_pa_result_t LocationPAController::deinitialize()
{
    TAF_PA_INFO("Starting location PA deinitialization...");

    // Step 1: Release all location clients. Each ReleaseLocationClient() call removes the
    // client from the map and calls CleanUp() which deregisters SDK listeners.
    TAF_PA_INFO("Releasing all location clients...");
    {
        // Collect IDs first to avoid iterator invalidation during erasure.
        std::vector<taf_pa_location_LocationId> clientIds;
        clientIds.reserve(locationClients_.size());
        for (auto& entry : locationClients_)
        {
            clientIds.push_back(entry.first);
        }
        for (auto id : clientIds)
        {
            ReleaseLocationClient(id);
        }
    }

    // Step 2: Deinitialize DGNSS manager if it is still active.
    if (mDgnssManager)
    {
        TAF_PA_INFO("Deinitializing DGNSS manager during deinit...");
        DeInitializeDgnss(nullptr, std::any{});
    }

    // Step 3: Clear DGNSS event listener and context under mutex so no further
    // callbacks are dispatched after this point.
    TAF_PA_INFO("Clearing dgnssListener_ and dgnssListenerContext_");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        dgnssListener_ = nullptr;
        dgnssListenerContext_.reset();
    }

    // Step 4: Reset location configurator shared pointer.
    TAF_PA_INFO("Resetting mLocationConfigurator");
    mLocationConfigurator.reset();

    TAF_PA_INFO("Location PA deinitialization complete.");
    return TAF_PA_OK;
}

taf_pa_result_t tafpa::location::taf_pa_location_Deinit()
{
    TAF_PA_DEBUG("PA implementation.");

    // Check if initialized before attempting deinit
    if (!gLocationPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return TAF_PA_FAULT;
    }

    auto paCtrl = LocationPAController::getInstance();
    taf_pa_result_t res = paCtrl->deinitialize();
    if (res == TAF_PA_OK)
    {
        gLocationPaInitialized.store(false, std::memory_order_release);
        TAF_PA_INFO("Location platform adaptor initialization flag reset to false.");
        TAF_PA_INFO("Location Platform adapter deinitialization done.");
    }
    else
    {
        TAF_PA_ERROR("Location Platform adapter deinitialization failed.");
    }
    return res;
}

void LocationPAController::PALocationClient::onGnssMeasurementsInfo(const telux::loc::GnssMeasurements &measurementInfo)
{
    TAF_PA_DEBUG( "**** onGnssMeasurementsInfo Information -->PA****" );

    std::shared_ptr<taf_pa_location_GnssMeasurements_t> measurementsInfo = std::make_shared<taf_pa_location_GnssMeasurements_t>();

    measurementsInfo->agcStatusL1 = (taf_pa_location_AgcStatus_t)measurementInfo.agcStatusL1;
    measurementsInfo->agcStatusL2 = (taf_pa_location_AgcStatus_t)measurementInfo.agcStatusL2;
    measurementsInfo->agcStatusL5 = (taf_pa_location_AgcStatus_t)measurementInfo.agcStatusL5;
    measurementsInfo->isNHz = measurementInfo.isNHz;


    measurementsInfo->clock.valid = (taf_pa_location_GnssMeasurementsClockValidityType_t)measurementInfo.clock.valid;
    measurementsInfo->clock.leapSecond = measurementInfo.clock.leapSecond;
    measurementsInfo->clock.timeNs = measurementInfo.clock.timeNs;
    measurementsInfo->clock.timeUncertaintyNs = measurementInfo.clock.timeUncertaintyNs;
    measurementsInfo->clock.fullBiasNs = measurementInfo.clock.fullBiasNs;
    measurementsInfo->clock.biasNs = measurementInfo.clock.biasNs;
    measurementsInfo->clock.biasUncertaintyNs = measurementInfo.clock.biasUncertaintyNs;
    measurementsInfo->clock.driftNsps = measurementInfo.clock.driftNsps;
    measurementsInfo->clock.driftUncertaintyNsps = measurementInfo.clock.driftUncertaintyNsps;
    measurementsInfo->clock.hwClockDiscontinuityCount = measurementInfo.clock.hwClockDiscontinuityCount;
    measurementsInfo->clock.elapsedRealTime = measurementInfo.clock.elapsedRealTime;
    measurementsInfo->clock.elapsedRealTimeUnc = measurementInfo.clock.elapsedRealTimeUnc;
    measurementsInfo->clock.elapsedgPTPTime = measurementInfo.clock.elapsedgPTPTime;
    measurementsInfo->clock.elapsedgPTPTimeUnc = measurementInfo.clock.elapsedgPTPTimeUnc;

    std::vector<telux::loc::GnssMeasurementsData> measData = measurementInfo.measurements;

    TAF_PA_DEBUG("measData.size() : %d", (int)measData.size());

    measurementsInfo->measurements.clear();
    measurementsInfo->measurements.reserve(measData.size());

    for (const auto& in : measData)
    {
        taf_pa_location_GnssMeasurementsData_t outItem{};

        outItem.valid                          = (taf_pa_location_GnssMeasurementsDataValidityType_t)in.valid;
        outItem.svId                           = in.svId;
        outItem.svType                         = (taf_pa_location_GnssConstellationType_t)in.svType;
        outItem.timeOffsetNs                   = in.timeOffsetNs;
        outItem.stateMask                      = (taf_pa_location_GnssMeasurementsStateValidityType_t)in.stateMask;
        outItem.receivedSvTimeNs               = in.receivedSvTimeNs;
        outItem.receivedSvTimeSubNs            = in.receivedSvTimeSubNs;
        outItem.receivedSvTimeUncertaintyNs    = in.receivedSvTimeUncertaintyNs;
        outItem.carrierToNoiseDbHz             = in.carrierToNoiseDbHz;
        outItem.pseudorangeRateMps             = in.pseudorangeRateMps;
        outItem.pseudorangeRateUncertaintyMps  = in.pseudorangeRateUncertaintyMps;
        outItem.adrStateMask                   = (taf_pa_location_GnssMeasurementsAdrStateValidityType_t)in.adrStateMask;
        outItem.adrMeters                      = in.adrMeters;
        outItem.adrUncertaintyMeters           = in.adrUncertaintyMeters;
        outItem.carrierFrequencyHz             = in.carrierFrequencyHz;
        outItem.carrierCycles                  = in.carrierCycles;
        outItem.carrierPhase                   = in.carrierPhase;
        outItem.carrierPhaseUncertainty        = in.carrierPhaseUncertainty;
        outItem.multipathIndicator             = (taf_pa_location_GnssMeasurementsMultipathIndicator_t)in.multipathIndicator;
        outItem.signalToNoiseRatioDb           = in.signalToNoiseRatioDb;
        outItem.agcLevelDb                     = in.agcLevelDb;
        outItem.gnssSignalType                 = in.gnssSignalType;
        outItem.basebandCarrierToNoise         = in.basebandCarrierToNoise;
        outItem.fullInterSignalBias            = in.fullInterSignalBias;
        outItem.fullInterSignalBiasUncertainty = in.fullInterSignalBiasUncertainty;
        measurementsInfo->measurements.push_back(outItem);
    }


    // snapshot eventListener_ under mtx_ before invoking outside the lock.
    taf_pa_location_EventListener* locListener8 = nullptr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        locListener8 = eventListener_;
    }
    if(locListener8 && locListener8->onGnssMeasurementsInfo){
        locListener8->onGnssMeasurementsInfo(locationId_,measurementsInfo,eventListenerContext_);
    }
    else{
        TAF_PA_ERROR("unable to find event Listener for onGnssMeasurementsInfo");
    }

}
