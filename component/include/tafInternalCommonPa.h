/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_INTERNAL_COMMON_PA_H
#define TAF_INTERNAL_COMMON_PA_H

#include <inttypes.h>
#include <string.h>

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

/**
 * @brief Secure memory copy — copies MIN(dst_size, src_size) bytes.
 *
 * Prevents buffer overflow by clamping the copy to the destination buffer
 * size. Returns the number of bytes actually copied; if returned < src_size,
 * truncation occurred and the caller should handle it.
 *
 * @param dst       Destination buffer (must be non-NULL).
 * @param dst_size  Size of the destination buffer in bytes.
 * @param src       Source buffer (must be non-NULL).
 * @param src_size  Number of bytes requested to copy from src.
 * @return          Bytes copied = MIN(dst_size, src_size). Returns 0 if dst or src is NULL.
 */
static inline size_t taf_pa_memscpy(void *dst, size_t dst_size,
                                    const void *src, size_t src_size)
{
    if (!dst || !src) return 0;
    size_t copy_size = (dst_size <= src_size) ? dst_size : src_size;
    if (copy_size > 0)
        memcpy(dst, src, copy_size);

    return copy_size;
}

#endif // TAF_INTERNAL_COMMON_PA_H

