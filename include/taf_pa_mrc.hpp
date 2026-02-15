/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_MRC_HPP
#define TAF_PA_MRC_HPP

#include "taf_pa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PA_MRC_EFS_PARTITION_BLOCKS 90

typedef enum
{
    TAF_PA_MRC_RESULT_UNKNOWN = 0,
    TAF_PA_MRC_RESULT_SUCCESS = 1,
    TAF_PA_MRC_RESULT_FAILURE = 2
} taf_pa_mrc_Result_t;

typedef enum
{
    TAF_PA_MRC_STATUS_UNKNOWN = 0,
    TAF_PA_MRC_STATUS_INITIATED = 1,
    TAF_PA_MRC_STATUS_RESUMED = 2,
    TAF_PA_MRC_STATUS_SUCCEEDED = 3,
    TAF_PA_MRC_STATUS_FAILED = 4,
} taf_pa_mrc_Status_t;

typedef enum
{
    TAF_PA_MRC_PROCESS_UNKNOWN = 0,
    TAF_PA_MRC_PROCESS_ABSYNC = 1,
    TAF_PA_MRC_PROCESS_OTA = 2,
} taf_pa_mrc_Process_t;

typedef enum
{
    TAF_PA_MRC_TIMER_UNKNOWN = 0,
    TAF_PA_MRC_TIMER_SCRUB = 1,
    TAF_PA_MRC_TIMER_EFS_BACKUP = 2,
    TAF_PA_MRC_TIMER_SUSPEND = 3,
    TAF_PA_MRC_TIMER_DEFER = 4
} taf_pa_mrc_Timer_t;

typedef struct
{
    uint8_t processValid;
    taf_pa_mrc_Process_t process;
    uint8_t statusValid;
    taf_pa_mrc_Status_t status;
    uint8_t resultValid;
    taf_pa_mrc_Result_t result;
} taf_pa_mrc_ProcessStatusIndication_t;

typedef struct
{
    uint32_t peCountLen;
    uint32_t peCount[TAF_PA_MRC_EFS_PARTITION_BLOCKS];
} taf_pa_mrc_EfsPeStatus_t;

typedef struct
{
    uint32_t maxEraseCount;
    uint32_t totalBadBlocks;
} taf_pa_mrc_EfsBlockStatus_t;

typedef struct taf_pa_mrc_ProcessStatusHandler* taf_pa_mrc_ProcessStatusHandlerRef_t;

typedef void (*taf_pa_mrc_ProcessStatusHdlrFunc_t)
(
    taf_pa_mrc_ProcessStatusIndication_t indication,
    void* contextPtr
);

typedef struct
{
    uint8_t slotToggleRequested;
} taf_pa_mrc_ScrubStatusIndication_t;

typedef struct taf_pa_mrc_ScrubStatusHandler* taf_pa_mrc_ScrubStatusHandlerRef_t;

typedef void (*taf_pa_mrc_ScrubStatusHdlrFunc_t)
(
    taf_pa_mrc_ScrubStatusIndication_t indication,
    void* contextPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_Init
(
    void
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_RegisterIndication
(
    uint8_t registration
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_SetProcessStatus
(
    taf_pa_mrc_Process_t process,
    taf_pa_mrc_Status_t status
);

PA_SHARED PA_WEAK taf_pa_mrc_ProcessStatusHandlerRef_t taf_pa_mrc_AddProcessStatusHandler
(
    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_GetEfsPeStatus
(
    taf_pa_mrc_EfsPeStatus_t* statusPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_GetEfsBlockStatus
(
    taf_pa_mrc_EfsBlockStatus_t* statusPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_SetTimerPeriod
(
    taf_pa_mrc_Timer_t timer,
    uint32_t period
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_PerformABSync
(
    void
);

PA_SHARED PA_WEAK taf_pa_mrc_ScrubStatusHandlerRef_t taf_pa_mrc_AddScrubStatusHandler
(
    taf_pa_mrc_ScrubStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_mrc_AckSlotToggle
(
    int32_t success
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PA_MRC_HPP */
