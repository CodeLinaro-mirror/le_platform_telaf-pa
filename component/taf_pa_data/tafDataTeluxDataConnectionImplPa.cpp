/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataConnectionImpl.cpp
 * @brief Telux Data connection implementation.
 *
 */

#include "tafDataUtilsPa.hpp"
#include "tafDataTeluxDataPa.hpp"
#include "tafDataTeluxDataConnectionPa.hpp"

taf::pa::data::TafPaTeluxDataConnection &taf::pa::data::TafPaTeluxDataConnection::GetInstance()
{
    static TafPaTeluxDataConnection instance;
    return instance;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetSubsysState
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
    {
        std::shared_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
        sState = dataConnectionManagersSubsysStateMap_[slotId];
    }
    PA_INFO("Conn init state for slot id[%d]: %d", slotId, TO_INT(sState));
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::SetSubsysState
(
    taf::pa::data::SlotId_e slotId,
    taf::pa::data::SubsystemState_e sState,
    bool bSendEvent
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    {
        std::unique_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
        dataConnectionManagersSubsysStateMap_[slotId] = sState;
    }
    PA_INFO("Conn init state for slot id[%d]: %d", slotId, TO_INT(sState));

    // Send the state change event to clients.
    if (bSendEvent)
    {
        PA_INFO("Send event to clients.");
        auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
        SubsystemEvent_t event;
        pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(slotId, event.phoneId);
        TAF_PA_ERROR_IF_RET_VAL(PA_OK != result, result, "PaGetPhoneIdFromSlotId err: %d", result);
        event.subsystem      = Subsystem_e::DATACALL_MANAGER;
        event.subsystemState = sState;
        teluxPaData.SendSubsystemEventToClients(event);
    }
    return PA_OK;
}

void taf::pa::data::TafPaTeluxDataConnection::Init(taf::pa::data::SlotCount_e slotCount)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_NIL(SubsystemState_e::AVAILABLE != phoneMngrState,
                                                             "PA phone manager not initialized.");
    slotCount_ = slotCount;
    initDataConnectionManagers();
    PaRegisterDataConnCallbacks();
}

void taf::pa::data::TafPaTeluxDataConnection::Deinit()
{
    deInitDataConnectionManagers();
}

void taf::pa::data::TafPaTeluxDataConnection::LogDataCallInfo
(
    const std::shared_ptr<telux::data::IDataCall> &dataCall,
    const char *fromPtr
)
{
    int32_t profileId = dataCall->getProfileId();
    uint8_t slotId = (uint8_t)dataCall->getSlotId();
    telux::data::DataCallStatus callStatus;

    PA_DEBUG("data callback details from: %s", fromPtr);
    PA_DEBUG("profile id:           %d", profileId);
    PA_DEBUG("slot id:              %d", slotId);
    PA_DEBUG("interface name:       %s", dataCall->getInterfaceName().c_str());
    callStatus = dataCall->getDataCallStatus();
    PA_DEBUG("call status:          %s", taf::pa::data::Utils::CallStatusToString(callStatus));
    PA_DEBUG("ip type:              %s", taf::pa::data::Utils::IpFamilyTypeToString(
                                             dataCall->getIpFamilyType()));
    PA_DEBUG("ipv4 status:          %s", taf::pa::data::Utils::CallStatusToString(
                                             dataCall->getIpv4Info().status));
    if (telux::data::DataCallStatus::NET_CONNECTED == dataCall->getIpv4Info().status)
    {
        unsigned int mask = 0;
        PA_DEBUG("IPv4 Address Info:");
        PA_DEBUG("  Interface addr : %s", dataCall->getIpv4Info().addr.ifAddress.c_str());
        mask = dataCall->getIpv4Info().addr.ifMask;
        PA_DEBUG("  Interface mask : 0x%X(%u.%u.%u.%u)", mask,
                                                        (mask >> 24) & 0xFF,
                                                        (mask >> 16) & 0xFF,
                                                        (mask >> 8)  & 0xFF,
                                                         mask        & 0xFF);
        PA_DEBUG("  Gateway addr   : %s", dataCall->getIpv4Info().addr.gwAddress.c_str());
        mask = dataCall->getIpv4Info().addr.gwMask;
        PA_DEBUG("  Gateway mask   : 0x%X(%u.%u.%u.%u)", mask,
                                                        (mask >> 24) & 0xFF,
                                                        (mask >> 16) & 0xFF,
                                                        (mask >> 8)  & 0xFF,
                                                         mask        & 0xFF);
        PA_DEBUG("  Pri DNS addr   : %s", dataCall->getIpv4Info().addr.primaryDnsAddress.c_str());
        PA_DEBUG("  Sec DNS addr   : %s", dataCall->getIpv4Info().addr.secondaryDnsAddress.c_str());
    }
    PA_DEBUG("ipv6 status:          %s", taf::pa::data::Utils::CallStatusToString(
                                             dataCall->getIpv6Info().status));
    if (telux::data::DataCallStatus::NET_CONNECTED == dataCall->getIpv6Info().status)
    {
        PA_DEBUG("IPv6 Address Info:");
        PA_DEBUG("  Interface addr : %s", dataCall->getIpv6Info().addr.ifAddress.c_str());
        PA_DEBUG("  Interface mask : 0x%X", dataCall->getIpv6Info().addr.ifMask);
        PA_DEBUG("  Gateway addr   : %s", dataCall->getIpv6Info().addr.gwAddress.c_str());
        PA_DEBUG("  Gateway mask   : 0x%X", dataCall->getIpv6Info().addr.gwMask);
        PA_DEBUG("  Pri DNS addr   : %s", dataCall->getIpv6Info().addr.primaryDnsAddress.c_str());
        PA_DEBUG("  Sec DNS addr   : %s", dataCall->getIpv6Info().addr.secondaryDnsAddress.c_str());
    }

    telux::common::DataCallEndReason reason = dataCall->getDataCallEndReason();
    PA_DEBUG("call end reason type:   %s",
                                     taf::pa::data::Utils::CallEndReasonTypeToString(reason.type));
    if (telux::data::DataCallStatus::NET_NO_NET == callStatus ||
        telux::data::DataCallStatus::NET_DISCONNECTING == callStatus)
    {
        switch (reason.type)
        {
        case telux::data::EndReasonType::CE_MOBILE_IP:
            PA_DEBUG("call end MIP reason code: %d", TO_INT(reason.IpCode));
            break;
        case telux::data::EndReasonType::CE_INTERNAL:
            PA_DEBUG("call end internal reason code: %d", TO_INT(reason.internalCode));
            break;
        case telux::data::EndReasonType::CE_CALL_MANAGER_DEFINED:
            PA_DEBUG("call end CM reason code: %d", TO_INT(reason.cmCode));
            break;
        case telux::data::EndReasonType::CE_3GPP_SPEC_DEFINED:
            PA_DEBUG("call end 3GPP spec reason code: %d", TO_INT(reason.specCode));
            break;
        case telux::data::EndReasonType::CE_PPP:
            PA_DEBUG("call end PPP reason code: %d", TO_INT(reason.pppCode));
            break;
        case telux::data::EndReasonType::CE_EHRPD:
            PA_DEBUG("call end EHRPD reason code: %d", TO_INT(reason.ehrpdCode));
            break;
        case telux::data::EndReasonType::CE_IPV6:
            PA_DEBUG("call end IPv6 reason code: %d", TO_INT(reason.ipv6Code));
            break;
        case telux::data::EndReasonType::CE_HANDOFF:
            PA_DEBUG("call end handoff reason code: %d", TO_INT(reason.handOffCode));
            break;
        default:
            PA_DEBUG("Invalid Reason code: %d", TO_INT(reason.type));
            break;
        }
    }

    PA_DEBUG("tech preference:      %s", taf::pa::data::Utils::TechPreferenceToString(
                                             dataCall->getTechPreference()));
    {
        auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
        taf::pa::data::SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(
                                                                         dataCall->getSlotId());
        telux::data::ServiceStatus svcStatus{};
        if (PA_OK == teluxPaData.PaGetServiceStatus(slotIdPa, svcStatus))
        {
            PA_DEBUG("NetworkRat (bearer tech): %s",
                     taf::pa::data::Utils::NetworkRatToString(svcStatus.networkRat));
        }
        else
        {
            PA_DEBUG("NetworkRat (bearer tech): unavailable");
        }
    }

    return;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetDefaultProfile
(
    const taf::pa::data::PhoneId_e phoneId,
    taf::pa::data::ProfileId_e &profileId
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

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

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr =
                std::make_shared<std::promise<std::tuple<int, SlotId, telux::common::ErrorCode>>>();
    std::future<std::tuple<int, SlotId, telux::common::ErrorCode>> future =
                promisePtr->get_future();

    // Define a lambda function to handle the callback and set the promise
    status = dataConnectionManagersMap_[slotId]->getDefaultProfile
    (
        telux::data::OperationType::DATA_LOCAL,
        // Lambda callback - captures promisePtr by value (shared ownership)
        [promisePtr]
        (
            int profileId, SlotId slotId, telux::common::ErrorCode error
        )
        {
            SET_SDK_THREAD_NAME();
            try
            {
                promisePtr->set_value(std::make_tuple(profileId, slotId, error));
            }
            catch (const std::future_error& e)
            {
                PA_ERROR("Future error in callback: %s", e.what());
                // Try to set promise to unblock waiting thread
                try { promisePtr->set_value(std::make_tuple(-1, INVALID_SLOT_ID,
                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
            }
            catch (const std::exception& e)
            {
                PA_ERROR("Exception in callback: %s", e.what());
                try { promisePtr->set_value(std::make_tuple(-1, INVALID_SLOT_ID,
                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
            }
            catch (...)
            {
                PA_ERROR("Unknown error in getDefaultProfile callback.");
                try { promisePtr->set_value(std::make_tuple(-1, INVALID_SLOT_ID,
                    telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
            }
        }
    );
    if (telux::common::Status::SUCCESS != status)
    {
        PA_ERROR("getDefaultProfile failed. Status: %d", TO_INT(status));
        return PA_FAULT;
    }

    // Wait for the callback to complete and capture the results
    PA_DEBUG("Wait for callback..");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = future.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("getDefaultProfile promise timeout");
        return PA_TIMEOUT;
    }

    std::tuple<int, SlotId, telux::common::ErrorCode> futResult;
    FUTURE_GET_RET_VAL(future, futResult, PA_FAULT);
    int futProfileId = std::get<0>(futResult);
    SlotId futSlotId = std::get<1>(futResult);
    telux::common::ErrorCode futError = std::get<2>(futResult);

    if (telux::common::ErrorCode::SUCCESS != futError)
    {
        PA_ERROR("getDefaultProfile failed in callback. Error: %d", TO_INT(futError));
        return PA_FAULT;
    }
    PA_DEBUG("Slot Id: %d", TO_INT(futSlotId));
    PA_DEBUG("Profile: %d", futProfileId);

    if (futSlotId != slotId)
    {
        PA_ERROR("Slot ID mismatch. Expected: %d, Actual: %d", TO_INT(slotId), TO_INT(futSlotId));
        return PA_FAULT;
    }
    profileId = static_cast<taf::pa::data::ProfileId_e>(futProfileId);

    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaSetDefaultProfile
(
    const taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileId_e profileId
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

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

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    std::future<telux::common::ErrorCode> fut = promisePtr->get_future();

    PA_INFO("Profile ID to set: %d", TO_INT(profileId));
    status = dataConnectionManagersMap_[slotId]->setDefaultProfile
                (
                    telux::data::OperationType::DATA_LOCAL,
                    static_cast<uint8_t>(profileId),
                    // Lambda callback - captures promisePtr by value (shared ownership)
                    [promisePtr](telux::common::ErrorCode error)
                    {
                        SET_SDK_THREAD_NAME();
                        try
                        {
                            promisePtr->set_value(error);
                        }
                        catch (const std::future_error& e)
                        {
                            PA_ERROR("Future error in callback: %s", e.what());
                            // Try to set promise to unblock waiting thread
                            try { promisePtr->set_value
                                (telux::common::ErrorCode::INTERNAL_ERROR); } catch(...) {}
                        }
                        catch (const std::exception& e)
                        {
                            PA_ERROR("Exception in callback: %s", e.what());
                            try { promisePtr->set_value
                                (telux::common::ErrorCode::INTERNAL_ERROR); } catch(...) {}
                        }
                        catch (...)
                        {
                            PA_ERROR("Unknown error in setDefaultProfile callback.");
                            try { promisePtr->set_value
                                (telux::common::ErrorCode::INTERNAL_ERROR); } catch(...) {}
                        }
                    }
                );
    if (telux::common::Status::SUCCESS != status)
    {
        PA_ERROR("setDefaultProfile failed. Status: %d", TO_INT(status));
        return PA_FAULT;
    }

    // Wait for the callback to complete and capture the results
    PA_DEBUG("Wait for callback..");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("setDefaultProfile promise timeout");
        return PA_TIMEOUT;
    }

    telux::common::ErrorCode futError;
    FUTURE_GET_RET_VAL(fut, futError, PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futError)
    {
        PA_ERROR("setDefaultProfile failed in callback. Error: %d", TO_INT(futError));
        return PA_FAULT;
    }
    PA_DEBUG("setDefaultProfile done.");
    return PA_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Private functions
 */
////////////////////////////////////////////////////////////////////////////////////////////////////

void taf::pa::data::TafPaTeluxDataConnection::initDataConnectionManagers()
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

    PA_INFO("Starting data connection manager initialization for %d slot(s)", TO_INT(slotCount_));

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        taf::pa::data::SlotId_e paSlotId = static_cast<taf::pa::data::SlotId_e>(slotId);

        // Check if already initialized
        SubsystemState_e connInitCheckState;
        {
            std::shared_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
            connInitCheckState = dataConnectionManagersSubsysStateMap_[paSlotId];
        }
        if (taf::pa::data::SubsystemState_e::AVAILABLE == connInitCheckState)
        {
            PA_INFO("Data connection manager already initialized for slot id: %d.", TO_INT(slotId));
            successfulSlots.push_back(slotId);
            continue;
        }

        PA_INFO("Initializing data connection manager for slot %d...", slotId);

        // Initialize the data connection manager for each slot
        // Create shared state for synchronization using condition variable
        // Use heap-allocated shared_ptr to ensure state outlives this function scope
        struct InitState {
            std::mutex mtx;
            std::condition_variable cv;
            bool callbackReceived = false;
            telux::common::ServiceStatus status;
        };
        auto state = std::make_shared<InitState>();

        auto dataProfMngr = dataFactory.getDataConnectionManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [state, slotId](telux::common::ServiceStatus svcStatus)
            {
                SET_SDK_THREAD_NAME();
                PA_INFO("getDataConnectionManager callback for slot ID: %d", TO_INT(slotId));
                PA_INFO("Data connection manager status: %d", TO_INT(svcStatus));

                {
                    std::lock_guard<std::mutex> lock(state->mtx);
                    state->status = svcStatus;
                    state->callbackReceived = true;
                }
                state->cv.notify_one();
            });

        if (!dataProfMngr)
        {
            PA_ERROR("Failed to get Data connection manager for slot %d", slotId);
            SetSubsysState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }

        // Wait for the callback with timeout
        telux::common::ServiceStatus dataServSysMgrStatus;
        PA_INFO("Waiting for Data connection manager to be ready for slot %d...", slotId);

        {
            std::unique_lock<std::mutex> lock(state->mtx);
            bool success = state->cv.wait_for(
                lock,
                std::chrono::seconds(taf::pa::data::SUBSYSTEM_INIT_TIMEOUT),
                [&state]() { return state->callbackReceived; }
            );

            if (!success)
            {
                PA_ERROR("Timeout waiting for Data connection manager for slot %d", slotId);
                SetSubsysState(paSlotId, SubsystemState_e::FAILED);
                failedSlots.push_back(slotId);
                // Continue processing other slots instead of returning
                continue;
            }

            dataServSysMgrStatus = state->status;
        }

        PA_INFO("dataConnMngr for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));

        // Handle different service status outcomes
        if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
        {
            PA_INFO("dataConnMngr for slot %d: AVAILABLE", slotId);
            // Store the manager in the map with slot Id as index
            dataConnectionManagersMap_.emplace((SlotId)slotId, dataProfMngr);
            // Initialize and store the callback objects for later registration.
            tafPaTeluxDataConnectionListenersMap_.emplace((SlotId)slotId,
                            std::make_shared<TafPaTeluxDataConnectionListener>((SlotId)slotId));
            dataConnectionListenersMap_[(SlotId)slotId] =
                                        tafPaTeluxDataConnectionListenersMap_[(SlotId)slotId];
            // Mark listener as not registered (under lock for consistency with
            // PaRegisterDataConnCallbacks / PaDeregisterDataConnCallbacks).
            {
                std::unique_lock lock(dataConnectionCbksMtx_);
                bDataConnectionListenersRegistered_[slotId-1] = false;
            }
            // Update that the data connection manager is initialized.
            SetSubsysState(paSlotId, SubsystemState_e::AVAILABLE);
            successfulSlots.push_back(slotId);
            PA_INFO("Data connection manager initialization for slot %d complete", slotId);
        }
        else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
        {
            PA_WARN("dataConnMngr for slot %d: UNAVAILABLE (temporary)", slotId);
            // Mark as unavailable but don't set FAILED state - this is temporary
            // The service may become available later
            unavailableSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
        else
        {
            // Unknown/error status
            PA_ERROR("Failed to init Data connection manager for slot %d with status: %d",
                     slotId, TO_INT(dataServSysMgrStatus));
            SetSubsysState(paSlotId, SubsystemState_e::FAILED);
            failedSlots.push_back(slotId);
            // Continue processing other slots instead of returning
            continue;
        }
    }

    // Log summary of initialization results
    PA_INFO("=== Data Connection Manager Initialization Summary ===");
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

pa_result_t taf::pa::data::TafPaTeluxDataConnection::deInitDataConnectionManagers()
{
    PA_INFO("Starting data connection managers deinitialization...");

    // Phase 1: Check for active operations
    if (bRequestCallListInProgress_.load())
    {
        PA_WARN("Request call list operation in progress during deinit");
        // Log warning but continue - the operation will be aborted when maps are cleared
    }

    // Phase 2: Deregister callbacks with error checking
    pa_result_t result = PaDeregisterDataConnCallbacks();
    if (PA_OK != result)
    {
        PA_ERROR("CRITICAL: Failed to deregister data connection callbacks!");
        PA_ERROR("Cannot safely proceed with cleanup - callbacks may still be active");
        PA_ERROR("This could lead to crashes if SDK invokes callbacks after cleanup");
        // Reset the flag to prevent further event forwarding attempts even though
        // deregistration failed - this stops the PA layer from forwarding any
        // incoming SDK throughput events to a partially cleaned-up state.
        bThroughputEventsEnabled_.store(false);
        return PA_FAULT; // Do NOT clear maps if deregistration failed
    }

    PA_INFO("Callbacks successfully deregistered, proceeding with cleanup...");

    // Phase 3: Ordered cleanup with mutex protection
    {
        std::unique_lock lock(dataConnectionCbksMtx_);

        // Clear callback vectors first to prevent new callbacks from being invoked
        PA_INFO("Clearing callback vectors...");
        dataCallEventsCallbacks_.clear();
        throttledApnEventsCallbacks_.clear();
        qosTftEventsCallbacks_.clear();
        hwAccelerationEventsCallbacks_.clear();
        throughputEventsCallbacks_.clear();
        // Explicitly reset the throughput events gate flag to ensure a consistent
        // state. During deinit the callbacks are cleared directly (not via
        // PaRemoveThroughputEventsCallback), so the flag must be reset here.
        bThroughputEventsEnabled_.store(false);

        PA_INFO("Cleared all callback vectors");
    }

    // Clear listener and manager maps in proper order
    PA_INFO("Clear tafPaTeluxDataConnectionListenersMap_");
    tafPaTeluxDataConnectionListenersMap_.clear();

    PA_INFO("Clear dataConnectionListenersMap_");
    dataConnectionListenersMap_.clear();

    PA_INFO("Clear dataConnectionManagersMap_");
    dataConnectionManagersMap_.clear();

    // Update subsystem states to reflect deinitialized state
    {
        std::unique_lock<std::shared_mutex> lock(dataConnSubsysStateMapMtx_);
        dataConnectionManagersSubsysStateMap_[SlotId_e::SLOT_1] = SubsystemState_e::FAILED;
        dataConnectionManagersSubsysStateMap_[SlotId_e::SLOT_2] = SubsystemState_e::FAILED;
    }

    // Reset request call list state: release the stored context shared_ptr and
    // clear the in-progress flag so a subsequent Init()/Deinit() cycle starts clean.
    PA_INFO("Reset requestCallListClientEntry_ and bRequestCallListInProgress_");
    {
        std::lock_guard<std::mutex> lock(requestCallListMutex_);
        requestCallListClientEntry_ = {nullptr, nullptr};
    }
    bRequestCallListInProgress_.store(false);

    PA_INFO("Data connection managers deinitialization complete");
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRegisterDataConnCallbacks()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    bool allSuccess = true;
    std::vector<int> failedSlots;
    // Protect the critical section
    std::unique_lock lock(dataConnectionCbksMtx_);

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Register serving system listener for each slot it
        if (bDataConnectionListenersRegistered_[slotId - 1])
        {
            PA_INFO("Data connection listener already registered for slot ID: %d.", slotId);
        }
        else
        {
            auto managerIt = dataConnectionManagersMap_.find((SlotId)slotId);
            if (managerIt != dataConnectionManagersMap_.end())
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
                auto listenerIt = dataConnectionListenersMap_.find((SlotId)slotId);
                if (listenerIt != dataConnectionListenersMap_.end())
                {
                    // Register for DEFAULT indications only.
                    // The THROUGHPUT indication is registered separately in
                    // PaAddThroughputEventsCallback() when the first throughput callback is
                    // added, and deregistered in PaRemoveThroughputEventsCallback() when the
                    // last throughput callback is removed.
                    if (managerIt->second->registerListener(listenerIt->second,
                        telux::data::DEFAULT_INDICATIONS) == telux::common::Status::SUCCESS)
                    {
                        PA_INFO("Data connection listener for slot ID %d registered.", slotId);
                        bDataConnectionListenersRegistered_[slotId - 1] = true;
                    }
                    else
                    {
                        PA_ERROR("Failed to register data connection listener for slot ID %d.",
                                                                                           slotId);
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

    PA_INFO("All data connection listeners successfully registered");
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaDeregisterDataConnCallbacks()
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    // Protect the critical section
    std::unique_lock lock(dataConnectionCbksMtx_);

    bool allSuccess = true;
    std::vector<int> failedSlots;

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Deregister serving system listener for each slot
        if (bDataConnectionListenersRegistered_[slotId - 1])
        {
            if (dataConnectionManagersMap_.find((SlotId)slotId) != dataConnectionManagersMap_.end())
            {
                // Deregister from DEFAULT indications only.
                // The THROUGHPUT indication is managed independently by
                // PaAddThroughputEventsCallback() and PaRemoveThroughputEventsCallback().
                telux::common::Status status = dataConnectionManagersMap_[(SlotId)slotId]->
                    deregisterListener(dataConnectionListenersMap_[(SlotId)slotId],
                                      telux::data::DEFAULT_INDICATIONS);

                if (telux::common::Status::SUCCESS == status)
                {
                    PA_INFO("Data connection listener for slot ID %d deregistered.", slotId);
                    bDataConnectionListenersRegistered_[slotId - 1] = false;
                }
                else
                {
                    PA_ERROR("FAILED to deregister listener for slot ID %d. Status: %d",
                             slotId, TO_INT(status));
                    failedSlots.push_back(slotId);
                    allSuccess = false;
                }
            }
        }
        else
        {
            PA_INFO("Serving System listener already deregistered for slot ID: %d.", slotId);
        }
    }

    if (!allSuccess)
    {
        PA_ERROR("=== Deregistration Failed ===");
        PA_ERROR("Failed to deregister callbacks for %zu slot(s)", failedSlots.size());
        for (auto slot : failedSlots)
        {
            PA_ERROR("  - Slot %d: DEREGISTRATION FAILED", slot);
        }
        PA_ERROR("============================");
        return PA_FAULT;
    }

    PA_INFO("All data connection callbacks successfully deregistered");
    return PA_OK;
}

/**
 * The callback fuction for TelSDK startDataCall()
 *
 * When callback is used with startDataCall, expected behavior is as following:
 * If this is first client to start datacall in the system and no error is detected,
 * getDataCallStatus() of telux::data::IDataCall object will return NET_CONNECTING and
 * onDataCallInfoChanged will be called once data call is brought up successfully or failed.
 *
 * If client tries to start data call that is already up and no error is detected,
 * getDataCallStatus() of telux::data::IDataCall object will return NET_CONNECTED and
 * onDataCallInfoChanged will not get called.
 *
 * If any client starts data call and error is detected, error argument will contain error code and
 * onDataCallInfoChanged will not get called.
 *
 */
void taf::pa::data::TafPaTeluxDataConnection::startDataCallCallback
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall,
    telux::common::ErrorCode errorCode
)
{
    SET_SDK_THREAD_NAME();

    // Log the call data
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.LogDataCallInfo(iDataCall, __func__);

    // Fill DataCallEventInfo_t
    DataCallEventInfo_t eventInfo;
    taf::pa::data::PhoneId_e phoneId;
    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());
    telux::data::IpFamilyInfo IPv4Info = iDataCall->getIpv4Info();
    telux::data::IpFamilyInfo IPv6Info = iDataCall->getIpv6Info();

    auto &teluxPaData    = TafPaTeluxData::GetInstance();
    pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(eventInfo.slotId, phoneId);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "PaGetPhoneIdFromSlotId err: %d, drop this event",
                                                                                            result);

    eventInfo.phoneId    = phoneId;
    eventInfo.profileId  = static_cast<taf::pa::data::ProfileId_e>(iDataCall->getProfileId());

    eventInfo.maxRxBitRate = 0;
    eventInfo.maxTxBitRate = 0;

    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_ERROR("Starting data session failed with error code: %d", TO_INT(errorCode));
        // Set the call status to DISCONNECTED
        eventInfo.callStatus                  = DataCallStatus_e::DISCONNECTED;
        eventInfo.ipv4DataCallInfo.callStatus = DataCallStatus_e::DISCONNECTED;
        eventInfo.ipv6DataCallInfo.callStatus = DataCallStatus_e::DISCONNECTED;
    }
    else
    {
        /**
         * In some cases, getDataCallStatus() returns NET_NO_NET even in success.
         * Set status to NET_CONNECTING.
        */
        if (telux::data::DataCallStatus::NET_NO_NET == iDataCall->getDataCallStatus())
        {
            PA_WARN("TelSDK returned NET_NOT_NET");
            eventInfo.callStatus                  = DataCallStatus_e::CONNECTING;
            eventInfo.ipv4DataCallInfo.callStatus = DataCallStatus_e::CONNECTING;
            eventInfo.ipv6DataCallInfo.callStatus = DataCallStatus_e::CONNECTING;
        }
        else
        {
            // Update status with TelSDK status.
            eventInfo.callStatus                  = taf::pa::data::Utils::ConvertCallStatus(
                                                                    iDataCall->getDataCallStatus());
            eventInfo.ipv4DataCallInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(
                                                                                IPv4Info.status);
            eventInfo.ipv6DataCallInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(
                                                                                IPv6Info.status);
        }
    }

    eventInfo.ipType     = taf::pa::data::Utils::ConvertIpType(iDataCall->getIpFamilyType());

    // Send the data call event info to registered clients
    teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
    return;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaStartDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params
)
{
    telux::data::DataCallParams teluxParams;
    taf::pa::data::SlotId_e slotIDpa;

    teluxParams.profileId = static_cast<int>(params.profileId);
    teluxParams.ipFamilyType = taf::pa::data::Utils::ConvertIpType(params.ipType);
    teluxParams.interfaceName = params.interfaceName;

    PA_DEBUG("PaStartDataSessionAsync - teluxParams (after): profileId=%d, ipFamilyType=%d, "
            "interfaceName='%s', operationType=%d",
            teluxParams.profileId, TO_INT(teluxParams.ipFamilyType),
            teluxParams.interfaceName.c_str(), TO_INT(teluxParams.operationType));

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(params.phoneId, slotIDpa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(params.phoneId));
        return result;
    }
    PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    taf::pa::data::PhoneId_e phoneId = params.phoneId;
    taf::pa::data::ProfileId_e profileId = params.profileId;
    taf::pa::data::SlotId_e paSlotId = slotIDpa;
    taf::pa::data::IpType_e ipType = params.ipType;

    auto cb =
        [phoneId, profileId, paSlotId, ipType]
        (
            const std::shared_ptr<telux::data::IDataCall> &iDataCall,
            telux::common::ErrorCode errorCode
        )
        {
            SET_SDK_THREAD_NAME();
            auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();

            if (!iDataCall)
            {
                // Graceful handling of NULL iDataCall
                PA_WARN("startDataCallCallback iDataCall is NULL, phoneId=%d profileId=%d err=%d",
                        TO_INT(phoneId), TO_INT(profileId), TO_INT(errorCode));

                DataCallEventInfo_t eventInfo{};
                eventInfo.phoneId = phoneId;
                eventInfo.slotId = paSlotId;
                eventInfo.profileId = profileId;
                eventInfo.ipType = ipType;

                // On error, report DISCONNECTED. If TelSDK claims SUCCESS with null pointer,
                // treat as failure defensively.
                if (telux::common::ErrorCode::SUCCESS != errorCode)
                {
                    PA_ERROR("Starting data session failed with error code: %d",
                             TO_INT(errorCode));
                }

                eventInfo.callStatus = DataCallStatus_e::DISCONNECTED;
                eventInfo.ipv4DataCallInfo.callStatus = DataCallStatus_e::DISCONNECTED;
                eventInfo.ipv6DataCallInfo.callStatus = DataCallStatus_e::DISCONNECTED;

                teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
                return;
            }

            // Normal path: we have a valid IDataCall, reuse existing implementation
            taf::pa::data::TafPaTeluxDataConnection::startDataCallCallback(
                iDataCall, errorCode);
        };

    auto status = dataConnectionManagersMap_[slotId]->startDataCall(teluxParams, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_ERROR("startDataCall failed. Status: %d", TO_INT(status));
        return PA_FAULT;
    }
    return PA_OK;
}

void taf::pa::data::TafPaTeluxDataConnection::stopDataCallCallback
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall,
    telux::common::ErrorCode errorCode
)
{
    SET_SDK_THREAD_NAME();

    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_ERROR("Stopping data session failed with error code: %d", TO_INT(errorCode));
    }

    // Log the call data
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.LogDataCallInfo(iDataCall, __func__);

    // Fill DataCallEventInfo_t
    DataCallEventInfo_t eventInfo;
    taf::pa::data::PhoneId_e phoneId;
    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());
    telux::data::IpFamilyInfo IPv4Info = iDataCall->getIpv4Info();
    telux::data::IpFamilyInfo IPv6Info = iDataCall->getIpv6Info();

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(eventInfo.slotId, phoneId);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "PaGetPhoneIdFromSlotId err: %d, drop this event",
                                                                                           result);
    eventInfo.phoneId = phoneId;
    eventInfo.profileId = static_cast<taf::pa::data::ProfileId_e>(iDataCall->getProfileId());
    eventInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(iDataCall->getDataCallStatus());
    eventInfo.ipv4DataCallInfo.callStatus=taf::pa::data::Utils::ConvertCallStatus(IPv4Info.status);
    eventInfo.ipv6DataCallInfo.callStatus=taf::pa::data::Utils::ConvertCallStatus(IPv6Info.status);
    eventInfo.ipType     = taf::pa::data::Utils::ConvertIpType(iDataCall->getIpFamilyType());

    // Send the data call event info to registered clients
    teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
    return;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaStopDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params
)
{
    telux::data::DataCallParams teluxParams;
    taf::pa::data::SlotId_e slotIDpa;

    teluxParams.profileId = static_cast<int>(params.profileId);
    teluxParams.ipFamilyType = taf::pa::data::Utils::ConvertIpType(params.ipType);

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(params.phoneId, slotIDpa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(params.phoneId));
        return result;
    }
    PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    taf::pa::data::PhoneId_e phoneId = params.phoneId;
    taf::pa::data::ProfileId_e profileId = params.profileId;
    taf::pa::data::SlotId_e paSlotId = slotIDpa;
    taf::pa::data::IpType_e ipType = params.ipType;

    auto cb =
        [phoneId, profileId, paSlotId, ipType]
        (
            const std::shared_ptr<telux::data::IDataCall> &iDataCall,
            telux::common::ErrorCode errorCode
        )
        {
            SET_SDK_THREAD_NAME();
            auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();

            if (!iDataCall)
            {
                // Graceful handling of NULL iDataCall
                PA_WARN("stopDataCallCallback iDataCall is NULL, phoneId=%d profileId=%d err=%d",
                        TO_INT(phoneId), TO_INT(profileId), TO_INT(errorCode));

                if (telux::common::ErrorCode::SUCCESS != errorCode)
                {
                    PA_ERROR("Stopping data session failed with error code: %d",
                             TO_INT(errorCode));
                }

                DataCallEventInfo_t eventInfo{};
                eventInfo.phoneId = phoneId;
                eventInfo.slotId = paSlotId;
                eventInfo.profileId = profileId;
                eventInfo.ipType = ipType;

                // Stopping a session with null call is effectively disconnected.
                eventInfo.callStatus = DataCallStatus_e::DISCONNECTED;
                eventInfo.ipv4DataCallInfo.callStatus = DataCallStatus_e::DISCONNECTED;
                eventInfo.ipv6DataCallInfo.callStatus = DataCallStatus_e::DISCONNECTED;

                teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
                return;
            }

            // Normal path: we have a valid IDataCall, reuse existing implementation
            taf::pa::data::TafPaTeluxDataConnection::stopDataCallCallback(
                iDataCall, errorCode);
        };

    auto status = dataConnectionManagersMap_[slotId]->stopDataCall(teluxParams, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_ERROR("stopDataCall failed. Status: %d", TO_INT(status));
        return PA_FAULT;
    }
    return PA_OK;
}

void taf::pa::data::TafPaTeluxDataConnection::onRequestDataCallList
(
    const std::vector<std::shared_ptr<telux::data::IDataCall>> &iDataCalls,
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();

    auto &teluxPaDataConn = TafPaTeluxDataConnection::GetInstance();
    TAF_PA_ERROR_IF_RET_NIL(!teluxPaDataConn.bRequestCallListInProgress_.load(),
                                        "No pending request call list. Ignoring the event.");

    PA_INFO("Number of active calls: %zu", iDataCalls.size());

    // read the entry under the mutex so we always see the fully-written
    // callback/context pair that was stored atomically in PaRequestDataCallsListAsync.
    std::vector<DataCallEventInfo_t> callList;
    RequestDataCallListCallbackEntry_t entry;
    {
        std::lock_guard<std::mutex> lock(teluxPaDataConn.requestCallListMutex_);
        entry = teluxPaDataConn.requestCallListClientEntry_;
    }
    auto callback = entry.callback;
    auto context  = entry.context;

    // Verify callback is not null before proceeding
    if (!callback)
    {
        PA_ERROR("Callback is NULL. Exit request call list processing.");
        teluxPaDataConn.bRequestCallListInProgress_.store(false);
        return;
    }

    if (telux::common::ErrorCode::SUCCESS != error)
    {
        PA_ERROR("onRequestDataCallList failed with error code: %d", TO_INT(error));
        // Call the callback with error
        callback(PA_FAULT, callList, context);
        // Mark call as completed.
        teluxPaDataConn.bRequestCallListInProgress_.store(false);
        return;
    }

    for (const auto& iDataCall : iDataCalls)
    {
        LogDataCallInfo(iDataCall, __func__);

        SlotId slotId = iDataCall->getSlotId();
        PA_INFO("Slot ID: %d", TO_INT(slotId));
        if (teluxPaDataConn.tafPaTeluxDataConnectionListenersMap_.find(slotId) ==
                                    teluxPaDataConn.tafPaTeluxDataConnectionListenersMap_.end())
        {
            PA_ERROR("Connection connection listener not available for slot %d", TO_INT(slotId));
            return;
        }
        DataCallEventInfo_t eventInfo;
        teluxPaDataConn.tafPaTeluxDataConnectionListenersMap_[slotId]->ParseIDataCall(
                                                                            iDataCall, eventInfo);
        callList.push_back(eventInfo);
    }

    PA_INFO("Number of calls in list: %zu", callList.size());

    // Send the data call info to the requesting client
    callback(PA_OK, callList, context);

    // Reset the callback entry info.
    teluxPaDataConn.resetCallListClientEntry();
    // Mark call as completed.
    teluxPaDataConn.bRequestCallListInProgress_.store(false);
    return;
}

void taf::pa::data::TafPaTeluxDataConnection::resetCallListClientEntry()
{
    // reset the entry under the mutex.
    std::lock_guard<std::mutex> lock(requestCallListMutex_);
    requestCallListClientEntry_ = {nullptr, nullptr};
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRequestDataCallsListAsync
(
    taf::pa::data::PhoneId_e phoneId,
    taf::pa::data::taf_pa_data_RequestCallListCb callback,
    std::shared_ptr<void> context
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callback, PA_BAD_PARAMETER, "callback is null");

    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");

    // acquire the mutex, set the flag, and write the entry atomically
    // so the SB callback (onRequestDataCallList) can never read a null/stale entry.
    {
        std::lock_guard<std::mutex> lock(requestCallListMutex_);
        bool expected = false;
        if (!bRequestCallListInProgress_.compare_exchange_strong(expected, true))
        {
            // Another thread already set the flag, return PA_BUSY
            PA_ERROR("Request call list already in progress for phone id: %d", TO_INT(phoneId));
            return PA_BUSY;
        }
        // Write the entry under the lock, before the flag is visible to the SB thread.
        requestCallListClientEntry_ = {callback, context};
    }

    taf::pa::data::SlotId_e slotIdPa;
    telux::common::Status status;
    SlotId slotId;

    pa_result_t result = teluxPaData.PaGetSlotIdFromPhoneId(phoneId, slotIdPa);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        // Reset the entry and flag since we're returning early.
        {
            std::lock_guard<std::mutex> lock(requestCallListMutex_);
            requestCallListClientEntry_ = {nullptr, nullptr};
        }
        bRequestCallListInProgress_.store(false);
        return result;
    }
    PA_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIdPa));
    slotId = taf::pa::data::Utils::ConvertSlotId(slotIdPa);
    if (INVALID_SLOT_ID == slotId)
    {
        PA_ERROR("Invalid Slot ID");
        // Reset the entry and flag since we're returning early.
        {
            std::lock_guard<std::mutex> lock(requestCallListMutex_);
            requestCallListClientEntry_ = {nullptr, nullptr};
        }
        bRequestCallListInProgress_.store(false);
        return PA_BAD_PARAMETER;
    }

    // Check if the data connection manager is initialized.
    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        // Reset the entry and flag since we're returning early.
        {
            std::lock_guard<std::mutex> lock(requestCallListMutex_);
            requestCallListClientEntry_ = {nullptr, nullptr};
        }
        bRequestCallListInProgress_.store(false);
        return PA_FAULT;
    }

    status = dataConnectionManagersMap_[slotId]->requestDataCallList(
                            telux::data::OperationType::DATA_LOCAL, onRequestDataCallList);
    if (telux::common::Status::SUCCESS != status)
    {
        PA_ERROR("requestDataCallList failed for phone id[%d]: %d",TO_INT(phoneId), TO_INT(status));
        // Reset the entry and flag since we're returning early.
        {
            std::lock_guard<std::mutex> lock(requestCallListMutex_);
            requestCallListClientEntry_ = {nullptr, nullptr};
        }
        bRequestCallListInProgress_.store(false);
        return PA_FAULT;
    }

    PA_INFO("requestDataCallList in progress for phone id %d", TO_INT(phoneId));
    return PA_OK;
}



pa_result_t taf::pa::data::TafPaTeluxDataConnection::paGetThrottledApnInfo
(
    const taf::pa::data::PhoneId_e        phoneId,
    std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;
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
    PA_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    // Create shared promise to ensure it outlives this function scope
    auto promisePtr = std::make_shared<std::promise<
            std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode>>>();
    std::future<std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode>> fut
            = promisePtr->get_future();

    // Single SDK call — the callback only fulfils the promise, nothing else.
    status = dataConnectionManagersMap_[slotId]->requestThrottledApnInfo(
    [promisePtr]
    (
        const std::vector<telux::data::APNThrottleInfo> &throttleInfoList,
        telux::common::ErrorCode error
    )
    {
        SET_SDK_THREAD_NAME();
        try
        {
            promisePtr->set_value(std::make_pair(throttleInfoList, error));
        }
        catch (const std::future_error& e)
        {
            PA_ERROR("Future error in callback: %s", e.what());
            // Try to set promise to unblock waiting thread
            try { promisePtr->set_value(std::make_pair(std::vector<telux::data::APNThrottleInfo>(),
                telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
            try { promisePtr->set_value(std::make_pair(std::vector<telux::data::APNThrottleInfo>(),
                telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
        }
        catch (...)
        {
            PA_ERROR("Unknown error in requestThrottledApnInfo callback.");
            try { promisePtr->set_value(std::make_pair(std::vector<telux::data::APNThrottleInfo>(),
                telux::common::ErrorCode::INTERNAL_ERROR)); } catch(...) {}
        }
    });
    if (telux::common::Status::SUCCESS != status)
    {
        PA_ERROR("requestThrottledApnInfo failed. Status: %d", TO_INT(status));
        return PA_FAULT;
    }
    PA_DEBUG("Wait for callback ...");

    std::chrono::seconds span(taf::pa::data::NON_NETWORK_COMMAND_TIMEOUT); // 15 seconds
    std::future_status waitStatus = fut.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("requestThrottledApnInfo promise timeout");
        return PA_TIMEOUT;
    }

    std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode> futResult;
    FUTURE_GET_RET_VAL(fut, futResult, PA_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futResult.second)
    {
        PA_ERROR("requestThrottledApnInfo error: %d", TO_INT(futResult.second));
        return PA_FAULT;
    }

    std::vector<telux::data::APNThrottleInfo> throttleInfoList = futResult.first;
    size_t numThrottledApn = throttleInfoList.size();
    PA_DEBUG("Number of throttled APN(s): %zu", numThrottledApn);
    if (numThrottledApn > 0)
    {
        for (auto &throttleInfo : throttleInfoList)
        {
            ThrottledApnEventInfo_t throttledApnEventInfo;
            Utils::ConvertThrottledApnEvent(throttleInfo, throttledApnEventInfo);
            // Add the phone ID
            throttledApnEventInfo.phoneId = phoneId;
            // Add to vector
            throttledApnEventInfoList.push_back(throttledApnEventInfo);
        }
    }

    return PA_OK;
}

/**
 * SDK to PA event callbacks
 */
void taf::pa::data::TafPaTeluxDataConnection::PaSendHwAccelerationEventInfoToClients
(
    const HwAccelerationChangeEvent_t &hwAccelerationEventInfo
)
{
    PA_DEBUG("Calling registered callbacks...");
    std::vector<HwAccelerationEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = hwAccelerationEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(hwAccelerationEventInfo, cbk.context);
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

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddHwAccelerationChangeEventsCallback
(
    taf_pa_data_HwAccelerationEventsCb callBack,
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

    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    HwAccelerationEventsCallbackEntry_t entry = {hwAccelerationEventsCallbackId_, callBack,
                                                                                        context};
    hwAccelerationEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = hwAccelerationEventsCallbackId_;
    // Increment the ID.
    hwAccelerationEventsCallbackId_++;
    PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    PA_INFO("Number of registered callbacks: %zu", hwAccelerationEventsCallbacks_.size());
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveHwAccelerationChangeEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = hwAccelerationEventsCallbacks_.begin();
        cbk != hwAccelerationEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            hwAccelerationEventsCallbacks_.erase(cbk);
            return PA_OK;
        }
    }
    PA_WARN("Callback not found. Id: %d", id);
    return PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendQosTftEventInfoToClients
(
    const QosTftEventInfo_t &qosTftEventsList
)
{
    PA_DEBUG("Calling registered callbacks...");
    std::vector<QosTftEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = qosTftEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(qosTftEventsList, cbk.context);
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

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddQosTftEventsCallback
(
    taf_pa_data_QosTftEventsCb callBack,
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
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    QosTftEventsCallbackEntry_t entry = {qosTftEventsCallbackId_, callBack, context};
    qosTftEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = qosTftEventsCallbackId_;
    // Increment the ID.
    qosTftEventsCallbackId_++;
    PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    PA_INFO("Number of registered callbacks: %zu", qosTftEventsCallbacks_.size());
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveQosTftEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = qosTftEventsCallbacks_.begin();
        cbk != qosTftEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            qosTftEventsCallbacks_.erase(cbk);
            return PA_OK;
        }
    }
    PA_WARN("Callback not found. Id: %d", id);
    return PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendThrottledApnEventInfoToClients
(
    const std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfo
)
{
    PA_DEBUG("Calling registered callbacks...");
    std::vector<ThrottledApnEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = throttledApnEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(throttledApnEventInfo, cbk.context);
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

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddThrottledApnEventsCallback
(
    taf_pa_data_ThrottledApnEventsCb callBack,
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
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    ThrottledApnEventsCallbackEntry_t entry = {throttledApnEventsCallbackId_, callBack, context};
    throttledApnEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = throttledApnEventsCallbackId_;
    // Increment the ID.
    throttledApnEventsCallbackId_++;
    PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    PA_INFO("Number of registered callbacks: %zu", throttledApnEventsCallbacks_.size());
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveThrottledApnEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for
    (
        auto cbk = throttledApnEventsCallbacks_.begin();
        cbk != throttledApnEventsCallbacks_.end(); ++cbk
    )
    {
        if (cbk->id == id)
        {
            PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            throttledApnEventsCallbacks_.erase(cbk);
            return PA_OK;
        }
    }
    PA_WARN("Callback not found. Id: %d", id);
    return PA_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendDataCallEventInfoToClients
(
    const taf::pa::data::DataCallEventInfo_t &dataCallEventInfo
)
{
    PA_DEBUG("Calling registered callbacks...");
    std::vector<DataCallEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = dataCallEventsCallbacks_;
    }
    // Call the service callback(s)
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(dataCallEventInfo, cbk.context);
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

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddDataCallEventsCallback
(
    taf_pa_data_CallEventsCb callBack,
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

    std::unique_lock lock(dataConnectionCbksMtx_);
    // Add the callback
    DataCallEventsCallbackEntry_t entry = {dataCallEventsCallbackId_, callBack, context};
    dataCallEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = dataCallEventsCallbackId_;
    // Increment the ID.
    dataCallEventsCallbackId_++;
    PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    PA_INFO("Number of registered callbacks: %zu", dataCallEventsCallbacks_.size());
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveDataCallEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    std::unique_lock lock(dataConnectionCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = dataCallEventsCallbacks_.begin(); cbk != dataCallEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
            dataCallEventsCallbacks_.erase(cbk);
            return PA_OK;
        }
    }
    PA_WARN("Callback not found. Id: %d", id);
    return PA_NOT_FOUND;
}

/*
 * Throughput Events Implementation
*/

void taf::pa::data::TafPaTeluxDataConnection::PaSendThroughputEventInfoToClients
(
    const std::vector<ThroughputInfo_t> &throughputInfoList
)
{
    // Gate: suppress SDK throughput events until at least one PA-level callback is registered.
    // This keeps the gating logic exclusively in the PA layer.
    if (!bThroughputEventsEnabled_.load())
    {
        PA_DEBUG("Throughput events gated: no callbacks registered yet, dropping event.");
        return;
    }
    PA_DEBUG("Calling registered callbacks...");
    std::vector<ThroughputEventsCallbackEntry_t> localCbksCopy;
    {
        // Lock and get a copy of the callbacks.
        std::shared_lock lock(dataConnectionCbksMtx_);
        localCbksCopy = throughputEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(throughputInfoList, cbk.context);
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

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddThroughputEventsCallback
(
    taf_pa_data_ThroughputEventsCb callBack,
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

    bool needRegister = false;
    {
        std::unique_lock lock(dataConnectionCbksMtx_);
        // Add the callback
        ThroughputEventsCallbackEntry_t entry = {throughputEventsCallbackId_, callBack, context};
        throughputEventsCallbacks_.push_back(entry);
        // Give ID back to app
        id = throughputEventsCallbackId_;
        // Increment the ID.
        throughputEventsCallbackId_++;
        PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
        PA_INFO("Number of registered callbacks: %zu", throughputEventsCallbacks_.size());

        // Decide first-registration under the lock, but do NOT call TelSDK while holding it.
        needRegister = (throughputEventsCallbacks_.size() == 1);
    } // Lock released here before any TelSDK call

    if (needRegister)
    {
        PA_INFO("First throughput callback added: registering THROUGHPUT indication with SDK.");
        telux::data::DataConnectionIndications throughputIndication;
        throughputIndication.set(
            telux::data::DataConnectionIndicationsType::THROUGHPUT);

        for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
        {
            auto managerIt = dataConnectionManagersMap_.find((SlotId)slotId);
            auto listenerIt = dataConnectionListenersMap_.find((SlotId)slotId);
            if (managerIt != dataConnectionManagersMap_.end() &&
                listenerIt != dataConnectionListenersMap_.end())
            {
                telux::common::Status status = managerIt->second->registerListener(
                    listenerIt->second, throughputIndication);
                if (telux::common::Status::SUCCESS == status)
                {
                    PA_INFO("THROUGHPUT indication registered for slot ID %d.", slotId);
                }
                else
                {
                    PA_ERROR("Failed to register THROUGHPUT indication for slot ID %d.", slotId);
                }
            }
        }
        bThroughputEventsEnabled_.store(true);
        PA_INFO("Throughput events gate opened: SDK events will now be forwarded to clients.");
    }
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveThroughputEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    auto &teluxPaData = taf::pa::data::TafPaTeluxData::GetInstance();
    SubsystemState_e phoneMngrState = teluxPaData.PaGetPhoneManagerInitState();
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != phoneMngrState, PA_FAULT,
                                                             "PA phone manager not initialized.");
    bool needDeregister = false;
    bool found = false;
    {
        std::unique_lock lock(dataConnectionCbksMtx_);
        // Iterate over the vector and remove the one with the provided id.
        for (
            auto cbk = throughputEventsCallbacks_.begin();
            cbk != throughputEventsCallbacks_.end();
            ++cbk)
        {
            if (cbk->id == id)
            {
                PA_INFO("Id: %d, Cbk: %p, Ctx: %p", id, cbk->callBack, cbk->context.get());
                throughputEventsCallbacks_.erase(cbk);
                found = true;
                needDeregister = throughputEventsCallbacks_.empty();
                // Close the gate immediately under the lock so no further events are forwarded
                // while we are about to deregister from TelSDK.
                if (needDeregister)
                    bThroughputEventsEnabled_.store(false);
                break;
            }
        }
    } // Lock released here before any TelSDK call

    if (!found)
    {
        PA_WARN("Callback not found. Id: %d", id);
        return PA_NOT_FOUND;
    }

    if (needDeregister)
    {
        PA_INFO("Last throughput callback removed: deregistering THROUGHPUT indication "
                "from SDK.");
        telux::data::DataConnectionIndications throughputIndication;
        throughputIndication.set(
            telux::data::DataConnectionIndicationsType::THROUGHPUT);

        for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
        {
            auto managerIt = dataConnectionManagersMap_.find((SlotId)slotId);
            auto listenerIt = dataConnectionListenersMap_.find((SlotId)slotId);
            if (managerIt != dataConnectionManagersMap_.end() &&
                listenerIt != dataConnectionListenersMap_.end())
            {
                telux::common::Status status = managerIt->second->deregisterListener(
                    listenerIt->second, throughputIndication);
                if (telux::common::Status::SUCCESS == status)
                {
                    PA_INFO("THROUGHPUT indication deregistered for slot ID %d.", slotId);
                }
                else
                {
                    PA_ERROR("Failed to deregister THROUGHPUT indication for slot ID %d.",
                             slotId);
                }
            }
        }
        PA_INFO("Throughput events gate closed: no remaining callbacks.");
    }
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaSetThroughputReportInterval
(
    PhoneId_e phoneId,
    uint32_t reportInterval
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::ErrorCode errorCode;

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
    PA_INFO("Phone Id: %d, Slot Id: %d, Interval: %u ms", TO_INT(phoneId), TO_INT(slotIDpa),
            reportInterval);

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    errorCode = dataConnectionManagersMap_[slotId]->setThroughputInterval(reportInterval);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_ERROR("setThroughputInterval failed. ErrorCode: %d", TO_INT(errorCode));
        return PA_FAULT;
    }

    PA_INFO("setThroughputInterval succeeded");
    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetLastThroughputInfo
(
    PhoneId_e phoneId,
    std::vector<ThroughputInfo_t> &throughputInfoList
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::ErrorCode errorCode;

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

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        PA_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return PA_FAULT;
    }

    std::vector<telux::data::ThroughputInfo> sdkThroughputInfoList;
    errorCode = dataConnectionManagersMap_[slotId]->getLastThroughputInfo(sdkThroughputInfoList);
    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        PA_ERROR("getLastThroughputInfo failed. ErrorCode: %d", TO_INT(errorCode));
        return PA_FAULT;
    }

    PA_INFO("getLastThroughputInfo succeeded. Count: %zu", sdkThroughputInfoList.size());

    // Convert SDK throughput info to PA throughput info
    throughputInfoList.clear();
    for (const auto &sdkInfo : sdkThroughputInfoList)
    {
        ThroughputInfo_t paInfo;
        taf::pa::data::Utils::ConvertThroughputInfo(sdkInfo, paInfo);
        // Add phone ID to the info
        paInfo.phoneId = phoneId;
        throughputInfoList.push_back(paInfo);
    }

    return PA_OK;
}

pa_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetMtuByInterfaceName
(
    const std::string& interfaceName,
    int32_t& mtu
)
{
    if (interfaceName.empty())
    {
        PA_ERROR("Interface name is empty");
        return PA_BAD_PARAMETER;
    }

    // Iterate through all slot listeners to find the data call with the matching interface name.
    // The MTU is read directly from IpAddrInfo (SDK-provided) via the active IDataCall object.
    for (auto& entry : tafPaTeluxDataConnectionListenersMap_)
    {
        pa_result_t result = entry.second->GetMtuByInterfaceName(interfaceName, mtu);
        if (PA_OK == result)
        {
            return PA_OK;
        }
        else if (PA_NOT_FOUND != result)
        {
            // Interface found but MTU not available
            return result;
        }
        // PA_NOT_FOUND: interface not in this slot's listener, try next slot
    }

    PA_WARN("Interface %s not found in any active data calls", interfaceName.c_str());
    return PA_NOT_FOUND;
}
