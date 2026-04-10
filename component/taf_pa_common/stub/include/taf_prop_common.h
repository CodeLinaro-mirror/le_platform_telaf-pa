/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_COMMON_H
#define TAF_PROP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>   // va_list

#define PROP_SHARED __attribute__((visibility("default")))

typedef enum
{
    TAF_PROP_COMMON_LOG_LEVEL_DEBUG = 0,
    TAF_PROP_COMMON_LOG_LEVEL_INFO = 1,
    TAF_PROP_COMMON_LOG_LEVEL_NOTICE = 2,
    TAF_PROP_COMMON_LOG_LEVEL_WARN = 3,
    TAF_PROP_COMMON_LOG_LEVEL_ERROR = 4,
    TAF_PROP_COMMON_LOG_LEVEL_CRIT = 5,
    TAF_PROP_COMMON_LOG_LEVEL_ALERT = 6,
    TAF_PROP_COMMON_LOG_LEVEL_EMERG = 7
} taf_prop_common_LogLevel_t;

/* ===== Injected logging interface (vtable) =====
 * Provided by PA at runtime. PA-prop / PA-noship must not call syslog/DLT directly.
 */
typedef struct
{
    /* ABI guard for compatibility */
    unsigned int abi_version;
    unsigned int size;

    /* printf-style logging using va_list */
    void (*log_vprintf)(taf_prop_common_LogLevel_t level,
                        const char* file,
                        const char* func,
                        int line,
                        const char* fmt,
                        va_list ap);

    /* optional: allow PA to push policy (can be NULL) */
    void (*set_level)(taf_prop_common_LogLevel_t level);

} taf_prop_common_LogVtable_t;

/* Called by PA to inject/clear the vtable. */
PROP_SHARED void taf_prop_common_LogBind(const taf_prop_common_LogVtable_t* vt);

PROP_SHARED void taf_prop_common_LogSetlevel(taf_prop_common_LogLevel_t level);

PROP_SHARED void taf_prop_common_LogMessage
(
    taf_prop_common_LogLevel_t level,
    const char* file,
    const char* func,
    int line,
    const char* fmt,
    ...
);

#define PROP_DEBUG(fmt, ...)  taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_DEBUG,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_INFO(fmt, ...)   taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_INFO,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_NOTICE(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_NOTICE, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_WARN(fmt, ...)   taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_WARN,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_ERROR(fmt, ...)  taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_ERROR,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_CRIT(fmt, ...)   taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_CRIT,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_ALERT(fmt, ...)  taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_ALERT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_EMERG(fmt, ...)  taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_EMERG,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // TAF_PROP_COMMON_H
