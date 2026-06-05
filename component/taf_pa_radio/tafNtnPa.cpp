/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <future>

#include <telux/common/CommonDefines.hpp>
#include <telux/common/Utils.hpp>
#include <telux/satcom/SatcomFactory.hpp>
#include <telux/satcom/NtnManager.hpp>
#include <telux/tel/PhoneFactory.hpp>
#include <telux/tel/SubscriptionManager.hpp>

#include "tafNtnPa.hpp"

using namespace std;
using namespace telux;

namespace NtnPa {

#define SERVICE_TIMEOUT 5

#define REQUEST_TIMEOUT 5

#define NTN_MAX_INSTANCE 2

#define SERVICE_PROMISE_AND_CALLBACK(name)                                \
    auto name##Promise = make_shared<promise<common::ServiceStatus>>();   \
    auto name##Callback = [name##Promise](common::ServiceStatus status)   \
    {                                                                     \
        try                                                               \
        {                                                                 \
            name##Promise->set_value(status);                             \
        }                                                                 \
        catch (const future_error& e)                                     \
        {                                                                 \
            PA_ERROR("Future error in %s callback: %s", #name, e.what()); \
        }                                                                 \
        catch (const exception& e)                                        \
        {                                                                 \
            PA_ERROR("Exception in %s callback: %s", #name, e.what());    \
        }                                                                 \
        catch (...)                                                       \
        {                                                                 \
            PA_ERROR("Unknown error in %s callback.", #name);             \
        }                                                                 \
    };

#define SERVICE_READY(name, manager)                                             \
    future<common::ServiceStatus> name##Future = name##Promise->get_future();    \
    future_status name##Status = name##Future.wait_for(                          \
        chrono::seconds(SERVICE_TIMEOUT));                                       \
    common::ServiceStatus name##ServiceStatus;                                   \
    if (future_status::timeout == name##Status)                                  \
    {                                                                            \
        PA_CRIT("Timeout for %s.", #name);                                       \
        manager = nullptr;                                                       \
    }                                                                            \
    else                                                                         \
    {                                                                            \
        name##ServiceStatus = name##Future.get();                                \
        if (name##ServiceStatus != common::ServiceStatus::SERVICE_AVAILABLE)     \
        {                                                                        \
            PA_CRIT("%s is not available.", #name);                              \
            manager = nullptr;                                                   \
        }                                                                        \
        else                                                                     \
            PA_INFO("%s is available.", #name);                                  \
    }

typedef struct
{
    void* handlerFuncPtr;
    void* contextPtr;
} Handler_t;

typedef struct
{
    shared_ptr<satcom::INtnManager> ntnManager;
    shared_ptr<tel::ISubscriptionManager> subMgr;
} Manager_t;

class BaseCallback
{
    public:
        sem_t semaphore;
        pa_result_t result;

        BaseCallback(void) : result(0)
        {
            sem_init(&semaphore, 0, 0);
        }

        ~BaseCallback(void)
        {
            sem_destroy(&semaphore);
        }
};

class RequestCallback : public BaseCallback
{
    public:
        bool isNtnSupported;
        satcom::NtnCapabilities ntnCapabilities;
        satcom::SignalStrength signalStrength;
        satcom::NtnState ntnState;
};

class NtnListener : public satcom::INtnListener
{
    public:
        void onSignalStrengthChange(satcom::SignalStrength signalStrength) override;
        void onNtnStateChange(satcom::NtnState newState) override;
        void onDataAck(telux::common::ErrorCode err, satcom::TransactionId id) override;
        void onIncomingData(std::unique_ptr<uint8_t[]> data, uint32_t size) override;
        void onCapabilitiesChange(satcom::NtnCapabilities capabilities) override;
        void onServiceStatusChange(telux::common::ServiceStatus status) override;
        void onCellularCoverageAvailable(bool isCellularCoverageAvailable) override;
        void onLocationFixRequest(satcom::LocationFixRequestReason reqReason) override;
        void onNtnBandUpdate(uint32_t bandValue) override;
};

typedef struct
{
    Handler_t signalStrengthChange;
    Handler_t ntnStateChange;
    Handler_t dataAck;
    Handler_t incomingData;
    Handler_t capabilitiesChange;
    Handler_t serviceStatusChange;
    Handler_t cellularCoverageAvailable;
    Handler_t locationFixRequest;
    Handler_t ntnBandUpdate;
} Indicator_t;

typedef struct
{
    shared_ptr<RequestCallback> request;
} Callback_t;

typedef struct
{
    shared_ptr<NtnListener> ntnListener;
} Listener_t;

class PlatformAdaptor
{
    public:
        Indicator_t indicators;
        Callback_t callbacks;
        Manager_t managers;
        Listener_t listeners;

        static PlatformAdaptor& GetInstance(void);
};

PlatformAdaptor& PlatformAdaptor::GetInstance(void)
{
    static PlatformAdaptor instance;
    return instance;
}

void NtnListener::onSignalStrengthChange(satcom::SignalStrength signalStrength)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("onSignalStrengthChange is triggered");
    taf_pa_ntn_SignalStrengthChangeHdlrFunc_t handlerFunc =
        (taf_pa_ntn_SignalStrengthChangeHdlrFunc_t)pa.indicators.signalStrengthChange.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_SignalStrengthChangeIndication_t indication;
        indication.signalStrength = static_cast<taf_pa_ntn_SignalStrength_t>(signalStrength);
        handlerFunc(indication, pa.indicators.signalStrengthChange.contextPtr);
    }
}

void NtnListener::onNtnStateChange(satcom::NtnState newState)
{
    PA_INFO("onNtnStateChange is triggered");
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_NtnStateChangeHdlrFunc_t handlerFunc =
        (taf_pa_ntn_NtnStateChangeHdlrFunc_t)pa.indicators.ntnStateChange.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_NtnStateChangeIndication_t indication;
        indication.ntnState = static_cast<taf_pa_ntn_State_t>(newState);
        PA_INFO("onNtnStateChange->handlerFunc");
        handlerFunc(indication, pa.indicators.ntnStateChange.contextPtr);
    }
}

void NtnListener::onDataAck(telux::common::ErrorCode err, satcom::TransactionId id)
{
    PA_INFO("onDataAck is triggered");
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_DataAckHdlrFunc_t handlerFunc =
        (taf_pa_ntn_DataAckHdlrFunc_t)pa.indicators.dataAck.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_DataAckIndication_t indication;
        indication.errorCode = (err == telux::common::ErrorCode::SUCCESS) ? PA_OK : PA_FAULT;
        indication.transactionId = id;
        PA_INFO("onDataAck->handlerFunc, transactionId: %lu, errorCode: %d", id, indication.errorCode);
        handlerFunc(indication, pa.indicators.dataAck.contextPtr);
    }
}

void NtnListener::onIncomingData(std::unique_ptr<uint8_t[]> data, uint32_t size)
{
    PA_INFO("onIncomingData is triggered, size: %u", size);
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_IncomingDataHdlrFunc_t handlerFunc =
        (taf_pa_ntn_IncomingDataHdlrFunc_t)pa.indicators.incomingData.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_IncomingDataIndication_t indication;
        indication.data = data.get();
        indication.size = size;
        PA_INFO("onIncomingData->handlerFunc");
        handlerFunc(indication, pa.indicators.incomingData.contextPtr);
    }
}

void NtnListener::onCapabilitiesChange(satcom::NtnCapabilities capabilities)
{
    PA_INFO("onCapabilitiesChange is triggered");
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_CapabilitiesChangeHdlrFunc_t handlerFunc =
        (taf_pa_ntn_CapabilitiesChangeHdlrFunc_t)pa.indicators.capabilitiesChange.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_CapabilitiesChangeIndication_t indication;
        indication.capabilities.maxDataSize = capabilities.maxDataSize;
        PA_INFO("onCapabilitiesChange->handlerFunc, maxDataSize: %ld", capabilities.maxDataSize);
        handlerFunc(indication, pa.indicators.capabilitiesChange.contextPtr);
    }
}

void NtnListener::onServiceStatusChange(telux::common::ServiceStatus status)
{
    PA_INFO("onServiceStatusChange is triggered");
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_ServiceStatusChangeHdlrFunc_t handlerFunc =
        (taf_pa_ntn_ServiceStatusChangeHdlrFunc_t)pa.indicators.serviceStatusChange.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_ServiceStatusChangeIndication_t indication;
        switch (status)
        {
            case common::ServiceStatus::SERVICE_AVAILABLE:
                indication.status = TAF_PA_NTN_SERVICE_STATUS_AVAILABLE;
                break;
            case common::ServiceStatus::SERVICE_UNAVAILABLE:
                indication.status = TAF_PA_NTN_SERVICE_STATUS_UNAVAILABLE;
                break;
            case common::ServiceStatus::SERVICE_FAILED:
                indication.status = TAF_PA_NTN_SERVICE_STATUS_FAILED;
                break;
            default:
                indication.status = TAF_PA_NTN_SERVICE_STATUS_UNKNOWN;
                break;
        }
        PA_INFO("onServiceStatusChange->handlerFunc, status: %d", indication.status);
        handlerFunc(indication, pa.indicators.serviceStatusChange.contextPtr);
    }
}

void NtnListener::onCellularCoverageAvailable(bool isCellularCoverageAvailable)
{
    PA_INFO("onCellularCoverageAvailable is triggered: %d", isCellularCoverageAvailable);
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_CellularCoverageAvailableHdlrFunc_t handlerFunc =
        (taf_pa_ntn_CellularCoverageAvailableHdlrFunc_t)pa.indicators.cellularCoverageAvailable.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_CellularCoverageAvailableIndication_t indication;
        indication.isCellularCoverageAvailable = isCellularCoverageAvailable;
        PA_INFO("onCellularCoverageAvailable->handlerFunc");
        handlerFunc(indication, pa.indicators.cellularCoverageAvailable.contextPtr);
    }
}

void NtnListener::onLocationFixRequest(satcom::LocationFixRequestReason reqReason)
{
    PA_INFO("onLocationFixRequest is triggered, reason: %d", static_cast<int>(reqReason));
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_LocationFixRequestHdlrFunc_t handlerFunc =
        (taf_pa_ntn_LocationFixRequestHdlrFunc_t)pa.indicators.locationFixRequest.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_LocationFixRequestIndication_t indication;
        indication.reason = static_cast<taf_pa_ntn_LocationFixRequestReason_t>(reqReason);
        PA_INFO("onLocationFixRequest->handlerFunc");
        handlerFunc(indication, pa.indicators.locationFixRequest.contextPtr);
    }
}

void NtnListener::onNtnBandUpdate(uint32_t bandValue)
{
    PA_INFO("onNtnBandUpdate is triggered, bandValue: %u", bandValue);
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_ntn_NtnBandUpdateHdlrFunc_t handlerFunc =
        (taf_pa_ntn_NtnBandUpdateHdlrFunc_t)pa.indicators.ntnBandUpdate.handlerFuncPtr;

    if (handlerFunc != nullptr)
    {
        taf_pa_ntn_NtnBandUpdateIndication_t indication;
        indication.bandValue = bandValue;
        PA_INFO("onNtnBandUpdate->handlerFunc");
        handlerFunc(indication, pa.indicators.ntnBandUpdate.contextPtr);
    }
}

class Utility
{
    public:
        class Convert
        {
            public:
                //------------------------------------------------------------------
                /**
                 * Converts a 0-based instance value to a 1-based phone/slot ID,
                 * matching the convention used by telux::tel::ISubscriptionManager
                 * ::getSubscription() and the tafRadioPa InstanceToPhone helper.
                 *
                 * @param instance  0-based instance index.
                 * @return          1-based phone/slot ID, or -1 if out of range.
                 */
                //------------------------------------------------------------------
                static int InstanceToPhone
                (
                    uint32_t instance
                )
                {
                    if (instance >= NTN_MAX_INSTANCE)
                    {
                        PA_ERROR("InstanceToPhone: invalid instance %u (max %d)",
                                 instance, NTN_MAX_INSTANCE);
                        return -1;
                    }
                    return static_cast<int>(instance + 1);
                }
        };
};

} // namespace NtnPa

pa_result_t taf_pa_ntn_Init()
{
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    auto& satcomFactory = satcom::SatcomFactory::getInstance();

    pa.callbacks.request = make_shared<NtnPa::RequestCallback>();

    SERVICE_PROMISE_AND_CALLBACK(ntnManager)
    pa.managers.ntnManager = satcomFactory.getNtnManager(ntnManagerCallback);
    SERVICE_READY(ntnManager, pa.managers.ntnManager)

    pa.listeners.ntnListener = make_shared<NtnPa::NtnListener>();

    // Register listener
    PA_INFO("Registering NTN listener");
    if (pa.managers.ntnManager != nullptr &&
        pa.listeners.ntnListener != nullptr &&
        pa.managers.ntnManager->getServiceStatus() == common::ServiceStatus::SERVICE_AVAILABLE)
    {
        common::Status status = pa.managers.ntnManager->registerListener(pa.listeners.ntnListener);
        if (status != common::Status::SUCCESS)
        {
            PA_WARN("Failed to register NTN listener during initialization");
        }
    }
    else
    {
        PA_WARN("Skipping NTN listener registration - manager not available");
    }

    // Initialize subscription manager for ICCID retrieval in EnableNtn
    PA_INFO("Initializing subscription manager for ICCID retrieval");
    {
        auto& phoneFactory = tel::PhoneFactory::getInstance();
        pa.managers.subMgr = phoneFactory.getSubscriptionManager();
        if (pa.managers.subMgr != nullptr)
        {
            if (pa.managers.subMgr->getServiceStatus() != common::ServiceStatus::SERVICE_AVAILABLE)
            {
                PA_WARN("Subscription manager service not yet available during NTN init");
            }
            else
            {
                PA_INFO("Subscription manager initialized successfully");
            }
        }
        else
        {
            PA_WARN("Failed to get subscription manager during NTN init - ICCID fetch may fail");
        }
    }

    PA_INFO("NTN platform adaptor initialization is done.");

    return PA_OK;
}

pa_result_t taf_pa_ntn_Deinit()
{
    PA_INFO("Starting NTN platform adaptor deinitialization...");

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    // Clear all indicator handler function pointers and context pointers
    PA_INFO("Clearing all indicator handlers and contexts");
    pa.indicators.signalStrengthChange.handlerFuncPtr = nullptr;
    pa.indicators.signalStrengthChange.contextPtr = nullptr;
    pa.indicators.ntnStateChange.handlerFuncPtr = nullptr;
    pa.indicators.ntnStateChange.contextPtr = nullptr;
    pa.indicators.dataAck.handlerFuncPtr = nullptr;
    pa.indicators.dataAck.contextPtr = nullptr;
    pa.indicators.incomingData.handlerFuncPtr = nullptr;
    pa.indicators.incomingData.contextPtr = nullptr;
    pa.indicators.capabilitiesChange.handlerFuncPtr = nullptr;
    pa.indicators.capabilitiesChange.contextPtr = nullptr;
    pa.indicators.serviceStatusChange.handlerFuncPtr = nullptr;
    pa.indicators.serviceStatusChange.contextPtr = nullptr;
    pa.indicators.cellularCoverageAvailable.handlerFuncPtr = nullptr;
    pa.indicators.cellularCoverageAvailable.contextPtr = nullptr;
    pa.indicators.locationFixRequest.handlerFuncPtr = nullptr;
    pa.indicators.locationFixRequest.contextPtr = nullptr;
    pa.indicators.ntnBandUpdate.handlerFuncPtr = nullptr;
    pa.indicators.ntnBandUpdate.contextPtr = nullptr;

    // Deregister listener
    PA_INFO("Deregistering NTN listener");
    if (pa.managers.ntnManager != nullptr &&
        pa.listeners.ntnListener != nullptr &&
        pa.managers.ntnManager->getServiceStatus() == common::ServiceStatus::SERVICE_AVAILABLE)
    {
        pa.managers.ntnManager->deregisterListener(pa.listeners.ntnListener);
    }
    else
    {
        PA_WARN("Skipping NTN listener deregister - manager not available");
    }

    // Reset listener shared pointer
    PA_INFO("Resetting listener shared pointer");
    pa.listeners.ntnListener.reset();

    // Reset manager shared pointer
    PA_INFO("Resetting manager shared pointer");
    pa.managers.ntnManager.reset();

    // Reset request callback object
    PA_INFO("Resetting request callback");
    pa.callbacks.request.reset();

    // Reset subscription manager shared pointer
    PA_INFO("Resetting subscription manager shared pointer");
    pa.managers.subMgr.reset();

    PA_INFO("NTN platform adaptor deinitialization complete.");
    return PA_OK;
}

pa_result_t taf_pa_ntn_IsNtnSupported
(
    uint32_t instance,
    bool* isSupportedPtr
)
{
    PA_INFO("taf_pa_ntn_IsNtnSupported");
    if (isSupportedPtr == nullptr)
    {
        PA_ERROR("isSupportedPtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    bool isSupported = false;
    common::ErrorCode error = pa.managers.ntnManager->isNtnSupported(isSupported);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to check NTN support: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    *isSupportedPtr = isSupported;
    return PA_OK;
}

pa_result_t taf_pa_ntn_EnableNtn
(
    uint32_t instance,
    bool enable,
    bool isEmergency
)
{
    PA_INFO("taf_pa_ntn_EnableNtn instance=%u", instance);

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    // Fetch ICCID from telux::tel::ISubscription::getIccId() using
    // the instance parameter converted to a 1-based phone/slot ID.
    // The subscription manager was initialized in taf_pa_ntn_Init().
    string iccidStr;
    {
        int phoneId = NtnPa::Utility::Convert::InstanceToPhone(instance);
        if (phoneId < 0)
        {
            PA_WARN("taf_pa_ntn_EnableNtn: invalid phoneId %u, proceeding with empty ICCID",
                    phoneId);
        }
        else if (pa.managers.subMgr != nullptr &&
            pa.managers.subMgr->getServiceStatus() == common::ServiceStatus::SERVICE_AVAILABLE)
        {
            telux::common::Status subStatus;
            auto subscription = pa.managers.subMgr->getSubscription(phoneId, &subStatus);
            if (subscription != nullptr)
            {
                string fetchedIccid = subscription->getIccId();
                if (!fetchedIccid.empty())
                {
                    iccidStr = fetchedIccid;
                    PA_INFO("taf_pa_ntn_EnableNtn: ICCID fetched from subscription for "
                            "slot %u: %s", phoneId, iccidStr.c_str());
                }
                else
                {
                    PA_WARN("taf_pa_ntn_EnableNtn: getIccId() returned empty string for slot %u",
                            phoneId);
                }
            }
            else
            {
                PA_WARN("taf_pa_ntn_EnableNtn: subscription is null for slot %u", instance);
            }
        }
        else
        {
            PA_WARN("taf_pa_ntn_EnableNtn: subscription manager not available for slot %u",
                    phoneId);
        }
    }

    common::ErrorCode error = pa.managers.ntnManager->enableNtn(enable, isEmergency, iccidStr);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to enable/disable NTN: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_GetNtnCapabilities
(
    uint32_t instance,
    taf_pa_ntn_NtnCapabilities_t* capabilitiesPtr
)
{
    PA_INFO("taf_pa_ntn_GetNtnCapabilities");
    if (capabilitiesPtr == nullptr)
    {
        PA_ERROR("capabilitiesPtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    satcom::NtnCapabilities capabilities;
    common::ErrorCode error = pa.managers.ntnManager->getNtnCapabilities(capabilities);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to get NTN capabilities: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    capabilitiesPtr->maxDataSize = capabilities.maxDataSize;
    PA_INFO("taf_pa_ntn_GetNtnCapabilities capabilities.maxDataSize:%d",capabilities.maxDataSize);
    return PA_OK;
}

pa_result_t taf_pa_ntn_GetSignalStrength
(
    uint32_t instance,
    taf_pa_ntn_SignalStrength_t* signalStrengthPtr
)
{
    PA_INFO("taf_pa_ntn_GetSignalStrength");
    if (signalStrengthPtr == nullptr)
    {
        PA_ERROR("signalStrengthPtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    satcom::SignalStrength signalStrength;
    common::ErrorCode error = pa.managers.ntnManager->getSignalStrength(signalStrength);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to get signal strength: %s", 
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    *signalStrengthPtr = static_cast<taf_pa_ntn_SignalStrength_t>(signalStrength);
    PA_INFO("taf_pa_ntn_GetSignalStrength *signalStrengthPtr:%d",static_cast<int>(signalStrength));
    return PA_OK;
}

pa_result_t taf_pa_ntn_GetNtnState
(
    uint32_t instance,
    taf_pa_ntn_State_t* statePtr
)
{
    PA_INFO("taf_pa_ntn_GetNtnState");
    if (statePtr == nullptr)
    {
        PA_ERROR("statePtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    satcom::NtnState state = pa.managers.ntnManager->getNtnState();
    *statePtr = static_cast<taf_pa_ntn_State_t>(state);
    PA_INFO("taf_pa_ntn_GetNtnState *statePtr:%d",static_cast<taf_pa_ntn_State_t>(state));
    return PA_OK;
}

pa_result_t taf_pa_ntn_AddSignalStrengthChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_SignalStrengthChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_SignalStrengthChangeHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddSignalStrengthChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.signalStrengthChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.signalStrengthChange.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveSignalStrengthChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_SignalStrengthChangeHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveSignalStrengthChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.signalStrengthChange.handlerFuncPtr = nullptr;
    pa.indicators.signalStrengthChange.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddNtnStateChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnStateChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_NtnStateChangeHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddNtnStateChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.ntnStateChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.ntnStateChange.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveNtnStateChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnStateChangeHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveNtnStateChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.ntnStateChange.handlerFuncPtr = nullptr;
    pa.indicators.ntnStateChange.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddDataAckHandler
(
    uint32_t instance,
    taf_pa_ntn_DataAckHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_DataAckHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddDataAckHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.dataAck.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.dataAck.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveDataAckHandler
(
    uint32_t instance,
    taf_pa_ntn_DataAckHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveDataAckHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.dataAck.handlerFuncPtr = nullptr;
    pa.indicators.dataAck.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_SendData
(
    uint32_t instance,
    uint8_t* data,
    uint32_t size,
    bool isEmergency,
    uint64_t* transactionIdPtr
)
{
    PA_INFO("taf_pa_ntn_SendData");
    if (data == nullptr)
    {
        PA_ERROR("data is nullptr.");
        return PA_BAD_PARAMETER;
    }

    if (transactionIdPtr == nullptr)
    {
        PA_ERROR("transactionIdPtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    if (size == 0)
    {
        PA_ERROR("size is 0.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    // Call the TelSDK sendData API
    // TransactionId is defined as unsigned long in TelSDK
    satcom::TransactionId transactionId = 0;
    common::Status status = pa.managers.ntnManager->sendData(data, size, isEmergency, transactionId);

    if (status != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to send data: status = %d", static_cast<int>(status));
        return PA_FAULT;
    }

    *transactionIdPtr = static_cast<uint64_t>(transactionId);
    PA_INFO("Data sent successfully. Transaction ID: %lu", transactionId);

    return 0;
}

pa_result_t taf_pa_ntn_EnableCellularScan
(
    uint32_t instance,
    bool enable
)
{
    PA_INFO("taf_pa_ntn_EnableCellularScan");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    common::ErrorCode error = pa.managers.ntnManager->enableCellularScan(enable);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to enable/disable cellular scan: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    PA_INFO("Cellular scan %s successfully", enable ? "enabled" : "disabled");
    return PA_OK;
}

pa_result_t taf_pa_ntn_UpdateSystemSelectionSpecifiers
(
    uint32_t instance,
    const taf_pa_ntn_SystemSelectionSpecifier_t* specifierPtr,
    uint32_t specifierCount
)
{
    PA_INFO("taf_pa_ntn_UpdateSystemSelectionSpecifiers");

    if (specifierPtr == nullptr)
    {
        PA_ERROR("specifierPtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    if (specifierCount == 0)
    {
        PA_ERROR("specifierCount is 0.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    // Convert to TelSDK structure
    vector<satcom::SystemSelectionSpecifier> teluxSpecifiers;

    for (uint32_t i = 0; i < specifierCount; i++)
    {
        const taf_pa_ntn_SystemSelectionSpecifier_t* currentSpecifier = &specifierPtr[i];

        if (currentSpecifier->mcc[0] == '\0' || currentSpecifier->mnc[0] == '\0')
        {
            PA_ERROR("mcc or mnc is empty for specifier %d.", i);
            return PA_BAD_PARAMETER;
        }

        satcom::SystemSelectionSpecifier spec;

        // Copy MCC and MNC
        spec.mcc = string(currentSpecifier->mcc);
        spec.mnc = string(currentSpecifier->mnc);

        // Copy NTN bands
        if (currentSpecifier->ntnBands != nullptr && currentSpecifier->ntnBandsLen > 0)
        {
            spec.ntnBands.assign(currentSpecifier->ntnBands,
                                 currentSpecifier->ntnBands + currentSpecifier->ntnBandsLen);
        }

        // Copy NTN EARFCNs
        if (currentSpecifier->ntnEarfcns != nullptr && currentSpecifier->ntnEarfcnsLen > 0)
        {
            spec.ntnEarfcns.assign(currentSpecifier->ntnEarfcns,
                                   currentSpecifier->ntnEarfcns + currentSpecifier->ntnEarfcnsLen);
        }

        teluxSpecifiers.push_back(spec);

        PA_INFO("Specifier[%d]: MCC=%s, MNC=%s, Bands=%d, EARFCNs=%d",
                i, spec.mcc.c_str(), spec.mnc.c_str(),
                spec.ntnBands.size(), spec.ntnEarfcns.size());
    }

    // Call TelSDK API
    common::ErrorCode error = pa.managers.ntnManager->updateSystemSelectionSpecifiers(teluxSpecifiers);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to update system selection specifiers: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    PA_INFO("System selection specifiers updated successfully");
    return PA_OK;
}

pa_result_t taf_pa_ntn_AbortData
(
    uint32_t instance
)
{
    PA_INFO("taf_pa_ntn_AbortData");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    common::ErrorCode error = pa.managers.ntnManager->abortData();

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to abort data: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    PA_INFO("Data aborted successfully");
    return PA_OK;
}

pa_result_t taf_pa_ntn_SetLocationFix
(
    uint32_t instance,
    const taf_pa_ntn_LocationFix_t* locationFixPtr
)
{
    PA_INFO("taf_pa_ntn_SetLocationFix");
    if (locationFixPtr == nullptr)
    {
        PA_ERROR("locationFixPtr is nullptr.");
        return PA_BAD_PARAMETER;
    }

    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    // Convert to TelSDK structure
    satcom::LocationFix locationFix;
    locationFix.lat = locationFixPtr->lat;
    locationFix.lon = locationFixPtr->lon;
    locationFix.alt = locationFixPtr->alt;
    locationFix.uncerCircular = locationFixPtr->uncerCircular;

    locationFix.velInfo.isEnuValueValid = locationFixPtr->velInfo.isEnuValueValid;
    for (int i = 0; i < TAF_PA_MAX_DIMENSIONS; i++)
    {
        locationFix.velInfo.enuVel[i] = locationFixPtr->velInfo.enuVel[i];
        locationFix.velInfo.enuUncer[i] = locationFixPtr->velInfo.enuUncer[i];
    }
    locationFix.velInfo.isEnuUncerValid = locationFixPtr->velInfo.isEnuUncerValid;

    locationFix.isHeadingValid = locationFixPtr->isHeadingValid;
    locationFix.heading = locationFixPtr->heading;
    locationFix.isHeadingUncerValid = locationFixPtr->isHeadingUncerValid;
    locationFix.headingUncer = locationFixPtr->headingUncer;
    locationFix.isConfidenceValid = locationFixPtr->isConfidenceValid;
    locationFix.confidence = locationFixPtr->confidence;

    common::ErrorCode error = pa.managers.ntnManager->setLocationFix(locationFix);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to set location fix: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    PA_INFO("Location fix set successfully");
    return PA_OK;
}

pa_result_t taf_pa_ntn_LocationFixResponse
(
    uint32_t instance,
    taf_pa_ntn_LocationStatus_t status,
    uint64_t waitTime
)
{
    PA_INFO("taf_pa_ntn_LocationFixResponse");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();
    if (pa.managers.ntnManager == nullptr)
    {
        PA_ERROR("Invalid NTN manager.");
        return PA_FAULT;
    }

    satcom::LocationStatus locationStatus = static_cast<satcom::LocationStatus>(status);
    common::ErrorCode error = pa.managers.ntnManager->locationFixResponse(locationStatus, waitTime);

    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to send location fix response: %s",
                 common::Utils::getErrorCodeAsString(error).c_str());
        return PA_FAULT;
    }

    PA_INFO("Location fix response sent successfully");
    return PA_OK;
}

pa_result_t taf_pa_ntn_AddIncomingDataHandler
(
    uint32_t instance,
    taf_pa_ntn_IncomingDataHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_IncomingDataHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddIncomingDataHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.incomingData.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.incomingData.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveIncomingDataHandler
(
    uint32_t instance,
    taf_pa_ntn_IncomingDataHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveIncomingDataHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.incomingData.handlerFuncPtr = nullptr;
    pa.indicators.incomingData.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddCapabilitiesChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_CapabilitiesChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_CapabilitiesChangeHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddCapabilitiesChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.capabilitiesChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.capabilitiesChange.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveCapabilitiesChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_CapabilitiesChangeHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveCapabilitiesChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.capabilitiesChange.handlerFuncPtr = nullptr;
    pa.indicators.capabilitiesChange.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddServiceStatusChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_ServiceStatusChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_ServiceStatusChangeHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddServiceStatusChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.serviceStatusChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.serviceStatusChange.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveServiceStatusChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_ServiceStatusChangeHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveServiceStatusChangeHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.serviceStatusChange.handlerFuncPtr = nullptr;
    pa.indicators.serviceStatusChange.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddCellularCoverageAvailableHandler
(
    uint32_t instance,
    taf_pa_ntn_CellularCoverageAvailableHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_CellularCoverageAvailableHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddCellularCoverageAvailableHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.cellularCoverageAvailable.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.cellularCoverageAvailable.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveCellularCoverageAvailableHandler
(
    uint32_t instance,
    taf_pa_ntn_CellularCoverageAvailableHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveCellularCoverageAvailableHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.cellularCoverageAvailable.handlerFuncPtr = nullptr;
    pa.indicators.cellularCoverageAvailable.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddLocationFixRequestHandler
(
    uint32_t instance,
    taf_pa_ntn_LocationFixRequestHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_LocationFixRequestHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddLocationFixRequestHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.locationFixRequest.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.locationFixRequest.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveLocationFixRequestHandler
(
    uint32_t instance,
    taf_pa_ntn_LocationFixRequestHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveLocationFixRequestHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.locationFixRequest.handlerFuncPtr = nullptr;
    pa.indicators.locationFixRequest.contextPtr = nullptr;

    return PA_OK;
}

pa_result_t taf_pa_ntn_AddNtnBandUpdateHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnBandUpdateHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_NtnBandUpdateHandlerRef_t* handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_AddNtnBandUpdateHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.ntnBandUpdate.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.ntnBandUpdate.contextPtr = contextPtr;

    if (handlerRefPtr != nullptr)
    {
        *handlerRefPtr = nullptr;
    }

    return PA_OK;
}

pa_result_t taf_pa_ntn_RemoveNtnBandUpdateHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnBandUpdateHandlerRef_t handlerRefPtr
)
{
    PA_INFO("taf_pa_ntn_RemoveNtnBandUpdateHandler");
    auto& pa = NtnPa::PlatformAdaptor::GetInstance();

    pa.indicators.ntnBandUpdate.handlerFuncPtr = nullptr;
    pa.indicators.ntnBandUpdate.contextPtr = nullptr;

    return PA_OK;
}
