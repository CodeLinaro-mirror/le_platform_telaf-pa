/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NS_COMMON_H
#define TAF_NS_COMMON_H

#include <stdint.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t ns_result_t;

#define NS_SHARED __attribute__((visibility("default")))

typedef enum {
    NS_OK = 0,
    NS_NOT_FOUND = -1,
    NS_NOT_POSSIBLE = -2,
    NS_OUT_OF_RANGE = -3,
    NS_NO_MEMORY = -4,
    NS_NOT_PERMITTED = -5,
    NS_FAULT = -6,
    NS_COMM_ERROR = -7,
    NS_TIMEOUT = -8,
    NS_OVERFLOW = -9,
    NS_UNDERFLOW = -10,
    NS_WOULD_BLOCK = -11,
    NS_DEADLOCK = -12,
    NS_FORMAT_ERROR = -13,
    NS_DUPLICATE = -14,
    NS_BAD_PARAMETER = -15,
    NS_CLOSED = -16,
    NS_BUSY = -17,
    NS_UNSUPPORTED = -18,
    NS_IO_ERROR = -19,
    NS_NOT_IMPLEMENTED = -20,
    NS_UNAVAILABLE = -21,
    NS_TERMINATED = -22,
    NS_IN_PROGRESS = -23,
    NS_SUSPENDED = -24
} ns_result_enum_t;

typedef enum
{
    TAF_NS_COMMON_LOG_LEVEL_DEBUG = 0,
    TAF_NS_COMMON_LOG_LEVEL_INFO = 1,
    TAF_NS_COMMON_LOG_LEVEL_NOTICE = 2,
    TAF_NS_COMMON_LOG_LEVEL_WARN = 3,
    TAF_NS_COMMON_LOG_LEVEL_ERROR = 4,
    TAF_NS_COMMON_LOG_LEVEL_CRIT = 5,
    TAF_NS_COMMON_LOG_LEVEL_ALERT = 6,
    TAF_NS_COMMON_LOG_LEVEL_EMERG = 7
} taf_ns_common_LogLevel_t;

/* ===== Injected logging interface (vtable) =====
 * Provided by PA at runtime. PA-noship must not call syslog/DLT directly.
 */
typedef struct
{
    /* ABI guard for compatibility */
    unsigned int abi_version;
    unsigned int size;

    /* printf-style logging using va_list */
    void (*log_vprintf)(taf_ns_common_LogLevel_t level,
                        const char* file,
                        const char* func,
                        int line,
                        const char* fmt,
                        va_list ap);

    /* optional: allow PA to push policy (can be NULL) */
    void (*set_level)(taf_ns_common_LogLevel_t level);

} taf_ns_common_LogVtable_t;

/* Called by PA to inject/clear the vtable. */
NS_SHARED void taf_ns_common_LogBind(const taf_ns_common_LogVtable_t* vt);

NS_SHARED void taf_ns_common_LogSetlevel
(
    taf_ns_common_LogLevel_t level
);

NS_SHARED void taf_ns_common_LogMessage
(
    taf_ns_common_LogLevel_t level,
    const char* file,
    const char* func,
    int line,
    const char* fmt,
    ...
);

#define NS_DEBUG(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_INFO(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_INFO,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_NOTICE(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_NOTICE,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_WARN(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_ERROR(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_ERROR,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_CRIT(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_CRIT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_ALERT(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_ALERT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define NS_EMERG(fmt, ...) taf_ns_common_LogMessage(TAF_NS_COMMON_LOG_LEVEL_EMERG,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define NS_FATAL(fmt, ...) { NS_EMERG(fmt, ##__VA_ARGS__); exit(EXIT_FAILURE); }
#define NS_FATAL_IF(condition, fmt, ...) if (condition) { NS_FATAL(fmt, ##__VA_ARGS__) }
#define NS_ASSERT(condition) NS_FATAL_IF(!(condition), "Assert Failed: '%s'", #condition)

#ifdef __cplusplus
}
#endif

#endif // TAF_NS_COMMON_H
