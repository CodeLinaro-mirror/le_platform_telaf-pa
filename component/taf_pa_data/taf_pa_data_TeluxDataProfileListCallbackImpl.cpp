/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfileListCallbackImpl.cpp
 * @brief Telux Data profile list command implementation
 *
 */

#include "taf_pa_data_TeluxData.hpp"
#include "taf_pa_data_TeluxDataProfile.hpp"
#include <thread>
#include <sstream>

void taf::pa::data::TafPaTeluxDataProfileListCallback::SetListProfilesCmdInProgress(bool bState)
{
    bListProfilesCmdInProgress_.store(bState);
    LE_DEBUG("bListProfilesCmdInProgress_ = %s",
            bListProfilesCmdInProgress_.load() ? "true" : "false");
}

bool taf::pa::data::TafPaTeluxDataProfileListCallback::GetListProfilesCmdInProgress()
{
    LE_DEBUG("bListProfilesCmdInProgress_ = %s",
            bListProfilesCmdInProgress_.load() ? "true" : "false");
    return bListProfilesCmdInProgress_.load();
}

void taf::pa::data::TafPaTeluxDataProfileListCallback::SetListProfilesCallback
(
    taf_pa_data_profile_GetAllAsyncCb callback
)
{
    callbackListProfiles_ = callback;
}

void
taf::pa::data::TafPaTeluxDataProfileListCallback::SetListProfilesContext(void *ctxPtr)
{
    contextListProfiles_ = ctxPtr;
    LE_DEBUG("contextListProfiles_: %p", contextListProfiles_);
}

void taf::pa::data::TafPaTeluxDataProfileListCallback::onProfileListResponse(
    const std::vector<std::shared_ptr<telux::data::DataProfile>> &profiles,
    telux::common::ErrorCode error)
{
    SET_SDK_THREAD_NAME();

    taf::pa::data::PhoneId_e phoneId;
    taf::pa::data::SlotId_e slotId = taf::pa::data::Utils::ConvertSlotId(slotId_);

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    teluxPaData.PaGetPhoneIdFromSimSlotId(slotId, phoneId);
    LE_INFO("Phone ID: %d, Slot ID: %d", TO_INT(phoneId), TO_INT(slotId));

    if (telux ::common::ErrorCode::SUCCESS != error)
    {
        LE_WARN("onProfileListResponse failed: %d(%s)", TO_INT(error),
                telux::common::Utils::getErrorCodeAsString(error).c_str());
        // call the callback with error
        if (nullptr != callbackListProfiles_)
        {
            callbackListProfiles_(
                phoneId,                                     // The phone ID
                LE_FAULT,                                    // Error
                std::vector<taf::pa::data::ProfileInfo_t>(), // Empty vector
                contextListProfiles_                         // App provided context
            );
        }
        else
        {
            LE_WARN("callbackListProfiles_ is NULL");
        }
        // Reset the callback and context
        SetListProfilesCallback(nullptr);
        SetListProfilesContext(nullptr);
        // Reset call in progress flag
        SetListProfilesCmdInProgress(false);
        return;
    }
    LE_INFO("onProfileListResponse: %zu profiles", profiles.size());

    // Protect the critical section
    std::lock_guard<std::mutex> lock(profileListMutex_);

    // Convert the telux::data::DataProfile to taf::pa::data::ProfileInfo_t
    std::vector<taf::pa::data::ProfileInfo_t> profileInfos;
    for (const auto &profile : profiles)
    {
        // Check if the shared_ptr is not null
        if (profile)
        {
            taf::pa::data::ProfileInfo_t profileInfo;
            taf::pa::data::Utils::ConvertProfileInfo(*profile, profileInfo);
            profileInfos.push_back(profileInfo);
        }
        else
        {
            LE_WARN("share_ptr profile is null");
            break;
        }
    }

    // call the callback
    if (nullptr != callbackListProfiles_)
    {
        callbackListProfiles_(
            phoneId,             // The phone ID
            LE_OK,               // Success
            profileInfos,        // ProfileInfo_t vector
            contextListProfiles_ // App provided context
        );
    }
    else
    {
        LE_WARN("callbackListProfiles_ is NULL");
    }

    // Reset the callback and context
    SetListProfilesCallback(nullptr);
    SetListProfilesContext(nullptr);
    // Reset call in progress flag
    SetListProfilesCmdInProgress(false);
    return;
}
