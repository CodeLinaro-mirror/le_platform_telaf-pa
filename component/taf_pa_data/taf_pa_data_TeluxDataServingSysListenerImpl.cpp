/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataServingSysListenerImpl.cpp
 * @brief Telux Data Serving System Listener onRoamingStatusChanged indication implementation.
 *
 */

#include "taf_pa_data_Utils.hpp"
#include "taf_pa_data_TeluxData.hpp"


taf::pa::data::TafPaTeluxDataServingSysListener::TafPaTeluxDataServingSysListener(SlotId slotId)
{
    slotId_ = slotId;
}

taf::pa::data::TafPaTeluxDataServingSysListener::~TafPaTeluxDataServingSysListener()
{

}

void taf::pa::data::TafPaTeluxDataServingSysListener::onRoamingStatusChanged
(
    telux::data::RoamingStatus status
)
{
    SET_SDK_THREAD_NAME();
    PA_DEBUG("Roaming status changed: %d", status.isRoaming);
    taf::pa::data::RoamingStatus_t eventInfo;
    auto &teluxPaData = TafPaTeluxData::GetInstance();

    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(slotId_);
    pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(eventInfo.slotId, eventInfo.phoneId);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "PaGetPhoneIdFromSlotId err: %d. Dropping event",
                                                                                           result);
    eventInfo.isRoaming = status.isRoaming;
    eventInfo.type = taf::pa::data::Utils::ConvertRoamingType(status.type);

    // Call app provided callbacks
    teluxPaData.SendRoamingEventInfoToClients(eventInfo);
}

void taf::pa::data::TafPaTeluxDataServingSysListener::onServiceStatusChange
(
    telux::common::ServiceStatus status
)
{
    SET_SDK_THREAD_NAME();
    taf::pa::data::SubsystemState_e state;
    switch (status)
    {
    case telux::common::ServiceStatus::SERVICE_AVAILABLE:
        PA_INFO("Serving System Manager for Slot ID %d: AVAILABLE", slotId_);
        state = taf::pa::data::SubsystemState_e::AVAILABLE;
        break;
    case telux::common::ServiceStatus::SERVICE_UNAVAILABLE:
        PA_ERROR("Serving System Manager for Slot ID %d: UNAVAILABLE", slotId_);
        state = taf::pa::data::SubsystemState_e::UNAVAILABLE;
        break;
    case telux::common::ServiceStatus::SERVICE_FAILED:
        PA_ERROR("Serving System Manager for Slot ID %d: FAILED", slotId_);
        state = taf::pa::data::SubsystemState_e::FAILED;
        break;
    default:
        PA_WARN("Serving System Manager for Slot ID %d: status unknown", slotId_);
        state = taf::pa::data::SubsystemState_e::FAILED;
        break;
    };

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    taf::pa::data::SlotId_e slotId = taf::pa::data::Utils::ConvertSlotId(slotId_);
    teluxPaData.SetServingSystemInitState(slotId, state, true);
}
