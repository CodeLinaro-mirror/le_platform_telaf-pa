/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfile.hpp
 * @brief Telux Data profile management.
 *
 */

#ifndef __TAF_DATA_TELUX_DATA_PROFILE_PA_HPP__
#define __TAF_DATA_TELUX_DATA_PROFILE_PA_HPP__

#include "tafDataPa.hpp"

#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include "telux/common/CommonDefines.hpp"
#include "telux/common/Utils.hpp"
#include "tafDataTeluxDataPa.hpp"
#include "tafDataUtilsPa.hpp"

#include <map>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <future>
#include <shared_mutex>

namespace taf
{
namespace pa
{
namespace data
{
    // Profile events entry
    struct ProfileEventsCallbackEntry_t
    {
        uint16_t id;
        taf_pa_data_ProfileEventsCb callBack;
        std::shared_ptr<void> context;
    };

    // Class to handle data profile related events
    class TafPaTeluxDataProfileListener : public telux::data::IDataProfileListener
    {
    public:
        // Constructor
        TafPaTeluxDataProfileListener(SlotId slotId) : slotId_(slotId) {};
        ~TafPaTeluxDataProfileListener(){};
        void onServiceStatusChange(telux::common::ServiceStatus status) override;
        void onProfileUpdate
        (
            int profileId,
            telux::data::TechPreference techPreference,
            telux::data::ProfileChangeEvent event
        ) override;

    private:
        const SlotId slotId_;
    };

    // Class to handle TelSDK profile list callback.
    class TafPaTeluxDataProfileListCallback : public telux::data::IDataProfileListCallback
    {
    public:
        // Constructor
        TafPaTeluxDataProfileListCallback(SlotId slotId) : slotId_(slotId) {};
        ~TafPaTeluxDataProfileListCallback() {};
        void onProfileListResponse(
            const std::vector<std::shared_ptr<telux::data::DataProfile>> &profiles,
            telux::common::ErrorCode error) override;
        void SetListProfilesCallback(taf_pa_data_profile_GetAllAsyncCb callback);
        void SetListProfilesContext(void *ctxPtr);
        // Set both callback and context atomically under profileListMutex_
        void SetListProfilesCallbackAndContext(taf_pa_data_profile_GetAllAsyncCb callback,
                                              void *ctxPtr);

        // State management functions
        void SetListProfilesCmdInProgress(bool bState);
        // Returns true if a profile list command is in progress
        bool GetListProfilesCmdInProgress();
        // atomically transition bListProfilesCmdInProgress_ from false→true.
        // Returns true if the caller successfully acquired the "in-progress" token
        // (i.e. the flag was false and is now set to true).
        // Returns false if the flag was already true (another request is in progress).
        bool TryAcquireListProfilesCmd();

    private:

        SlotId slotId_;

        // Mutex declared static to have one mutex across all objects.
        inline static std::mutex profileListMutex_;

        // Variables for PaListProfiles command
        taf_pa_data_profile_GetAllAsyncCb callbackListProfiles_ = nullptr;
        void *contextListProfiles_ = nullptr;
        std::atomic<bool> bListProfilesCmdInProgress_ = {false};
    };

    // Class to handle requestProfile
    class TafPaTeluxDataRequestProfile : public telux::data::IDataProfileCallback
    {
    public:
        // Constructor
        TafPaTeluxDataRequestProfile(
            SlotId slotId,
            std::shared_ptr<std::promise<std::tuple<telux::common::ErrorCode,
                                           std::shared_ptr<telux::data::DataProfile>>>> promisePtr
        ) : slotId_(slotId), promisePtr_(promisePtr) {};
        ~TafPaTeluxDataRequestProfile() {};
        void onResponse (
            const std::shared_ptr<telux::data::DataProfile> &profile,
            telux::common::ErrorCode error
        ) override;

    private:
        const SlotId slotId_;
        std::shared_ptr<std::promise<std::tuple<telux::common::ErrorCode,
                                        std::shared_ptr<telux::data::DataProfile>>>> promisePtr_;
    };

    // Create profile callback
    class TafPaTeluxDataProfileCreateCallback : public telux::data::IDataCreateProfileCallback
    {
    public:
        TafPaTeluxDataProfileCreateCallback
        (
            std::shared_ptr<std::promise<std::tuple<telux::common::ErrorCode, int>>> p
        ) : promise_ptr_(p){}
        void onResponse(int profileId, telux::common::ErrorCode error) override;

    private:
        std::shared_ptr<std::promise<std::tuple<telux::common::ErrorCode, int>>> promise_ptr_;
    };

    // Modify profile callback
    class TafPaTeluxDataProfileModifyCallback : public telux::common::ICommandResponseCallback
    {
        public:
            TafPaTeluxDataProfileModifyCallback (
                std::shared_ptr<std::promise<telux::common::ErrorCode>> p
            ) : promise_ptr_(p) {}
            void commandResponse(telux::common::ErrorCode error) override;

        private:
            std::shared_ptr<std::promise<telux::common::ErrorCode>> promise_ptr_;
    };

    // Delete profile callback
    class TafPaTeluxDataProfileDeleteCallback : public telux::common::ICommandResponseCallback
    {
    public:
        TafPaTeluxDataProfileDeleteCallback
        (
            std::shared_ptr<std::promise<telux::common::ErrorCode>> p
        ) : promise_ptr_(p) {}
        void commandResponse(telux::common::ErrorCode error) override;

    private:
        std::shared_ptr<std::promise<telux::common::ErrorCode>> promise_ptr_;
    };

    class TafPaTeluxDataProfile
    {
    public:
        TafPaTeluxDataProfile(const TafPaTeluxDataProfile &) = delete;
        TafPaTeluxDataProfile &operator=(const TafPaTeluxDataProfile &) = delete;
        static TafPaTeluxDataProfile &GetInstance();

        void Init(taf::pa::data::SlotCount_e slotCount);
        void Deinit();
        pa_result_t PaGetSubsysState(taf::pa::data::SlotId_e slotId, SubsystemState_e &sState);
        pa_result_t SetSubsysState
        (
            taf::pa::data::SlotId_e slotId,
            SubsystemState_e sState,
            bool bSendEvent=false
        );
        pa_result_t PaRegisterProfileCallbacks();
        pa_result_t PaDeregisterProfileCallbacks();
        pa_result_t PaListProfiles( taf::pa::data::PhoneId_e phoneId,
                                    taf_pa_data_profile_GetAllAsyncCb callback,
                                    void *contextPtr);
        pa_result_t PaGetProfileInfo(PhoneId_e phoneId, ProfileInfo_t &profileInfo);
        pa_result_t PaCreateProfile(
            PhoneId_e phoneId,
            const ProfileInfo_t &profileInfo,
            ProfileId_e &profileId);
        pa_result_t PaUpdateProfile(PhoneId_e phoneId, const ProfileInfo_t &profileInfo);
        pa_result_t PaDeleteProfile(PhoneId_e phoneId, const ProfileInfo_t &profileInfo);
        pa_result_t PaAddProfileEventsCallback
        (
            taf_pa_data_ProfileEventsCb callBack,
            std::shared_ptr<void> context,
            uint16_t &id
        );
        pa_result_t PaRemoveProfileEventsCallback(uint16_t id);
        // Handle profile events from onProfileUpdate
        void PaUpdateProfileEventInfo
        (
            SlotId slotId,
            int profileId,
            telux::data::ProfileChangeEvent event,
            telux::data::TechPreference techPreference
        );
        void PaSendProfileEventInfoToClients
        (
            PhoneId_e               phoneId,
            ProfileEvent_e          event,
            const ProfileInfo_t    &profileInfo
        );

    private:
        // Variables
        taf::pa::data::SlotCount_e slotCount_ = taf::pa::data::SlotCount_e::ONE; // 1

        std::map<SlotId_e, SubsystemState_e> dataProfileManagersInitStateMap_ = {
            {SlotId_e::SLOT_1, SubsystemState_e::FAILED},
            {SlotId_e::SLOT_2, SubsystemState_e::FAILED}
        };
        std::map<SlotId, std::shared_ptr<telux::data::IDataProfileManager>> dataProfileManagersMap_;
        std::map<SlotId, std::shared_ptr<taf::pa::data::TafPaTeluxDataProfileListCallback>>
                                                                    tafPaDataProfileListCbksMap_;
        std::map<SlotId, std::shared_ptr<TafPaTeluxDataProfileListener>>
                                                                    tafPaTeluxDataProfListenersMap_;

        // Track the registration of data profile listeners
        bool bProfileListenersRegistered_[MAX_SLOT_NUM] = {false,false};

        // Mutexes
        std::mutex dataProfileCreateMutex_;
        std::mutex dataProfileDeleteMutex_;
        std::mutex dataProfileUpdateMutex_;
        std::mutex dataProfileGetDetailsMutex_;

        // counter + CV used to drain all detached async threads launched
        // from PaUpdateProfileEventInfo() before deInitDataProfileManagers() clears
        // dataProfileManagersMap_.
        //
        // Protocol (all three fields are accessed only while holding asyncThreadDrainMtx_,
        // except for the atomic load/store which uses acquire/release ordering):
        //
        //  PaUpdateProfileEventInfo():
        //    1. Lock asyncThreadDrainMtx_.
        //    2. If deinitInProgress_ is true, drop the event and return (Deinit has started).
        //    3. Increment activeAsyncThreadCount_.
        //    4. Unlock.
        //    5. Launch (and detach) the worker thread.
        //    In the worker thread, at exit:
        //    6. Lock asyncThreadDrainMtx_.
        //    7. Decrement activeAsyncThreadCount_.
        //    8. notify_all on asyncThreadDrainCv_.
        //    9. Unlock.
        //
        //  deInitDataProfileManagers() (after PaDeregisterProfileCallbacks() returns):
        //    1. Lock asyncThreadDrainMtx_.
        //    2. Set deinitInProgress_ = true  (blocks any new thread launches).
        //    3. wait_for on asyncThreadDrainCv_ until activeAsyncThreadCount_ == 0.
        //    4. Unlock.
        //    5. Clear dataProfileManagersMap_ (safe: no thread is accessing it).
        //
        //  initDataProfileManagers() resets deinitInProgress_ = false so that a
        //  subsequent Init() call re-enables thread launches.
        std::atomic<int>        activeAsyncThreadCount_{0};
        std::mutex              asyncThreadDrainMtx_;
        std::condition_variable asyncThreadDrainCv_;
        bool                    deinitInProgress_{false};

        // Mutex for protecting dataProfileManagersInitStateMap_ (NB reads, SB writes).
        std::shared_mutex dataProfileSubsysStateMapMtx_;

        // Callbacks
        std::shared_mutex dataProfileCallbacksMutex_;
        // The callback for profile events
        std::vector<ProfileEventsCallbackEntry_t> profileEventsCallbacks_;
        uint16_t profileEventsCallbackId_ = 1;

        //Functions
        void initDataProfileManagers();
        pa_result_t deInitDataProfileManagers();
        pa_result_t getProfileDetails
        (
            SlotId slotId,
            int profileId,
            telux::data::TechPreference techPreference,
            ProfileInfo_t &profileInfo
        );

        TafPaTeluxDataProfile() {};
    };

} //data
} //pa
} //taf

#endif //__TAF_DATA_TELUX_DATA_PROFILE_PA_HPP__
