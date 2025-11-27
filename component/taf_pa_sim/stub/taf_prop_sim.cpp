/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_sim.hpp"

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh register.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_sim_Result_t taf_prop_sim_RefreshRegister
(
    taf_prop_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_prop_sim_RefreshFile_t* files
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_BAD_PARAMETER;
}

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh ok.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_sim_Result_t taf_prop_sim_RefreshOk
(
    taf_prop_sim_SessionType_t sessionType,
    bool* refreshAllow
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_BAD_PARAMETER;
}

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh complete.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_sim_Result_t taf_prop_sim_RefreshComplete
(
    taf_prop_sim_SessionType_t sessionType
)
{   PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_BAD_PARAMETER;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler for SIM refresh
 */
//--------------------------------------------------------------------------------------------------
taf_prop_sim_RefreshChangeHandlerRef_t taf_prop_sim_AddRefreshChangeHandler
(
    taf_prop_sim_RefreshChangeHandlerFunc_t handlerFuncPtr,
    void* contextPtr
)
{
	PROP_INFO("Function is not implemented in stub PA.");
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler for SIM refresh
 */
//-------------------------------------------------------------------------------------------------
taf_prop_sim_Result_t taf_prop_sim_Init
(
    void
)
{
    
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_BAD_PARAMETER;
}