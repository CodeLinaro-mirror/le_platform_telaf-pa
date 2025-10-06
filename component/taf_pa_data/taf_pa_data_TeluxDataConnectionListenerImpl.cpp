/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataConnectionListenerImpl.cpp
 * @brief Telux Data connection listener implementation.
 *
 */

#include "taf_pa_data_Utils.hpp"
#include "taf_pa_data_TeluxData.hpp"
#include "taf_pa_data_TeluxDataConnection.hpp"

void taf::pa::data::TafPaTeluxDataConnectionListener::fillCallEndReason
(
    const telux::common::DataCallEndReason &teluxReason,
     DataCallEndReason_t &paReason
)
{
    using namespace taf::pa::data;

    // Initialize to default values.
    paReason.reason = CallEndReason_e::CE_REASON_UNKNOWN;
    paReason.internalCode = static_cast<InternalReasonCode_e>(-1);

    paReason.reason = static_cast<CallEndReason_e>(teluxReason.type);
    switch (teluxReason.type)
    {
    case telux::common::EndReasonType::CE_MOBILE_IP:
        paReason.mipCode = static_cast<MobileIpReasonCode_e>(teluxReason.IpCode);
        break;
    case telux::common::EndReasonType::CE_INTERNAL:
        paReason.internalCode = static_cast<InternalReasonCode_e>(teluxReason.internalCode);
        break;
    case telux::common::EndReasonType::CE_CALL_MANAGER_DEFINED:
        paReason.cmCode = static_cast<CallManagerReasonCode_e>(teluxReason.cmCode);
        break;
    case telux::common::EndReasonType::CE_3GPP_SPEC_DEFINED:
        paReason.specCode = static_cast<SpecReasonCode_e>(teluxReason.specCode);
        break;
    case telux::common::EndReasonType::CE_PPP:
        paReason.pppCode = static_cast<PPPReasonCode_e>(teluxReason.pppCode);
        break;
    case telux::common::EndReasonType::CE_EHRPD:
        paReason.ehrpdCode = static_cast<EHRPDReasonCode_e>(teluxReason.ehrpdCode);
        break;
    case telux::common::EndReasonType::CE_IPV6:
        paReason.ipv6Code = static_cast<Ipv6ReasonCode_e>(teluxReason.ipv6Code);
        break;
    case telux::common::EndReasonType::CE_HANDOFF:
        paReason.handOffCode = static_cast<HandoffReasonCode_e>(teluxReason.handOffCode);
        break;
    default:
        PA_WARN("Invalid Reason type: %d", static_cast<int32_t>(teluxReason.type));
        break;
    }
}

pa_result_t taf::pa::data::TafPaTeluxDataConnectionListener::updateBitRate
(
    const std::shared_ptr<telux::data::IDataCall> &dataCall,
    telux::data::BitRateInfo &bitRate
)
{
    telux::data::DataCallStatus datacallStatus = dataCall->getDataCallStatus();
    // For debugging
    void *rawPtr = static_cast<void *>(dataCall.get());

    bitRate.maxRxRate = 0;
    bitRate.maxTxRate = 0;

    // Check if this IDataCall object exists in the map and compare status atomically
    {
        std::lock_guard<std::mutex> lock(listenerMtx_);
        auto it = callStatusMap_.find(dataCall);
        if (it == callStatusMap_.end())
        {
            PA_DEBUG("IDataCall %p not found in callStatusMap_", rawPtr);
            // First call. Proceed to get the bit rate
        }
        else
        {
            PA_DEBUG("IDataCall %p found in callStatusMap_", rawPtr);
            // Check if the data call status is the same or has changed.
            if (it->second == datacallStatus)
            {
                PA_DEBUG("IDataCall %p data call status has not changed.", rawPtr);
                return PA_DUPLICATE;
            }
        }
    }

    // Promise and future used for synchronization.
    std::promise<bool> dataCallBitratePromise = std::promise<bool>();
    std::future<bool> fut = dataCallBitratePromise.get_future();
    std::chrono::seconds span(taf::pa::NON_NETWORK_COMMAND_TIMEOUT); // 5 seconds

    // requestDataCallBitRate callback lambda
    auto respCb = [&bitRate, &dataCallBitratePromise, this](telux::data::BitRateInfo &cbkBitRate,
                                                                telux::common::ErrorCode errorCode)
    {
        SET_SDK_THREAD_NAME();
        bool bResult = false;
        if (telux::common::ErrorCode::SUCCESS == errorCode)
        {
            // Success
            PA_DEBUG("maxRxRate: %" PRIu64 "", cbkBitRate.maxRxRate);
            PA_DEBUG("maxTxRate: %" PRIu64 "", cbkBitRate.maxTxRate);
            bitRate.maxRxRate = cbkBitRate.maxRxRate;
            bitRate.maxTxRate = cbkBitRate.maxTxRate;
            bResult = true;
        }
        else
        {
            PA_WARN("requestDataCallBitRateCb error: %d", static_cast<int>(errorCode));
        }
        if (bWaitingForDataCallBitratePromise_.load())
        {
            PA_DEBUG("dataCallBitratePromise.set_value(%s)", bResult ? "true" : "false");
            dataCallBitratePromise.set_value(bResult);
        }
        else
        {
            PA_DEBUG("dataCallBitratePromise.set_value() skipped");
        }
    };
    // Mark that future is waiting for promise
    bWaitingForDataCallBitratePromise_.store(true);
    telux::common::Status status = dataCall->requestDataCallBitRate(respCb);
    if (telux::common::Status::SUCCESS == status)
    {
        PA_DEBUG("requestDataCallBitRate SUCCESS. Wait for cbk");
        std::future_status waitStatus = fut.wait_for(span);
        if (std::future_status::timeout == waitStatus)
        {
            // Mark that future is not waiting for promise
            bWaitingForDataCallBitratePromise_.store(false);
            PA_ERROR("requestDataCallBitRate promise timeout");
            return PA_TIMEOUT;
        }
        bool bFutResult;
        FUTURE_GET_RET_VAL(fut, bFutResult, PA_FAULT);
        if (bFutResult)
        {
            // Mark that future is not waiting for promise
            bWaitingForDataCallBitratePromise_.store(false);
            PA_DEBUG("requestDataCallBitRate SUCCESS");
            return PA_OK;
        }
    }
    else
    {
        // Mark that future is not waiting for promise
        bWaitingForDataCallBitratePromise_.store(false);
        PA_WARN("requestDataCallBitRate failed: %d", static_cast<int>(status));
    }
    PA_DEBUG("requestDataCallBitRate failed");
    return PA_FAULT;
}

void taf::pa::data::TafPaTeluxDataConnectionListener::ParseIDataCall
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall,
    DataCallEventInfo_t &eventInfo
)
{
    auto &teluxPaData = TafPaTeluxData::GetInstance();
    taf::pa::data::PhoneId_e phoneId;

    telux::data::DataCallStatus datacallStatus = iDataCall->getDataCallStatus();

    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());

    pa_result_t result = teluxPaData.PaGetPhoneIdFromSlotId(eventInfo.slotId, phoneId);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "PaGetPhoneIdFromSlotId err: %d. Dropping event",
                                                                                          result);
    telux::data::IpFamilyInfo IPv4Info = iDataCall->getIpv4Info();
    telux::data::IpFamilyInfo IPv6Info = iDataCall->getIpv6Info();

    // Fill DataCallEventInfo_t
    eventInfo.phoneId = phoneId;
    eventInfo.profileId = static_cast<taf::pa::data::ProfileId_e>(iDataCall->getProfileId());
    eventInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(datacallStatus);
    eventInfo.ipType = taf::pa::data::Utils::ConvertIpType(iDataCall->getIpFamilyType());
    eventInfo.techPref = taf::pa::data::Utils::ConvertTechPref(iDataCall->getTechPreference());
    eventInfo.hostIfName = iDataCall->getInterfaceName();

    // Fill the common call end reason
    if (telux::data::DataCallStatus::NET_NO_NET == datacallStatus)
    {
        PA_DEBUG("Fill call end reason.");
        telux::common::DataCallEndReason callEndReason = iDataCall->getDataCallEndReason();
        fillCallEndReason(callEndReason, eventInfo.callEndReason);
    }

    // Fill in IPv4 call information.
    eventInfo.ipv4DataCallInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(
                                                                                  IPv4Info.status);
    if (telux::data::DataCallStatus::NET_CONNECTED == IPv4Info.status ||
        telux::data::DataCallStatus::NET_RECONFIGURED == IPv4Info.status ||
        telux::data::DataCallStatus::NET_NEWADDR == IPv4Info.status)
    {
        PA_DEBUG("Fill in IPv4 call info.");
        eventInfo.ipv4DataCallInfo.ipAddr = IPv4Info.addr.ifAddress;
        eventInfo.ipv4DataCallInfo.ipAddrMask = IPv4Info.addr.ifMask;
        eventInfo.ipv4DataCallInfo.gwAddr = IPv4Info.addr.gwAddress;
        eventInfo.ipv4DataCallInfo.gwAddrMask = IPv4Info.addr.gwMask;
        eventInfo.ipv4DataCallInfo.dnsAddrPrimary = IPv4Info.addr.primaryDnsAddress;
        eventInfo.ipv4DataCallInfo.dnsAddrSecondary = IPv4Info.addr.secondaryDnsAddress;
    }
    // Checks status and fill IPv4 call end reason
    if (telux::data::DataCallStatus::NET_NO_NET == IPv4Info.status)
    {
        PA_DEBUG("Fill IPv4 call end reason.");
        telux::common::DataCallEndReason callEndReason = iDataCall->getDataCallEndReason();
        fillCallEndReason(callEndReason, eventInfo.ipv4DataCallInfo.callEndReason);
    }

    // Fill in IPv6 call information.
    eventInfo.ipv6DataCallInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(
                                                                                  IPv6Info.status);
    if (telux::data::DataCallStatus::NET_CONNECTED == IPv6Info.status ||
        telux::data::DataCallStatus::NET_RECONFIGURED == IPv6Info.status ||
        telux::data::DataCallStatus::NET_NEWADDR == IPv6Info.status)
    {
        PA_DEBUG("Fill in IPv6 call info.");
        eventInfo.ipv6DataCallInfo.ipAddr = IPv6Info.addr.ifAddress;
        eventInfo.ipv6DataCallInfo.ipAddrMask = IPv6Info.addr.ifMask;
        eventInfo.ipv6DataCallInfo.gwAddr = IPv6Info.addr.gwAddress;
        eventInfo.ipv6DataCallInfo.gwAddrMask = IPv6Info.addr.gwMask;
        eventInfo.ipv6DataCallInfo.dnsAddrPrimary = IPv6Info.addr.primaryDnsAddress;
        eventInfo.ipv6DataCallInfo.dnsAddrSecondary = IPv6Info.addr.secondaryDnsAddress;
    }
    // Checks status and fill IPv6 call end reason
    if (telux::data::DataCallStatus::NET_NO_NET == IPv6Info.status)
    {
        PA_DEBUG("Fill IPv6 call end reason.");
        telux::common::DataCallEndReason callEndReason = iDataCall->getDataCallEndReason();
        fillCallEndReason(callEndReason, eventInfo.ipv6DataCallInfo.callEndReason);
    }

    // If this is a connected event, update the max Tx and Rx bit rates.
    eventInfo.maxRxBitRate = 0;
    eventInfo.maxTxBitRate = 0;
    if (telux::data::DataCallStatus::NET_CONNECTED == datacallStatus ||
        telux::data::DataCallStatus::NET_CONNECTED == IPv4Info.status ||
        telux::data::DataCallStatus::NET_CONNECTED == IPv6Info.status)
    {
        PA_DEBUG("Get max data bit rate");
        telux::data::BitRateInfo bitRate;
        if (PA_OK == updateBitRate(iDataCall, bitRate))
        {
            PA_DEBUG("maxRxRate: %" PRIu64 "", bitRate.maxRxRate);
            PA_DEBUG("maxTxRate: %" PRIu64 "", bitRate.maxTxRate);
            eventInfo.maxRxBitRate = bitRate.maxRxRate;
            eventInfo.maxTxBitRate = bitRate.maxTxRate;
        }
    }

    // Fill the data bearer technology and update the IDataCall call status map.
    if (telux::data::DataCallStatus::NET_NO_NET != datacallStatus)
    {
        // TODO: getCurrentBearerTech() is deprecated. Update the Telux API.
        eventInfo.bearerTech = taf::pa::data::Utils::ConvertBearerTech(
                                                                iDataCall->getCurrentBearerTech());
        PA_DEBUG("Bearer tech obtained");

        // For debugging
        void *rawPtr = static_cast<void *>(iDataCall.get());
        // Lock and updated call status in map
        std::lock_guard<std::mutex> lock(listenerMtx_);
        callStatusMap_[iDataCall] = datacallStatus;
        PA_DEBUG("iDataCall(%p) updated in callStatusMap_. Size: %zu", rawPtr,
                                                                            callStatusMap_.size());
    }
    else
    {
        // For debugging
        void *rawPtr = static_cast<void *>(iDataCall.get());
        // NET_NO_NET. iDataCall is not valid anymore. Lock and remove from map
        std::lock_guard<std::mutex> lock(listenerMtx_);
        callStatusMap_.erase(iDataCall);
        PA_DEBUG("iDataCall(%p) removed from callStatusMap_. Size: %zu", rawPtr,
                                                                            callStatusMap_.size());
    }
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onDataCallInfoChanged
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall
)
{
    SET_SDK_THREAD_NAME();
    PA_INFO("Slot ID: %d", TO_INT(slotId_));

    taf::pa::data::TafPaTeluxDataConnection::LogDataCallInfo(iDataCall, __func__);
    DataCallEventInfo_t eventInfo;
    // Parse IDataCall into call data info structure.
    // Note: ParseIDataCall() handles mutex locking internally for callStatusMap_ access
    ParseIDataCall(iDataCall, eventInfo);
    // Send the data call event info to registered clients
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onThrottledApnInfoChanged
(
    const std::vector<telux::data::APNThrottleInfo>  &throttleInfoList
)
{
    SET_SDK_THREAD_NAME();
    std::vector<ThrottledApnEventInfo_t> throttledApnEventInfoList;
    PA_INFO("Slot ID: %d", TO_INT(slotId_));
    taf::pa::data::SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(slotId_);
    taf::pa::data::PhoneId_e phoneId;
    pa_result_t result = taf::pa::data::GetPhoneIdFromSimSlotId(slotIdPa, phoneId);
    if (PA_OK != result)
    {
        PA_ERROR("Failed to get phone ID for slot ID %d", TO_INT(slotIdPa));
        return;
    }

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

    // Send the throttled APN event info to registered clients
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.PaSendThrottledApnEventInfoToClients(throttledApnEventInfoList);
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onTrafficFlowTemplateChange
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall,
    const std::vector<std::shared_ptr<telux::data::TftChangeInfo>> &tfts)
{
    SET_SDK_THREAD_NAME();
    PA_INFO("Slot ID: %d", TO_INT(slotId_));

    PA_DEBUG("Profile Id: %d, Slot ID: %d", iDataCall->getProfileId(), iDataCall->getSlotId());
    size_t numTfts = tfts.size();
    PA_DEBUG("Number of TFT(s): %zu", numTfts);

    SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());
    PhoneId_e phoneId;
    pa_result_t result = taf::pa::data::GetPhoneIdFromSimSlotId(slotIdPa, phoneId);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "Unable to get phone ID");

    QosTftEventInfo_t qosTftEventInfo;

    if (numTfts > 0)
    {
        qosTftEventInfo.phoneId = phoneId;
        qosTftEventInfo.profileId = static_cast<ProfileId_e>(iDataCall->getProfileId());
        for (auto &tft : tfts)
        {
            QosTft_t paTft;
            PA_DEBUG("QOS Event: %d ", static_cast<int>(tft->stateChange));
            PA_DEBUG("TFT Details:");
            PA_DEBUG("  QOS Flow ID   : %d ", tft->tft->qosId);
            PA_DEBUG("  QOS Flow State: %d ", static_cast<int>(tft->tft->stateChange));
            PA_DEBUG("  QOS Flow Mask : %lu", tft->tft->mask.to_ulong());
            paTft.qosFlowId = tft->tft->qosId;
            paTft.state     = taf::pa::data::Utils::ConvertQosFlowState(tft->stateChange);
            paTft.paramMask = std::bitset<32>(tft->tft->mask.to_ulong());
            // Add to flow vector
            qosTftEventInfo.qosFlows.push_back(paTft);
        }
    }

    // Send event to registered clients.
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.PaSendQosTftEventInfoToClients(qosTftEventInfo);
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onHwAccelerationChanged
(
    const telux::data::ServiceState state
)
{
    SET_SDK_THREAD_NAME();
    PA_INFO("Slot ID: %d", TO_INT(slotId_));
    PA_DEBUG("HW acceleration state: %d", static_cast<int>(state));

    PhoneId_e phoneId;
    SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(slotId_);
    pa_result_t result = taf::pa::data::GetPhoneIdFromSimSlotId(slotIdPa, phoneId);
    TAF_PA_ERROR_IF_RET_NIL(PA_OK != result, "Unable to get phone ID");

    HwAccelerationChangeEvent_t event;
    event.phoneId = phoneId;
    event.state   = taf::pa::data::Utils::ConvertHwAccelerationState(state);

    // Send event to registered clients.
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.PaSendHwAccelerationEventInfoToClients(event);
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onServiceStatusChange
(
    telux::common::ServiceStatus status
)
{
    SET_SDK_THREAD_NAME();
    SubsystemState_e sState;
    switch (status)
    {
    case telux::common::ServiceStatus::SERVICE_AVAILABLE:
        PA_INFO("Data connection Manager for Slot ID %d: AVAILABLE", slotId_);
        sState = SubsystemState_e::AVAILABLE;
        break;
    case telux::common::ServiceStatus::SERVICE_UNAVAILABLE:
        PA_ERROR("Data connection Manager for Slot ID %d: UNAVAILABLE", slotId_);
        sState = SubsystemState_e::UNAVAILABLE;
        break;
    case telux::common::ServiceStatus::SERVICE_FAILED:
        PA_ERROR("Data connection Manager for Slot ID %d: FAILED", slotId_);
        sState = SubsystemState_e::FAILED;
        break;
    default:
        PA_WARN("Data connection Manager for Slot ID %d: status unknown", slotId_);
        sState = SubsystemState_e::FAILED;
        break;
    };

    SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(slotId_);
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.SetSubsysState(slotIdPa, sState, true);
}
