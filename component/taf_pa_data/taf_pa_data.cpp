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

#include "legato.h"
#include "taf_pa_data.hpp"
#include "taf_pa_data_TeluxData.hpp"
#include "taf_pa_data_TeluxDataProfile.hpp"
#include "taf_pa_data_TeluxDataConnection.hpp"

//--------------------------------------------------------------------------------------------------
/**
 * Get the Telux data PA state.
 *
 * @return
 *  - LE_OK              PA completely initialized
 *  - LE_UNAVAILABLE     PA not completely initialized. A part of the PA maybe usable. Check state.
 *  - LE_FAULT           PA is not usable due to fatal failure.
 *  - LE_NOT_IMPLEMENTED API is not implemented.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::Init
(
    taf::pa::data::InitState_e &state
        ///< [OUT] The Telux data PA initialization state.
)
{
    LE_DEBUG("PA implementation.");

    bool bTeluxDataState;
    bool bTeluxDataProfileState;
    bool bTeluxDataConnState;

    // Init data sub system
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    teluxPaData.Init();

    // Get the slot count
    taf::pa::data::SlotCount_e slotCount;
    teluxPaData.PaGetSimSlotCount(slotCount);

    // Init data profile sub system
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    teluxPaDataProfile.Init(slotCount);

    // Initialize the data connection subsystem
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.Init(slotCount);

    // Check the subsystem states
    teluxPaData.PaGetInitState(bTeluxDataState);
    teluxPaDataProfile.PaGetInitState(bTeluxDataProfileState);
    teluxPaDataConn.PaGetInitState(bTeluxDataConnState);
    if (bTeluxDataState && bTeluxDataProfileState && bTeluxDataConnState)
    {
        state = taf::pa::data::InitState_e::INIT_DONE;
        return LE_OK;
    }
    state = taf::pa::data::InitState_e::INIT_FAILED;
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize the Telux data PA state.
 *
 * @return
 *  - LE_OK              PA completely initialized
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::Deinit()
{
    LE_DEBUG("PA implementation.");
    // Init data profile sub system
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    teluxPaDataProfile.Deinit();

    // Initialize the data connection subsystem
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.Deinit();

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Telux data PA initialization state.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetInitState
(
    taf::pa::data::InitState_e &state///< [OUT] The Telux data PA initialization state.
)
{
    LE_DEBUG("PA implementation.");

    bool bTeluxDataState;
    bool bTeluxDataProfileState;
    bool bTeluxDataConnState;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();

    // Check the subsystem states
    teluxPaData.PaGetInitState(bTeluxDataState);
    teluxPaDataProfile.PaGetInitState(bTeluxDataProfileState);
    teluxPaDataConn.PaGetInitState(bTeluxDataConnState);
    if (bTeluxDataState && bTeluxDataProfileState && bTeluxDataConnState)
    {
        state = taf::pa::data::InitState_e::INIT_DONE;
        return LE_OK;
    }
    state = taf::pa::data::InitState_e::INIT_FAILED;
    return LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetSimSlotCount
(
    taf::pa::data::SlotCount_e &slotCount
    ///< [OUT] The number of SIM slots.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetSimSlotCount(slotCount);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the phone Ids.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetPhoneIds
(
    std::vector<taf::pa::data::PhoneId_e> &phoneIds
    ///< [OUT] The phone IDs.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetPhoneIds(phoneIds);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetPhoneIdFromSimSlotId
(
    taf::pa::data::SlotId_e slotID,
    ///< [IN] The SIM slot ID.
    taf::pa::data::PhoneId_e &phoneID
    ///< [OUT] The phone ID.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetPhoneIdFromSimSlotId(slotID, phoneID);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM slot count.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetSimSlotIdFromPhoneId
(
    taf::pa::data::PhoneId_e phoneID,
    ///< [IN] The phone ID.
    taf::pa::data::SlotId_e &slotID
    ///< [OUT] The SIM slot ID.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetSimSlotIdFromPhoneId(phoneID, slotID);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get profiles from the NAD for the specified slot ID.
 *
 * Use context to distinguish between different slots if the same handler is used.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetProfilesAsync
(
    taf::pa::data::PhoneId_e phoneId,
    taf_pa_data_profile_GetAllAsyncCb callback,
    void* contextPtr
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaListProfiles(phoneId, callback, contextPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the default profile
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetDefaultProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The profile information.
    taf::pa::data::ProfileId_e &profileId
    ///< [OUT] The default profile ID.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaGetDefaultProfile(phoneId, profileId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the default profile
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::SetDefaultProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The profile information.
    taf::pa::data::ProfileId_e profileId
    ///< [IN] The default profile ID.
)
{
    LE_DEBUG("PA implementation.");
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
le_result_t taf::pa::data::CreateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone id.
    taf::pa::data::ProfileInfo_t profileInfo,
    ///< [IN] The profile information.
    ProfileId_e &profileId
    ///< [OUT] The profile id on success.
)
{
    LE_DEBUG("PA implementation.");
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
le_result_t taf::pa::data::UpdateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone id.
    taf::pa::data::ProfileInfo_t profileInfo
    ///< [IN] The profile information.
)
{
    LE_DEBUG("PA implementation.");
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
le_result_t taf::pa::data::DeleteProfile
(
    taf::pa::data::PhoneId_e phoneId,
    ///< [IN] The phone id.
    taf::pa::data::ProfileInfo_t profileInfo
    ///< [IN] The profile information.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaDeleteProfile(phoneId, profileInfo);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start a data session
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::StartDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params ///< [IN] The IP type.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaStartDataSessionAsync(params);
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop a data session
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::StopDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params ///< [IN] The IP type.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaStopDataSessionAsync(params);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// PA Callbacks
////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------
/**
 * Register for data call events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::AddDataCallEventsCallback
(
    taf_pa_data_CallEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddDataCallEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered data call events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::RemoveDataCallEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveDataCallEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register roaming events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::AddRoamingEventsCallback
(
    taf_pa_data_RoamingEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaAddRoamingEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered roaming events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::RemoveRoamingEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaRemoveRoamingEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get roaming status. Events will be provided via taf_pa_data_RoamingEventsCb that is registered
 * via AddRoamingEventsCallback()
 *
 * @return LE_OK on success. Wait for callback for final status.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetRoamingStatus
(
    const taf::pa::data::PhoneId_e phoneId,
    RoamingStatus_t &roamingStatus
)
{
    LE_DEBUG("PA implementation.");
    LE_UNUSED(phoneId);
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    return teluxPaData.PaGetRoamingStatus(phoneId, roamingStatus);
}
//--------------------------------------------------------------------------------------------------
/**
 * Register throttled APN events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::AddThrottledApnEventsCallback
(
    taf_pa_data_ThrottledApnEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddThrottledApnEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered throttled APN events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::RemoveThrottledApnEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveThrottledApnEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get throttled APNs information.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::GetThrottledApnInfo
(
    const taf::pa::data::PhoneId_e phoneId,
        ///< [IN] The phone ID.
    std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
        ///< [OUT] The list of throttled APNs info.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.paGetThrottledApnInfo(phoneId, throttledApnEventInfoList);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register QoS TFT events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::AddQosTftEventsCallback
(
    taf_pa_data_QosTftEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddQosTftEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered QoS TFT events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::RemoveQosTftEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveQosTftEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register HW acceleration change events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::AddHwAccelerationChangeEventsCallback
(
    taf_pa_data_HwAccelerationEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaAddHwAccelerationChangeEventsCallback(callBack, context, id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered HW acceleration change events callback
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::RemoveHwAccelerationChangeEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaRemoveHwAccelerationChangeEventsCallback(id);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register SDK callbacks. This is typically not needed as the callbacks will be registered during
 * initialization.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf::pa::data::RegisterSDKCallbacks()
{
    LE_DEBUG("PA implementation.");
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
le_result_t taf::pa::data::DeregisterSDKCallbacks()
{
    LE_DEBUG("PA implementation.");
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    teluxPaData.DeregisterDataServingSystemListeners();

    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    return teluxPaDataConn.PaDeregisterDataConnCallbacks();
}

/**
 * The component entry point.
 **/
COMPONENT_INIT
{
    LE_INFO("DataCall PA Component Init");
}
