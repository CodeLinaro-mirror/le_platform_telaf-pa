/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef PA_COMMON_H
#define PA_COMMON_H

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

typedef int32_t pa_result_t;

#define PA_SHARED __attribute__((visibility("default")))

#ifdef TAF_PA_DEFAULT
#define PA_WEAK __attribute__((weak))
#else
#define PA_WEAK
#endif

typedef enum {
    PA_OK = 0,
    PA_NOT_FOUND = -1,
    PA_NOT_POSSIBLE = -2,
    PA_OUT_OF_RANGE = -3,
    PA_NO_MEMORY = -4,
    PA_NOT_PERMITTED = -5,
    PA_FAULT = -6,
    PA_COMM_ERROR = -7,
    PA_TIMEOUT = -8,
    PA_OVERFLOW = -9,
    PA_UNDERFLOW = -10,
    PA_WOULD_BLOCK = -11,
    PA_DEADLOCK = -12,
    PA_FORMAT_ERROR = -13,
    PA_DUPLICATE = -14,
    PA_BAD_PARAMETER = -15,
    PA_CLOSED = -16,
    PA_BUSY = -17,
    PA_UNSUPPORTED = -18,
    PA_IO_ERROR = -19,
    PA_NOT_IMPLEMENTED = -20,
    PA_UNAVAILABLE = -21,
    PA_TERMINATED = -22,
    PA_IN_PROGRESS = -23,
    PA_SUSPENDED = -24
} pa_result_enum_t;

//PA_SHARED void tafpa_common_log_init(void);

PA_SHARED void taf_pa_common_log_message(int level, const char *file,
                                        const char *func, int line,
                                        const char *fmt, ...);

#define PA_INFO(fmt, ...)  taf_pa_common_log_message(LOG_INFO,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PA_DEBUG(fmt, ...) taf_pa_common_log_message(LOG_DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PA_ERROR(fmt, ...) taf_pa_common_log_message(LOG_ERR,   __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define PA_CRIT(fmt, ...)  taf_pa_common_log_message(LOG_CRIT,  __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // PA_COMMON_H
