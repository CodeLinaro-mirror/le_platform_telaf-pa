/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_Utils.cpp
 * @brief The TelAF Data PA utility functions.
 *
 */
#include "tafDataUtilsPa.hpp"
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <cstring>

using namespace taf::pa::data;

// Conversion functions for SlotId
SlotId_e Utils::ConvertSlotId(SlotId slotId)
{
    switch (slotId)
    {
    case SLOT_ID_1:
        return SlotId_e::SLOT_1;
    case SLOT_ID_2:
        return SlotId_e::SLOT_2;
    case INVALID_SLOT_ID:
    default:
        PA_WARN("Invalid slot ID: %d", TO_INT(slotId));
        return SlotId_e::INVALID;
    }
}

SlotId Utils::ConvertSlotId(SlotId_e slotId)
{
    switch (slotId)
    {
    case SlotId_e::SLOT_1:
        return SLOT_ID_1;
    case SlotId_e::SLOT_2:
        return SLOT_ID_2;
    case SlotId_e::INVALID:
    default:
        PA_WARN("Invalid slot ID: %d", TO_INT(slotId));
        return INVALID_SLOT_ID;
    }
}

// Conversion functions for RoamingType
telux::data::RoamingType Utils::ConvertRoamingType
(
    RoamingType_e roamingType
)
{
    switch (roamingType)
    {
    case RoamingType_e::DOMESTIC:
        return telux::data::RoamingType::DOMESTIC;
    case RoamingType_e::INTERNATIONAL:
        return telux::data::RoamingType::INTERNATIONAL;
    case RoamingType_e::UNKNOWN:
    default:
        PA_WARN("Invalid roaming type: %d", TO_INT(roamingType));
        return telux::data::RoamingType::UNKNOWN;
    }
}

RoamingType_e Utils::ConvertRoamingType
(
    telux::data::RoamingType roamingType
)
{
    switch (roamingType)
    {
    case telux::data::RoamingType::DOMESTIC:
        return RoamingType_e::DOMESTIC;
    case telux::data::RoamingType::INTERNATIONAL:
        return RoamingType_e::INTERNATIONAL;
    case telux::data::RoamingType::UNKNOWN:
    default:
        PA_WARN("Invalid roaming type: %d", TO_INT(roamingType));
        return RoamingType_e::UNKNOWN;
    }
}

ProfileId_e Utils::ConvertProfileId(int id)
{
    if (IS_VALUE_IN_RANGE ( id,
                          TO_INT(ProfileId_e::ID_1),
                          TO_INT(ProfileId_e::ID_255))
                        )
    {
        PA_DEBUG("Id: %d", id);
        return static_cast<ProfileId_e>(id);
    }
    else
    {
        PA_WARN ("Id out of range: %d", id);
        return ProfileId_e::INVALID;
    }
}

TechPref_e Utils::ConvertTechPref (telux::data::TechPreference techPref)
{
    using namespace taf::pa::data;
    switch (techPref)
    {
    case telux::data::TechPreference::TP_3GPP:
        return TechPref_e::TP_3GPP;
    case telux::data::TechPreference::TP_3GPP2:
        return TechPref_e::TP_3GPP2;
    case telux::data::TechPreference::TP_ANY:
        return TechPref_e::TP_ANY;
    default:
        PA_WARN("Invalid tech pref: %d", TO_INT(techPref));
        return TechPref_e::TP_UNKNOWN;
    }
}

telux::data::TechPreference Utils::ConvertTechPref
                                                    (TechPref_e techPref)
{
    using namespace taf::pa::data;
    switch (techPref)
    {
    case TechPref_e::TP_3GPP:
        return telux::data::TechPreference::TP_3GPP;
    case TechPref_e::TP_3GPP2:
        return telux::data::TechPreference::TP_3GPP2;
    case TechPref_e::TP_ANY:
        return telux::data::TechPreference::TP_ANY;
    default:
        PA_WARN("Invalid tech pref: %d", TO_INT(techPref));
        return telux::data::TechPreference::UNKNOWN;
    }
}

AuthType_e Utils::ConvertAuthType (telux::data::AuthProtocolType authType)
{
    using namespace taf::pa::data;
    switch (authType)
    {
    case telux::data::AuthProtocolType::AUTH_NONE:
        return AuthType_e::NONE;
    case telux::data::AuthProtocolType::AUTH_PAP:
        return AuthType_e::PAP;
    case telux::data::AuthProtocolType::AUTH_CHAP:
        return AuthType_e::CHAP;
    case telux::data::AuthProtocolType::AUTH_PAP_CHAP:
        return AuthType_e::PAP_CHAP;
    default:
        PA_WARN("Invalid auth type: %d", TO_INT(authType));
        return AuthType_e::NONE;
    }
}

telux::data::AuthProtocolType Utils::ConvertAuthType(AuthType_e authType)
{
    using namespace taf::pa::data;
    switch (authType)
    {
    case AuthType_e::NONE:
        return telux::data::AuthProtocolType::AUTH_NONE;
    case AuthType_e::PAP:
        return telux::data::AuthProtocolType::AUTH_PAP;
    case AuthType_e::CHAP:
        return telux::data::AuthProtocolType::AUTH_CHAP;
    case AuthType_e::PAP_CHAP:
        return telux::data::AuthProtocolType::AUTH_PAP_CHAP;
    default:
        PA_WARN("Invalid auth type: %d", TO_INT(authType));
        return telux::data::AuthProtocolType::AUTH_NONE;
    }
}

IpType_e Utils::ConvertIpType(telux::data::IpFamilyType ipType)
{
    using namespace taf::pa::data;
    switch (ipType)
    {
    case telux::data::IpFamilyType::IPV4:
        return IpType_e::IPV4;
    case telux::data::IpFamilyType::IPV6:
        return IpType_e::IPV6;
    case telux::data::IpFamilyType::IPV4V6:
        return IpType_e::IPV4V6;
    default:
        PA_WARN("Invalid IP type: %d", TO_INT(ipType));
        return IpType_e::UNKNOWN;
    }
}

telux::data::IpFamilyType Utils::ConvertIpType (IpType_e ipType)
{
    using namespace taf::pa::data;
    switch (ipType)
    {
    case IpType_e::IPV4:
        return telux::data::IpFamilyType::IPV4;
    case IpType_e::IPV6:
        return telux::data::IpFamilyType::IPV6;
    case IpType_e::IPV4V6:
        return telux::data::IpFamilyType::IPV4V6;
    default:
        PA_WARN("Invalid IP type: %d", TO_INT(ipType));
        return telux::data::IpFamilyType::UNKNOWN;
    }
}


EmergencyCapability_e Utils::ConvertEmerCallCap
(
    telux::data::EmergencyCapability emerCallCapa
)
{
    using namespace taf::pa::data;
    switch (emerCallCapa)
    {
    case telux::data::EmergencyCapability::ALLOWED:
        return EmergencyCapability_e::ALLOWED;
    case telux::data::EmergencyCapability::NOT_ALLOWED:
        return EmergencyCapability_e::NOT_ALLOWED;
    default:
        PA_WARN("Invalid emergency call capability: %d", TO_INT(emerCallCapa));
        return EmergencyCapability_e::UNSPECIFIED;
    }
}

telux::data::EmergencyCapability Utils::ConvertEmerCallCap
(
    EmergencyCapability_e emerCallCapa
)
{
    using namespace taf::pa::data;
    switch (emerCallCapa)
    {
    case EmergencyCapability_e::ALLOWED:
        return telux::data::EmergencyCapability::ALLOWED;
    case EmergencyCapability_e::NOT_ALLOWED:
        return telux::data::EmergencyCapability::NOT_ALLOWED;
    default:
        PA_WARN("Invalid emergency call capability: %d", TO_INT(emerCallCapa));
        return telux::data::EmergencyCapability::NOT_ALLOWED;
    }
}

ApnTypeBitmask_e Utils::ConvertApnTypeMask
(
    telux::data::ApnTypes apnTypesMask
)
{
    using namespace taf::pa::data;
    return static_cast<ApnTypeBitmask_e>(apnTypesMask.to_ulong());
}

telux::data::ApnTypes Utils::ConvertApnTypeMask
(
    ApnTypeBitmask_e apnTypesMask
)
{
    return telux::data::ApnTypes(static_cast<uint16_t>(apnTypesMask));
}

telux::data::DataCallStatus Utils::ConvertCallStatus
(
    DataCallStatus_e status
)
{
    using namespace taf::pa::data;
    switch (status)
    {
        case DataCallStatus_e::CONNECTED:
            return telux::data::DataCallStatus::NET_CONNECTED;
        case DataCallStatus_e::DISCONNECTED:
            return telux::data::DataCallStatus::NET_NO_NET;
        case DataCallStatus_e::IDLE:
            return telux::data::DataCallStatus::NET_IDLE;
        case DataCallStatus_e::CONNECTING:
            return telux::data::DataCallStatus::NET_CONNECTING;
        case DataCallStatus_e::DISCONNECTING:
            return telux::data::DataCallStatus::NET_DISCONNECTING;
        case DataCallStatus_e::RECONFIGURED:
            return telux::data::DataCallStatus::NET_RECONFIGURED;
        case DataCallStatus_e::NEWADDR:
            return telux::data::DataCallStatus::NET_NEWADDR;
        case DataCallStatus_e::DELADDR:
            return telux::data::DataCallStatus::NET_DELADDR;
        default:
            PA_WARN("Invalid data call status: %d", TO_INT(status));
            return telux::data::DataCallStatus::INVALID;
    }
}

DataCallStatus_e Utils::ConvertCallStatus
(
    telux::data::DataCallStatus status
)
{
    using namespace taf::pa::data;
    switch (status)
    {
        case telux::data::DataCallStatus::NET_CONNECTED:
            return DataCallStatus_e::CONNECTED;
        case telux::data::DataCallStatus::NET_NO_NET:
            return DataCallStatus_e::DISCONNECTED;
        case telux::data::DataCallStatus::NET_IDLE:
            return DataCallStatus_e::IDLE;
        case telux::data::DataCallStatus::NET_CONNECTING:
            return DataCallStatus_e::CONNECTING;
        case telux::data::DataCallStatus::NET_DISCONNECTING:
            return DataCallStatus_e::DISCONNECTING;
        case telux::data::DataCallStatus::NET_RECONFIGURED:
            return DataCallStatus_e::RECONFIGURED;
        case telux::data::DataCallStatus::NET_NEWADDR:
            return DataCallStatus_e::NEWADDR;
        case telux::data::DataCallStatus::NET_DELADDR:
            return DataCallStatus_e::DELADDR;
        default:
            PA_WARN("Invalid data call status: %d", TO_INT(status));
            return DataCallStatus_e::UNKNOWN;
    }
}

pa_result_t Utils::ConvertProfileInfo
(
    telux::data::DataProfile &dataProfile, //IN
    ProfileInfo_t   &profileInfo           //OUT
)
{
    PA_DEBUG("Profile details:");
    using namespace taf::pa::data;

    profileInfo.profileId = ConvertProfileId(dataProfile.getId());
    PA_DEBUG(" Id          : %d", TO_INT(profileInfo.profileId));

    int iRet = 0;
    if (!dataProfile.getApn().empty())
    {
        iRet = std::snprintf(profileInfo.apn, MAX_APN_LEN, "%s", dataProfile.getApn().c_str());
        if (iRet < 0 || iRet >= MAX_APN_LEN)
        {
            // Add a warning as this is not fatal, and continue processing.
            PA_WARN("APN truncated or encoding error");
        }
        PA_DEBUG(" APN         : %s", profileInfo.apn);
    }

    if (!dataProfile.getName().empty())
    {
        iRet = std::snprintf(profileInfo.name, MAX_NAME_LEN, "%s", dataProfile.getName().c_str());
        if (iRet < 0 || iRet >= MAX_NAME_LEN)
        {
            // Add a warning as this is not fatal, and continue processing.
            PA_WARN("Name truncated or encoding error");
        }
        PA_DEBUG(" Name        : %s", profileInfo.name);
    }

    if (!dataProfile.getUserName().empty())
    {
        iRet = std::snprintf(profileInfo.userName, MAX_USERNAME_LEN, "%s",
                                                                dataProfile.getUserName().c_str());
        if (iRet < 0 || iRet >= MAX_USERNAME_LEN)
        {
            // Add a warning as this is not fatal, and continue processing.
            PA_WARN("Username truncated or encoding error");
        }
        PA_DEBUG(" Username    : %s", profileInfo.userName);
    }

    if (!dataProfile.getPassword().empty())
    {
        iRet = std::snprintf(profileInfo.password, MAX_PASSWORD_LEN, "%s",
                                                                dataProfile.getPassword().c_str());
        if (iRet < 0 || iRet >= MAX_PASSWORD_LEN)
        {
            // Add a warning as this is not fatal, and continue processing.
            PA_WARN("Password truncated or encoding error");
        }
        PA_DEBUG(" Password    : %s", profileInfo.password);
    }

    profileInfo.techPref = ConvertTechPref(dataProfile.getTechPreference());
    PA_DEBUG(" TechPref    : %d", TO_INT(profileInfo.techPref));

    profileInfo.authType = ConvertAuthType(dataProfile.getAuthProtocolType());
    PA_DEBUG(" AuthType    : %d", TO_INT(profileInfo.authType));

    profileInfo.ipType = ConvertIpType(dataProfile.getIpFamilyType());
    PA_DEBUG(" IPType      : %d", TO_INT(profileInfo.ipType));

    profileInfo.apnTypeMask = ConvertApnTypeMask(dataProfile.getApnTypes());
    PA_DEBUG(" TelSDK mask : %ld", dataProfile.getApnTypes().to_ulong());
    PA_DEBUG(" APNType mask: %d", TO_INT(profileInfo.apnTypeMask));

    profileInfo.emergencyCallSupport = ConvertEmerCallCap(dataProfile.getIsEmergencyAllowed());
    PA_DEBUG(" Emer call   : %d", TO_INT(profileInfo.emergencyCallSupport));

    return PA_OK;
}

const char *Utils::CallStatusToString(telux::data::DataCallStatus status)
{
    switch (status)
    {
    case telux::data::DataCallStatus::INVALID:
        return "INVALID";
    case telux::data::DataCallStatus::NET_CONNECTED:
        return "NET_CONNECTED";
    case telux::data::DataCallStatus::NET_NO_NET:
        return "NET_NO_NET";
    case telux::data::DataCallStatus::NET_IDLE:
        return "NET_IDLE";
    case telux::data::DataCallStatus::NET_CONNECTING:
        return "NET_CONNECTING";
    case telux::data::DataCallStatus::NET_DISCONNECTING:
        return "NET_DISCONNECTING";
    case telux::data::DataCallStatus::NET_RECONFIGURED:
        return "NET_RECONFIGURED";
    case telux::data::DataCallStatus::NET_NEWADDR:
        return "NET_NEWADDR";
    case telux::data::DataCallStatus::NET_DELADDR:
        return "NET_DELADDR";
    default:
        PA_WARN("call(%d) status error", static_cast<int32_t>(status));
        return "INVALID";
    }
}

const char *Utils::IpFamilyTypeToString(telux::data::IpFamilyType ipType)
{
    switch (ipType)
    {
    case telux::data::IpFamilyType::IPV4:
        return "IPv4";
    case telux::data::IpFamilyType::IPV6:
        return "IPv6";
    case telux::data::IpFamilyType::IPV4V6:
        return "IPv4v6";
    case telux::data::IpFamilyType::UNKNOWN:
    default:
        PA_WARN("unknown ip: %d", static_cast<int32_t>(ipType));
        return "UNKNOWN";
    }
}

const char *Utils::TechPreferenceToString(telux::data::TechPreference techPref)
{
    switch (techPref)
    {
    case telux::data::TechPreference::TP_3GPP:
        return "3GPP";
    case telux::data::TechPreference::TP_3GPP2:
        return "3GPP2";
    case telux::data::TechPreference::TP_ANY:
        return "TP_ANY";
    case telux::data::TechPreference::UNKNOWN:
    default:
        PA_WARN("unknown tech preference(%d)", static_cast<int32_t>(techPref));
        return "UNKNOWN";
    }
}

const char *Utils::DataBearerToString(telux::data::DataBearerTechnology dataBearer)
{
    switch (dataBearer)
    {
    case telux::data::DataBearerTechnology::CDMA_1X:
        return "1X technology";
    case telux::data::DataBearerTechnology::EVDO_REV0:
        return "CDMA Rev 0";
    case telux::data::DataBearerTechnology::EVDO_REVA:
        return "CDMA Rev A";
    case telux::data::DataBearerTechnology::EVDO_REVB:
        return "CDMA Rev B";
    case telux::data::DataBearerTechnology::EHRPD:
        return "EHRPD";
    case telux::data::DataBearerTechnology::FMC:
        return "Fixed mobile convergence";
    case telux::data::DataBearerTechnology::HRPD:
        return "HRPD";
    case telux::data::DataBearerTechnology::BEARER_TECH_3GPP2_WLAN:
        return "3GPP2 IWLAN";
    case telux::data::DataBearerTechnology::WCDMA:
        return "WCDMA";
    case telux::data::DataBearerTechnology::GPRS:
        return "GPRS";
    case telux::data::DataBearerTechnology::HSDPA:
        return "HSDPA";
    case telux::data::DataBearerTechnology::HSUPA:
        return "HSUPA";
    case telux::data::DataBearerTechnology::EDGE:
        return "EDGE";
    case telux::data::DataBearerTechnology::LTE:
        return "LTE";
    case telux::data::DataBearerTechnology::HSDPA_PLUS:
        return "HSDPA+";
    case telux::data::DataBearerTechnology::DC_HSDPA_PLUS:
        return "DC HSDPA+.";
    case telux::data::DataBearerTechnology::HSPA:
        return "HSPA";
    case telux::data::DataBearerTechnology::BEARER_TECH_64_QAM:
        return "64 QAM";
    case telux::data::DataBearerTechnology::TDSCDMA:
        return "TDSCDMA";
    case telux::data::DataBearerTechnology::GSM:
        return "GSM";
    case telux::data::DataBearerTechnology::BEARER_TECH_3GPP_WLAN:
        return "3GPP WLAN";
    case telux::data::DataBearerTechnology::BEARER_TECH_5G:
        return "5G";
    default:
        PA_WARN("unknown data bearer type(%d)", static_cast<int32_t>(dataBearer));
        return "UNKNOWN";
    }
}

const char *Utils::CallEndReasonTypeToString(telux::common::EndReasonType endReasontype)
{
    switch (endReasontype)
    {
    case telux::common::EndReasonType::CE_MOBILE_IP:
        return "CE_MOBILE_IP";
    case telux::common::EndReasonType::CE_INTERNAL:
        return "CE_INTERNAL";
    case telux::common::EndReasonType::CE_CALL_MANAGER_DEFINED:
        return "CE_CALL_MANAGER_DEFINED";
    case telux::common::EndReasonType::CE_3GPP_SPEC_DEFINED:
        return "CE_3GPP_SPEC_DEFINED";
    case telux::common::EndReasonType::CE_PPP:
        return "CE_PPP";
    case telux::common::EndReasonType::CE_EHRPD:
        return "CE_EHRPD";
    case telux::common::EndReasonType::CE_IPV6:
        return "CE_IPV6";
    case telux::common::EndReasonType::CE_UNKNOWN:
        return "CE_UNKNOWN";
    default:
        PA_WARN("end reason(%d) error", static_cast<int32_t>(endReasontype));
        return "CE_UNKNOWN";
    }
}

DataBearerTechnology_e Utils::ConvertBearerTech
(
    telux::data::DataBearerTechnology bearerTech
)
{
    switch (bearerTech)
    {
    case telux::data::DataBearerTechnology::CDMA_1X:
        return DataBearerTechnology_e::BEARER_CDMA_1X;
    case telux::data::DataBearerTechnology::EVDO_REV0:
        return DataBearerTechnology_e::BEARER_EVDO_REV0;
    case telux::data::DataBearerTechnology::EVDO_REVA:
        return DataBearerTechnology_e::BEARER_EVDO_REVA;
    case telux::data::DataBearerTechnology::EVDO_REVB:
        return DataBearerTechnology_e::BEARER_EVDO_REVB;
    case telux::data::DataBearerTechnology::EHRPD:
        return DataBearerTechnology_e::BEARER_EHRPD;
    case telux::data::DataBearerTechnology::FMC:
        return DataBearerTechnology_e::BEARER_FMC;
    case telux::data::DataBearerTechnology::HRPD:
        return DataBearerTechnology_e::BEARER_HRPD;
    case telux::data::DataBearerTechnology::BEARER_TECH_3GPP2_WLAN:
        return DataBearerTechnology_e::BEARER_3GPP2_WLAN;
    case telux::data::DataBearerTechnology::WCDMA:
        return DataBearerTechnology_e::BEARER_WCDMA;
    case telux::data::DataBearerTechnology::GPRS:
        return DataBearerTechnology_e::BEARER_GPRS;
    case telux::data::DataBearerTechnology::HSDPA:
        return DataBearerTechnology_e::BEARER_HSDPA;
    case telux::data::DataBearerTechnology::HSUPA:
        return DataBearerTechnology_e::BEARER_HSUPA;
    case telux::data::DataBearerTechnology::EDGE:
        return DataBearerTechnology_e::BEARER_EDGE;
    case telux::data::DataBearerTechnology::LTE:
        return DataBearerTechnology_e::BEARER_LTE;
    case telux::data::DataBearerTechnology::HSDPA_PLUS:
        return DataBearerTechnology_e::BEARER_HSDPA_PLUS;
    case telux::data::DataBearerTechnology::DC_HSDPA_PLUS:
        return DataBearerTechnology_e::BEARER_DC_HSDPA_PLUS;
    case telux::data::DataBearerTechnology::HSPA:
        return DataBearerTechnology_e::BEARER_HSPA;
    case telux::data::DataBearerTechnology::BEARER_TECH_64_QAM:
        return DataBearerTechnology_e::BEARER_64_QAM;
    case telux::data::DataBearerTechnology::TDSCDMA:
        return DataBearerTechnology_e::BEARER_TDSCDMA;
    case telux::data::DataBearerTechnology::GSM:
        return DataBearerTechnology_e::BEARER_GSM;
    case telux::data::DataBearerTechnology::BEARER_TECH_3GPP_WLAN:
        return DataBearerTechnology_e::BEARER_3GPP_WLAN;
    case telux::data::DataBearerTechnology::BEARER_TECH_5G:
        return DataBearerTechnology_e::BEARER_5G;
    default:
        PA_WARN("Unknown bearer technology: %d", TO_INT(bearerTech));
        return DataBearerTechnology_e::BEARER_UNKNOWN;
    }
};

pa_result_t Utils::ConvertThrottledApnEvent
(
    const telux::data::APNThrottleInfo &sdkEvent,
    ThrottledApnEventInfo_t &paEvent
)
{
    paEvent.apn                 = sdkEvent.apn;
    paEvent.ipv4Time            = sdkEvent.ipv4Time;
    paEvent.ipv6Time            = sdkEvent.ipv6Time;
    paEvent.isBlockedOnAllPLMNs = sdkEvent.isBlocked;
    paEvent.mcc                 = sdkEvent.mcc;
    paEvent.mnc                 = sdkEvent.mnc;
    for (int id : sdkEvent.profileIds)
    {
        paEvent.profileIds.push_back(static_cast<ProfileId_e>(id));
    }
    return PA_OK;
}

QosFlowState_e Utils::ConvertQosFlowState
(
    telux::data::QosFlowStateChangeEvent stateChangeEvent
)
{
    switch (stateChangeEvent)
    {
    case telux::data::QosFlowStateChangeEvent::ACTIVATED:
        return QosFlowState_e::ACTIVATED;
    case telux::data::QosFlowStateChangeEvent::MODIFIED:
        return QosFlowState_e::MODIFIED;
    case telux::data::QosFlowStateChangeEvent::DELETED:
        return QosFlowState_e::DELETED;
    default:
        PA_WARN("Unknown QoS flow state: %d", TO_INT(stateChangeEvent));
        return QosFlowState_e::UNKNOWN;
    }
}
HwAccelerationState_e Utils::ConvertHwAccelerationState
(
    telux::data::ServiceState state
)
{
    if (telux::data::ServiceState::ACTIVE == state)
    {
        return HwAccelerationState_e::ACTIVE;
    }
    return HwAccelerationState_e::INACTIVE;
}

const char *Utils::ProfileChangeEventToString(telux::data::ProfileChangeEvent event)
{

    switch (event)
    {
    case telux::data::ProfileChangeEvent::CREATE_PROFILE_EVENT:
        return "CREATED";
    case telux::data::ProfileChangeEvent::DELETE_PROFILE_EVENT:
        return "DELETED";
    case telux::data::ProfileChangeEvent::MODIFY_PROFILE_EVENT:
        return "MODIFIED";
    default:
        PA_WARN("Unknown profile change event: %d", TO_INT(event));
        return "UNKNOWN";
    };
}

taf::pa::data::ProfileEvent_e Utils::ConvertProfileChangeEvent
(
    const telux::data::ProfileChangeEvent sdkEvent
)
{
    switch (sdkEvent)
    {
    case telux::data::ProfileChangeEvent::CREATE_PROFILE_EVENT:
        return ProfileEvent_e::CREATED;
    case telux::data::ProfileChangeEvent::DELETE_PROFILE_EVENT:
        return ProfileEvent_e::DELETED;
    case telux::data::ProfileChangeEvent::MODIFY_PROFILE_EVENT:
        return ProfileEvent_e::MODIFIED;
    default:
        PA_WARN("Unknown profile change event: %d", TO_INT(sdkEvent));
        return ProfileEvent_e::UNKNOWN;
    };
}

const char *Utils::SubsysStateToString(taf::pa::data::SubsystemState_e state)
{
    switch (state)
    {
    case SubsystemState_e::AVAILABLE:
        return "AVAILABLE";
    case SubsystemState_e::UNAVAILABLE:
        return "UNAVAILABLE";
    case SubsystemState_e::FAILED:
        return "FAILED";
    default:
        PA_WARN("Unknown state: %d", TO_INT(state));
        return "FAILED";
    };
}

taf::pa::data::BandIntPriority_e Utils::ConvertBandIntPriority
(
    const telux::data::BandPriority bandPriority
)
{
    switch (bandPriority)
    {
    case telux::data::BandPriority::N79:
        return BandIntPriority_e::N79_5G;
    case telux::data::BandPriority::WLAN:
        return BandIntPriority_e::WLAN_5G;
    default:
        PA_WARN("Unknown band priority: %d", TO_INT(bandPriority));
        return BandIntPriority_e::UNKNOWN;
    };
}

telux::data::BandPriority Utils::ConvertBandIntPriority
(
    const taf::pa::data::BandIntPriority_e bandPriority
)
{
    switch (bandPriority)
    {
    case BandIntPriority_e::N79_5G:
        return telux::data::BandPriority::N79;
    case BandIntPriority_e::WLAN_5G:
        return telux::data::BandPriority::WLAN;
    default:
        PA_WARN("Unknown band priority: %d. Return N79", TO_INT(bandPriority));
        return telux::data::BandPriority::N79;
    };
}

void Utils::ConvertUplinkThroughputInfo
(
    const telux::data::UplinkThroughputInfo &sdkInfo,
    UplinkThroughputInfo_t &paInfo
)
{
    paInfo.throughput = sdkInfo.throughput;
    paInfo.maxThroughput = sdkInfo.maxThroughput;
    paInfo.queueSize = sdkInfo.queueSize;
}

void Utils::ConvertDownlinkThroughputInfo
(
    const telux::data::DownlinkThroughputInfo &sdkInfo,
    DownlinkThroughputInfo_t &paInfo
)
{
    paInfo.throughput = sdkInfo.throughput;
}

void Utils::ConvertThroughputInfo
(
    const telux::data::ThroughputInfo &sdkInfo,
    ThroughputInfo_t &paInfo
)
{
    PA_DEBUG("Converting throughput info for profile %d", sdkInfo.profileId);

    // Convert basic fields
    paInfo.slotId = ConvertSlotId(sdkInfo.slot);
    paInfo.profileId = static_cast<ProfileId_e>(sdkInfo.profileId);

    // Convert uplink info
    ConvertUplinkThroughputInfo(sdkInfo.ulThroughput, paInfo.ulThroughput);

    // Convert downlink info
    ConvertDownlinkThroughputInfo(sdkInfo.dlThroughput, paInfo.dlThroughput);
}

pa_result_t Utils::GetMtuFromInterface
(
    const std::string& interfaceName,
    int32_t& mtu
)
{
    PA_DEBUG("Getting MTU for interface: %s", interfaceName.c_str());

    if (interfaceName.empty())
    {
        PA_ERROR("Interface name is empty");
        return PA_BAD_PARAMETER;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        PA_ERROR("Failed to create socket: %s", strerror(errno));
        return PA_FAULT;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));

    // Copy interface name, ensuring null termination
    int iRet = std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", interfaceName.c_str());
    if (iRet < 0 || iRet >= IFNAMSIZ)
    {
        PA_ERROR("Interface name too long or encoding error: %s", interfaceName.c_str());
        close(sockfd);
        return PA_BAD_PARAMETER;
    }
    // Ensure null termination even if truncated
      ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(sockfd, SIOCGIFMTU, &ifr) < 0)
    {
        PA_ERROR("IOCTL SIOCGIFMTU failed for interface %s: %s",
                 interfaceName.c_str(), strerror(errno));
        close(sockfd);
        return PA_FAULT;
    }

    close(sockfd);
    mtu = ifr.ifr_mtu;
    PA_DEBUG("MTU for interface %s: %d", interfaceName.c_str(), mtu);

    return PA_OK;
}
