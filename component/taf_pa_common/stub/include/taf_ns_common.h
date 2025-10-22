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

#ifdef __cplusplus
extern "C" {
#endif

#define NS_SHARED __attribute__((visibility("default")))

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

#ifdef __cplusplus
}
#endif

#endif // TAF_NS_COMMON_H

