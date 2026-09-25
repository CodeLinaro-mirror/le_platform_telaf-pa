/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_common.h"

void taf_prop_common_LogSetlevel
(
    taf_prop_common_LogLevel_t level
)
{
}

void taf_prop_common_LogMessage
(
    taf_prop_common_LogLevel_t level,
    const char* file,
    const char* func,
    int line,
    const char* fmt,
    ...
)
{
}

void taf_prop_common_LogBind(const taf_prop_common_LogVtable_t* vt)
{
}
