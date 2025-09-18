/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/**
 * @file taf_pa_data_TeluxDataProfile.hpp
 * @brief Telux Data profile management.
 *
 */

#ifndef __TAF_PA_DATA_TELUXDATAPROFILE_HPP__
#define __TAF_PA_DATA_TELUXDATAPROFILE_HPP__

#include "legato.h"

#include "taf_pa_data.hpp"

#include "telux/data/DataDefines.hpp"
#include "telux/data/DataFactory.hpp"
#include "telux/common/CommonDefines.hpp"
#include "telux/common/Utils.hpp"
#include "taf_pa_data_Utils.hpp"

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

    // Class to handle TelSDK callbacks.
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

        // State management functions
        void SetListProfilesCmdInProgress(bool bState);
        // Returns true if a profile list command is in progress
        bool GetListProfilesCmdInProgress();

    private:

        SlotId slotId_;

        // Mutex declared static to have one mutex across all objects.
        inline static std::mutex profileListMutex_;

        // Variables for PaListProfiles command
        taf_pa_data_profile_GetAllAsyncCb callbackListProfiles_ = nullptr;
        void *contextListProfiles_ = nullptr;
        std::atomic<bool> bListProfilesCmdInProgress_ = {false};
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
            TafPaTeluxDataProfileModifyCallback
            (
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
        le_result_t PaGetInitState(bool &initState);
        le_result_t PaListProfiles(taf::pa::data::PhoneId_e phoneId,
                                taf_pa_data_profile_GetAllAsyncCb callback,
                                void *contextPtr);
        le_result_t PaCreateProfile
        (
            PhoneId_e phoneId,
            const ProfileInfo_t &profileInfo,
            ProfileId_e &profileId
        );
        le_result_t PaUpdateProfile(PhoneId_e phoneId, const ProfileInfo_t &profileInfo);
        le_result_t PaDeleteProfile(PhoneId_e phoneId, const ProfileInfo_t &profileInfo);

    private:

        // Variables
        bool bDataProfileMngrInitialized_ = false;
        taf::pa::data::SlotCount_e slotCount_ = taf::pa::data::SlotCount_e::ONE; // 1

        std::map<SlotId, std::shared_ptr<telux::data::IDataProfileManager>> dataProfileManagersMap_;
        std::map<SlotId, std::shared_ptr<taf::pa::data::TafPaTeluxDataProfileListCallback>>
                                                                    tafPaDataProfileListCbksMap_;


        // Mutexes
        std::mutex dataProfileCreateMutex_;
        std::mutex dataProfileDeleteMutex_;
        std::mutex dataProfileUpdateMutex_;

        // Used in initDataProfileManagers() to track whether a promise can be set or not
        std::atomic<bool> bWaitingOnDataProfMngrProm_;

        //Functions
        void initDataProfileManagers();
        void deInitDataProfileManagers();

        TafPaTeluxDataProfile() {};
    };

} //data
} //pa
} //taf

#endif //__TAF_PA_DATA_TELUXDATAPROFILE_HPP__
