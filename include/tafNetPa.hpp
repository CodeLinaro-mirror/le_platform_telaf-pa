/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NET_PA_HPP
#define TAF_NET_PA_HPP

#include <vector>
#include "tafCommonPa.h"

PA_SHARED pa_result_t taf_pa_net_Init();

PA_SHARED pa_result_t taf_pa_net_Deinit();

//--------------------------------------------------------------------------------------------------
/**
 * Get phone ID from slot ID
 *
 * @return PA_OK            Success
 *         PA_BAD_PARAMETER Invalid parameter
 *         PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_GetPhoneIdFromSlotId
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
PA_SHARED pa_result_t taf_pa_net_GetSlotIdFromPhoneId
(
    uint8_t phoneId,     ///< [IN] Phone ID
    uint8_t *slotIdPtr   ///< [OUT] Slot ID
);

//--------------------------------------------------------------------------------------------------
/**
 * Get supported slot IDs
 *
 * @return PA_OK            Success
 *         PA_BAD_PARAMETER Invalid parameter
 *         PA_FAULT         Failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_GetSupportedSlotIds
(
    std::vector<uint8_t> &slotIds   ///< [OUT] Slot IDs
);

#endif /* TAF_NET_PA_HPP */
