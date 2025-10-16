/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_Utils.hpp
 * @brief Telux Data utility functions.
 *
 */

#ifndef __TAF_PA_DATA_UTILS_HPP__
#define __TAF_PA_DATA_UTILS_HPP__

#include "legato.h"
#include "taf_pa_dataTypes.hpp"
#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include "telux/common/CommonDefines.hpp"
#include <thread>
#include <sstream>
#include <future>
#include <exception>

/**
 * Convert a ENUM to an integer primarily for printing with LE log APIs.
 *
 * Consider using the to_int template for more complex use cases.
 */
#define TO_INT(value) static_cast<int>(value)

/**
 * @brief Use a try-catch when using future.get() and return the result.
 *
 * Use a future wait() or wait_for() before calling get() to handle long running tasks.
 *
 */
#define FUTURE_GET_RET_VAL(future, futureResult, result) \
    try                                                         \
    {                                                           \
        futureResult = future.get();                            \
    }                                                           \
    catch (const std::exception &e)                             \
    {                                                           \
        LE_WARN("Exception caught: %s", e.what());              \
        return result;                                          \
    }                                                           \
    catch (...)                                                 \
    {                                                           \
        LE_WARN("Unknown exception caught");                    \
        return result;                                          \
    }

/**
 * @brief Use a try-catch when using future.get() and simply return.
 *
 * Use a future wait() or wait_for() before calling get() to handle long running tasks.
 */
#define FUTURE_GET_RET_NIL(future, futureResult) \
    try                                                 \
    {                                                   \
        futureResult = future.get();                    \
    }                                                   \
    catch (const std::exception &e)                     \
    {                                                   \
        LE_WARN("Exception caught: %s", e.what());      \
        return;                                         \
    }                                                   \
    catch (...)                                         \
    {                                                   \
        LE_WARN("Unknown exception caught");            \
        return;                                         \
    }

/**
 * @brief Set the SDK thread name for easier debugging.
 */
#define SET_SDK_THREAD_NAME()                                     \
    do                                                            \
    {                                                             \
        std::ostringstream threadName;                            \
        threadName << "SDK-" << std::this_thread::get_id();       \
        le_thread_InitLegatoThreadData(threadName.str().c_str()); \
    } while (0);

/**
 * @brief Return true if the given "value" is within the specified range
 */
template <typename T>
bool isValueInRange(T value, T lowerBound, T upperBound)
{
    return value >= lowerBound && value <= upperBound;
}

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

        static le_result_t ConvertProfileInfo
        (
            telux::data::DataProfile  &dataProfile,
            ProfileInfo_t    &profileInfo
        );

        static le_result_t ConvertThrottledApnEvent
        (
            const telux::data::APNThrottleInfo &sdkEvent,
            ThrottledApnEventInfo_t &paEvent
        );

        // TelSDK conversions
        static const char *CallStatusToString(telux::data::DataCallStatus status);
        static const char *IpFamilyTypeToString(telux::data::IpFamilyType ipType);
        static const char *TechPreferenceToString(telux::data::TechPreference techPref);
        static const char *DataBearerToString(telux::data::DataBearerTechnology techPref);
        static const char *CallEndReasonTypeToString(telux::common::EndReasonType endType);
};

} // data
} // pa
} // taf

#endif //__TAF_PA_DATA_UTILS_HPP__
