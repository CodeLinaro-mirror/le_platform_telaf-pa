/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_COMMON_H
#define TAF_PROP_COMMON_H

#include <stdint.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

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

PROP_SHARED void taf_prop_common_LogSetlevel
(
    taf_prop_common_LogLevel_t level
);

PROP_SHARED void taf_prop_common_LogMessage
(
    taf_prop_common_LogLevel_t level,
    const char* file,
    const char* func,
    int line,
    const char* fmt,
    ...
);

#define PROP_DEBUG(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_INFO(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_INFO,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_NOTICE(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_NOTICE,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_WARN(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_WARN,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_ERROR(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_ERROR,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_CRIT(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_CRIT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_ALERT(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_ALERT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PROP_EMERG(fmt, ...) taf_prop_common_LogMessage(TAF_PROP_COMMON_LOG_LEVEL_EMERG,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // TAF_PROP_COMMON_H

