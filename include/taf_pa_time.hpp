/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_pa_common.h"


#ifndef TAF_PA_TIME_H
#define TAF_PA_TIME_H

#ifdef TAF_PA_DEFAULT
#define TAF_PA_WEAK __attribute__((weak))
#else
#define TAF_PA_WEAK
#endif

#define NITZ_STR_BUF_MAX       60
#define NETWORK_SLOT_1         1
#define NETWORK_SLOT_2         2
#define NETWORK_SLOT_NUM_MAX   2

#define NETWORK_PHONE_1        1
#define NETWORK_PHONE_2        2
#define NETWORK_PHONE_NUM_MAX  2

//--------------------------------------------------------------------------------------------------
/**
 * Network time information structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint16_t year;                   ///< Year.
    uint8_t month;                   ///< Month. 1 is January and 12 is December.
    uint8_t day;                     ///< Day. Range: 1 to 31.
    uint8_t hour;                    ///< Hour. Range: 0 to 23.
    uint8_t minute;                  ///< Minute. Range: 0 to 59.
    uint8_t second;                  ///< Second. Range: 0 to 59.
    uint8_t dayOfWeek;               ///< Day of the week. 0 is Monday and 6 is Sunday.
    int8_t timeZone;                 ///< Offset between UTC and local time in units of 15 minutes.
                                     ///  Actual value = field value * 15 minutes.
    uint8_t dstAdj;                  ///< Daylight saving adjustment in hours to obtain local time.
                                     ///  Possible values: 0, 1, and 2.
    char nitzTime[NITZ_STR_BUF_MAX]; ///< Network Identity and Time Zone(NITZ) information in
                                     ///  form "yyyy/mm/dd,hh:mm:ss(+/-)tzh:tzm,dt"
}taf_time_NetTimeInfo_t;

//--------------------------------------------------------------------------------------------------
/**
 * Prototype for GNSS UTC time update notification handler.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_time_GnssUtcTimeUpdateHandler_t)
(
    const uint64_t utc  ///< utc time
);


//--------------------------------------------------------------------------------------------------
/**
 * Prototype for network time update notification handler.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_time_NetworkChangeHandler_t)
(
    taf_time_NetTimeInfo_t info, ///< Network time information.
    int slotId                   ///< Slot ID
);


//--------------------------------------------------------------------------------------------------
/**
 * Prototype for network time update notification handler.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_time_NetworkInfoHandler_t)
(
    taf_time_NetTimeInfo_t info, ///< [IN] Network time information.
    int slotId,                  ///< Slot ID
    pa_result_t result           ///< Status
);

//--------------------------------------------------------------------------------------------------
/**
 * Register GNSS UTC time update handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_time_RegGnssUtcTimeUpdateHandler
(
    taf_pa_time_GnssUtcTimeUpdateHandler_t handlerFunc
);


PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_gnss_Init(void);
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_RegGnssTimeListener(void);
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_DeregGnssTimeListener(void);


//--------------------------------------------------------------------------------------------------
/**
 * Register network time change notification handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_time_RegNetworkTimeChangeHandler
(
    taf_pa_time_NetworkChangeHandler_t handlerFunc
);

PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_network_Init(int slotId);
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_time_RegNetworkTimeListener(int slotId);
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_time_DeregNetworkTimeListener(int slotId);
PA_SHARED TAF_PA_WEAK pa_result_t taf_pa_time_RequestNetworkTime
(
    int slotId,
    taf_pa_time_NetworkInfoHandler_t handlerFunc
);


#endif /* TAF_PA_TIME_H */

