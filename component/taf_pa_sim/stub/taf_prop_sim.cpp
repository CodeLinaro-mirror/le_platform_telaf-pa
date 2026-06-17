/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

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
    return TAF_PROP_SIM_RESULT_NOT_SUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh deregister.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_sim_Result_t taf_prop_sim_RefreshDeregister
(
    taf_prop_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_prop_sim_RefreshFile_t* files
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_NOT_SUPPORTED;
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
    return TAF_PROP_SIM_RESULT_NOT_SUPPORTED;
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
    return TAF_PROP_SIM_RESULT_NOT_SUPPORTED;
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
    return nullptr;
}

taf_prop_sim_ProfileInfo_t taf_prop_sim_GetActiveSimProfile
(
    uint8_t slot
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    taf_prop_sim_ProfileInfo_t info{};
    info.profileId   = 0;
    info.profileType = TAF_PROP_SIM_PROFILE_TYPE_UNKNOWN;
    info.isActive    = false;
    return info;
}

taf_prop_sim_Result_t taf_prop_sim_SetSimProfileById
(
    uint8_t slot,
    uint8_t profileId
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_BAD_PARAMETER;
}

taf_prop_sim_Result_t taf_prop_sim_GetProfileList
(
    uint8_t slot,
    taf_prop_sim_ProfileInfo_t *profiles,
    uint32_t *profilesLenPtr
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_SIM_RESULT_BAD_PARAMETER;
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
    return TAF_PROP_SIM_RESULT_NOT_SUPPORTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * SIM prop deinitialization.
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_sim_Deinit
(
    void
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return -ENOSYS;
}