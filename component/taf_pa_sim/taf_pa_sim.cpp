/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <time.h>
#include <errno.h>
#include <semaphore.h>

#include <telux/common/DeviceConfig.hpp>
#include <telux/common/Utils.hpp>
#include <telux/tel/PhoneDefines.hpp>
#include <telux/tel/PhoneFactory.hpp>

#include "taf_pa_sim.hpp"
#include "taf_prop_sim.hpp"

using namespace std;
using namespace telux;

#define SERVICE_TIMEOUT 5
#define REQUEST_TIMEOUT 5

#define SERVICE_PROMISE_AND_CALLBACK(name)                                \
    auto name##Promise = make_shared<promise<common::ServiceStatus>>();   \
    auto name##Callback = [name##Promise](common::ServiceStatus status)   \
    {                                                                     \
        try                                                               \
        {                                                                 \
            name##Promise->set_value(status);                             \
        }                                                                 \
        catch (const future_error& e)                                     \
        {                                                                 \
            PA_ERROR("Future error in %s callback: %s", #name, e.what()); \
        }                                                                 \
        catch (const exception& e)                                        \
        {                                                                 \
            PA_ERROR("Exception in %s callback: %s", #name, e.what());    \
        }                                                                 \
        catch (...)                                                       \
        {                                                                 \
            PA_ERROR("Unknown error in %s callback.", #name);             \
        }                                                                 \
    };

#define SERVICE_READY(name)                                                      \
    future<common::ServiceStatus> name##Future = name##Promise->get_future();    \
    future_status name##Status = name##Future.wait_for(                          \
        chrono::seconds(SERVICE_TIMEOUT));                                       \
        common::ServiceStatus name##ServiceStatus;                               \
        if (future_status::timeout == name##Status)                              \
            PA_CRIT("Timeout for %s.", #name);                                   \
        else                                                                     \
        {                                                                        \
            name##ServiceStatus = name##Future.get();                            \
            if (name##ServiceStatus != common::ServiceStatus::SERVICE_AVAILABLE) \
                PA_CRIT("%s is not available.", #name);                          \
            else                                                                 \
                PA_INFO("%s is available.", #name);                              \
        }

typedef struct
{
    void* handlerFuncPtr;
    void* contextPtr;
} Handler_t;

typedef struct
{
    shared_ptr<tel::IPhoneManager> phone;
} Manager_t;
typedef struct
{
    Handler_t refreshSvcStatus;

} Indicator_t;

class PlatformAdaptor
{
    public:
        Manager_t managers;
        Indicator_t indicators;
        static PlatformAdaptor& GetInstance
        (
            void
        );
};
class Utility
{
    public:
        class Convert
        {
            public:
                static pa_result_t Result
                (
                    taf_prop_sim_Result_t result
                );
                static taf_prop_sim_SessionType_t SessionType
                (
                    taf_pa_sim_SessionType_t sessionType
                );
                static taf_pa_sim_SessionType_t SessionType
                (
                    taf_prop_sim_SessionType_t sessionType
                );
                static taf_pa_sim_RefreshMode_t RefreshMode
                (
                    taf_prop_sim_RefreshMode_t refreshMode
                );
                static taf_pa_sim_RefreshStage_t RefreshStage
                (
                    taf_prop_sim_RefreshStage_t refreshStage
                );
        };
};

static taf_pa_sim_ProfileType_t ConvertToPaProfileType(taf_prop_sim_ProfileType_t t)
{
    switch (t)
    {
        case TAF_PROP_SIM_PROFILE_TYPE_REGULAR:
            return TAF_PA_SIM_PROFILE_TYPE_REGULAR;
        case TAF_PROP_SIM_PROFILE_TYPE_EMERGENCY:
            return TAF_PA_SIM_PROFILE_TYPE_EMERGENCY;
        default:
            return TAF_PA_SIM_PROFILE_TYPE_UNKNOWN;
    }
}

static taf_pa_sim_SlotId_t IndexToSlot(uint8_t idx)
{
    switch (idx)
    {
        case 1: return TAF_PA_SIM_SLOT_1;
        case 2: return TAF_PA_SIM_SLOT_2;
        default: return TAF_PA_SIM_SLOT_UNKNOWN;
    }
}

static uint8_t SlotToIndex(taf_pa_sim_SlotId_t slot)
{
    switch (slot)
    {
        case TAF_PA_SIM_SLOT_1: return 1;
        case TAF_PA_SIM_SLOT_2: return 2;
        default:                return 1; // choose safe default
    }
}

static uint8_t ProfileEnumToId(taf_pa_sim_ProfileId_t pid)
{
    switch (pid)
    {
        case TAF_PA_SIM_PROFILE_ID_1: return 1;
        case TAF_PA_SIM_PROFILE_ID_2: return 2;
        default: return 0; // invalid
    }
}

static taf_pa_sim_ProfileId_t ProfileIdToEnum(uint8_t pid)
{
    switch (pid)
    {
        case 1: return TAF_PA_SIM_PROFILE_ID_1;
        case 2: return TAF_PA_SIM_PROFILE_ID_2;
        default: return TAF_PA_SIM_PROFILE_ID_UNKNOWN;
    }
}

static taf_pa_sim_ProfileInfo_t MakeInvalidProfileInfo()
{
    taf_pa_sim_ProfileInfo_t info;
    info.profileId = TAF_PA_SIM_PROFILE_ID_UNKNOWN;
    info.type      = TAF_PA_SIM_PROFILE_TYPE_UNKNOWN;
    info.state     = TAF_PA_SIM_PROFILE_STATE_UNKNOWN;
    return info;
}

PlatformAdaptor& PlatformAdaptor::GetInstance
(
    void
)
{
    static PlatformAdaptor instance;
    return instance;
}

pa_result_t Utility::Convert::Result
(
    taf_prop_sim_Result_t result
)
{
    switch (result)
    {
        case TAF_PROP_SIM_RESULT_OK:
            return TAF_PA_SIM_RESULT_OK;
        case TAF_PROP_SIM_RESULT_QMI_REQ_ERROR:
            return TAF_PA_SIM_RESULT_FAULT;
        case TAF_PROP_SIM_RESULT_INIT_ERROR:
            return TAF_PA_SIM_RESULT_TIMEOUT;
        default:
            PA_DEBUG("Unknown result %d.", result);
    }
    return TAF_PA_SIM_RESULT_FAULT;
}

taf_prop_sim_SessionType_t Utility::Convert::SessionType
(
    taf_pa_sim_SessionType_t sessionType
)
{
    switch (sessionType)
    {
        case TAF_PA_SIM_SESSION_TYPE_PRI_GW_PROV:
            return TAF_PROP_SIM_SESSION_TYPE_PRI_GW_PROV;
        case TAF_PA_SIM_SESSION_TYPE_SEC_GW_PROV:
            return TAF_PROP_SIM_SESSION_TYPE_SEC_GW_PROV;
        case TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_1:
            return TAF_PROP_SIM_SESSION_TYPE_CARD_ON_SLOT_1;
        case TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_2:
            return TAF_PROP_SIM_SESSION_TYPE_CARD_ON_SLOT_2;
        default:
            PA_INFO("Unknown SIM session type.");
            return TAF_PROP_SIM_SESSION_TYPE_UNKNOWN;
    }
}

taf_pa_sim_SessionType_t Utility::Convert::SessionType
(
    taf_prop_sim_SessionType_t SessionType
)
{
    switch (SessionType)
    {
        case TAF_PROP_SIM_SESSION_TYPE_PRI_GW_PROV:
            return TAF_PA_SIM_SESSION_TYPE_PRI_GW_PROV;
        case TAF_PROP_SIM_SESSION_TYPE_SEC_GW_PROV:
            return TAF_PA_SIM_SESSION_TYPE_SEC_GW_PROV;
        case TAF_PROP_SIM_SESSION_TYPE_CARD_ON_SLOT_1:
            return TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_1;
        case TAF_PROP_SIM_SESSION_TYPE_CARD_ON_SLOT_2:
            return TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_2;
        default:
            PA_ERROR("Unknown proprietary SIM session type %d.", SessionType);
            return TAF_PA_SIM_SESSION_TYPE_UNKNOWN;
    }
}

taf_pa_sim_RefreshMode_t Utility::Convert::RefreshMode
(
    taf_prop_sim_RefreshMode_t refreshMode
)
{
    switch (refreshMode)
    {
        case TAF_PROP_SIM_REFRESH_MODE_RESET:
            return TAF_PA_SIM_REFRESH_MODE_RESET;
        case TAF_PROP_SIM_REFRESH_MODE_INIT:
            return TAF_PA_SIM_REFRESH_MODE_INIT;
        case TAF_PROP_SIM_REFRESH_MODE_INIT_FCN:
            return TAF_PA_SIM_REFRESH_MODE_INIT_FCN;
        case TAF_PROP_SIM_REFRESH_MODE_FCN:
            return TAF_PA_SIM_REFRESH_MODE_FCN;
        case TAF_PROP_SIM_REFRESH_MODE_INIT_FULL_FCN:
            return TAF_PA_SIM_REFRESH_MODE_INIT_FULL_FCN;
        case TAF_PROP_SIM_REFRESH_MODE_APP_RESET:
            return TAF_PA_SIM_REFRESH_MODE_APP_RESET;
        case TAF_PROP_SIM_REFRESH_MODE_3G_RESET:
            return TAF_PA_SIM_REFRESH_MODE_3G_RESET;
        default:
            PA_INFO("Unknown SIM refresh mode.");
            return TAF_PA_SIM_REFRESH_MODE_UNKNOWN;
    }
}

taf_pa_sim_RefreshStage_t Utility::Convert::RefreshStage
(
    taf_prop_sim_RefreshStage_t refreshStage
)
{
    switch (refreshStage)
    {
        case TAF_PROP_SIM_REFRESH_STAGE_WAIT_FOR_OK:
            return TAF_PA_SIM_REFRESH_STAGE_WAIT_FOR_OK;
        case TAF_PROP_SIM_REFRESH_STAGE_START:
            return TAF_PA_SIM_REFRESH_STAGE_START;
        case TAF_PROP_SIM_REFRESH_STAGE_END_WITH_SUCCESS:
            return TAF_PA_SIM_REFRESH_STAGE_END_WITH_SUCCESS;
        case TAF_PROP_SIM_REFRESH_STAGE_END_WITH_FAILURE:
            return TAF_PA_SIM_REFRESH_STAGE_END_WITH_FAILURE;
        default:
            PA_INFO("Unknown SIM refresh stage.");
            return TAF_PA_SIM_REFRESH_STAGE_UNKNOWN;
    }
}

static void RefreshSvcStatusHandler(taf_prop_sim_RefreshChangeInd_t indication, void* contextPtr)
{
    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.indicators.refreshSvcStatus.handlerFuncPtr != nullptr)
    {
        auto handler = (taf_pa_sim_RefreshChangeHandlerFunc_t)pa.indicators.refreshSvcStatus.handlerFuncPtr;
        // Convert prop types to PA types and call the registered handler
        taf_pa_sim_RefreshChangeInd_t paInd;

        // Perform type conversion here
        paInd.sessionType = Utility::Convert::SessionType(indication.sessionType);
        paInd.refreshMode = Utility::Convert::RefreshMode(indication.refreshMode);
        paInd.refreshStage = Utility::Convert::RefreshStage(indication.refreshStage);

        handler(paInd, pa.indicators.refreshSvcStatus.contextPtr);
    }
}
pa_result_t taf_pa_sim_Init()
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& phoneFactory = tel::PhoneFactory::getInstance();
    uint32_t slots = 0;

    if (common::DeviceConfig::isMultiSimSupported())
    {
        slots = 2;
        PA_INFO("MultiSim supported.");
    }
    else
    {
        slots = 1;
        PA_INFO("MultiSim not supported.");
    }

    SERVICE_PROMISE_AND_CALLBACK(phone)
    pa.managers.phone = phoneFactory.getPhoneManager(phoneCallback);
    SERVICE_READY(phone)
    PA_INFO("SIM platform adaptor initialization is done.");

    taf_prop_sim_Result_t result = taf_prop_sim_Init();
    pa_result_t paResult =Utility::Convert::Result(result);
    if (paResult != TAF_PA_SIM_RESULT_OK)
    {
        PA_INFO("Sim proprietary platform adaptor is not Initialized.");
        return paResult;
    }
        taf_prop_sim_AddRefreshChangeHandler(RefreshSvcStatusHandler,nullptr);
        PA_INFO("Sim proprietary platform adaptor initialization is done.");
    return TAF_PA_SIM_RESULT_OK;
}
pa_result_t taf_pa_sim_RefreshOk(taf_pa_sim_SessionType_t sessionType, bool* refreshAllow)
{
    if(sessionType == TAF_PA_SIM_SESSION_TYPE_UNKNOWN)
    {
        PA_ERROR("Invalid parameter: sessionType is UNKNOWN.");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    taf_prop_sim_SessionType_t propSessionType = Utility::Convert::SessionType(sessionType);
    taf_prop_sim_Result_t result = taf_prop_sim_RefreshOk(propSessionType,refreshAllow);
    return Utility::Convert::Result(result);
}

pa_result_t taf_pa_sim_RefreshRegister(
    taf_pa_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_pa_sim_RefreshFile_t* files
)
{
    // Validate input
    if (files == nullptr || filesLen > MAX_SIM_REFRESH_FILES)
    {
        PA_ERROR("Invalid parameters: files is nullptr or filesLen exceeds limit.");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    taf_prop_sim_SessionType_t propSessionType = Utility::Convert::SessionType(sessionType);
    taf_prop_sim_Result_t result = taf_prop_sim_RefreshRegister(propSessionType, filesLen, (taf_prop_sim_RefreshFile_t*)files);
    return Utility::Convert::Result(result);
}

pa_result_t taf_pa_sim_RefreshComplete
(
   taf_pa_sim_SessionType_t sessionType
)
{
    if(sessionType == TAF_PA_SIM_SESSION_TYPE_UNKNOWN)
    {
        PA_ERROR("Invalid parameter: sessionType is UNKNOWN.");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    taf_prop_sim_SessionType_t propSessionType = Utility::Convert::SessionType(sessionType);
    taf_prop_sim_Result_t result = taf_prop_sim_RefreshComplete(propSessionType);
    return Utility::Convert::Result(result);
}

taf_pa_sim_RefreshChangeHandlerRef_t taf_pa_sim_AddRefreshChangeHandler
(
    taf_pa_sim_RefreshChangeHandlerFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    pa.indicators.refreshSvcStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.refreshSvcStatus.contextPtr = contextPtr;
    return (taf_pa_sim_RefreshChangeHandlerRef_t)&pa.indicators.refreshSvcStatus;

}

void taf_pa_sim_RemoveRefreshChangeHandler
(
   taf_pa_sim_RefreshChangeHandlerRef_t handlerRef ///< [IN] Handler reference.
)
{
  PA_INFO("taf_pa_sim_RemoveRefreshChangeHandler");
  auto& pa = PlatformAdaptor::GetInstance();
  // Reset the stored handler details
  pa.indicators.refreshSvcStatus.handlerFuncPtr = nullptr;
  pa.indicators.refreshSvcStatus.contextPtr = nullptr;
}

uint8_t taf_pa_sim_GetProfileNum
(
    taf_pa_sim_SlotId_t slot
)
{
    if (slot == TAF_PA_SIM_SLOT_UNKNOWN)
    {
        PA_ERROR("invalid slot");
        return 0;
    }

    uint8_t slotIndex = SlotToIndex(slot);
    const uint32_t MAX_PROFILES = 8;

    taf_prop_sim_ProfileInfo_t propProfiles[MAX_PROFILES];
    memset(propProfiles, 0, sizeof(propProfiles));

    uint32_t count = MAX_PROFILES;
    taf_prop_sim_Result_t r = taf_prop_sim_GetProfileList(slotIndex, propProfiles, &count);
    pa_result_t paRes = Utility::Convert::Result(r);

    if (paRes != TAF_PA_SIM_RESULT_OK)
    {
        PA_ERROR("taf_prop_sim_GetProfileList failed (res=%d)", (int)r);
        return 0;
    }

    return (uint8_t)count;
}

taf_pa_sim_ProfileInfo_t taf_pa_sim_GetProfile
(
    taf_pa_sim_SlotId_t slot,
    uint8_t index
)
{
    taf_pa_sim_ProfileInfo_t info = MakeInvalidProfileInfo();

    if (slot == TAF_PA_SIM_SLOT_UNKNOWN)
    {
        PA_ERROR("invalid slot");
        return info;
    }

    uint8_t slotIndex = SlotToIndex(slot);

    const uint32_t MAX_PROFILES = 8;
    taf_prop_sim_ProfileInfo_t propProfiles[MAX_PROFILES];
    memset(propProfiles, 0, sizeof(propProfiles));

    uint32_t count = MAX_PROFILES;
    taf_prop_sim_Result_t r = taf_prop_sim_GetProfileList(slotIndex, propProfiles, &count);
    pa_result_t paRes = Utility::Convert::Result(r);

    if (paRes != TAF_PA_SIM_RESULT_OK)
    {
        PA_ERROR("taf_prop_sim_GetProfileList failed (res=%d)", (int)r);
        return info;
    }

    if (index >= count)
    {
        PA_ERROR("index %u out of range (count=%u)", (unsigned)index, (unsigned)count);
        return info;
    }

    auto &src = propProfiles[index];

    info.profileId = ProfileIdToEnum(src.profileId);
    info.type      = ConvertToPaProfileType(src.profileType);
    info.state     = src.isActive
                     ? TAF_PA_SIM_PROFILE_STATE_ACTIVE
                     : TAF_PA_SIM_PROFILE_STATE_INACTIVE;

    return info;
}

pa_result_t taf_pa_sim_SetActiveProfile
(
    taf_pa_sim_SlotId_t slot,
    taf_pa_sim_ProfileId_t profileId
)
{
    if (slot == TAF_PA_SIM_SLOT_UNKNOWN || profileId == TAF_PA_SIM_PROFILE_ID_UNKNOWN)
    {
        PA_ERROR("invalid slot or profileId");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }

    uint8_t slotIndex   = SlotToIndex(slot);
    uint8_t profileIdU8 = ProfileEnumToId(profileId);

    if (profileIdU8 == 0)
    {
        PA_ERROR("profileId enum maps to invalid id");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }

    PA_INFO("slot=%u, profileId=%u", (unsigned)slotIndex, (unsigned)profileIdU8);

    // QMI_UIM_SET_SIM_PROFILE with enable_profile = QMI_ENABLE
    taf_prop_sim_Result_t r = taf_prop_sim_SetSimProfileById(slotIndex, profileIdU8);
    pa_result_t paRes = Utility::Convert::Result(r);

    if (paRes == TAF_PA_SIM_RESULT_OK)
    {
        PA_INFO("SetActiveProfile OK");
    }
    else
    {
        PA_ERROR("SetActiveProfile FAIL, res=%d", (int)r);
    }

    return paRes;
}
