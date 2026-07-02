/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include "taf_prop_sim.h"

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh register.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_sim_RefreshRegister
(
    taf_prop_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_prop_sim_RefreshFile_t* files
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh deregister.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_sim_RefreshDeregister
(
    taf_prop_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_prop_sim_RefreshFile_t* files
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh ok.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_sim_RefreshOk
(
    taf_prop_sim_SessionType_t sessionType,
    bool* refreshAllow
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh complete.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_sim_RefreshComplete
(
    taf_prop_sim_SessionType_t sessionType
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
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
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return nullptr;
}

taf_prop_result_t taf_prop_sim_GetActiveSimProfile
(
    uint8_t slot,
    taf_prop_sim_ProfileInfo_t *profileInfoPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    if (profileInfoPtr)
    {
        profileInfoPtr->profileId   = 0;
        profileInfoPtr->profileType = TAF_PROP_SIM_PROFILE_TYPE_UNKNOWN;
        profileInfoPtr->isActive    = false;
    }
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_sim_SetSimProfileById
(
    uint8_t slot,
    uint8_t profileId
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_sim_GetProfileList
(
    uint8_t slot,
    taf_prop_sim_ProfileInfo_t *profiles,
    uint32_t *profilesLenPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * SIM prop initialization.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_sim_Init
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * SIM prop deinitialization.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_sim_Deinit
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}
