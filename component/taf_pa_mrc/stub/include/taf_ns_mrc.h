/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NS_MRC_H
#define TAF_NS_MRC_H

#include "taf_ns_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_NS_MRC_EFS_PARTITION_BLOCKS 90

typedef enum
{
    TAF_NS_MRC_RESULT_UNKNOWN = 0,
    TAF_NS_MRC_RESULT_SUCCESS = 1,
    TAF_NS_MRC_RESULT_FAILURE = 2
} taf_ns_mrc_Result_t;

typedef enum
{
    TAF_NS_MRC_STATUS_UNKNOWN = 0,
    TAF_NS_MRC_STATUS_INITIATED = 1,
    TAF_NS_MRC_STATUS_RESUMED = 2,
    TAF_NS_MRC_STATUS_SUCCEEDED = 3,
    TAF_NS_MRC_STATUS_FAILED = 4,
} taf_ns_mrc_Status_t;

typedef enum
{
    TAF_NS_MRC_PROCESS_UNKNOWN = 0,
    TAF_NS_MRC_PROCESS_ABSYNC = 1,
    TAF_NS_MRC_PROCESS_OTA = 2,
} taf_ns_mrc_Process_t;

typedef enum
{
    TAF_NS_MRC_TIMER_UNKNOWN = 0,
    TAF_NS_MRC_TIMER_SCRUB = 1,
    TAF_NS_MRC_TIMER_EFS_BACKUP = 2,
    TAF_NS_MRC_TIMER_SUSPEND = 3,
    TAF_NS_MRC_TIMER_DEFER = 4
} taf_ns_mrc_Timer_t;

typedef struct
{
    uint8_t processValid;
    taf_ns_mrc_Process_t process;
    uint8_t statusValid;
    taf_ns_mrc_Status_t status;
    uint8_t resultValid;
    taf_ns_mrc_Result_t result;
} taf_ns_mrc_ProcessStatusIndication_t;

typedef struct
{
    uint32_t peCountLen;
    uint32_t peCount[TAF_NS_MRC_EFS_PARTITION_BLOCKS];
} taf_ns_mrc_EfsPeStatus_t;

typedef struct
{
    uint32_t maxEraseCount;
    uint32_t totalBadBlocks;
} taf_ns_mrc_EfsBlockStatus_t;

typedef struct taf_ns_mrc_ProcessStatusHandler* taf_ns_mrc_ProcessStatusHandlerRef_t;

typedef void (*taf_ns_mrc_ProcessStatusHdlrFunc_t)
(
    taf_ns_mrc_ProcessStatusIndication_t indication,
    void* contextPtr
);

typedef struct
{
    uint8_t slotToggleRequested;
} taf_ns_mrc_ScrubStatusIndication_t;

typedef struct taf_ns_mrc_ScrubStatusHandler* taf_ns_mrc_ScrubStatusHandlerRef_t;

typedef void (*taf_ns_mrc_ScrubStatusHdlrFunc_t)
(
    taf_ns_mrc_ScrubStatusIndication_t indication,
    void* contextPtr
);

NS_SHARED int32_t taf_ns_mrc_Init
(
    void
);

NS_SHARED int32_t taf_ns_mrc_RegisterIndication
(
    uint8_t registration
);

NS_SHARED int32_t taf_ns_mrc_SetProcessStatus
(
    taf_ns_mrc_Process_t process,
    taf_ns_mrc_Status_t status
);

NS_SHARED taf_ns_mrc_ProcessStatusHandlerRef_t taf_ns_mrc_AddProcessStatusHandler
(
    taf_ns_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

NS_SHARED int32_t taf_ns_mrc_GetEfsPeStatus
(
    taf_ns_mrc_EfsPeStatus_t* statusPtr
);

NS_SHARED int32_t taf_ns_mrc_GetEfsBlockStatus
(
    taf_ns_mrc_EfsBlockStatus_t* statusPtr
);

NS_SHARED int32_t taf_ns_mrc_SetTimerPeriod
(
    taf_ns_mrc_Timer_t timer,
    uint32_t period
);

NS_SHARED taf_ns_mrc_ScrubStatusHandlerRef_t taf_ns_mrc_AddScrubStatusHandler
(
    taf_ns_mrc_ScrubStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

/**
 * Acknowledge slot toggle request.
 *
 * @param success Status code: 0 = SUCCESS, 1 = FAILED
 * @return 0 on success, negative error code on failure
 */
NS_SHARED int32_t taf_ns_mrc_AckSlotToggle
(
    int32_t success
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_NS_MRC_H */
