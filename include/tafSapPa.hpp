/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_SAP_PA_HPP
#define TAF_SAP_PA_HPP

#include "tafCommonPa.h"
#include <string>
#include <vector>
#include <functional>
#include <any>
#include <memory>


#define DEFAULT_SLOT_ID 0

// SAP connection conditions
typedef enum
{
    TAF_PA_SAP_CONDITION_BLOCK_VOICE_OR_DATA = 0,
    TAF_PA_SAP_CONDITION_BLOCK_DATA = 1,
    TAF_PA_SAP_CONDITION_BLOCK_VOICE = 2,
    TAF_PA_SAP_CONDITION_BLOCK_NONE = 3
} taf_pa_sap_Condition_t;

// SAP connection states
typedef enum
{
    TAF_PA_SAP_STATE_NOT_ENABLED = 0,
    TAF_PA_SAP_STATE_CONNECTING = 1,
    TAF_PA_SAP_STATE_CONNECTED_SUCCESSFULLY = 2,
    TAF_PA_SAP_STATE_CONNECTION_ERROR = 3,
    TAF_PA_SAP_STATE_DISCONNECTING = 4,
    TAF_PA_SAP_STATE_DISCONNECTED_SUCCESSFULLY = 5
} taf_pa_sap_State_t;

// Response structures
struct taf_pa_sap_ApduResponse_t
{
    uint8_t id;
    pa_result_t result;
    uint8_t sw1;
    uint8_t sw2;
    std::vector<int> data;
};

struct taf_pa_sap_AtrResponse_t
{
    pa_result_t result;
    std::vector<int> atr;
};

struct taf_pa_sap_CardReaderResponse_t
{
    pa_result_t result;
    int id;
    bool isRemovable;
    bool isPresent;
    bool isID1size;
    bool isCardPresent;
    bool isCardPoweredOn;
};

// Per-call callback types (for API responses with context)
using taf_pa_sap_ResponseCb = std::function<void(pa_result_t result, std::any context)>;

using taf_pa_sap_ApduResponseCb = 
    std::function<void(const std::shared_ptr<taf_pa_sap_ApduResponse_t>& response, std::any context)>;

using taf_pa_sap_AtrResponseCb = 
    std::function<void(const std::shared_ptr<taf_pa_sap_AtrResponse_t>& response, std::any context)>;

using taf_pa_sap_CardReaderResponseCb = 
    std::function<void(const std::shared_ptr<taf_pa_sap_CardReaderResponse_t>& response, std::any context)>;

using taf_pa_sap_StateResponseCb =
    std::function<void(taf_pa_sap_State_t sapState, pa_result_t result, std::any context)>;

//--------------------------------------------------------------------------------------------------
/**
 * Initialize PA SAP
 *
 * @param slotId The SIM slot ID to use
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_Init(int slotId);

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize PA SAP
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_Deinit();

//--------------------------------------------------------------------------------------------------
/**
 * Open SAP connection
 *
 * @param [in] sapCondition  SAP connection condition
 * @param [in] callback      Callback function to receive the response
 * @param [in] context       Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_OpenConnection(
    taf_pa_sap_Condition_t sapCondition,
    taf_pa_sap_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Close SAP connection
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_CloseConnection(
    taf_pa_sap_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Power on the card
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_RequestPowerOn(
    taf_pa_sap_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Power off the card
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_RequestPowerOff(
    taf_pa_sap_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Reset the card
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_RequestReset(
    taf_pa_sap_ResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Transfer APDU to card
 *
 * @param [in] apduId       APDU identifier
 * @param [in] cla          Class byte of APDU
 * @param [in] instruction  Instruction byte of APDU
 * @param [in] p1           Parameter 1 byte of APDU
 * @param [in] p2           Parameter 2 byte of APDU
 * @param [in] lc           Length of command data
 * @param [in] data         Command data
 * @param [in] le           Expected length of response data
 * @param [in] callback     Callback function to receive the response
 * @param [in] context      Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_TransmitApdu(
    uint8_t apduId,
    uint8_t cla,
    uint8_t instruction,
    uint8_t p1,
    uint8_t p2,
    uint8_t lc,
    const std::vector<uint8_t>& data,
    uint8_t le,
    taf_pa_sap_ApduResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Request ATR from card
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_RequestAtr(
    taf_pa_sap_AtrResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Request card reader status
 *
 * @param [in] callback  Callback function to receive the response
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_RequestCardReaderStatus(
    taf_pa_sap_CardReaderResponseCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Request SAP connection state (asynchronous)
 *
 * @param [in] callback  Callback function to receive the SAP state
 * @param [in] context   Context to pass back in callback
 *
 * @return
 *  - PA_OK on success
 *  - PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_sap_RequestState(
    taf_pa_sap_StateResponseCb callback,
    std::any context
);


#endif /* TAF_SAP_PA_HPP */
