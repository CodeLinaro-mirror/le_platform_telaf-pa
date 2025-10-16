/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataServingSysListenerImpl.cpp
 * @brief Telux Data Serving System Listener onRoamingStatusChanged indication implementation.
 *
 */

#include "legato.h"
#include "tafSvcIF.hpp"
#include "taf_pa_data_Utils.hpp"
#include "taf_pa_data_TeluxData.hpp"


taf::pa::data::tafPaTeluxDataServingSysListener::tafPaTeluxDataServingSysListener(SlotId slotId)
{
    slotId_ = slotId;
}

taf::pa::data::tafPaTeluxDataServingSysListener::~tafPaTeluxDataServingSysListener()
{

}

void taf::pa::data::tafPaTeluxDataServingSysListener::onRoamingStatusChanged
(
    telux::data::RoamingStatus status
)
{
    SET_SDK_THREAD_NAME();
    LE_DEBUG("Roaming status changed: %d", status.isRoaming);
    taf::pa::data::RoamingStatus_t eventInfo;
    auto &teluxPaData = TafPaTeluxData::GetInstance();

    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(slotId_);
    teluxPaData.PaGetPhoneIdFromSimSlotId(eventInfo.slotId, eventInfo.phoneId);
    eventInfo.isRoaming = status.isRoaming;
    eventInfo.type = taf::pa::data::Utils::ConvertRoamingType(status.type);

    // Call app provided callbacks
    teluxPaData.SendRoamingEventInfoToClients(eventInfo);
}
