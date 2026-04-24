/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_Utils.hpp
 * @brief Telux Data utility functions.
 *
 */

#ifndef __TAF_DATA_UTILS_PA_HPP__
#define __TAF_DATA_UTILS_PA_HPP__

#include "tafCommonDefinesPa.hpp"
#include "tafDataPa.hpp"
#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include "telux/common/CommonDefines.hpp"
#include <thread>
#include <sstream>
#include <future>
#include <exception>
namespace taf
{
namespace pa
{
namespace data
{

class Utils
{
    public:
        static SlotId_e ConvertSlotId(SlotId);
        static SlotId   ConvertSlotId(SlotId_e);

        static telux::data::RoamingType ConvertRoamingType(RoamingType_e);
        static RoamingType_e            ConvertRoamingType(telux::data::RoamingType);

        static ProfileId_e ConvertProfileId(int);

        static TechPref_e                  ConvertTechPref(telux::data::TechPreference);
        static telux::data::TechPreference ConvertTechPref(TechPref_e);

        static AuthType_e                    ConvertAuthType(telux::data::AuthProtocolType);
        static telux::data::AuthProtocolType ConvertAuthType(AuthType_e);

        static IpType_e                  ConvertIpType(telux::data::IpFamilyType);
        static telux::data::IpFamilyType ConvertIpType(IpType_e);

        static ApnTypeBitmask_e       ConvertApnTypeMask(telux::data::ApnTypes);
        static telux::data::ApnTypes  ConvertApnTypeMask(ApnTypeBitmask_e);

        static telux::data::DataCallStatus ConvertCallStatus(DataCallStatus_e);
        static DataCallStatus_e            ConvertCallStatus(telux::data::DataCallStatus);

        static QosFlowState_e ConvertQosFlowState
        (
            telux::data::QosFlowStateChangeEvent
        );

        static DataBearerTechnology_e ConvertBearerTech
        (
            telux::data::DataBearerTechnology
        );

        static EmergencyCapability_e ConvertEmerCallCap
        (
            telux::data::EmergencyCapability
        );
        static telux::data::EmergencyCapability     ConvertEmerCallCap
        (
            EmergencyCapability_e
        );

        static HwAccelerationState_e ConvertHwAccelerationState
        (
            telux::data::ServiceState
        );

        static pa_result_t ConvertProfileInfo
        (
            telux::data::DataProfile  &dataProfile, // IN
            ProfileInfo_t    &profileInfo           // OUT
        );

        static pa_result_t ConvertThrottledApnEvent
        (
            const telux::data::APNThrottleInfo &sdkEvent,
            ThrottledApnEventInfo_t &paEvent
        );
        static taf::pa::data::ProfileEvent_e ConvertProfileChangeEvent
        (
            const telux::data::ProfileChangeEvent sdkEvent
        );
        static taf::pa::data::BandIntPriority_e ConvertBandIntPriority
        (
            const telux::data::BandPriority bandPriority
        );
        static telux::data::BandPriority ConvertBandIntPriority
        (
            const taf::pa::data::BandIntPriority_e bandPriority
        );

        static void ConvertThroughputInfo
        (
            const telux::data::ThroughputInfo &sdkInfo,
            ThroughputInfo_t &paInfo
        );

        static void ConvertUplinkThroughputInfo
        (
            const telux::data::UplinkThroughputInfo &sdkInfo,
            UplinkThroughputInfo_t &paInfo
        );

        static void ConvertDownlinkThroughputInfo
        (
            const telux::data::DownlinkThroughputInfo &sdkInfo,
            DownlinkThroughputInfo_t &paInfo
        );

        // MTU helper function
        static pa_result_t GetMtuFromInterface
        (
            const std::string& interfaceName,
            int32_t& mtu
        );

        // String conversions
        static const char *CallStatusToString(telux::data::DataCallStatus status);
        static const char *IpFamilyTypeToString(telux::data::IpFamilyType ipType);
        static const char *TechPreferenceToString(telux::data::TechPreference techPref);
        static const char *DataBearerToString(telux::data::DataBearerTechnology techPref);
        static const char *CallEndReasonTypeToString(telux::common::EndReasonType endType);
        static const char *ProfileChangeEventToString(telux::data::ProfileChangeEvent event);
        static const char *SubsysStateToString(taf::pa::data::SubsystemState_e state);
};

} // data
} // pa
} // taf

#endif //__TAF_DATA_UTILS_PA_HPP__
