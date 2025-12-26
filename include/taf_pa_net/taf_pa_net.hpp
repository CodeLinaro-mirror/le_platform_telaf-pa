/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_NET_HPP
#define TAF_PA_NET_HPP

#ifdef TAF_PA_DEFAULT
#define PA_WEAK __attribute__((weak))
#else
#define PA_WEAK
#endif

#include "taf_pa_common.h"

PA_SHARED PA_WEAK pa_result_t taf_pa_net_Init();

//--------------------------------------------------------------------------------------------------
/**
 * Get phone ID from slot ID
 *
 * @return PA_OK            Success
 *         PA_BAD_PARAMETER Invalid parameter
 *         PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetPhoneIdFromSlotId
(
    uint8_t slotId,      ///< [IN] Slot ID
    uint8_t *phoneIdPtr  ///< [OUT] Phone ID
);

//--------------------------------------------------------------------------------------------------
/**
 * Get slot ID from phone ID
 *
 * @return PA_OK            Success
 *         PA_BAD_PARAMETER Invalid parameter
 *         PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetSlotIdFromPhoneId
(
    uint8_t phoneId,     ///< [IN] Phone ID
    uint8_t *slotIdPtr   ///< [OUT] Slot ID
);

#endif /* TAF_PA_NET_HPP */
