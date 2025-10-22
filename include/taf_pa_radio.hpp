/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_RADIO_HPP
#define TAF_PA_RADIO_HPP

#include "taf_pa_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PA_RADIO_PCI_SCAN_CELL_MAX_COUNT 5
#define TAF_PA_RADIO_PCI_SCAN_PLMN_ID_MAX_COUNT 6
#define TAF_PA_RADIO_DATA_AVAIL_SYS_MAX_COUNT 15
#define TAF_PA_RADIO_LTE_CPHY_SCELL_INFO_MAX_COUNT 10

#define TAF_PA_RADIO_BITMASK_RAT_GSM 0x1
#define TAF_PA_RADIO_BITMASK_RAT_CDMA 0x2
#define TAF_PA_RADIO_BITMASK_RAT_UMTS 0x4
#define TAF_PA_RADIO_BITMASK_RAT_TDSCDMA 0x8
#define TAF_PA_RADIO_BITMASK_RAT_LTE 0x10
#define TAF_PA_RADIO_BITMASK_RAT_NR5G 0x20
typedef uint64_t taf_pa_radio_RatBitMask_t;

#define TAF_PA_RADIO_BITMASK_SO_5G_NSA 0x80000000000
typedef uint64_t taf_pa_radio_SoBitMask_t;

typedef enum
{
    TAF_PA_RADIO_OPERATING_MODE_UNKNOWN = 0,
    TAF_PA_RADIO_OPERATING_MODE_ONLINE = 1,
    TAF_PA_RADIO_OPERATING_MODE_LOW_POWER = 2,
    TAF_PA_RADIO_OPERATING_MODE_FACTORY_TEST_MODE = 3,
    TAF_PA_RADIO_OPERATING_MODE_OFFLINE = 4,
    TAF_PA_RADIO_OPERATING_MODE_RESETTING = 5,
    TAF_PA_RADIO_OPERATING_MODE_SHUTTING_DOWN = 6,
    TAF_PA_RADIO_OPERATING_MODE_PERSISTENT_LOW_POWER = 7
} taf_pa_radio_OperatingMode_t;

typedef enum
{
    TAF_PA_RADIO_RAT_UNKNOWN = 0,
    TAF_PA_RADIO_RAT_GSM = 1,
    TAF_PA_RADIO_RAT_CDMA = 2,
    TAF_PA_RADIO_RAT_UMTS = 3,
    TAF_PA_RADIO_RAT_TDSCDMA = 4,
    TAF_PA_RADIO_RAT_LTE = 5,
    TAF_PA_RADIO_RAT_NR5G = 6
} taf_pa_radio_Rat_t;

typedef enum
{
    TAF_PA_RADIO_RAT_SERVICE_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_RAT_SERVICE_STATUS_NO_SERVICE = 1,
    TAF_PA_RADIO_RAT_SERVICE_STATUS_LIMITED = 2,
    TAF_PA_RADIO_RAT_SERVICE_STATUS_SERVICE = 3,
    TAF_PA_RADIO_RAT_SERVICE_STATUS_LIMITED_REGIONAL = 4,
    TAF_PA_RADIO_RAT_SERVICE_STATUS_POWER_SAVE = 5
} taf_pa_radio_RatServiceStatus_t;

typedef enum
{
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_UNKNOWN = 0,
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_6 = 1,
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_15 = 2,
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_25 = 3,
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_50 = 4,
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_75 = 5,
    TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_100 = 6
} taf_pa_radio_LteCphyCaBandwidth_t;

typedef enum
{
    TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_UNKNOWN = 0,
    TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_DECONFIGURED = 1,
    TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_CONFIGURED_DEACTIVATED = 2,
    TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_CONFIGURED_ACTIVATED = 3
} taf_pa_radio_LteCphyScellState_t;

typedef enum
{
    TAF_PA_RADIO_DATA_ROAMING_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_DATA_ROAMING_STATUS_ON = 1,
    TAF_PA_RADIO_DATA_ROAMING_STATUS_OFF = 2
} taf_pa_radio_DataRoamingStatus_t;

typedef struct
{
    uint16_t mcc;
    uint16_t mnc;
    uint8_t mncIncludesPcsDigit;
} taf_pa_radio_PlmnId_t;

typedef struct
{
    uint16_t cellId;
    uint32_t globalCellId;
    uint32_t plmnCount;
    taf_pa_radio_PlmnId_t plmnId[TAF_PA_RADIO_PCI_SCAN_PLMN_ID_MAX_COUNT];
} taf_pa_radio_PciCellInformation_t;

typedef struct
{
    uint32_t pciCellCount;
    taf_pa_radio_PciCellInformation_t pciCellInfo[TAF_PA_RADIO_PCI_SCAN_CELL_MAX_COUNT];
} taf_pa_radio_PciScanInformation_t;

typedef struct
{
    taf_pa_radio_Rat_t rat;
    taf_pa_radio_SoBitMask_t soMask;
} taf_pa_radio_DataAvailSysStatusInfo_t;

typedef struct
{
    uint32_t availSysCount;
    taf_pa_radio_DataAvailSysStatusInfo_t availSysStatusInfo[TAF_PA_RADIO_DATA_AVAIL_SYS_MAX_COUNT];
} taf_pa_radio_DataAvailSysStatus_t;

typedef struct
{
    uint16_t pci;
    uint32_t freq;
    taf_pa_radio_LteCphyCaBandwidth_t cphyCaDlBandwidth;
    uint32_t band;
} taf_pa_radio_LteCphyPcellInfo_t;

typedef struct
{
    uint16_t pci;
    uint32_t freq;
    taf_pa_radio_LteCphyCaBandwidth_t cphyCaDlBandwidth;
    uint32_t band;
    taf_pa_radio_LteCphyScellState_t scellState;
    uint8_t scellIndex;
    uint8_t ulConfigured;
} taf_pa_radio_LteCphyScellInfo_t;

typedef struct
{
    taf_pa_radio_LteCphyPcellInfo_t pcellInfo;
    uint32_t scellInfoCount;
    taf_pa_radio_LteCphyScellInfo_t scellInfo[TAF_PA_RADIO_LTE_CPHY_SCELL_INFO_MAX_COUNT];
} taf_pa_radio_LteCphyCaInfo_t;

typedef struct
{
    uint8_t gsmSvcStatusValid;
    taf_pa_radio_RatServiceStatus_t gsmSvcStatus;
    uint8_t cdmaSvcStatusValid;
    taf_pa_radio_RatServiceStatus_t cdmaSvcStatus;
    uint8_t umtsSvcStatusValid;
    taf_pa_radio_RatServiceStatus_t umtsSvcStatus;
    uint8_t tdscdmaSvcStatusValid;
    taf_pa_radio_RatServiceStatus_t tdscdmaSvcStatus;
    uint8_t lteSvcStatusValid;
    taf_pa_radio_RatServiceStatus_t lteSvcStatus;
    uint8_t nr5gSvcStatusValid;
    taf_pa_radio_RatServiceStatus_t nr5gSvcStatus;
} taf_pa_radio_RatSvcStatusIndication_t;

typedef struct
{
    uint8_t pcellInfoValid;
    taf_pa_radio_LteCphyPcellInfo_t pcellInfo;
    uint8_t scellInfoValid;
    uint32_t scellInfoCount;
    taf_pa_radio_LteCphyScellInfo_t scellInfo[TAF_PA_RADIO_LTE_CPHY_SCELL_INFO_MAX_COUNT];
} taf_pa_radio_LteCphyCaIndication_t;

typedef struct
{
    uint8_t availSysValid;
    taf_pa_radio_DataAvailSysStatus_t availSys;
} taf_pa_radio_DataAvailSysStatusIndication_t;

typedef struct taf_pa_radio_RatSvcStatusHandler* taf_pa_radio_RatSvcStatusHandlerRef_t;

typedef struct taf_pa_radio_LteCphyCaHandler* taf_pa_radio_LteCphyCaHandlerRef_t;

typedef struct taf_pa_radio_DataAvailSysStatusHandler* taf_pa_radio_DataAvailSysStatusHandlerRef_t;

typedef void (*taf_pa_radio_RatSvcStatusHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_RatSvcStatusIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_LteCphyCaHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_DataAvailSysStatusHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatusIndication_t indication,
    void* contextPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_Init
(
    void
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetOperatingMode
(
    uint32_t instance,
    taf_pa_radio_OperatingMode_t* modePtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_RegisterIndication
(
    uint32_t instance,
    uint8_t registration
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_PerformPciNetworkScan
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t bitmask,
    taf_pa_radio_PciScanInformation_t* informationPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetServingRat
(
    uint32_t instance,
    taf_pa_radio_Rat_t* ratPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetRatSvcStatus
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_RatServiceStatus_t* statusPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetServingCellRac
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    uint8_t* racPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetDataAvailSysStatus
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatus_t* statusPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetLteCphyCaInfo
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaInfo_t* infoPtr
);

PA_SHARED PA_WEAK taf_pa_radio_RatSvcStatusHandlerRef_t taf_pa_radio_AddRatSvcStatusHandler
(
    uint32_t instance,
    taf_pa_radio_RatSvcStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PA_SHARED PA_WEAK taf_pa_radio_LteCphyCaHandlerRef_t taf_pa_radio_AddLteCphyCaHandler
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PA_SHARED PA_WEAK taf_pa_radio_DataAvailSysStatusHandlerRef_t taf_pa_radio_AddDataAvailSysStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
);

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_GetDataCurrRoamingStatus
(
    uint32_t instance,
    taf_pa_radio_DataRoamingStatus_t* statusPtr
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PA_RADIO_HPP */

