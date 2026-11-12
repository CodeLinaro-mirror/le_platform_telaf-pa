/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_ABSLOT_H
#define TAF_PA_ABSLOT_H

#include "tafCommonPa.h"

#ifdef __cplusplus
extern "C" {
#endif

// Internal common helper implemented by libtaf_pa_abslot_common.a and linked into PA SOs.
taf_pa_result_t taf_pa_abslot_GetActiveSlot
(
    taf_pa_ABSlot_t* slotPtr
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PA_ABSLOT_H */
