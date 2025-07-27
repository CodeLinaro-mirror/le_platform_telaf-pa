/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataConnectionImpl.cpp
 * @brief Telux Data connection implementation.
 *
 */

#include "taf_pa_data_Utils.hpp"
#include "taf_pa_data_TeluxData.hpp"
#include "taf_pa_data_TeluxDataConnection.hpp"
#include "tafSvcIF.hpp"

taf::pa::data::TafPaTeluxDataConnection &taf::pa::data::TafPaTeluxDataConnection::GetInstance()
{
    static TafPaTeluxDataConnection instance;
    return instance;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetInitState(bool &initState)
{
    LE_DEBUG("Conn init state: %d", bDataConnectionMngrInitialized_);
    initState = bDataConnectionMngrInitialized_;
    return LE_OK;
}

void taf::pa::data::TafPaTeluxDataConnection::Init(taf::pa::data::SlotCount_e slotCount)
{
    slotCount_ = slotCount;
    initDataConnectionManagers();
    PaRegisterDataConnCallbacks();
}

void taf::pa::data::TafPaTeluxDataConnection::Deinit()
{
    PaDeregisterDataConnCallbacks();
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

    LE_DEBUG("data callback details from: %s", fromPtr);
    LE_DEBUG("profile id:           %d", profileId);
    LE_DEBUG("slot id:              %d", slotId);
    LE_DEBUG("interface name:       %s", dataCall->getInterfaceName().c_str());
    callStatus = dataCall->getDataCallStatus();
    LE_DEBUG("call status:          %s", taf::pa::data::Utils::CallStatusToString(callStatus));
    LE_DEBUG("ip type:              %s", taf::pa::data::Utils::IpFamilyTypeToString(
                                             dataCall->getIpFamilyType()));
    LE_DEBUG("ipv4 status:          %s", taf::pa::data::Utils::CallStatusToString(
                                             dataCall->getIpv4Info().status));
    if (telux::data::DataCallStatus::NET_CONNECTED == dataCall->getIpv4Info().status)
    {
        unsigned int mask = 0;
        LE_DEBUG("IPv4 Address Info:");
        LE_DEBUG("  Interface addr : %s", dataCall->getIpv4Info().addr.ifAddress.c_str());
        mask = dataCall->getIpv4Info().addr.ifMask;
        LE_DEBUG("  Interface mask : 0x%X(%u.%u.%u.%u)", mask,
                                                        (mask >> 24) & 0xFF,
                                                        (mask >> 16) & 0xFF,
                                                        (mask >> 8)  & 0xFF,
                                                         mask        & 0xFF);
        LE_DEBUG("  Gateway addr   : %s", dataCall->getIpv4Info().addr.gwAddress.c_str());
        mask = dataCall->getIpv4Info().addr.gwMask;
        LE_DEBUG("  Gateway mask   : 0x%X(%u.%u.%u.%u)", mask,
                                                        (mask >> 24) & 0xFF,
                                                        (mask >> 16) & 0xFF,
                                                        (mask >> 8)  & 0xFF,
                                                         mask        & 0xFF);
        LE_DEBUG("  Pri DNS addr   : %s", dataCall->getIpv4Info().addr.primaryDnsAddress.c_str());
        LE_DEBUG("  Sec DNS addr   : %s", dataCall->getIpv4Info().addr.secondaryDnsAddress.c_str());
    }
    LE_DEBUG("ipv6 status:          %s", taf::pa::data::Utils::CallStatusToString(
                                             dataCall->getIpv6Info().status));
    if (telux::data::DataCallStatus::NET_CONNECTED == dataCall->getIpv6Info().status)
    {
        LE_DEBUG("IPv6 Address Info:");
        LE_DEBUG("  Interface addr : %s", dataCall->getIpv6Info().addr.ifAddress.c_str());
        LE_DEBUG("  Interface mask : 0x%X", dataCall->getIpv6Info().addr.ifMask);
        LE_DEBUG("  Gateway addr   : %s", dataCall->getIpv6Info().addr.gwAddress.c_str());
        LE_DEBUG("  Gateway mask   : 0x%X", dataCall->getIpv6Info().addr.gwMask);
        LE_DEBUG("  Pri DNS addr   : %s", dataCall->getIpv6Info().addr.primaryDnsAddress.c_str());
        LE_DEBUG("  Sec DNS addr   : %s", dataCall->getIpv6Info().addr.secondaryDnsAddress.c_str());
    }
    /*
    std::list<telux::data::IpAddrInfo> ipAddrList = dataCall->getIpAddressInfo();
    for (auto &it : ipAddrList)
    {
        LE_DEBUG("interface addr:       %s", it.ifAddress.c_str());
        LE_DEBUG("gateway   addr:       %s", it.gwAddress.c_str());
        LE_DEBUG("primary dns addr:     %s", it.primaryDnsAddress.c_str());
        LE_DEBUG("secondary dns addr:   %s", it.secondaryDnsAddress.c_str());
    }
    */
    telux::common::DataCallEndReason reason = dataCall->getDataCallEndReason();
    LE_DEBUG("call end reason type:   %s",
                                     taf::pa::data::Utils::CallEndReasonTypeToString(reason.type));
    if (telux::data::DataCallStatus::NET_NO_NET == callStatus ||
        telux::data::DataCallStatus::NET_DISCONNECTING == callStatus)
    {
        switch (reason.type)
        {
        case telux::data::EndReasonType::CE_MOBILE_IP:
            LE_DEBUG("call end MIP reason code: %d", TO_INT(reason.IpCode));
            break;
        case telux::data::EndReasonType::CE_INTERNAL:
            LE_DEBUG("call end internal reason code: %d", TO_INT(reason.internalCode));
            break;
        case telux::data::EndReasonType::CE_CALL_MANAGER_DEFINED:
            LE_DEBUG("call end CM reason code: %d", TO_INT(reason.cmCode));
            break;
        case telux::data::EndReasonType::CE_3GPP_SPEC_DEFINED:
            LE_DEBUG("call end 3GPP spec reason code: %d", TO_INT(reason.specCode));
            break;
        case telux::data::EndReasonType::CE_PPP:
            LE_DEBUG("call end PPP reason code: %d", TO_INT(reason.pppCode));
            break;
        case telux::data::EndReasonType::CE_EHRPD:
            LE_DEBUG("call end EHRPD reason code: %d", TO_INT(reason.ehrpdCode));
            break;
        case telux::data::EndReasonType::CE_IPV6:
            LE_DEBUG("call end IPv6 reason code: %d", TO_INT(reason.ipv6Code));
            break;
        case telux::data::EndReasonType::CE_HANDOFF:
            LE_DEBUG("call end handoff reason code: %d", TO_INT(reason.handOffCode));
            break;
        default:
            LE_DEBUG("Invalid Reason code: %d", TO_INT(reason.type));
            break;
        }
    }

    LE_DEBUG("tech preference:      %s", taf::pa::data::Utils::TechPreferenceToString(
                                             dataCall->getTechPreference()));
    LE_DEBUG("DataBearerTechnology: %s", taf::pa::data::Utils::DataBearerToString(
                                             dataCall->getCurrentBearerTech()));

    return;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaGetDefaultProfile
(
    const taf::pa::data::PhoneId_e phoneId,
    taf::pa::data::ProfileId_e &profileId
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        LE_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
    }

    // Lock the mutex to protect access to getDefProfilePromise
    std::lock_guard<std::mutex> lock(getDefProfileMtx_);

    // Init promise and get a future
    getDefProfilePromise = std::promise<std::tuple<int, SlotId, telux::common::ErrorCode>>();
    std::future<std::tuple<int, SlotId, telux::common::ErrorCode>> future =
                                                                getDefProfilePromise.get_future();

    // Define a lambda function to handle the callback and set the promise
    status = dataConnectionManagersMap_[slotId]->getDefaultProfile
                (
                    telux::data::OperationType::DATA_LOCAL,
                    // Lambda callback
                    [this]
                    (
                        int profileId, SlotId slotId, telux::common::ErrorCode error
                    )
                    {
                        SET_SDK_THREAD_NAME();
                        getDefProfilePromise.set_value(std::make_tuple(profileId, slotId, error));
                    }
                );
    if (telux::common::Status::SUCCESS != status)
    {
        LE_ERROR("getDefaultProfile failed. Status: %d", TO_INT(status));
        return LE_FAULT;
    }

    // Wait for the callback to complete and capture the results
    LE_DEBUG("Wait for callback..");
    std::tuple<int, SlotId, telux::common::ErrorCode> futResult;
    FUTURE_GET_RET_VAL(future, futResult, LE_FAULT);
    int futProfileId = std::get<0>(futResult);
    SlotId futSlotId = std::get<1>(futResult);
    telux::common::ErrorCode futError = std::get<2>(futResult);

    if (telux::common::ErrorCode::SUCCESS != futError)
    {
        LE_ERROR("getDefaultProfile failed in callback. Error: %d", TO_INT(futError));
        return LE_FAULT;
    }
    LE_DEBUG("Slot Id: %d", TO_INT(futSlotId));
    LE_DEBUG("Profile: %d", futProfileId);

    if (futSlotId != slotId)
    {
        LE_ERROR("Slot ID mismatch. Expected: %d, Actual: %d", TO_INT(slotId), TO_INT(futSlotId));
        return LE_FAULT;
    }
    profileId = static_cast<taf::pa::data::ProfileId_e>(futProfileId);

    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaSetDefaultProfile
(
    const taf::pa::data::PhoneId_e phoneId,
    const taf::pa::data::ProfileId_e profileId
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Phone Id: %d, Slot Id: %d", TO_INT(phoneId), TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        LE_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
    }

    // Lock the mutex to protect access to setDefProfilePromise
    std::lock_guard<std::mutex> lock(setDefProfileMtx_);

    // Init promise and get a future
    setDefProfilePromise = std::promise<telux::common::ErrorCode>();
    std::future<telux::common::ErrorCode> future = setDefProfilePromise.get_future();


    LE_INFO("Profile ID to set: %d", TO_INT(profileId));
    status = dataConnectionManagersMap_[slotId]->setDefaultProfile
                (
                    telux::data::OperationType::DATA_LOCAL,
                    static_cast<uint8_t>(profileId),
                    [this](telux::common::ErrorCode error)
                    {
                        SET_SDK_THREAD_NAME();
                        setDefProfilePromise.set_value(error);
                    }
                );
    if (telux::common::Status::SUCCESS != status)
    {
        LE_ERROR("setDefaultProfile failed. Status: %d", TO_INT(status));
        return LE_FAULT;
    }

    // Wait for the callback to complete and capture the results
    LE_DEBUG("Wait for callback..");
    telux::common::ErrorCode futError;
    FUTURE_GET_RET_VAL(future, futError, LE_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futError)
    {
        LE_ERROR("setDefaultProfile failed in callback. Error: %d", TO_INT(futError));
        return LE_FAULT;
    }
    LE_DEBUG("setDefaultProfile done.");
    return LE_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Private functions
 */
////////////////////////////////////////////////////////////////////////////////////////////////////

void taf::pa::data::TafPaTeluxDataConnection::initDataConnectionManagers()
{
    if (bDataConnectionMngrInitialized_)
    {
        LE_INFO("Data connection managers already initialized.");
        return;
    }

    // Get the data factory
    auto &dataFactory = telux::data::DataFactory::getInstance();
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Initialize the data connection manager for each slot
        auto  dataConnPromPtr = std::make_shared<std::promise<telux::common::ServiceStatus>>();
        std::future<telux::common::ServiceStatus> dataConnFut = dataConnPromPtr->get_future();

        auto dataProfMngr = dataFactory.getDataConnectionManager(
            // Lambda callback function. Sets the service status
            (SlotId)slotId, [&](telux::common::ServiceStatus svcStatus)
            {
                if (bWaitingOnDataConnMngrProm_.load())
                {
                    LE_INFO("getDataConnectionManager for slot ID: %d", TO_INT(slotId));
                    LE_INFO("Data connection manager status      : %d", TO_INT(svcStatus));

                    dataConnPromPtr->set_value(svcStatus);
                }
                else
                {
                    LE_WARN("dataConnPromPtr->set_value() not called");
                } });
        if (!dataProfMngr)
        {
            LE_ERROR("Failed to get Data connection manager.");
            return;
        }
        else
        {
            // Mark that promise should be completed.
            bWaitingOnDataConnMngrProm_.store(true);

            telux::common::ServiceStatus dataServSysMgrStatus;
            LE_INFO("Waiting for Data connection manager to be ready...");
            std::future_status waitStatus = dataConnFut.wait_for(
                std::chrono::seconds(taf::pa::SUBSYSTEM_INIT_TIMEOUT));
            if (std::future_status::timeout == waitStatus)
            {
                // Mark that promise completion is not needed.
                bWaitingOnDataConnMngrProm_.store(false);
                LE_ERROR("Timeout waiting for Data connection manager");
                return;
            }
            else
            {
                FUTURE_GET_RET_NIL(dataConnFut, dataServSysMgrStatus);
                LE_INFO("dataConnMngr for slot %d status: %d", slotId, TO_INT(dataServSysMgrStatus));
                // Mark that promise completion is not needed.
                bWaitingOnDataConnMngrProm_.store(false);
            }
            if (telux::common::ServiceStatus::SERVICE_AVAILABLE == dataServSysMgrStatus)
            {
                LE_INFO("dataConnMngr for slot %d: AVAILABLE", slotId);
                // Store the manager in the map with slot Id as index
                dataConnectionManagersMap_.emplace((SlotId)slotId, dataProfMngr);
                // Initialize and store the callback objects for later registration.
                tafPaTeluxDataConnectionListenersMap_.emplace((SlotId)slotId,
                                std::make_shared<TafPaTeluxDataConnectionListener>((SlotId)slotId));
                dataConnectionListenersMap_[(SlotId)slotId] =
                                            tafPaTeluxDataConnectionListenersMap_[(SlotId)slotId];
                // Mark listener as not registered.
                bDataConnectionListenersRegistered_[slotId-1].store(false);
            }
            else if (telux::common::ServiceStatus::SERVICE_UNAVAILABLE == dataServSysMgrStatus)
            {
                LE_WARN("dataConnMngr for slot %d: UNAVAILABLE", slotId);
                // This is a temporary unavailability. Try after a delay
                // TODO
                return;
            }
            else
            {
                // Fatal error. TODO
                LE_ERROR("Failed to init Data connection manager for slot ID %d", slotId);
                return;
            }
        }
        LE_INFO("Data connection manager initialization for slot %d complete", slotId);
    }
    // Update that the data connection manager is initialized.
    bDataConnectionMngrInitialized_ = true;
    return;
}

void taf::pa::data::TafPaTeluxDataConnection::deInitDataConnectionManagers()
{
    LE_INFO("Clear dataConnectionManagersMap_");
    dataConnectionManagersMap_.clear();
    LE_INFO("Clear tafPaTeluxDataConnectionListenersMap_");
    tafPaTeluxDataConnectionListenersMap_.clear();
    LE_INFO("Clear dataConnectionListenersMap_");
    dataConnectionListenersMap_.clear();
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaRegisterDataConnCallbacks()
{
    // Protect the critical section
    std::lock_guard<std::mutex> lock(cbksMtx_);

    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Register serving system listener for each slot it
        if (bDataConnectionListenersRegistered_[slotId - 1].load())
        {
            LE_INFO("Serving System listeners already registered for slot ID: %d.", slotId);
        }
        else
        {
            if (dataConnectionManagersMap_.find((SlotId)slotId) != dataConnectionManagersMap_.end())
            {
                if (dataConnectionManagersMap_[(SlotId)slotId]->registerListener(
                                                dataConnectionListenersMap_[(SlotId)slotId]) ==
                    telux::common::Status::SUCCESS)
                {
                    LE_INFO("Data connection listener for slot ID %d registered.", slotId);
                    bDataConnectionListenersRegistered_[slotId - 1].store(true);
                }
                else
                {
                    LE_ERROR("Failed to register data connection listener for slot ID %d.", slotId);
                }
            }
        }
    }
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaDeregisterDataConnCallbacks()
{
    // Protect the critical section
    std::lock_guard<std::mutex> lock(cbksMtx_);
    for (auto slotId = 1; slotId <= TO_INT(slotCount_); slotId++)
    {
        // Register serving system listener for each slot it
        if (bDataConnectionListenersRegistered_[slotId - 1].load())
        {
            if (dataConnectionManagersMap_.find((SlotId)slotId) != dataConnectionManagersMap_.end())
            {
                if (dataConnectionManagersMap_[(SlotId)slotId]->deregisterListener(
                                                dataConnectionListenersMap_[(SlotId)slotId]) ==
                    telux::common::Status::SUCCESS)
                {
                    LE_INFO("Data connection listener for slot ID %d deregistered.", slotId);
                    bDataConnectionListenersRegistered_[slotId - 1].store(false);
                }
                else
                {
                    LE_ERROR("Failed to deregister data connection listener for slot ID %d.",
                                                                                        slotId);
                }
            }
        }
        else
        {
            LE_INFO("Serving System listeners already deregistered for slot ID: %d.", slotId);
        }
    }
    return LE_OK;
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

    TAF_ERROR_IF_RET_NIL(nullptr == iDataCall, "iDataCall is NULL, drop this event");

    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        LE_ERROR("Starting data session failed with error code: %d", TO_INT(errorCode));
    }

    // Log the call data
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.LogDataCallInfo(iDataCall, __func__);

    // Fill DataCallEventInfo_t
    DataCallEventInfo_t eventInfo;
    taf::pa::data::PhoneId_e phoneId;
    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());

    auto &teluxPaData    = TafPaTeluxData::GetInstance();
    teluxPaData.PaGetPhoneIdFromSimSlotId(eventInfo.slotId, phoneId);

    eventInfo.phoneId    = phoneId;
    eventInfo.profileId  = static_cast<taf::pa::data::ProfileId_e>(iDataCall->getProfileId());
    eventInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(iDataCall->getDataCallStatus());
    eventInfo.ipType     = taf::pa::data::Utils::ConvertIpType(iDataCall->getIpFamilyType());

    // Send the data call event info to registered clients
    teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
    return;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaStartDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params
)
{
    telux::data::DataCallParams teluxParams;
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    teluxParams.profileId = static_cast<int>(params.profileId);
    teluxParams.ipFamilyType = taf::pa::data::Utils::ConvertIpType(params.ipType);

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(params.phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(params.phoneId));
        return result;
    }
    LE_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        LE_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
    }

    status = dataConnectionManagersMap_[slotId]->startDataCall(teluxParams, startDataCallCallback);
    if (telux::common::Status::SUCCESS != status)
    {
        LE_ERROR("startDataCall failed. Status: %d", TO_INT(status));
        return LE_FAULT;
    }
    return LE_OK;
}

void taf::pa::data::TafPaTeluxDataConnection::stopDataCallCallback
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall,
    telux::common::ErrorCode errorCode
)
{
    SET_SDK_THREAD_NAME();

    TAF_ERROR_IF_RET_NIL(nullptr == iDataCall, "iDataCall is NULL, drop this event");

    if (telux::common::ErrorCode::SUCCESS != errorCode)
    {
        LE_ERROR("Starting data session failed with error code: %d", TO_INT(errorCode));
    }

    // Log the call data
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.LogDataCallInfo(iDataCall, __func__);

    // Fill DataCallEventInfo_t
    DataCallEventInfo_t eventInfo;
    taf::pa::data::PhoneId_e phoneId;
    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    teluxPaData.PaGetPhoneIdFromSimSlotId(eventInfo.slotId, phoneId);
    eventInfo.phoneId = phoneId;
    eventInfo.profileId = static_cast<taf::pa::data::ProfileId_e>(iDataCall->getProfileId());
    eventInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(iDataCall->getDataCallStatus());
    eventInfo.ipType     = taf::pa::data::Utils::ConvertIpType(iDataCall->getIpFamilyType());

    // Send the data call event info to registered clients
    teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
    return;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaStopDataSessionAsync
(
    const taf::pa::data::DataCallStartStopParams_t& params
)
{
    telux::data::DataCallParams teluxParams;
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;

    teluxParams.profileId = static_cast<int>(params.profileId);
    teluxParams.ipFamilyType = taf::pa::data::Utils::ConvertIpType(params.ipType);

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(params.phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(params.phoneId));
        return result;
    }
    LE_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        LE_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
    }

    status = dataConnectionManagersMap_[slotId]->stopDataCall(teluxParams, stopDataCallCallback);
    if (telux::common::Status::SUCCESS != status)
    {
        LE_ERROR("stopDataCall failed. Status: %d", TO_INT(status));
        return LE_FAULT;
    }
    return LE_OK;
}

// The callback fuction for TelSDK requestThrottledApnInfo()
void taf::pa::data::TafPaTeluxDataConnection::requestThrottledApnInfoCallback
(
    const std::vector<telux::data::APNThrottleInfo> &throttleInfoList,
    telux::common::ErrorCode error
)
{
    SET_SDK_THREAD_NAME();
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.requestThrottledApnInfoPromise_.set_value(
                                                std::make_pair(throttleInfoList, error));
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::paGetThrottledApnInfo
(
    const taf::pa::data::PhoneId_e        phoneId,
    std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
)
{
    taf::pa::data::SlotId_e slotIDpa;
    telux::common::Status status;
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    le_result_t result = teluxPaData.PaGetSimSlotIdFromPhoneId(phoneId, slotIDpa);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get slot ID for phone ID %d.", TO_INT(phoneId));
        return result;
    }
    LE_INFO("Slot Id: %d", TO_INT(slotIDpa));

    SlotId slotId = taf::pa::data::Utils::ConvertSlotId(slotIDpa);

    if (dataConnectionManagersMap_.find(slotId) == dataConnectionManagersMap_.end())
    {
        LE_ERROR("Connection manager is not init for slot %d", TO_INT(slotId));
        return LE_FAULT;
    }

    // Lock
    std::lock_guard<std::mutex> lock(requestThrottledApnInfoMtx_);

    requestThrottledApnInfoPromise_ = std::promise<
                std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode>>();
    auto fut = requestThrottledApnInfoPromise_.get_future();

    status = dataConnectionManagersMap_[slotId]->requestThrottledApnInfo(
                                                            requestThrottledApnInfoCallback);
    if (telux::common::Status::SUCCESS != status)
    {
        LE_ERROR("requestThrottledApnInfo failed. Status: %d", TO_INT(status));
        return LE_FAULT;
    }

    LE_DEBUG("Wait for callback ...");

    std::pair<std::vector<telux::data::APNThrottleInfo>, telux::common::ErrorCode> futResult;
    FUTURE_GET_RET_VAL(fut, futResult, LE_FAULT);
    if (telux::common::ErrorCode::SUCCESS != futResult.second)
    {
        LE_ERROR("requestThrottledApnInfo error: %d", TO_INT(futResult.second));
        return LE_FAULT;
    }
    std::vector<telux::data::APNThrottleInfo> throttleInfoList = futResult.first;
    size_t numThrottledApn = throttleInfoList.size();
    LE_DEBUG("Number of throttled APN(s): %zu", numThrottledApn);
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

    return LE_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * SDK to PA event callbacks
 */
////////////////////////////////////////////////////////////////////////////////////////////////////
void taf::pa::data::TafPaTeluxDataConnection::PaSendHwAccelerationEventInfoToClients
(
    const HwAccelerationChangeEvent_t &hwAccelerationEventInfo
)
{
    LE_DEBUG("Calling registered callbacks...");
    // Call the service callback(s)
    for (auto &cbk : hwAccelerationEventsCallbacks_)
    {
        LE_DEBUG("Calling callback: %d", cbk.id);
        cbk.callBack(hwAccelerationEventInfo, cbk.context);
    }
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddHwAccelerationChangeEventsCallback
(
    taf_pa_data_HwAccelerationEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_ERROR_IF_RET_VAL(nullptr == callBack, LE_BAD_PARAMETER, "callBack is NULL!");

    // Add the callback
    HwAccelerationEventsCallbackEntry_t entry = {hwAccelerationEventsCallbackId_, callBack, context};
    hwAccelerationEventsCallbacks_.push_back(entry);
    // Increment the ID.
    hwAccelerationEventsCallbackId_++;
    // Give ID back to app
    id = hwAccelerationEventsCallbackId_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveHwAccelerationChangeEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = hwAccelerationEventsCallbacks_.begin();
        cbk != hwAccelerationEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            hwAccelerationEventsCallbacks_.erase(cbk);
            return LE_OK;
        }
    }
    LE_WARN("Callback not found. Id: %d", id);
    return LE_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendQosTftEventInfoToClients
(
    const QosTftEventInfo_t &qosTftEventsList
)
{
    LE_DEBUG("Calling registered callbacks...");
    // Call the service callback(s)
    for (auto &cbk : qosTftEventsCallbacks_)
    {
        LE_DEBUG("Calling callback: %d", cbk.id);
        cbk.callBack(qosTftEventsList, cbk.context);
    }
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddQosTftEventsCallback
(
    taf_pa_data_QosTftEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_ERROR_IF_RET_VAL(nullptr == callBack, LE_BAD_PARAMETER, "callBack is NULL!");

    // Add the callback
    QosTftEventsCallbackEntry_t entry = {qosTftEventsCallbackId_, callBack, context};
    qosTftEventsCallbacks_.push_back(entry);
    // Increment the ID.
    qosTftEventsCallbackId_++;
    // Give ID back to app
    id = qosTftEventsCallbackId_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveQosTftEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    // Iterate over the vector and remove the one with the provided id.
    for (
        auto cbk = qosTftEventsCallbacks_.begin();
        cbk != qosTftEventsCallbacks_.end();
        ++cbk)
    {
        if (cbk->id == id)
        {
            qosTftEventsCallbacks_.erase(cbk);
            return LE_OK;
        }
    }
    LE_WARN("Callback not found. Id: %d", id);
    return LE_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendThrottledApnEventInfoToClients
(
    const std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfo
)
{
    LE_DEBUG("Calling registered callbacks...");
    // Call the service callback(s)
    for (auto &cbk : throttledApnEventsCallbacks_)
    {
        LE_DEBUG("Calling callback: %d", cbk.id);
        cbk.callBack(throttledApnEventInfo, cbk.context);
    }
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddThrottledApnEventsCallback
(
    taf_pa_data_ThrottledApnEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_ERROR_IF_RET_VAL(nullptr == callBack, LE_BAD_PARAMETER, "callBack is NULL!");

    // Add the callback
    ThrottledApnEventsCallbackEntry_t entry = {throttledApnEventsCallbackId_, callBack, context};
    throttledApnEventsCallbacks_.push_back(entry);
    // Increment the ID.
    throttledApnEventsCallbackId_++;
    // Give ID back to app
    id = throttledApnEventsCallbackId_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveThrottledApnEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    // Iterate over the vector and remove the one with the provided id.
    for
    (
        auto cbk = throttledApnEventsCallbacks_.begin();
        cbk != throttledApnEventsCallbacks_.end(); ++cbk
    )
    {
        if (cbk->id == id)
        {
            throttledApnEventsCallbacks_.erase(cbk);
            return LE_OK;
        }
    }
    LE_WARN("Callback not found. Id: %d", id);
    return LE_NOT_FOUND;
}

void taf::pa::data::TafPaTeluxDataConnection::PaSendDataCallEventInfoToClients
(
    const taf::pa::data::DataCallEventInfo_t &dataCallEventInfo
)
{
    LE_DEBUG("Calling registered callbacks...");
    // Call the service callback(s)
    for (auto &cbk : dataCallEventsCallbacks_)
    {
        LE_DEBUG("Calling callback: %d", cbk.id);
        cbk.callBack(dataCallEventInfo, cbk.context);
    }
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaAddDataCallEventsCallback
(
    taf_pa_data_CallEventsCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
)
{
    TAF_ERROR_IF_RET_VAL(nullptr == callBack, LE_BAD_PARAMETER, "callBack is NULL!");

    // Add the callback
    DataCallEventsCallbackEntry_t entry = {dataCallEventsCallbackId_, callBack, context};
    dataCallEventsCallbacks_.push_back(entry);
    // Increment the ID.
    dataCallEventsCallbackId_++;
    // Give ID back to app
    id = dataCallEventsCallbackId_;
    return LE_OK;
}

le_result_t taf::pa::data::TafPaTeluxDataConnection::PaRemoveDataCallEventsCallback
(
    uint16_t id
    ///< [IN] The ID of the registered callback.
)
{
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = dataCallEventsCallbacks_.begin(); cbk != dataCallEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            dataCallEventsCallbacks_.erase(cbk);
            return LE_OK;
        }
    }
    LE_WARN("Callback not found. Id: %d", id);
    return LE_NOT_FOUND;
}
