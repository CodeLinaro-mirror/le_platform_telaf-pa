/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfileListenerImpl.hpp
 * @brief Telux Data profile listener implementation.
 *
 */

#include "tafDataUtilsPa.hpp"
#include "tafDataTeluxDataProfilePa.hpp"

void taf::pa::data::TafPaTeluxDataProfileListener::onServiceStatusChange
(
    telux::common::ServiceStatus status
)
{
    SET_SDK_THREAD_NAME();
    SubsystemState_e sState;
    switch (status)
    {
    case telux::common::ServiceStatus::SERVICE_AVAILABLE:
        TAF_PA_INFO("Profile Manager for Slot ID %d: AVAILABLE", slotId_);
        sState = SubsystemState_e::AVAILABLE;
        break;
    case telux::common::ServiceStatus::SERVICE_UNAVAILABLE:
        TAF_PA_ERROR("Profile Manager for Slot ID %d: UNAVAILABLE", slotId_);
        sState = SubsystemState_e::UNAVAILABLE;
        break;
    case telux::common::ServiceStatus::SERVICE_FAILED:
        TAF_PA_ERROR("Profile Manager for Slot ID %d: FAILED", slotId_);
        sState = SubsystemState_e::FAILED;
        break;
    default:
        TAF_PA_WARN("Profile Manager for Slot ID %d: status unknown", slotId_);
        sState = SubsystemState_e::FAILED;
        break;
    };
    SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(slotId_);
    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    teluxPaDataProfile.SetSubsysState(slotIdPa, sState, true);
}

void taf::pa::data::TafPaTeluxDataProfileListener::onProfileUpdate
(
    int profileId,
    telux::data::TechPreference techPreference,
    telux::data::ProfileChangeEvent event)
{
    SET_SDK_THREAD_NAME();

    TAF_PA_INFO("Slot: %d, Profile: %d, TechPref: %s, Event: %s", slotId_, profileId,
            taf::pa::data::Utils::TechPreferenceToString(techPreference),
            taf::pa::data::Utils::ProfileChangeEventToString(event));

    auto &teluxPaDataProfile = taf::pa::data::TafPaTeluxDataProfile::GetInstance();
    return teluxPaDataProfile.PaUpdateProfileEventInfo(slotId_, profileId, event, techPreference);
}
