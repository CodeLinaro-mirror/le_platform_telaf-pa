/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataConnection.hpp
 * @brief Telux Data connection management.
 *
 */

#ifndef __TAF_PA_DATA_TELUX_DATA_CONNECTION_HPP__
#define __TAF_PA_DATA_TELUX_DATA_CONNECTION_HPP__

#include "legato.h"

#include "taf_pa_data.hpp"

#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include "telux/common/CommonDefines.hpp"
#include "telux/common/Utils.hpp"

#include <map>
#include <mutex>
#include <future>

namespace taf
{
namespace pa
{
namespace data
{

    struct DataCallEventsCallbackEntry_t
    {
        uint16_t id;
        taf_pa_data_CallEventsCb callBack;
        std::shared_ptr<void> context;
    };

    struct ThrottledApnEventsCallbackEntry_t
    {
        uint16_t id;
        taf_pa_data_ThrottledApnEventsCb callBack;
        std::shared_ptr<void> context;
    };

    // QoS traffic flow template event entry
    struct QosTftEventsCallbackEntry_t
    {
        uint16_t id;
        taf_pa_data_QosTftEventsCb callBack;
        std::shared_ptr<void> context;
    };

    // HW acceleration state change event entry
    struct HwAccelerationEventsCallbackEntry_t
    {
        uint16_t id;
        taf_pa_data_HwAccelerationEventsCb callBack;
        std::shared_ptr<void> context;
    };

    constexpr uint8_t MAX_SLOT_NUM = static_cast<uint8_t>(SlotCount_e::TWO);

    // Class to handle TelSDK callbacks.
    class TafPaTeluxDataConnectionListener : public telux::data::IDataConnectionListener
    {
    public:
        // Constructor
        TafPaTeluxDataConnectionListener(SlotId slotId) : slotId_(slotId) {};

        void onDataCallInfoChanged(const std::shared_ptr<telux::data::IDataCall> &iCall) override;
        void onThrottledApnInfoChanged(
            const std::vector<telux::data::APNThrottleInfo> &throttleInfoList) override;
        void onTrafficFlowTemplateChange(
            const std::shared_ptr<telux::data::IDataCall> &dataCall,
            const std::vector<std::shared_ptr<telux::data::TftChangeInfo>> &tft) override;
        void onHwAccelerationChanged(const telux::data::ServiceState state) override;

    private:
        SlotId slotId_;
        // Mutex declared static to have one mutex across all objects.
        static std::mutex mtx_;

        void fillCallEndReason(const telux::common::DataCallEndReason&, DataCallEndReason_t &);

        //Functions

    };

    class TafPaTeluxDataConnection
    {
    public:
        TafPaTeluxDataConnection(const TafPaTeluxDataConnection &) = delete;
        TafPaTeluxDataConnection &operator=(const TafPaTeluxDataConnection &) = delete;
        static TafPaTeluxDataConnection &GetInstance();

        void LogDataCallInfo
        (
            const std::shared_ptr<telux::data::IDataCall> &dataCall, const char *fromPtr
        );

        // External APIs
        void Init(SlotCount_e slotCount);
        void Deinit();
        le_result_t PaGetInitState(bool &initState);
        le_result_t PaRegisterDataConnCallbacks();
        le_result_t PaDeregisterDataConnCallbacks();
        le_result_t PaGetDefaultProfile(const PhoneId_e phoneId, ProfileId_e &profileId);
        le_result_t PaSetDefaultProfile(const PhoneId_e phoneId, const ProfileId_e profileId);

        le_result_t PaStartDataSessionAsync
        (
            const DataCallStartStopParams_t& params
        );
        le_result_t PaStopDataSessionAsync
        (
            const DataCallStartStopParams_t& params
        );
        le_result_t paGetThrottledApnInfo
        (
            const taf::pa::data::PhoneId_e        phoneId,
            std::vector<ThrottledApnEventInfo_t> &throttledApnEventInfoList
        );

        // Data call events
        void PaSendDataCallEventInfoToClients(const DataCallEventInfo_t &dataCallEventInfo);
        le_result_t PaAddDataCallEventsCallback
        (
            taf_pa_data_CallEventsCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        le_result_t PaRemoveDataCallEventsCallback
        (
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );

        // Throttled APN events
        void PaSendThrottledApnEventInfoToClients
        (
            const std::vector<ThrottledApnEventInfo_t> &throttledApnEventList
        );
        le_result_t PaAddThrottledApnEventsCallback
        (
            taf_pa_data_ThrottledApnEventsCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        le_result_t PaRemoveThrottledApnEventsCallback
        (
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );

        // QoS TFT events
        void PaSendQosTftEventInfoToClients
        (
            const QosTftEventInfo_t &qosTftEventsList
        );
        le_result_t PaAddQosTftEventsCallback
        (
            taf_pa_data_QosTftEventsCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        le_result_t PaRemoveQosTftEventsCallback
        (
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );

        // HW acceleration change events.
        void PaSendHwAccelerationEventInfoToClients
        (
            const HwAccelerationChangeEvent_t &hwAccelerationEventInfo
        );
        le_result_t PaAddHwAccelerationChangeEventsCallback
        (
            taf_pa_data_HwAccelerationEventsCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        le_result_t PaRemoveHwAccelerationChangeEventsCallback
        (
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );

    private:
        // Variables
        bool bDataConnectionMngrInitialized_ = false;
        SlotCount_e slotCount_ = SlotCount_e::ONE; // 1

        // The callback for data call events
        std::vector<DataCallEventsCallbackEntry_t> dataCallEventsCallbacks_;
        uint16_t dataCallEventsCallbackId_ = 1;

        // The callback for APN throttled events
        std::vector<ThrottledApnEventsCallbackEntry_t> throttledApnEventsCallbacks_;
        uint16_t throttledApnEventsCallbackId_ = 1;

        // The callback for QoS TFT events
        std::vector<QosTftEventsCallbackEntry_t> qosTftEventsCallbacks_;
        uint16_t qosTftEventsCallbackId_ = 1;

        // The callback for HW acceleration events
        std::vector<HwAccelerationEventsCallbackEntry_t> hwAccelerationEventsCallbacks_;
        uint16_t hwAccelerationEventsCallbackId_ = 1;

        std::map<SlotId, std::shared_ptr<telux::data::IDataConnectionManager>>
                                                                         dataConnectionManagersMap_;
        std::map<SlotId, std::shared_ptr<telux::data::IDataConnectionListener>>
                                                                        dataConnectionListenersMap_;
        std::map<SlotId, std::shared_ptr<TafPaTeluxDataConnectionListener>>
                                                              tafPaTeluxDataConnectionListenersMap_;

        // Used in initDataConnectionManagers()
        std::atomic<bool> bWaitingOnDataConnMngrProm_;
        // Used to track if connection listeners are registered or not.
        std::atomic<bool> bDataConnectionListenersRegistered_[MAX_SLOT_NUM] = {false};
        // Mutex for synchronizing registering and deregistering callbacks.
        std::mutex cbksMtx_;

        // Promise for PaGetDefaultProfile
        std::promise<std::tuple<int, SlotId, telux::common::ErrorCode>> getDefProfilePromise;
        // Mutex for PaGetDefaultProfile
        std::mutex getDefProfileMtx_;
        // Promise for setDefaultProfile
        std::promise<telux::common::ErrorCode> setDefProfilePromise;
        // Mutex for PaSetDefaultProfile
        std::mutex setDefProfileMtx_;

        // Functions
        // The callback fuction for TelSDK startDataCall()
        static void startDataCallCallback
        (
            const std::shared_ptr<telux::data::IDataCall> &iCall,
            telux::common::ErrorCode errorCode
        );
        // The callback fuction for TelSDK stoptDataCall()
        static void stopDataCallCallback
        (
            const std::shared_ptr<telux::data::IDataCall> &iCall,
            telux::common::ErrorCode errorCode
        );
        // The callback fuction for TelSDK requestThrottledApnInfo()
        static void requestThrottledApnInfoCallback
        (
            const std::vector<telux::data::APNThrottleInfo> &throttleInfoList,
            telux::common::ErrorCode error
        );
        // The promise for requestThrottledApnInfo()
        std::promise<std::pair<std::vector<
          telux::data::APNThrottleInfo>, telux::common::ErrorCode>> requestThrottledApnInfoPromise_;
        std::mutex requestThrottledApnInfoMtx_;

        void initDataConnectionManagers();
        void deInitDataConnectionManagers();
        TafPaTeluxDataConnection() {};
    };

} // data
} // pa
} // taf

#endif //__TAF_PA_DATA_TELUX_DATA_CONNECTION_HPP__