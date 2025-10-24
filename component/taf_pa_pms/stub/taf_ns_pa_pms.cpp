/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_ns_pa_pms.hpp"

NsPaType(Result) taf_ns_pa_pms_Init
(
    taf_ns_pa_pms_MpssRef_t   *mpssRefPtr,
    taf_ns_pa_pms_ErrCallback  errCbFn,
    void *                     errCbCtx
)
{
    NS_INFO("NOT_IMPLEMENTED");
    return NsPaResult(NOT_IMPLEMENTED);
}

NsPaType(Result) taf_ns_pa_pms_Deinit
(
    taf_ns_pa_pms_MpssRef_t * mpssRefPtr
)
{
    NS_INFO("NOT_IMPLEMENTED");
    return NsPaResult(NOT_IMPLEMENTED);
}

NsPaType(Result) taf_ns_pa_pms_SetWsFilter
(
    taf_ns_pa_pms_MpssRef_t mpssRef,
    taf_ns_pa_pms_ModemWakeupSource_t bitset
)
{
    NS_INFO("NOT_IMPLEMENTED");
    return NsPaResult(NOT_IMPLEMENTED);
}

NsPaType(Result) taf_ns_pa_pms_GetWsFilter
(
    taf_ns_pa_pms_MpssRef_t mpssRef,
    taf_ns_pa_pms_ModemWakeupSource_t *bitset
)
{
    NS_INFO("NOT_IMPLEMENTED");
    return NsPaResult(NOT_IMPLEMENTED);
}

NsPaType(Result) taf_ns_pa_pms_EnableAllWs
(
    taf_ns_pa_pms_MpssRef_t mpssRef
)
{
    NS_INFO("NOT_IMPLEMENTED");
    return NsPaResult(NOT_IMPLEMENTED);
}
