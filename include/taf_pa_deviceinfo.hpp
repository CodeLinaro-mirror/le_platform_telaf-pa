/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. 
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_DEVICEINFO_H
#define TAF_PA_DEVICEINFO_H

#include <chrono>
#include <future>
#include <unistd.h>
#include <any>
#include "taf_pa_common.h"

namespace tafpa::deviceinfo{

PA_SHARED PA_WEAK pa_result_t taf_pa_deviceinfo_Init();

PA_SHARED PA_WEAK pa_result_t taf_pa_deviceinfo_GetIMEI(char* imeiPtr, size_t numElements);

}

#endif /* TAF_PA_DEVICEINFO_H */
