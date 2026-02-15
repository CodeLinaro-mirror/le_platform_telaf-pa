/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include "taf_ns_mrc.h"

taf_ns_mrc_ScrubStatusHandlerRef_t taf_ns_mrc_AddScrubStatusHandler
(
    taf_ns_mrc_ScrubStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    NS_INFO("Function is not implemented in stub PA.");
    return nullptr;
}

int32_t taf_ns_mrc_AckSlotToggle
(
    int32_t success
)
{
    NS_INFO("Function is not implemented in stub PA.");
    return -ENOSYS;
}

int32_t taf_ns_mrc_Init
(
    void
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

int32_t taf_ns_mrc_RegisterIndication
(
    uint8_t registration
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

int32_t taf_ns_mrc_SetProcessStatus
(
    taf_ns_mrc_Process_t process,
    taf_ns_mrc_Status_t status
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

taf_ns_mrc_ProcessStatusHandlerRef_t taf_ns_mrc_AddProcessStatusHandler
(
    taf_ns_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return nullptr;
}

int32_t taf_ns_mrc_GetEfsPeStatus
(
    taf_ns_mrc_EfsPeStatus_t* statusPtr
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

int32_t taf_ns_mrc_GetEfsBlockStatus
(
    taf_ns_mrc_EfsBlockStatus_t* statusPtr
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

int32_t taf_ns_mrc_SetTimerPeriod
(
    taf_ns_mrc_Timer_t timer,
    uint32_t period
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}
