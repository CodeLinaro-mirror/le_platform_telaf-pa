/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_Utils.cpp
 * @brief The TelAF Data PA utility functions.
 *
 */
#include "taf_pa_data_Utils.hpp"

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
        return RoamingType_e::UNKNOWN;
    }
}

ProfileId_e Utils::ConvertProfileId(int id)
{
    if (isValueInRange ( id,
                          TO_INT(ProfileId_e::ID_1),
                          TO_INT(ProfileId_e::ID_255))
                        )
    {
        LE_DEBUG("Id: %d", id);
        return static_cast<ProfileId_e>(id);
    }
    else
    {
        LE_WARN ("Id out of range: %d", id);
        return ProfileId_e::INVALID;
    }
}

TechPref_e Utils::ConvertTechPref
                                                    (telux::data::TechPreference techPref)
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
        break;
    }
    return TechPref_e::TP_UNKNOWN;
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
        break;
    }
    return telux::data::TechPreference::UNKNOWN;
}

AuthType_e Utils::ConvertAuthType
                                                    (telux::data::AuthProtocolType authType)
{
    using namespace taf::pa::data;
    switch (authType)
    {
    case telux::data::AuthProtocolType::AUTH_PAP:
        return AuthType_e::PAP;
    case telux::data::AuthProtocolType::AUTH_CHAP:
        return AuthType_e::CHAP;
    case telux::data::AuthProtocolType::AUTH_PAP_CHAP:
        return AuthType_e::PAP_CHAP;
    default:
        break;
    }
    return AuthType_e::NONE;
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
        break;
    }
    return IpType_e::UNKNOWN;
}

telux::data::IpFamilyType Utils::ConvertIpType
                                                        (IpType_e ipType)
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
        break;
    }
    return telux::data::IpFamilyType::UNKNOWN;
}

telux::data::AuthProtocolType Utils::ConvertAuthType
                                                    (AuthType_e authType)
{
    using namespace taf::pa::data;
    switch (authType)
    {
    case AuthType_e::PAP:
        return telux::data::AuthProtocolType::AUTH_PAP;
    case AuthType_e::CHAP:
        return telux::data::AuthProtocolType::AUTH_CHAP;
    case AuthType_e::PAP_CHAP:
        return telux::data::AuthProtocolType::AUTH_PAP_CHAP;
    default:
        break;
    }
    return telux::data::AuthProtocolType::AUTH_NONE;
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
        break;
    }
    return EmergencyCapability_e::UNSPECIFIED;
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
        break;
    }
    return telux::data::EmergencyCapability::NOT_ALLOWED;
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
            break;
    };
    return telux::data::DataCallStatus::INVALID;
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
            break;
    };
    return DataCallStatus_e::UNKNOWN;
}

le_result_t Utils::ConvertProfileInfo
(
    telux::data::DataProfile &dataProfile,
    ProfileInfo_t   &profileInfo
)
{
    LE_DEBUG("Profile details:");
    using namespace taf::pa::data;

    profileInfo.profileId = ConvertProfileId(dataProfile.getId());
    LE_DEBUG(" Id          : %d", TO_INT(profileInfo.profileId));

    if (!dataProfile.getApn().empty())
    {
        le_utf8_Copy(profileInfo.apn, dataProfile.getApn().c_str(), MAX_APN_LEN, NULL);
        LE_DEBUG(" APN         : %s", profileInfo.apn);
    }

    if (!dataProfile.getName().empty())
    {
        le_utf8_Copy(profileInfo.name, dataProfile.getName().c_str(), MAX_NAME_LEN, NULL);
        LE_DEBUG(" Name        : %s", profileInfo.name);
    }

    if (!dataProfile.getUserName().empty())
    {
        le_utf8_Copy(profileInfo.userName,dataProfile.getUserName().c_str(),MAX_USERNAME_LEN, NULL);
        LE_DEBUG(" Username    : %s", profileInfo.userName);
    }

    if (!dataProfile.getPassword().empty())
    {
        le_utf8_Copy(profileInfo.password,dataProfile.getPassword().c_str(),MAX_PASSWORD_LEN, NULL);
        LE_DEBUG(" Password    : %s", profileInfo.password);
    }

    profileInfo.techPref = ConvertTechPref(dataProfile.getTechPreference());
    LE_DEBUG(" TechPref    : %d", TO_INT(profileInfo.techPref));

    profileInfo.authType = ConvertAuthType(dataProfile.getAuthProtocolType());
    LE_DEBUG(" AuthType    : %d", TO_INT(profileInfo.authType));

    profileInfo.ipType = ConvertIpType(dataProfile.getIpFamilyType());
    LE_DEBUG(" IPType      : %d", TO_INT(profileInfo.ipType));

    profileInfo.apnTypeMask = ConvertApnTypeMask(dataProfile.getApnTypes());
    LE_DEBUG(" TelSDK mask : %ld", dataProfile.getApnTypes().to_ulong());
    LE_DEBUG(" APNType mask: %d", TO_INT(profileInfo.apnTypeMask));

    profileInfo.emergencyCallSupport = ConvertEmerCallCap(dataProfile.getIsEmergencyAllowed());
    LE_DEBUG(" Emer call   : %d", TO_INT(profileInfo.emergencyCallSupport));

    return LE_OK;
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
        LE_ERROR("call(%d) status error", static_cast<int32_t>(status));
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
        LE_ERROR("unknown ip: %d", static_cast<int32_t>(ipType));
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
        LE_ERROR("unknown tech preference(%d)", static_cast<int32_t>(techPref));
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
        LE_ERROR("unknown data bearer type(%d)", static_cast<int32_t>(dataBearer));
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
        LE_ERROR("end reason(%d) error", static_cast<int32_t>(endReasontype));
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
        return DataBearerTechnology_e::BEARER_UNKNOWN;
    }
};

le_result_t Utils::ConvertThrottledApnEvent
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
    return LE_OK;
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
