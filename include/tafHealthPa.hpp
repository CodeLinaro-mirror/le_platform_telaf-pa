/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafCommonPa.h"
#include <thread>
#include <chrono>

#ifndef TAF_HEALTH_PA_HPP
#define TAF_HEALTH_PA_HPP

#define TAF_HMS_PHONE_MANAGER_TIMEOUT     30        //Phone manager wait timeout
#define TAF_HMS_SUBSYSTEM_MANAGER_TIMEOUT 30        // Subsytem manager wait timeout
#define TAF_HMS_MODEM_RESET_TIMER         120000    // Modem reset timer in ms

enum class ModemOperationalStatusPA { UNKNOWN, UNAVAILABLE, OPERATIONAL };

enum class ProcTypePA {LOCAL_PROC = 0, REMOTE_PROC};

typedef struct
{
    ProcTypePA procType;
    uint32_t subsystemBitmask;
}SubsytemInfoPA;

typedef struct {
    SubsytemInfoPA subSystemInfo;
    ModemOperationalStatusPA newOperationalStatus;
} taf_health_ModemStatusInfo_t;

//--------------------------------------------------------------------------------------------------
/**
 * Type of modem event maintained by health monitor service.
 *
 */
//--------------------------------------------------------------------------------------------------
enum ModemEvtTypePA
{
    MODEM_EVENT_TYPE_CONTINUE_REBOOT_PA,
    MODEM_EVENT_TYPE_CONNECTION_LOST_PA,
    MODEM_EVENT_TYPE_CONNECTION_AVAIL_PA
};
//--------------------------------------------------------------------------------------------------
/**
 * Type of modem event severity level maintained by health monitor service.
 *
 */
//--------------------------------------------------------------------------------------------------
enum ModemEvtSeverityPA
{
    MODEM_EVENT_SEVERITY_LOW_PA,
    MODEM_EVENT_SEVERITY_MEDIUM_PA,
    MODEM_EVENT_SEVERITY_HIGH_PA
};

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for modem status change update notification handler.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_health_ModemStatusUpdateHandler_t)
(
    taf_health_ModemStatusInfo_t modemStatusInfo
);

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for modem operation mode status update notification handler.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_health_ModemOperatingModeUpdateHandler_t)
(
    taf_pa_result_t result
);

TAF_PA_SHARED  taf_pa_result_t taf_pa_health_ModemNotificationInit(void);
TAF_PA_SHARED  taf_pa_result_t taf_pa_health_RegModemListener(void);
TAF_PA_SHARED  taf_pa_result_t taf_pa_health_DeregModemListener(void);
TAF_PA_SHARED  taf_pa_result_t taf_pa_health_PhoneInit(void);

TAF_PA_SHARED taf_pa_result_t taf_pa_health_Deinit(void);
TAF_PA_SHARED  taf_pa_result_t taf_pa_health_ReqPhoneOperatingMode(void);

//--------------------------------------------------------------------------------------------------
/**
 * Register modem status change update handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_health_RegModemStatusUpdateHandler
(
    taf_pa_health_ModemStatusUpdateHandler_t handlerFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * Register modem oprating status change update handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_health_RegModemOperationModeUpdateHandler
(
    taf_pa_health_ModemOperatingModeUpdateHandler_t handlerFunc
);

#endif // TAF_HEALTH_PA_HPP
