/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_INTERNAL_COMMON_PA_H
#define TAF_INTERNAL_COMMON_PA_H

#include <inttypes.h>

//--------------------------------------------------------------------------------------------------
/**
 * Mark a variable as unused.
 *
 */
//--------------------------------------------------------------------------------------------------
#define PA_UNUSED(v) ((void)(v))

//--------------------------------------------------------------------------------------------------
/**
 * Return from function if condition is true.
 *
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_ERROR_IF_RET_NIL(condition, formatString, ...) \
    do                                                        \
    {                                                         \
        if (condition)                                        \
        {                                                     \
            PA_ERROR(formatString, ##__VA_ARGS__);            \
            return;                                           \
        }                                                     \
    } while (0);

//--------------------------------------------------------------------------------------------------
/**
 * Return specified value from function if condition is true.
 *
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_ERROR_IF_RET_VAL(condition, val, formatString, ...) \
    do                                                             \
    {                                                              \
        if (condition)                                             \
        {                                                          \
            PA_ERROR(formatString, ##__VA_ARGS__);                 \
            return (val);                                          \
        }                                                          \
    } while (0);

#endif // TAF_INTERNAL_COMMON_PA_H