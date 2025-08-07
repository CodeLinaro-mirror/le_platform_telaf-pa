/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_pa_common.h"

#if 0
static pthread_once_t log_once = PTHREAD_ONCE_INIT;

static void open_syslog(void) {
    openlog("TelAF_PA", LOG_PID | LOG_CONS, LOG_USER);
}

void tafpa_common_log_init(void) {
    pthread_once(&log_once, open_syslog);
}
#endif

void taf_pa_common_log_message(int level, const char *file,
                              const char *func, int line,
                              const char *fmt, ...) {
    //tafpa_common_log_init();

    const char *base = strrchr(file, '/');
    if (!base) base = strrchr(file, '\\');
    base = base ? base + 1 : file;

    char newFmt[1024];
    snprintf(newFmt, sizeof(newFmt), "%s %s() %d | %s", base, func, line, fmt);

    va_list args;
    va_start(args, fmt);
    vsyslog(level, newFmt, args);
    va_end(args);
}
