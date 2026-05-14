/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_SMS_PA_HPP
#define TAF_SMS_PA_HPP

#include <string>
#include <vector>
#include <functional>
#include <unistd.h>
#include "tafCommonPa.h"

namespace tafpa::sms {

#define PA_MAX_DESTINATION_LEN_BYTE 51

enum class taf_pa_sms_Tag
{
    TAF_PA_UNKNOWN = -1,    /**< Unknown tag type */
    TAF_PA_READ,            /**< Message marked as read */
    TAF_PA_NOT_READ         /**< Message marked as not read */
};

enum class taf_pa_sms_Storage
{
    TAF_PA_STORAGE_NONE,    /**< NONE storage (notify only) */
    TAF_PA_STORAGE_NV,      /**< NV storage (not supported) */
    TAF_PA_STORAGE_SIM,     /**< SIM storage */
    TAF_PA_STORAGE_HLOS,    /**< HLOS storage */
    TAF_PA_STORAGE_UNKNOWN  /**< Unknown storage */
};

enum class taf_pa_sms_StorageFullType
{
    TAF_PA_FULL_UNKNOWN,            /**< Unknown */
    TAF_PA_FULL_HLOS_ALERT,         /**< HLOS storage is going to be full */
    TAF_PA_FULL_HLOS,               /**< HLOS storage is full */
    TAF_PA_FULL_HLOS_STORE_TO_SIM,  /**< HLOS storage is full and message is stored in SIM */
    TAF_PA_FULL_BOTH_HLOS_AND_SIM,  /**< Both HLOS storage and SIM storage are full */
    TAF_PA_FULL_NV,                 /**< NV storage is full */
    TAF_PA_FULL_SIM,                /**< SIM storage is full */
    TAF_PA_FULL_SIM2                /**< SIM2 storage is full */
};

typedef struct
{
    int startMessageId;
    int endMessageId;
} taf_cb_filter_t;

typedef std::function<void(void* context, pa_result_t result)> taf_pa_sms_SendCallback_t;

typedef std::function<void(
    int phoneId, const std::string& pdu, const std::string& sender, int storageIdx)>
    IncomingSmsCallback;

typedef std::function<void(int phoneId, taf_pa_sms_StorageFullType fullType)>
    MemoryFullCallback;

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_Init(void);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_ReadMessage
(
    uint32_t readAtIdx,
    uint32_t timeout,
    uint8_t phoneId,
    taf_pa_sms_Tag* pduRxStatus,
    std::vector<uint8_t>& pduBuffer,
    uint32_t* pduMsgIndex
);

PA_SHARED PA_WEAK void taf_pa_sms_RegisterIncomingSmsCallback
(
    IncomingSmsCallback cb
);

PA_SHARED PA_WEAK void taf_pa_sms_RegisterMemoryFullCallback
(
    MemoryFullCallback cb
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_SendRawSms
(
    uint8_t* pduData,
    uint32_t pduLength,
    uint32_t timeout,
    uint8_t phoneId
);

PA_SHARED PA_WEAK void taf_pa_sms_SendPDUMessageAsync
(
    uint8_t phoneId,
    const uint8_t* pduData,
    size_t pduLength,
    std::function<void(pa_result_t)> cb
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_SetActivationStatus
(
    uint8_t phoneId,
    bool activate,
    uint32_t timeout
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_RequestMessageFilters
(
    uint8_t phoneId,
    uint32_t timeout
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_AddCellBroadcastIds
(
    uint8_t phoneId,
    uint16_t fromId,
    uint16_t toId,
    uint32_t timeout
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_RemoveCellBroadcastIds
(
    uint8_t phoneId,
    uint16_t fromId,
    uint16_t toId,
    uint32_t timeout
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_GetSmsCenterAddress
(
    uint8_t phoneId,
    char* addr,
    size_t len,
    uint32_t timeout
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_SetSmsCenterAddress
(
    uint8_t phoneId,
    const char* addr,
    uint32_t timeout
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_GetPreferredStorage
(
    taf_pa_sms_Storage* type,
    uint32_t timeout,
    uint8_t phoneId
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_SetPreferredStorage
(
    taf_pa_sms_Storage type,
    uint32_t timeout,
    uint8_t phoneId
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_SetTag
(
    uint32_t msgIndex,
    taf_pa_sms_Tag tagType,
    uint32_t timeout,
    uint8_t phoneId
);

PA_SHARED PA_WEAK pa_result_t taf_pa_sms_DeleteMessage
(
    uint32_t msgIndex,
    uint32_t timeout,
    uint8_t phoneId
);

PA_SHARED PA_WEAK int32_t taf_pa_sms_RequestSmsMessageList
(
    uint32_t* arr,
    size_t arrSize,
    uint32_t timeout,
    taf_pa_sms_Tag tagType,
    uint8_t phoneId
);

}

#endif /* TAF_SMS_PA_HPP */

