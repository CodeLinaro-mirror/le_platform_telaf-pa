/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfileListCallbackImpl.cpp
 * @brief Telux Data profile list command implementation
 *
 */

#include "tafDataTeluxDataPa.hpp"
#include "tafDataTeluxDataProfilePa.hpp"
#include <thread>
#include <sstream>

void taf::pa::data::TafPaTeluxDataProfileListCallback::SetListProfilesCmdInProgress(bool bState)
{
    bListProfilesCmdInProgress_.store(bState);
    TAF_PA_DEBUG("bListProfilesCmdInProgress_ = %s",
            bListProfilesCmdInProgress_.load() ? "true" : "false");
}

bool taf::pa::data::TafPaTeluxDataProfileListCallback::GetListProfilesCmdInProgress()
{
    TAF_PA_DEBUG("bListProfilesCmdInProgress_ = %s",
            bListProfilesCmdInProgress_.load() ? "true" : "false");
    return bListProfilesCmdInProgress_.load();
}

// atomically transition bListProfilesCmdInProgress_ from false→true using
// compare_exchange_strong, collapsing the separate check-then-set into a single indivisible
// operation so two concurrent NB threads cannot both pass the "not busy" guard.
// Returns true  → caller acquired the token (flag was false, now true; proceed).
// Returns false → flag was already true; caller must return TAF_PA_BUSY.
bool taf::pa::data::TafPaTeluxDataProfileListCallback::TryAcquireListProfilesCmd()
{
    bool expected = false;
    bool acquired = bListProfilesCmdInProgress_.compare_exchange_strong(expected, true);
    TAF_PA_DEBUG("TryAcquireListProfilesCmd: %s", acquired ? "acquired" : "busy");
    return acquired;
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
    TAF_PA_DEBUG("contextListProfiles_: %p", contextListProfiles_);
}

// set both fields atomically under profileListMutex_ so the SB callback
// can never observe a partially-updated pair (new callback with stale/null context).
void taf::pa::data::TafPaTeluxDataProfileListCallback::SetListProfilesCallbackAndContext(
    taf_pa_data_profile_GetAllAsyncCb callback, void *ctxPtr)
{
    std::lock_guard<std::mutex> lock(profileListMutex_);
    callbackListProfiles_ = callback;
    contextListProfiles_  = ctxPtr;
    TAF_PA_DEBUG("contextListProfiles_: %p", contextListProfiles_);
}

void taf::pa::data::TafPaTeluxDataProfileListCallback::onProfileListResponse(
    const std::vector<std::shared_ptr<telux::data::DataProfile>> &profiles,
    telux::common::ErrorCode error)
{
    SET_SDK_THREAD_NAME();

    taf::pa::data::PhoneId_e phoneId;
    taf::pa::data::SlotId_e slotId = taf::pa::data::Utils::ConvertSlotId(slotId_);

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    taf_pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(slotId, phoneId);
    TAF_PA_ERROR_IF_RET_NIL(TAF_PA_OK != result, "PaGetPhoneIdFromSlotId err: %d. Dropping event",
                                                                                         result);
    TAF_PA_INFO("Phone ID: %d, Slot ID: %d", TO_INT(phoneId), TO_INT(slotId));

    // copy both callback and context atomically under profileListMutex_ before
    // any use.  This prevents observing a partially-updated pair written by PaListProfiles()
    // on the NB side (new callback with stale/null context, or vice versa).  The lock is
    // released before invoking the callback to avoid holding it during the user's handler.
    taf_pa_data_profile_GetAllAsyncCb localCallback;
    void *localContext;
    {
        std::lock_guard<std::mutex> lock(profileListMutex_);
        localCallback = callbackListProfiles_;
        localContext  = contextListProfiles_;
    }

    if (telux::common::ErrorCode::SUCCESS != error)
    {
        TAF_PA_WARN("onProfileListResponse failed: %d(%s)", TO_INT(error),
                telux::common::Utils::getErrorCodeAsString(error).c_str());
        // call the callback with error (outside the lock)
        if (nullptr != localCallback)
        {
            localCallback(
                phoneId,                                     // The phone ID
                TAF_PA_FAULT,                                    // Error
                std::vector<taf::pa::data::ProfileInfo_t>(), // Empty vector
                localContext                                 // App provided context
            );
        }
        else
        {
            TAF_PA_WARN("callbackListProfiles_ is NULL");
        }
        // Reset both fields atomically
        SetListProfilesCallbackAndContext(nullptr, nullptr);
        // Reset call in progress flag
        SetListProfilesCmdInProgress(false);
        return;
    }
    TAF_PA_INFO("onProfileListResponse: %zu profiles", profiles.size());

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
            TAF_PA_WARN("share_ptr profile is null");
            break;
        }
    }

    // call the callback outside the lock
    if (nullptr != localCallback)
    {
        localCallback(
            phoneId,      // The phone ID
            TAF_PA_OK,        // Success
            profileInfos, // ProfileInfo_t vector
            localContext  // App provided context
        );
    }
    else
    {
        TAF_PA_WARN("callbackListProfiles_ is NULL");
    }

    // Reset both fields atomically
    SetListProfilesCallbackAndContext(nullptr, nullptr);
    // Reset call in progress flag
    SetListProfilesCmdInProgress(false);
    return;
}
