/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataImpl.cpp
 * @brief Telux Data management.
 *
 */

#include "tafCommonDefinesPa.hpp"
#include "tafDataUtilsPa.hpp"
#include "tafDataTeluxDataPa.hpp"
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
    taf_pa_result_t result = deInitDataServingSystemManagers();
    TAF_PA_ERROR_IF_RET_NIL(TAF_PA_OK != result, "deInitDataServingSystemManagers failed");
}

void taf::pa::data::TafPaTeluxData::initPhoneManager()
{
    TAF_PA_INFO("Initialize phone manager");
    auto &phoneFactory = telux::tel::PhoneFactory::getInstance();

    phoneManager_ = phoneFactory.getPhoneManager();
    if (!phoneManager_)
    {
        TAF_PA_ERROR("Failed to get Phone Manager instance");
        return;
    }

    telux::common::ServiceStatus phoneManagerStatus = phoneManager_->getServiceStatus();

    if (phoneManagerStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_INFO("Phone Manager subsystem is not ready, waiting for it to be ready...");

        auto phoneMgrPromPtr =
            std::make_shared<std::promise<telux::common::ServiceStatus>>();

        phoneManager_ = phoneFactory.getPhoneManager(
            [phoneMgrPromPtr](telux::common::ServiceStatus status)
            {
                TAF_PA_INFO("Getting status:%d from phone manager", static_cast<int>(status));
                try {
                    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                        phoneMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_AVAILABLE);
                    } else {
                        phoneMgrPromPtr->set_value(
                            telux::common::ServiceStatus::SERVICE_FAILED);
                    }
                } catch (const std::exception &e) {
                    TAF_PA_ERROR("Exception setting phone manager promise: %s", e.what());
                } catch (...) {
                    TAF_PA_ERROR("Unknown error setting phone manager promise");
                }
            });

        if (!phoneManager_)
        {
            TAF_PA_ERROR("Failed to get Phone Manager instance with init callback");
            return;
        }

        std::future<telux::common::ServiceStatus> initFuture =
            phoneMgrPromPtr->get_future();
        std::future_status waitStatus =
            initFuture.wait_for(std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT));

        if (std::future_status::timeout == waitStatus)
        {
            TAF_PA_ERROR("Timeout waiting for Phone Manager subsystem");
            return;
        }
        else
        {
            phoneManagerStatus = initFuture.get();
        }
    }

    TAF_PA_INFO("Phone Manager status: %d", TO_INT(phoneManagerStatus));

    if (telux::common::ServiceStatus::SERVICE_AVAILABLE == phoneManagerStatus)
    {
        dataPhoneMngrInitState_ = SubsystemState_e::AVAILABLE;
        TAF_PA_INFO("Phone manager initialized.");
    }
    else
    {
        TAF_PA_ERROR("Phone Manager subsystem not available. Status: %d", TO_INT(phoneManagerStatus));
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::GetServinSystemInitState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e &sState
)
{
    std::shared_lock<std::shared_mutex> lock(servingSystemStateMapMtx_);
    TAF_PA_INFO("Serving system init state for slot id[%d]: %d", slotId,
                                            servingSystemManagersInitStateMap_[slotId]);
    sState = servingSystemManagersInitStateMap_[slotId];
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::SetServingSystemInitState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e sState,
    bool bSendEvent
)
{
    {
        std::unique_lock<std::shared_mutex> lock(servingSystemStateMapMtx_);
        servingSystemManagersInitStateMap_[slotId] = sState;
    }
    TAF_PA_INFO("Serving system init state for slot id[%d]: %d", slotId, TO_INT(sState));

    if (bSendEvent)
    {
        TAF_PA_INFO("Send event to clients.");
        SubsystemEvent_t event;
        taf_pa_result_t result = PaGetPhoneIdFromSlotId(slotId, event.phoneId);
        TAF_PA_ERROR_IF_RET_VAL(TAF_PA_OK != result, result, "PaGetPhoneIdFromSlotId err: %d", result);
        event.subsystem      = Subsystem_e::SERVING_SYSTEM_MANAGER;
        event.subsystemState = sState;
        SendSubsystemEventToClients(event);
    }
    return TAF_PA_OK;
}

void taf::pa::data::TafPaTeluxData::checkAndUpdateSlotCount()
{
    if (telux::common::DeviceConfig::isMultiSimSupported())
    {
        bMultiSimSupported_ = true;
    }
    TAF_PA_INFO("Multi-SIM supported: %s", (bMultiSimSupported_ ? "true" : "false"));

    if (bMultiSimSupported_)
    {
        slotCount_ = taf::pa::data::SlotCount_e::TWO;
    }
    else
    {
        slotCount_ = taf::pa::data::SlotCount_e::ONE;
    }

    TAF_PA_INFO("Sim slot count : %d", TO_INT(slotCount_));

    // Update the phone IDs
    telux::common::Status status = phoneManager_->getPhoneIds(phoneIds_);
    if (status != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to get phone IDs. Status: %d. Set to 1", TO_INT(status));
        phoneIds_.clear();
        phoneIds_.push_back(1); // Set to 1 if failed to get phone IDs.
    }
    TAF_PA_DEBUG ("Number of phones: %zu", phoneIds_.size());
    for (auto phoneId : phoneIds_)
    {
        TAF_PA_DEBUG ("Phone ID: %d", phoneId);
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::RegisterDataServingSystemListeners()
{
    TAF_PA_INFO("Registering Data Serving System listeners.");
    bool allSuccess = true;
    std::vector<int> failedSlots;

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        if (false == bDataSSLRegisteredMap_[(SlotId)slotId])
        {
            SubsystemState_e subsysState;
            {
                std::shared_lock<std::shared_mutex> lock(servingSystemStateMapMtx_);
                subsysState = servingSystemManagersInitStateMap_[static_cast<SlotId_e>(slotId)];
            }
            if (SubsystemState_e::AVAILABLE != subsysState)
            {
                TAF_PA_ERROR("Subsystem not initialized for slot id: %d", slotId);
                allSuccess = false;
                failedSlots.push_back(slotId);
                // Go to the next slot if available
                continue;
            }
            telux::common::Status status = telux::common::Status::SUCCESS;
            TAF_PA_INFO("Registering SSL for slot %d", slotId);
            status = dataServingSystemManagersMap_[(SlotId)slotId]->registerListener(
                                                    dataServingSystemListenersMap_[(SlotId)slotId]);
            if (telux::common::Status::SUCCESS == status)
            {
                TAF_PA_INFO("Serving system SSL for slot ID %d registered.", slotId);
                bDataSSLRegisteredMap_[(SlotId)slotId] = true;
            }
            else
            {
                TAF_PA_ERROR("Register SSL for slot ID %d failed: %d", slotId, TO_INT(status));
                allSuccess = false;
                failedSlots.push_back(slotId);
            }
        }
        else
        {
            TAF_PA_INFO("SSL for slot %d already registered.", slotId);
        }
    }
    if (!allSuccess)
    {
        TAF_PA_ERROR("=== Registration Failed ===");
        TAF_PA_ERROR("Failed to register listeners for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_ERROR("  - Slot %d: REGISTRATION FAILED", slot);
        }
        TAF_PA_ERROR("============================");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("All serving system listeners successfully registered");
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::DeregisterDataServingSystemListeners()
{
    TAF_PA_INFO("Unregistering Data Serving System listeners.");

    bool allSuccess = true;
    std::vector<int> failedSlots;

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        if (true == bDataSSLRegisteredMap_[(SlotId)slotId])
        {
            TAF_PA_INFO("Deregistering SSL for slot %d", slotId);
            telux::common::Status status = dataServingSystemManagersMap_[(SlotId)slotId]->
                deregisterListener(dataServingSystemListenersMap_[(SlotId)slotId]);

            if (telux::common::Status::SUCCESS == status)
            {
                TAF_PA_INFO("Serving system SSL for slot ID %d unregistered.", slotId);
                bDataSSLRegisteredMap_[(SlotId)slotId] = false;
            }
            else
            {
                TAF_PA_ERROR("FAILED to unregister SSL for slot ID %d. Status: %d",
                         slotId, TO_INT(status));
                failedSlots.push_back(slotId);
                allSuccess = false;
            }
        }
        else
        {
            TAF_PA_INFO("SSL for slot %d not registered.", slotId);
        }
    }

    if (!allSuccess)
    {
        TAF_PA_ERROR("=== Deregistration Failed ===");
        TAF_PA_ERROR("Failed to deregister listeners for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_ERROR("  - Slot %d: DEREGISTRATION FAILED", slot);
        }
        TAF_PA_ERROR("============================");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("All serving system listeners successfully deregistered");
    return TAF_PA_OK;
}

/**
 * Initialize the data serving system managers
 */
void taf::pa::data::TafPaTeluxData::initDataServingSystemManagers()
{
    // Get the data factory
    auto &dataFactory = telux::data::DataFactory::getInstance();

    // Track slots that failed or are unavailable for summary reporting
    std::vector<int> failedSlots;
    std::vector<int> unavailableSlots;
    std::vector<int> successfulSlots;

    TAF_PA_INFO("Starting data serving system manager initialization for %d slot(s)", TO_INT(slotCount_));

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        taf::pa::data::SlotId_e paSlotId = static_cast<taf::pa::data::SlotId_e>(slotId);

        // Check if already initialized
        SubsystemState_e initCheckState;
        {
            std::shared_lock<std::shared_mutex> lock(servingSystemStateMapMtx_);
            initCheckState = servingSystemManagersInitStateMap_[paSlotId];
        }
        if (taf::pa::data::SubsystemState_e::AVAILABLE == initCheckState)
        {
            TAF_PA_INFO("Data serving system manager already initialized for slot ID %d.", slotId);
            successfulSlots.push_back(slotId);
            continue;
        }

        TAF_PA_INFO("Initializing data serving system manager for slot %d...", slotId);

        // Initialize the data serving system manager for each slot
        // Create shared state for synchronization using condition variable
        // Use heap-allocated shared_ptr to ensure state outlives this function scope
        struct InitState {
            std::mutex mtx;
            std::condition_variable cv;
            bool callbackReceived = false;
            telux::common::ServiceStatus status;
        };
        auto state = std::make_shared<InitState>();

        auto dataSSMgr = dataFactory.getServingSystemManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [state, slotId](telux::common::ServiceStatus svcStatus)
            {
                SET_SDK_THREAD_NAME();
                TAF_PA_INFO("getServingSystemManager callback for slot ID: %d", TO_INT(slotId));
                TAF_PA_INFO("Data ServingSystem subsystem status: %d", TO_INT(svcStatus));

                {
                    std::lock_guard<std::mutex> lock(state->mtx);
                    state->status = svcStatus;
                    state->callbackReceived = true;
                }
                state->cv.notify_one();
            });

        if (!dataSSMgr)
        {
            TAF_PA_ERROR("Failed to get Data Serving System manager for slot %d", slotId);
            SetServingSystemInitState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }

        // Wait for the callback with timeout
        telux::common::ServiceStatus dataServSysMgrStatus;
        TAF_PA_INFO("Waiting for Data Serving System subsystem to be ready for slot %d...", slotId);

        {
            std::unique_lock<std::mutex> lock(state->mtx);
            bool success = state->cv.wait_for(
                lock,
                std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT),
                [&state]() { return state->callbackReceived; }
            );

            if (!success)
            {
                TAF_PA_ERROR("Timeout waiting for Data Serving System subsystem for slot %d", slotId);
                SetServingSystemInitState(paSlotId, SubsystemState_e::FAILED);
                failedSlots.push_back(slotId);
                // Continue processing other slots instead of returning
                continue;
            }

            dataServSysMgrStatus = state->status;
        }

        TAF_PA_INFO("DSS for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));

        // Handle different service status outcomes
        if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
        {
            TAF_PA_INFO("DSS for slot %d: AVAILABLE", slotId);
            // Store the manager in the map with slot Id as index
            dataServingSystemManagersMap_.emplace((SlotId)slotId, dataSSMgr);

            // Store the listeners in the map with slot Id as index
            auto listener = std::make_shared<TafPaTeluxDataServingSysListener>((SlotId)slotId);
            dataServingSystemListenersMap_.emplace((SlotId)slotId, listener);

            // Update that the SSL manager is initialized.
            SetServingSystemInitState(paSlotId, SubsystemState_e::AVAILABLE);
            successfulSlots.push_back(slotId);
            TAF_PA_INFO("Data Serving System subsystem initialization for slot %d complete", slotId);
        }
        else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
        {
            TAF_PA_WARN("DSS for slot %d: UNAVAILABLE (temporary)", slotId);
            // Mark as unavailable but don't set FAILED state - this is temporary
            // The service may become available later
            unavailableSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
        else
        {
            // Unknown/error status
            TAF_PA_ERROR("Failed to init Data Serving subsystem for slot %d with status: %d",
                     slotId, TO_INT(dataServSysMgrStatus));
            SetServingSystemInitState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
    }

    // Log summary of initialization results
    TAF_PA_INFO("=== Data Serving System Manager Initialization Summary ===");
    TAF_PA_INFO("Total slots: %d", TO_INT(slotCount_));
    TAF_PA_INFO("Successfully initialized: %zu slot(s)", successfulSlots.size());
    if (!successfulSlots.empty())
    {
        for (auto slot : successfulSlots)
        {
            TAF_PA_INFO("  - Slot %d: AVAILABLE", slot);
        }
    }

    if (!unavailableSlots.empty())
    {
        TAF_PA_WARN("Temporarily unavailable: %zu slot(s)", unavailableSlots.size());
        for (auto slot : unavailableSlots)
        {
            TAF_PA_WARN("  - Slot %d: UNAVAILABLE (may retry later)", slot);
        }
    }

    if (!failedSlots.empty())
    {
        TAF_PA_ERROR("Failed to initialize: %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            TAF_PA_ERROR("  - Slot %d: FAILED", slot);
        }
    }
    TAF_PA_INFO("===========================================================");

    return;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::deInitDataServingSystemManagers()
{
    TAF_PA_INFO("Starting data serving system managers deinitialization...");

    // Deregister callbacks with error checking
    taf_pa_result_t result = DeregisterDataServingSystemListeners();
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("CRITICAL: Failed to deregister serving system listeners!");
        TAF_PA_ERROR("Cannot safely proceed with cleanup - callbacks may still be active");
        TAF_PA_ERROR("This could lead to crashes if SDK invokes callbacks after cleanup");
        return TAF_PA_FAULT; // Do NOT clear maps if deregistration failed
    }

    TAF_PA_INFO("Callbacks successfully deregistered, proceeding with cleanup...");

    TAF_PA_INFO("Clear dataServingSystemListenersMap_");
    dataServingSystemListenersMap_.clear();

    TAF_PA_INFO("Clear dataServingSystemManagersMap_");
    dataServingSystemManagersMap_.clear();

    {
        std::unique_lock<std::shared_mutex> lock(servingSystemStateMapMtx_);
        servingSystemManagersInitStateMap_[SlotId_e::SLOT_1] = SubsystemState_e::FAILED;
        servingSystemManagersInitStateMap_[SlotId_e::SLOT_2] = SubsystemState_e::FAILED;
    }

    // Clear subsystem state change event callbacks
    TAF_PA_INFO("Clear subsystemEventsCallbacks_");
    {
        std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
        subsystemEventsCallbacks_.clear();
    }

    // Clear roaming event callbacks
    TAF_PA_INFO("Clear roamingEventsCallbacks_");
    {
        std::lock_guard<std::mutex> lock(roamingEventsCbksMtx_);
        roamingEventsCallbacks_.clear();
    }

    // Clear phone IDs vector and serving-system listener registration tracking map
    TAF_PA_INFO("Clear phoneIds_ and bDataSSLRegisteredMap_");
    phoneIds_.clear();
    bDataSSLRegisteredMap_.clear();

    // Reset phone manager shared pointer and its init state so that any post-deinit
    // call that checks dataPhoneMngrInitState_ will correctly see FAILED.
    TAF_PA_INFO("Reset phoneManager_ and dataPhoneMngrInitState_");
    phoneManager_.reset();
    dataPhoneMngrInitState_ = SubsystemState_e::FAILED;

    TAF_PA_INFO("Data serving system managers deinitialization complete");
    return TAF_PA_OK;
}

taf::pa::data::SubsystemState_e taf::pa::data::TafPaTeluxData::PaGetPhoneManagerInitState()
{
    TAF_PA_DEBUG("Phone Manager state : %d", dataPhoneMngrInitState_);
    return dataPhoneMngrInitState_;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetServingSystemInitState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e &sState
)
{
    // Initialize to FAILED state
    sState = taf::pa::data::SubsystemState_e::FAILED;

    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    {
        std::shared_lock<std::shared_mutex> lock(servingSystemStateMapMtx_);
        sState = servingSystemManagersInitStateMap_[slotId];
    }
    TAF_PA_DEBUG("Serving system init state for slot id[%d]: %d", slotId,TO_INT(sState));
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetSimSlotCount(taf::pa::data::SlotCount_e &count)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    count = slotCount_;
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetPhoneIds
(
    std::vector<taf::pa::data::PhoneId_e> &phoneIds
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    phoneIds.clear();
    for (int id : phoneIds_)
    {
        TAF_PA_DEBUG("Phone Id: %d", id);
        phoneIds.push_back(static_cast<taf::pa::data::PhoneId_e>(id));
    }
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetPhoneIdFromSlotId
(
    const taf::pa::data::SlotId_e slotId,
    taf::pa::data::PhoneId_e &phoneID
)
{

    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");

    // Use cached phoneIds_ vector instead of SDK API to avoid timing issues
    // during initialization when SDK mapping may not be ready yet
    int slotIdInt = static_cast<int>(slotId);
    if (slotIdInt < 1 || slotIdInt > static_cast<int>(phoneIds_.size()))
    {
        TAF_PA_ERROR("Invalid slot ID %d (valid range: 1-%zu)", slotIdInt, phoneIds_.size());
        phoneID = static_cast<taf::pa::data::PhoneId_e>(255); // UNKNOWN
        return TAF_PA_BAD_PARAMETER;
    }

    // Map slot ID to phoneIds_ vector index (slot 1 -> index 0, slot 2 -> index 1)
    phoneID = static_cast<taf::pa::data::PhoneId_e>(phoneIds_[slotIdInt - 1]);
    TAF_PA_DEBUG("Slot ID %d = Phone Id: %d (from cached phoneIds_)", slotIdInt, TO_INT(phoneID));
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetPhoneIdFromSlotId
(
    const SlotId slotId,
    PhoneId_e &phoneID
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");

    // Use cached phoneIds_ vector instead of SDK API to avoid timing issues
    // during initialization when SDK mapping may not be ready yet
    if (slotId < 1 || slotId > static_cast<int>(phoneIds_.size()))
    {
        TAF_PA_ERROR("Invalid slot ID %d (valid range: 1-%zu)", slotId, phoneIds_.size());
        phoneID = static_cast<taf::pa::data::PhoneId_e>(255); // UNKNOWN
        return TAF_PA_BAD_PARAMETER;
    }

    // Map slot ID to phoneIds_ vector index (slot 1 -> index 0, slot 2 -> index 1)
    phoneID = static_cast<taf::pa::data::PhoneId_e>(phoneIds_[slotId - 1]);
    TAF_PA_DEBUG("Slot ID %d = Phone Id: %d (from cached phoneIds_)", slotId, TO_INT(phoneID));
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetSlotIdFromPhoneId
(
    const taf::pa::data::PhoneId_e phoneID,
    taf::pa::data::SlotId_e &slotID
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    slotID = static_cast<taf::pa::data::SlotId_e> (
                                    phoneManager_->getSlotIdFromPhoneId(static_cast<int>(phoneID)));
    TAF_PA_DEBUG("Phone Id: %d = Slot ID %d", TO_INT(phoneID), TO_INT(slotID));
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaAddSubsystemStateChangeCallback
(
    taf_pa_data_SubsystemStateChangeCb callBack,
    std::shared_ptr<void> context,
    uint16_t &id
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");

    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    // Lock
    std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
    // Add the callback
    SubsystemEventsCallbackEntry_t entry = {
        subsystemEventsCallbackId_,
        callBack,
        context,
    };
    subsystemEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = subsystemEventsCallbackId_;
    // Increment the ID.
    subsystemEventsCallbackId_++;

    TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    TAF_PA_INFO("Number of registered callbacks: %zu", subsystemEventsCallbacks_.size());

    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaRemoveSubsystemStateChangeCallback
(
    uint16_t id
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    // Lock
    std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = subsystemEventsCallbacks_.begin();cbk != subsystemEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            TAF_PA_INFO("Id: %d, Cbk: %p", id, cbk);
            subsystemEventsCallbacks_.erase(cbk);
            return TAF_PA_OK;
        }
    }
    TAF_PA_WARN("Callback not found. Id: %d", id);
    return TAF_PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxData::SendSubsystemEventToClients
(
    const SubsystemEvent_t &eventInfo
)
{
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<SubsystemEventsCallbackEntry_t> localCbksCopy;
    {
        // Use exclusive lock to serialize event delivery and prevent race conditions
        // This ensures that all registered callbacks receive events in the correct order
        // even when called from multiple threads simultaneously
        std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
        localCbksCopy = subsystemEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(eventInfo.phoneId, eventInfo.subsystem, eventInfo.subsystemState,
                                                                                     cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaAddRoamingEventsCallback
(
    taf_pa_data_RoamingEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, TAF_PA_BAD_PARAMETER, "callBack is NULL!");
    // Lock
    std::lock_guard<std::mutex> lock(roamingEventsCbksMtx_);
    // Add the callback
    RoamingEventsCallbackEntry_t entry = {roamingEventsCallbackId_, callBack, context };
    roamingEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = roamingEventsCallbackId_;
    // Increment the ID.
    roamingEventsCallbackId_++;
    TAF_PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    TAF_PA_INFO("Number of registered callbacks: %zu", roamingEventsCallbacks_.size());
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaRemoveRoamingEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");
    // Lock
    std::lock_guard<std::mutex> lock(roamingEventsCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = roamingEventsCallbacks_.begin(); cbk != roamingEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            TAF_PA_INFO("Id: %d, Cbk: %p", id, cbk);
            roamingEventsCallbacks_.erase(cbk);
            return TAF_PA_OK;
        }
    }
    TAF_PA_WARN("Callback not found. Id: %d", id);
    return TAF_PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxData::SendRoamingEventInfoToClients
(
    const taf::pa::data::RoamingStatus_t &eventInfo
)
{
    TAF_PA_DEBUG("Calling registered callbacks...");
    std::vector<RoamingEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::lock_guard<std::mutex> lock(roamingEventsCbksMtx_);
        localCbksCopy = roamingEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            TAF_PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(eventInfo, cbk.context);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetRoamingStatus
(
    const PhoneId_e phoneId,
    RoamingStatus_t &roamingStatus
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != dataPhoneMngrInitState_, TAF_PA_FAULT,
                                                              "PA phone manager not initialized.");

    // Atomically check and set the flag to prevent race condition
    bool expected = false;
    if (!bGetRoamingStatusInProgress_.compare_exchange_strong(expected, true))
    {
        TAF_PA_ERROR("Get roaming status already in progress for phone id: %d", TO_INT(phoneId));
        return TAF_PA_BUSY;
    }

    taf_pa_result_t result = PaGetSlotIdFromPhoneId(phoneId, slotIDpa);
    if (TAF_PA_OK != result)
    {
        TAF_PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        // Reset the flag since we're returning early
        bGetRoamingStatusInProgress_.store(false);
        return result;
    }
    TAF_PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataServingSystemManagersMap_.find(slotId) == dataServingSystemManagersMap_.end())
    {
        TAF_PA_ERROR("Serving system manager is not init for slot %d", TO_INT(slotId));
        // Reset the flag since we're returning early
        bGetRoamingStatusInProgress_.store(false);
        return TAF_PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr = std::make_shared<std::promise<
                std::pair<telux::data::RoamingStatus, telux::common::ErrorCode>>>();
    auto fut = promisePtr->get_future();

    // Request - lambda captures promisePtr by value (shared ownership)
    status = dataServingSystemManagersMap_[slotId]->requestRoamingStatus
                    (
                        [promisePtr]
                        (
                            telux::data::RoamingStatus roamingStatus,
                            telux::common::ErrorCode error
                        )
                        {
                            SET_SDK_THREAD_NAME();
                            try
                            {
                                promisePtr->set_value(std::make_pair(roamingStatus, error));
                            }
                            catch (const std::future_error& e)
                            {
                                TAF_PA_ERROR("Future error in callback: %s", e.what());
                                // Try to set promise to unblock waiting thread
                                try { promisePtr->set_value(std::make_pair
                                    (telux::data::RoamingStatus(),
                                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
                            }
                            catch (const std::exception& e)
                            {
                                TAF_PA_ERROR("Exception in callback: %s", e.what());
                                try { promisePtr->set_value(std::make_pair
                                    (telux::data::RoamingStatus(),
                                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
                            }
                            catch (...)
                            {
                                TAF_PA_ERROR("Unknown error in requestRoamingStatus callback.");
                                try { promisePtr->set_value(std::make_pair
                                    (telux::data::RoamingStatus(),
                                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
                            }
                        }
                    );
    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("requestRoamingStatus failed. Status: %d", TO_INT(status));
        // Reset the in progress flag
        bGetRoamingStatusInProgress_.store(false);
        return TAF_PA_FAULT;
    }

    TAF_PA_DEBUG("Waiting for callback...");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("requestRoamingStatusCb promise timeout");
        // Reset the in progress flag
        bGetRoamingStatusInProgress_.store(false);
        return TAF_PA_TIMEOUT;
    }

    // Wait for the response
    std::pair<telux::data::RoamingStatus, telux::common::ErrorCode> futRsp;
    FUTURE_GET_RET_VAL(fut, futRsp, TAF_PA_FAULT);
    telux::common::ErrorCode futResult = futRsp.second;
    if (telux::common::ErrorCode::SUCCESS != futResult)
    {
        TAF_PA_ERROR("requestRoamingStatusCb failed. Status: %d", TO_INT(futResult));
        // Reset the in progress flag
        bGetRoamingStatusInProgress_.store(false);
        return TAF_PA_FAULT;
    }
    TAF_PA_DEBUG("success.");
    roamingStatus.isRoaming = futRsp.first.isRoaming;
    roamingStatus.type      = taf::pa::data::Utils::ConvertRoamingType(futRsp.first.type);
    roamingStatus.slotId    = slotIDpa;
    roamingStatus.phoneId   = phoneId;

    // Reset the in progress flag
    bGetRoamingStatusInProgress_.store(false);
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::data::TafPaTeluxData::PaGetServiceStatus
(
    const taf::pa::data::SlotId_e slotId,
    telux::data::ServiceStatus &serviceStatus
)
{
    SlotId teluxSlotId = static_cast<SlotId>(slotId);
    telux::common::Status status;

    if (dataServingSystemManagersMap_.find(teluxSlotId) == dataServingSystemManagersMap_.end())
    {
        TAF_PA_ERROR("Serving system manager is not init for slot %d", TO_INT(slotId));
        return TAF_PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr = std::make_shared<std::promise<
                std::pair<telux::data::ServiceStatus, telux::common::ErrorCode>>>();
    auto fut = promisePtr->get_future();

    // Request - lambda captures promisePtr by value (shared ownership)
    status = dataServingSystemManagersMap_[teluxSlotId]->requestServiceStatus
                    (
                        [promisePtr]
                        (
                            telux::data::ServiceStatus svcStatus,
                            telux::common::ErrorCode error
                        )
                        {
                            SET_SDK_THREAD_NAME();
                            try
                            {
                                promisePtr->set_value(std::make_pair(svcStatus, error));
                            }
                            catch (const std::future_error& e)
                            {
                                TAF_PA_ERROR("Future error in requestServiceStatus callback: %s",
                                         e.what());
                                try { promisePtr->set_value(std::make_pair(
                                    telux::data::ServiceStatus{},
                                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
                            }
                            catch (const std::exception& e)
                            {
                                TAF_PA_ERROR("Exception in requestServiceStatus callback: %s",
                                         e.what());
                                try { promisePtr->set_value(std::make_pair(
                                    telux::data::ServiceStatus{},
                                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
                            }
                            catch (...)
                            {
                                TAF_PA_ERROR("Unknown error in requestServiceStatus callback.");
                                try { promisePtr->set_value(std::make_pair(
                                    telux::data::ServiceStatus{},
                                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
                            }
                        }
                    );
    if (telux::common::Status::SUCCESS != status)
    {
        TAF_PA_ERROR("requestServiceStatus failed. Status: %d", TO_INT(status));
        return TAF_PA_FAULT;
    }

    TAF_PA_DEBUG("Waiting for requestServiceStatus callback...");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        TAF_PA_ERROR("requestServiceStatus promise timeout");
        return TAF_PA_TIMEOUT;
    }

    std::pair<telux::data::ServiceStatus, telux::common::ErrorCode> futRsp;
    FUTURE_GET_RET_VAL(fut, futRsp, TAF_PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futRsp.second)
    {
        TAF_PA_ERROR("requestServiceStatus callback failed. ErrorCode: %d", TO_INT(futRsp.second));
        return TAF_PA_FAULT;
    }

    serviceStatus = futRsp.first;
    return TAF_PA_OK;
}
