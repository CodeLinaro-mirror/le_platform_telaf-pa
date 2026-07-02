/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_DEVICEINFO_PA_HPP
#define TAF_DEVICEINFO_PA_HPP

#include <chrono>
#include <future>
#include <unistd.h>
#include <any>
#include "tafCommonPa.h"

namespace tafpa::deviceinfo{

TAF_PA_SHARED taf_pa_result_t taf_pa_deviceinfo_Init();

TAF_PA_SHARED taf_pa_result_t taf_pa_deviceinfo_Deinit();

TAF_PA_SHARED taf_pa_result_t taf_pa_deviceinfo_GetIMEI(char* imeiPtr, size_t numElements);

}

#endif /* TAF_DEVICEINFO_PA_HPP */
