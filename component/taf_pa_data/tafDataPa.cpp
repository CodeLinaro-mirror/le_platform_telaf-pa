/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


/**
 * @file taf_pa_data.cpp
 * @brief The TelAF PA for Data Call Service. It implements the PA APIs that are used by the data
 * call service.
 *
 *
 */

#include "tafDataPa.hpp"
#include "tafDataTeluxDataPa.hpp"
#include "tafDataTeluxDataProfilePa.hpp"
#include "tafDataTeluxDataConnectionPa.hpp"

//--------------------------------------------------------------------------------------------------
/**
 * Get the Telux data PA state.
 *
 * @return
 *  - PA_OK              PA completely initialized
 *  - PA_UNAVAILABLE     PA not completely initialized. A part of the PA maybe usable. Check state.
 *  - PA_FAULT           PA is not usable due to fatal failure.
 *  - PA_NOT_IMPLEMENTED API is not implemented.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::Init
(
    taf::pa::data::SubsystemState_e &state
        ///< [OUT] The Telux data PA initialization state.
)
{
    PA_DEBUG("PA implementation.");
    SubsystemState_e teluxPhoneManagerState = SubsystemState_e::FAILED;
    SubsystemState_e teluxServingSystemState_slot1 = SubsystemState_e::FAILED;
    SubsystemState_e teluxServingSystemState_slot2 = SubsystemState_e::FAILED;
    SubsystemState_e teluxDataProfileState_slot1 = SubsystemState_e::FAILED;
    SubsystemState_e teluxDataProfileState_slot2 = SubsystemState_e::FAILED;
    SubsystemState_e teluxDataConnState_slot1 = SubsystemState_e::FAILED;
    SubsystemState_e teluxDataConnState_slot2 = SubsystemState_e::FAILED;

    // Init data sub system
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    teluxPaData.Init();

    // Get the slot count
    taf::pa::data::SlotCount_e slotCount;
    teluxPaData.PaGetSimSlotCount(slotCount);
    PA_INFO("Num slots: %d", slotCount);

    // Init data profile sub system
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    teluxPaDataProfile.Init(slotCount);

    // Initialize the data connection subsystem
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.Init(slotCount);

    // Check the subsystem states
    teluxPhoneManagerState = teluxPaData.PaGetPhoneManagerInitState();
    if (SubsystemState_e::AVAILABLE != teluxPhoneManagerState)
    {
        state = taf::pa::data::SubsystemState_e::FAILED;
        PA_ERROR("Phone manager is not initialized - complete failure.");
        return PA_FAULT;
    }
    if (taf::pa::data::SlotCount_e::ONE == slotCount)
    {
        teluxPaData.PaGetServingSystemInitState(taf::pa::data::SlotId_e::SLOT_1,
                                                                teluxServingSystemState_slot1);
        teluxPaDataProfile.PaGetSubsysState(taf::pa::data::SlotId_e::SLOT_1,
                                                                teluxDataProfileState_slot1);
        teluxPaDataConn.PaGetSubsysState(taf::pa::data::SlotId_e::SLOT_1,
                                                                teluxDataConnState_slot1);
        if (
            SubsystemState_e::AVAILABLE == teluxServingSystemState_slot1 &&
            SubsystemState_e::AVAILABLE == teluxDataProfileState_slot1 &&
            SubsystemState_e::AVAILABLE == teluxDataConnState_slot1)
        {
            PA_INFO("Data PA ready.");
            state = taf::pa::data::SubsystemState_e::AVAILABLE;
            return PA_OK;
        }
    }
    else if (taf::pa::data::SlotCount_e::TWO == slotCount)
    {
        teluxPaData.PaGetServingSystemInitState(taf::pa::data::SlotId_e::SLOT_1,
                                                                teluxServingSystemState_slot1);
        teluxPaDataProfile.PaGetSubsysState(taf::pa::data::SlotId_e::SLOT_1,
                                                                teluxDataProfileState_slot1);
        teluxPaDataConn.PaGetSubsysState(taf::pa::data::SlotId_e::SLOT_1,
                                                                teluxDataConnState_slot1);

        teluxPaData.PaGetServingSystemInitState(taf::pa::data::SlotId_e::SLOT_2,
                                                                teluxServingSystemState_slot2);
        teluxPaDataProfile.PaGetSubsysState(taf::pa::data::SlotId_e::SLOT_2,
                                                                teluxDataProfileState_slot2);
        teluxPaDataConn.PaGetSubsysState(taf::pa::data::SlotId_e::SLOT_2,
                                                                teluxDataConnState_slot2);

        bool slot1Ready = (SubsystemState_e::AVAILABLE == teluxServingSystemState_slot1 &&
                           SubsystemState_e::AVAILABLE == teluxDataProfileState_slot1 &&
                           SubsystemState_e::AVAILABLE == teluxDataConnState_slot1);

        bool slot2Ready = (SubsystemState_e::AVAILABLE == teluxServingSystemState_slot2 &&
                           SubsystemState_e::AVAILABLE == teluxDataProfileState_slot2 &&
                           SubsystemState_e::AVAILABLE == teluxDataConnState_slot2);

        if (slot1Ready && slot2Ready)
        {
            PA_INFO("Data PA ready for both slots.");
            state = taf::pa::data::SubsystemState_e::AVAILABLE;
            return PA_OK;
        }
        else if (slot1Ready || slot2Ready)
        {
            PA_WARN("Data PA partially ready. Slot1: %s, Slot2: %s",
                    slot1Ready ? "READY" : "FAILED",
                    slot2Ready ? "READY" : "FAILED"
                    );
            state = taf::pa::data::SubsystemState_e::UNAVAILABLE;
            return PA_UNAVAILABLE;
        }
    }
    PA_ERROR("Data PA init failed - complete failure.");
    state = taf::pa::data::SubsystemState_e::FAILED;
    return PA_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize the Telux data PA state.
 *
 * @return
 *  - PA_OK              PA completely initialized
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::Deinit()
{
    PA_DEBUG("PA implementation.");
    // Init data profile sub system
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    teluxPaDataProfile.Deinit();

    // Initialize the data connection subsystem
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.Deinit();

    // Deinit other subsystems.
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    teluxPaData.Deinit();

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Telux data PA initialization state.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetSubsystemState
(
    taf::pa::data::PhoneId_e phoneId,
    taf::pa::data::Subsystem_e subsystem,
    taf::pa::data::SubsystemState_e &state
)
{
    PA_DEBUG("PA implementation.");

    taf::pa::data::SlotId_e slotId;

    // Initialize to FAILED state
    state = taf::pa::data::SubsystemState_e::FAILED;

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotId);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotId));

    switch (subsystem)
    {
    case Subsystem_e::PHONE_MANAGER:
        {
            state = teluxPaData.PaGetPhoneManagerInitState();
            PA_INFO("Phone manager state: %s", Utils::SubsysStateToString(state));
            break;
        }
        case Subsystem_e::DATACALL_MANAGER:
        {
            auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
            teluxPaDataConn.PaGetSubsysState(slotId, state);
            PA_INFO("Call manager state: %s", Utils::SubsysStateToString(state));
            break;
        }
        case Subsystem_e::PROFILE_MANAGER:
        {
            auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
            teluxPaDataProfile.PaGetSubsysState(slotId, state);
            PA_INFO("Profile manager state: %s", Utils::SubsysStateToString(state));
            break;
        }
        case Subsystem_e::SERVING_SYSTEM_MANAGER:
        {
            teluxPaData.PaGetServingSystemInitState(slotId, state);
            PA_INFO("Serving system manager state: %s", Utils::SubsysStateToString(state));
            break;
        }
        default:
        {
            PA_WARN("Unknown subsystem: %d", TO_INT(subsystem));
            return PA_FAULT;
        }
    };

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetSimSlotCount
(
    taf::pa::data::SlotCount_e &slotCount
    ///< [OUT] The number of SIM slots.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetSimSlotCount(slotCount);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the phone Ids.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetPhoneIds
(
    std::vector<taf::pa::data::PhoneId_e> &phoneIds
    ///< [OUT] The phone IDs.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetPhoneIds(phoneIds);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetPhoneIdFromSimSlotId
(
    taf::pa::data::SlotId_e slotID,
    ///< [IN] The SIM slot ID.
    taf::pa::data::PhoneId_e &phoneID
    ///< [OUT] The phone ID.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetPhoneIdFromSlotId(slotID, phoneID);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetSimSlotIdFromPhoneId
(
    taf::pa::data::PhoneId_e phoneID,
    ///< [IN] The phone ID.
    taf::pa::data::SlotId_e &slotID
    ///< [OUT] The SIM slot ID.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetSlotIdFromPhoneId(phoneID, slotID);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get profiles from the NAD for the specified slot ID.
 *
 * Use context to distinguish between different slots if the same handler is used.
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetProfilesAsync
(
    taf::pa::data::PhoneId_e phoneId,
    taf_pa_data_profile_GetAllAsyncCb callback,
    void* contextPtr
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaListProfiles(phoneId, callback, contextPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get details of specified profile.
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetProfileInfo
(
    PhoneId_e phoneId,
    ///< [IN] The phone id.
    ProfileInfo_t &profileInfo
    ///< [OUT] The profile information.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaGetProfileInfo(phoneId, profileInfo);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the default profile
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetDefaultProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The profile information.
    taf::pa::data::ProfileId_e &profileId
    ///< [OUT] The default profile ID.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaGetDefaultProfile(phoneId, profileId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the default profile
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::SetDefaultProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The profile information.
    taf::pa::data::ProfileId_e profileId
    ///< [IN] The default profile ID.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaSetDefaultProfile(phoneId, profileId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a profile
 *
 * On success, the created profile ID will be available.
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::CreateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone id.
    taf::pa::data::ProfileInfo_t profileInfo,
    ///< [IN] The profile information.
    ProfileId_e &profileId
    ///< [OUT] The profile id on success.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaCreateProfile(phoneId, profileInfo, profileId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Update a profile
 *
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::UpdateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone id.
    taf::pa::data::ProfileInfo_t profileInfo
    ///< [IN] The profile information.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaUpdateProfile(phoneId, profileInfo);
}

//--------------------------------------------------------------------------------------------------
/**
 * Update a profile
 *
 * Only the phone ID and profile ID are considered in the structure.
 *
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::DeleteProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone id.
    taf::pa::data::ProfileInfo_t profileInfo
    ///< [IN] The profile information.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaDeleteProfile(phoneId, profileInfo);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a data session
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::StartDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params ///< [IN] The IP type.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaStartDataSessionAsync(params);
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop a data session
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::StopDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params ///< [IN] The IP type.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaStopDataSessionAsync(params);
}

pa_result_t taf::pa::data::RequestDataCallsListAsync
(
    PhoneId_e phoneId,
                ///< [IN] The phone ID.
    taf_pa_data_RequestCallListCb callBack,
                ///< [IN] The callback function.
    std::shared_ptr<void> context
                ///< [IN] The context pointer.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRequestDataCallsListAsync(phoneId, callBack, context);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PA Callbacks
////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------
/**
 * Register for data call events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddDataCallEventsCallback
(
    taf_pa_data_CallEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddDataCallEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered data call events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveDataCallEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveDataCallEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register roaming events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddRoamingEventsCallback
(
    taf_pa_data_RoamingEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaAddRoamingEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered roaming events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveRoamingEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaRemoveRoamingEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get roaming status. Events will be provided via taf_pa_data_RoamingEventsCb that is registered
 * via AddRoamingEventsCallback()
 *
 * @return PA_OK on success. Wait for callback for final status.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetRoamingStatus
(
    const taf::pa::data::PhoneId_e phoneId,
    RoamingStatus_t &roamingStatus
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetRoamingStatus(phoneId, roamingStatus);
}
//--------------------------------------------------------------------------------------------------
/**
 * Register throttled APN events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddThrottledApnEventsCallback
(
    taf_pa_data_ThrottledApnEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddThrottledApnEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered throttled APN events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveThrottledApnEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveThrottledApnEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get throttled APNs information.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetThrottledApnInfo
(
    const taf::pa::data::PhoneId_e phoneId,
        ///< [IN] The phone ID.
    std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
        ///< [OUT] The list of throttled APNs info.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.paGetThrottledApnInfo(phoneId, throttledApnEventInfoList);
}


//--------------------------------------------------------------------------------------------------
/**
 * Register QoS TFT events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddQosTftEventsCallback
(
    taf_pa_data_QosTftEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddQosTftEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered QoS TFT events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveQosTftEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveQosTftEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register HW acceleration change events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddHwAccelerationChangeEventsCallback
(
    taf_pa_data_HwAccelerationEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddHwAccelerationChangeEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered HW acceleration change events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveHwAccelerationChangeEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveHwAccelerationChangeEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register profile change events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddProfileEventsCallback
(
    taf_pa_data_ProfileEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
        PA_DEBUG("PA implementation.");
        auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
        return teluxPaDataProfile.PaAddProfileEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered profile  events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveProfileEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaRemoveProfileEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register roaming events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddSubsystemStateChangeCallback
(
    taf_pa_data_SubsystemStateChangeCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    return teluxPaData.PaAddSubsystemStateChangeCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered subsystem state change callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveSubsystemStateChangeCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    return teluxPaData.PaRemoveSubsystemStateChangeCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register SDK callbacks. This is typically not needed as the callbacks will be registered during
 * initialization.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RegisterSDKCallbacks()
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    teluxPaData.RegisterDataServingSystemListeners();

    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRegisterDataConnCallbacks();
}

//--------------------------------------------------------------------------------------------------
/**
 * Deregister SDK callbacks. This is to support the service to manage suspend/resume scenarios.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::DeregisterSDKCallbacks()
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    teluxPaData.DeregisterDataServingSystemListeners();

    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaDeregisterDataConnCallbacks();
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the throughput report interval.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::SetThroughputReportInterval
(
    PhoneId_e phoneId,
    uint32_t reportInterval
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaSetThroughputReportInterval(phoneId, reportInterval);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the last throughput information for all active profiles.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetLastThroughputInfo
(
    PhoneId_e phoneId,
    std::vector<ThroughputInfo_t> &throughputInfoList
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaGetLastThroughputInfo(phoneId, throughputInfoList);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register throughput events callback.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::AddThroughputEventsCallback
(
    taf_pa_data_ThroughputEventsCb callBack,
    std::shared_ptr<void> context,
    uint16_t &id
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddThroughputEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove a previously registered throughput events callback.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::RemoveThroughputEventsCallback
(
    uint16_t id
)
{
    PA_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveThroughputEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the MTU (Maximum Transmission Unit) for a network interface.
 *
 * This function retrieves the MTU for a network interface by its name.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::GetMtu
(
    const std::string& interfaceName,
    int32_t& mtu
)
{
    PA_DEBUG("PA implementation. Interface: %s", interfaceName.c_str());

    // Validate input parameter
    if (interfaceName.empty())
    {
        PA_ERROR("Interface name is empty");
        return PA_BAD_PARAMETER;
    }

    // Get MTU from the interface using utility function
    return Utils::GetMtuFromInterface(interfaceName, mtu);
}
