/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * @file       taf_pa_data.hpp
 * @brief      The PA data interface.
 */
#ifndef __TAF_PA_DATA_HPP__
#define __TAF_PA_DATA_HPP__

#include "taf_pa_common.h"
#include "taf_pa_dataTypes.hpp"

namespace taf
{
namespace pa
{
namespace data
{

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Telux data PA state.
 *
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

} //data
} //pa
} //taf

#endif //__TAF_PA_DATA_HPP__