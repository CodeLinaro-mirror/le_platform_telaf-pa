/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_INTERNAL_COMMON_PA_H
#define TAF_INTERNAL_COMMON_PA_H

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "tafCommonPa.h"
#include "taf_prop_common.h"

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
            TAF_PA_ERROR(formatString, ##__VA_ARGS__);        \
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
            TAF_PA_ERROR(formatString, ##__VA_ARGS__);             \
            return (val);                                          \
        }                                                          \
    } while (0);

/**
 * @brief Get a thread-safe error string from an errno value.
 *
 * @note
 * This is an inline wrapper around `strerror_r` to handle both GNU and
 * POSIX variants in a thread-safe way.
 */
static inline const char *taf_pa_get_error_string(int errnum)
{
    static __thread char err_buf[256];

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
    if (strerror_r(errnum, err_buf, sizeof(err_buf)) == 0) {
        return err_buf;
    }
#else
    char *ret = strerror_r(errnum, err_buf, sizeof(err_buf));
    if (ret) {
        return ret;
    }
#endif

    snprintf(err_buf, sizeof(err_buf), "Unknown error %d", errnum);
    return err_buf;
}

//--------------------------------------------------------------------------------------------------
/**
 * If propResult is TAF_PROP_SYSTEM_ERROR, log the raw diagnostic value that was
 * returned via the underlyingErr output parameter of the PROP function call.
 *
 * The log message includes the error class name and the raw errCode value:
 *   - TAF_PROP_SYSTEM_ERROR: "system error, errCode: <int> (<strerror string>)"
 *
 * This macro is a no-op for all other result values.
 *
 * Usage:
 *   uint32_t underlyingErr = TAF_PROP_UNDERLYING_ERR_NONE;
 *   taf_prop_result_t rc = taf_prop_mrc_RegisterIndication(registration, &underlyingErr);
 *   TAF_PROP_LOG_UNDERLYING_ERROR(rc, underlyingErr);
 *   return PropResultToPaResult(rc, underlyingErr);
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PROP_LOG_UNDERLYING_ERROR(propResult, underlyingErr)                                  \
    do {                                                                                          \
        if ((propResult) == TAF_PROP_SYSTEM_ERROR)                                                \
        {                                                                                         \
            TAF_PA_ERROR("system error, errCode: %u (%s)", (unsigned)(underlyingErr),                 \
                     taf_pa_get_error_string((int)(underlyingErr)));                                \
        }                                                                                         \
    } while (0)

//--------------------------------------------------------------------------------------------------
/**
 * Map a raw POSIX errno value to the appropriate taf_pa_result_t as documented
 * in tafCommonPa.h. Unknown errno values map to TAF_PA_FAULT.
 */
//--------------------------------------------------------------------------------------------------
static inline taf_pa_result_t ErrnoToPaResult(uint32_t errno_val)
{
    switch ((int)errno_val)
    {
        case ENOENT:      return TAF_PA_NOT_FOUND;
        case ERANGE:      return TAF_PA_OUT_OF_RANGE;
        case ENOMEM:      return TAF_PA_NO_MEMORY;
        case EPERM:
        case EACCES:      return TAF_PA_NOT_PERMITTED;
        case EFAULT:      return TAF_PA_FAULT;
        case ETIMEDOUT:   return TAF_PA_TIMEOUT;
        case ENOSPC:
        case EOVERFLOW:   return TAF_PA_OVERFLOW;
        case EAGAIN:      return TAF_PA_WOULD_BLOCK; /* EWOULDBLOCK == EAGAIN on Linux */
        case EDEADLK:     return TAF_PA_DEADLOCK;
        case EILSEQ:
        case EBADMSG:     return TAF_PA_FORMAT_ERROR;
        case EEXIST:      return TAF_PA_DUPLICATE;
        case EINVAL:      return TAF_PA_BAD_PARAMETER;
        case EBADF:
        case EPIPE:       return TAF_PA_CLOSED;
        case EBUSY:       return TAF_PA_BUSY;
        case ENOTSUP:     return TAF_PA_UNSUPPORTED; /* EOPNOTSUPP == ENOTSUP on Linux */
        case EIO:         return TAF_PA_IO_ERROR;
        case ENOSYS:      return TAF_PA_NOT_IMPLEMENTED;
        case ENODEV:
        case ENXIO:       return TAF_PA_UNAVAILABLE;
        case ECANCELED:   return TAF_PA_TERMINATED;
        case EINPROGRESS:
        case EALREADY:    return TAF_PA_IN_PROGRESS;
        default:          return TAF_PA_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert a prop-layer result code (taf_prop_result_t) to a PA-layer result code
 * (taf_pa_result_t).
 *
 * Both types share the same numeric values for codes 0 to -24.
 * TAF_PROP_SYSTEM_ERROR maps to the taf_pa_result_t value corresponding to the
 * raw POSIX errno in underlyingErr (via ErrnoToPaResult). Unknown values map to
 * TAF_PA_FAULT.
 *
 * Call TAF_PROP_LOG_UNDERLYING_ERROR before this function to capture raw
 * diagnostic detail in the log.
 */
//--------------------------------------------------------------------------------------------------
static inline taf_pa_result_t PropResultToPaResult(
    taf_prop_result_t propResult, uint32_t underlyingErr)
{
    switch (propResult)
    {
        case TAF_PROP_OK:              return TAF_PA_OK;
        case TAF_PROP_NOT_FOUND:       return TAF_PA_NOT_FOUND;
        case TAF_PROP_NOT_POSSIBLE:    return TAF_PA_NOT_POSSIBLE;
        case TAF_PROP_OUT_OF_RANGE:    return TAF_PA_OUT_OF_RANGE;
        case TAF_PROP_NO_MEMORY:       return TAF_PA_NO_MEMORY;
        case TAF_PROP_NOT_PERMITTED:   return TAF_PA_NOT_PERMITTED;
        case TAF_PROP_FAULT:           return TAF_PA_FAULT;
        case TAF_PROP_COMM_ERROR:      return TAF_PA_COMM_ERROR;
        case TAF_PROP_TIMEOUT:         return TAF_PA_TIMEOUT;
        case TAF_PROP_OVERFLOW:        return TAF_PA_OVERFLOW;
        case TAF_PROP_UNDERFLOW:       return TAF_PA_UNDERFLOW;
        case TAF_PROP_WOULD_BLOCK:     return TAF_PA_WOULD_BLOCK;
        case TAF_PROP_DEADLOCK:        return TAF_PA_DEADLOCK;
        case TAF_PROP_FORMAT_ERROR:    return TAF_PA_FORMAT_ERROR;
        case TAF_PROP_DUPLICATE:       return TAF_PA_DUPLICATE;
        case TAF_PROP_BAD_PARAMETER:   return TAF_PA_BAD_PARAMETER;
        case TAF_PROP_CLOSED:          return TAF_PA_CLOSED;
        case TAF_PROP_BUSY:            return TAF_PA_BUSY;
        case TAF_PROP_UNSUPPORTED:     return TAF_PA_UNSUPPORTED;
        case TAF_PROP_IO_ERROR:        return TAF_PA_IO_ERROR;
        case TAF_PROP_NOT_IMPLEMENTED: return TAF_PA_NOT_IMPLEMENTED;
        case TAF_PROP_UNAVAILABLE:     return TAF_PA_UNAVAILABLE;
        case TAF_PROP_TERMINATED:      return TAF_PA_TERMINATED;
        case TAF_PROP_IN_PROGRESS:     return TAF_PA_IN_PROGRESS;
        case TAF_PROP_SUSPENDED:       return TAF_PA_SUSPENDED;
        case TAF_PROP_SYSTEM_ERROR:    return ErrnoToPaResult(underlyingErr);
        default:                       return TAF_PA_FAULT;
    }
}

#endif // TAF_INTERNAL_COMMON_PA_H
