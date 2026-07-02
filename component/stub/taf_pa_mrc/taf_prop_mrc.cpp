/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_mrc.h"

taf_prop_mrc_ScrubStatusHandlerRef_t taf_prop_mrc_AddScrubStatusHandler
(
    taf_prop_mrc_ScrubStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return nullptr;
}

taf_prop_result_t taf_prop_mrc_AckSlotToggle
(
    int32_t success
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_Init
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_Deinit
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_RegisterIndication
(
    uint8_t registration
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_SetProcessStatus
(
    taf_prop_mrc_Process_t process,
    taf_prop_mrc_Status_t status
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_mrc_ProcessStatusHandlerRef_t taf_prop_mrc_AddProcessStatusHandler
(
    taf_prop_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return nullptr;
}

taf_prop_result_t taf_prop_mrc_GetEfsPeStatus
(
    taf_prop_mrc_EfsPeStatus_t* statusPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_GetEfsBlockStatus
(
    taf_prop_mrc_EfsBlockStatus_t* statusPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_GetEfsUsageStats
(
    taf_prop_mrc_EfsUsageStats_t* statsPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_mrc_SetTimerPeriod
(
    taf_prop_mrc_Timer_t timer,
    uint32_t period
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}
