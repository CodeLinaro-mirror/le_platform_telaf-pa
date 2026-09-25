/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * ABI STABILITY REQUIREMENT
 * This header is part of the prop interface.
 * All declarations must use C-style linkage only (no C++ classes,
 * templates, references, or overloaded functions) to guarantee ABI
 * stability across independently compiled shared libraries.
 */

#ifndef TAF_PROP_SIM_H
#define TAF_PROP_SIM_H

/* Standard integer and boolean types */
#include <stdint.h>
#include <stdbool.h>

#include "taf_prop_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PROP_SIM_MAX_SIM_PATH 10
#define TAF_PROP_SIM_MAX_SIM_REFRESH_FILES 100


typedef enum
{
    TAF_PROP_SIM_SESSION_TYPE_UNKNOWN = -1,
    TAF_PROP_SIM_SESSION_TYPE_PRI_GW_PROV = 0,
    TAF_PROP_SIM_SESSION_TYPE_SEC_GW_PROV = 1,
    TAF_PROP_SIM_SESSION_TYPE_CARD_ON_SLOT_1 = 2,
    TAF_PROP_SIM_SESSION_TYPE_CARD_ON_SLOT_2 = 3
}taf_prop_sim_SessionType_t;

typedef enum
{
    TAF_PROP_SIM_REFRESH_MODE_UNKNOWN = -1,
    TAF_PROP_SIM_REFRESH_MODE_RESET = 0,
    TAF_PROP_SIM_REFRESH_MODE_INIT = 1,
    TAF_PROP_SIM_REFRESH_MODE_INIT_FCN = 2,
    TAF_PROP_SIM_REFRESH_MODE_FCN = 3,
    TAF_PROP_SIM_REFRESH_MODE_INIT_FULL_FCN =4,
    TAF_PROP_SIM_REFRESH_MODE_APP_RESET = 5,
    TAF_PROP_SIM_REFRESH_MODE_3G_RESET = 6
}taf_prop_sim_RefreshMode_t;

typedef enum
{
    TAF_PROP_SIM_REFRESH_STAGE_UNKNOWN = -1,
    TAF_PROP_SIM_REFRESH_STAGE_WAIT_FOR_OK = 0,
    TAF_PROP_SIM_REFRESH_STAGE_START = 1,
    TAF_PROP_SIM_REFRESH_STAGE_END_WITH_SUCCESS = 2,
    TAF_PROP_SIM_REFRESH_STAGE_END_WITH_FAILURE = 3
}taf_prop_sim_RefreshStage_t;

typedef struct
{
    uint16_t file_id;
    uint32_t path_len;
    uint8_t path[TAF_PROP_SIM_MAX_SIM_PATH];
}taf_prop_sim_RefreshFile_t;

typedef struct
{
    taf_prop_sim_SessionType_t sessionType;
    taf_prop_sim_RefreshMode_t refreshMode;
    taf_prop_sim_RefreshStage_t refreshStage;
    uint32_t filesLen;
    taf_prop_sim_RefreshFile_t files[TAF_PROP_SIM_MAX_SIM_REFRESH_FILES];
}taf_prop_sim_RefreshChangeInd_t;

typedef void (*taf_prop_sim_RefreshChangeHandlerFunc_t)
(
    taf_prop_sim_RefreshChangeInd_t refreshChangeInd,
    void* contextPtr
);

typedef struct taf_prop_sim_RefreshChangeHandler* taf_prop_sim_RefreshChangeHandlerRef_t;

typedef enum
{
    TAF_PROP_SIM_PROFILE_TYPE_UNKNOWN   = -1,
    TAF_PROP_SIM_PROFILE_TYPE_REGULAR   = 0,
    TAF_PROP_SIM_PROFILE_TYPE_EMERGENCY = 1,
} taf_prop_sim_ProfileType_t;

typedef struct
{
    uint8_t                     profileId;
    taf_prop_sim_ProfileType_t  profileType;
    bool                        isActive;
} taf_prop_sim_ProfileInfo_t;

TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_GetProfileList
(
    uint8_t slot,
    taf_prop_sim_ProfileInfo_t *profiles,
    uint32_t *profilesLenPtr
);

TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_GetActiveSimProfile
(
    uint8_t slot,
    taf_prop_sim_ProfileInfo_t *profileInfoPtr
);

TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_SetSimProfileById
(
    uint8_t slot,
    uint8_t profileId
);

//--------------------------------------------------------------------------------------------------
/**
 *  SIM prop initialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 *  SIM prop deinitialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_Deinit
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh register.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_RefreshRegister
(
    taf_prop_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_prop_sim_RefreshFile_t* files
);

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh deregister.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_RefreshDeregister
(
    taf_prop_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_prop_sim_RefreshFile_t* files
);

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh ok.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_RefreshOk
(
    taf_prop_sim_SessionType_t sessionType,
    bool* refreshAllow
);

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh complete.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_sim_RefreshComplete
(
    taf_prop_sim_SessionType_t sessionType
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler for SIM refresh
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_sim_RefreshChangeHandlerRef_t taf_prop_sim_AddRefreshChangeHandler
(
    taf_prop_sim_RefreshChangeHandlerFunc_t handlerFuncPtr,
    void* contextPtr
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PROP_SIM_H */
