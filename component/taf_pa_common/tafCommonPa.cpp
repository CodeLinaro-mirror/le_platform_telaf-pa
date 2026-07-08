/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_common.h"

#include "tafCommonPa.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <atomic>
#include <dlfcn.h>
#include <dlt/dlt.h>

// Thread-safe initialization flag
static std::atomic<bool> gCommonPaInitialized(false);

/* Guard to prevent PA -> common -> PA recursive setlevel loop */
static std::atomic<bool> gSetLevelFanoutInProgress(false);

DltContext* DltCtxPtr = NULL;

#define MAX_MSG_SIZE 1024

static taf_pa_common_LogLevel_t   gLogLevel   = TAF_PA_COMMON_LOG_LEVEL_INFO;
static taf_pa_common_LogBackend_t gLogBackend = TAF_PA_COMMON_LOG_BACKEND_SYSLOG;

/* Function pointers resolved per-library so that both prop and noship
 * common libraries can be bound explicitly even though they export the
 * same symbol names.
 */
using PropCommonLogBindFn = void (*)(const taf_prop_common_LogVtable_t* vt);
using PropCommonLogSetLevelFn = void (*)(taf_prop_common_LogLevel_t level);

static void* gPropCommonHandle = NULL;
static void* gNsCommonHandle   = NULL;

static PropCommonLogBindFn     gPropCommonLogBindFn     = NULL;
static PropCommonLogBindFn     gNsCommonLogBindFn       = NULL;
static PropCommonLogSetLevelFn gPropCommonLogSetLevelFn = NULL;
static PropCommonLogSetLevelFn gNsCommonLogSetLevelFn   = NULL;

static const char* LogLevelToStr
(
    taf_pa_common_LogLevel_t level
)
{
    switch (level)
    {
        case TAF_PA_COMMON_LOG_LEVEL_DEBUG:
            return " DBUG";
        case TAF_PA_COMMON_LOG_LEVEL_INFO:
            return " INFO";
        case TAF_PA_COMMON_LOG_LEVEL_NOTICE:
            return "-NTC-";
        case TAF_PA_COMMON_LOG_LEVEL_WARN:
            return "-WRN-";
        case TAF_PA_COMMON_LOG_LEVEL_ERROR:
            return "=ERR=";
        case TAF_PA_COMMON_LOG_LEVEL_CRIT:
            return "*CRT*";
        case TAF_PA_COMMON_LOG_LEVEL_ALERT:
            return "*ALT*";
        case TAF_PA_COMMON_LOG_LEVEL_EMERG:
            return "*EMR*";
        default:
            break;
    }

    return " INFO";
}

static DltLogLevelType LogLevelToDlt(taf_pa_common_LogLevel_t level)
{
    switch (level)
    {
        case TAF_PA_COMMON_LOG_LEVEL_DEBUG:  return DLT_LOG_DEBUG;
        case TAF_PA_COMMON_LOG_LEVEL_INFO:   return DLT_LOG_INFO;
        case TAF_PA_COMMON_LOG_LEVEL_NOTICE: return DLT_LOG_WARN;
        case TAF_PA_COMMON_LOG_LEVEL_WARN:   return DLT_LOG_WARN;
        case TAF_PA_COMMON_LOG_LEVEL_ERROR:  return DLT_LOG_ERROR;
        case TAF_PA_COMMON_LOG_LEVEL_CRIT:
        case TAF_PA_COMMON_LOG_LEVEL_ALERT:
        case TAF_PA_COMMON_LOG_LEVEL_EMERG:  return DLT_LOG_FATAL;
        default:                             return DLT_LOG_INFO;
    }
}

static int LogLevelToSyslog
(
    taf_pa_common_LogLevel_t level
)
{
    switch (level)
    {
        case TAF_PA_COMMON_LOG_LEVEL_DEBUG:
            return LOG_DEBUG;
        case TAF_PA_COMMON_LOG_LEVEL_INFO:
            return LOG_INFO;
        case TAF_PA_COMMON_LOG_LEVEL_NOTICE:
            return LOG_NOTICE;
        case TAF_PA_COMMON_LOG_LEVEL_WARN:
            return LOG_WARNING;
        case TAF_PA_COMMON_LOG_LEVEL_ERROR:
            return LOG_ERR;
        case TAF_PA_COMMON_LOG_LEVEL_CRIT:
            return LOG_CRIT;
        case TAF_PA_COMMON_LOG_LEVEL_ALERT:
            return LOG_ALERT;
        case TAF_PA_COMMON_LOG_LEVEL_EMERG:
            return LOG_EMERG;
        default:
            break;
    }

    return LOG_INFO;
}

static taf_prop_common_LogLevel_t LogLevelToPropLogLevel
(
    taf_pa_common_LogLevel_t level
)
{
    switch (level)
    {
        case TAF_PA_COMMON_LOG_LEVEL_DEBUG:
            return TAF_PROP_COMMON_LOG_LEVEL_DEBUG;
        case TAF_PA_COMMON_LOG_LEVEL_INFO:
            return TAF_PROP_COMMON_LOG_LEVEL_INFO;
        case TAF_PA_COMMON_LOG_LEVEL_NOTICE:
            return TAF_PROP_COMMON_LOG_LEVEL_NOTICE;
        case TAF_PA_COMMON_LOG_LEVEL_WARN:
            return TAF_PROP_COMMON_LOG_LEVEL_WARN;
        case TAF_PA_COMMON_LOG_LEVEL_ERROR:
            return TAF_PROP_COMMON_LOG_LEVEL_ERROR;
        case TAF_PA_COMMON_LOG_LEVEL_CRIT:
            return TAF_PROP_COMMON_LOG_LEVEL_CRIT;
        case TAF_PA_COMMON_LOG_LEVEL_ALERT:
            return TAF_PROP_COMMON_LOG_LEVEL_ALERT;
        case TAF_PA_COMMON_LOG_LEVEL_EMERG:
            return TAF_PROP_COMMON_LOG_LEVEL_EMERG;
        default:
            break;
    }

    return TAF_PROP_COMMON_LOG_LEVEL_INFO;
}

static taf_pa_common_LogBackend_t DetectBackendFromEnv(void)
{
    const char *env = getenv("TAF_PA_LOG_BACKEND");
    if (env)
    {
        if (strcasecmp(env, "DLT") == 0)    return TAF_PA_COMMON_LOG_BACKEND_DLT;
        if (strcasecmp(env, "SYSLOG") == 0) return TAF_PA_COMMON_LOG_BACKEND_SYSLOG;
    }

    return TAF_PA_COMMON_LOG_BACKEND_SYSLOG;
}

static const char* Basename(const char* path)
{
    if (!path) return "?";
    const char* s1 = strrchr(path, '/');
    const char* s2 = strrchr(path, '\\');
    const char* base = (s1 && s2) ? (s1 > s2 ? s1 : s2) : (s1 ? s1 : s2);
    return base ? base + 1 : path;
}

static int FormatLog(char *out, size_t out_sz,
                     taf_pa_common_LogLevel_t level,
                     const char* file, const char* func, int line,
                     const char* fmt, va_list ap)
{
    const char* base = Basename(file);
    const char* fn = func ? func : "?";

    // Write the prefix into 'out' first
    int n = snprintf(out, out_sz, "%s | %s %s() %d | ",
                     LogLevelToStr(level), base, fn, line);
    if (n < 0) return -1;
    if ((size_t)n >= out_sz) return (int)(out_sz - 1); // truncated

    // Append the formatted message body
    int m = vsnprintf(out + n, out_sz - (size_t)n, fmt, ap);
    if (m < 0) return -1;

    // Return the total length that would have been written (may exceed out_sz)
    return n + m;
}

static void ResolveCommonLogApis(void)
{
    if (!gPropCommonHandle)
    {
        gPropCommonHandle = dlopen("libComponent_taf_prop_common.so", RTLD_NOW | RTLD_GLOBAL);
        if (gPropCommonHandle)
        {
            gPropCommonLogBindFn =
                reinterpret_cast<PropCommonLogBindFn>(
                    dlsym(gPropCommonHandle, "taf_prop_common_LogBind"));
            gPropCommonLogSetLevelFn =
                reinterpret_cast<PropCommonLogSetLevelFn>(
                    dlsym(gPropCommonHandle, "taf_prop_common_LogSetlevel"));
        }
    }

    if (!gNsCommonHandle)
    {
        gNsCommonHandle = dlopen("libComponent_taf_ns_common.so", RTLD_NOW | RTLD_GLOBAL);
        if (gNsCommonHandle)
        {
            gNsCommonLogBindFn =
                reinterpret_cast<PropCommonLogBindFn>(
                    dlsym(gNsCommonHandle, "taf_prop_common_LogBind"));
            gNsCommonLogSetLevelFn =
                reinterpret_cast<PropCommonLogSetLevelFn>(
                    dlsym(gNsCommonHandle, "taf_prop_common_LogSetlevel"));
        }
    }
}

/* ===== PA -> common injected vtable implementation ===== */

static taf_pa_common_LogLevel_t PropLevelToPaLevel(taf_prop_common_LogLevel_t level)
{
    switch (level)
    {
        case TAF_PROP_COMMON_LOG_LEVEL_DEBUG:  return TAF_PA_COMMON_LOG_LEVEL_DEBUG;
        case TAF_PROP_COMMON_LOG_LEVEL_INFO:   return TAF_PA_COMMON_LOG_LEVEL_INFO;
        case TAF_PROP_COMMON_LOG_LEVEL_NOTICE: return TAF_PA_COMMON_LOG_LEVEL_NOTICE;
        case TAF_PROP_COMMON_LOG_LEVEL_WARN:   return TAF_PA_COMMON_LOG_LEVEL_WARN;
        case TAF_PROP_COMMON_LOG_LEVEL_ERROR:  return TAF_PA_COMMON_LOG_LEVEL_ERROR;
        case TAF_PROP_COMMON_LOG_LEVEL_CRIT:   return TAF_PA_COMMON_LOG_LEVEL_CRIT;
        case TAF_PROP_COMMON_LOG_LEVEL_ALERT:  return TAF_PA_COMMON_LOG_LEVEL_ALERT;
        case TAF_PROP_COMMON_LOG_LEVEL_EMERG:  return TAF_PA_COMMON_LOG_LEVEL_EMERG;
        default:                               return TAF_PA_COMMON_LOG_LEVEL_INFO;
    }
}

pa_result_t taf_pa_common_LogSetlevel(taf_pa_common_LogLevel_t level)
{
    gLogLevel = level;

    ResolveCommonLogApis();

    bool expected = false;
    if (!gSetLevelFanoutInProgress.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        return PA_OK;
    }

    taf_prop_common_LogLevel_t commonLevel = LogLevelToPropLogLevel(level);

    if (gPropCommonLogSetLevelFn)
    {
        gPropCommonLogSetLevelFn(commonLevel);
    }

    if (gNsCommonLogSetLevelFn)
    {
        gNsCommonLogSetLevelFn(commonLevel);
    }

    gSetLevelFanoutInProgress.store(false, std::memory_order_release);
    return PA_OK;
}

pa_result_t taf_pa_common_LogSetBackend(taf_pa_common_LogBackend_t backend)
{
    if (backend == TAF_PA_COMMON_LOG_BACKEND_AUTO)
        backend = DetectBackendFromEnv();

    gLogBackend = backend;
    return PA_OK;
}

static inline taf_pa_common_LogBackend_t GetBackend(void)
{
    return gLogBackend;
}

static inline taf_pa_common_LogLevel_t GetLevel(void)
{
    return gLogLevel;
}

static void EmitLog(taf_pa_common_LogLevel_t level, const char* msg)
{
    switch (GetBackend())
    {
        case TAF_PA_COMMON_LOG_BACKEND_DLT:
            if (DltCtxPtr != NULL)
            {
                DLT_LOG(*DltCtxPtr, LogLevelToDlt(level), DLT_STRING(msg));
                break;
            }

        case TAF_PA_COMMON_LOG_BACKEND_SYSLOG:
        default:
            syslog(LogLevelToSyslog(level), "%s", msg);
            break;
    }
}

pa_result_t taf_pa_common_LogMessage(taf_pa_common_LogLevel_t level,
                              const char* file, const char* func, int line,
                              const char* fmt, ...)
{
    if (level < GetLevel())
        return PA_OK;

    char buf[MAX_MSG_SIZE];

    va_list ap;
    va_start(ap, fmt);

    int len = FormatLog(buf, sizeof(buf), level, file, func, line, fmt, ap);
    va_end(ap);

    if (len < 0)
        return PA_OK;

    // If truncated, optionally append "..." to make truncation visible.
    if ((size_t)len >= sizeof(buf))
    {
        const size_t sz = sizeof(buf);
        if (sz >= 4)
        {
            buf[sz - 4] = '.';
            buf[sz - 3] = '.';
            buf[sz - 2] = '.';
            buf[sz - 1] = '\0';
        }
    }

    EmitLog(level, buf);
    return PA_OK;
}

static void taf_pa_common_LogVMessage(taf_pa_common_LogLevel_t level,
                               const char* file,
                               const char* func,
                               int line,
                               const char* fmt,
                               va_list ap)
{
    if (level < GetLevel())
        return;

    char buf[MAX_MSG_SIZE];
    int len = FormatLog(buf, sizeof(buf), level, file, func, line, fmt, ap);
    if (len < 0) return;

    if ((size_t)len >= sizeof(buf) && sizeof(buf) >= 4)
    {
        buf[sizeof(buf)-4] = '.';
        buf[sizeof(buf)-3] = '.';
        buf[sizeof(buf)-2] = '.';
        buf[sizeof(buf)-1] = '\0';
    }

    EmitLog(level, buf);
}

static void PaShared_SetLevel(taf_prop_common_LogLevel_t level)
{
    /* Keep PA log level in sync, but DO NOT fan-out again from callback */
    gLogLevel = PropLevelToPaLevel(level);
}

static void PaShared_LogVprintf(taf_prop_common_LogLevel_t level,
                                const char* file,
                                const char* func,
                                int line,
                                const char* fmt,
                                va_list ap)
{
    taf_pa_common_LogVMessage(PropLevelToPaLevel(level), file, func, line, fmt, ap);
}

static const taf_prop_common_LogVtable_t SharedLogVt = {
    .abi_version = 1,
    .size = sizeof(taf_prop_common_LogVtable_t),
    .log_vprintf = PaShared_LogVprintf,
    .set_level   = PaShared_SetLevel,
};

pa_result_t taf_pa_common_LogInit(
    taf_pa_common_LogBackend_t backend,
    taf_pa_common_LogLevel_t initLogLevel,
    void* logCtxPtr
)
{
    // Atomically transition false->true; if already true, we are already initialized.
    bool expected = false;
    if (!gCommonPaInitialized.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        return PA_OK;
    }

    if (backend == TAF_PA_COMMON_LOG_BACKEND_AUTO)
        backend = DetectBackendFromEnv();

    gLogBackend = backend;
    gLogLevel = initLogLevel;

    // Initialize DLT context from the provided pointer
    if (logCtxPtr != NULL)
    {
        DltCtxPtr = (DltContext*)logCtxPtr;
    }

    /* Inject the SAME PA logging vtable into both common libraries explicitly. */
    ResolveCommonLogApis();

    if (gPropCommonLogBindFn)
    {
        gPropCommonLogBindFn(&SharedLogVt);
    }

    if (gNsCommonLogBindFn)
    {
        gNsCommonLogBindFn(&SharedLogVt);
    }

    PA_INFO("Common PA initialization flag set to true.");

    return PA_OK;
}

pa_result_t taf_pa_common_LogDeinit(void)
{
    // Check if Init() was called before Deinit()
    if (!gCommonPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return PA_FAULT;
    }

    // Clear injected vtables to avoid dangling pointers in PA-prop and PA-noship
    ResolveCommonLogApis();

    if (gPropCommonLogBindFn)
    {
        gPropCommonLogBindFn(NULL);
    }

    if (gNsCommonLogBindFn)
    {
        gNsCommonLogBindFn(NULL);
    }

    // Clear DLT context pointer so no further DLT log attempts are made
    DltCtxPtr = NULL;

    // Reset log backend and level to defaults
    gLogBackend = TAF_PA_COMMON_LOG_BACKEND_SYSLOG;
    gLogLevel   = TAF_PA_COMMON_LOG_LEVEL_INFO;

    // Log before resetting the flag
    PA_INFO("Common PA initialization flag reset to false.");

    // Reset the atomic flag last, after all cleanup is complete.
    gCommonPaInitialized.store(false, std::memory_order_release);

    return PA_OK;
}
