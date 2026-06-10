/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#ifndef TAF_PROP_MRC_H
#define TAF_PROP_MRC_H

#include "taf_prop_common.h"
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PROP_MRC_EFS_PARTITION_BLOCKS 90
#define TAF_PROP_MRC_EFS_WRITE_TASKS      100
#define TAF_PROP_MRC_EFS_TASK_NAME_LEN    20

typedef enum
{
    TAF_PROP_MRC_RESULT_UNKNOWN = 0,
    TAF_PROP_MRC_RESULT_SUCCESS = 1,
    TAF_PROP_MRC_RESULT_FAILURE = 2
} taf_prop_mrc_Result_t;

typedef enum
{
    TAF_PROP_MRC_STATUS_UNKNOWN = 0,
    TAF_PROP_MRC_STATUS_INITIATED = 1,
    TAF_PROP_MRC_STATUS_RESUMED = 2,
    TAF_PROP_MRC_STATUS_SUCCEEDED = 3,
    TAF_PROP_MRC_STATUS_FAILED = 4,
} taf_prop_mrc_Status_t;

typedef enum
{
    TAF_PROP_MRC_PROCESS_UNKNOWN = 0,
    TAF_PROP_MRC_PROCESS_ABSYNC = 1,
    TAF_PROP_MRC_PROCESS_OTA = 2,
} taf_prop_mrc_Process_t;

typedef enum
{
    TAF_PROP_MRC_TIMER_UNKNOWN = 0,
    TAF_PROP_MRC_TIMER_SCRUB = 1,
    TAF_PROP_MRC_TIMER_EFS_BACKUP = 2,
    TAF_PROP_MRC_TIMER_SUSPEND = 3,
    TAF_PROP_MRC_TIMER_DEFER = 4
} taf_prop_mrc_Timer_t;

typedef struct
{
    uint8_t processValid;
    taf_prop_mrc_Process_t process;
    uint8_t statusValid;
    taf_prop_mrc_Status_t status;
    uint8_t resultValid;
    taf_prop_mrc_Result_t result;
} taf_prop_mrc_ProcessStatusIndication_t;

typedef struct
{
    uint32_t peCountLen;
    uint32_t peCount[TAF_PROP_MRC_EFS_PARTITION_BLOCKS];
} taf_prop_mrc_EfsPeStatus_t;

typedef struct
{
    uint32_t maxEraseCount;
    uint32_t totalBadBlocks;
} taf_prop_mrc_EfsBlockStatus_t;

typedef struct
{
    uint32_t blockReadStats;
    uint32_t blockEraseStats;
    uint32_t blockEccBitflipStats;
} taf_prop_mrc_EfsBlockCounters_t;

typedef struct
{
    uint64_t writeCallCounters;
    uint64_t maxNbyte;
    uint32_t taskNameLen;
    char taskName[TAF_PROP_MRC_EFS_TASK_NAME_LEN];
} taf_prop_mrc_EfsWriteClient_t;

typedef struct
{
    uint32_t blockStatsLen;
    taf_prop_mrc_EfsBlockCounters_t blockStats[TAF_PROP_MRC_EFS_PARTITION_BLOCKS];
    uint32_t clientListLen;
    taf_prop_mrc_EfsWriteClient_t clientList[TAF_PROP_MRC_EFS_WRITE_TASKS];
} taf_prop_mrc_EfsUsageStats_t;

typedef struct taf_prop_mrc_ProcessStatusHandler* taf_prop_mrc_ProcessStatusHandlerRef_t;

typedef void (*taf_prop_mrc_ProcessStatusHdlrFunc_t)
(
    taf_prop_mrc_ProcessStatusIndication_t indication,
    void* contextPtr
);

typedef struct
{
    uint8_t slotToggleRequested;
} taf_prop_mrc_ScrubStatusIndication_t;

typedef struct taf_prop_mrc_ScrubStatusHandler* taf_prop_mrc_ScrubStatusHandlerRef_t;

typedef void (*taf_prop_mrc_ScrubStatusHdlrFunc_t)
(
    taf_prop_mrc_ScrubStatusIndication_t indication,
    void* contextPtr
);

PROP_SHARED int32_t taf_prop_mrc_Init
(
    void
);

PROP_SHARED int32_t taf_prop_mrc_RegisterIndication
(
    uint8_t registration
);

PROP_SHARED int32_t taf_prop_mrc_SetProcessStatus
(
    taf_prop_mrc_Process_t process,
    taf_prop_mrc_Status_t status
);

PROP_SHARED taf_prop_mrc_ProcessStatusHandlerRef_t taf_prop_mrc_AddProcessStatusHandler
(
    taf_prop_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PROP_SHARED int32_t taf_prop_mrc_GetEfsPeStatus
(
    taf_prop_mrc_EfsPeStatus_t* statusPtr
);

PROP_SHARED int32_t taf_prop_mrc_GetEfsBlockStatus
(
    taf_prop_mrc_EfsBlockStatus_t* statusPtr
);

PROP_SHARED int32_t taf_prop_mrc_SetTimerPeriod
(
    taf_prop_mrc_Timer_t timer,
    uint32_t period
);

PROP_SHARED taf_prop_mrc_ScrubStatusHandlerRef_t taf_prop_mrc_AddScrubStatusHandler
(
    taf_prop_mrc_ScrubStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PROP_SHARED int32_t taf_prop_mrc_GetEfsUsageStats
(
    taf_prop_mrc_EfsUsageStats_t* statsPtr
);

/**
 * Acknowledge slot toggle request.
 *
 * @param success Status code: 0 = SUCCESS, 1 = FAILED
 * @return 0 on success, negative error code on failure
 */
PROP_SHARED int32_t taf_prop_mrc_AckSlotToggle
(
    int32_t success
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PROP_MRC_H */
