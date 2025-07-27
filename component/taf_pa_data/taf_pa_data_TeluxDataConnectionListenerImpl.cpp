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
#include "tafSvcIF.hpp"

void taf::pa::data::TafPaTeluxDataConnectionListener::fillCallEndReason
(
    const telux::common::DataCallEndReason &teluxReason,
     DataCallEndReason_t &paReason
)
{
    using namespace taf::pa::data;
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
        LE_WARN("Invalid Reason type: %d", static_cast<int32_t>(teluxReason.type));
        break;
    }
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onDataCallInfoChanged
(
    const std::shared_ptr<telux::data::IDataCall> &iDataCall
)
{
    SET_SDK_THREAD_NAME();
    LE_INFO("Slot ID: %d", TO_INT(slotId_));

    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.LogDataCallInfo(iDataCall, __func__);

    auto &teluxPaData = TafPaTeluxData::GetInstance();
    DataCallEventInfo_t eventInfo;
    taf::pa::data::PhoneId_e phoneId;

    telux::data::DataCallStatus datacallStatus = iDataCall->getDataCallStatus();

    eventInfo.slotId = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());

    teluxPaData.PaGetPhoneIdFromSimSlotId(eventInfo.slotId, phoneId);
    telux::data::IpFamilyInfo IPv4Info = iDataCall->getIpv4Info();
    telux::data::IpFamilyInfo IPv6Info = iDataCall->getIpv6Info();

    // Fill DataCallEventInfo_t
    eventInfo.phoneId    = phoneId;
    eventInfo.profileId  = static_cast<taf::pa::data::ProfileId_e>(iDataCall->getProfileId());
    eventInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(datacallStatus);
    eventInfo.ipType     = taf::pa::data::Utils::ConvertIpType(iDataCall->getIpFamilyType());
    eventInfo.techPref   = taf::pa::data::Utils::ConvertTechPref(iDataCall->getTechPreference());
    eventInfo.hostIfName = iDataCall->getInterfaceName();

    // Fill the common call end reason
    if (telux::data::DataCallStatus::NET_NO_NET == datacallStatus)
    {
        LE_DEBUG("Fill call end reason.");
        telux::common::DataCallEndReason callEndReason = iDataCall->getDataCallEndReason();
        fillCallEndReason(callEndReason, eventInfo.callEndReason);
    }

    // Fill in IPv4 call information.
    eventInfo.ipv4DataCallInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(IPv4Info.status);
    if (telux::data::DataCallStatus::NET_CONNECTED    == IPv4Info.status ||
        telux::data::DataCallStatus::NET_RECONFIGURED == IPv4Info.status ||
        telux::data::DataCallStatus::NET_NEWADDR      == IPv4Info.status
        )
    {
        LE_DEBUG("Fill in IPv4 call info.");
        eventInfo.ipv4DataCallInfo.ipAddr           = IPv4Info.addr.ifAddress;
        eventInfo.ipv4DataCallInfo.ipAddrMask       = IPv4Info.addr.ifMask;
        eventInfo.ipv4DataCallInfo.gwAddr           = IPv4Info.addr.gwAddress;
        eventInfo.ipv4DataCallInfo.gwAddrMask       = IPv4Info.addr.gwMask;
        eventInfo.ipv4DataCallInfo.dnsAddrPrimary   = IPv4Info.addr.primaryDnsAddress;
        eventInfo.ipv4DataCallInfo.dnsAddrSecondary = IPv4Info.addr.secondaryDnsAddress;
    }
    // Checks status and fill IPv4 call end reason
    if (telux::data::DataCallStatus::NET_NO_NET == IPv4Info.status)
    {
        LE_DEBUG("Fill IPv4 call end reason.");
        telux::common::DataCallEndReason callEndReason = iDataCall->getDataCallEndReason();
        fillCallEndReason(callEndReason, eventInfo.ipv4DataCallInfo.callEndReason);
    }

    // Fill in IPv6 call information.
    eventInfo.ipv6DataCallInfo.callStatus = taf::pa::data::Utils::ConvertCallStatus(IPv6Info.status);
    if (telux::data::DataCallStatus::NET_CONNECTED == IPv6Info.status ||
        telux::data::DataCallStatus::NET_RECONFIGURED == IPv6Info.status ||
        telux::data::DataCallStatus::NET_NEWADDR == IPv6Info.status)
    {
        LE_DEBUG("Fill in IPv6 call info.");
        eventInfo.ipv6DataCallInfo.ipAddr           = IPv6Info.addr.ifAddress;
        eventInfo.ipv6DataCallInfo.ipAddrMask       = IPv6Info.addr.ifMask;
        eventInfo.ipv6DataCallInfo.gwAddr           = IPv6Info.addr.gwAddress;
        eventInfo.ipv6DataCallInfo.gwAddrMask       = IPv6Info.addr.gwMask;
        eventInfo.ipv6DataCallInfo.dnsAddrPrimary   = IPv6Info.addr.primaryDnsAddress;
        eventInfo.ipv6DataCallInfo.dnsAddrSecondary = IPv6Info.addr.secondaryDnsAddress;
    }
    // Checks status and fill IPv6 call end reason
    if (telux::data::DataCallStatus::NET_NO_NET == IPv6Info.status)
    {
        LE_DEBUG("Fill IPv6 call end reason.");
        telux::common::DataCallEndReason callEndReason = iDataCall->getDataCallEndReason();
        fillCallEndReason(callEndReason, eventInfo.ipv6DataCallInfo.callEndReason);
    }

    // If this is a connected event, update the max Tx and Rx bit rates.
    eventInfo.maxRxBitRate = 0;
    eventInfo.maxTxBitRate = 0;
    if (telux::data::DataCallStatus::NET_CONNECTED == datacallStatus  ||
        telux::data::DataCallStatus::NET_CONNECTED == IPv4Info.status ||
        telux::data::DataCallStatus::NET_CONNECTED == IPv6Info.status)
    {
        LE_DEBUG("Get max data bit rate");
        // Promise and future used for synchronization.
        std::promise<bool> prom;
        std::future<bool> fut = prom.get_future();

        // requestDataCallBitRate callback lambda
        auto respCb = [&eventInfo, &prom](telux::data::BitRateInfo &bitRate,
                                       telux::common::ErrorCode errorCode)
        {
            if (telux::common::ErrorCode::SUCCESS == errorCode)
            {
                // Success
                LE_DEBUG("maxRxRate: %" PRIu64 "", bitRate.maxRxRate);
                LE_DEBUG("maxTxRate: %" PRIu64 "", bitRate.maxTxRate);
                eventInfo.maxRxBitRate = bitRate.maxRxRate;
                eventInfo.maxTxBitRate = bitRate.maxTxRate;
                prom.set_value(true);
            }
            else
            {
                LE_WARN("requestDataCallBitRateCb error: %d", static_cast<int>(errorCode));
                prom.set_value(true);
            }
        };
        telux::common::Status status = iDataCall->requestDataCallBitRate(respCb);
        if (telux::common::Status::SUCCESS == status)
        {
            LE_DEBUG("requestDataCallBitRate SUCCESS. Wait for cbk");
            // Just to use our macro
            bool bFutResult;
            FUTURE_GET_RET_NIL(fut, bFutResult);
            LE_UNUSED(bFutResult);
        }
        else
        {
            LE_WARN("requestDataCallBitRate failed: %d", static_cast<int>(status));
        }
    }

    // Fill the data bearer technology.
    if (telux::data::DataCallStatus::NET_NO_NET != datacallStatus)
    {
        // TODO: getCurrentBearerTech() is deprecated. Update the Telux API.
        eventInfo.bearerTech = taf::pa::data::Utils::ConvertBearerTech(
                                                                iDataCall->getCurrentBearerTech());
    }

    // Send the data call event info to registered clients
    teluxPaDataConn.PaSendDataCallEventInfoToClients(eventInfo);
}

void taf::pa::data::TafPaTeluxDataConnectionListener::onThrottledApnInfoChanged
(
    const std::vector<telux::data::APNThrottleInfo>  &throttleInfoList
)
{
    SET_SDK_THREAD_NAME();
    std::vector<ThrottledApnEventInfo_t> throttledApnEventInfoList;
    LE_INFO("Slot ID: %d", TO_INT(slotId_));
    taf::pa::data::SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(slotId_);
    taf::pa::data::PhoneId_e phoneId;
    le_result_t result = taf::pa::data::GetPhoneIdFromSimSlotId(slotIdPa, phoneId);
    if (LE_OK != result)
    {
        LE_ERROR("Failed to get phone ID for slot ID %d", TO_INT(slotIdPa));
        return;
    }

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
    LE_INFO("Slot ID: %d", TO_INT(slotId_));

    LE_DEBUG("Profile Id: %d, Slot ID: %d", iDataCall->getProfileId(), iDataCall->getSlotId());
    size_t numTfts = tfts.size();
    LE_DEBUG("Number of TFT(s): %zu", numTfts);

    SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(iDataCall->getSlotId());
    PhoneId_e phoneId;
    le_result_t result = taf::pa::data::GetPhoneIdFromSimSlotId(slotIdPa, phoneId);
    TAF_ERROR_IF_RET_NIL(LE_OK != result, "Unable to get phone ID");

    QosTftEventInfo_t qosTftEventInfo;

    if (numTfts > 0)
    {
        qosTftEventInfo.phoneId = phoneId;
        qosTftEventInfo.profileId = static_cast<ProfileId_e>(iDataCall->getProfileId());
        for (auto &tft : tfts)
        {
            QosTft_t paTft;
            LE_DEBUG("QOS Event: %d ", static_cast<int>(tft->stateChange));
            LE_DEBUG("TFT Details:");
            LE_DEBUG("  QOS Flow ID   : %d ", tft->tft->qosId);
            LE_DEBUG("  QOS Flow State: %d ", static_cast<int>(tft->tft->stateChange));
            LE_DEBUG("  QOS Flow Mask : %lu", tft->tft->mask.to_ulong());
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
    LE_INFO("Slot ID: %d", TO_INT(slotId_));
    LE_DEBUG("HW acceleration state: %d", static_cast<int>(state));

    PhoneId_e phoneId;
    SlotId_e slotIdPa = taf::pa::data::Utils::ConvertSlotId(slotId_);
    le_result_t result = taf::pa::data::GetPhoneIdFromSimSlotId(slotIdPa, phoneId);
    TAF_ERROR_IF_RET_NIL(LE_OK != result, "Unable to get phone ID");

    HwAccelerationChangeEvent_t event;
    event.phoneId = phoneId;
    event.state   = taf::pa::data::Utils::ConvertHwAccelerationState(state);

    // Send event to registered clients.
    auto &teluxPaDataConn = taf::pa::data::TafPaTeluxDataConnection::GetInstance();
    teluxPaDataConn.PaSendHwAccelerationEventInfoToClients(event);
}
