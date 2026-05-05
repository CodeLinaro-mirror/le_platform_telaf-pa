/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef TAF_ECALL_PA_HPP
#define TAF_ECALL_PA_HPP

#include <chrono>
#include <future>
#include <unistd.h>
#include <any>
#include <bitset>
#include <string>
#include <vector>
#include <memory>
#include "tafCommonPa.h"

namespace tafpa::ecall {

// Defines operational modes for eCall.
enum class taf_pa_ecall_mode_t : uint8_t
{
    NORMAL = 0,
    ONLY = 1,
    INVALID = 2
};

// HLAP timer states.
enum class taf_pa_ecall_hlap_timer_state_t : int8_t
{
    UNKNOWN = -1,
    INACTIVE,
    ACTIVE
};

// HLAP timer types.
enum class taf_pa_ecall_hlap_timer_type_t : uint8_t
{
    UNKNOWN = 0,
    T2 = 2,
    T5 = 5,
    T6 = 6,
    T7 = 7,
    T9 = 9,
    T10 = 10
};

// HLAP timer events.
enum class taf_pa_ecall_hlap_event_t : int8_t
{
    UNKNOWN = -1,
    UNCHANGED,
    STARTED,
    STOPPED,
    EXPIRED,
    RESUMED
};

// Call status values.
enum class taf_pa_ecall_call_status_t : int8_t
{
    IDLE = -1,
    ACTIVE,
    ON_HOLD,
    DIALING,
    INCOMING,
    WAITING,
    ALERTING,
    ENDED
};

// Call termination reasons.
enum class taf_pa_ecall_termination_t
{
    UNOBTAINABLE_NUMBER = 1,
    NO_ROUTE_TO_DESTINATION = 3,
    CHANNEL_UNACCEPTABLE = 6,
    OPERATOR_DETERMINED_BARRING = 8,
    NORMAL = 16,
    BUSY = 17,
    NO_USER_RESPONDING = 18,
    NO_ANSWER_FROM_USER = 19,
    NOT_REACHABLE = 20,
    CALL_REJECTED = 21,
    NUMBER_CHANGED = 22,
    PREEMPTION = 25,
    DESTINATION_OUT_OF_ORDER = 27,
    INVALID_NUMBER_FORMAT = 28,
    FACILITY_REJECTED = 29,
    RESP_TO_STATUS_ENQUIRY = 30,
    NORMAL_UNSPECIFIED = 31,
    CONGESTION = 34,
    NETWORK_OUT_OF_ORDER = 38,
    TEMPORARY_FAILURE = 41,
    SWITCHING_EQUIPMENT_CONGESTION = 42,
    ACCESS_INFORMATION_DISCARDED = 43,
    REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE = 44,
    RESOURCES_UNAVAILABLE_OR_UNSPECIFIED = 47,
    QOS_UNAVAILABLE = 49,
    REQUESTED_FACILITY_NOT_SUBSCRIBED = 50,
    INCOMING_CALLS_BARRED_WITHIN_CUG = 55,
    BEARER_CAPABILITY_NOT_AUTHORIZED = 57,
    BEARER_CAPABILITY_UNAVAILABLE = 58,
    SERVICE_OPTION_NOT_AVAILABLE = 63,
    BEARER_SERVICE_NOT_IMPLEMENTED = 65,
    ACM_LIMIT_EXCEEDED = 68,
    REQUESTED_FACILITY_NOT_IMPLEMENTED = 69,
    ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE = 70,
    SERVICE_OR_OPTION_NOT_IMPLEMENTED = 79,
    INVALID_TRANSACTION_IDENTIFIER = 81,
    USER_NOT_MEMBER_OF_CUG = 87,
    INCOMPATIBLE_DESTINATION = 88,
    INVALID_TRANSIT_NW_SELECTION = 91,
    SEMANTICALLY_INCORRECT_MESSAGE = 95,
    INVALID_MANDATORY_INFORMATION = 96,
    MESSAGE_TYPE_NON_IMPLEMENTED = 97,
    MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE = 98,
    INFORMATION_ELEMENT_NON_EXISTENT = 99,
    CONDITIONAL_IE_ERROR = 100,
    MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE = 101,
    RECOVERY_ON_TIMER_EXPIRED = 102,
    PROTOCOL_ERROR_UNSPECIFIED = 111,
    INTERWORKING_UNSPECIFIED = 127,
    CALL_BARRED = 240,
    FDN_BLOCKED = 241,
    IMSI_UNKNOWN_IN_VLR = 242,
    IMEI_NOT_ACCEPTED = 243,
    DIAL_MODIFIED_TO_USSD = 244,
    DIAL_MODIFIED_TO_SS = 245,
    DIAL_MODIFIED_TO_DIAL = 246,
    RADIO_OFF = 247,
    OUT_OF_SERVICE = 248,
    NO_VALID_SIM = 249,
    RADIO_INTERNAL_ERROR = 250,
    NETWORK_RESP_TIMEOUT = 251,
    NETWORK_REJECT = 252,
    RADIO_ACCESS_FAILURE = 253,
    RADIO_LINK_FAILURE = 254,
    RADIO_LINK_LOST = 255,
    RADIO_UPLINK_FAILURE = 256,
    RADIO_SETUP_FAILURE = 257,
    RADIO_RELEASE_NORMAL = 258,
    RADIO_RELEASE_ABNORMAL = 259,
    ACCESS_CLASS_BLOCKED = 260,
    NETWORK_DETACH = 261,
    EMERGENCY_TEMP_FAILURE = 325,
    EMERGENCY_PERM_FAILURE = 326,
    HO_NOT_FEASIBLE = 382,
    USER_BUSY = 501,
    USER_REJECT = 502,
    LOW_BATTERY = 503,
    BLACKLISTED_CALL_ID = 504,
    CS_RETRY_REQUIRED = 505,

    CDMA_LOCKED_UNTIL_POWER_CYCLE = 1000,
    CDMA_DROP = 1001,
    CDMA_INTERCEPT = 1002,
    CDMA_REORDER = 1003,
    CDMA_SO_REJECT = 1004,
    CDMA_RETRY_ORDER = 1005,
    CDMA_ACCESS_FAILURE = 1006,
    CDMA_PREEMPTED = 1007,
    CDMA_NOT_EMERGENCY = 1008,

    CDMA_ACCESS_BLOCKED = 1009,
    NETWORK_UNAVAILABLE = 1010,
    FEATURE_UNAVAILABLE = 1011,
    SIP_ERROR = 1012,
    MISC = 1013,
    ANSWERED_ELSEWHERE = 1014,

    PULL_OUT_OF_SYNC = 1015,
    CAUSE_CALL_PULLED = 1016,

    SIP_REDIRECTED = 2001,
    SIP_BAD_REQUEST = 2002,
    SIP_FORBIDDEN = 2003,
    SIP_NOT_FOUND = 2004,
    SIP_NOT_SUPPORTED = 2005,
    SIP_REQUEST_TIMEOUT = 2006,
    SIP_TEMPORARILY_UNAVAILABLE = 2007,
    SIP_BAD_ADDRESS = 2008,
    SIP_BUSY = 2009,
    SIP_REQUEST_CANCELLED = 2010,
    SIP_NOT_ACCEPTABLE = 2011,
    SIP_NOT_REACHABLE = 2012,
    SIP_SERVER_INTERNAL_ERROR = 2013,
    SIP_SERVER_NOT_IMPLEMENTED = 2014,
    SIP_SERVER_BAD_GATEWAY = 2015,
    SIP_SERVICE_UNAVAILABLE = 2016,
    SIP_SERVER_TIMEOUT = 2017,
    SIP_SERVER_VERSION_UNSUPPORTED = 2018,
    SIP_SERVER_MESSAGE_TOOLARGE = 2019,
    SIP_SERVER_PRECONDITION_FAILURE = 2020,
    SIP_USER_REJECTED = 2021,
    SIP_GLOBAL_ERROR = 2022,
    MEDIA_INIT_FAILED = 3001,
    MEDIA_NO_DATA = 3002,
    MEDIA_NOT_ACCEPTABLE = 3003,
    MEDIA_UNSPECIFIED_ERROR = 3004,
    HOLD_RESUME_FAILED = 3005,
    HOLD_RESUME_CANCELED = 3006,
    HOLD_REINVITE_COLLISION = 3007,
    SIP_ALTERNATE_EMERGENCY_CALL = 3008,
    NO_CSFB_IN_CS_ROAM = 3009,
    SRV_NOT_REGISTERED = 3010,
    CALL_TYPE_NOT_ALLOWED = 3011,
    EMRG_CALL_ONGOING = 3012,
    CALL_SETUP_ONGOING = 3013,
    MAX_CALL_LIMIT_REACHED = 3014,
    UNSUPPORTED_SIP_HDRS = 3015,
    CALL_TRANSFER_ONGOING = 3016,
    PRACK_TIMEOUT = 3017,
    QOS_FAILURE = 3018,
    ONGOING_HANDOVER = 3019,
    VT_WITH_TTY_NOT_ALLOWED = 3020,
    CALL_UPGRADE_ONGOING = 3021,
    CONFERENCE_WITH_TTY_NOT_ALLOWED = 3022,
    CALL_CONFERENCE_ONGOING = 3023,
    VT_WITH_AVPF_NOT_ALLOWED = 3024,
    ENCRYPTION_CALL_ONGOING = 3025,
    CALL_ONGOING_CW_DISABLED = 3026,
    CALL_ON_OTHER_SUB = 3027,
    ONE_X_COLLISION = 3028,
    UI_NOT_READY = 3029,
    CS_CALL_ONGOING = 3030,
    REJECTED_ELSEWHERE = 3031,
    USER_REJECTED_SESSION_MODIFICATION = 3032,
    USER_CANCELLED_SESSION_MODIFICATION = 3033,
    SESSION_MODIFICATION_FAILED = 3034,
    SIP_UNAUTHORIZED = 3035,
    SIP_PAYMENT_REQUIRED = 3036,
    SIP_METHOD_NOT_ALLOWED = 3037,
    SIP_PROXY_AUTHENTICATION_REQUIRED = 3038,
    SIP_REQUEST_ENTITY_TOO_LARGE = 3039,
    SIP_REQUEST_URI_TOO_LARGE = 3040,
    SIP_EXTENSION_REQUIRED = 3041,
    SIP_INTERVAL_TOO_BRIEF = 3042,
    SIP_CALL_OR_TRANS_DOES_NOT_EXIST = 3043,
    SIP_LOOP_DETECTED = 3044,
    SIP_TOO_MANY_HOPS = 3045,
    SIP_AMBIGUOUS = 3046,
    SIP_REQUEST_PENDING = 3047,
    SIP_UNDECIPHERABLE = 3048,
    RETRY_ON_IMS_WITHOUT_RTT = 3049,
    MAX_PS_CALLS = 3050,
    SIP_MULTIPLE_CHOICES = 3051,
    SIP_MOVED_PERMANENTLY = 3052,
    SIP_MOVED_TEMPORARILY = 3053,
    SIP_USE_PROXY = 3054,
    SIP_ALTERNATE_SERVICE = 3055,
    SIP_UNSUPPORTED_URI_SCHEME = 3056,
    SIP_REMOTE_UNSUPP_MEDIA_TYPE = 3057,
    SIP_BAD_EXTENSION = 3058,
    DSDA_CONCURRENT_CALL_NOT_POSSIBLE = 3059,
    EPSFB_FAILURE = 3060,
    TWAIT_EXPIRED = 3061,
    TCP_CONNECTION_REQ = 3062,
    THERMAL_EMERGENCY = 3100,
    ERROR_UNSPECIFIED = 0xffff,
};

// MSD transmission status codes.
enum class taf_pa_ecall_msd_status_t : uint8_t
{
    SUCCESS = 0,
    FAILURE = 1,
    TRANSMISSION_STARTED = 2,
    NACK_OUT_OF_ORDER = 3,
    ACK_OUT_OF_ORDER = 4,
    START_RECEIVED = 5,
    LL_ACK_RECEIVED = 6,
    OUTBAND_MSD_TRANSMISSION_STARTED = 10,
    OUTBAND_MSD_TRANSMISSION_SUCCESS = 11,
    OUTBAND_MSD_TRANSMISSION_FAILURE = 12,
    LL_NACK_DUE_TO_T7_EXPIRY = 13,
    MSD_AL_ACK_CLEARDOWN = 14
};

// Call type classifications.
enum class taf_pa_ecall_type_t : uint8_t
{
    TEST = 1,
    EMERGENCY = 2,
    VOICE = 4
};

// Call category classifications.
enum class taf_pa_ecall_category_t : uint8_t
{
    AUTO = 64,
    MANUAL = 32
};

// Number type classifications.
enum class taf_pa_ecall_num_type_t : uint8_t
{
    DEFAULT,
    OVERRIDDEN
};

// HLAP timer ID types.
enum class taf_pa_ecall_hlap_timer_id_t : uint8_t
{
    UNKNOWN = 0,
    T9 = 5,
    T10 = 6
};

// Processing type classifications.
enum class taf_pa_ecall_proc_type_t : uint8_t
{
    LOCAL = 0,
    REMOTE
};

// Operational status values.
enum class taf_pa_ecall_operational_status_t : uint8_t
{
    UNKNOWN = 0,
    OPERATIONAL,
    UNAVAILABLE
};

// Call direction classifications.
enum class taf_pa_ecall_dir_t : uint8_t
{
    NONE = 0,
    INCOMING = 1,
    OUTGOING = 2
};

// Reason type classifications.
enum class taf_pa_ecall_reason_type_t : uint8_t
{
    NONE = 0,
    ORIG_FAILURE = 1,
    DROP = 2,
    MAX_REDIAL_ATTEMPTED = 3,
    CONNECTED = 4
};

// Vehicle type classifications.
enum class taf_pa_ecall_vehicle_type_t : uint8_t
{
    PASSENGER_VEHICLE_CLASS_M1,
    BUSES_AND_COACHES_CLASS_M2,
    BUSES_AND_COACHES_CLASS_M3,
    LIGHT_COMMERCIAL_VEHICLES_CLASS_N1,
    HEAVY_DUTY_VEHICLES_CLASS_N2,
    HEAVY_DUTY_VEHICLES_CLASS_N3,
    MOTOR_CYCLES_CLASS_L1E,
    MOTOR_CYCLES_CLASS_L2E,
    MOTOR_CYCLES_CLASS_L3E,
    MOTOR_CYCLES_CLASS_L4E,
    MOTOR_CYCLES_CLASS_L5E,
    MOTOR_CYCLES_CLASS_L6E,
    MOTOR_CYCLES_CLASS_L7E,
    TRAILERS_CLASS_O,
    AGRI_VEHICLES_CLASS_R,
    AGRI_VEHICLES_CLASS_S,
    AGRI_VEHICLES_CLASS_T,
    OFF_ROAD_VEHICLES_G,
    SPECIAL_PURPOSE_MOTOR_CARAVAN_CLASS_SA,
    SPECIAL_PURPOSE_ARMOURED_VEHICLE_CLASS_SB,
    SPECIAL_PURPOSE_AMBULANCE_CLASS_SC,
    SPECIAL_PURPOSE_HEARCE_CLASS_SD,
    OTHER_VEHICLE_CLASS
};

// Optional data type classifications.
enum class taf_pa_ecall_optional_data_type_t : uint8_t
{
    DEFAULT
};

// Configuration parameter types.
enum taf_pa_ecall_config_type_t
{
    NUM_TYPE,
    OVERRIDDEN_NUM,
    T2_TIMER,
    T7_TIMER,
    T9_TIMER,
    COUNT
};

// Configuration validity bitset type.
using taf_pa_ecall_config_validity_t = std::bitset<COUNT>;

// Configuration parameters for eCall.
struct taf_pa_ecall_config_t
{
    taf_pa_ecall_config_validity_t validityMask;
    taf_pa_ecall_num_type_t numtype;
    std::string overriddenNum;
    uint32_t t2Timer;
    uint32_t t7Timer;
    uint32_t t9Timer;
};

// Subsystem information structure.
struct taf_pa_ecall_subsystem_info_t
{
    taf_pa_ecall_proc_type_t location;
    uint32_t subsystems;
};

// HLAP timer status structure.
struct taf_pa_ecall_hlap_timer_status_t
{
    taf_pa_ecall_hlap_timer_state_t t2;
    taf_pa_ecall_hlap_timer_state_t t5;
    taf_pa_ecall_hlap_timer_state_t t6;
    taf_pa_ecall_hlap_timer_state_t t7;
    taf_pa_ecall_hlap_timer_state_t t9;
    taf_pa_ecall_hlap_timer_state_t t10;
};

// Redial information structure.
struct taf_pa_ecall_redial_info_t
{
    bool willEcallRedial;
    taf_pa_ecall_reason_type_t reason;
};

// MSD optional data structure.
struct taf_pa_ecall_msd_optional_data_t
{
    taf_pa_ecall_optional_data_type_t optionalDataType;
    bool isMsdOptionalDataPresent;
    bool recentVehicleLocationN1Present;
    bool recentVehicleLocationN2Present;
    bool numberOfPassengersPresent;
};

// MSD control bits structure.
struct taf_pa_ecall_msd_control_bits_t
{
    bool automaticActivation;
    bool testCall;
    bool positionCanBeTrusted;
    taf_pa_ecall_vehicle_type_t vehicleType;
};

// Vehicle identification number structure.
struct taf_pa_ecall_vehicle_id_number_t
{
    std::string isowmi;
    std::string isovds;
    std::string isovisModelyear;
    std::string isovisSeqPlant;
};

// Vehicle location structure.
struct taf_pa_ecall_vehicle_location_t
{
    int32_t positionLatitude;
    int32_t positionLongitude;
};

// Vehicle location delta structure.
struct taf_pa_ecall_vehicle_location_delta_t
{
    int16_t positionLatitude;
    int16_t positionLongitude;
};

// Vehicle propulsion type structure.
struct taf_pa_ecall_vehicle_propulsion_type_t
{
    bool gasolineTankPresent;
    bool dieselTankPresent;
    bool compressedNaturalGas;
    bool liquidPropaneGas;
    bool electricEnergyStorage;
    bool hydrogenStorage;
    bool otherStorage;
};

// Custom SIP header structure.
struct taf_pa_ecall_custom_sip_header_t
{
    std::string contentType;
    std::string acceptInfo;
};

// Object ID structure.
struct taf_pa_ecall_object_id_t
{
    uint8_t id1 : 4;
    uint8_t id2 : 4;
    uint16_t id3 : 14;
    uint16_t id4 : 14;
    uint16_t id5 : 14;
    uint16_t id6 : 14;
    uint16_t id7 : 14;
    uint16_t id8 : 14;
    uint16_t id9 : 14;
};

// Default options structure.
struct taf_pa_ecall_default_options_t
{
    taf_pa_ecall_object_id_t objId;
    std::string optionalData;
};

// HLAP timer events structure.
struct taf_pa_ecall_hlap_timer_events_t
{
    taf_pa_ecall_hlap_event_t t2;
    taf_pa_ecall_hlap_event_t t5;
    taf_pa_ecall_hlap_event_t t6;
    taf_pa_ecall_hlap_event_t t7;
    taf_pa_ecall_hlap_event_t t9;
    taf_pa_ecall_hlap_event_t t10;
};

// Optional PDU structure.
struct taf_pa_ecall_optional_pdu_t
{
    taf_pa_ecall_default_options_t eCallDefaultOptions;
    std::string oid;
    std::vector<uint8_t> data;
};

// Mode information structure.
struct taf_pa_ecall_mode_info_t
{
    taf_pa_ecall_mode_t mode;
};

// MSD data structure.
struct taf_pa_ecall_msd_data_t
{
    taf_pa_ecall_msd_optional_data_t optionalData;
    uint8_t messageIdentifier;
    taf_pa_ecall_msd_control_bits_t control;
    taf_pa_ecall_vehicle_id_number_t vehicleIdentification;
    uint32_t timestamp;
    taf_pa_ecall_vehicle_location_t vehicleLocation;
    taf_pa_ecall_vehicle_location_delta_t recentVehicleLocationN1;
    taf_pa_ecall_vehicle_location_delta_t recentVehicleLocationN2;
    uint8_t vehicleDirection;
    taf_pa_ecall_vehicle_propulsion_type_t propulsionType;
    taf_pa_ecall_optional_pdu_t optionalPdu;
    uint8_t numberOfPassengers;
    uint8_t msdVersion;
};

// Call information structure
struct taf_pa_ecall_CallInfo_t
{
    int8_t phoneId;
    int32_t callIndex;
    taf_pa_ecall_call_status_t callState;
    taf_pa_ecall_dir_t dir;
    std::string remotePartyNumber;
    taf_pa_ecall_termination_t endCause;
};

// Command callback type.
using taf_pa_ecall_CommandCb = std::function<void(
    pa_result_t errorCode,
    std::any context
)>;

// HLAP timer callback type.
using taf_pa_ecall_HlapTimerCb = std::function<void(
    pa_result_t errorCode,
    uint32_t duration,
    std::any context
)>;

// Get mode callback type.
using taf_pa_ecall_GetModeCb = std::function<void(
    taf_pa_ecall_mode_t mode,
    pa_result_t errorCode,
    std::any context
)>;

// Make eCall callback type - using shared_ptr for CallInfo_t
using taf_pa_ecall_MakeEcallCb = std::function<void(
    std::shared_ptr<taf_pa_ecall_CallInfo_t> callInfo,
    pa_result_t errorCode,
    std::any context
)>;

// HLAP timer status callback type - using shared_ptr for hlap_timer_status_t
using taf_pa_ecall_HlapTimerStatusCb = std::function<void(
    pa_result_t errorCode,
    int8_t phoneId,
    std::shared_ptr<const taf_pa_ecall_hlap_timer_status_t> hlapStatus,
    std::any context
)>;

// MSD transmission status callback type.
using taf_pa_ecall_MsdTransmissionStatusCb = std::function<void(
    int32_t phoneId,
    taf_pa_ecall_msd_status_t msdStatus,
    std::any context
)>;

// MSD update request callback type.
using taf_pa_ecall_MsdUpdateRequestCb = std::function<void(
    int32_t phoneId,
    std::any context
)>;

// On redial callback type - using shared_ptr for redial_info_t
using taf_pa_ecall_OnRedialCb = std::function<void(
    int32_t phoneId,
    std::shared_ptr<taf_pa_ecall_redial_info_t> redialInfo,
    std::any context
)>;

// On HLAP timer event callback type - using shared_ptr for hlap_timer_events_t
using taf_pa_ecall_OnHlapTimerEventCb = std::function<void(
    int32_t phoneId,
    std::shared_ptr<taf_pa_ecall_hlap_timer_events_t> timerEvent,
    std::any context
)>;

// Operational mode change callback type - using shared_ptr for mode_info_t
using taf_pa_ecall_OpModeChangeCb = std::function<void(
    int32_t phoneId,
    std::shared_ptr<taf_pa_ecall_mode_info_t> modeInfo,
    std::any context
)>;

// Modem state change callback type - using shared_ptr for subsystem_info_t
using taf_pa_ecall_ModemStateChangeCb = std::function<void(
    std::shared_ptr<taf_pa_ecall_subsystem_info_t> info,
    taf_pa_ecall_operational_status_t status,
    std::any context
)>;

// Event listener structure.
struct taf_pa_ecall_event_listener_t
{
    taf_pa_ecall_MakeEcallCb onIncomingCall;
    taf_pa_ecall_MakeEcallCb onCallInfoChange;
    taf_pa_ecall_MsdTransmissionStatusCb onMsdTransmissionStatus;
    taf_pa_ecall_MsdUpdateRequestCb onMsdUpdateRequest;
    taf_pa_ecall_OnRedialCb onRedial;
    taf_pa_ecall_OnHlapTimerEventCb onHlapTimerEvent;
    taf_pa_ecall_OpModeChangeCb onEcallOperatingModeChange;
    taf_pa_ecall_ModemStateChangeCb onStateChange;
};

// Initializes the eCall subsystem.
PA_SHARED pa_result_t taf_pa_ecall_Init();

PA_SHARED pa_result_t taf_pa_ecall_Deinit();

// Sets eCall configuration parameters.
PA_SHARED pa_result_t taf_pa_ecall_SetConfig(
    const taf_pa_ecall_config_t& config
);

// Gets in Progress Calls
PA_SHARED std::vector<std::shared_ptr<taf_pa_ecall_CallInfo_t>>
    taf_pa_ecall_GetInProgressCalls();

// Request Network Deregistration.
PA_SHARED pa_result_t taf_pa_ecall_RequestNetworkDeregistration(
    uint8_t phoneId,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Gets eCall configuration parameters.
PA_SHARED pa_result_t taf_pa_ecall_GetConfig(
    taf_pa_ecall_config_t& config
);

// Sets eCall operational mode.
PA_SHARED pa_result_t taf_pa_ecall_SetOpMode(
    uint8_t phoneId,
    taf_pa_ecall_mode_t mode,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Gets eCall operational mode.
PA_SHARED pa_result_t taf_pa_ecall_GetOpMode(
    uint8_t phoneId,
    taf_pa_ecall_GetModeCb callback,
    std::any context
);

// Initiates an eCall with MSD data.
PA_SHARED pa_result_t taf_pa_ecall_MakeECall(
    int phoneId,
    const taf_pa_ecall_msd_data_t& msdData,
    taf_pa_ecall_category_t category,
    taf_pa_ecall_type_t type,
    taf_pa_ecall_MakeEcallCb callback,
    std::any context
);

// Initiates an eCall with raw MSD PDU.
PA_SHARED pa_result_t taf_pa_ecall_MakeECall(
    int phoneId,
    const std::vector<uint8_t>& msdPdu,
    taf_pa_ecall_category_t category,
    taf_pa_ecall_type_t type,
    taf_pa_ecall_MakeEcallCb callback,
    std::any context
);

// Initiates an eCall with custom dial number and SIP headers.
PA_SHARED pa_result_t taf_pa_ecall_MakeECall(
    int phoneId,
    std::string dialNumber,
    const taf_pa_ecall_custom_sip_header_t& header,
    const std::vector<uint8_t>& msdPdu,
    taf_pa_ecall_MakeEcallCb callback,
    std::any context
);

// Updates MSD data during an active eCall.
PA_SHARED pa_result_t taf_pa_ecall_UpdateMsd(
    uint8_t phoneId,
    const taf_pa_ecall_msd_data_t& msdData,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Updates MSD data with raw PDU during an active eCall.
PA_SHARED pa_result_t taf_pa_ecall_UpdateMsd(
    uint8_t phoneId,
    const std::vector<uint8_t>& msdData,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Encodes MSD data to PDU format.
PA_SHARED pa_result_t taf_pa_ecall_EncodeMsd(
    const taf_pa_ecall_msd_data_t& msdData,
    std::vector<uint8_t>& msdPdu
);

// Requests HLAP timer status.
PA_SHARED pa_result_t taf_pa_ecall_RequestHlapTimerStatus(
    int phoneId,
    taf_pa_ecall_HlapTimerStatusCb callback,
    std::any context
);

// Requests specific HLAP timer information.
PA_SHARED pa_result_t taf_pa_ecall_RequestHlapTimer(
    int phoneId,
    taf_pa_ecall_hlap_timer_type_t type,
    taf_pa_ecall_HlapTimerCb callback,
    std::any context
);

// Updates HLAP timer parameters.
PA_SHARED pa_result_t taf_pa_ecall_UpdateHlapTimer(
    int phoneId,
    taf_pa_ecall_hlap_timer_type_t type,
    uint32_t duration,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Sets eCall redial parameters.
PA_SHARED pa_result_t taf_pa_ecall_SetEcallRedial(
    const std::vector<int>& timeGap,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Gets eCall redial parameters.
PA_SHARED pa_result_t taf_pa_ecall_GetEcallRedial(
    std::vector<int>& callOrigTimeGap,
    std::vector<int>& callDropTimeGap
);

// Restarts a specific HLAP timer.
PA_SHARED pa_result_t taf_pa_ecall_RestartHlapTimer(
    int phoneId,
    taf_pa_ecall_hlap_timer_id_t id,
    uint32_t duration,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Converts slot ID to phone ID.
PA_SHARED int8_t taf_pa_ecall_GetPhoneIdFromSlotId(
    int8_t slotId
);

// Hangs up an active eCall.
PA_SHARED pa_result_t taf_pa_ecall_Hangup(
    const taf_pa_ecall_CallInfo_t& callInfo,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Rejects an incoming eCall.
PA_SHARED pa_result_t taf_pa_ecall_Reject(
    const taf_pa_ecall_CallInfo_t& callInfo,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Answers an incoming eCall.
PA_SHARED pa_result_t taf_pa_ecall_Answer(
    const taf_pa_ecall_CallInfo_t& callInfo,
    taf_pa_ecall_CommandCb callback,
    std::any context
);

// Registers event listener.
PA_SHARED pa_result_t taf_pa_ecall_RegisterListener(
    const taf_pa_ecall_event_listener_t* eventListener,
    std::any context
);

} // namespace tafpa::ecall

#endif // TAF_ECALL_PA_HPP