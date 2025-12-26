/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_SIM_HPP
#define TAF_PA_SIM_HPP

#include "taf_pa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PA_SIM_RESULT_OK 0
#define TAF_PA_SIM_RESULT_FAULT -6
#define TAF_PA_SIM_RESULT_TIMEOUT -8
#define TAF_PA_SIM_RESULT_BAD_PARAMETER -15
#define TAF_PA_SIM_RESULT_UNSUPPORTED -18
#define TAF_PA_SIM_RESULT_NOT_IMPLEMENTED -20

#define MAX_SIM_PATH 10
#define MAX_SIM_REFRESH_FILES 100

typedef enum
{
    TAF_PA_SIM_SESSION_TYPE_UNKNOWN = -1,
    TAF_PA_SIM_SESSION_TYPE_PRI_GW_PROV = 0,
    TAF_PA_SIM_SESSION_TYPE_SEC_GW_PROV = 1,
    TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_1 = 2,
    TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_2 = 3
}taf_pa_sim_SessionType_t;

typedef enum
{
    TAF_PA_SIM_REFRESH_MODE_UNKNOWN = -1,
    TAF_PA_SIM_REFRESH_MODE_RESET = 0,
    TAF_PA_SIM_REFRESH_MODE_INIT = 1,
    TAF_PA_SIM_REFRESH_MODE_INIT_FCN = 2,
    TAF_PA_SIM_REFRESH_MODE_FCN = 3,
    TAF_PA_SIM_REFRESH_MODE_INIT_FULL_FCN =4,
    TAF_PA_SIM_REFRESH_MODE_APP_RESET = 5,
    TAF_PA_SIM_REFRESH_MODE_3G_RESET = 6
}taf_pa_sim_RefreshMode_t;

typedef enum
{
    TAF_PA_SIM_REFRESH_STAGE_UNKNOWN = -1,
    TAF_PA_SIM_REFRESH_STAGE_WAIT_FOR_OK = 0,
    TAF_PA_SIM_REFRESH_STAGE_START = 1,
    TAF_PA_SIM_REFRESH_STAGE_END_WITH_SUCCESS = 2,
    TAF_PA_SIM_REFRESH_STAGE_END_WITH_FAILURE = 3
}taf_pa_sim_RefreshStage_t;

typedef struct {
   taf_pa_sim_SessionType_t sessionType;
   taf_pa_sim_RefreshMode_t refreshMode;
   taf_pa_sim_RefreshStage_t refreshStage;
}taf_pa_sim_RefreshChangeInd_t;

typedef struct {
    uint16_t file_id;
    uint32_t path_len;
    uint8_t path[MAX_SIM_PATH];
}taf_pa_sim_RefreshFile_t;

typedef void (*taf_pa_sim_RefreshChangeHandlerFunc_t)
(
   taf_pa_sim_RefreshChangeInd_t refreshChangeInd, void* contextPtr
);

typedef struct taf_pa_sim_RefreshChangeHandler* taf_pa_sim_RefreshChangeHandlerRef_t;

/**
 * Initialize SIM platform adaptor.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_sim_Init(void);

//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh register.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_sim_RefreshRegister
(
    taf_pa_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_pa_sim_RefreshFile_t* files
);

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh ok.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_sim_RefreshOk
(
    taf_pa_sim_SessionType_t sessionType,
    bool* refreshAllow
);

//--------------------------------------------------------------------------------------------------
/**
 *  UIM refresh complete.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_sim_RefreshComplete
(
   taf_pa_sim_SessionType_t sessionType
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler for SIM refresh
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK taf_pa_sim_RefreshChangeHandlerRef_t taf_pa_sim_AddRefreshChangeHandler
(
    taf_pa_sim_RefreshChangeHandlerFunc_t handlerFuncPtr,
    void* contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler for SIM refresh
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK void taf_pa_sim_RemoveRefreshChangeHandler
(
   taf_pa_sim_RefreshChangeHandlerRef_t handlerRef ///< [IN] Handler reference.
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PROP_SIM_HPP */
