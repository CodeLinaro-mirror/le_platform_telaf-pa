/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_RADIO_PA_HPP
#define TAF_RADIO_PA_HPP

#include "tafCommonPa.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PA_RADIO_PCI_SCAN_CELL_MAX_COUNT 5
#define TAF_PA_RADIO_PCI_SCAN_PLMN_ID_MAX_COUNT 6
#define TAF_PA_RADIO_DATA_AVAIL_SYS_MAX_COUNT 15
#define TAF_PA_RADIO_LTE_CPHY_SCELL_INFO_MAX_COUNT 10
#define TAF_PA_RADIO_PREFERRED_NETWORK_MAX_COUNT 200
#define TAF_PA_RADIO_STATIC_PREFERRED_NETWORK_MAX_COUNT 40
#define TAF_PA_RADIO_SIGNAL_STRENGTH_THRESHOLD_MAX_COUNT 32
#define TAF_PA_RADIO_CELL_LOCATION_MAX_COUNT 65
#define TAF_PA_RADIO_PLMN_SCAN_NETWORK_MAX_COUNT 40
#define TAF_PA_RADIO_PLMN_NETWORK_DESCRIPTION_MAX_BYTES 256
#define TAF_PA_RADIO_LTE_BAND_GROUP_COUNT 4

#define TAF_PA_RADIO_BITMASK_RAT_GSM 0x1
#define TAF_PA_RADIO_BITMASK_RAT_CDMA 0x2
#define TAF_PA_RADIO_BITMASK_RAT_UMTS 0x4
#define TAF_PA_RADIO_BITMASK_RAT_TDSCDMA 0x8
#define TAF_PA_RADIO_BITMASK_RAT_LTE 0x10
#define TAF_PA_RADIO_BITMASK_RAT_NR5G 0x20
typedef uint64_t taf_pa_radio_RatBitMask_t;

#define TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_CS_ONLY 0x1
#define TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_PS_ONLY 0x2
#define TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_CS_AND_PS 0x4
typedef uint64_t taf_pa_radio_ServiceDomainBitMask_t;

#define TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_SMS 0x1
#define TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_RTT 0x2
#define TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VOLTE 0x4
#define TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_IMS_SERVICE 0x8
#define TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VONR 0x10
typedef uint64_t taf_pa_radio_ImsServiceSettingBitMask_t;

#define TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_450 0x1
#define TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_480 0x2
#define TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_750 0x4
#define TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_850 0x8
#define TAF_PA_RADIO_BITMASK_BAND_CLASS_E_GSM_900_BAND 0x10
#define TAF_PA_RADIO_BITMASK_BAND_CLASS_P_GSM_900_BAND 0x20
#define TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_RAILWAYS_900_BAND 0x40
#define TAF_PA_RADIO_BITMASK_BAND_CLASS_GSM_DCS_1800_BAND 0x80
#define TAF_PA_RADIO_BITMASK_BAND_GSM_PCS_1900_BAND 0x100
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_CH_IMT_2100_BAND 0x200
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_PCS_1900_BAND 0x400
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_CH_DCS_1800_BAND 0x800
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_1700_BAND 0x1000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_850_BAND 0x2000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_800_BAND 0x4000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_2600_BAND 0x8000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_900_BAND 0x10000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_1700_BAND 0x20000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_1500_BAND 0x40000
#define TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_850_BAND 0x80000
#define TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_A 0x100000
#define TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_B 0x200000
#define TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_C 0x400000
#define TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_D 0x800000
#define TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_E 0x1000000
#define TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_F 0x2000000
typedef uint64_t taf_pa_radio_BandBitMask_t;

#define TAF_PA_RADIO_BITMASK_CELL_ROLE_SERVING 0x1
#define TAF_PA_RADIO_BITMASK_CELL_ROLE_NEIGHBOR 0x2
typedef uint64_t taf_pa_radio_CellRoleBitMask_t;

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
    TAF_PA_RADIO_NETWORK_SELECTION_MODE_UNKNOWN = 0,
    TAF_PA_RADIO_NETWORK_SELECTION_MODE_AUTOMATIC = 1,
    TAF_PA_RADIO_NETWORK_SELECTION_MODE_MANUAL = 2
} taf_pa_radio_NetworkSelectionMode_t;

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
    TAF_PA_RADIO_SERVICE_DOMAIN_UNKNOWN = 0,
    TAF_PA_RADIO_SERVICE_DOMAIN_NO_SERVICE = 1,
    TAF_PA_RADIO_SERVICE_DOMAIN_CS_ONLY = 2,
    TAF_PA_RADIO_SERVICE_DOMAIN_PS_ONLY = 3,
    TAF_PA_RADIO_SERVICE_DOMAIN_CS_AND_PS = 4,
    TAF_PA_RADIO_SERVICE_DOMAIN_CAMPED = 5
} taf_pa_radio_ServiceDomain_t;

typedef enum
{
    TAF_PA_RADIO_EMERGENCY_MODE_UNKNOWN = 0,
    TAF_PA_RADIO_EMERGENCY_MODE_ON = 1,
    TAF_PA_RADIO_EMERGENCY_MODE_OFF = 2
} taf_pa_radio_EmergencyMode_t;

typedef enum
{
    TAF_PA_RADIO_REGISTRATION_STATE_UNKNOWN = 0,
    TAF_PA_RADIO_REGISTRATION_STATE_NOT_REGISTERED = 1,
    TAF_PA_RADIO_REGISTRATION_STATE_REGISTERED = 2,
    TAF_PA_RADIO_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING = 3,
    TAF_PA_RADIO_REGISTRATION_STATE_DENIED = 4
} taf_pa_radio_RegistrationState_t;

typedef enum
{
    TAF_PA_RADIO_ROAMING_INDICATOR_UNKNOWN = 0,
    TAF_PA_RADIO_ROAMING_INDICATOR_ON = 1,
    TAF_PA_RADIO_ROAMING_INDICATOR_OFF = 2,
    TAF_PA_RADIO_ROAMING_INDICATOR_FLASHING = 3
} taf_pa_radio_RoamingIndicator_t;

typedef enum
{
    TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_UNKNOWN = 0,
    TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_1 = 1,
    TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_2 = 2,
    TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_3 = 3,
    TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_4 = 4,
    TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_5 = 5,
} taf_pa_radio_SignalStrengthLevel_t;

typedef enum
{
    TAF_PA_RADIO_SIGNAL_METRIC_UNKNOWN = 0,
    TAF_PA_RADIO_SIGNAL_METRIC_RSSI = 1,
    TAF_PA_RADIO_SIGNAL_METRIC_ECIO = 2,
    TAF_PA_RADIO_SIGNAL_METRIC_IO = 3,
    TAF_PA_RADIO_SIGNAL_METRIC_SINR = 4,
    TAF_PA_RADIO_SIGNAL_METRIC_RSCP = 5,
    TAF_PA_RADIO_SIGNAL_METRIC_RSRP = 6,
    TAF_PA_RADIO_SIGNAL_METRIC_RSRQ = 7,
    TAF_PA_RADIO_SIGNAL_METRIC_SNR = 8
} taf_pa_radio_SignalMetric_t;

typedef enum
{
    TAF_PA_RADIO_DATA_SERVICE_STATE_UNKNOWN = 0,
    TAF_PA_RADIO_DATA_SERVICE_STATE_IN_SERVICE = 1,
    TAF_PA_RADIO_DATA_SERVICE_STATE_OUT_OF_SERVICE = 2
} taf_pa_radio_DataServiceState_t;

typedef enum
{
    TAF_PA_RADIO_CELL_LOCATION_UNKNOWN = 0,
    TAF_PA_RADIO_CELL_LOCATION_SERVING = 1,
    TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR = 2
} taf_pa_radio_CellLocation_t;

typedef enum
{
    TAF_PA_RADIO_NETWORK_ROAMING_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_NETWORK_ROAMING_STATUS_HOME = 1,
    TAF_PA_RADIO_NETWORK_ROAMING_STATUS_ROAMING = 2
} taf_pa_radio_NetworkRoamingStatus_t;

typedef enum
{
    TAF_PA_RADIO_NETWORK_IN_USE_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_NETWORK_IN_USE_STATUS_CURRENT_SERVING = 1,
    TAF_PA_RADIO_NETWORK_IN_USE_STATUS_AVAILABLE = 2
} taf_pa_radio_NetworkInUseStatus_t;

typedef enum
{
    TAF_PA_RADIO_NETWORK_FORBIDDEN_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_NETWORK_FORBIDDEN_STATUS_FORBIDDEN = 1,
    TAF_PA_RADIO_NETWORK_FORBIDDEN_STATUS_NOT_FORBIDDEN = 2
} taf_pa_radio_NetworkForbiddenStatus_t;

typedef enum
{
    TAF_PA_RADIO_NETWORK_PREFERRED_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_NETWORK_PREFERRED_STATUS_PREFERRED = 1,
    TAF_PA_RADIO_NETWORK_PREFERRED_STATUS_NOT_PREFERRED = 2
} taf_pa_radio_NetworkPreferredStatus_t;

typedef enum
{
    TAF_PA_RADIO_IMS_REGISTRATION_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_IMS_REGISTRATION_STATUS_NOT_REGISTERED = 1,
    TAF_PA_RADIO_IMS_REGISTRATION_STATUS_REGISTRERING = 2,
    TAF_PA_RADIO_IMS_REGISTRATION_STATUS_REGISTERED = 3,
    TAF_PA_RADIO_IMS_REGISTRATION_STATUS_LIMITED_REGISTERED = 4
} taf_pa_radio_ImsRegistrationStatus_t;

typedef enum
{
    TAF_PA_RADIO_LTE_CS_CAPABILITY_UNKNOWN = 0,
    TAF_PA_RADIO_LTE_CS_CAPABILITY_FULL_SERVICE = 1,
    TAF_PA_RADIO_LTE_CS_CAPABILITY_CSFB_NOT_PREFERRED = 2,
    TAF_PA_RADIO_LTE_CS_CAPABILITY_SMS_ONLY = 3,
    TAF_PA_RADIO_LTE_CS_CAPABILITY_LIMITED = 4,
    TAF_PA_RADIO_LTE_CS_CAPABILITY_BARRED = 5
} taf_pa_radio_LteCsCapability_t;

typedef enum
{
    TAF_PA_RADIO_IMS_SERVICE_UNKNOWN = 0,
    TAF_PA_RADIO_IMS_SERVICE_REGISTRATION = 1,
    TAF_PA_RADIO_IMS_SERVICE_SMS = 2,
    TAF_PA_RADIO_IMS_SERVICE_VOIP = 3,
    TAF_PA_RADIO_IMS_SERVICE_RTT = 4,
    TAF_PA_RADIO_IMS_SERVICE_VONR = 5
} taf_pa_radio_ImsService_t;

typedef enum
{
    TAF_PA_RADIO_IMS_SERVICE_STATUS_UNKNOWN = 0,
    TAF_PA_RADIO_IMS_SERVICE_STATUS_NO_SERVICE = 1,
    TAF_PA_RADIO_IMS_SERVICE_STATUS_LIMITED_SERVICE = 2,
    TAF_PA_RADIO_IMS_SERVICE_STATUS_FULL_SERVICE = 3
} taf_pa_radio_ImsServiceStatus_t;

typedef enum
{
	TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_UNKNOWN = 0,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_OTHER_FAILURE = 1,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_OPTION_UNSUBSCRIBED = 2,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_UNKNOWN_PDP = 3,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_REASON_NOT_SPECIFIED = 4,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_CONNECTION_BRINGUP_FAILURE = 5,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_CONNECTION_IKE_AUTH_FAILURE = 6,
    TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_USER_AUTH_FAILED = 7
} taf_pa_radio_ImsPdpFailureErrorCode_t;

typedef enum
{
	TAF_PA_RADIO_ENDC_AVAILABILITY_UNKNOWN = 0,
    TAF_PA_RADIO_ENDC_AVAILABILITY_AVAILABLE = 1,
    TAF_PA_RADIO_ENDC_AVAILABILITY_UNAVAILABLE = 2
} taf_pa_radio_EndcAvailability_t;

typedef enum
{
	TAF_PA_RADIO_DCNR_RESTRICTION_UNKNOWN = 0,
    TAF_PA_RADIO_DCNR_RESTRICTION_RESTRICTED = 1,
    TAF_PA_RADIO_DCNR_RESTRICTION_NOT_RESTRICTED = 2
} taf_pa_radio_DcnrRestriction_t;

typedef enum
{
    TAF_PA_RADIO_BANDWIDTH_UNKNOWN = 0,
    TAF_PA_RADIO_BANDWIDTH_GSM_BW_NRB_2 = 1,
    TAF_PA_RADIO_BANDWIDTH_WCDMA_BW_NRB_5 = 2,
    TAF_PA_RADIO_BANDWIDTH_WCDMA_BW_NRB_10 = 3,
    TAF_PA_RADIO_BANDWIDTH_TDSCDMA_BW_NRB_2= 4,
    TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_6 = 5,
    TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_15 = 6,
    TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_25 = 7,
    TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_50 = 8,
    TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_75 = 9,
    TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_100 = 10,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_5 = 11,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_10 = 12,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_15 = 13,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_20 = 14,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_25 = 15,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_30 = 16,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_40 = 17,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_50 = 18,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_60 = 19,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_70 = 20,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_80 = 21,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_90 = 22,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_100 = 23,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_200 = 24,
    TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_400 = 25
} taf_pa_radio_Bandwidth_t;

typedef enum
{
    TAF_PA_RADIO_NR_ICON_UNKNOWN = 0,
    TAF_PA_RADIO_NR_ICON_NONE = 1,
    TAF_PA_RADIO_NR_ICON_BASIC = 2,
    TAF_PA_RADIO_NR_ICON_UWB = 3
} taf_pa_radio_NrIcon_t;

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
    taf_pa_radio_NetworkSelectionMode_t mode;
    uint16_t mcc;
    uint16_t mnc;
    uint8_t mncIncludesPcsDigit;
} taf_pa_radio_NetworkSelectionPreference_t;

typedef struct
{
    taf_pa_radio_RatBitMask_t bitmask;
    uint16_t mcc;
    uint16_t mnc;
    uint8_t mncIncludesPcsDigit;
} taf_pa_radio_PreferredNetwork_t;

typedef struct
{
    uint8_t clearPrevious;
    uint32_t networkCount;
    taf_pa_radio_PreferredNetwork_t networks[TAF_PA_RADIO_PREFERRED_NETWORK_MAX_COUNT];
} taf_pa_radio_PreferredNetworkConfig_t;

typedef struct
{
    uint8_t staticNetworksValid;
    uint32_t staticNetworkCount;
    taf_pa_radio_PreferredNetwork_t staticNetworks[TAF_PA_RADIO_STATIC_PREFERRED_NETWORK_MAX_COUNT];
    uint8_t nonStaticNetworksValid;
    uint32_t nonStaticNetworkCount;
    taf_pa_radio_PreferredNetwork_t nonStaticNetworks[TAF_PA_RADIO_PREFERRED_NETWORK_MAX_COUNT];
} taf_pa_radio_PreferredNetworks_t;

typedef struct
{
    uint8_t emergModeValid;
    taf_pa_radio_EmergencyMode_t emergMode;
    taf_pa_radio_RegistrationState_t regState;
    uint8_t roamingIndicatorValid;
    taf_pa_radio_RoamingIndicator_t roamingIndicator;
} taf_pa_radio_VoiceServiceInfo_t;

typedef struct
{
    int32_t rssi;
    int32_t ber;
} taf_pa_radio_GsmSignalStrengthInfo_t;

typedef struct
{
    int32_t ss;
    int32_t ecio;
    int32_t io;
    int32_t sinr;
} taf_pa_radio_CdmaSignalStrengthInfo_t;

typedef struct
{
    int32_t ss;
    int32_t ecio;
    int32_t ber;
    int32_t bler;
    int32_t rscp;
} taf_pa_radio_UmtsSignalStrengthInfo_t;

typedef struct
{
    int32_t rssi;
    int32_t rscp;
    int32_t ecio;
    int32_t sinr;
} taf_pa_radio_TdscdmaSignalStrengthInfo_t;

typedef struct
{
    int32_t rssi;
    int32_t rsrq;
    int32_t rsrp;
    int32_t snr;
} taf_pa_radio_LteSignalStrengthInfo_t;

typedef struct
{
    int32_t rsrq;
    int32_t rsrp;
    int32_t snr;
} taf_pa_radio_Nr5gSignalStrengthInfo_t;

typedef struct
{
    taf_pa_radio_RatBitMask_t bitmask;
    taf_pa_radio_GsmSignalStrengthInfo_t gsmInfo;
    taf_pa_radio_CdmaSignalStrengthInfo_t cdmaInfo;
    taf_pa_radio_UmtsSignalStrengthInfo_t umtsInfo;
    taf_pa_radio_TdscdmaSignalStrengthInfo_t tdscdmaInfo;
    taf_pa_radio_LteSignalStrengthInfo_t lteInfo;
    taf_pa_radio_Nr5gSignalStrengthInfo_t nr5gInfo;
} taf_pa_radio_SignalStrengthInfo_t;

typedef struct
{
    taf_pa_radio_Rat_t rat;
    taf_pa_radio_SignalMetric_t metric;
    uint8_t thresholdValid;
    uint32_t thresholdCount;
    int32_t thresholds[TAF_PA_RADIO_SIGNAL_STRENGTH_THRESHOLD_MAX_COUNT];
    uint8_t deltaValid;
    uint32_t delta;
    uint8_t hysteresisTimeValid;
    uint32_t hysteresisTime;
    uint8_t hysteresisDeltaValid;
    uint32_t hysteresisDelta;
} taf_pa_radio_SignalStrengthIndConfig_t;

typedef struct
{
    uint16_t mcc;
    uint16_t mnc;
    uint8_t mncIncludesPcsDigit;
} taf_pa_radio_PlmnId_t;

typedef struct
{
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
    uint32_t cid;
    uint16_t lac;
    uint16_t arfcn;
    uint8_t bsic;
    uint32_t ta;
    int32_t rssi;
} taf_pa_radio_GsmCellLocationInfo_t;

typedef struct
{
    uint16_t sid;
    uint16_t nid;
    uint16_t bsid;
    uint16_t refpn;
    uint32_t bslat;
    uint32_t bslong;
    int32_t ss;
} taf_pa_radio_CdmaCellLocationInfo_t;

typedef struct
{
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
    uint32_t cid;
    uint16_t lac;
    uint16_t uarfcn;
    uint16_t psc;
    int16_t rscp;
    int16_t ecio;
    int32_t ss;
} taf_pa_radio_UmtsCellLocationInfo_t;

typedef struct
{
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
    uint16_t lac;
    uint16_t uarfcn;
    uint32_t cid;
    uint8_t cpid;
    uint32_t ta;
    int32_t rscp;
} taf_pa_radio_TdscdmaCellLocationInfo_t;

typedef struct
{
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
    uint32_t cid;
    uint16_t pcid;
    uint16_t tac;
    uint32_t earfcn;
    int32_t ta;
    int32_t rssi;
} taf_pa_radio_LteCellLocationInfo_t;

typedef struct
{
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
    uint64_t cid;
    uint32_t pcid;
    int32_t tac;
    int32_t arfcn;
} taf_pa_radio_Nr5gCellLocationInfo_t;

typedef struct
{
    taf_pa_radio_CellLocation_t location;
    taf_pa_radio_Rat_t rat;
    union
    {
        taf_pa_radio_GsmCellLocationInfo_t gsmInfo;
        taf_pa_radio_CdmaCellLocationInfo_t cdmaInfo;
        taf_pa_radio_UmtsCellLocationInfo_t umtsInfo;
        taf_pa_radio_TdscdmaCellLocationInfo_t tdscdmaInfo;
        taf_pa_radio_LteCellLocationInfo_t lteInfo;
        taf_pa_radio_Nr5gCellLocationInfo_t nr5gInfo;
    };
} taf_pa_radio_CellLocationInfo_t;

typedef struct
{
    uint32_t cellLocInfoCount;
    taf_pa_radio_CellLocationInfo_t cellLocInfo[TAF_PA_RADIO_CELL_LOCATION_MAX_COUNT];
} taf_pa_radio_CellLocationListInfo_t;

typedef struct
{
    uint8_t shortNameValid;
    size_t shortNameSize;
    char* shortNamePtr;
    uint8_t fullNameValid;
    size_t fullNameSize;
    char* fullNamePtr;
} taf_pa_radio_CurrNetworkName_t;

typedef struct
{
    taf_pa_radio_RatBitMask_t bitmask;
    uint32_t timeout;
} taf_pa_radio_PlmnNetworkScanConfig_t;

typedef struct
{
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
    taf_pa_radio_Rat_t rat;
    taf_pa_radio_NetworkRoamingStatus_t roamingStatus;
    taf_pa_radio_NetworkInUseStatus_t inUseStatus;
    taf_pa_radio_NetworkForbiddenStatus_t forbiddenStatus;
    taf_pa_radio_NetworkPreferredStatus_t preferredStatus;
    char description[TAF_PA_RADIO_PLMN_NETWORK_DESCRIPTION_MAX_BYTES];
} taf_pa_radio_PlmnInformation_t;

typedef struct
{
    uint32_t plmnCount;
    taf_pa_radio_PlmnInformation_t plmnInfo[TAF_PA_RADIO_PLMN_SCAN_NETWORK_MAX_COUNT];
} taf_pa_radio_PlmnScanInformation_t;

typedef struct
{
    uint64_t bitmask[TAF_PA_RADIO_LTE_BAND_GROUP_COUNT];
} taf_pa_radio_LteBand_t;

typedef struct
{
    uint8_t totalCount;
    uint8_t maxActiveCount;
} taf_pa_radio_SimCapabilityInfo_t;

typedef struct
{
    taf_pa_radio_RatBitMask_t devBitmask;
    taf_pa_radio_RatBitMask_t simBitmask;
} taf_pa_radio_DeviceAndSimCardRatCapability_t;

typedef struct
{
    uint8_t bandInfoValid;
    taf_pa_radio_BandBitMask_t activeBand;
    taf_pa_radio_Bandwidth_t bandwidth;
    uint8_t lteBandInfoValid;
    size_t lteActiveBandSize;
    uint32_t* lteActiveBandPtr;
    size_t lteBandwidthSize;
    taf_pa_radio_Bandwidth_t* lteBandwidthPtr;
    uint8_t nr5gBandInfoValid;
    size_t nr5gActiveBandSize;
    uint32_t* nr5gActiveBandPtr;
    size_t nr5gBandwidthSize;
    taf_pa_radio_Bandwidth_t* nr5gBandwidthPtr;
} taf_pa_radio_ServingCellBandInfo_t;

typedef struct
{
    taf_pa_radio_Rat_t rat;
    taf_pa_radio_ServiceDomain_t domain;
    int32_t cause;
    uint8_t plmnIdValid;
    taf_pa_radio_PlmnId_t plmnId;
} taf_pa_radio_NetworkRejectIndication_t;

typedef struct
{
    taf_pa_radio_Rat_t rat;
} taf_pa_radio_RatChangeIndication_t;

typedef struct
{
    taf_pa_radio_VoiceServiceInfo_t info;
} taf_pa_radio_VoiceServiceInfoIndication_t;

typedef struct
{
    taf_pa_radio_DataServiceState_t state;
} taf_pa_radio_DataServiceStatusIndication_t;

typedef struct
{
    taf_pa_radio_DataRoamingStatus_t status;
} taf_pa_radio_DataRoamingStatusIndication_t;

typedef struct
{
    taf_pa_radio_SignalStrengthInfo_t info;
} taf_pa_radio_SignalStrengthInfoChangeIndication_t;

typedef struct
{
    taf_pa_radio_ImsRegistrationStatus_t status;
} taf_pa_radio_ImsRegStatusChangeIndication_t;

typedef struct
{
    taf_pa_radio_OperatingMode_t mode;
} taf_pa_radio_OperatingModeChangeIndication_t;

typedef struct
{
    taf_pa_radio_ServiceDomain_t domain;
} taf_pa_radio_ServiceDomainIndication_t;

typedef struct
{
    taf_pa_radio_LteCsCapability_t capability;
} taf_pa_radio_LteCsCapabilityIndication_t;


typedef struct
{
    uint8_t voipServiceStatusValid;
    taf_pa_radio_ImsServiceStatus_t voipServiceStatus;
    uint8_t smsServiceStatusValid;
    taf_pa_radio_ImsServiceStatus_t smsServiceStatus;
} taf_pa_radio_ImsServiceInfoIndication_t;

typedef struct
{
    uint8_t failureErrorCodeValid;
    taf_pa_radio_ImsPdpFailureErrorCode_t failureErrorCode;
} taf_pa_radio_ImsPdpErrorIndication_t;

typedef struct
{
    uint8_t cellRoleValid;
    taf_pa_radio_CellRoleBitMask_t cellRole;
} taf_pa_radio_CellInfoChangeIndication_t;

typedef struct
{
    taf_pa_radio_NrIcon_t icon;
} taf_pa_radio_NrIconChangeIndication_t;

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

typedef struct taf_pa_radio_NetworkRejectHandler* taf_pa_radio_NetworkRejectHandlerRef_t;

typedef struct taf_pa_radio_RatChangeHandler* taf_pa_radio_RatChangeHandlerRef_t;

typedef struct taf_pa_radio_VoiceServiceInfoHandler* taf_pa_radio_VoiceServiceInfoHandlerRef_t;

typedef struct taf_pa_radio_DataServiceStatusHandler* taf_pa_radio_DataServiceStatusHandlerRef_t;

typedef struct taf_pa_radio_DataRoamingStatusHandler* taf_pa_radio_DataRoamingStatusHandlerRef_t;

typedef struct taf_pa_radio_SignalStrengthInfoChangeHandler* taf_pa_radio_SignalStrengthInfoChangeHandlerRef_t;

typedef struct taf_pa_radio_ImsRegStatusChangeHandler* taf_pa_radio_ImsRegStatusChangeHandlerRef_t;

typedef struct taf_pa_radio_OperatingModeChangeHandler* taf_pa_radio_OperatingModeChangeHandlerRef_t;

typedef struct taf_pa_radio_ServiceDomainHandler* taf_pa_radio_ServiceDomainHandlerRef_t;

typedef struct taf_pa_radio_LteCsCapabilityHandler* taf_pa_radio_LteCsCapabilityHandlerRef_t;

typedef struct taf_pa_radio_ImsServiceInfoHandler* taf_pa_radio_ImsServiceInfoHandlerRef_t;

typedef struct taf_pa_radio_ImsPdpErrorHandler* taf_pa_radio_ImsPdpErrorHandlerRef_t;

typedef struct taf_pa_radio_CellInfoChangeHandler* taf_pa_radio_CellInfoChangeHandlerRef_t;

typedef struct taf_pa_radio_NrIconChangeHandler* taf_pa_radio_NrIconChangeHandlerRef_t;

typedef struct taf_pa_radio_RatSvcStatusHandler* taf_pa_radio_RatSvcStatusHandlerRef_t;

typedef struct taf_pa_radio_LteCphyCaHandler* taf_pa_radio_LteCphyCaHandlerRef_t;

typedef struct taf_pa_radio_DataAvailSysStatusHandler* taf_pa_radio_DataAvailSysStatusHandlerRef_t;

typedef void (*taf_pa_radio_NetworkRejectHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_NetworkRejectIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_RatChangeHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_RatChangeIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_VoiceServiceInfoHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_VoiceServiceInfoIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_DataServiceStatusHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_DataServiceStatusIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_DataRoamingStatusHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_DataRoamingStatusIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_SignalStrengthInfoChangeHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthInfoChangeIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_ImsRegStatusChangeHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_ImsRegStatusChangeIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_OperatingModeChangeHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_OperatingModeChangeIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_ServiceDomainHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_LteCsCapabilityHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_LteCsCapabilityIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_ImsServiceInfoHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_ImsServiceInfoIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_ImsPdpErrorHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_ImsPdpErrorIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_CellInfoChangeHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_CellInfoChangeIndication_t indication,
    void* contextPtr
);

typedef void (*taf_pa_radio_NrIconChangeHdlrFunc_t)
(
    uint32_t instance,
    taf_pa_radio_NrIconChangeIndication_t indication,
    void* contextPtr
);

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

PA_SHARED pa_result_t taf_pa_radio_Init
(
    void
);

PA_SHARED pa_result_t taf_pa_radio_Deinit
(
    void
);

PA_SHARED pa_result_t taf_pa_radio_GetOperatingMode
(
    uint32_t instance,
    taf_pa_radio_OperatingMode_t* modePtr
);

PA_SHARED pa_result_t taf_pa_radio_SetOperatingMode
(
    uint32_t instance,
    taf_pa_radio_OperatingMode_t mode
);

PA_SHARED pa_result_t taf_pa_radio_SetNetworkSelectionPreference
(
    uint32_t instance,
    taf_pa_radio_NetworkSelectionPreference_t* preferencePtr
);

PA_SHARED pa_result_t taf_pa_radio_GetNetworkSelectionPreference
(
    uint32_t instance,
    taf_pa_radio_NetworkSelectionPreference_t* preferencePtr
);

PA_SHARED pa_result_t taf_pa_radio_SetPreferredNetwork
(
    uint32_t instance,
    taf_pa_radio_PreferredNetworkConfig_t* configPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetPreferredNetwork
(
    uint32_t instance,
    taf_pa_radio_PreferredNetworks_t* networksPtr
);

PA_SHARED pa_result_t taf_pa_radio_SetPreferredRat
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t bitmask
);

PA_SHARED pa_result_t taf_pa_radio_GetPreferredRat
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t* bitmaskPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetVoiceServiceInfo
(
    uint32_t instance,
    taf_pa_radio_VoiceServiceInfo_t* infoPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetDataServieState
(
    uint32_t instance,
    taf_pa_radio_DataServiceState_t* statePtr
);

PA_SHARED pa_result_t taf_pa_radio_GetServiceDomain
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_ServiceDomain_t* domainPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetServiceDomainPreferences
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainBitMask_t* bitmaskPtr
);

PA_SHARED pa_result_t taf_pa_radio_SetServiceDomainPreferences
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainBitMask_t bitmask
);

PA_SHARED pa_result_t taf_pa_radio_GetSignalStrengthLevel
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_SignalStrengthLevel_t* levelPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetSignalStrengthInfo
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthInfo_t* infoPtr
);

PA_SHARED pa_result_t taf_pa_radio_SetSignalStrengthInd
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthIndConfig_t* configPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetCellLocationListInfo
(
    uint32_t instance,
    taf_pa_radio_CellLocationListInfo_t* infoPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetCurrNetworkName
(
    uint32_t instance,
    taf_pa_radio_CurrNetworkName_t* namePtr
);

PA_SHARED pa_result_t taf_pa_radio_PerformPlmnNetworkScan
(
    uint32_t instance,
    taf_pa_radio_PlmnNetworkScanConfig_t* configPtr,
    taf_pa_radio_PlmnScanInformation_t* informationPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetBandCapabilities
(
    uint32_t instance,
    taf_pa_radio_BandBitMask_t* bitmaskPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetLteBandCapabilities
(
    uint32_t instance,
    taf_pa_radio_LteBand_t* bandPtr
);

PA_SHARED pa_result_t taf_pa_radio_SetBandPreferences
(
    uint32_t instance,
    taf_pa_radio_BandBitMask_t bitmask
);

PA_SHARED pa_result_t taf_pa_radio_GetBandPreferences
(
    uint32_t instance,
    taf_pa_radio_BandBitMask_t* bitmaskPtr
);

PA_SHARED pa_result_t taf_pa_radio_SetLteBandPreferences
(
    uint32_t instance,
    taf_pa_radio_LteBand_t* bandPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetLteBandPreferences
(
    uint32_t instance,
    taf_pa_radio_LteBand_t* bandPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetImsRegistrationStatus
(
    uint32_t instance,
    taf_pa_radio_ImsRegistrationStatus_t* statusPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetLteCsCapability
(
    uint32_t instance,
    taf_pa_radio_LteCsCapability_t* capabilityPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetImsServiceStatus
(
    uint32_t instance,
    taf_pa_radio_ImsService_t service,
    taf_pa_radio_ImsServiceStatus_t* statusPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetImsPdpFailureErrorCode
(
    uint32_t instance,
    taf_pa_radio_ImsPdpFailureErrorCode_t* codePtr
);

PA_SHARED pa_result_t taf_pa_radio_ToggleImsService
(
    uint32_t instance,
    taf_pa_radio_ImsServiceSettingBitMask_t bitmask,
    bool enable
);

PA_SHARED pa_result_t taf_pa_radio_GetEnabledImsService
(
    uint32_t instance,
    taf_pa_radio_ImsServiceSettingBitMask_t* bitmaskPtr
);

PA_SHARED pa_result_t taf_pa_radio_SetImsUserAgent
(
    uint32_t instance,
    const char* namePtr
);

PA_SHARED pa_result_t taf_pa_radio_GetImsUserAgent
(
    uint32_t instance,
    char* namePtr,
    size_t namePtrSize
);

PA_SHARED pa_result_t taf_pa_radio_GetEndcAvailability
(
    uint32_t instance,
    taf_pa_radio_EndcAvailability_t* availabilityPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetDcnrRestriction
(
    uint32_t instance,
    taf_pa_radio_DcnrRestriction_t* restrictionPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetSimCapacityInfo
(
    taf_pa_radio_SimCapabilityInfo_t* infoPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetDeviceAndSimCardRatCapability
(
    uint32_t instance,
    taf_pa_radio_DeviceAndSimCardRatCapability_t* capabilityPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetServingCellBandInfo
(
    uint32_t instance,
    taf_pa_radio_ServingCellBandInfo_t* infoPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetNrIcon
(
    uint32_t instance,
    taf_pa_radio_NrIcon_t* iconPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddNetworkRejectHandler
(
    uint32_t instance,
    taf_pa_radio_NetworkRejectHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_NetworkRejectHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddRatChangeHandler
(
    uint32_t instance,
    taf_pa_radio_RatChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_RatChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddVoiceServiceInfoHandler
(
    uint32_t instance,
    taf_pa_radio_VoiceServiceInfoHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_VoiceServiceInfoHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddDataServiceStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataServiceStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_DataServiceStatusHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddDataRoamingStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataRoamingStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_DataRoamingStatusHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddSignalStrengthInfoChangeHandler
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthInfoChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_SignalStrengthInfoChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddImsRegStatusChangeHandler
(
    uint32_t instance,
    taf_pa_radio_ImsRegStatusChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_ImsRegStatusChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddCellInfoChangeHandler
(
    uint32_t instance,
    taf_pa_radio_CellInfoChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_CellInfoChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddOperatingModeChangeHandler
(
    uint32_t instance,
    taf_pa_radio_OperatingModeChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_OperatingModeChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddServiceDomainHandler
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_ServiceDomainHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddLteCsCapabilityHandler
(
    uint32_t instance,
    taf_pa_radio_LteCsCapabilityHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_LteCsCapabilityHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddImsServiceInfoHandler
(
    uint32_t instance,
    taf_pa_radio_ImsServiceInfoHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_ImsServiceInfoHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddImsPdpErrorHandler
(
    uint32_t instance,
    taf_pa_radio_ImsPdpErrorHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_ImsPdpErrorHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddNrIconChangeHandler
(
    uint32_t instance,
    taf_pa_radio_NrIconChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_NrIconChangeHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_RegisterIndication
(
    uint32_t instance,
    uint8_t registration
);

PA_SHARED pa_result_t taf_pa_radio_PerformPciNetworkScan
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t bitmask,
    taf_pa_radio_PciScanInformation_t* informationPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetServingRat
(
    uint32_t instance,
    taf_pa_radio_Rat_t* ratPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetRatSvcStatus
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_RatServiceStatus_t* statusPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetServingCellRac
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    uint8_t* racPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetDataAvailSysStatus
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatus_t* statusPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetLteCphyCaInfo
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaInfo_t* infoPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddRatSvcStatusHandler
(
    uint32_t instance,
    taf_pa_radio_RatSvcStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_RatSvcStatusHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddLteCphyCaHandler
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_LteCphyCaHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_AddDataAvailSysStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_radio_DataAvailSysStatusHandlerRef_t* handlerRefPtr
);

PA_SHARED pa_result_t taf_pa_radio_GetDataCurrRoamingStatus
(
    uint32_t instance,
    taf_pa_radio_DataRoamingStatus_t* statusPtr
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_RADIO_PA_HPP */
