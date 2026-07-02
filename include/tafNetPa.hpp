/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NET_PA_HPP
#define TAF_NET_PA_HPP

#include <vector>
#include "tafCommonPa.h"

TAF_PA_SHARED taf_pa_result_t taf_pa_net_Init();

TAF_PA_SHARED taf_pa_result_t taf_pa_net_Deinit();

//--------------------------------------------------------------------------------------------------
/**
 * Get phone ID from slot ID
 *
 * @return TAF_PA_OK            Success
 *         TAF_PA_BAD_PARAMETER Invalid parameter
 *         TAF_PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_net_GetPhoneIdFromSlotId
(
    uint8_t slotId,      ///< [IN] Slot ID
    uint8_t *phoneIdPtr  ///< [OUT] Phone ID
);

//--------------------------------------------------------------------------------------------------
/**
 * Get slot ID from phone ID
 *
 * @return TAF_PA_OK            Success
 *         TAF_PA_BAD_PARAMETER Invalid parameter
 *         TAF_PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_net_GetSlotIdFromPhoneId
(
    uint8_t phoneId,     ///< [IN] Phone ID
    uint8_t *slotIdPtr   ///< [OUT] Slot ID
);

//--------------------------------------------------------------------------------------------------
/**
 * Get supported slot IDs
 *
 * @return TAF_PA_OK            Success
 *         TAF_PA_BAD_PARAMETER Invalid parameter
 *         TAF_PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_net_GetSupportedSlotIds
(
    std::vector<uint8_t> &slotIds   ///< [OUT] Slot IDs
);

#endif /* TAF_NET_PA_HPP */
