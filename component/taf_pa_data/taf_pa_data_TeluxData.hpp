/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


/**
 * @file taf_pa_data_TeluxData.hpp
 * @brief Telux Data subsystem management.
 *
 */

#ifndef __TAF_PA_DATA_TELUXDATA__
#define __TAF_PA_DATA_TELUXDATA__

#include "legato.h"

#include "taf_pa_data.hpp"

#include "telux/common/CommonDefines.hpp"
#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include <telux/tel/PhoneFactory.hpp>

#include <map>
#include <atomic>
#include <mutex>
#include <future>

namespace taf
{
namespace pa
{
namespace data
{

struct RoamingEventsCallbackEntry_t
{
    uint16_t id;
    taf_pa_data_RoamingEventsCb callBack;
    std::shared_ptr<void> context;
};

class tafPaTeluxDataServingSysListener : public telux::data::IServingSystemListener
{
public:
    tafPaTeluxDataServingSysListener(SlotId slot);
    ~tafPaTeluxDataServingSysListener();
    void onRoamingStatusChanged(telux::data::RoamingStatus status) override;

private:
    // Common variables
    SlotId slotId_;
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
        le_result_t PaGetInitState(bool &state);
        le_result_t PaGetSimSlotCount(taf::pa::data::SlotCount_e &count);
        le_result_t PaGetPhoneIds(std::vector<taf::pa::data::PhoneId_e> &phoneIds);
        le_result_t PaGetPhoneIdFromSimSlotId
        (
            taf::pa::data::SlotId_e slotID,
            taf::pa::data::PhoneId_e &phoneID
        );
        le_result_t PaGetSimSlotIdFromPhoneId
        (
            taf::pa::data::PhoneId_e phoneID,
            taf::pa::data::SlotId_e &slotID
        );
        le_result_t RegisterDataServingSystemListeners();
        le_result_t DeregisterDataServingSystemListeners();

        le_result_t PaGetRoamingStatus(const PhoneId_e phoneId, RoamingStatus_t &roamingStatus);

        le_result_t PaAddRoamingEventsCallback
        (
            taf_pa_data_RoamingEventsCb callBack,
            ///< [IN] The callback function.
            std::shared_ptr<void> context,
            ///< [IN] The context pointer.
            uint16_t &id
            ///< [OUT] The ID of the registered callback.
        );
        le_result_t PaRemoveRoamingEventsCallback
        (
            uint16_t id
            ///< [IN] The ID of the registered callback.
        );
        void SendRoamingEventInfoToClients(const taf::pa::data::RoamingStatus_t &eventInfo);

    private:
        void initInternalEvents();
        void checkAndUpdateSlotCount();

        // Data
        void initPhoneManager();
        void initDataServingSystemManagers();
        void deInitDataServingSystemManagers();

        bool bDataPhoneMngrInitialized_    = false;
        bool bDataServSysSMngrInitialized_ = false;
        bool bMultiSimSupported_           = false;
        taf::pa::data::SlotCount_e slotCount_ = taf::pa::data::SlotCount_e::ONE; // 1

        /**
         * Variable to track whether dataSSProm can be set or not.The lambda callback will be
         * called from a different thread.
         */
        std::atomic<bool> bWaitingOnDataSSProm_;

        // The phone IDs
        std::vector<int> phoneIds_;

        // Data SSL variables
        std::map<SlotId, bool> bDataSSLRegisteredMap_;

        // Mutex for synchronizing registering and deregistering callbacks.
        std::mutex roamingEventCbksMtx_;
        // Mutex for PaGetRoamingStatus
        std::mutex getRoamingStatusMtx_;

        // The callback for roaming events
        std::vector<RoamingEventsCallbackEntry_t> roamingEventsCallbacks_;
        uint16_t roamingEventsCallbackId_ = 1;
        // Promise for get roaming status
        std::promise<std::pair<
                    telux::data::RoamingStatus, telux::common::ErrorCode>> roamingStatusPromise_;
        std::atomic<bool> bGetRoamingStatusInProgress_;

        // Telux variables
        std::map<SlotId, std::shared_ptr<telux::data::IServingSystemManager>>
                                                                    dataServingSystemManagersMap_;
        std::map<SlotId, std::shared_ptr<telux::data::IServingSystemListener>>
                                                                    dataServingSystemListenersMap_;

        std::shared_ptr<telux::tel::IPhoneManager> phoneManager_;
        TafPaTeluxData();
};

} //data
} //pa
} //taf

#endif //__TAF_PA_DATA_TELUXDATA__
