/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_REMOTESIM_PA_HPP
#define TAF_REMOTESIM_PA_HPP

#include "tafCommonPa.h"
#include <string>
#include <vector>
#include <functional>
#include <any>
#include <memory>


// Service status
typedef enum
{
    TAF_PA_REMOTESIM_SERVICE_UNAVAILABLE = 0,
    TAF_PA_REMOTESIM_SERVICE_AVAILABLE = 1
} taf_pa_remotesim_ServiceStatus_e;

// Card error causes
typedef enum
{
    TAF_PA_REMOTESIM_CARD_ERROR_INVALID = -1,
    TAF_PA_REMOTESIM_CARD_ERROR_UNKNOWN = 0,
    TAF_PA_REMOTESIM_CARD_ERROR_NO_LINK = 1,
    TAF_PA_REMOTESIM_CARD_ERROR_COMMAND_TIMEOUT = 2,
    TAF_PA_REMOTESIM_CARD_ERROR_POWER_DOWN = 3
} taf_pa_remotesim_CardErrorCause_t;

// Response types
typedef enum
{
    TAF_PA_REMOTESIM_OPEN_CONNECTION = 0,
    TAF_PA_REMOTESIM_CLOSE_CONNECTION = 1,
    TAF_PA_REMOTESIM_POWER_ON = 2,
    TAF_PA_REMOTESIM_POWER_OFF = 3,
    TAF_PA_REMOTESIM_RESET = 4
} taf_pa_remotesim_ResponseType_t;

// Event structures
struct taf_pa_remotesim_ApduTransfer_t
{
    unsigned int id;
    std::vector<uint8_t> apdu;
};

struct taf_pa_remotesim_ServiceStatus_t
{
    taf_pa_remotesim_ServiceStatus_e status;
};

// Unsolicited event listener callbacks (for modem-initiated events)
using taf_pa_remotesim_onApduTransfer =
    std::function<void(const std::shared_ptr<taf_pa_remotesim_ApduTransfer_t>& apduEvent)>;

using taf_pa_remotesim_onCardConnect =
    std::function<void()>;

using taf_pa_remotesim_onCardDisconnect =
    std::function<void()>;

using taf_pa_remotesim_onCardPowerUp =
    std::function<void()>;

using taf_pa_remotesim_onCardPowerDown =
    std::function<void()>;

using taf_pa_remotesim_onCardReset =
    std::function<void()>;

using taf_pa_remotesim_onServiceStatusChange =
    std::function<void(const std::shared_ptr<taf_pa_remotesim_ServiceStatus_t>& statusEvent)>;

// Per-call callback type (for API responses with context)
using taf_pa_remotesim_ResponseCb = std::function<void(pa_result_t result, std::any context)>;

// Event listener structure (ONLY for unsolicited events from modem)
struct taf_pa_remotesim_EventListener
{
    taf_pa_remotesim_onApduTransfer onApduTransfer;
    taf_pa_remotesim_onCardConnect onCardConnect;
    taf_pa_remotesim_onCardDisconnect onCardDisconnect;
    taf_pa_remotesim_onCardPowerUp onCardPowerUp;
    taf_pa_remotesim_onCardPowerDown onCardPowerDown;
    taf_pa_remotesim_onCardReset onCardReset;
    taf_pa_remotesim_onServiceStatusChange onServiceStatusChange;
};

//--------------------------------------------------------------------------------------------------
/**
 * Initialize PA RemoteSim
 *
 * @param slotId
 *  The slot ID for the RemoteSim manager
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_Init(int slotId);

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize PA RemoteSim
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_Deinit();

//--------------------------------------------------------------------------------------------------
/**
 * Register event listener for RemoteSim events
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_RegisterEventListener(
    taf_pa_remotesim_EventListener* eventListener,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Deregister event listener for RemoteSim events
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_DeregisterEventListener();

//--------------------------------------------------------------------------------------------------
/**
 * Send connection available notification
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendConnectionAvailable(
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send connection unavailable notification
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendConnectionUnavailable(
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send APDU to modem
 *
 * @param [in] id         APDU identifier
 * @param [in] apdu       APDU data
 * @param [in] isSuccess  Success flag
 * @param [in] totalSize  Total size
 * @param [in] offset     Offset
 * @param [in] callback   Callback function to receive the response
 * @param [in] context    Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendApdu(
    unsigned int id,
    const std::vector<uint8_t>& apdu,
    bool isSuccess,
    uint32_t totalSize,
    uint32_t offset,
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send card reset notification
 *
 * @param [in] atr       ATR data
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendCardReset(
    const std::vector<uint8_t>& atr,
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send card inserted notification
 *
 * @param [in] atr       ATR data
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendCardInserted(
    const std::vector<uint8_t>& atr,
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send card removed notification
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendCardRemoved(
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send card error notification
 *
 * @param [in] errorCause  Error cause
 * @param [in] callback    Callback function to receive the response
 * @param [in] context     Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendCardError(
    taf_pa_remotesim_CardErrorCause_t errorCause,
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send card wakeup notification
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendCardWakeup(
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send reset notification
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_remotesim_SendReset(
    taf_pa_remotesim_ResponseCb callback,
    std::any context
);

#endif /* TAF_REMOTESIM_PA_HPP */
