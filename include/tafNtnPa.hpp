/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NTN_PA_HPP
#define TAF_NTN_PA_HPP

#include "tafCommonPa.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    TAF_PA_NTN_SERVICE_STATUS_UNKNOWN = -1,
    TAF_PA_NTN_SERVICE_STATUS_AVAILABLE = 0,
    TAF_PA_NTN_SERVICE_STATUS_UNAVAILABLE = 1,
    TAF_PA_NTN_SERVICE_STATUS_FAILED = 2
} taf_pa_ntn_ServiceStatus_t;

typedef enum
{
    TAF_PA_NTN_STATE_UNKNOWN  = -1,
    TAF_PA_NTN_STATE_DISABLED = 0,
    TAF_PA_NTN_STATE_OUT_OF_SERVICE = 1,
    TAF_PA_NTN_STATE_IN_SERVICE = 2
} taf_pa_ntn_State_t;

typedef enum
{
    TAF_PA_NTN_SIGNAL_STRENGTH_UNKNOWN = 0,
    TAF_PA_NTN_SIGNAL_STRENGTH_NONE     = -1,
    TAF_PA_NTN_SIGNAL_STRENGTH_POOR     = 1,
    TAF_PA_NTN_SIGNAL_STRENGTH_MODERATE = 2,
    TAF_PA_NTN_SIGNAL_STRENGTH_GOOD     = 3,
    TAF_PA_NTN_SIGNAL_STRENGTH_GREAT    = 4
} taf_pa_ntn_SignalStrength_t;

typedef struct
{
    int64_t maxDataSize;
} taf_pa_ntn_NtnCapabilities_t;

typedef struct
{
    taf_pa_ntn_SignalStrength_t signalStrength;
} taf_pa_ntn_SignalStrengthChangeIndication_t;

typedef struct
{
    taf_pa_ntn_State_t ntnState;
} taf_pa_ntn_NtnStateChangeIndication_t;

typedef void (*taf_pa_ntn_SignalStrengthChangeHdlrFunc_t)
(
    taf_pa_ntn_SignalStrengthChangeIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_ntn_NtnStateChangeHdlrFunc_t)
(
    taf_pa_ntn_NtnStateChangeIndication_t indication,
    void* contextPtr
);

typedef struct
{
    pa_result_t errorCode;
    uint64_t transactionId;
} taf_pa_ntn_DataAckIndication_t;

typedef void (*taf_pa_ntn_DataAckHdlrFunc_t)
(
    taf_pa_ntn_DataAckIndication_t indication,
    void* contextPtr
);

PA_SHARED pa_result_t taf_pa_ntn_Init
(
    void
);

PA_SHARED pa_result_t taf_pa_ntn_Deinit
(
    void
);

PA_SHARED pa_result_t taf_pa_ntn_IsNtnSupported
(
    uint32_t instance,
    bool* isSupportedPtr
);

PA_SHARED pa_result_t taf_pa_ntn_EnableNtn
(
    uint32_t instance,
    bool enable,
    bool isEmergency
);

PA_SHARED pa_result_t taf_pa_ntn_GetNtnCapabilities
(
    uint32_t instance,
    taf_pa_ntn_NtnCapabilities_t* capabilitiesPtr
);

PA_SHARED pa_result_t taf_pa_ntn_GetSignalStrength
(
    uint32_t instance,
    taf_pa_ntn_SignalStrength_t* signalStrengthPtr
);

PA_SHARED pa_result_t taf_pa_ntn_GetNtnState
(
    uint32_t instance,
    taf_pa_ntn_State_t* statePtr
);

typedef void* taf_pa_ntn_SignalStrengthChangeHandlerRef_t;
typedef void* taf_pa_ntn_NtnStateChangeHandlerRef_t;
typedef void* taf_pa_ntn_DataAckHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddSignalStrengthChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_SignalStrengthChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_SignalStrengthChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveSignalStrengthChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_SignalStrengthChangeHandlerRef_t handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_AddNtnStateChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnStateChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_NtnStateChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveNtnStateChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnStateChangeHandlerRef_t handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_AddDataAckHandler
(
    uint32_t instance,
    taf_pa_ntn_DataAckHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_DataAckHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveDataAckHandler
(
    uint32_t instance,
    taf_pa_ntn_DataAckHandlerRef_t handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_SendData
(
    uint32_t instance,
    uint8_t* data,
    uint32_t size,
    bool isEmergency,
    uint64_t* transactionIdPtr
);

PA_SHARED pa_result_t taf_pa_ntn_EnableCellularScan
(
    uint32_t instance,
    bool enable
);

typedef struct
{
    char mcc[4];           // Mobile Country Code (null-terminated)
    char mnc[4];           // Mobile Network Code (null-terminated)
    const uint64_t* ntnBands;    // Array of NTN bands
    uint32_t ntnBandsLen;        // Number of bands
    const uint64_t* ntnEarfcns;  // Array of E-UTRAN absolute radio frequency channels
    uint32_t ntnEarfcnsLen; // Number of EARFCNs
} taf_pa_ntn_SystemSelectionSpecifier_t;

PA_SHARED pa_result_t taf_pa_ntn_UpdateSystemSelectionSpecifiers
(
    uint32_t instance,
    const taf_pa_ntn_SystemSelectionSpecifier_t* specifierPtr,
    uint32_t specifierCount
);

PA_SHARED pa_result_t taf_pa_ntn_AbortData
(
    uint32_t instance
);

typedef struct
{
    uint8_t* data;
    uint32_t size;
} taf_pa_ntn_IncomingDataIndication_t;

typedef void (*taf_pa_ntn_IncomingDataHdlrFunc_t)
(
    taf_pa_ntn_IncomingDataIndication_t indication,
    void* contextPtr
);

typedef void* taf_pa_ntn_IncomingDataHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddIncomingDataHandler
(
    uint32_t instance,
    taf_pa_ntn_IncomingDataHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_IncomingDataHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveIncomingDataHandler
(
    uint32_t instance,
    taf_pa_ntn_IncomingDataHandlerRef_t handlerRefPtr
);

typedef struct
{
    taf_pa_ntn_NtnCapabilities_t capabilities;
} taf_pa_ntn_CapabilitiesChangeIndication_t;

typedef void (*taf_pa_ntn_CapabilitiesChangeHdlrFunc_t)
(
    taf_pa_ntn_CapabilitiesChangeIndication_t indication,
    void* contextPtr
);

typedef void* taf_pa_ntn_CapabilitiesChangeHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddCapabilitiesChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_CapabilitiesChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_CapabilitiesChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveCapabilitiesChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_CapabilitiesChangeHandlerRef_t handlerRefPtr
);

typedef struct
{
    taf_pa_ntn_ServiceStatus_t status;
} taf_pa_ntn_ServiceStatusChangeIndication_t;

typedef void (*taf_pa_ntn_ServiceStatusChangeHdlrFunc_t)
(
    taf_pa_ntn_ServiceStatusChangeIndication_t indication,
    void* contextPtr
);

typedef void* taf_pa_ntn_ServiceStatusChangeHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddServiceStatusChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_ServiceStatusChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_ServiceStatusChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveServiceStatusChangeHandler
(
    uint32_t instance,
    taf_pa_ntn_ServiceStatusChangeHandlerRef_t handlerRefPtr
);

typedef struct
{
    bool isCellularCoverageAvailable;
} taf_pa_ntn_CellularCoverageAvailableIndication_t;

typedef void (*taf_pa_ntn_CellularCoverageAvailableHdlrFunc_t)
(
    taf_pa_ntn_CellularCoverageAvailableIndication_t indication,
    void* contextPtr
);

typedef void* taf_pa_ntn_CellularCoverageAvailableHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddCellularCoverageAvailableHandler
(
    uint32_t instance,
    taf_pa_ntn_CellularCoverageAvailableHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_CellularCoverageAvailableHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveCellularCoverageAvailableHandler
(
    uint32_t instance,
    taf_pa_ntn_CellularCoverageAvailableHandlerRef_t handlerRefPtr
);

#define TAF_PA_MAX_DIMENSIONS 3

typedef struct
{
    bool isEnuValueValid;
    float enuVel[TAF_PA_MAX_DIMENSIONS];
    bool isEnuUncerValid;
    float enuUncer[TAF_PA_MAX_DIMENSIONS];
} taf_pa_ntn_VelocityInfo_t;

typedef struct
{
    float lat;
    float lon;
    float alt;
    uint32_t uncerCircular;
    taf_pa_ntn_VelocityInfo_t velInfo;
    bool isHeadingValid;
    uint32_t heading;
    bool isHeadingUncerValid;
    uint32_t headingUncer;
    bool isConfidenceValid;
    uint32_t confidence;
} taf_pa_ntn_LocationFix_t;

PA_SHARED pa_result_t taf_pa_ntn_SetLocationFix
(
    uint32_t instance,
    const taf_pa_ntn_LocationFix_t* locationFixPtr
);

typedef enum
{
    TAF_PA_NTN_LOCATION_STATUS_INVALID = -1,
    TAF_PA_NTN_LOCATION_STATUS_SUCCESS = 0,
    TAF_PA_NTN_LOCATION_STATUS_INVALID_ARG = 1,
    TAF_PA_NTN_LOCATION_STATUS_INTERNAL_ERR = 2,
    TAF_PA_NTN_LOCATION_STATUS_NOT_SUPPORTED = 3,
    TAF_PA_NTN_LOCATION_STATUS_RETRY = 4,
    TAF_PA_NTN_LOCATION_STATUS_FAILED = 5
} taf_pa_ntn_LocationStatus_t;

PA_SHARED pa_result_t taf_pa_ntn_LocationFixResponse
(
    uint32_t instance,
    taf_pa_ntn_LocationStatus_t status,
    uint64_t waitTime
);

typedef enum
{
    TAF_PA_NTN_LOCATION_FIX_REQUEST_REASON_UNKNOWN = 0,
    TAF_PA_NTN_LOCATION_FIX_REQUEST_REASON_NORMAL = 1,
    TAF_PA_NTN_LOCATION_FIX_REQUEST_REASON_VALIDITY_TIMER_EXPIRED = 2
} taf_pa_ntn_LocationFixRequestReason_t;

typedef struct
{
    taf_pa_ntn_LocationFixRequestReason_t reason;
} taf_pa_ntn_LocationFixRequestIndication_t;

typedef void (*taf_pa_ntn_LocationFixRequestHdlrFunc_t)
(
    taf_pa_ntn_LocationFixRequestIndication_t indication,
    void* contextPtr
);

typedef void* taf_pa_ntn_LocationFixRequestHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddLocationFixRequestHandler
(
    uint32_t instance,
    taf_pa_ntn_LocationFixRequestHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_LocationFixRequestHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveLocationFixRequestHandler
(
    uint32_t instance,
    taf_pa_ntn_LocationFixRequestHandlerRef_t handlerRefPtr
);

typedef struct
{
    uint32_t bandValue;
} taf_pa_ntn_NtnBandUpdateIndication_t;

typedef void (*taf_pa_ntn_NtnBandUpdateHdlrFunc_t)
(
    taf_pa_ntn_NtnBandUpdateIndication_t indication,
    void* contextPtr
);

typedef void* taf_pa_ntn_NtnBandUpdateHandlerRef_t;

PA_SHARED pa_result_t taf_pa_ntn_AddNtnBandUpdateHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnBandUpdateHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_ntn_NtnBandUpdateHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_ntn_RemoveNtnBandUpdateHandler
(
    uint32_t instance,
    taf_pa_ntn_NtnBandUpdateHandlerRef_t handlerRefPtr
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_NTN_PA_HPP */
