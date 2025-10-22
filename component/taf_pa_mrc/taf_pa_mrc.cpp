/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include "taf_pa_mrc.hpp"

#include "taf_ns_mrc.h"

typedef struct
{
    void* handlerFuncPtr;
    void* contextPtr;
} Handler_t;

typedef struct
{
    Handler_t processStatus;
} Indicator_t;

class PlatformAdaptor
{
    public:
        Indicator_t indicators;

        static PlatformAdaptor& GetInstance
        (
            void
        );
};

class Utility
{
    public:
        class Convert
        {
            public:
                static taf_pa_mrc_Result_t Result
                (
                    taf_ns_mrc_Result_t result
                );

                static taf_ns_mrc_Process_t Process
                (
                    taf_pa_mrc_Process_t process
                );

                static taf_pa_mrc_Process_t Process
                (
                    taf_ns_mrc_Process_t process
                );

                static taf_ns_mrc_Status_t Status
                (
                    taf_pa_mrc_Status_t status
                );

                static taf_pa_mrc_Status_t Status
                (
                    taf_ns_mrc_Status_t status
                );

                static taf_ns_mrc_Timer_t Timer
                (
                    taf_pa_mrc_Timer_t timer
                );
        };
};

taf_pa_mrc_Result_t Utility::Convert::Result
(
    taf_ns_mrc_Result_t result
)
{
    switch (result)
    {
        case TAF_NS_MRC_RESULT_SUCCESS:
            return TAF_PA_MRC_RESULT_SUCCESS;
        case TAF_NS_MRC_RESULT_FAILURE:
            return TAF_PA_MRC_RESULT_FAILURE;
        default:
            PA_DEBUG("Unknown result %d.", result);
    }

    return TAF_PA_MRC_RESULT_UNKNOWN;
}

taf_ns_mrc_Process_t Utility::Convert::Process
(
    taf_pa_mrc_Process_t process
)
{
    switch (process)
    {
        case TAF_PA_MRC_PROCESS_ABSYNC:
            return TAF_NS_MRC_PROCESS_ABSYNC;
        case TAF_PA_MRC_PROCESS_OTA:
            return TAF_NS_MRC_PROCESS_OTA;
        default:
            PA_DEBUG("Unknown process %d.", process);
    }

    return TAF_NS_MRC_PROCESS_UNKNOWN;
}

taf_pa_mrc_Process_t Utility::Convert::Process
(
    taf_ns_mrc_Process_t process
)
{
    switch (process)
    {
        case TAF_NS_MRC_PROCESS_ABSYNC:
            return TAF_PA_MRC_PROCESS_ABSYNC;
        case TAF_NS_MRC_PROCESS_OTA:
            return TAF_PA_MRC_PROCESS_OTA;
        default:
            PA_DEBUG("Unknown process %d.", process);
    }

    return TAF_PA_MRC_PROCESS_UNKNOWN;
}

taf_ns_mrc_Status_t Utility::Convert::Status
(
    taf_pa_mrc_Status_t status
)
{
    switch (status)
    {
        case TAF_PA_MRC_STATUS_INITIATED:
            return TAF_NS_MRC_STATUS_INITIATED;
        case TAF_PA_MRC_STATUS_RESUMED:
            return TAF_NS_MRC_STATUS_RESUMED;
        case TAF_PA_MRC_STATUS_SUCCEEDED:
            return TAF_NS_MRC_STATUS_SUCCEEDED;
        case TAF_PA_MRC_STATUS_FAILED:
            return TAF_NS_MRC_STATUS_FAILED;
        default:
            PA_DEBUG("Unknown status %d.", status);
    }

    return TAF_NS_MRC_STATUS_UNKNOWN;
}

taf_pa_mrc_Status_t Utility::Convert::Status
(
    taf_ns_mrc_Status_t status
)
{
    switch (status)
    {
        case TAF_NS_MRC_STATUS_INITIATED:
            return TAF_PA_MRC_STATUS_INITIATED;
        case TAF_NS_MRC_STATUS_RESUMED:
            return TAF_PA_MRC_STATUS_RESUMED;
        case TAF_NS_MRC_STATUS_SUCCEEDED:
            return TAF_PA_MRC_STATUS_SUCCEEDED;
        case TAF_NS_MRC_STATUS_FAILED:
            return TAF_PA_MRC_STATUS_FAILED;
        default:
            PA_DEBUG("Unknown status %d.", status);
    }

    return TAF_PA_MRC_STATUS_UNKNOWN;
}

taf_ns_mrc_Timer_t Utility::Convert::Timer
(
    taf_pa_mrc_Timer_t timer
)
{
    switch (timer)
    {
        case TAF_PA_MRC_TIMER_SCRUB:
            return TAF_NS_MRC_TIMER_SCRUB;
        case TAF_PA_MRC_TIMER_EFS_BACKUP:
            return TAF_NS_MRC_TIMER_EFS_BACKUP;
        case TAF_PA_MRC_TIMER_SUSPEND:
            return TAF_NS_MRC_TIMER_SUSPEND;
        case TAF_PA_MRC_TIMER_DEFER:
            return TAF_NS_MRC_TIMER_DEFER;
        default:
            PA_DEBUG("Unknown timer %d.", timer);
    }

    return TAF_NS_MRC_TIMER_UNKNOWN;
}

PlatformAdaptor& PlatformAdaptor::GetInstance
(
    void
)
{
    static PlatformAdaptor instance;
    return instance;
}

static void ProcessStatusHandler
(
    taf_ns_mrc_ProcessStatusIndication_t indication,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFunc =
        (taf_pa_mrc_ProcessStatusHdlrFunc_t)pa.indicators.processStatus.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_mrc_ProcessStatusIndication_t paIndication;

        paIndication.processValid = indication.processValid;
        if (paIndication.processValid)
            paIndication.process = Utility::Convert::Process(indication.process);

        paIndication.statusValid = indication.statusValid;
        if (paIndication.statusValid)
            paIndication.status = Utility::Convert::Status(indication.status);

        paIndication.resultValid = indication.resultValid;
        if (paIndication.resultValid)
            paIndication.result = Utility::Convert::Result(indication.result);

        handlerFunc(paIndication, pa.indicators.processStatus.contextPtr);
    }
}

pa_result_t taf_pa_mrc_Init()
{
    PA_INFO("MRC platform adaptor initialization is done.");

    int32_t result = taf_ns_mrc_Init();
    if (result == -ENOSYS)
        PA_INFO("MRC proprietary platform adaptor is not implemented.");
    else if (result == 0)
    {
        taf_ns_mrc_AddProcessStatusHandler(ProcessStatusHandler, nullptr);
        PA_INFO("MRC proprietary platform adaptor initialization is done.");
    }

    return 0;
}

pa_result_t taf_pa_mrc_RegisterIndication
(
    uint8_t registration
)
{
    return taf_ns_mrc_RegisterIndication(registration);
}

pa_result_t taf_pa_mrc_SetProcessStatus
(
    taf_pa_mrc_Process_t process,
    taf_pa_mrc_Status_t status
)
{
    taf_ns_mrc_Process_t nsProcess = Utility::Convert::Process(process);
    taf_ns_mrc_Status_t nsStatus = Utility::Convert::Status(status);
    return taf_ns_mrc_SetProcessStatus(nsProcess, nsStatus);
}

taf_pa_mrc_ProcessStatusHandlerRef_t taf_pa_mrc_AddProcessStatusHandler
(
    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.processStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.processStatus.contextPtr = contextPtr;

    return nullptr;
}

pa_result_t taf_pa_mrc_GetEfsPeStatus
(
    taf_pa_mrc_EfsPeStatus_t* statusPtr
)
{
    taf_ns_mrc_EfsPeStatus_t status;
    int32_t result = taf_ns_mrc_GetEfsPeStatus(&status);
    uint32_t i;
    for (i = 0; i < TAF_PA_MRC_EFS_PARTITION_BLOCKS && i < TAF_NS_MRC_EFS_PARTITION_BLOCKS
        && i < status.peCountLen; i++)
        statusPtr->peCount[i] = status.peCount[i];

    statusPtr->peCountLen = i;

    return result;
}

pa_result_t taf_pa_mrc_GetEfsBlockStatus
(
    taf_pa_mrc_EfsBlockStatus_t* statusPtr
)
{
    taf_ns_mrc_EfsBlockStatus_t status;
    int32_t result = taf_ns_mrc_GetEfsBlockStatus(&status);
    statusPtr->maxEraseCount = status.maxEraseCount;
    statusPtr->totalBadBlocks = status.totalBadBlocks;

    return result;
}

pa_result_t taf_pa_mrc_SetTimerPeriod
(
    taf_pa_mrc_Timer_t timer,
    uint32_t period
)
{
    taf_ns_mrc_Timer_t nsTimer = Utility::Convert::Timer(timer);
    return taf_ns_mrc_SetTimerPeriod(nsTimer, period);
}