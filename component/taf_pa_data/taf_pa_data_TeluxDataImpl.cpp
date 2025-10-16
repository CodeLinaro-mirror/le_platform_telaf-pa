/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataImpl.cpp
 * @brief Telux Data management.
 *
 */

#include "taf_pa_data_Utils.hpp"
#include "taf_pa_data_TeluxData.hpp"
#include "tafSvcIF.hpp"
#include <telux/common/DeviceConfig.hpp>
#include <chrono>
#include <future>

/**
 * @brief Constructor
 */
taf::pa::data::TafPaTeluxData::TafPaTeluxData()
{
    bWaitingOnDataSSProm_.store( false);
    bGetRoamingStatusInProgress_.store(false);
}

/**
 * Returns TafPaTeluxData instance
 */
taf::pa::data::TafPaTeluxData &taf::pa::data::TafPaTeluxData::GetInstance()
{
    static TafPaTeluxData instance;
    return instance;
}

/**
 * The init function
 */
void taf::pa::data::TafPaTeluxData::Init()
{
    // Initialize the internal event(s) and memory(s) first because if they fail, the service cannot
    // function.
    initInternalEvents();
    initPhoneManager();
    checkAndUpdateSlotCount();
    // Initialize the listeners registered map to false.
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        bDataSSLRegisteredMap_[(SlotId)slotId] = false;
    }
    initDataServingSystemManagers();
    RegisterDataServingSystemListeners();
}

/**
 * The deinit function
 */
void taf::pa::data::TafPaTeluxData::Deinit()
{
    DeregisterDataServingSystemListeners();
    deInitDataServingSystemManagers();
}

void taf::pa::data::TafPaTeluxData::initInternalEvents()
{
}

void taf::pa::data::TafPaTeluxData::initPhoneManager()
{
    auto &phoneFactory = telux::tel::PhoneFactory::getInstance();
    phoneManager_ = phoneFactory.getPhoneManager();

    // Check if telephony subsystem is ready
    bool subSystemStatus = phoneManager_->isSubsystemReady();
    if (!subSystemStatus)
    {
        LE_INFO("Wait for telephony subsystem to be ready...");
        std::future<bool> fut = phoneManager_->onSubsystemReady();
        //  Wait until the subsystem is ready.
        FUTURE_GET_RET_NIL(fut, subSystemStatus);
    }
    bDataPhoneMngrInitialized_ = true;
    LE_INFO("Phone manager initialized.");
}

void taf::pa::data::TafPaTeluxData::checkAndUpdateSlotCount()
{
    if (telux::common::DeviceConfig::isMultiSimSupported())
    {
        bMultiSimSupported_ = true;
    }
    LE_INFO("Multi-SIM supported: %s", (bMultiSimSupported_ ? "true" : "false"));

    if (bMultiSimSupported_)
    {
        slotCount_ = taf::pa::data::SlotCount_e::TWO;
    }
    else
    {
        slotCount_ = taf::pa::data::SlotCount_e::ONE;
    }

    LE_INFO("Sim slot count : %d", TO_INT(slotCount_));

    // Update the phone IDs
    telux::common::Status status = phoneManager_->getPhoneIds(phoneIds_);
    if (status != telux::common::Status::SUCCESS)
    {
        LE_ERROR("Failed to get phone IDs. Status: %d. Set to 1", TO_INT(status));
        phoneIds_.clear();
        phoneIds_.push_back(1); // Set to 1 if failed to get phone IDs.
    }
    LE_DEBUG ("Number of phones: %zu", phoneIds_.size());
    for (auto phoneId : phoneIds_)
    {
        LE_DEBUG ("Phone ID: %d", phoneId);
    }
}

le_result_t taf::pa::data::TafPaTeluxData::RegisterDataServingSystemListeners()
{
    LE_INFO("Registering Data Serving System listeners.");
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        if (false == bDataSSLRegisteredMap_[(SlotId)slotId])
        {
            telux::common::Status status = telux::common::Status::SUCCESS;
            LE_INFO("Registering SSL for slot %d", slotId);
            status = dataServingSystemManagersMap_[(SlotId)slotId]->registerListener(
                                                    dataServingSystemListenersMap_[(SlotId)slotId]);
            if (telux::common::Status::SUCCESS == status)
            {
                LE_INFO("Serving system SSL for slot ID %d registered.", slotId);
                bDataSSLRegisteredMap_[(SlotId)slotId] = true;
            }
            else
            {
                LE_ERROR("Register SSL for slot ID %d failed: %d", slotId, TO_INT(status));
            }
        }
        else
        {
            LE_INFO("SSL for slot %d already registered.", slotId);
        }
    }
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::DeregisterDataServingSystemListeners()
{
    LE_INFO("Unregistering Data Serving System listeners.");
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        if (true == bDataSSLRegisteredMap_[(SlotId)slotId])
        {
            telux::common::Status status = telux::common::Status::SUCCESS;
            LE_INFO("Registering SSL for slot %d", slotId);
            status = dataServingSystemManagersMap_[(SlotId)slotId]->deregisterListener(
                                                    dataServingSystemListenersMap_[(SlotId)slotId]);
            if (telux::common::Status::SUCCESS == status)
            {
                LE_INFO("Serving system SSL for slot ID %d unregistered.", slotId);
                bDataSSLRegisteredMap_[(SlotId)slotId] = false;
            }
            else
            {
                LE_ERROR("Unregister SSL for slot ID %d failed: %d", slotId, TO_INT(status));
            }
        }
        else
        {
            LE_INFO("SSL for slot %d not registered.", slotId);
        }
    }
    return LE_OK;
}

/**
 * Initialize the data serving system managers
 */
void taf::pa::data::TafPaTeluxData::initDataServingSystemManagers()
{
    // Get the data factory
    auto &dataFactory = telux::data::DataFactory::getInstance();
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Initialize the data serving system manager for each slot
        auto dataSSPromPtr = std::make_shared<std::promise<telux::common::ServiceStatus>>();

        auto dataSSMgr = dataFactory.getServingSystemManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [&](telux::common::ServiceStatus svcStatus)
            {
                    LE_INFO ("getServingSystemManager for slot ID: %d", TO_INT(slotId));
                    LE_INFO ("Data ServingSystem subsystem status: %d", TO_INT(svcStatus));
                    if (bWaitingOnDataSSProm_.load())
                    {
                        dataSSPromPtr->set_value(svcStatus);
                    }
                    else
                    {
                        LE_WARN("dataSSPromPtr->set_value() not called");
                    } });
        if (!dataSSMgr)
        {
            LE_ERROR("Failed to get Data Serving System manager.");
            return;
        }
        else
        {
            // Mark that promise should be completed.
            bWaitingOnDataSSProm_.store(true);

            telux::common::ServiceStatus dataServSysMgrStatus;
            LE_INFO("Wait for Data Serving System subsystem to be ready...");
            std::future<telux::common::ServiceStatus> dataSSFut = dataSSPromPtr->get_future();

            std::future_status waitStatus = dataSSFut.wait_for
                                    (
                                        std::chrono::seconds(taf::pa::SUBSYSTEM_INIT_TIMEOUT)
                                    );
            if (std::future_status::timeout == waitStatus)
            {
                // Mark that promise completion is not needed.
                bWaitingOnDataSSProm_.store(false);
                LE_ERROR("Timeout waiting for Data Serving System subsystem");
                return;
            }
            else
            {
                FUTURE_GET_RET_NIL(dataSSFut, dataServSysMgrStatus);
                LE_INFO("DSS for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));
                // Mark that promise completion is not needed.
                bWaitingOnDataSSProm_.store(false);
            }
            if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
            {
                LE_INFO("DSS for slot %d: AVAILABLE", slotId);
                // Store the manager in the map with slot Id as index
                dataServingSystemManagersMap_.emplace((SlotId)slotId, dataSSMgr);

                // Store the listeners in the map with slot Id as index
                auto listener = std::make_shared<tafPaTeluxDataServingSysListener>((SlotId)slotId);
                dataServingSystemListenersMap_.emplace((SlotId)slotId, listener);
            }
            else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
            {
                LE_WARN("DSS for slot %d: UNAVAILABLE", slotId);
                // This is a temporary unavailability. Try after a delay
                // TODO
                return;
            }
            else
            {
                // Exit the service
                LE_FATAL("Failed to init Data Serving subsystem for slot ID %d", slotId);
            }
        }
        LE_INFO("Data Serving System subsystem initialization for slot %d complete", slotId);
    }
    // Update that the SSL manager is initialized.
    bDataServSysSMngrInitialized_ = true;
    LE_DEBUG("Initialization state updated: %d", bDataServSysSMngrInitialized_);
    return;
}

void taf::pa::data::TafPaTeluxData::deInitDataServingSystemManagers()
{
    LE_INFO("Clear dataServingSystemListenersMap_");
    dataServingSystemListenersMap_.clear();
    LE_INFO("Clear dataServingSystemManagersMap_");
    dataServingSystemManagersMap_.clear();
}

le_result_t taf::pa::data::TafPaTeluxData::PaGetInitState(
    bool &state)
{
    LE_DEBUG("Phone Manager state               : %d", bDataPhoneMngrInitialized_);
    LE_DEBUG("Data Serving System Listener state: %d", bDataServSysSMngrInitialized_);
    state = bDataPhoneMngrInitialized_ && bDataServSysSMngrInitialized_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::PaGetSimSlotCount(taf::pa::data::SlotCount_e &count)
{
    count = slotCount_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::PaGetPhoneIds
(
    std::vector<taf::pa::data::PhoneId_e> &phoneIds
)
{
    for (int id : phoneIds_)
    {
        LE_DEBUG("Phone Id: %d", id);
        phoneIds.push_back(static_cast<taf::pa::data::PhoneId_e>(id));
    }
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::PaGetPhoneIdFromSimSlotId
(
    taf::pa::data::SlotId_e slotID,
    taf::pa::data::PhoneId_e &phoneID
)
{
    phoneID = static_cast<taf::pa::data::PhoneId_e> (
                                    phoneManager_->getPhoneIdFromSlotId(static_cast<int>(slotID)));
    LE_DEBUG("Slot ID %d = Phone Id: %d", TO_INT(slotID), TO_INT(phoneID));
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::PaGetSimSlotIdFromPhoneId
(
    taf::pa::data::PhoneId_e phoneID,
    taf::pa::data::SlotId_e &slotID
)
{
    slotID = static_cast<taf::pa::data::SlotId_e> (
                                    phoneManager_->getSlotIdFromPhoneId(static_cast<int>(phoneID)));
    LE_DEBUG("Phone Id: %d = Slot ID %d", TO_INT(slotID), TO_INT(phoneID));
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::PaAddRoamingEventsCallback
(
    taf_pa_data_RoamingEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_ERROR_IF_RET_VAL(nullptr == callBack, LE_BAD_PARAMETER, "callBack is NULL!");
    // Lock
    std::lock_guard<std::mutex> lock(roamingEventCbksMtx_);
    // Add the callback
    RoamingEventsCallbackEntry_t entry = {roamingEventsCallbackId_, callBack, context, };
    roamingEventsCallbacks_.push_back(entry);
    // Increment the ID.
    roamingEventsCallbackId_++;
    // Give ID back to app
    id = roamingEventsCallbackId_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxData::PaRemoveRoamingEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    // Lock
    std::lock_guard<std::mutex> lock(roamingEventCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = roamingEventsCallbacks_.begin(); cbk != roamingEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            roamingEventsCallbacks_.erase(cbk);
            return LE_OK;
        }
    }
    LE_WARN("Callback not found. Id: %d", id);
    return LE_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxData::SendRoamingEventInfoToClients
(
    const taf::pa::data::RoamingStatus_t &eventInfo
)
{
    LE_DEBUG("Calling registered callbacks...");
    // Call the service callback(s)
    for (auto &cbk : roamingEventsCallbacks_)
    {
        LE_DEBUG("Calling callback: %d", cbk.id);
        cbk.callBack(eventInfo, cbk.context);
    }
}

le_result_t taf::pa::data::TafPaTeluxData::PaGetRoamingStatus
(
    const PhoneId_e phoneId,
    RoamingStatus_t &roamingStatus
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    TAF_ERROR_IF_RET_VAL(bGetRoamingStatusInProgress_.load(), LE_BUSY, "Command in progress.");

    le_result_t result = PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataServingSystemManagersMap_.find(slotId) == dataServingSystemManagersMap_.end())
    {
        LE_ERROR("Serving system manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
    }

    // Lock
    std::lock_guard<std::mutex> lock(getRoamingStatusMtx_);
    // Set the promise
    roamingStatusPromise_ =
                std::promise<std::pair<telux::data::RoamingStatus, telux::common::ErrorCode>>();
    auto fut = roamingStatusPromise_.get_future();

    // Request
    status = dataServingSystemManagersMap_[slotId]->requestRoamingStatus
                    (
                        [this]
                        (
                            telux::data::RoamingStatus roamingStatus,
                            telux::common::ErrorCode error
                        )
                        {
                            SET_SDK_THREAD_NAME();
                            roamingStatusPromise_.set_value(std::make_pair(roamingStatus, error));
                        }
                    );
    if (telux::common::Status::SUCCESS != status)
    {
        LE_ERROR("requestRoamingStatus failed. Status: %d", TO_INT(status));
        return LE_FAULT;
    }

    LE_DEBUG("Waiting for callback...");
    // Set the in progress flag
    bGetRoamingStatusInProgress_.store(true);
    // Wait for the response
    std::pair<telux::data::RoamingStatus, telux::common::ErrorCode> futRsp;
    FUTURE_GET_RET_VAL(fut, futRsp, LE_FAULT);
    telux::common::ErrorCode futResult = futRsp.second;
    if (telux::common::ErrorCode::SUCCESS != futResult)
    {
        LE_ERROR("requestRoamingStatusCb failed. Status: %d", TO_INT(futResult));
        // Reset the in progress flag
        bGetRoamingStatusInProgress_.store(false);
        return LE_FAULT;
    }
    LE_DEBUG("success.");
    roamingStatus.isRoaming = futRsp.first.isRoaming;
    roamingStatus.type      = taf::pa::data::Utils::ConvertRoamingType(futRsp.first.type);
    roamingStatus.slotId    = slotIDpa;
    roamingStatus.phoneId   = phoneId;

    // Reset the in progress flag
    bGetRoamingStatusInProgress_.store(false);
    return LE_OK;
}
