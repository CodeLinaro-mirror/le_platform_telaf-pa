/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __TAF_NS_PA_PMS_HPP__
#define __TAF_NS_PA_PMS_HPP__

#include "taf_ns_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NsPaType(x) taf_ns_pa_pms_ ## x
#define NsPaResult(tag) taf_ns_pa_pms_Result ## tag

typedef enum {
    NsPaResult(OK),
    NsPaResult(FAULT),
    NsPaResult(TIMEOUT),
    NsPaResult(NOT_FOUND),
    NsPaResult(BAD_PARAMETER),
    NsPaResult(SVC_UNAVAILABLE),
    NsPaResult(NOT_IMPLEMENTED),

    NsPaResult(MAX),
} NsPaType(Result);

//--------------------------------------------------------------------------------------------------
/**
 * The reference type for no-ship pms Mpss active-object
 */
//--------------------------------------------------------------------------------------------------
typedef struct taf_ns_pa_pms_Mpss * taf_ns_pa_pms_MpssRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Error codes for callback function
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    /* QMI service is unavailable */
    TAF_NS_PA_PMS_ERR_SVC_GONE = 0,

} taf_ns_pa_pms_ErrCode_t;

//--------------------------------------------------------------------------------------------------
/**
 * The collection of bitset about Modem wakeup sources
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    MODEM_WS_INCOMING_SMS     = 0x0001,
    MODEM_WS_INCOMING_VCALL   = 0x0002,
    MODEM_WS_SIM_PROFILE_SWAP = 0x0004,

} taf_ns_pa_pms_ModemWakeupSource_t;

//--------------------------------------------------------------------------------------------------
/**
 * Parse the command-line arguments for options.
 */
//--------------------------------------------------------------------------------------------------
typedef void ( * taf_ns_pa_pms_ErrCallback )
(
    taf_ns_pa_pms_ErrCode_t errCode,
    void * errCbCtx
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the dependent QMI client and return the reference handle for other APIs.
 *
 * @return
 *  - NsPaResult(OK)            Function succeeded.
 *  - NsPaResult(BAD_PARAMETER) Bad parameters.
 *  - NsPaResult(DUPLICATE)     Duplicated to call this function.
 *  - NsPaResult(FAULT)         Failed to initialize the QMI client for remote QMI service.
 *
 * @b NOTE: DO NOT call taf_ns_pa_pms_XXX APIs in callback function, otherwise deadlock will occur!
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED NsPaType(Result) taf_ns_pa_pms_Init
(
    taf_ns_pa_pms_MpssRef_t * mpssRefPtr,  ///< [IN/OUT] Reference handle for active-object.
    taf_ns_pa_pms_ErrCallback errCbFn,     ///< [IN] Error notification callback function.
    void * errCbCtx                        ///< [IN] Callback function context.
);

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize the dependent QMI client and release the passed reference pointer.
 *
 * @return
 *  - NsPaResult(OK)            Function succeeded.
 *  - NsPaResult(BAD_PARAMETER) Bad parameters.
 *  - NsPaResult(NOT_PERMITTED) Can not perform this function due to some bad preconditions.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED NsPaType(Result) taf_ns_pa_pms_Deinit
(
    taf_ns_pa_pms_MpssRef_t * mpssRefPtr   ///< [IN] Pointer for the recorded reference handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the wakeup source filter(bitset) for specific MPSS services.
 *
 * @return
 *  - NsPaResult(OK)            Function succeeded.
 *  - NsPaResult(BAD_PARAMETER) Bad parameters.
 *  - NsPaResult(NOT_PERMITTED) Can not perform this function due to some bad preconditions.
 *  - NsPaResult(TIMEOUT)       Not received the indication from the QMI service.
 *  - NsPaResult(FAULT)         Failed to set the bitset to the remote service.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED NsPaType(Result) taf_ns_pa_pms_SetWsFilter
(
    taf_ns_pa_pms_MpssRef_t mpssRef,         ///< [IN] Recorded reference from _Init API.
    taf_ns_pa_pms_ModemWakeupSource_t bitset ///< [IN] Filter bitset for special indications.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the wakeup source filter(bitset) from remote QMI PDC service.
 *
 * @return
 *  - NsPaResult(OK)            Function succeeded.
 *  - NsPaResult(BAD_PARAMETER) Bad parameters.
 *  - NsPaResult(NOT_PERMITTED) Can not perform this function due to some bad preconditions.
 *  - NsPaResult(TIMEOUT)       Not received the indication from the QMI service.
 *  - NsPaResult(FAULT)         Failt to get the bitset from the remote service.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED NsPaType(Result) taf_ns_pa_pms_GetWsFilter
(
    taf_ns_pa_pms_MpssRef_t mpssRef,          ///< [IN]  Recorded reference from _Init API.
    taf_ns_pa_pms_ModemWakeupSource_t *bitset ///< [OUT] Configured filter bitset in remote sevice
);

//--------------------------------------------------------------------------------------------------
/**
 * Enable all wakeup source indications for MPSS.
 *
 * @return
 *  - NsPaResult(OK)            Function succeeded.
 *  - NsPaResult(BAD_PARAMETER) Bad parameters.
 *  - NsPaResult(NOT_PERMITTED) Can not perform this function due to some bad preconditions.
 *  - NsPaResult(TIMEOUT)       Not received the indication from the QMI service.
 *  - NsPaResult(FAULT)         Failed to enable all indications for MPSS side.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED NsPaType(Result) taf_ns_pa_pms_EnableAllWs
(
    taf_ns_pa_pms_MpssRef_t mpssRef
);

#ifdef __cplusplus
}
#endif

#endif
