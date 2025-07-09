/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_NAT_HPP
#define TAF_PA_NAT_HPP

#ifdef TAF_PA_DEFAULT
#define PA_WEAK __attribute__((weak))
#else
#define PA_WEAK
#endif

#include "taf_pa_common.h"

PA_SHARED PA_WEAK pa_result_t taf_pa_nat_Init();

#endif /* TAF_PA_NAT_HPP */

