/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * @file       tafDataPa.hpp
 * @brief      The PA data interface.
 */
#ifndef TAF_DATA_PA_HPP
#define TAF_DATA_PA_HPP

#include "tafCommonPa.h"
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <bitset>

namespace taf
{
namespace pa
{

//--------------------------------------------------------------------------------------------------
/**
 * The max length of a name string (including the terminating null character)
 */
//--------------------------------------------------------------------------------------------------
static const int MAX_NAME_LEN = 65;

namespace data
{

//--------------------------------------------------------------------------------------------------
/**
 * MTU not set indicator
 */
//--------------------------------------------------------------------------------------------------
inline constexpr int MTU_NOT_SET = -1;

//--------------------------------------------------------------------------------------------------
/**
 * Data connection indication types that can be enabled/disabled.
 */
//--------------------------------------------------------------------------------------------------
enum class DataConnectionIndicationType_e : uint8_t
{
    DEFAULT = 0,      ///< Default indications (always enabled)
    THROUGHPUT = 1    ///< Throughput indications (optional)
};

//--------------------------------------------------------------------------------------------------
/**
 * Bitset for managing data connection indications.
 * Bit 0: DEFAULT indications
 * Bit 1: THROUGHPUT indications
 */
//--------------------------------------------------------------------------------------------------
using DataConnectionIndications_t = std::bitset<32>;

//--------------------------------------------------------------------------------------------------
/**
 * The data subsystems.
 */
//--------------------------------------------------------------------------------------------------
enum class Subsystem_e
{
    PHONE_MANAGER,           ///< Phone manager.
    PROFILE_MANAGER,         ///< Profile manager.
    DATACALL_MANAGER,        ///< Data call manager.
    SERVING_SYSTEM_MANAGER   ///< Serving system manager.
};

//--------------------------------------------------------------------------------------------------
/**
 * The PA Subsystem state.
 */
//--------------------------------------------------------------------------------------------------
enum class SubsystemState_e
{
    AVAILABLE,   ///< Subsystem available.
    UNAVAILABLE, ///< Subsystem unavailable.
    FAILED       ///< Subsystem initialization failed.
};

//--------------------------------------------------------------------------------------------------
/**
 * The max number of profiles supported.
 *
 * Sync this with the max supported by DCS, TAF_DCS_PROFILE_LIST_MAX_ENTRY
 */
//--------------------------------------------------------------------------------------------------
static const int MAX_PROFILES = 32;

//--------------------------------------------------------------------------------------------------
/**
 * The max length of a profile name string (including the terminating null character)
 *
*/
//--------------------------------------------------------------------------------------------------
static const int MAX_NAME_LEN = taf::pa::MAX_NAME_LEN; //64 + 1

//--------------------------------------------------------------------------------------------------
/**
 * The APN (string) maximum length (including the terminating null character)
 */
//--------------------------------------------------------------------------------------------------
static const int MAX_APN_LEN = taf::pa::MAX_NAME_LEN;

//--------------------------------------------------------------------------------------------------
/**
 * The authorized username (string) maximum length (including the terminating null character).
 */
//--------------------------------------------------------------------------------------------------
static const int MAX_USERNAME_LEN = taf::pa::MAX_NAME_LEN;

//--------------------------------------------------------------------------------------------------
/**
 * The user's password (string) maximum length (including the terminating null character).
 */
//--------------------------------------------------------------------------------------------------
static const int MAX_PASSWORD_LEN = taf::pa::MAX_NAME_LEN;

//--------------------------------------------------------------------------------------------------
/**
 * The supported profile IDs.
 */
//--------------------------------------------------------------------------------------------------
enum class ProfileId_e : uint8_t
{
    INVALID = 0,
    ID_1 = 1,
    ID_2 = 2,
    ID_3 = 3,
    ID_4 = 4,
    ID_5 = 5,
    ID_6 = 6,
    ID_7 = 7,
    ID_8 = 8,
    ID_9 = 9,
    ID_10 = 10,
    ID_11 = 11,
    ID_12 = 12,
    ID_13 = 13,
    ID_14 = 14,
    ID_15 = 15,
    ID_16 = 16,
    ID_17 = 17,
    ID_18 = 18,
    ID_19 = 19,
    ID_20 = 20,
    ID_21 = 21,
    ID_22 = 22,
    ID_23 = 23,
    ID_24 = 24,
    ID_25 = 25,
    ID_26 = 26,
    ID_27 = 27,
    ID_28 = 28,
    ID_29 = 29,
    ID_30 = 30,
    ID_31 = 31,
    ID_32 = 32,
    ID_33 = 33,
    ID_34 = 34,
    ID_35 = 35,
    ID_36 = 36,
    ID_37 = 37,
    ID_38 = 38,
    ID_39 = 39,
    ID_40 = 40,
    ID_41 = 41,
    ID_42 = 42,
    ID_43 = 43,
    ID_44 = 44,
    ID_45 = 45,
    ID_46 = 46,
    ID_47 = 47,
    ID_48 = 48,
    ID_49 = 49,
    ID_50 = 50,
    ID_51 = 51,
    ID_52 = 52,
    ID_53 = 53,
    ID_54 = 54,
    ID_55 = 55,
    ID_56 = 56,
    ID_57 = 57,
    ID_58 = 58,
    ID_59 = 59,
    ID_60 = 60,
    ID_61 = 61,
    ID_62 = 62,
    ID_63 = 63,
    ID_64 = 64,
    ID_65 = 65,
    ID_66 = 66,
    ID_67 = 67,
    ID_68 = 68,
    ID_69 = 69,
    ID_70 = 70,
    ID_71 = 71,
    ID_72 = 72,
    ID_73 = 73,
    ID_74 = 74,
    ID_75 = 75,
    ID_76 = 76,
    ID_77 = 77,
    ID_78 = 78,
    ID_79 = 79,
    ID_80 = 80,
    ID_81 = 81,
    ID_82 = 82,
    ID_83 = 83,
    ID_84 = 84,
    ID_85 = 85,
    ID_86 = 86,
    ID_87 = 87,
    ID_88 = 88,
    ID_89 = 89,
    ID_90 = 90,
    ID_91 = 91,
    ID_92 = 92,
    ID_93 = 93,
    ID_94 = 94,
    ID_95 = 95,
    ID_96 = 96,
    ID_97 = 97,
    ID_98 = 98,
    ID_99 = 99,
    ID_100 = 100,
    ID_101 = 101,
    ID_102 = 102,
    ID_103 = 103,
    ID_104 = 104,
    ID_105 = 105,
    ID_106 = 106,
    ID_107 = 107,
    ID_108 = 108,
    ID_109 = 109,
    ID_110 = 110,
    ID_111 = 111,
    ID_112 = 112,
    ID_113 = 113,
    ID_114 = 114,
    ID_115 = 115,
    ID_116 = 116,
    ID_117 = 117,
    ID_118 = 118,
    ID_119 = 119,
    ID_120 = 120,
    ID_121 = 121,
    ID_122 = 122,
    ID_123 = 123,
    ID_124 = 124,
    ID_125 = 125,
    ID_126 = 126,
    ID_127 = 127,
    ID_128 = 128,
    ID_129 = 129,
    ID_130 = 130,
    ID_131 = 131,
    ID_132 = 132,
    ID_133 = 133,
    ID_134 = 134,
    ID_135 = 135,
    ID_136 = 136,
    ID_137 = 137,
    ID_138 = 138,
    ID_139 = 139,
    ID_140 = 140,
    ID_141 = 141,
    ID_142 = 142,
    ID_143 = 143,
    ID_144 = 144,
    ID_145 = 145,
    ID_146 = 146,
    ID_147 = 147,
    ID_148 = 148,
    ID_149 = 149,
    ID_150 = 150,
    ID_151 = 151,
    ID_152 = 152,
    ID_153 = 153,
    ID_154 = 154,
    ID_155 = 155,
    ID_156 = 156,
    ID_157 = 157,
    ID_158 = 158,
    ID_159 = 159,
    ID_160 = 160,
    ID_161 = 161,
    ID_162 = 162,
    ID_163 = 163,
    ID_164 = 164,
    ID_165 = 165,
    ID_166 = 166,
    ID_167 = 167,
    ID_168 = 168,
    ID_169 = 169,
    ID_170 = 170,
    ID_171 = 171,
    ID_172 = 172,
    ID_173 = 173,
    ID_174 = 174,
    ID_175 = 175,
    ID_176 = 176,
    ID_177 = 177,
    ID_178 = 178,
    ID_179 = 179,
    ID_180 = 180,
    ID_181 = 181,
    ID_182 = 182,
    ID_183 = 183,
    ID_184 = 184,
    ID_185 = 185,
    ID_186 = 186,
    ID_187 = 187,
    ID_188 = 188,
    ID_189 = 189,
    ID_190 = 190,
    ID_191 = 191,
    ID_192 = 192,
    ID_193 = 193,
    ID_194 = 194,
    ID_195 = 195,
    ID_196 = 196,
    ID_197 = 197,
    ID_198 = 198,
    ID_199 = 199,
    ID_200 = 200,
    ID_201 = 201,
    ID_202 = 202,
    ID_203 = 203,
    ID_204 = 204,
    ID_205 = 205,
    ID_206 = 206,
    ID_207 = 207,
    ID_208 = 208,
    ID_209 = 209,
    ID_210 = 210,
    ID_211 = 211,
    ID_212 = 212,
    ID_213 = 213,
    ID_214 = 214,
    ID_215 = 215,
    ID_216 = 216,
    ID_217 = 217,
    ID_218 = 218,
    ID_219 = 219,
    ID_220 = 220,
    ID_221 = 221,
    ID_222 = 222,
    ID_223 = 223,
    ID_224 = 224,
    ID_225 = 225,
    ID_226 = 226,
    ID_227 = 227,
    ID_228 = 228,
    ID_229 = 229,
    ID_230 = 230,
    ID_231 = 231,
    ID_232 = 232,
    ID_233 = 233,
    ID_234 = 234,
    ID_235 = 235,
    ID_236 = 236,
    ID_237 = 237,
    ID_238 = 238,
    ID_239 = 239,
    ID_240 = 240,
    ID_241 = 241,
    ID_242 = 242,
    ID_243 = 243,
    ID_244 = 244,
    ID_245 = 245,
    ID_246 = 246,
    ID_247 = 247,
    ID_248 = 248,
    ID_249 = 249,
    ID_250 = 250,
    ID_251 = 251,
    ID_252 = 252,
    ID_253 = 253,
    ID_254 = 254,
    ID_255 = 255
};

//--------------------------------------------------------------------------------------------------
/**
 * The data profile technology preference.
 */
//--------------------------------------------------------------------------------------------------
enum class TechPref_e : uint8_t
{
    TP_UNKNOWN = 0, ///< Unknown.
    TP_3GPP = 1,    ///< 3GPP.
    TP_3GPP2 = 2,   ///< 3GPP2.
    TP_ANY = 3      ///< Any.
};

//--------------------------------------------------------------------------------------------------
/**
 * The data profile authentication type.
 */
//--------------------------------------------------------------------------------------------------
enum class AuthType_e : uint8_t
{
    NONE     = 0, ///< None.
    PAP      = 1, ///< PAP.
    CHAP     = 2, ///< CHAP.
    PAP_CHAP = 3  ///< PAP and CHAP.
};

//--------------------------------------------------------------------------------------------------
/**
 * The IP family type
 */
//--------------------------------------------------------------------------------------------------
enum class IpType_e : uint8_t
{
    UNKNOWN = 0, ///< Unknown.
    IPV4    = 1, ///< IPv4.
    IPV6    = 2, ///< IPv6.
    IPV4V6  = 3  ///< IPv4 and IPv6.
};

//--------------------------------------------------------------------------------------------------
/**
 * The APN type mask.
 */
//--------------------------------------------------------------------------------------------------
enum class ApnTypeBitmask_e : uint16_t
{
    UNKNOWN   = 0x0,    ///< 0x000. No APN type.
    DEFAULT   = 0x1,    ///< 0x001. APN type for default/internet traffic.
    IMS       = 0x2,    ///< 0x002. APN type for the IP multimedia subsystem.
    MMS       = 0x4,    ///< 0x004. APN type for the multimedia messaging service.
    DUN       = 0x8,    ///< 0x008. APN type for the dial up network.
    SUPL      = 0x10,   ///< 0x010. APN type for secure user plane location.
    HIPRI     = 0x20,   ///< 0x020. APN type for high priority mobile data.
    FOTA      = 0x40,   ///< 0x040. APN type for over the air administration.
    CBS       = 0x80,   ///< 0x080. APN type for carrier branded services.
    IA        = 0x100,  ///< 0x100. APN type for initial attach.
    EMERGENCY = 0x200,  ///< 0x200. APN type for emergency.
    UT        = 0x400,  ///< 0x400. APN type for UT.
    MCX       = 0x800   ///< 0x800. APN type for mission critical service.
};

//--------------------------------------------------------------------------------------------------
/**
 */
//--------------------------------------------------------------------------------------------------
enum class EmergencyCapability_e : uint8_t
{
    UNSPECIFIED = 0, ///< Emergency capability is not specified.
    ALLOWED     = 1, ///< Emergency call is allowed on this profile.
    NOT_ALLOWED = 2  ///< Emergency call is not allowed on this profile.
};

//--------------------------------------------------------------------------------------------------
/**
 * The phones count.
 */
//--------------------------------------------------------------------------------------------------
enum class PhoneCount_e : uint8_t
{
    UNKNOWN = 0,
    ONE     = 1, ///< One phone
    TWO     = 2  ///< Two phones
};

//--------------------------------------------------------------------------------------------------
/**
 * The supported phone IDs.
 */
//--------------------------------------------------------------------------------------------------
enum class PhoneId_e : uint8_t
{
    INVALID  = 0,  ///< Invalid phone id
    PHONE_1  = 1,  ///< Phone 1 id. This is also the default slot id.
    PHONE_2  = 2   ///< Phone 2 id. This is the max slot id.
};

//--------------------------------------------------------------------------------------------------
/**
 * The SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
enum class SlotCount_e : uint8_t
{
    UNKNOWN = 0,
    ONE     = 1, ///< One slot
    TWO     = 2  ///< Two slots
};

//--------------------------------------------------------------------------------------------------
/**
 * The supported SIM Slot IDs.
 */
//--------------------------------------------------------------------------------------------------
enum class SlotId_e : uint8_t
{
    INVALID = 0,  ///< Invalid slot id
    SLOT_1  = 1,  ///< Slot 1 id. This is also the default slot id.
    SLOT_2  = 2   ///< Slot 2 id. This is the max slot id.
};

//--------------------------------------------------------------------------------------------------
/**
 * Roaming type.
 *
 */
//--------------------------------------------------------------------------------------------------
enum class RoamingType_e : uint8_t
{
    UNKNOWN       = 0,  ///< Device roaming mode is unknown
    DOMESTIC      = 1,  ///< Device is in Domestic roaming network
    INTERNATIONAL = 2   ///< Device is in International roaming network
};

//--------------------------------------------------------------------------------------------------
/**
 * The data bearer technology.
 *
 */
//--------------------------------------------------------------------------------------------------
enum class DataBearerTechnology_e
{
    BEARER_UNKNOWN,
    BEARER_CDMA_1X,       ///< 1X technology.
    BEARER_EVDO_REV0,     ///< CDMA Rev 0.
    BEARER_EVDO_REVA,     ///< CDMA Rev A.
    BEARER_EVDO_REVB,     ///< CDMA Rev B.
    BEARER_EHRPD,         ///< EHRPD.
    BEARER_FMC,           ///< Fixed mobile convergence.
    BEARER_HRPD,          ///< HRPD.
    BEARER_3GPP2_WLAN,    ///< IWLAN.
    // UMTS related data bearer technologies
    BEARER_WCDMA,          ///< WCDMA.
    BEARER_GPRS,           ///< GPRS.
    BEARER_HSDPA,          ///< HSDPA.
    BEARER_HSUPA,          ///< HSUPA.
    BEARER_EDGE,           ///< EDGE.
    BEARER_LTE,            ///< LTE.
    BEARER_HSDPA_PLUS,     ///< HSDPA+.
    BEARER_DC_HSDPA_PLUS,  ///< DC HSDPA+.
    BEARER_HSPA,           ///< HSPA.
    BEARER_64_QAM,         ///< 64 QAM.
    BEARER_TDSCDMA,        ///< TD-SCDMA.
    BEARER_GSM,            ///< GSM.
    BEARER_3GPP_WLAN,      ///< IWLAN.
    BEARER_5G              ///< 5G.
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call status
 *
 */
//--------------------------------------------------------------------------------------------------
enum class DataCallStatus_e
{
    UNKNOWN = 0,     ///<  Unknown.
    CONNECTED,       ///< Call is connected.
    DISCONNECTED,    ///< Call is disconnected.
    IDLE,            ///< Call is in idle state.
    CONNECTING,      ///< Call is in connecting state.
    DISCONNECTING,   ///< Call is in disconnecting state.
    RECONFIGURED,    ///< Interface is reconfigured, IP Address got changed.
    NEWADDR,         ///< A new IP address was added on an existing call.
    DELADDR          ///< An IP address was removed from the existing interface.
};

//--------------------------------------------------------------------------------------------------
/**
 * The call end reason type.
 */
//--------------------------------------------------------------------------------------------------
enum class CallEndReason_e
{
    CE_REASON_UNKNOWN = 0xFF,              ///< Unknown. Default type
    CE_REASON_MOBILE_IP = 0x01,            ///< Mobile IP defined.
    CE_REASON_INTERNAL = 0x02,             ///< Internal reason.
    CE_REASON_CALL_MANAGER_DEFINED = 0x03, ///< Call manager defined.
    CE_REASON_3GPP_SPEC_DEFINED = 0x06,    ///< 3GPP specification defined.
    CE_REASON_PPP = 0x07,                  ///< Point-to-Point Protocol defined.
    CE_REASON_EHRPD = 0x08,                ///< EHRPD defined.
    CE_REASON_IPV6 = 0x09,                 ///< IPv6 reason.
    CE_REASON_HANDOFF = 0x0C               ///< Handoff reason.
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_MOBILE_IP
 */
//--------------------------------------------------------------------------------------------------
enum class MobileIpReasonCode_e
{
    /*Mobile IP Call End reasons*/
    REASON_MIP_FA_ERR_REASON_UNSPECIFIED = 64,
    REASON_MIP_FA_ERR_ADMINISTRATIVELY_PROHIBITED = 65,
    REASON_MIP_FA_ERR_INSUFFICIENT_RESOURCES = 66,
    REASON_MIP_FA_ERR_MOBILE_NODE_AUTHENTICATION_FAILURE = 67,
    REASON_MIP_FA_ERR_HA_AUTHENTICATION_FAILURE = 68,
    REASON_MIP_FA_ERR_REQUESTED_LIFETIME_TOO_LONG = 69,
    REASON_MIP_FA_ERR_MALFORMED_REQUEST = 70,
    REASON_MIP_FA_ERR_MALFORMED_REPLY = 71,
    REASON_MIP_FA_ERR_ENCAPSULATION_UNAVAILABLE = 72,
    REASON_MIP_FA_ERR_VJHC_UNAVAILABLE = 73,
    REASON_MIP_FA_ERR_REVERSE_TUNNEL_UNAVAILABLE = 74,
    REASON_MIP_FA_ERR_REVERSE_TUNNEL_IS_MANDATORY_AND_T_BIT_NOT_SET = 75,
    REASON_MIP_FA_ERR_DELIVERY_STYLE_NOT_SUPPORTED = 79,
    REASON_MIP_FA_ERR_MISSING_NAI = 97,
    REASON_MIP_FA_ERR_MISSING_HA = 98,
    REASON_MIP_FA_ERR_MISSING_HOME_ADDR = 99,
    REASON_MIP_FA_ERR_UNKNOWN_CHALLENGE = 104,
    REASON_MIP_FA_ERR_MISSING_CHALLENGE = 105,
    REASON_MIP_FA_ERR_STALE_CHALLENGE = 106,
    REASON_MIP_HA_ERR_REASON_UNSPECIFIED = 128,
    REASON_MIP_HA_ERR_ADMINISTRATIVELY_PROHIBITED = 129,
    REASON_MIP_HA_ERR_INSUFFICIENT_RESOURCES = 130,
    REASON_MIP_HA_ERR_MOBILE_NODE_AUTHENTICATION_FAILURE = 131,
    REASON_MIP_HA_ERR_FA_AUTHENTICATION_FAILURE = 132,
    REASON_MIP_HA_ERR_REGISTRATION_ID_MISMATCH = 133,
    REASON_MIP_HA_ERR_MALFORMED_REQUEST = 134,
    REASON_MIP_HA_ERR_UNKNOWN_HA_ADDR = 136,
    REASON_MIP_HA_ERR_REVERSE_TUNNEL_UNAVAILABLE = 137,
    REASON_MIP_HA_ERR_REVERSE_TUNNEL_IS_MANDATORY_AND_T_BIT_NOT_SET = 138,
    REASON_MIP_HA_ERR_ENCAPSULATION_UNAVAILABLE = 139,
    REASON_MIP_ERR_REASON_UNKNOWN = -1,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_INTERNAL
 */
//--------------------------------------------------------------------------------------------------
enum class InternalReasonCode_e
{
    /*Internal Error Call End reasons*/
    REASON_INT_RETRY = 1,
    REASON_INT_ERROR = 201,
    REASON_INT_CALL_ENDED = 202,
    REASON_INT_UNKNOWN_CAUSE_CODE_203 = 203,
    REASON_INT_UNKNOWN_CAUSE_CODE_204 = 204,
    REASON_INT_CLOSE_IN_PROGRESS = 205,
    REASON_INT_NW_INITIATED_TERMINATION = 206,
    REASON_INT_APP_PREEMPTED = 207,
    REASON_INT_ERR_PDN_IPV4_CALL_DISALLOWED = 208,
    REASON_INT_ERR_PDN_IPV4_CALL_THROTTLED = 209,
    REASON_INT_ERR_PDN_IPV6_CALL_DISALLOWED = 210,
    REASON_INT_ERR_PDN_IPV6_CALL_THROTTLED = 211,
    REASON_INT_MODEM_RESTART = 212,
    REASON_INT_PDP_PPP_NOT_SUPPORTED = 213,
    REASON_INT_UNPREFERRED_RAT = 214,
    REASON_INT_PHYS_LINK_CLOSE_IN_PROGRESS = 215,
    REASON_INT_APN_PENDING_HANDOVER = 216,
    REASON_INT_PROFILE_BEARER_INCOMPATIBLE = 217,
    REASON_INT_MMGSDI_CARD_EVT = 218,
    REASON_INT_LPM_OR_PWR_DOWN = 219,
    REASON_INT_APN_DISABLED = 220,
    REASON_INT_MPIT_EXPIRED = 221,
    REASON_INT_IPV6_ADDR_TRANSFER_FAILED = 222,
    REASON_INT_TRAT_SWAP_FAILED = 223,
    REASON_INT_EHRPD_TO_HRPD_FALLBACK = 224,
    REASON_INT_MANDATORY_APN_DISABLED = 225,
    REASON_MIP_CONFIG_FAILURE = 226,
    REASON_INT_INTERNAL_PDN_INACTIVITY_TIMER_EXPIRED = 227,
    REASON_INT_MAX_V4_CONNECTIONS = 228,
    REASON_INT_MAX_V6_CONNECTIONS = 229,
    REASON_INT_APN_MISMATCH = 230,
    REASON_INT_IP_VERSION_MISMATCH = 231,
    REASON_INT_DUN_CALL_DISALLOWED = 232,
    REASON_INT_INVALID_PROFILE = 233,
    REASON_INT_INTERNAL_EPC_NONEPC_TRANSITION = 234,
    REASON_INT_INVALID_PROFILE_ID = 235,
    REASON_INT_CALL_ALREADY_PRESENT = 236,
    REASON_INT_IFAREASON_INT_IN_USE = 237,
    REASON_INT_IP_PDP_MISMATCH = 238,
    REASON_INT_APN_DISALLOWED_ON_ROAMING = 239,
    REASON_INT_APN_PARAM_CHANGE = 240,
    REASON_INT_IFAREASON_INT_IN_USE_CFG_MATCH = 241,
    REASON_INT_NULL_APN_DISALLOWED = 242,
    REASON_INT_THERMAL_MITIGATION = 243,
    REASON_INT_SUBS_ID_MISMATCH = 244,
    REASON_INT_DATA_SETTINGS_DISABLED = 245,
    REASON_INT_DATA_ROAMING_SETTINGS_DISABLED = 246,
    REASON_INT_APN_FORMAT_INVALID = 247,
    REASON_INT_DDS_CALL_ABORT = 248,
    REASON_INT_VALIDATION_FAILURE = 249,
    REASON_INT_PROFILES_NOT_COMPATIBLE = 251,
    REASON_INT_NULL_RESOLVED_APN_NO_MATCH = 252,
    REASON_INT_INVALID_APN_NAME = 253,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_CALL_MANAGER_DEFINED
 */
//--------------------------------------------------------------------------------------------------
enum class CallManagerReasonCode_e
{
    /*CM defined Call End reasons*/
    REASON_CM_CDMA_LOCK = 500,
    REASON_CM_INTERCEPT = 501,
    REASON_CM_REORDER = 502,
    REASON_CM_REL_SO_REJ = 503,
    REASON_CM_INCOM_CALL = 504,
    REASON_CM_ALERT_STOP = 505,
    REASON_CM_ACTIVATION = 506,
    REASON_CM_MAX_ACCESS_PROBE = 507,
    REASON_CM_CCS_NOT_SUPPORTED_BY_BS = 508,
    REASON_CM_NO_RESPONSE_FROM_BS = 509,
    REASON_CM_REJECTED_BY_BS = 510,
    REASON_CM_INCOMPATIBLE = 511,
    REASON_CM_ALREADY_IN_TC = 512,
    REASON_CM_USER_CALL_ORIG_DURING_GPS = 513,
    REASON_CM_USER_CALL_ORIG_DURING_SMS = 514,
    REASON_CM_NO_CDMA_SRV = 515,
    REASON_CM_MC_ABORT = 516,
    REASON_CM_PSIST_NG = 517,
    REASON_CM_UIM_NOT_PRESENT = 518,
    REASON_CM_RETRY_ORDER = 519,
    REASON_CM_ACCESS_BLOCK = 520,
    REASON_CM_CEACCESS_BLOCK_ALL = 521,
    REASON_CM_IS707B_MAX_ACC = 522,
    REASON_CM_THERMAL_EMERGENCY = 523,
    REASON_CM_CALL_ORIG_THROTTLED = 524,
    REASON_CM_USER_CALL_ORIG_DURING_VOIREASON_CM_CALL = 535,
    REASON_CM_CONF_FAILED = 1000,
    REASON_CM_INCOM_REJ = 1001,
    REASON_CM_NEW_NO_GW_SRV = 1002,
    REASON_CM_NEW_NO_GPRS_CONTEXT = 1003,
    REASON_CM_NEW_ILLEGAL_MS = 1004,
    REASON_CM_NEW_ILLEGAL_ME = 1005,
    REASON_CM_NEW_GPRS_SERVICES_AND_NON_GPRS_SERVICES_NOT_ALLOWED = 1006,
    REASON_CM_NEW_GPRS_SERVICES_NOT_ALLOWED = 1007,
    REASON_CM_NEW_MS_IDENTITY_CANNOT_BE_DERIVED_BY_THE_NETWORK = 1008,
    REASON_CM_NEW_IMPLICITLY_DETACHED = 1009,
    REASON_CM_NEW_PLMN_NOT_ALLOWED = 1010,
    REASON_CM_NEW_LA_NOT_ALLOWED = 1011,
    REASON_CM_NEW_GPRS_SERVICES_NOT_ALLOWED_IN_THIS_PLMN = 1012,
    REASON_CM_NEW_PDP_DUPLICATE = 1013,
    REASON_CM_NEW_UE_RAT_CHANGE = 1014,
    REASON_CM_NEW_CONGESTION = 1015,
    REASON_CM_NEW_NO_PDP_CONTEXT_ACTIVATED = 1016,
    REASON_CM_NEW_ACCESS_CLASS_DSAC_REJECTION = 1017,
    REASON_CM_PDP_ACTIVATE_MAX_RETRY_FAILED = 1018,
    REASON_CM_RAB_FAILURE = 1019,
    REASON_CM_ESM_UNKNOWN_EPS_BEARER_CONTEXT = 1025,
    REASON_CM_DRB_RELEASED_AT_RRC = 1026,
    REASON_CM_NAS_SIG_CONN_RELEASED = 1027,
    REASON_CM_REASON_EMM_DETACHED = 1028,
    REASON_CM_EMM_ATTACH_FAILED = 1029,
    REASON_CM_EMM_ATTACH_STARTED = 1030,
    REASON_CM_LTE_NAS_SERVIREASON_CM_REQ_FAILED = 1031,
    REASON_CM_ESM_ACTIVE_DEDICATED_BEARER_REACTIVATED_BY_NW = 1032,
    REASON_CM_ESM_LOWER_LAYER_FAILURE = 1033,
    REASON_CM_ESM_SYNC_UP_WITH_NW = 1034,
    REASON_CM_ESM_NW_ACTIVATED_DED_BEARER_WITH_ID_OF_DEF_BEARER = 1035,
    REASON_CM_ESM_BAD_OTA_MESSAGE = 1036,
    REASON_CM_ESM_DS_REJECTED_THE_CALL = 1037,
    REASON_CM_ESM_CONTEXT_TRANSFERED_DUE_TO_IRAT = 1038,
    REASON_CM_DS_EXPLICIT_DEACT = 1039,
    REASON_CM_ESM_LOCAL_CAUSE_NONE = 1040,
    REASON_CM_LTE_NAS_SERVIREASON_CM_REQ_FAILED_NO_THROTTLE = 1041,
    REASON_CM_ACL_FAILURE = 1042,
    REASON_CM_LTE_NAS_SERVIREASON_CM_REQ_FAILED_DS_DISALLOW = 1043,
    REASON_CM_EMM_T3417_EXPIRED = 1044,
    REASON_CM_EMM_T3417_EXT_EXPIRED = 1045,
    REASON_CM_LRRC_UL_DATA_CNF_FAILURE_TXN = 1046,
    REASON_CM_LRRC_UL_DATA_CNF_FAILURE_HO = 1047,
    REASON_CM_LRRC_UL_DATA_CNF_FAILURE_CONN_REL = 1048,
    REASON_CM_LRRC_UL_DATA_CNF_FAILURE_RLF = 1049,
    REASON_CM_LRRC_UL_DATA_CNF_FAILURE_CTRL_NOT_CONN = 1050,
    REASON_CM_LRRC_CONN_EST_FAILURE = 1051,
    REASON_CM_LRRC_CONN_EST_FAILURE_ABORTED = 1052,
    REASON_CM_LRRC_CONN_EST_FAILURE_ACCESS_BARRED = 1053,
    REASON_CM_LRRC_CONN_EST_FAILURE_CELL_RESEL = 1054,
    REASON_CM_LRRC_CONN_EST_FAILURE_CONFIG_FAILURE = 1055,
    REASON_CM_LRRC_CONN_EST_FAILURE_TIMER_EXPIRED = 1056,
    REASON_CM_LRRC_CONN_EST_FAILURE_LINK_FAILURE = 1057,
    REASON_CM_LRRC_CONN_EST_FAILURE_NOT_CAMPED = 1058,
    REASON_CM_LRRC_CONN_EST_FAILURE_SI_FAILURE = 1059,
    REASON_CM_LRRC_CONN_EST_FAILURE_CONN_REJECT = 1060,
    REASON_CM_LRRC_CONN_REL_NORMAL = 1061,
    REASON_CM_LRRC_CONN_REL_RLF = 1062,
    REASON_CM_LRRC_CONN_REL_CRE_FAILURE = 1063,
    REASON_CM_LRRC_CONN_REL_OOS_DURING_CRE = 1064,
    REASON_CM_LRRC_CONN_REL_ABORTED = 1065,
    REASON_CM_LRRC_CONN_REL_SIB_READ_ERROR = 1066,
    REASON_CM_DETACH_WITH_REATTACH_LTE_NW_DETACH = 1067,
    REASON_CM_DETACH_WITH_OUT_REATTACH_LTE_NW_DETACH = 1068,
    REASON_CM_ESM_PROC_TIME_OUT = 1069,
    REASON_CM_INVALID_CONNECTION_ID = 1070,
    REASON_CM_INVALID_NSAPI = 1071,
    REASON_CM_INVALID_PRI_NSAPI = 1072,
    REASON_CM_INVALID_FIELD = 1073,
    REASON_CM_RAB_SETUP_FAILURE = 1074,
    REASON_CM_PDP_ESTABLISH_MAX_TIMEOUT = 1075,
    REASON_CM_PDP_MODIFY_MAX_TIMEOUT = 1076,
    REASON_CM_PDP_INACTIVE_MAX_TIMEOUT = 1077,
    REASON_CM_PDP_LOWERLAYER_ERROR = 1078,
    REASON_CM_PPD_UNKNOWN_REASON = 1079,
    REASON_CM_PDP_MODIFY_COLLISION = 1080,
    REASON_CM_PDP_MBMS_REQUEST_COLLISION = 1081,
    REASON_CM_MBMS_DUPLICATE = 1082,
    REASON_CM_SM_PS_DETACHED = 1083,
    REASON_CM_SM_NO_RADIO_AVAILABLE = 1084,
    REASON_CM_SM_ABORT_SERVIREASON_CM_NOT_AVAILABLE = 1085,
    REASON_CM_MESSAGE_EXCEED_MAX_L2_LIMIT = 1086,
    REASON_CM_SM_NAS_SRV_REQ_FAILURE = 1087,
    REASON_CM_RRC_CONN_EST_FAILURE_REQ_ERROR = 1088,
    REASON_CM_RRC_CONN_EST_FAILURE_TAI_CHANGE = 1089,
    REASON_CM_RRC_CONN_EST_FAILURE_RF_UNAVAILABLE = 1090,
    REASON_CM_RRC_CONN_REL_ABORTED_IRAT_SUCCESS = 1091,
    REASON_CM_RRC_CONN_REL_RLF_SEC_NOT_ACTIVE = 1092,
    REASON_CM_RRC_CONN_REL_IRAT_TO_LTE_ABORTED = 1093,
    REASON_CM_RRC_CONN_REL_IRAT_FROM_LTE_TO_G_CCO_SUCCESS = 1094,
    REASON_CM_RRC_CONN_REL_IRAT_FROM_LTE_TO_G_CCO_ABORTED = 1095,
    REASON_CM_IMSI_UNKNOWN_IN_HSS = 1096,
    REASON_CM_IMEI_NOT_ACCEPTED = 1097,
    REASON_CM_EPS_SERVICES_AND_NON_EPS_SERVICES_NOT_ALLOWED = 1098,
    REASON_CM_EPS_SERVICES_NOT_ALLOWED_IN_PLMN = 1099,
    REASON_CM_MSC_TEMPORARILY_NOT_REACHABLE = 1100,
    REASON_CM_CS_DOMAIN_NOT_AVAILABLE = 1101,
    REASON_CM_ESM_FAILURE = 1102,
    REASON_CM_MAC_FAILURE = 1103,
    REASON_CM_SYNCH_FAILURE = 1104,
    REASON_CM_UE_SECURITY_CAPABILITIES_MISMATCH = 1105,
    REASON_CM_SECURITY_MODE_REJ_UNSPECIFIED = 1106,
    REASON_CM_NON_EPS_AUTH_UNACCEPTABLE = 1107,
    REASON_CM_CS_FALLBACK_CALL_EST_NOT_ALLOWED = 1108,
    REASON_CM_NO_EPS_BEARER_CONTEXT_ACTIVATED = 1109,
    REASON_CM_EMM_INVALID_STATE = 1110,
    REASON_CM_NAS_LAYER_FAILURE = 1111,
    REASON_CM_MULTI_PDN_NOT_ALLOWED = 1112,
    REASON_CM_EMBMS_NOT_ENABLED = 1113,
    REASON_CM_PENDING_REDIAL_CALL_CLEANUP = 1114,
    REASON_CM_EMBMS_REGULAR_DEACTIVATION = 1115,
    REASON_CM_TLB_REGULAR_DEACTIVATION = 1116,
    REASON_CM_LOWER_LAYER_REGISTRATION_FAILURE = 1117,
    REASON_CM_DETACH_EPS_SERVICES_NOT_ALLOWED = 1118,
    REASON_CM_SM_INTERNAL_PDP_DEACTIVATION = 1119,
    REASON_CM_UNSUPPORTED_1X_PREV = 1515,
    REASON_CM_CD_GEN_OR_BUSY = 1500,
    REASON_CM_CD_BILL_OR_AUTH = 1501,
    REASON_CM_CHG_HDR = 1502,
    REASON_CM_EXIT_HDR = 1503,
    REASON_CM_HDR_NO_SESSION = 1504,
    REASON_CM_HDR_ORIG_DURING_GPS_FIX = 1505,
    REASON_CM_HDR_CS_TIMEOUT = 1506,
    REASON_CM_HDR_RELEASED_BY_CM = 1507,
    REASON_CM_COLLOC_ACQ_FAIL = 1508,
    REASON_CM_OTASP_COMMIT_IN_PROG = 1509,
    REASON_CM_NO_HYBR_HDR_SRV = 1510,
    REASON_CM_HDR_NO_LOCK_GRANTED = 1511,
    REASON_CM_HOLD_OTHER_IN_PROG = 1512,
    REASON_CM_HDR_FADE = 1513,
    REASON_CM_HDR_ACC_FAIL = 1514,
    REASON_CM_CLIENT_END = 2000,
    REASON_CM_NO_SRV = 2001,
    REASON_CM_FADE = 2002,
    REASON_CM_REL_NORMAL = 2003,
    REASON_CM_ACC_IN_PROG = 2004,
    REASON_CM_ACC_FAIL = 2005,
    REASON_CM_REDIR_OR_HANDOFF = 2006,
    REASON_CM_CM_UNKNOWN_ERROR = 2007,
    REASON_CM_OFFLINE = 2500,
    REASON_CM_EMERGENCY_MODE = 2501,
    REASON_CM_PHONE_IN_USE = 2502,
    REASON_CM_INVALID_MODE = 2503,
    REASON_CM_INVALID_SIM_STATE = 2504,
    REASON_CM_NO_COLLOC_HDR = 2505,
    REASON_CM_CALL_CONTROL_REJECTED = 2506,
    REASON_CM_UNKNOWN = -1,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_3GPP_SPEC_DEFINED
 */
//--------------------------------------------------------------------------------------------------
enum class SpecReasonCode_e
{
    /*3GPP spec defined Call End reasons*/
    REASON_3GPP_OPERATOR_DETERMINED_BARRING = 8,
    REASON_3GPP_NAS_SIGNALLING_ERROR = 14,
    REASON_3GPP_LLC_SNDCP_FAILURE = 25,
    REASON_3GPP_INSUFFICIENT_RESOURCES = 26,
    REASON_3GPP_UNKNOWN_APN = 27,
    REASON_3GPP_UNKNOWN_PDP = 28,
    REASON_3GPP_AUTH_FAILED = 29,
    REASON_3GPP_GGSN_REJECT = 30,
    REASON_3GPP_ACTIVATION_REJECT = 31,
    REASON_3GPP_OPTION_NOT_SUPPORTED = 32,
    REASON_3GPP_OPTION_UNSUBSCRIBED = 33,
    REASON_3GPP_OPTION_TEMP_OOO = 34,
    REASON_3GPP_NSAPI_ALREADY_USED = 35,
    REASON_3GPP_REGULAR_DEACTIVATION = 36,
    REASON_3GPP_QOS_NOT_ACCEPTED = 37,
    REASON_3GPP_NETWORK_FAILURE = 38,
    REASON_3GPP_UMTS_REACTIVATION_REQ = 39,
    REASON_3GPP_FEATURE_NOT_SUPPORTED = 40,
    REASON_3GPP_TFT_SEMANTIC_ERROR = 41,
    REASON_3GPP_TFT_SYNTAX_ERROR = 42,
    REASON_3GPP_UNKNOWN_PDP_CONTEXT = 43,
    REASON_3GPP_FILTER_SEMANTIC_ERROR = 44,
    REASON_3GPP_FILTER_SYNTAX_ERROR = 45,
    REASON_3GPP_PDP_WITHOUT_ACTIVE_TFT = 46,
    REASON_3GPP_IP_V4_ONLY_ALLOWED = 50,
    REASON_3GPP_IP_V6_ONLY_ALLOWED = 51,
    REASON_3GPP_SINGLE_ADDR_BEARER_ONLY = 52,
    REASON_3GPP_ESM_INFO_NOT_RECEIVED = 53,
    REASON_3GPP_PDN_CONN_DOES_NOT_EXIST = 54,
    REASON_3GPP_MULTI_CONN_TO_SAME_PDN_NOT_ALLOWED = 55,
    REASON_3GPP_MAX_ACTIVE_PDP_CONTEXT_REACHED = 65,
    REASON_3GPP_UNSUPPORTED_APN_IN_CURRENT_PLMN = 66,
    REASON_3GPP_INVALID_TRANSACTION_ID = 81,
    REASON_3GPP_MESSAGE_INCORRECT_SEMANTIC = 95,
    REASON_3GPP_INVALID_MANDATORY_INFO = 96,
    REASON_3GPP_MESSAGE_TYPE_UNSUPPORTED = 97,
    REASON_3GPP_MSG_TYPE_NONCOMPATIBLE_STATE = 98,
    REASON_3GPP_UNKNOWN_INFO_ELEMENT = 99,
    REASON_3GPP_CONDITIONAL_IE_ERROR = 100,
    REASON_3GPP_MSG_AND_PROTOCOL_STATE_UNCOMPATIBLE = 101,
    REASON_3GPP_PROTOCOL_ERROR = 111,
    REASON_3GPP_APN_TYPE_CONFLICT = 112,
    REASON_3GPP_INVALID_PCSCF_ADDRESS = 113,
    REASON_3GPP_INTERNAL_CALL_PREEMPT_BY_HIGH_PRIO_APN = 114,
    REASON_3GPP_EMM_ACCESS_BARRED = 115,
    REASON_3GPP_EMERGENCY_IFAREASON_3GPP_ONLY = 116,
    REASON_3GPP_IFAREASON_3GPP_MISMATCH = 117,
    REASON_3GPP_COMPANION_IFAREASON_3GPP_IN_USE = 118,
    REASON_3GPP_IP_ADDRESS_MISMATCH = 119,
    REASON_3GPP_IFAREASON_3GPP_AND_POL_FAMILY_MISMATCH = 120,
    REASON_3GPP_EMM_ACCESS_BARRED_INFINITE_RETRY = 121,
    REASON_3GPP_AUTH_FAILURE_ON_EMERGENCY_CALL = 122,
    REASON_3GPP_INVALID_DNS_ADDR = 123,
    REASON_3GPP_INVALID_PCSCF_DNS_ADDR = 124,
    REASON_3GPP_TEST_LOOPBACK_MODE_A_OR_B_ENABLED = 125,
    REASON_3GPP_UNKNOWN = -1,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_PPP
 */
//--------------------------------------------------------------------------------------------------
enum class PPPReasonCode_e
{
    /*Enumeration for the PPP verbose call end reason*/
    REASON_PPP_TIMEOUT = 1,
    REASON_PPP_AUTH_FAILURE = 2,
    REASON_PPP_OPTION_MISMATCH = 3,
    REASON_PPP_PAP_FAILURE = 31,
    REASON_PPP_CHAP_FAILURE = 32,
    REASON_PPP_CLOSE_IN_PROGRESS = 33,
    REASON_PPP_NV_REFRESH_IN_PROGRESS = 34,
    REASON_PPP_UNKNOWN = -1,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_EHRPD
 */
//--------------------------------------------------------------------------------------------------
enum class EHRPDReasonCode_e
{
    /* Enumeration for the EHRPD verbose call end reason */
    REASON_EHRPD_SUBS_LIMITED_TO_V4 = 1,
    REASON_EHRPD_SUBS_LIMITED_TO_V6 = 2,
    REASON_EHRPD_VSNCP_TIMEOUT = 4,
    REASON_EHRPD_VSNCP_FAILURE = 5,
    REASON_EHRPD_VSNCP_3GPP2I_GEN_ERROR = 6,
    REASON_EHRPD_VSNCP_3GPP2I_UNAUTH_APN = 7,
    REASON_EHRPD_VSNCP_3GPP2I_PDN_LIMIT_EXCEED = 8,
    REASON_EHRPD_VSNCP_3GPP2I_NO_PDN_GW = 9,
    REASON_EHRPD_VSNCP_3GPP2I_PDN_GW_UNREACH = 10,
    REASON_EHRPD_VSNCP_3GPP2I_PDN_GW_REJ = 11,
    REASON_EHRPD_VSNCP_3GPP2I_INSUFF_PARAM = 12,
    REASON_EHRPD_VSNCP_3GPP2I_RESOURCE_UNAVAIL = 13,
    REASON_EHRPD_VSNCP_3GPP2I_ADMIN_PROHIBIT = 14,
    REASON_EHRPD_VSNCP_3GPP2I_PDN_ID_IN_USE = 15,
    REASON_EHRPD_VSNCP_3GPP2I_SUBSCR_LIMITATION = 16,
    REASON_EHRPD_VSNCP_3GPP2I_PDN_EXISTS_FOR_THIS_APN = 17,
    REASON_EHRPD_VSNCP_3GPP2I_RECONNECT_NOT_ALLOWED = 19,
    REASON_EHRPD_UNKNOWN = -1,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_IPV6
 */
//--------------------------------------------------------------------------------------------------
enum class Ipv6ReasonCode_e
{
    /*IPV6 defined Call End reasons*/
    REASON_PREFIX_UNAVAILABLE = 1,
    REASON_IPV6_ERR_HRPD_IPV6_DISABLED = 2,
    REASON_IPV6_DISABLED = 3,
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call end/termination reason code for CallEndReason_e::CE_REASON_HANDOFF
 */
//--------------------------------------------------------------------------------------------------
enum class HandoffReasonCode_e
{
    /*Hand off Call End reasons*/
    REASON_VCER_HANDOFF_PREF_SYS_BACK_TO_SRAT = 1,
};

//--------------------------------------------------------------------------------------------------
/**
 * The QoS flow state event.
 */
//--------------------------------------------------------------------------------------------------
enum class QosFlowState_e
{
    UNKNOWN   = -1,  ///< UNKNOWN state.
    ACTIVATED = 0,   ///< Flow activated.
    MODIFIED  = 1,   ///< Flow modified.
    DELETED   = 2,   ///< Flow deleted.
};

//--------------------------------------------------------------------------------------------------
/**
 * Hardware acceleration state.
 */
//--------------------------------------------------------------------------------------------------
enum class HwAccelerationState_e
{
    INACTIVE = 0,
    ACTIVE   = 1
};

//--------------------------------------------------------------------------------------------------
/**
 * The QoS flow parameters mask.
 */
//--------------------------------------------------------------------------------------------------
enum class QosFlowMaskValues_e
{
    NONE       = 0, ///< No parameters set.
    TX_GRANTED = 1, ///< TX Granted flow set.
    RX_GRANTED = 2, ///< RX Granted flow set.
    TX_FILTERS = 3, ///< TX filters set.
    RX_FILTERS = 4, ///< RX filters set.
};

//--------------------------------------------------------------------------------------------------
/**
 * The profile event type.
 */
//--------------------------------------------------------------------------------------------------
enum class ProfileEvent_e
{
    UNKNOWN  = 0, ///< Unknown event.
    CREATED  = 1, ///< A profile has been created.
    DELETED  = 2, ///< A profile has been deleted.
    MODIFIED = 3  ///< A profile has been modified.
};

//--------------------------------------------------------------------------------------------------
/**
 * The 5G N79 and WLAN 5G band interference priority.
 */
//--------------------------------------------------------------------------------------------------
enum class BandIntPriority_e
{
    UNKNOWN = 0,  ///< Unknown band priority.
    N79_5G  = 1,  ///< The 5G N79 band.
    WLAN_5G = 2   ///< The Wifi 5 GHz band.
};

//--------------------------------------------------------------------------------------------------
/**
 * 32 bit mask that denotes which of the flow paramaters are available. The mask is defined in
 * QosFlowMaskValues_e enum.
 */
//--------------------------------------------------------------------------------------------------
using QosFlowMask_e = std::bitset<32>;

//--------------------------------------------------------------------------------------------------
/**
 * The call end reason strcture with reason and specific reason code
 *
 */
//--------------------------------------------------------------------------------------------------
struct DataCallEndReason_t
{
    CallEndReason_e reason = CallEndReason_e::CE_REASON_UNKNOWN;
    union
    {
        MobileIpReasonCode_e    mipCode;
        InternalReasonCode_e    internalCode;
        CallManagerReasonCode_e cmCode;
        SpecReasonCode_e        specCode;
        PPPReasonCode_e         pppCode;
        EHRPDReasonCode_e       ehrpdCode;
        Ipv6ReasonCode_e        ipv6Code;
        HandoffReasonCode_e     handOffCode;
    };
};

//--------------------------------------------------------------------------------------------------
/**
 * Roaming status.
 *
 */
//--------------------------------------------------------------------------------------------------
struct RoamingStatus_t
{
    PhoneId_e     phoneId;    ///< The phone id.
    SlotId_e      slotId;     ///< The slot id.
    bool          isRoaming;  ///< The roaming status.
    RoamingType_e type;       ///< The roaming type. Valid only if isRoaming is true.
};

//--------------------------------------------------------------------------------------------------
/**
 * Profile information that is shared between the service and the PA.
 *
 * When creating/updating profiles, all parameters will be considered.
 *
 * When deleting a profile, only phoneId and profileId will be considered.
 *
 */
//--------------------------------------------------------------------------------------------------
struct ProfileInfo_t
{
    ProfileId_e profileId{ProfileId_e::INVALID};             ///< The profile id.
    char apn[MAX_APN_LEN]{};                                 ///< The access point name.
    char name[MAX_NAME_LEN]{};                               ///< The profile name.
    char userName[MAX_USERNAME_LEN]{};                       ///< The user name.
    char password[MAX_PASSWORD_LEN]{};                       ///< The password.
    TechPref_e techPref{TechPref_e::TP_UNKNOWN};             ///< The technology preference.
    AuthType_e authType{AuthType_e::NONE};                   ///< The authentication type.
    IpType_e ipType{IpType_e::IPV4};                         ///< The IP type.
    ApnTypeBitmask_e apnTypeMask{ApnTypeBitmask_e::UNKNOWN}; ///< The APN type bitmask.
    EmergencyCapability_e emergencyCallSupport{EmergencyCapability_e::UNSPECIFIED};
                                                             ///< Emergency call support.
};

//--------------------------------------------------------------------------------------------------
/**
 * Structure that contains parameters to use with StartDataSessionAsync and StopDataSessionAsync.
 */
//--------------------------------------------------------------------------------------------------
struct DataCallStartStopParams_t
{
    PhoneId_e   phoneId;           ///< The phone id.
    ProfileId_e profileId;         ///< The profile id.
    IpType_e    ipType;            ///< The IP type.
    std::string interfaceName="";  ///< Optional.The interface name to start the data call with.
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call information including IP addresses, interface name and status specific to an IP type.
 */
//--------------------------------------------------------------------------------------------------
struct DataCallInfo_t
{
    DataCallStatus_e    callStatus;       ///< The data call status.
    DataCallEndReason_t callEndReason;    ///< The call end reason
    std::string         ipAddr;           ///< The IP address.
    unsigned int        ipAddrMask = 0;   ///< The IP address.
    std::string         gwAddr;           ///< The gateway address.
    unsigned int        gwAddrMask = 0;   ///< The gateway address mask.
    std::string         dnsAddrPrimary;   ///< The primary DNS address.
    std::string         dnsAddrSecondary; ///< The secondary DNS address.
    int32_t             mtu = taf::pa::data::MTU_NOT_SET; ///< The MTU size in bytes.
};

//--------------------------------------------------------------------------------------------------
/**
 * Data call events that are provided back to the service.
 * Use the phone Id and profile Id to uniquely identify a data session.
 */
//--------------------------------------------------------------------------------------------------
struct DataCallEventInfo_t
{
    PhoneId_e             phoneId;              ///< The phone id.
    ProfileId_e           profileId;            ///< The profile id.
    SlotId_e              slotId;               ///< The slot id.
    DataCallStatus_e      callStatus;           ///< The data call status.
    DataCallEndReason_t   callEndReason;        ///< The call end reason.
    IpType_e              ipType;               ///< The IP type.
    TechPref_e            techPref;             ///< The technology preference.
    DataBearerTechnology_e bearerTech;          ///< The bearer technology.
    std::string            hostIfName = "";     ///< The hist interface name.
    DataCallInfo_t         ipv4DataCallInfo;    ///< The IPv4 call information.
    DataCallInfo_t         ipv6DataCallInfo;    ///< The IPv6 call information.
    uint64_t               maxRxBitRate;        ///< The maximum receive data rate in bits/second.
    uint64_t               maxTxBitRate;        ///< The maximum transmit data rate in bits/second.
};

//--------------------------------------------------------------------------------------------------
/**
 * APN throttled events.
 */
//--------------------------------------------------------------------------------------------------
struct ThrottledApnEventInfo_t
{
    PhoneId_e                phoneId;     ///< The phone id.
    std::string              apn;         ///< APN.
    std::vector<ProfileId_e> profileIds;  ///< Profile IDs with the same APN.
    uint32_t                 ipv4Time;    ///< Remaining IPv4 throttled time in milliseconds.
    uint32_t                 ipv6Time;    ///< Remaining IPv6 throttled time in milliseconds.
    bool                     isBlockedOnAllPLMNs;   ///< Is APN blocked on all plmns.
    std::string              mcc;         ///< Mobile Country Code.
    std::string              mnc;         ///< Mobile Network Code.
};

//--------------------------------------------------------------------------------------------------
/**
 * The QoS flow info.
 */
//--------------------------------------------------------------------------------------------------
struct QosTft_t
{
    uint32_t        qosFlowId;      ///< The QoS flow id.
    QosFlowState_e  state;          ///< The QoS flow state.
    QosFlowMask_e   paramMask;      ///< The mask to check for valid parameters in the flow.
    /**
     * Other parameters are optional and will be added later.
     */
};

//--------------------------------------------------------------------------------------------------
/**
 * The QoS flow event.
 */
//--------------------------------------------------------------------------------------------------
struct QosTftEventInfo_t
{
    PhoneId_e             phoneId;        ///< The phone id.
    ProfileId_e           profileId;      ///< The profile id.
    std::vector<QosTft_t> qosFlows;       ///< The QoS flows.
};

//--------------------------------------------------------------------------------------------------
/**
 * The HW acceleration state change event.
 */
//--------------------------------------------------------------------------------------------------
struct HwAccelerationChangeEvent_t
{
    PhoneId_e             phoneId;        ///< The phone id.
    HwAccelerationState_e state;          ///< The HW acceleration state.
};

//--------------------------------------------------------------------------------------------------
/**
 * Uplink throughput information for a data profile.
 */
//--------------------------------------------------------------------------------------------------
struct UplinkThroughputInfo_t
{
    uint32_t throughput = 0;        ///< Current uplink throughput in kbps.
    uint32_t maxThroughput = 0;     ///< Maximum allowed uplink throughput in kbps.
    uint32_t queueSize = 0;         ///< Number of bytes pending in the uplink queue.
};

//--------------------------------------------------------------------------------------------------
/**
 * Downlink throughput information for a data profile.
 */
//--------------------------------------------------------------------------------------------------
struct DownlinkThroughputInfo_t
{
    uint32_t throughput = 0;        ///< Current downlink throughput in kbps.
};

//--------------------------------------------------------------------------------------------------
/**
 * Combined uplink and downlink throughput information for a data profile.
 *
 * This structure contains comprehensive throughput metrics for a specific data profile,
 * including both uplink and downlink information. It is used in periodic throughput
 * reports and on-demand queries.
 */
//--------------------------------------------------------------------------------------------------
struct ThroughputInfo_t
{
    PhoneId_e                 phoneId{PhoneId_e::INVALID};    ///< The phone ID.
    SlotId_e                  slotId{SlotId_e::INVALID};      ///< The slot ID.
    ProfileId_e               profileId{ProfileId_e::INVALID};///< The profile ID.
    UplinkThroughputInfo_t    ulThroughput;                   ///< Uplink throughput details.
    DownlinkThroughputInfo_t  dlThroughput;                   ///< Downlink throughput details.
};


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Telux data PA state.
 **
 * Initialize the Telux data PA state.

 * @return
 *  - LE_OK              PA completely initialized
 *  - LE_UNAVAILABLE     PA not completely initialized. A part of the PA maybe usable. Check state.
 *  - LE_FAULT           PA is not usable due to fatal failure.
 *  - LE_NOT_IMPLEMENTED API is not implemented.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t Init
(
    taf::pa::data::SubsystemState_e &state
        ///< [OUT] The Telux data PA initialization state.
);

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize the Telux data PA state.
 *
 * @return
 *  - LE_OK              PA completely initialized
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t Deinit
(

);

//--------------------------------------------------------------------------------------------------
/**
 * Get the data PA subsystem state.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetSubsystemState
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone ID.
    taf::pa::data::Subsystem_e subsystem,
    ///< [IN] The subsystem.
    taf::pa::data::SubsystemState_e &state
    ///< [OUT] The subsystem initialization state.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the phone Ids.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetPhoneIds
(
    std::vector<PhoneId_e> &phoneIds
        ///< [OUT] The phone IDs.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetSimSlotCount
(
    SlotCount_e &slotCount
        ///< [OUT] The number of SIM slots.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetPhoneIdFromSimSlotId
(
    SlotId_e slotID,
        ///< [IN] The SIM slot ID.
    PhoneId_e& phoneID
        ///< [OUT] The phone ID.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetSimSlotIdFromPhoneId
(
    PhoneId_e phoneID,
        ///< [IN] The phone ID.
    SlotId_e& slotID
        ///< [OUT] The SIM slot ID.
);

//--------------------------------------------------------------------------------------------------
/**
 * The callbackfunction taf::pa::data::GetProfilesAsync
 *
 *
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_profile_GetAllAsyncCb =
    std::function<void
        (
            PhoneId_e                         phoneId,      ///< [IN] The phone id.
            pa_result_t                       result,       ///< [IN] The result of the operation.
            const std::vector<ProfileInfo_t>& profiles,     ///< [IN] The profile list.
            void                              *contextPtr   ///< [IN] The context pointer.
        )>;

//--------------------------------------------------------------------------------------------------
/**
 * Get profiles from the NAD for the specified slot ID.
 *
 * Use context to distinguish between different slots if the same handler is used.
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetProfilesAsync
(
    PhoneId_e phone,
    ///< [IN] The phone id.
    taf_pa_data_profile_GetAllAsyncCb callback,
    ///< [IN] The callback function.
    void* contextPtr
    ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Get details of the specified profile.
 *
 * Set ProfileInfo_t::profileId to the desired profile ID.
 * Set ProfileInfo_t::techPref to 3GPP or 3GPP2. If unspecified, 3GPP will be used.
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetProfileInfo
(
    PhoneId_e phoneId,
    ///< [IN] The phone id.
    ProfileInfo_t &profileInfo
    ///< [IN/OUT] The profile information.
);

//--------------------------------------------------------------------------------------------------
/**
 * Create a profile
 *
 * On success, the created profile ID will be available.
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t CreateProfile
(
    PhoneId_e phone,
    ///< [IN] The phone id.
    ProfileInfo_t profileInfo,
    ///< [IN] The profile information.
    ProfileId_e &profileId
    ///< [OUT] The profile id on success.
);

//--------------------------------------------------------------------------------------------------
/**
 * Update a profile
 *
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t UpdateProfile
(
    PhoneId_e phone,
    ///< [IN] The phone id.
    ProfileInfo_t profileInfo
    ///< [IN] The profile information.
);

//--------------------------------------------------------------------------------------------------
/**
 * Update a profile
 *
 * Only  profile ID is considered in the structure.
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t DeleteProfile
(
    PhoneId_e phone,
    ///< [IN] The phone id.
    ProfileInfo_t profileInfo
    ///< [IN] The profile information.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the default profile
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetDefaultProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The profile information.
    taf::pa::data::ProfileId_e &profileId
    ///< [OUT] The default profile ID.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the default profile
 *
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t SetDefaultProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The profile information.
    taf::pa::data::ProfileId_e profileId
    ///< [IN] The default profile ID.
);

//--------------------------------------------------------------------------------------------------
/**
 * The data call events callback.
 * @param [in] dataCallEventInfo   The data call info.
 * @param [in] context             The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_CallEventsCb =
    std::function<void
                    (
                        const DataCallEventInfo_t &dataCallEventInfo,
                        std::shared_ptr<void> context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register for data call events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddDataCallEventsCallback
(
    taf_pa_data_CallEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered data call events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveDataCallEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Start a data session. Events will be provided via taf_pa_data_CallEventsCb that is registered
 * via AddDataCallEventsCallback()
 *
 * @return LE_OK on success. Wait for callback for final status.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t StartDataSessionAsync
(
    const DataCallStartStopParams_t& params
                ///< [IN] The data call parameters.
);

//--------------------------------------------------------------------------------------------------
/**
 * Stop a data session. Events will be provided via taf_pa_data_CallEventsCb that is registered
 * via RegisterDataCallEventsCallback()
 *
 * @return LE_OK on success. Wait for callback for final status.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t StopDataSessionAsync
(
    const DataCallStartStopParams_t &params
                ///< [IN] The data call parameters.
);

//--------------------------------------------------------------------------------------------------
/**
 * The data call events callback.
 * @param [in] dataCallEventInfo   The data call info.
 * @param [in] context             The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_RequestCallListCb =
    std::function<void
    (
        pa_result_t                             result,   ///< [IN] The result of the operation.
        const std::vector<DataCallEventInfo_t>& callList, ///< [IN] The data calls list.
        std::shared_ptr<void>                   context   ///< [IN] The app provided context pointer
    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Request list of all active data calls. Events will be provided via taf_pa_data_RequestCallListCb.
 *
 * @return LE_OK on success. Wait for callback for final status.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RequestDataCallsListAsync
(
    PhoneId_e phoneId,
                ///< [IN] The phone ID.
    taf_pa_data_RequestCallListCb callBack,
                ///< [IN] The callback function.
    std::shared_ptr<void> context
                ///< [IN] The context pointer.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get throttled APNs information.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetThrottledApnInfo
(
    const taf::pa::data::PhoneId_e phoneId,
        ///< [IN] The phone ID.
    std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
        ///< [OUT] The list of throttled APNs info.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get roaming status.
 *
 * @return LE_OK on success.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetRoamingStatus
(
    const taf::pa::data::PhoneId_e phoneId,
    RoamingStatus_t &roamingStatus
);

//--------------------------------------------------------------------------------------------------
/**
 * The data subsystems state callback.
 * @param [in] subsystem          The phone Id. Set to INVALID if not applicable for the subsytem.
 * @param [in] subsystem          The subsystem.
 * @param [in] subsystemState     The subsystem state.
 * @param [in] context            The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_SubsystemStateChangeCb =
    std::function<void
                    (
                        PhoneId_e             phoneId,
                        Subsystem_e           subsystem,
                        SubsystemState_e      subsystemState,
                        std::shared_ptr<void> context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register for subsystem state change events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddSubsystemStateChangeCallback
(
    taf_pa_data_SubsystemStateChangeCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered subsystem state change callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveSubsystemStateChangeCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * The roaming status events callback.
 * @param [in] roamingEventInfo   The roaming status info.
 * @param [in] context            The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_RoamingEventsCb =
    std::function<void
                    (
                        const RoamingStatus_t &roamingEventInfo,
                        std::shared_ptr<void> context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register roaming events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddRoamingEventsCallback
(
    taf_pa_data_RoamingEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered roaming events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveRoamingEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * The throttled APN events callback.
 * @param [in] throttledApnEventInfo   The list of throttled APNs info.
 * @param [in] context                 The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_ThrottledApnEventsCb =
    std::function<void
                    (
                        const std::vector<ThrottledApnEventInfo_t> &throttledApnEventsList,
                        std::shared_ptr<void> context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register throttled APN events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddThrottledApnEventsCallback
(
    taf_pa_data_ThrottledApnEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered throttled APN events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveThrottledApnEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * The QoS TFT events callback.
 * @param [in] qosTftEventInfo   The QoS TFT event.
 * @param [in] context           The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_QosTftEventsCb =
    std::function<void
                    (
                        const QosTftEventInfo_t &qosTftEventInfo,
                        std::shared_ptr<void>    context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register QoS TFT events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddQosTftEventsCallback
(
    taf_pa_data_QosTftEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered QoS TFT events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveQosTftEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * The HW acceleration change events callback.
 * @param [in] hwAccelerationEventInfo   The HW acceleration change event.
 * @param [in] context                   The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_HwAccelerationEventsCb =
    std::function<void
                    (
                        const HwAccelerationChangeEvent_t &hwAccelerationEventInfo,
                        std::shared_ptr<void>              context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register HW acceleration change events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddHwAccelerationChangeEventsCallback
(
    taf_pa_data_HwAccelerationEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered HW acceleration change events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveHwAccelerationChangeEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * The profile events callback.
 * @param [in] PhoneId_e          The phone ID.
 * @param [in] ProfileEvent_e     The profile event.
 * @param [in] profileInfo        The profile information(including the profile ID).
 * @param [in] context            The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_ProfileEventsCb =
    std::function<void
                    (
                        PhoneId_e               phoneId,        ///< The phone ID.
                        ProfileEvent_e          event,          ///< The profile event.
                        const ProfileInfo_t    &profileInfo,    ///< The profile information.
                        std::shared_ptr<void>   context
                    )>;

//--------------------------------------------------------------------------------------------------
/**
 * The throughput events callback.
 * @param [in] throughputInfoList  The list of throughput information for all active profiles.
 * @param [in] context             The app provided context.
 */
//--------------------------------------------------------------------------------------------------
using taf_pa_data_ThroughputEventsCb =
    std::function<void
        (
            const std::vector<ThroughputInfo_t> &throughputInfoList,
            std::shared_ptr<void> context
        )>;

//--------------------------------------------------------------------------------------------------
/**
 * Register profile change events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddProfileEventsCallback
(
    taf_pa_data_ProfileEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered profile  events callback
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveProfileEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

//--------------------------------------------------------------------------------------------------
/**
 * Register SDK callbacks. This is typically not needed as the callbacks will be registered during
 * initialization.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RegisterSDKCallbacks
(

);

//--------------------------------------------------------------------------------------------------
/**
 * Deregister SDK callbacks. This is to support the service manage suspend/resume scenarios.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t DeregisterSDKCallbacks
(

);

//--------------------------------------------------------------------------------------------------
/**
 * Set the throughput report interval.
 *
 * Configure the interval for generating periodic uplink and downlink throughput reports.
 * Reports are delivered via taf_pa_data_ThroughputEventsCb registered through
 * AddThroughputEventsCallback().
 *
 * The minimum allowed interval is 50 ms. Passing a value of 0 disables throughput reporting.
 * The application of this interval is a global setting.
 *
 * @param [in] phoneId          The phone ID.
 * @param [in] reportInterval   Interval in milliseconds (ms) for throughput reporting.
 *                              Minimum: 50ms, 0 to disable.
 *
 * @return
 *  - PA_OK              Successfully set the interval
 *  - PA_BAD_PARAMETER   Invalid parameters
 *  - PA_FAULT           Failed to set the interval
 *  - PA_TIMEOUT         Operation timed out
 *  - PA_NOT_IMPLEMENTED API is not implemented
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t SetThroughputReportInterval
(
    PhoneId_e phoneId,
    uint32_t reportInterval
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the last throughput information for all active profiles.
 *
 * Retrieves the most recent uplink and downlink throughput information for all active data
 * profiles on the specified phone.
 *
 * @param [in]  phoneId              The phone ID.
 * @param [out] throughputInfoList   The list of throughput information for all active profiles.
 *
 * @return
 *  - PA_OK              Successfully retrieved throughput information
 *  - PA_BAD_PARAMETER   Invalid parameters
 *  - PA_FAULT           Failed to retrieve throughput information
 *  - PA_TIMEOUT         Operation timed out
 *  - PA_NOT_IMPLEMENTED API is not implemented
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetLastThroughputInfo
(
    PhoneId_e phoneId,
    std::vector<ThroughputInfo_t> &throughputInfoList
);

//--------------------------------------------------------------------------------------------------
/**
 * Register throughput events callback.
 *
 * Register a callback to receive periodic throughput information updates. The callback will be
 * invoked at the interval configured via SetThroughputReportInterval().
 *
 * @param [in]  callBack   The callback function to register.
 * @param [in]  context    The context pointer passed to callback.
 * @param [out] id         The unique ID assigned to this callback.
 *
 * @return
 *  - PA_OK              Successfully registered callback
 *  - PA_BAD_PARAMETER   Invalid parameters (e.g., null callback)
 *  - PA_FAULT           Failed to register with TelSDK
 *  - PA_NOT_IMPLEMENTED API is not implemented
 *
 * @note When this is the first throughput callback:
 *       - Registers THROUGHPUT indication with TelSDK
 *       - Starts receiving periodic throughput events
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t AddThroughputEventsCallback
(
    taf_pa_data_ThroughputEventsCb callBack,
    std::shared_ptr<void> context,
    uint16_t &id
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove a previously registered throughput events callback.
 *
 * Unregister a callback that was previously registered via AddThroughputEventsCallback().
 *
 * @param [in] id  The ID of the callback to remove.
 *
 * @return
 *  - PA_OK              Successfully removed callback
 *  - PA_NOT_FOUND       Callback with specified ID not found
 *  - PA_FAULT           Failed to deregister from TelSDK
 *  - PA_NOT_IMPLEMENTED API is not implemented
 *
 * @note If this is the last throughput callback:
 *       - Deregisters THROUGHPUT indication from TelSDK
 *       - Stops receiving periodic throughput events
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t RemoveThroughputEventsCallback
(
    uint16_t id
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the MTU (Maximum Transmission Unit) for a network interface.
 *
 * This function retrieves the MTU for a network interface identified by its name.
 *
 * @param interfaceName The network interface name (e.g., "rmnet_data0").
 * @param mtu Output parameter for the MTU value in bytes.
 * @return PA_OK on success, error code otherwise.
 *         PA_BAD_PARAMETER if interfaceName is invalid or empty.
 *         PA_NOT_FOUND if the interface does not exist.
 *         PA_FAULT if MTU retrieval fails.
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t GetMtu
(
    const std::string& interfaceName,
    int32_t& mtu
);

} //data
} //pa
} //taf

#endif //TAF_DATA_PA_HPP
