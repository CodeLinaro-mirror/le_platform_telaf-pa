/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_COMMON_PA_H
#define TAF_COMMON_PA_H

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    /// Success
    TAF_PA_OK = 0,
    /// Resource not found [maps from -ENOENT (-2)]
    TAF_PA_NOT_FOUND = -1,
    /// Operation not possible in current state
    TAF_PA_NOT_POSSIBLE = -2,
    /// Value out of allowed range [maps from -ERANGE (-34)]
    TAF_PA_OUT_OF_RANGE = -3,
    /// Memory allocation failure [maps from -ENOMEM (-12)]
    TAF_PA_NO_MEMORY = -4,
    /// Operation not permitted [maps from -EPERM (-1) / -EACCES (-13)]
    TAF_PA_NOT_PERMITTED = -5,
    /// General fault [maps from -EFAULT (-14)]; also covers unhandled
    /// low-level errors (QMI, MTD, unmapped errno)
    TAF_PA_FAULT = -6,
    /// Communication error
    TAF_PA_COMM_ERROR = -7,
    /// Operation timed out [maps from -ETIMEDOUT (-110)]
    TAF_PA_TIMEOUT = -8,
    /// Buffer or value overflow [maps from -ENOSPC (-28) / -EOVERFLOW (-75)]
    TAF_PA_OVERFLOW = -9,
    /// Buffer or value underflow
    TAF_PA_UNDERFLOW = -10,
    /// Operation would block [maps from -EAGAIN (-11) / -EWOULDBLOCK (-11)]
    TAF_PA_WOULD_BLOCK = -11,
    /// Deadlock detected [maps from -EDEADLK (-35)]
    TAF_PA_DEADLOCK = -12,
    /// Data format error [maps from -EILSEQ (-84) / -EBADMSG (-74)]
    TAF_PA_FORMAT_ERROR = -13,
    /// Duplicate entry [maps from -EEXIST (-17)]
    TAF_PA_DUPLICATE = -14,
    /// Invalid parameter [maps from -EINVAL (-22)]
    TAF_PA_BAD_PARAMETER = -15,
    /// Resource is closed [maps from -EBADF (-9) / -EPIPE (-32)]
    TAF_PA_CLOSED = -16,
    /// Resource is busy [maps from -EBUSY (-16)]
    TAF_PA_BUSY = -17,
    /// Feature not supported [maps from -ENOTSUP (-95) / -EOPNOTSUPP (-95)]
    TAF_PA_UNSUPPORTED = -18,
    /// I/O error [maps from -EIO (-5)]
    TAF_PA_IO_ERROR = -19,
    /// Function not implemented [maps from -ENOSYS (-38)]
    /// Only PA default and stub functions will return this value.
    TAF_PA_NOT_IMPLEMENTED = -20,
    /// Resource unavailable [maps from -ENODEV (-19) / -ENXIO (-6)]
    TAF_PA_UNAVAILABLE = -21,
    /// Operation or session terminated [maps from -ECANCELED (-125)]
    TAF_PA_TERMINATED = -22,
    /// Operation already in progress
    /// [maps from -EINPROGRESS (-115) / -EALREADY (-114)]
    TAF_PA_IN_PROGRESS = -23,
    /// Operation suspended
    TAF_PA_SUSPENDED = -24,
} taf_pa_result_t;

#define TAF_PA_SHARED __attribute__((visibility("default")))

#ifdef TAF_PA_DEFAULT
#define TAF_PA_WEAK __attribute__((weak))
#else
#define TAF_PA_WEAK
#endif

typedef enum
{
    TAF_PA_COMMON_LOG_LEVEL_DEBUG = 0,
    TAF_PA_COMMON_LOG_LEVEL_INFO = 1,
    TAF_PA_COMMON_LOG_LEVEL_NOTICE = 2,
    TAF_PA_COMMON_LOG_LEVEL_WARN = 3,
    TAF_PA_COMMON_LOG_LEVEL_ERROR = 4,
    TAF_PA_COMMON_LOG_LEVEL_CRIT = 5,
    TAF_PA_COMMON_LOG_LEVEL_ALERT = 6,
    TAF_PA_COMMON_LOG_LEVEL_EMERG = 7
} taf_pa_common_LogLevel_t;

typedef enum
{
    TAF_PA_COMMON_LOG_BACKEND_SYSLOG = 0,
    TAF_PA_COMMON_LOG_BACKEND_DLT,
    TAF_PA_COMMON_LOG_BACKEND_AUTO
} taf_pa_common_LogBackend_t;

TAF_PA_SHARED taf_pa_result_t taf_pa_common_LogInit
(
    taf_pa_common_LogBackend_t backend,
    taf_pa_common_LogLevel_t initLogLevel,
    void* ctxPtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_common_LogSetBackend
(
    taf_pa_common_LogBackend_t backend
);

TAF_PA_SHARED taf_pa_result_t taf_pa_common_LogSetlevel
(
    taf_pa_common_LogLevel_t level
);

TAF_PA_SHARED taf_pa_result_t taf_pa_common_LogMessage
(
    taf_pa_common_LogLevel_t level,
    const char* file,
    const char* func,
    int line,
    const char* fmt,
    ...
);

TAF_PA_SHARED taf_pa_result_t taf_pa_common_LogDeinit
(
);

#define TAF_PA_DEBUG(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_INFO(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_INFO,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_NOTICE(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_NOTICE,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_WARN(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_ERROR(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_ERROR,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_CRIT(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_CRIT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_ALERT(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_ALERT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define TAF_PA_EMERG(fmt, ...) taf_pa_common_LogMessage( \
    TAF_PA_COMMON_LOG_LEVEL_EMERG,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define TAF_PA_FATAL(fmt, ...) { TAF_PA_EMERG(fmt, ##__VA_ARGS__); exit(EXIT_FAILURE); }
#define TAF_PA_FATAL_IF(condition, fmt, ...) if (condition) { TAF_PA_FATAL(fmt, ##__VA_ARGS__) }
#define TAF_PA_ASSERT(condition) TAF_PA_FATAL_IF(!(condition), "Assert Failed: '%s'", #condition)
#define TAF_PA_INFO_NOT_IMPLEMENTED() TAF_PA_INFO("%s: not implemented", __func__)

#ifdef __cplusplus
}
#endif

#endif // TAF_COMMON_PA_H
