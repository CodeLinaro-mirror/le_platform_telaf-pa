/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <time.h>
#include <errno.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <telux/tel/PhoneFactory.hpp>
#include <telux/common/CommonDefines.hpp>
#include "tafInternalCommonPa.h"
#include "tafRemotesimPa.hpp"

using namespace telux::tel;
using namespace telux::common;
using namespace std;

#define SERVICE_TIMEOUT 5
#define REQUEST_TIMEOUT 5
#define TAF_PA_REMOTESIM_SUBSYSTEM_TIMEOUT 30

static std::atomic<bool> g_remotesimPaInitialized(false);

// Callback class for RemoteSim operations with per-call context
class tafPaRemoteSimResponseCallback : public telux::common::ICommandResponseCallback {
public:
    tafPaRemoteSimResponseCallback(taf_pa_remotesim_ResponseCb cb, std::any ctx, std::promise<taf_pa_result_t>* prom = nullptr)
        : callback(cb), context(ctx), promise(prom) {}
    void commandResponse(telux::common::ErrorCode errorCode) override;
private:
    taf_pa_remotesim_ResponseCb callback;
    std::any context;
    std::promise<taf_pa_result_t>* promise;
};

// RemoteSim Listener for RemoteSimDaemon
class tafPaRemoteSimListener : public telux::tel::IRemoteSimListener {
public:
    tafPaRemoteSimListener() {};
    void onApduTransfer(const unsigned int id, const std::vector<uint8_t> &apdu) override;
    void onCardConnect() override;
    void onCardDisconnect() override;
    void onCardPowerUp() override;
    void onCardPowerDown() override;
    void onCardReset() override;
    void onServiceStatusChange(telux::common::ServiceStatus status) override;
};

// Platform Adaptor class
class PlatformAdaptor
{
public:
    std::shared_ptr<telux::tel::IRemoteSimManager> remoteSimMgr = nullptr;
    std::shared_ptr<telux::tel::IRemoteSimListener> remoteSimListener;
    std::shared_ptr<tafPaRemoteSimListener> tafRemoteSimListener;

    taf_pa_remotesim_EventListener* eventListener_ = nullptr;
    std::mutex eventListenerMutex_;
    int slotId = DEFAULT_SLOT_ID;

    static PlatformAdaptor& GetInstance();

    taf_pa_result_t RegisterEventListener(taf_pa_remotesim_EventListener* listener, std::any context);
    taf_pa_result_t MapErrorCode(telux::common::ErrorCode errorCode);

    PlatformAdaptor() = default;
    ~PlatformAdaptor() = default;
};

PlatformAdaptor& PlatformAdaptor::GetInstance()
{
    static PlatformAdaptor instance;
    return instance;
}

taf_pa_result_t PlatformAdaptor::RegisterEventListener(
    taf_pa_remotesim_EventListener* listener,
    std::any context)
{
    TAF_PA_INFO("RegisterEventListener listener: %p", listener);
    if (listener != nullptr) {
        std::lock_guard<std::mutex> lock(eventListenerMutex_);
        eventListener_ = listener;
    }
    else
    {
        return TAF_PA_BAD_PARAMETER;
    }
    return TAF_PA_OK;
}

taf_pa_result_t PlatformAdaptor::MapErrorCode(telux::common::ErrorCode errorCode)
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

// RemoteSim Response Callback implementation with per-call context
void tafPaRemoteSimResponseCallback::commandResponse(telux::common::ErrorCode errorCode)
{
    TAF_PA_INFO("RemoteSim callback: errorCode=%d", static_cast<int>(errorCode));
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_result_t result = pa.MapErrorCode(errorCode);

    if (promise)
    {
        promise->set_value(result);
    }

    if (callback)
    {
        TAF_PA_INFO("Invoking per-call callback with context");
        callback(result, context);
    }
}

// RemoteSimListener implementations
void tafPaRemoteSimListener::onApduTransfer(const unsigned int id, const std::vector<uint8_t> &apdu)
{
    TAF_PA_INFO("Received Apdu transfer notification from modem");
    auto& pa = PlatformAdaptor::GetInstance();

    auto apduEvent = std::make_shared<taf_pa_remotesim_ApduTransfer_t>();
    apduEvent->id = id;
    apduEvent->apdu = apdu;

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onApduTransfer)
    {
        TAF_PA_INFO("onApduTransfer triggered");
        listener->onApduTransfer(apduEvent);
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onApduTransfer");
    }
}

void tafPaRemoteSimListener::onCardConnect()
{
    TAF_PA_INFO("Received Card Connect notification from modem");
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onCardConnect)
    {
        TAF_PA_INFO("onCardConnect triggered");
        listener->onCardConnect();
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onCardConnect");
    }
}

void tafPaRemoteSimListener::onCardDisconnect()
{
    TAF_PA_INFO("Received Card Disconnect notification from modem");
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onCardDisconnect)
    {
        TAF_PA_INFO("onCardDisconnect triggered");
        listener->onCardDisconnect();
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onCardDisconnect");
    }
}

void tafPaRemoteSimListener::onCardPowerUp()
{
    TAF_PA_INFO("Received Card Power Up notification from modem");
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onCardPowerUp)
    {
        TAF_PA_INFO("onCardPowerUp triggered");
        listener->onCardPowerUp();
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onCardPowerUp");
    }
}

void tafPaRemoteSimListener::onCardPowerDown()
{
    TAF_PA_INFO("Received Card Power Down notification from modem");
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onCardPowerDown)
    {
        TAF_PA_INFO("onCardPowerDown triggered");
        listener->onCardPowerDown();
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onCardPowerDown");
    }
}

void tafPaRemoteSimListener::onCardReset()
{
    TAF_PA_INFO("Received Card Reset notification from modem");
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onCardReset)
    {
        TAF_PA_INFO("onCardReset triggered");
        listener->onCardReset();
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onCardReset");
    }
}

void tafPaRemoteSimListener::onServiceStatusChange(ServiceStatus status)
{
    auto& pa = PlatformAdaptor::GetInstance();

    auto statusEvent = std::make_shared<taf_pa_remotesim_ServiceStatus_t>();
    statusEvent->status = (status == ServiceStatus::SERVICE_AVAILABLE) ? 
                          TAF_PA_REMOTESIM_SERVICE_AVAILABLE : 
                          TAF_PA_REMOTESIM_SERVICE_UNAVAILABLE;

    taf_pa_remotesim_EventListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        listener = pa.eventListener_;
    }

    if(listener && listener->onServiceStatusChange)
    {
        TAF_PA_INFO("onServiceStatusChange triggered");
        listener->onServiceStatusChange(statusEvent);
    }
    else
    {
        TAF_PA_ERROR("unable to find event Listener for onServiceStatusChange");
    }
}

// PA API implementations
taf_pa_result_t taf_pa_remotesim_Init(int slotId)
{
    TAF_PA_INFO("taf_pa_remotesim_Init with slot ID: %d", slotId);
    auto& pa = PlatformAdaptor::GetInstance();
    auto& phoneFactory = telux::tel::PhoneFactory::getInstance();

    // Store the slot ID
    pa.slotId = slotId;

    // Initialize RemoteSimManager
    TAF_PA_INFO("Initializing RemoteSimManager...");
    pa.remoteSimMgr = phoneFactory.getRemoteSimManager(slotId);

    if (!pa.remoteSimMgr)
    {
        TAF_PA_CRIT("Failed to create RemoteSimManager");
        return TAF_PA_FAULT;
    }

    telux::common::ServiceStatus remoteSimStatus = pa.remoteSimMgr->getServiceStatus();
    if (remoteSimStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("RemoteSim subsystem is not ready, waiting for it to be ready...");
        std::promise<telux::common::ServiceStatus> remoteSimProm;
        pa.remoteSimMgr = phoneFactory.getRemoteSimManager(pa.slotId,
            [&](telux::common::ServiceStatus status)
        {
            TAF_PA_INFO("Getting status:%d from RemoteSim manager", (int)status);
            if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                remoteSimProm.set_value(telux::common::ServiceStatus::SERVICE_AVAILABLE);
            }
            else
            {
                remoteSimProm.set_value(telux::common::ServiceStatus::SERVICE_FAILED);
            }
        });

        std::future<telux::common::ServiceStatus> initFuture = remoteSimProm.get_future();
        std::future_status waitStatus = initFuture.wait_for(
            std::chrono::seconds(TAF_PA_REMOTESIM_SUBSYSTEM_TIMEOUT));

        if (std::future_status::timeout == waitStatus)
        {
            TAF_PA_CRIT("Timeout waiting for RemoteSim subsystem");
            pa.remoteSimMgr = nullptr;
            return TAF_PA_TIMEOUT;
        }
        else
        {
            remoteSimStatus = initFuture.get();
        }
    }

    if (remoteSimStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("RemoteSim subsystem is ready.");
    }
    else
    {
        TAF_PA_ERROR("Fail to init RemoteSim subsystem");
        pa.remoteSimMgr = nullptr;
        return TAF_PA_FAULT;
    }

    // Create and register the RemoteSim listener
    TAF_PA_INFO("Creating and registering RemoteSim listener...");
    pa.tafRemoteSimListener = std::make_shared<tafPaRemoteSimListener>();
    pa.remoteSimListener = pa.tafRemoteSimListener;

    if (pa.remoteSimMgr->registerListener(pa.remoteSimListener) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to register RemoteSim listener.");
        pa.remoteSimMgr = nullptr;
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("RemoteSim listener registered successfully.");
    TAF_PA_INFO("RemoteSim platform adaptor initialization is done.");
    g_remotesimPaInitialized.store(true, std::memory_order_release);

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_remotesim_Deinit()
{
    TAF_PA_INFO("Starting RemoteSim platform adaptor deinitialization...");

    if (!g_remotesimPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Deinit() called before Init() was successfully called");
        return TAF_PA_FAULT;
    }

    auto& pa = PlatformAdaptor::GetInstance();

    // Deregister the RemoteSim listener
    if (pa.remoteSimMgr && pa.remoteSimListener)
    {
        TAF_PA_INFO("Deregistering RemoteSim listener...");
        if (pa.remoteSimMgr->deregisterListener(pa.remoteSimListener) != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("Failed to deregister RemoteSim listener.");
        }
        else
        {
            TAF_PA_INFO("RemoteSim listener deregistered successfully.");
        }
    }

    // Clear event listener
    TAF_PA_INFO("Clearing eventListener_");
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        pa.eventListener_ = nullptr;
    }

    // Reset listener shared pointers
    TAF_PA_INFO("Resetting listener shared pointers");
    pa.remoteSimListener.reset();
    pa.tafRemoteSimListener.reset();

    // Reset manager shared pointer
    TAF_PA_INFO("Resetting remoteSimMgr");
    pa.remoteSimMgr.reset();

    // Reset initialization flag
    TAF_PA_INFO("Resetting initialization flag");
    g_remotesimPaInitialized.store(false, std::memory_order_release);

    TAF_PA_INFO("RemoteSim platform adaptor deinitialization complete.");
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_remotesim_RegisterEventListener(
    taf_pa_remotesim_EventListener* eventListener,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();
    TAF_PA_INFO("taf_pa_remotesim_RegisterEventListener");
    return pa.RegisterEventListener(eventListener, context);
}

taf_pa_result_t taf_pa_remotesim_DeregisterEventListener()
{
    auto& pa = PlatformAdaptor::GetInstance();
    TAF_PA_INFO("taf_pa_remotesim_DeregisterEventListener");

    std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
    pa.eventListener_ = nullptr;

    TAF_PA_INFO("Event listener deregistered successfully");
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_remotesim_SendConnectionAvailable(
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendConnectionAvailable(responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send connection available");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendConnectionAvailable response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Connection available sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendConnectionUnavailable(
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendConnectionUnavailable(responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send connection unavailable");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendConnectionUnavailable response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Connection unavailable sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendApdu(
    unsigned int id,
    const std::vector<uint8_t>& apdu,
    bool isSuccess,
    uint32_t totalSize,
    uint32_t offset,
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendApdu(id, apdu, isSuccess, totalSize, 
                                   offset, responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send APDU");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendApdu response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("APDU sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendCardReset(
    const std::vector<uint8_t>& atr,
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendCardReset(atr, responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send card reset");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendCardReset response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Card reset sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendCardInserted(
    const std::vector<uint8_t>& atr,
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendCardInserted(atr, responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send card inserted");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendCardInserted response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Card inserted sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendCardRemoved(
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendCardRemoved(responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send card removed");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendCardRemoved response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Card removed sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendCardError(
    taf_pa_remotesim_CardErrorCause_t errorCause,
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    CardErrorCause teluxErrorCause;
    switch(errorCause)
    {
        case TAF_PA_REMOTESIM_CARD_ERROR_INVALID:
            teluxErrorCause = CardErrorCause::INVALID;
            break;
        case TAF_PA_REMOTESIM_CARD_ERROR_UNKNOWN:
            teluxErrorCause = CardErrorCause::UNKNOWN_ERROR;
            break;
        case TAF_PA_REMOTESIM_CARD_ERROR_NO_LINK:
            teluxErrorCause = CardErrorCause::NO_LINK_ESTABLISHED;
            break;
        case TAF_PA_REMOTESIM_CARD_ERROR_COMMAND_TIMEOUT:
            teluxErrorCause = CardErrorCause::COMMAND_TIMEOUT;
            break;
        case TAF_PA_REMOTESIM_CARD_ERROR_POWER_DOWN:
            teluxErrorCause = CardErrorCause::POWER_DOWN;
            break;
        default:
            teluxErrorCause = CardErrorCause::UNKNOWN_ERROR;
            break;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendCardError(teluxErrorCause, responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send card error");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendCardError response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Card error sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendCardWakeup(
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendCardWakeup(responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send card wakeup");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendCardWakeup response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Card wakeup sent with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_remotesim_SendReset(
    taf_pa_remotesim_ResponseCb callback,
    std::any context)
{
    auto& pa = PlatformAdaptor::GetInstance();

    if (!pa.remoteSimMgr)
    {
        TAF_PA_ERROR("RemoteSimManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaRemoteSimResponseCallback>(callback, context, &promise);

    // Create lambda that captures the callback object
    auto responseCb = [cb](telux::common::ErrorCode errorCode) {
        cb->commandResponse(errorCode);
    };

    if (pa.remoteSimMgr->sendReset(responseCb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to send reset");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for SendReset response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Reset sent with result: %d", result);
    return result;
}
