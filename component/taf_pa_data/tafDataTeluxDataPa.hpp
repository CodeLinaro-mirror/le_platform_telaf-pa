/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


/**
 * @file taf_pa_data_TeluxData.hpp
 * @brief Telux Data subsystem management.
 *
 */

#ifndef __TAF_DATA_TELUX_DATA_PA_HPP__
#define __TAF_DATA_TELUX_DATA_PA_HPP__

#include "tafDataPa.hpp"

#include "telux/common/CommonDefines.hpp"
#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include "telux/data/ServingSystemManager.hpp"
#include <telux/tel/PhoneFactory.hpp>
#include "tafDataTeluxDataServingSysPa.hpp"
#include "tafInternalCommonPa.h"

#include <map>
#include <atomic>
#include <mutex>
#include <future>
#include <shared_mutex>

namespace taf
{
namespace pa
{
namespace data
{

/**
 * The maximum number of slots
 */
constexpr uint8_t MAX_SLOT_NUM = static_cast<uint8_t>(SlotCount_e::TWO);

struct SubsystemEventsCallbackEntry_t
{
    uint16_t id;
    taf_pa_data_SubsystemStateChangeCb callBack;
    std::shared_ptr<void> context;
};

struct SubsystemEvent_t
{
    PhoneId_e phoneId;
    Subsystem_e subsystem;
    SubsystemState_e subsystemState;
};

struct RoamingEventsCallbackEntry_t
{
    uint16_t id;
    taf_pa_data_RoamingEventsCb callBack;
    std::shared_ptr<void> context;
};

class TafPaTeluxData
{
    public:
        TafPaTeluxData(const TafPaTeluxData &) = delete;
        TafPaTeluxData &operator=(const TafPaTeluxData &) = delete;
        static TafPaTeluxData &GetInstance();

        void Init();
        void Deinit();

        // External APIs implementation
        SubsystemState_e PaGetPhoneManagerInitState();
        taf_pa_result_t PaGetServingSystemInitState
        (
            taf::pa::data::SlotId_e slotId,
            taf::pa::data::SubsystemState_e &sState
        );
        taf_pa_result_t PaGetSimSlotCount(taf::pa::data::SlotCount_e &count);
        taf_pa_result_t PaGetPhoneIds(std::vector<taf::pa::data::PhoneId_e> &phoneIds);
        taf_pa_result_t PaGetPhoneIdFromSlotId
        (
            const taf::pa::data::SlotId_e slotID,
            taf::pa::data::PhoneId_e &phoneID
        );
        taf_pa_result_t PaGetPhoneIdFromSlotId
        (
            const SlotId slotID,
            PhoneId_e &phoneID
        );
        taf_pa_result_t PaGetSlotIdFromPhoneId
        (
            const taf::pa::data::PhoneId_e phoneID,
            taf::pa::data::SlotId_e &slotID
        );

        taf_pa_result_t RegisterDataServingSystemListeners();
        taf_pa_result_t DeregisterDataServingSystemListeners();

        taf_pa_result_t PaAddSubsystemStateChangeCallback(
            taf_pa_data_SubsystemStateChangeCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        taf_pa_result_t PaRemoveSubsystemStateChangeCallback(
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );
        void SendSubsystemEventToClients(const SubsystemEvent_t &eventInfo);

        taf_pa_result_t PaGetRoamingStatus(const PhoneId_e phoneId, RoamingStatus_t &roamingStatus);

        taf_pa_result_t PaGetServiceStatus
        (
            const taf::pa::data::SlotId_e slotId,
            telux::data::ServiceStatus &serviceStatus
        );

        taf_pa_result_t PaAddRoamingEventsCallback
        (
            taf_pa_data_RoamingEventsCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        taf_pa_result_t PaRemoveRoamingEventsCallback
        (
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );

        taf_pa_result_t GetServinSystemInitState
        (
            taf::pa::data::SlotId_e slotId, SubsystemState_e &sState
        );
        taf_pa_result_t SetServingSystemInitState
        (
            taf::pa::data::SlotId_e slotId,
            SubsystemState_e sState,
            bool bSendEvent=false
        );
        void SendRoamingEventInfoToClients(const taf::pa::data::RoamingStatus_t &eventInfo);

    private:
        void checkAndUpdateSlotCount();

        // Data
        void initPhoneManager();
        void initDataServingSystemManagers();
        taf_pa_result_t deInitDataServingSystemManagers();

        taf::pa::data::SlotCount_e slotCount_ = taf::pa::data::SlotCount_e::ONE; // 1

        SubsystemState_e dataPhoneMngrInitState_   = SubsystemState_e::FAILED;
        bool bMultiSimSupported_              = false;

        /**
         * Variable to track whether dataSSProm can be set or not.The lambda callback will be
         * called from a different thread.
         */
        std::atomic<bool> bWaitingOnDataSSProm_;

        // The phone IDs
        std::vector<int> phoneIds_;

        // Data SSL variables
        std::map<SlotId, bool> bDataSSLRegisteredMap_;

        // Mutex for synchronizing subsystem state events.
        std::mutex subsystemEventsCbksMtx_;

        // Mutex for synchronizing registering, deregistering and calling client callbacks.
        std::mutex roamingEventsCbksMtx_;

        // Mutex for protecting servingSystemManagersInitStateMap_ (NB reads, SB writes).
        std::shared_mutex servingSystemStateMapMtx_;

        // The callback entry vector for subsystem events
        std::vector<SubsystemEventsCallbackEntry_t> subsystemEventsCallbacks_;
        uint16_t subsystemEventsCallbackId_ = 1;

        // The callback entry vector for roaming events
        std::vector<RoamingEventsCallbackEntry_t> roamingEventsCallbacks_;
        uint16_t roamingEventsCallbackId_ = 1;
        // Atomic flag to track if get roaming status is in progress
        std::atomic<bool> bGetRoamingStatusInProgress_;

        // Telux variables
        std::map<SlotId, std::shared_ptr<telux::data::IServingSystemManager>>
                                                                    dataServingSystemManagersMap_;
        std::map<SlotId, std::shared_ptr<taf::pa::data::TafPaTeluxDataServingSysListener>>
                                                                    dataServingSystemListenersMap_;
        std::map<SlotId_e, SubsystemState_e> servingSystemManagersInitStateMap_ = {
            {SlotId_e::SLOT_1, SubsystemState_e::FAILED},
            {SlotId_e::SLOT_2, SubsystemState_e::FAILED}
        };

        std::shared_ptr<telux::tel::IPhoneManager> phoneManager_;
        TafPaTeluxData();
};

} //data
} //pa
} //taf

#endif //__TAF_DATA_TELUX_DATA_PA_HPP__
