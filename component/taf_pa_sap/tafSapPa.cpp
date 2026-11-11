/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <time.h>
#include <errno.h>
#include <memory>
#include <vector>
#include <telux/tel/PhoneFactory.hpp>
#include <telux/common/CommonDefines.hpp>
#include "tafInternalCommonPa.h"
#include "tafSapPa.hpp"

using namespace telux::tel;
using namespace telux::common;
using namespace std;

#define SERVICE_TIMEOUT 5
#define REQUEST_TIMEOUT 5
#define TAF_PA_SAP_SUBSYSTEM_TIMEOUT 30

static std::atomic<bool> g_sapPaInitialized(false);

// Callback classes for SAP operations with per-call context
class tafPaResponseCallback : public telux::common::ICommandResponseCallback {
public:
    tafPaResponseCallback(taf_pa_sap_ResponseCb cb, std::any ctx, std::promise<taf_pa_result_t>* prom = nullptr)
        : callback(cb), context(ctx), promise(prom) {}
    void commandResponse(telux::common::ErrorCode errorCode) override;
private:
    taf_pa_sap_ResponseCb callback;
    std::any context;
    std::promise<taf_pa_result_t>* promise;
};

class tafPaApduResponseCallback : public telux::tel::ISapCardCommandCallback {
public:
    tafPaApduResponseCallback(uint8_t id, taf_pa_sap_ApduResponseCb cb, std::any ctx, std::promise<taf_pa_result_t>* prom = nullptr) 
        : apduId(id), callback(cb), context(ctx), promise(prom) {}
    void onResponse(telux::tel::IccResult result, telux::common::ErrorCode error) override;
private:
    uint8_t apduId;
    taf_pa_sap_ApduResponseCb callback;
    std::any context;
    std::promise<taf_pa_result_t>* promise;
};

class tafPaAtrResponseCallback : public telux::tel::IAtrResponseCallback {
public:
    tafPaAtrResponseCallback(taf_pa_sap_AtrResponseCb cb, std::any ctx, std::promise<taf_pa_result_t>* prom = nullptr)
        : callback(cb), context(ctx), promise(prom) {}
    void atrResponse(std::vector<int> responseAtr, telux::common::ErrorCode error) override;
private:
    taf_pa_sap_AtrResponseCb callback;
    std::any context;
    std::promise<taf_pa_result_t>* promise;
};

class tafPaCardReaderCallback : public telux::tel::ICardReaderCallback {
public:
    tafPaCardReaderCallback(taf_pa_sap_CardReaderResponseCb cb, std::any ctx, std::promise<taf_pa_result_t>* prom = nullptr)
        : callback(cb), context(ctx), promise(prom) {}
    void cardReaderResponse(CardReaderStatus cardReaderStatus, telux::common::ErrorCode error) override;
private:
    taf_pa_sap_CardReaderResponseCb callback;
    std::any context;
    std::promise<taf_pa_result_t>* promise;
};

class tafPaSapStateCallback {
public:
    tafPaSapStateCallback(taf_pa_sap_StateResponseCb cb, std::any ctx, std::promise<taf_pa_result_t>* prom = nullptr)
        : callback(cb), context(ctx), promise(prom) {}
    void operator()(telux::tel::SapState sapState, telux::common::ErrorCode error);
private:
    taf_pa_sap_StateResponseCb callback;
    std::any context;
    std::promise<taf_pa_result_t>* promise;
};

// Platform Adaptor class
class SapPlatformAdaptor
{
public:
    std::shared_ptr<telux::tel::ISapCardManager> sapCardMgr = nullptr;

    bool cardConnected = false;
    bool cardPoweredOn = true;
    int slotId = DEFAULT_SLOT_ID;

    static SapPlatformAdaptor& GetInstance();

    taf_pa_result_t MapErrorCode(telux::common::ErrorCode errorCode);

    SapPlatformAdaptor() = default;
    ~SapPlatformAdaptor() = default;
};

SapPlatformAdaptor& SapPlatformAdaptor::GetInstance()
{
    static SapPlatformAdaptor instance;
    return instance;
}

taf_pa_result_t SapPlatformAdaptor::MapErrorCode(telux::common::ErrorCode errorCode)
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

// SAP Callback implementations with per-call context
void tafPaResponseCallback::commandResponse(telux::common::ErrorCode errorCode)
{
    TAF_PA_INFO("General callback: errorCode=%d", static_cast<int>(errorCode));
    auto& pa = SapPlatformAdaptor::GetInstance();

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

void tafPaApduResponseCallback::onResponse(telux::tel::IccResult result, telux::common::ErrorCode error)
{
    TAF_PA_INFO("APDU Response callback: errorCode=%d", static_cast<int>(error));
    auto& pa = SapPlatformAdaptor::GetInstance();

    auto apduResponse = std::make_shared<taf_pa_sap_ApduResponse_t>();
    apduResponse->id = apduId;
    apduResponse->result = pa.MapErrorCode(error);
    apduResponse->sw1 = static_cast<uint8_t>(result.sw1);
    apduResponse->sw2 = static_cast<uint8_t>(result.sw2);
    apduResponse->data = result.data;

    if (promise)
    {
        promise->set_value(apduResponse->result);
    }

    if (callback)
    {
        TAF_PA_INFO("Invoking per-call APDU callback with context");
        callback(apduResponse, context);
    }
}

void tafPaAtrResponseCallback::atrResponse(std::vector<int> responseAtr, telux::common::ErrorCode error)
{
    TAF_PA_INFO("ATR Response callback: errorCode=%d", static_cast<int>(error));
    auto& pa = SapPlatformAdaptor::GetInstance();

    auto atrEvent = std::make_shared<taf_pa_sap_AtrResponse_t>();
    atrEvent->result = pa.MapErrorCode(error);
    atrEvent->atr = responseAtr;

    if (promise)
    {
        promise->set_value(atrEvent->result);
    }

    if (callback)
    {
        TAF_PA_INFO("Invoking per-call ATR callback with context");
        callback(atrEvent, context);
    }
}

void tafPaCardReaderCallback::cardReaderResponse(CardReaderStatus cardReaderStatus, telux::common::ErrorCode error)
{
    TAF_PA_INFO("CardReader Response callback: errorCode=%d", static_cast<int>(error));
    auto& pa = SapPlatformAdaptor::GetInstance();

    auto readerEvent = std::make_shared<taf_pa_sap_CardReaderResponse_t>();
    readerEvent->result = pa.MapErrorCode(error);
    readerEvent->id = cardReaderStatus.id;
    readerEvent->isRemovable = cardReaderStatus.isRemovable;
    readerEvent->isPresent = cardReaderStatus.isPresent;
    readerEvent->isID1size = cardReaderStatus.isID1size;
    readerEvent->isCardPresent = cardReaderStatus.isCardPresent;
    readerEvent->isCardPoweredOn = cardReaderStatus.isCardPoweredOn;

    if (promise)
    {
        promise->set_value(readerEvent->result);
    }

    if (callback)
    {
        TAF_PA_INFO("Invoking per-call CardReader callback with context");
        callback(readerEvent, context);
    }
}

void tafPaSapStateCallback::operator()(telux::tel::SapState sapState, telux::common::ErrorCode error)
{
    TAF_PA_INFO("SAP State callback: sapState=%d, errorCode=%d", static_cast<int>(sapState), static_cast<int>(error));
    auto& pa = SapPlatformAdaptor::GetInstance();

    // Map Telux SapState to PA SapState
    taf_pa_sap_State_t paSapState;
    switch(sapState)
    {
        case telux::tel::SapState::SAP_STATE_NOT_ENABLED:
            paSapState = TAF_PA_SAP_STATE_NOT_ENABLED;
            break;
        case telux::tel::SapState::SAP_STATE_CONNECTING:
            paSapState = TAF_PA_SAP_STATE_CONNECTING;
            break;
        case telux::tel::SapState::SAP_STATE_CONNECTED_SUCCESSFULLY:
            paSapState = TAF_PA_SAP_STATE_CONNECTED_SUCCESSFULLY;
            break;
        case telux::tel::SapState::SAP_STATE_CONNECTION_ERROR:
            paSapState = TAF_PA_SAP_STATE_CONNECTION_ERROR;
            break;
        case telux::tel::SapState::SAP_STATE_DISCONNECTING:
            paSapState = TAF_PA_SAP_STATE_DISCONNECTING;
            break;
        case telux::tel::SapState::SAP_STATE_DISCONNECTED_SUCCESSFULLY:
            paSapState = TAF_PA_SAP_STATE_DISCONNECTED_SUCCESSFULLY;
            break;
        default:
            paSapState = TAF_PA_SAP_STATE_NOT_ENABLED;
            break;
    }

    taf_pa_result_t result = pa.MapErrorCode(error);

    if (promise)
    {
        promise->set_value(result);
    }

    if (callback)
    {
        TAF_PA_INFO("Invoking per-call SAP State callback with context");
        callback(paSapState, result, context);
    }
}

// PA API implementations
taf_pa_result_t taf_pa_sap_Init(int slotId)
{
    TAF_PA_INFO("taf_pa_sap_Init with slot ID: %d", slotId);
    auto& pa = SapPlatformAdaptor::GetInstance();
    auto& phoneFactory = telux::tel::PhoneFactory::getInstance();

    // Store the slot ID
    pa.slotId = slotId;

    // Initialize SapCardManager
    TAF_PA_INFO("Initializing SapCardManager...");
    pa.sapCardMgr = phoneFactory.getSapCardManager(slotId);

    if (!pa.sapCardMgr)
    {
        TAF_PA_CRIT("Failed to create SapCardManager");
        return TAF_PA_FAULT;
    }

    telux::common::ServiceStatus sapStatus = pa.sapCardMgr->getServiceStatus();
    if (sapStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("SAP subsystem is not ready, waiting for it to be ready...");
        std::promise<telux::common::ServiceStatus> sapProm;
        pa.sapCardMgr = phoneFactory.getSapCardManager(pa.slotId,
            [&](telux::common::ServiceStatus status)
        {
            TAF_PA_INFO("Getting status:%d from SAP manager", (int)status);
            if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                sapProm.set_value(telux::common::ServiceStatus::SERVICE_AVAILABLE);
            }
            else
            {
                sapProm.set_value(telux::common::ServiceStatus::SERVICE_FAILED);
            }
        });

        std::future<telux::common::ServiceStatus> initFuture = sapProm.get_future();
        std::future_status waitStatus = initFuture.wait_for(
            std::chrono::seconds(TAF_PA_SAP_SUBSYSTEM_TIMEOUT));

        if (std::future_status::timeout == waitStatus)
        {
            TAF_PA_CRIT("Timeout waiting for SAP subsystem");
            pa.sapCardMgr = nullptr;
            return TAF_PA_TIMEOUT;
        }
        else
        {
            sapStatus = initFuture.get();
        }
    }

    if (sapStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("SAP subsystem is ready.");
    }
    else
    {
        TAF_PA_ERROR("Fail to init SAP subsystem");
        pa.sapCardMgr = nullptr;
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("SAP platform adaptor initialization is done.");
    g_sapPaInitialized.store(true, std::memory_order_release);

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_sap_Deinit()
{
    TAF_PA_INFO("Starting SAP platform adaptor deinitialization...");

    if (!g_sapPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Deinit() called before Init() was successfully called");
        return TAF_PA_FAULT;
    }

    auto& pa = SapPlatformAdaptor::GetInstance();

    // Reset manager shared pointer
    TAF_PA_INFO("Resetting sapCardMgr");
    pa.sapCardMgr.reset();

    // Reset initialization flag
    TAF_PA_INFO("Resetting initialization flag");
    g_sapPaInitialized.store(false, std::memory_order_release);

    TAF_PA_INFO("SAP platform adaptor deinitialization complete.");
    return TAF_PA_OK;
}

// SAP Card Manager APIs with per-call callbacks
taf_pa_result_t taf_pa_sap_OpenConnection(
    taf_pa_sap_Condition_t sapCondition,
    taf_pa_sap_ResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    // Map PA SapCondition to Telux SapCondition
    telux::tel::SapCondition teluxSapCondition;
    switch(sapCondition)
    {
        case TAF_PA_SAP_CONDITION_BLOCK_VOICE_OR_DATA:
            teluxSapCondition = telux::tel::SapCondition::SAP_CONDITION_BLOCK_VOICE_OR_DATA;
            break;
        case TAF_PA_SAP_CONDITION_BLOCK_DATA:
            teluxSapCondition = telux::tel::SapCondition::SAP_CONDITION_BLOCK_DATA;
            break;
        case TAF_PA_SAP_CONDITION_BLOCK_VOICE:
            teluxSapCondition = telux::tel::SapCondition::SAP_CONDITION_BLOCK_VOICE;
            break;
        case TAF_PA_SAP_CONDITION_BLOCK_NONE:
            teluxSapCondition = telux::tel::SapCondition::SAP_CONDITION_BLOCK_NONE;
            break;
        default:
            teluxSapCondition = telux::tel::SapCondition::SAP_CONDITION_BLOCK_VOICE_OR_DATA;
            break;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaResponseCallback>(callback, context, &promise);

    if (pa.sapCardMgr->openConnection(teluxSapCondition, cb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to open SAP connection");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for OpenConnection response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    if (result == TAF_PA_OK)
    {
        pa.cardConnected = true;
    }
    TAF_PA_INFO("SAP connection open completed with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_sap_CloseConnection(
    taf_pa_sap_ResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaResponseCallback>(callback, context, &promise);

    if (pa.sapCardMgr->closeConnection(cb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to close SAP connection");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for CloseConnection response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    if (result == TAF_PA_OK)
    {
        pa.cardConnected = false;
    }
    TAF_PA_INFO("SAP connection close completed with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_sap_RequestPowerOn(
    taf_pa_sap_ResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    // Power on is handled through the SAP protocol
    TAF_PA_INFO("Power on request - handled through SAP protocol");
    pa.cardPoweredOn = true;

    // Invoke callback immediately with success
    if (callback)
    {
        callback(TAF_PA_OK, context);
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_sap_RequestPowerOff(
    taf_pa_sap_ResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    // Power off is handled through the SAP protocol
    TAF_PA_INFO("Power off request - handled through SAP protocol");
    pa.cardPoweredOn = false;

    // Invoke callback immediately with success
    if (callback)
    {
        callback(TAF_PA_OK, context);
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_sap_RequestReset(
    taf_pa_sap_ResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    // Reset is handled through the SAP protocol
    TAF_PA_INFO("Reset request - handled through SAP protocol");

    // Invoke callback immediately with success
    if (callback)
    {
        callback(TAF_PA_OK, context);
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_sap_TransmitApdu(
    uint8_t apduId,
    uint8_t cla,
    uint8_t instruction,
    uint8_t p1,
    uint8_t p2,
    uint8_t lc,
    const std::vector<uint8_t>& data,
    uint8_t le,
    taf_pa_sap_ApduResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaApduResponseCallback>(apduId, callback, context, &promise);

    if (pa.sapCardMgr->transmitApdu(cla, instruction, p1, p2, lc, data, le, cb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to transmit APDU");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for TransmitApdu response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("APDU transmit completed with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_sap_RequestAtr(
    taf_pa_sap_AtrResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaAtrResponseCallback>(callback, context, &promise);

    if (pa.sapCardMgr->requestAtr(cb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to request ATR");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for RequestAtr response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("ATR request completed with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_sap_RequestCardReaderStatus(
    taf_pa_sap_CardReaderResponseCb callback,
    std::any context)
{
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = std::make_shared<tafPaCardReaderCallback>(callback, context, &promise);

    if (pa.sapCardMgr->requestCardReaderStatus(cb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to request card reader status");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for RequestCardReaderStatus response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("Card reader status request completed with result: %d", result);
    return result;
}

taf_pa_result_t taf_pa_sap_RequestState(
    taf_pa_sap_StateResponseCb callback,
    std::any context)
{
    TAF_PA_INFO("taf_pa_sap_RequestState");
    auto& pa = SapPlatformAdaptor::GetInstance();

    if (!pa.sapCardMgr)
    {
        TAF_PA_ERROR("SapCardManager is not initialized.");
        return TAF_PA_FAULT;
    }

    std::promise<taf_pa_result_t> promise;
    std::future<taf_pa_result_t> future = promise.get_future();

    auto cb = tafPaSapStateCallback(callback, context, &promise);

    if (pa.sapCardMgr->requestSapState(cb) != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to request SAP state");
        return TAF_PA_FAULT;
    }

    if (future.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::timeout)
    {
        TAF_PA_ERROR("Timeout waiting for RequestState response");
        return TAF_PA_TIMEOUT;
    }

    taf_pa_result_t result = future.get();
    TAF_PA_INFO("SAP state request completed with result: %d", result);
    return result;
}
