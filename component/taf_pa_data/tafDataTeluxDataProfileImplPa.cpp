/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfileImpl.cpp
 * @brief Telux Data profile management
 *
 */


#include "tafDataTeluxDataPa.hpp"
#include "tafDataTeluxDataProfilePa.hpp"

/**
 * Returns TafPaTeluxDataProfile instance
 */
taf::pa::data::TafPaTeluxDataProfile& taf::pa::data::TafPaTeluxDataProfile::GetInstance()
{
    static TafPaTeluxDataProfile instance;
    return instance;
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaGetSubsysState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e &sState
)
{
    // Initialize to FAILED state
    sState = taf::pa::data::SubsystemState_e::FAILED;

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");

    PA_INFO("Profile init state for slot Id[%d]: %d", slotId,
                                                    dataProfileManagersInitStateMap_[slotId]);
    sState = dataProfileManagersInitStateMap_[slotId];
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::SetSubsysState
(
    taf::pa::data::SlotId_e slotId,
    SubsystemState_e sState,
    bool bSendEvent
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");

    dataProfileManagersInitStateMap_[slotId] = sState;
    PA_INFO("Profile init state for slot Id[%d]: %d", slotId,
                                                    dataProfileManagersInitStateMap_[slotId]);
    if (bSendEvent)
    {
        PA_INFO("Send event to clients.");
        // Send the state change event to clients.
        auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
        SubsystemEvent_t event;
        pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(slotId, event.phoneId);
        TAF_PA_ERROR_IF_RET_VAL(PA_OK != result, result, "PaGetPhoneIdFromSlotId err: %d", result);
        event.subsystem = Subsystem_e::PROFILE_MANAGER;
        event.subsystemState = sState;
        teluxPaData.SendSubsystemEventToClients(event);
    }
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaRegisterProfileCallbacks()
{
    bool allSuccess = true;
    std::vector<int> failedSlots;

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");

    std::unique_lock lock(dataProfileCallbacksMutex_);
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Register serving system listener for each slot it
        if (bProfileListenersRegistered_[slotId - 1])
        {
            PA_INFO("Profile listener already registered for slot ID: %d.", slotId);
        }
        else
        {
            auto managerIt = dataProfileManagersMap_.find((SlotId)slotId);
            if (managerIt != dataProfileManagersMap_.end())
            {
                SubsystemState_e subsysState;
                PaGetSubsysState(static_cast<SlotId_e>(slotId), subsysState);
                if (SubsystemState_e::AVAILABLE != subsysState)
                {
                    PA_ERROR("Subsystem not initialized for slot id: %d", slotId);
                    allSuccess = false;
                    failedSlots.push_back(slotId);
                    // Go to the next slot if available
                    continue;
                }
                auto listenerIt = tafPaTeluxDataProfListenersMap_.find((SlotId)slotId);
                if (listenerIt != tafPaTeluxDataProfListenersMap_.end())
                {
                    if (managerIt->second->registerListener(listenerIt->second) ==
                        telux::common::Status::SUCCESS)
                    {
                        PA_INFO("Data profile listener for slot ID %d registered.", slotId);
                        bProfileListenersRegistered_[slotId - 1] = true;
                    }
                    else
                    {
                        PA_ERROR("Failed to register profile listener for slot ID %d.", slotId);
                        allSuccess = false;
                        failedSlots.push_back(slotId);
                    }
                }
            }
        }
    }
    if (!allSuccess)
    {
        PA_ERROR("=== Registration Failed ===");
        PA_ERROR("Failed to register listeners for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            PA_ERROR("  - Slot %d: REGISTRATION FAILED", slot);
        }
        PA_ERROR("============================");
        return PA_FAULT;
    }

    PA_INFO("All profile listeners successfully registered");
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaDeregisterProfileCallbacks()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    // Protect the critical section
    std::unique_lock lock(dataProfileCallbacksMutex_);

    bool allSuccess = true;
    std::vector<int> failedSlots;

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Deregister profile listener for each slot
        if (bProfileListenersRegistered_[slotId - 1])
        {
            if (dataProfileManagersMap_.find((SlotId)slotId) != dataProfileManagersMap_.end())
            {
                telux::common::Status status = dataProfileManagersMap_[(SlotId)slotId]->
                    deregisterListener(tafPaTeluxDataProfListenersMap_[(SlotId)slotId]);

                if (telux::common::Status::SUCCESS == status)
                {
                    PA_INFO("Profile listener for slot ID %d deregistered.", slotId);
                    bProfileListenersRegistered_[slotId - 1] = false;
                }
                else
                {
                    PA_ERROR("FAILED to deregister profile listener for slot ID %d. Status: %d",
                             slotId, TO_INT(status));
                    failedSlots.push_back(slotId);
                    allSuccess = false;
                }
            }
        }
        else
        {
            PA_INFO("Profile listener already deregistered for slot ID: %d.", slotId);
        }
    }

    if (!allSuccess)
    {
        PA_ERROR("=== Deregistration Failed ===");
        PA_ERROR("Failed to deregister profile listeners for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            PA_ERROR("  - Slot %d: DEREGISTRATION FAILED", slot);
        }
        PA_ERROR("============================");
        return PA_FAULT;
    }

    PA_INFO("All profile listeners successfully deregistered");
    return PA_OK;
}

void taf::pa::data::TafPaTeluxDataProfile::Init(taf::pa::data::SlotCount_e slotCount)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    slotCount_ = slotCount;
    initDataProfileManagers();
    PaRegisterProfileCallbacks();
}

void taf::pa::data::TafPaTeluxDataProfile::Deinit()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    pa_result_t result = deInitDataProfileManagers();
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "deInitDataProfileManagers failed");
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaListProfiles
(
    taf::pa::data::PhoneId_e phoneId,
    taf_pa_data_profile_GetAllAsyncCb callback,
    void *contextPtr
)
{
    taf::pa::data::SlotId_e slotIdPa;
    telux::common::Status status;
    SlotId slotId;

    TAF_PA_ERROR_IF_RET_VAL(nullptr == callback, PA_BAD_PARAMETER, "callback is null");

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");

    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIdPa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIdPa));
    slotId = taf::pa::data::Utils::ConvertSlotId(slotIdPa);

    // Ensure callback is available for the slot ID.
    {
        auto mgrIt = tafPaDataProfileListCbksMap_.find(slotId);
        if (mgrIt == tafPaDataProfileListCbksMap_.end())
        {
            PA_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
            return PA_FAULT;
        }
    }


    TAF_PA_ERROR_IF_RET_VAL(
        tafPaDataProfileListCbksMap_[slotId]->GetListProfilesCmdInProgress(),PA_BUSY,
                                            "Command in progress for slot ID: %d", TO_INT(slotId) );

    PA_DEBUG("Calling requestProfileList.");
    tafPaDataProfileListCbksMap_[slotId]->SetListProfilesCallback(callback);
    tafPaDataProfileListCbksMap_[slotId]->SetListProfilesContext(contextPtr);
    status = dataProfileManagersMap_[slotId]->requestProfileList(
                                                            tafPaDataProfileListCbksMap_[slotId]);

    if (status != telux::common::Status::SUCCESS)
    {
        PA_WARN("requestProfileList failed: %d", TO_INT(status) );
        tafPaDataProfileListCbksMap_[slotId]->SetListProfilesCallback(nullptr);
        tafPaDataProfileListCbksMap_[slotId]->SetListProfilesContext(nullptr);
        return PA_FAULT;
    }
    PA_DEBUG("Callback will be triggered...");

    // Set the flag to indicate that the request is in progress
    tafPaDataProfileListCbksMap_[slotId]->SetListProfilesCmdInProgress(true);
    return PA_OK;
}

/**
 * The callback for create profile.
 */
void taf::pa::data::TafPaTeluxDataProfileCreateCallback::onResponse
(
    int profileId,
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();
    promise_ptr_->set_value(std::make_tuple(error, profileId));
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaCreateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    const ProfileInfo_t &profileInfo,
    taf::pa::data::ProfileId_e &profileId
)
{
    SlotId_e slotIDpa;
    telux::data::ProfileParams params;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataProfileManagersMap_.find(slotId) == dataProfileManagersMap_.end())
    {
        PA_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    // Update parameters
    params.profileName      = profileInfo.name;
    params.apn              = profileInfo.apn;
    params.userName         = profileInfo.userName;
    params.password         = profileInfo.password;
    params.techPref         = taf::pa::data::Utils::ConvertTechPref(profileInfo.techPref);
    params.authType         = taf::pa::data::Utils::ConvertAuthType(profileInfo.authType);
    params.ipFamilyType     = taf::pa::data::Utils::ConvertIpType(profileInfo.ipType);
    params.apnTypes         = taf::pa::data::Utils::ConvertApnTypeMask(profileInfo.apnTypeMask);
    params.emergencyAllowed = taf::pa::data::Utils::ConvertEmerCallCap(
                                                            profileInfo.emergencyCallSupport);
    // Lock mutex
    std::lock_guard<std::mutex> lock(dataProfileCreateMutex_);

    auto dataProfileCreatePromise = std::make_shared<
                                        std::promise<std::tuple<telux::common::ErrorCode, int>>>();
    std::future fut = dataProfileCreatePromise->get_future();

    std::shared_ptr<TafPaTeluxDataProfileCreateCallback> callback =
                std::make_shared<TafPaTeluxDataProfileCreateCallback>(dataProfileCreatePromise);

    // Create the profile
    telux::common::Status status = dataProfileManagersMap_[slotId]->createProfile
                                    (
                                        params,
                                        callback //TafPaTeluxDataProfileCreateCallback::onResponse
                                    );

    if (telux::common::Status::SUCCESS != status)
    {
        PA_WARN("createProfile failed: %d", TO_INT(status) );
        return PA_FAULT;
    }
    PA_DEBUG("Callback will be triggered...");

    // Wait for the callback
    std::tuple<telux::common::ErrorCode, int> resultTuple;

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("createProfile promise timeout");
        return PA_TIMEOUT;
    }

    FUTURE_GET_RET_VAL(fut, resultTuple, PA_FAULT);
    telux::common::ErrorCode errorCode = std::get<0>(resultTuple);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_WARN("createProfile error: %d", TO_INT(errorCode) );
        return PA_FAULT;
    }
    int createdProfileId = std::get<1>(resultTuple);
    PA_DEBUG("createProfile succeeded: %d", createdProfileId);
    profileId = static_cast<ProfileId_e>( createdProfileId );
    return PA_OK;
}

/**
 * The callback for modify profile.
 */
void taf::pa::data::TafPaTeluxDataProfileModifyCallback::commandResponse
(
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();
    promise_ptr_->set_value(error);
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaUpdateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileInfo_t &profileInfo
)
{
    SlotId_e slotIDpa;
    telux::data::ProfileParams params;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataProfileManagersMap_.find(slotId) == dataProfileManagersMap_.end())
    {
        PA_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    // Update parameters
    params.profileName      = profileInfo.name;
    params.apn              = profileInfo.apn;
    params.userName         = profileInfo.userName;
    params.password         = profileInfo.password;
    params.techPref         = taf::pa::data::Utils::ConvertTechPref(profileInfo.techPref);
    params.authType         = taf::pa::data::Utils::ConvertAuthType(profileInfo.authType);
    params.ipFamilyType     = taf::pa::data::Utils::ConvertIpType(profileInfo.ipType);
    params.apnTypes         = taf::pa::data::Utils::ConvertApnTypeMask(profileInfo.apnTypeMask);
    params.emergencyAllowed = taf::pa::data::Utils::ConvertEmerCallCap(
                                                                profileInfo.emergencyCallSupport);

    // Lock mutex
    std::lock_guard<std::mutex> lock(dataProfileUpdateMutex_);

    auto dataProfileUpdatePromise = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future fut = dataProfileUpdatePromise->get_future();

    std::shared_ptr<TafPaTeluxDataProfileModifyCallback> callback =
                std::make_shared<TafPaTeluxDataProfileModifyCallback>(dataProfileUpdatePromise);

    // Modify the profile
    telux::common::Status status = dataProfileManagersMap_[slotId]->modifyProfile
                                (
                                    static_cast<uint8_t>(profileInfo.profileId),
                                    params,
                                    callback //TafPaTeluxDataProfileModifyCallback::commandResponse
                                );

    if (telux::common::Status::SUCCESS != status)
    {
        PA_WARN("modifyProfile failed: %d", TO_INT(status));
        return PA_FAULT;
    }
    PA_DEBUG("Callback will be triggered...");

    // Wait for the callback
    telux::common::ErrorCode errorCode;

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("modifyProfile promise timeout");
        return PA_TIMEOUT;
    }

    FUTURE_GET_RET_VAL(fut, errorCode, PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_WARN("modifyProfile error: %d", TO_INT(errorCode));
        return PA_FAULT;
    }
    PA_DEBUG("modifyProfile succeeded");
    return PA_OK;
}

/**
 * The callback for delete profile.
 */
void taf::pa::data::TafPaTeluxDataProfileDeleteCallback::commandResponse
(
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();
    promise_ptr_->set_value(error);
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaDeleteProfile
(
    taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileInfo_t &profileInfo
)
{
    SlotId_e slotIDpa;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataProfileManagersMap_.find(slotId) == dataProfileManagersMap_.end())
    {
        PA_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    // Lock mutex
    std::lock_guard<std::mutex> lock(dataProfileDeleteMutex_);

    auto dataProfileDeletePromise = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future fut = dataProfileDeletePromise->get_future();

    std::shared_ptr<TafPaTeluxDataProfileDeleteCallback> callback =
                std::make_shared<TafPaTeluxDataProfileDeleteCallback>(dataProfileDeletePromise);

    // Modify the profile
    telux::common::Status status = dataProfileManagersMap_[slotId]->deleteProfile
                                (
                                    static_cast<uint8_t>(profileInfo.profileId),
                                    taf::pa::data::Utils::ConvertTechPref(profileInfo.techPref),
                                    callback //TafPaTeluxDataProfileDeleteCallback::commandResponse
                                );

    if (telux::common::Status::SUCCESS != status)
    {
        PA_WARN("deleteProfile failed: %d", TO_INT(status));
        return PA_FAULT;
    }
    PA_DEBUG("Callback will be triggered...");

    // Wait for the callback
    telux::common::ErrorCode errorCode;

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("deleteProfile promise timeout");
        return PA_TIMEOUT;
    }

    FUTURE_GET_RET_VAL(fut, errorCode, PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_WARN("deleteProfile error: %d", TO_INT(errorCode));
        return PA_FAULT;
    }
    PA_DEBUG("deleteProfile succeeded");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register profile change events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaAddProfileEventsCallback
(
    taf_pa_data_ProfileEventsCb callBack,
        ///< [IN] The callback function.
    std::shared_ptr<void> context,
        ///< [IN] The context pointer.
    uint16_t &id
        ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, PA_BAD_PARAMETER, "callBack is NULL!");
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataProfileCallbacksMutex_);

    // Add the callback
    ProfileEventsCallbackEntry_t entry = {profileEventsCallbackId_, callBack, context};
    profileEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = profileEventsCallbackId_;
    // Increment the ID.
    profileEventsCallbackId_++;
    PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    PA_INFO("Number of registered callbacks: %zu", profileEventsCallbacks_.size());

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Removed a previously registered profile  events callback
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaRemoveProfileEventsCallback
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataProfileCallbacksMutex_);
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = profileEventsCallbacks_.begin();
        cbk != profileEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            PA_INFO("Id: %d, Cbk: %p", id, cbk);
            profileEventsCallbacks_.erase(cbk);
            return PA_OK;
        }
    }
    PA_WARN("Callback not found. Id: %d", id);
    return PA_NOT_FOUND;
}

//--------------------------------------------------------------------------------------------------
/**
 * The callback for requestProfile.
 */
//--------------------------------------------------------------------------------------------------
void taf::pa::data::TafPaTeluxDataRequestProfile::onResponse
(
    const std::shared_ptr<telux::data::DataProfile> &profile,
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();
    PA_INFO("onResponse called for slot %d with error code: %d", slotId_, TO_INT(error));
    // Set the promise
    if (promisePtr_)
    {
        try
        {
            promisePtr_->set_value(std::make_tuple(error, profile));
        }
        catch (const std::future_error &e)
        {
            PA_ERROR("Exception in set_value: %s", e.what());
        }
    }
    else
    {
        PA_ERROR("Promise pointer is null!");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the details of the specified profile.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::TafPaTeluxDataProfile::getProfileDetails
(
    SlotId slotId,
    int profileId,
    telux::data::TechPreference techPreference,
    ProfileInfo_t &profileInfo
)
{
    PA_INFO("Slot: ID: %d, Profile ID: %d", slotId, profileId);

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");

    // Create a promise to receive the async result
    auto promisePtr = std::make_shared<std::promise<std::tuple<telux::common::ErrorCode,
                                                    std::shared_ptr<telux::data::DataProfile>>>>();
    std::future<std::tuple<telux::common::ErrorCode,
                        std::shared_ptr<telux::data::DataProfile>>> fut = promisePtr->get_future();

    // Create callback with the promise
    std::shared_ptr<TafPaTeluxDataRequestProfile> callback =
                                std::make_shared<TafPaTeluxDataRequestProfile>(slotId, promisePtr);

    std::lock_guard<std::mutex> lock(dataProfileGetDetailsMutex_);

    // Call the function to get profile details.
    telux::common::Status status = dataProfileManagersMap_[slotId]->requestProfile(profileId,
                                                                        techPreference, callback);

    TAF_PA_ERROR_IF_RET_VAL(telux::common::Status::SUCCESS != status, PA_FAULT,
                                                        "requestProfile failed: %d", status);

    PA_DEBUG("Callback will be triggered...");

    // This will block until the callback is invoked or timeout occurs.
    auto waitStatus = fut.wait_for(std::chrono::seconds(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT));//15s
    //  Exit the application, if SDK is unable to initialize sensor subsystems

    TAF_PA_ERROR_IF_RET_VAL(std::future_status::timeout == waitStatus, PA_TIMEOUT,
                                                      "Timeout waiting requestProfile callback");

    auto futResult = fut.get();
    telux::common::ErrorCode error = std::get<0>(futResult);
    TAF_PA_ERROR_IF_RET_VAL(telux::common::ErrorCode::SUCCESS != error, PA_FAULT,
                                                "Failed to get profile details. Error: %d", error);

    auto dataProfile = std::get<1>(futResult);

    // Verify that the profile id matches
    TAF_PA_ERROR_IF_RET_VAL(dataProfile->getId() != profileId, PA_FAULT,
                    "Profile ID mismatch. Expected: %d, Got: %d",profileId, dataProfile->getId());

    pa_result_t result = taf::pa::data::Utils::ConvertProfileInfo(*dataProfile, profileInfo);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handle profile events from onProfileUpdate.
 */
//--------------------------------------------------------------------------------------------------
void taf::pa::data::TafPaTeluxDataProfile::PaUpdateProfileEventInfo
(
    SlotId slotId,
    int profileId,
    telux::data::ProfileChangeEvent event,
    telux::data::TechPreference techPreference
)
{
    PhoneId_e      paPhoneID;
    ProfileEvent_e paProfileEvent;
    ProfileInfo_t  profileInfo;

    taf::pa::data::SlotId_e paSlotId = taf::pa::data::Utils::ConvertSlotId(slotId);
    // Convert phone id
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(paSlotId, paPhoneID);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "PaGetPhoneIdFromSlotId err: %d. Dropping event",
                                                                                           result);

    // Convert event
    paProfileEvent = taf::pa::data::Utils::ConvertProfileChangeEvent(event);

    // Populate profile Id
    profileInfo.profileId = taf::pa::data::Utils::ConvertProfileId(profileId);

    // Return if it's an invalid event.
    TAF_PA_ERROR_IF_RET_NIL(ProfileEvent_e::UNKNOWN == paProfileEvent,
                                                        "UNKNOWN profile event. Dropping event.");
    if (ProfileEvent_e::DELETED == paProfileEvent)
    {
        PA_INFO("DELETED event. Call the callback");
        PaSendProfileEventInfoToClients(paPhoneID, paProfileEvent, profileInfo);
        return;
    }

    // UPDATED or CREATED event.
    PA_DEBUG("UPDATED or CREATED event. Launching thread to populate profile info.");
    PA_INFO("SlotId: %d, ProfileId: %d, TechPref: %d", TO_INT(slotId), profileId,
                                                                    TO_INT(techPreference));

    // Launch a thread to get profile details.
    // This is because if called from the callback context, TelSDK will not trigger the callback.
    // Launch detached thread - captures by value to ensure thread safety
    std::thread([this, slotId, profileId, techPreference,
                 paPhoneID, paProfileEvent, event]() {
        ProfileInfo_t asyncProfileInfo;
        asyncProfileInfo.profileId = taf::pa::data::Utils::ConvertProfileId(profileId);

        pa_result_t result = getProfileDetails(slotId, profileId, techPreference,
                                               asyncProfileInfo);
        if (PA_OK != result)
        {
            PA_ERROR("Profile details request failed in async thread. Dropping event.");
            return;
        }

        // Send the event to clients from this thread
        PA_INFO("%s event. Send to clients from async thread",
                taf::pa::data::Utils::ProfileChangeEventToString(event));
        PaSendProfileEventInfoToClients(paPhoneID, paProfileEvent, asyncProfileInfo);
    }).detach();

    PA_INFO("Async thread launched and detached, returning from callback thread");
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send profile event to registered clients
 */
//--------------------------------------------------------------------------------------------------
void taf::pa::data::TafPaTeluxDataProfile::PaSendProfileEventInfoToClients
(
    PhoneId_e               phoneId,
    ProfileEvent_e          event,
    const ProfileInfo_t    &profileInfo
)
{
    PA_DEBUG("Calling registered callbacks...");
    std::vector<ProfileEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataProfileCallbacksMutex_);
        localCbksCopy = profileEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(phoneId, event, profileInfo, cbk.context);
        }
        catch (const std::exception &e)
        {
            PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get profile information.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf::pa::data::TafPaTeluxDataProfile::PaGetProfileInfo
(
    PhoneId_e phoneId,
    ProfileInfo_t &profileInfo
)
{
    PA_INFO("PhoneId: %d, ProfileId: %d, TechPref: %d", TO_INT(phoneId),
                                TO_INT(profileInfo.profileId), TO_INT(profileInfo.techPref) );
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    SlotId slotId;
    SlotId_e paSlotId;
    telux::data::TechPreference techPreference;
    int profileId = -1;

    teluxPaData.PaGetSlotIdFromPhoneId(phoneId, paSlotId);
    slotId = taf::pa::data::Utils::ConvertSlotId(paSlotId);
    profileId = TO_INT(profileInfo.profileId);

    if (TechPref_e::TP_3GPP != profileInfo.techPref && TechPref_e::TP_3GPP2 != profileInfo.techPref)
    {
        PA_WARN("Using TP_3GPP");
        techPreference = telux::data::TechPreference::TP_3GPP;
    }
    else
    {
        techPreference = taf::pa::data::Utils::ConvertTechPref(profileInfo.techPref);
    }
    PA_INFO("SlotId: %d, ProfileId: %d, TechPref: %d", TO_INT(slotId), profileId,
                                                                    TO_INT(techPreference));
    return getProfileDetails(slotId, profileId, techPreference, profileInfo);
}

/**
 * Initialize the data profile managers
 */
void taf::pa::data::TafPaTeluxDataProfile::initDataProfileManagers()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    // Get the data factory
    auto &dataFactory = telux::data::DataFactory::getInstance();

    // Track slots that failed or are unavailable for summary reporting
    std::vector<int> failedSlots;
    std::vector<int> unavailableSlots;
    std::vector<int> successfulSlots;

    PA_INFO("Starting data profile manager initialization for %d slot(s)", TO_INT(slotCount_));

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        taf::pa::data::SlotId_e paSlotId = static_cast<taf::pa::data::SlotId_e>(slotId);

        // Check if already initialized
        if (taf::pa::data::SubsystemState_e::AVAILABLE ==
                                                        dataProfileManagersInitStateMap_[paSlotId])
        {
            PA_INFO("Data profile manager already initialized for slot ID: %d", slotId);
            successfulSlots.push_back(slotId);
            continue;
        }

        PA_INFO("Initializing data profile manager for slot %d...", slotId);

        // Initialize the data profile manager for each slot
        // Create shared state for synchronization using condition variable
        // Use heap-allocated shared_ptr to ensure state outlives this function scope
        struct InitState {
            std::mutex mtx;
            std::condition_variable cv;
            bool callbackReceived = false;
            telux::common::ServiceStatus status;
        };
        auto state = std::make_shared<InitState>();

        auto dataProfMgr = dataFactory.getDataProfileManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [state, slotId](telux::common::ServiceStatus svcStatus)
            {
                SET_SDK_THREAD_NAME();
                PA_INFO("getDataProfileManager callback for slot ID: %d", TO_INT(slotId));
                PA_INFO("Data profile manager status: %d", TO_INT(svcStatus));

                {
                    std::lock_guard<std::mutex> lock(state->mtx);
                    state->status = svcStatus;
                    state->callbackReceived = true;
                }
                state->cv.notify_one();
            });

        if (!dataProfMgr)
        {
            PA_ERROR("Failed to get Data Profile manager for slot %d", slotId);
            SetSubsysState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }

        // Wait for the callback with timeout
        telux::common::ServiceStatus dataServSysMgrStatus;
        PA_INFO("Waiting for Data Profile manager to be ready for slot %d...", slotId);

        {
            std::unique_lock<std::mutex> lock(state->mtx);
            bool success = state->cv.wait_for(
                lock,
                std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT),
                [&state]() { return state->callbackReceived; }
            );

            if (!success)
            {
                PA_ERROR("Timeout waiting for Data profile manager for slot %d", slotId);
                SetSubsysState(paSlotId, SubsystemState_e::FAILED);
                failedSlots.push_back(slotId);
                // Continue processing other slots instead of returning
                continue;
            }

            dataServSysMgrStatus = state->status;
        }

        PA_INFO("DataProfMgr for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));

        // Handle different service status outcomes
        if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
        {
            PA_INFO("DataProfMgr for slot %d: AVAILABLE", slotId);
            // Store the manager in the map with slot Id as index
            dataProfileManagersMap_.emplace((SlotId)slotId, dataProfMgr);
            // Add the on profiles list callback
            tafPaDataProfileListCbksMap_.emplace((SlotId)slotId,
                        std::make_shared<TafPaTeluxDataProfileListCallback>((SlotId)slotId));

            // Store the listeners in the map with slot Id as index
            auto listener = std::make_shared<TafPaTeluxDataProfileListener>((SlotId)slotId);
            tafPaTeluxDataProfListenersMap_.emplace((SlotId)slotId, listener);
            bProfileListenersRegistered_[slotId - 1] = false;
            PA_INFO("Listener for slot %d added", slotId);

            // Update that the data profile manager is initialized.
            dataProfileManagersInitStateMap_[paSlotId] = SubsystemState_e::AVAILABLE;
            successfulSlots.push_back(slotId);
            PA_INFO("Data profile manager initialization for slot %d complete", slotId);
        }
        else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
        {
            PA_WARN("DataProfMgr for slot %d: UNAVAILABLE (temporary)", slotId);
            // Mark as unavailable but don't set FAILED state - this is temporary
            // The service may become available later
            unavailableSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
        else
        {
            // Unknown/error status
            PA_ERROR("Failed to init Data profile manager for slot %d with status: %d",
                     slotId, TO_INT(dataServSysMgrStatus));
            SetSubsysState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
    }

    // Log summary of initialization results
    PA_INFO("=== Data Profile Manager Initialization Summary ===");
    PA_INFO("Total slots: %d", TO_INT(slotCount_));
    PA_INFO("Successfully initialized: %zu slot(s)", successfulSlots.size());
    if (!successfulSlots.empty())
    {
        for (auto slot : successfulSlots)
        {
            PA_INFO("  - Slot %d: AVAILABLE", slot);
        }
    }

    if (!unavailableSlots.empty())
    {
        PA_WARN("Temporarily unavailable: %zu slot(s)", unavailableSlots.size());
        for (auto slot : unavailableSlots)
        {
            PA_WARN("  - Slot %d: UNAVAILABLE (may retry later)", slot);
        }
    }

    if (!failedSlots.empty())
    {
        PA_ERROR("Failed to initialize: %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            PA_ERROR("  - Slot %d: FAILED", slot);
        }
    }
    PA_INFO("====================================================");

    return;
}

pa_result_t taf::pa::data::TafPaTeluxDataProfile::deInitDataProfileManagers()
{
    PA_INFO("Starting data profile managers deinitialization...");

    // Deregister callbacks with error checking
    pa_result_t result = PaDeregisterProfileCallbacks();
    if (PA_OK != result)
    {
        PA_ERROR("CRITICAL: Failed to deregister data profile callbacks!");
        PA_ERROR("Cannot safely proceed with cleanup - callbacks may still be active");
        PA_ERROR("This could lead to crashes if SDK invokes callbacks after cleanup");
        return PA_FAULT; // Do NOT clear maps if deregistration failed
    }

    PA_INFO("Callbacks successfully deregistered, proceeding with cleanup...");

    PA_INFO("Clear dataProfileManagersMap_");
    dataProfileManagersMap_.clear();

    PA_INFO("Clear tafPaDataProfileListCbksMap_");
    tafPaDataProfileListCbksMap_.clear();

    PA_INFO("Clear tafPaTeluxDataProfListenersMap_");
    tafPaTeluxDataProfListenersMap_.clear();

    dataProfileManagersInitStateMap_[SlotId_e::SLOT_1] = SubsystemState_e::FAILED;
    dataProfileManagersInitStateMap_[SlotId_e::SLOT_2] = SubsystemState_e::FAILED;

    // Clear profile events callbacks
    PA_INFO("Clear profileEventsCallbacks_");
    {
        std::unique_lock lock(dataProfileCallbacksMutex_);
        profileEventsCallbacks_.clear();
    }

    PA_INFO("Data profile managers deinitialization complete");
    return PA_OK;
}
