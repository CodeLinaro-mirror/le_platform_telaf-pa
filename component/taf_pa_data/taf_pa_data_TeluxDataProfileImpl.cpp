/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfileImpl.cpp
 * @brief Telux Data profile management
 *
 */

#include "taf_pa_data_TeluxData.hpp"
#include "taf_pa_data_TeluxDataProfile.hpp"
#include "tafSvcIF.hpp"

/**
 * Returns TafPaTeluxDataProfile instance
 */
taf::pa::data::TafPaTeluxDataProfile& taf::pa::data::TafPaTeluxDataProfile::GetInstance()
{
    static TafPaTeluxDataProfile instance;
    return instance;
}

le_result_t taf::pa::data::TafPaTeluxDataProfile::PaGetInitState(bool &initState)
{
    LE_DEBUG("Profile init state: %d", bDataProfileMngrInitialized_);
    initState = bDataProfileMngrInitialized_;
    return LE_OK;
}

void taf::pa::data::TafPaTeluxDataProfile::Init(taf::pa::data::SlotCount_e slotCount)
{
    slotCount_ = slotCount;
    initDataProfileManagers();
}

void taf::pa::data::TafPaTeluxDataProfile::Deinit()
{
    deInitDataProfileManagers();
}

le_result_t taf::pa::data::TafPaTeluxDataProfile::PaListProfiles
(
    taf::pa::data::PhoneId_e phoneId,
    taf_pa_data_profile_GetAllAsyncCb callback,
    void *contextPtr
)
{
    taf::pa::data::SlotId_e slotIdPa;
    telux::common::Status status;
    SlotId slotId;
    auto &teluxPaData = TafPaTeluxData::GetInstance();

    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIdPa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIdPa));
    slotId = taf::pa::data::Utils::ConvertSlotId(slotIdPa);

    TAF_ERROR_IF_RET_VAL(nullptr == callback, LE_BAD_PARAMETER, "callback is null");
    TAF_ERROR_IF_RET_VAL(
        tafPaDataProfileListCbksMap_[slotId]->GetListProfilesCmdInProgress(),LE_BUSY,
                                            "Command in progress for slot ID: %d", TO_INT(slotId) );

    LE_DEBUG("Calling requestProfileList.");
    tafPaDataProfileListCbksMap_[slotId]->SetListProfilesCallback(callback);
    tafPaDataProfileListCbksMap_[slotId]->SetListProfilesContext(contextPtr);
    status = dataProfileManagersMap_[slotId]->requestProfileList(
                                                            tafPaDataProfileListCbksMap_[slotId]);

    if (status != telux::common::Status::SUCCESS)
    {
        LE_WARN("requestProfileList failed: %d", TO_INT(status) );
        tafPaDataProfileListCbksMap_[slotId]->SetListProfilesCallback(nullptr);
        tafPaDataProfileListCbksMap_[slotId]->SetListProfilesContext(nullptr);
        return LE_FAULT;
    }
    LE_DEBUG("Callback will be triggered...");

    // Set the flag to indicate that the request is in progress
    tafPaDataProfileListCbksMap_[slotId]->SetListProfilesCmdInProgress(true);
    return LE_OK;
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

le_result_t taf::pa::data::TafPaTeluxDataProfile::PaCreateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    const ProfileInfo_t &profileInfo,
    taf::pa::data::ProfileId_e &profileId
)
{
    SlotId_e slotIDpa;
    telux::data::ProfileParams params;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataProfileManagersMap_.find(slotId) == dataProfileManagersMap_.end())
    {
        LE_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
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
        LE_WARN("createProfile failed: %d", TO_INT(status) );
        return LE_FAULT;
    }
    LE_DEBUG("Callback will be triggered...");

    // Wait for the callback
    std::tuple<telux::common::ErrorCode, int> resultTuple;
    FUTURE_GET_RET_VAL(fut, resultTuple, LE_FAULT);
    telux::common::ErrorCode errorCode = std::get<0>(resultTuple);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        LE_WARN("createProfile error: %d", TO_INT(errorCode) );
        return LE_FAULT;
    }
    int createdProfileId = std::get<1>(resultTuple);
    LE_DEBUG("createProfile succeeded: %d", createdProfileId);
    profileId = static_cast<ProfileId_e>( createdProfileId );
    return LE_OK;
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

le_result_t taf::pa::data::TafPaTeluxDataProfile::PaUpdateProfile
(
    taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileInfo_t &profileInfo
)
{
    SlotId_e slotIDpa;
    telux::data::ProfileParams params;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataProfileManagersMap_.find(slotId) == dataProfileManagersMap_.end())
    {
        LE_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
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
        LE_WARN("modifyProfile failed: %d", TO_INT(status));
        return LE_FAULT;
    }
    LE_DEBUG("Callback will be triggered...");

    // Wait for the callback
    telux::common::ErrorCode errorCode;
    FUTURE_GET_RET_VAL(fut, errorCode, LE_FAULT);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        LE_WARN("modifyProfile error: %d", TO_INT(errorCode));
        return LE_FAULT;
    }
    LE_DEBUG("modifyProfile succeeded");
    return LE_OK;
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

le_result_t taf::pa::data::TafPaTeluxDataProfile::PaDeleteProfile
(
    taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileInfo_t &profileInfo
)
{
    SlotId_e slotIDpa;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataProfileManagersMap_.find(slotId) == dataProfileManagersMap_.end())
    {
        LE_ERROR("Profile manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
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
        LE_WARN("deleteProfile failed: %d", TO_INT(status));
        return LE_FAULT;
    }
    LE_DEBUG("Callback will be triggered...");

    // Wait for the callback
    telux::common::ErrorCode errorCode;
    FUTURE_GET_RET_VAL(fut, errorCode, LE_FAULT);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        LE_WARN("deleteProfile error: %d", TO_INT(errorCode));
        return LE_FAULT;
    }
    LE_DEBUG("deleteProfile succeeded");
    return LE_OK;
}

/**
 * Initialize the data profile managers
 */
void taf::pa::data::TafPaTeluxDataProfile::initDataProfileManagers()
{
    if (bDataProfileMngrInitialized_)
    {
        LE_INFO("Data profile managers already initialized.");
        return;
    }
    // Get the data factory
    auto &dataFactory = telux::data::DataFactory::getInstance();
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Initialize the data profile manager for each slot
        auto dataProfilePromPtr = std::make_shared<std::promise<telux::common::ServiceStatus>>();

        auto dataProfMgr = dataFactory.getDataProfileManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [&](telux::common::ServiceStatus svcStatus)
            {
                if (bWaitingOnDataProfMngrProm_.load())
                {
                    LE_INFO("getDataProfileManager for slot ID: %d", TO_INT(slotId));
                    LE_INFO("Data profile manager       status: %d", TO_INT(svcStatus));
                    dataProfilePromPtr->set_value(svcStatus);
                }
                else
                {
                    LE_WARN("dataProfilePromPtr->set_value() not called");
                } });
        if (!dataProfMgr)
        {
            LE_ERROR("Failed to get Data Profile manager.");
            return;
        }
        else
        {
            // Mark that promise should be completed.
            bWaitingOnDataProfMngrProm_.store(true);

            telux::common::ServiceStatus dataServSysMgrStatus;
            LE_INFO("Waiting for Data Profile manager to be ready...");
            std::future<telux::common::ServiceStatus> dataProfileFut =
                                                                dataProfilePromPtr->get_future();

            std::future_status waitStatus = dataProfileFut.wait_for(
                std::chrono::seconds(taf::pa::SUBSYSTEM_INIT_TIMEOUT));
            if (std::future_status::timeout == waitStatus)
            {
                // Mark that promise completion is not needed.
                bWaitingOnDataProfMngrProm_.store(false);
                LE_ERROR("Timeout waiting for Data profile manager");
                return;
            }
            else
            {
                FUTURE_GET_RET_NIL(dataProfileFut, dataServSysMgrStatus);
                LE_INFO("DataProfMgr for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));
                // Mark that promise completion is not needed.
                bWaitingOnDataProfMngrProm_.store(false);
            }
            if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
            {
                LE_INFO("DataProfMgr for slot %d: AVAILABLE", slotId);
                // Store the manager in the map with slot Id as index
                dataProfileManagersMap_.emplace((SlotId)slotId, dataProfMgr);
                // Add the on profiles list callback
                tafPaDataProfileListCbksMap_.emplace((SlotId)slotId,
                            std::make_shared<TafPaTeluxDataProfileListCallback>((SlotId)slotId));

                // TODO
                // Store the listeners in the map with slot Id as index
                // auto listener = std::make_shared<tafPaTeluxDataProfListener>((SlotId)slotId);
                // dataProfileListenersMap_.emplace((SlotId)slotId, listener);
            }
            else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
            {
                LE_WARN("DataProfMgr for slot %d: UNAVAILABLE", slotId);
                // This is a temporary unavailability. Try after a delay
                // TODO
                return;
            }
            else
            {
                // Fatal error. TODO
                LE_ERROR("Failed to init Data profile manager for slot ID %d", slotId);
                return;
            }
        }
        LE_INFO("Data profile manager initialization for slot %d complete", slotId);
    }
    // Update that the data profile manager is initialized.
    bDataProfileMngrInitialized_ = true;
    return;
}

void taf::pa::data::TafPaTeluxDataProfile::deInitDataProfileManagers()
{
    LE_INFO("Clear dataProfileManagersMap_");
    // Clear all elements from dataProfileManagersMap_
    dataProfileManagersMap_.clear();

    // Clear all elements from tafPaDataProfileListCbksMap_
    LE_INFO("Clear tafPaDataProfileListCbksMap_");
    tafPaDataProfileListCbksMap_.clear();
}