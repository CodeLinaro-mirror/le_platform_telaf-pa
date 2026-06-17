/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <shared_mutex>
#include <atomic>
#include <telux/common/DeviceConfig.hpp>
#include <telux/common/Utils.hpp>
#include <telux/tel/PhoneDefines.hpp>
#include <telux/tel/PhoneFactory.hpp>
#include "tafInternalCommonPa.h"

#include "tafSimPa.hpp"
#include "taf_prop_sim.hpp"

using namespace telux::tel;
using namespace telux::common;
using namespace std;
using namespace telux;

#define SERVICE_TIMEOUT 5
#define REQUEST_TIMEOUT 5
#define TAF_PA_SIM_SUBSYSTEM_TIMEOUT 30

static bool subListenerRegistered = {false};
static bool cardListenerRegistered = {false};
static bool multiSimListenerRegistered = {false};
static std::atomic<bool> g_simPaInitialized(false);
static std::atomic<bool> g_propSimInitialized(false);

static void RefreshSvcStatusHandler(taf_prop_sim_RefreshChangeInd_t indication, void* contextPtr);

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

#define SERVICE_READY(name,manager)                                              \
    future<common::ServiceStatus> name##Future = name##Promise->get_future();    \
    future_status name##Status = name##Future.wait_for(                          \
        chrono::seconds(SERVICE_TIMEOUT));                                       \
        common::ServiceStatus name##ServiceStatus;                               \
        if (future_status::timeout == name##Status)                              \
        {                                                                        \
            PA_CRIT("Timeout for %s.", #name);                                   \
            manager = nullptr;                                                   \
        }                                                                        \
        else                                                                     \
        {                                                                        \
            name##ServiceStatus = name##Future.get();                            \
            if (name##ServiceStatus != common::ServiceStatus::SERVICE_AVAILABLE) \
            {                                                                    \
                PA_CRIT("%s is not available.", #name);                          \
                manager = nullptr;                                               \
            }                                                                    \
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
    std::map<int, std::shared_ptr<telux::tel::ICard>> cards;
} Manager_t;

typedef struct
{
    Handler_t refreshSvcStatus;
} Indicator_t;

typedef struct
{
    taf_pa_sim_info_t simList[TAF_PA_SIM_ID_MAX];
} SimContext_t;

enum class CardEvent
{
    TAF_PA_OPEN_LOGICAL_CHANNEL = 1,
    TAF_PA_CLOSE_LOGICAL_CHANNEL = 2,
    TAF_PA_TRANSMIT_APDU_CHANNEL = 3
};

class tafPaSubscriptionListener : public telux::tel::ISubscriptionListener
{
    public:
        tafPaSubscriptionListener(){};
        void onSubscriptionInfoChanged(std::shared_ptr<telux::tel::ISubscription> subscription);
};

class tafPaCardListener : public telux::tel::ICardListener
{
    public:
        tafPaCardListener(){};
        void onCardInfoChanged(int slotId) override;
        void onServiceStatusChange(telux::common::ServiceStatus status) override;
};

class tafPaMultiSimListener : public telux::tel::IMultiSimListener
{
    public:
       tafPaMultiSimListener(){};
       void onSlotStatusChanged(std::map<SlotId, telux::tel::SlotStatus> slotStatus) override;
};

class tafPaAuthenticationResponseCallback
{
    public:
    static void unlockCardByPinResponseCb(int retryCount, telux::common::ErrorCode error);
    static void ChangeCardPinResponseCb(int retryCount, telux::common::ErrorCode error);
    static void unlockCardByPukResponseCb(int retryCount, telux::common::ErrorCode error);
    static void setCardLockResponseCb(int retryCount, telux::common::ErrorCode error);
};

class tafPaOpenLogicalChannelCallback : public ICardChannelCallback
{
    public:
        tafPaOpenLogicalChannelCallback(std::shared_ptr<std::promise<pa_result_t>> promise)
        : promise_(std::move(promise)) {}
        void onChannelResponse(int channel, telux::tel::IccResult result,
            telux::common::ErrorCode error) override;
    private:
        std::shared_ptr<std::promise<pa_result_t>> promise_;
};

class tafPaCloseLogicalChannelCallback : public ICommandResponseCallback
{
    public:
        tafPaCloseLogicalChannelCallback(std::shared_ptr<std::promise<pa_result_t>> promise)
        : promise_(std::move(promise)) {}
        void commandResponse(telux::common::ErrorCode error) override;
    private:
        std::shared_ptr<std::promise<pa_result_t>> promise_;
};

class tafPaTransmitApduResponseCallback : public ICardCommandCallback
{
    public:
        tafPaTransmitApduResponseCallback(std::shared_ptr<std::promise<pa_result_t>> promise)
        : promise_(std::move(promise)) {}
    void onResponse(IccResult result, telux::common::ErrorCode error) override;
    private:
       std::shared_ptr<std::promise<pa_result_t>> promise_;
};

class PlatformAdaptor
{
    public:
        Manager_t managers;
        Indicator_t indicators;
        SimContext_t simContext;
        int slot = TAF_PA_DEFAULT_SLOT_ID;
        int slotCount = 0;
        bool isSingleActive = false;
        bool cardRespReceived = false;
        CardEvent cardEventExpected;
        telux::common::ErrorCode errorCode;
        uint8_t openChannel = 0;
        IccResult apduResponse;
        std::condition_variable eventCV;
        std::shared_mutex regListenerMutex_;
        std::shared_mutex deRegListenerMutex_;
        std::shared_mutex powerMutex_;
        std::shared_mutex cardsMutex_;
        std::shared_mutex simSlotMutex_;
        std::mutex eventMutex;
        std::mutex subscriptionMutex;
        std::mutex slotChangeMutex;
        // Single mutex shared by RegisterListeners and DeregisterListeners to ensure
        // they are mutually exclusive
        std::mutex listenerRegistrationMutex_;
        std::mutex eventListenerMutex_;
        std::mutex refreshHandlerMutex_;
        static PlatformAdaptor& GetInstance
        (
            void
        );
        void InitializeSimInfo
        (
            std::shared_ptr<telux::tel::ISubscription> subscription,
            taf_pa_sim_Id_t simId
        );
        taf_pa_sim_info_t* GetSimContext
        (
            taf_pa_sim_Id_t simId
        );
        taf_pa_sim_Id_t GetSelectedCard
        (
        );
        void requestsSlotsStatusResponse
        (
           std::map<SlotId,telux::tel::SlotStatus> slotStatus,
           telux::common::ErrorCode error
        );

        std::shared_ptr<telux::tel::ICard> GetCard
        (
            int slotId
        );

        // thread-safe accessors for pa.slot.
        // All reads of pa.slot must go through GetCurrentSlot() (shared lock) and all
        // writes must go through SetCurrentSlot() (unique lock) so that the SB write in
        // onSlotStatusChanged() / requestsSlotsStatusResponse() cannot race with the NB
        // reads in the APDU/card-pin/power functions.
        int  GetCurrentSlot();
        void SetCurrentSlot(int newSlot);
        // Convenience wrapper: reads pa.slot under the shared lock and returns the
        // corresponding card pointer.
        std::shared_ptr<telux::tel::ICard> GetCurrentCard();

        pa_result_t RegisterEventListener
        (
            taf_pa_sim_EventListener* listener,
            std::any context
        );
        pa_result_t MapErrorCode
        (
            telux::common::ErrorCode errorCode
        );
        bool WaitForCardEvent
        (
            CardEvent cardEvent
        );
        PlatformAdaptor() = default;
        ~PlatformAdaptor() = default;
        shared_ptr<tel::ISubscriptionListener> subscriptionListener;
        std::shared_ptr<tafPaSubscriptionListener> tafSubListener;
        std::shared_ptr<telux::tel::ICardListener> cardListener;
        std::shared_ptr<tafPaCardListener> tafCardListener;
        std::shared_ptr<telux::tel::IMultiSimListener> multiSimListener;
        std::shared_ptr<tafPaMultiSimListener> tafMultiSimListener;
        shared_ptr<tel::ISubscriptionManager> subMgr = nullptr;
        shared_ptr<tel::ICardManager> cardManager = nullptr;
        std::shared_ptr<telux::tel::IMultiSimManager> multiSimMgr = nullptr;
        taf_pa_sim_EventListener* eventListener_ = nullptr ;
    private:
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

pa_result_t PlatformAdaptor::RegisterEventListener
(
    taf_pa_sim_EventListener* listener,
    std::any context
)
{
    PA_INFO("RegisterEventListener listener: %p",listener);
    if (listener != nullptr) {
        std::lock_guard<std::mutex> lock(eventListenerMutex_);
        eventListener_ = listener;
    }
    else
    {
        return PA_BAD_PARAMETER;
    }
    return PA_OK;
}

pa_result_t PlatformAdaptor::MapErrorCode(telux::common::ErrorCode errorCode)
{
    switch(errorCode)
    {
        case telux::common::ErrorCode::SUCCESS:
            PA_DEBUG("Operation processed successfully");
            return PA_OK;
        case telux::common::ErrorCode::GENERIC_FAILURE:
            PA_ERROR("Operation processing failed");
            return PA_FAULT;
        case telux::common::ErrorCode::INVALID_ARGUMENTS:
            PA_ERROR("Input parameters are invalid");
            return PA_BAD_PARAMETER;
        case telux::common::ErrorCode::OPERATION_NOT_ALLOWED:
            PA_ERROR("Operation not allowed");
            return PA_NOT_PERMITTED;
        case telux::common::ErrorCode::TIMEOUT_ERROR:
            PA_ERROR("TimeOut Error");
            return PA_TIMEOUT;
        case telux::common::ErrorCode::INFO_UNAVAILABLE:
            PA_ERROR("Information not available");
            return PA_UNAVAILABLE;
        case telux::common::ErrorCode::SUBSYSTEM_UNAVAILABLE:
            PA_ERROR("Subsystem Not Available");
            return PA_UNAVAILABLE;
        case telux::common::ErrorCode::REQUEST_NOT_SUPPORTED:
            PA_ERROR("Request Not supported");
            return PA_UNSUPPORTED;
        default:
            return PA_FAULT;
    }
}

void tafPaSubscriptionListener::onSubscriptionInfoChanged
(
    std::shared_ptr<telux::tel::ISubscription> subscription
)
{
    PA_INFO("onSubscriptionInfoChanged in PA layer");

    // Get the platform adaptor instance to access managers
    auto& pa = PlatformAdaptor::GetInstance();
    std::unique_lock<std::mutex> lock(pa.subscriptionMutex);
    taf_pa_sim_info_t* simPtr = NULL;
    telux::common::Status status;

    if (subscription)
    {
        auto slotId = subscription->getSlotId();
        PA_INFO("Subscription info changed for slot: %d", slotId);
        PA_DEBUG("ICCID: %s", subscription->getIccId().c_str());
        PA_DEBUG("IMSI: %s", subscription->getImsi().c_str());
        PA_DEBUG("Phone Number: %s", subscription->getPhoneNumber().c_str());
        {
            // Acquire write lock on cardsMutex_ before modifying managers.cards
            std::unique_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
            auto it = pa.managers.cards.find(slotId);
            if((it == pa.managers.cards.end() || it->second == nullptr)
               && slotId == TAF_PA_DEFAULT_SLOT_ID)
            {
                slotId = TAF_PA_SIM_SLOT_ID_2;
                auto card = pa.cardManager->getCard(TAF_PA_DEFAULT_SLOT_ID, &status);
                PA_INFO("Update cards[%d] as %s", slotId, card == nullptr ? "null" : "non-null");
                pa.managers.cards[slotId] = card;
            }
        }
        pa.InitializeSimInfo(subscription,(taf_pa_sim_Id_t)slotId);
        simPtr = pa.GetSimContext((taf_pa_sim_Id_t)slotId);
    }
    else
    {
        PA_INFO("Subscription is null");
        // read pa.slot under simSlotMutex_ (shared lock).
        int curSlot = pa.GetCurrentSlot();
        pa.InitializeSimInfo(nullptr,(taf_pa_sim_Id_t)curSlot);
        simPtr = pa.GetSimContext((taf_pa_sim_Id_t)curSlot);
    }
    auto simIccidEvent = std::make_shared<taf_pa_sim_Iccid_t>();
    simIccidEvent->simId = (taf_pa_sim_Id_t)simPtr->simId;
    simIccidEvent->ICCID = simPtr->ICCID;
    PA_DEBUG(" simIccidEvent->ICCID : %s",simIccidEvent->ICCID.c_str());
    taf_pa_sim_EventListener* listener1 = nullptr;
    {
        std::lock_guard<std::mutex> elLock(pa.eventListenerMutex_);
        listener1 = pa.eventListener_;
    }
    if(listener1 && listener1->onSubscriptionInfoChanged)
    {
        PA_INFO("onSubscriptionInfoChanged triggered");
        listener1->onSubscriptionInfoChanged(simIccidEvent);
    }
    else
    {
        PA_ERROR("unable to find event Listener for onSubscriptionInfoChanged");
    }
}

void tafPaCardListener::onCardInfoChanged(int slotId)
{
    PA_INFO("onCardInfoChanged in PA layer");
    PA_INFO("Card info changed for slot: %d", slotId);
    auto& pa = PlatformAdaptor::GetInstance();
    std::unique_lock<std::mutex> lock(pa.slotChangeMutex);
    taf_pa_sim_pa_event_t simEvent;
    auto slotWithCard = slotId;
    PA_INFO("Input sim Id: %d, cards size: %zu", (int)slotId, pa.managers.cards.size());
    {
        // Acquire read lock to safely read managers.cards
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(slotWithCard);
        if(it != pa.managers.cards.end() && it->second == nullptr
           && slotWithCard == TAF_PA_DEFAULT_SLOT_ID)
        {
            // Map sdk logical slot to physical slot.
            slotWithCard = TAF_PA_SIM_SLOT_ID_2;
        }
    }
    simEvent.simId = (taf_pa_sim_Id_t)slotWithCard;
    taf_pa_sim_States_t state;
    if(taf_pa_sim_GetState((taf_pa_sim_Id_t)slotWithCard,&state) == TAF_PA_SIM_RESULT_OK)
    {
        simEvent.state = state;
        PA_INFO("taf_pa_sim_GetState simEvent.state: %d ",simEvent.state);
    }
    else
    {
        PA_INFO("taf_pa_sim_getState failed to get the state");
        simEvent.state = TAF_PA_SIM_ABSENT;
    }
    if (simEvent.state == TAF_PA_SIM_ABSENT)
    {
        // Revert to original slotId for initialization
        slotWithCard = slotId;
        simEvent.simId = (taf_pa_sim_Id_t)slotWithCard;
        pa.InitializeSimInfo(nullptr, (taf_pa_sim_Id_t)slotWithCard);
    }
    auto simCardInfo = std::make_shared<taf_pa_sim_CardInfo_t>();

    if(simCardInfo == nullptr)
    {
        PA_INFO("simCardInfo is null");
        return;
    }
    simCardInfo->slotId = (taf_pa_sim_Id_t)slotWithCard;
    simCardInfo->state = simEvent.state;

    taf_pa_sim_EventListener* listener2 = nullptr;
    {
        std::lock_guard<std::mutex> elLock(pa.eventListenerMutex_);
        listener2 = pa.eventListener_;
    }
    if(listener2 && listener2->onCardInfoChanged)
    {
        PA_INFO("onCardInfoChanged is triggered");
        listener2->onCardInfoChanged(simCardInfo);
    }
    else
    {
        PA_ERROR("unable to find event Listener for onCardInfoChanged");
    }
}

void tafPaCardListener::onServiceStatusChange
(
    telux::common::ServiceStatus status
)
{
    if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_INFO("CardManager service status changed to available. Re-initializing prop SIM.");

        if (!g_propSimInitialized.load(std::memory_order_acquire))
        {
            taf_prop_sim_Result_t res = taf_prop_sim_Init();
            if (res == TAF_PROP_SIM_RESULT_OK)
            {
                PA_INFO("taf_prop_sim_Init() completed successfully after service recovery.");
                taf_prop_sim_AddRefreshChangeHandler(RefreshSvcStatusHandler, nullptr);
                g_propSimInitialized.store(true, std::memory_order_release);
            }
            else
            {
                PA_ERROR("taf_prop_sim_Init() failed with result %d after service recovery.", res);
            }
        }
        return;
    }

    PA_WARN("CardManager service status changed to unavailable. Calling taf_prop_sim_Deinit().");

    if (g_propSimInitialized.load(std::memory_order_acquire))
    {
        pa_result_t removeRes = taf_pa_sim_RemoveRefreshChangeHandler(nullptr);
        if (removeRes == PA_OK)
        {
            PA_INFO("taf_pa_sim_RemoveRefreshChangeHandler() completed successfully.");
        }
        else
        {
            PA_ERROR("taf_pa_sim_RemoveRefreshChangeHandler() failed with result %d.", removeRes);
        }

        int32_t res = taf_prop_sim_Deinit();
        if (res == 0)
        {
            PA_INFO("taf_prop_sim_Deinit() completed successfully.");
        }
        else if (res == -EINVAL)
        {
            PA_WARN("taf_prop_sim_Deinit() called before Init() was successfully called.");
        }
        else
        {
            PA_ERROR("taf_prop_sim_Deinit() failed with result %d.", res);
        }
        g_propSimInitialized.store(false, std::memory_order_release);
    }
}

void tafPaMultiSimListener::onSlotStatusChanged
(
    std::map<SlotId, telux::tel::SlotStatus> slotStatus
)
{
    PA_INFO("onSlotStatusChanged: %d", slotStatus.size());
    auto& pa = PlatformAdaptor::GetInstance();
    int activeSlotCount = 0;
    telux::common::Status status;
    int activeSlots = 0;

    for(auto it = slotStatus.begin(); it != slotStatus.end(); ++it)
    {
        auto slotStatus = it->second;
        if (slotStatus.slotState == telux::tel::SlotState::ACTIVE)
        {
            activeSlots++;
        }
    }
    pa.cardManager->getSlotCount(activeSlotCount);
    if(activeSlotCount == 1)
    {
        pa.isSingleActive = true;
    }
    PA_INFO("activeSlotCount: %d, isSingleActive: %d", activeSlots, (int) pa.isSingleActive);

    // Acquire write lock before modifying managers.cards
    std::unique_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);

    for(auto it = slotStatus.begin(); it != slotStatus.end(); ++it)
    {
        auto slotId = it->first;
        auto slotStatus = it->second;

        PA_INFO("Slot: %d, slotState: %d, cardState: %d, cardError: %d", slotId,
               (int) slotStatus.slotState, (int) slotStatus.cardState,
               (int) slotStatus.cardError);

        if(activeSlots == 1)
        { //Single Active slot
            if (slotStatus.slotState == telux::tel::SlotState::ACTIVE)
            {
                // write pa.slot under simSlotMutex_ (unique lock).
                pa.SetCurrentSlot(slotId);
            }
            if (slotStatus.cardState != telux::tel::CardState::CARDSTATE_UNKNOWN
                   && slotStatus.cardState != telux::tel::CardState::CARDSTATE_ABSENT) {
                PA_INFO("Find card for single active in slot: %d", slotId);
                auto card = pa.cardManager->getCard(TAF_PA_DEFAULT_SLOT_ID, &status);
                pa.managers.cards[slotId] = card;
                PA_INFO("card as %s in cards[%d]", card == nullptr ? "null" : "non-null", slotId);
            }
            else
            {
                pa.managers.cards[slotId] = nullptr;
                PA_INFO("Update card as null in cards[%d]", slotId);
            }
        }
        else if((slotStatus.slotState == telux::tel::SlotState::ACTIVE)
                && (activeSlots == 2)){
            PA_INFO("Find card for dual active in slot: %d", slotId);
            auto card = pa.cardManager->getCard(slotId, &status);
            pa.managers.cards[slotId] = card;
        }
        else
        {
            PA_INFO("Find no card for slot: %d", slotId);
            pa.managers.cards[slotId] = nullptr;
        }
    }
}

void tafPaAuthenticationResponseCallback:: ChangeCardPinResponseCb
(
    int retryCount,
    telux::common::ErrorCode error
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("ChangeCardPinResponseCb");
    // read pa.slot under simSlotMutex_ (shared lock).
    int currentSlot = pa.GetCurrentSlot();
    taf_pa_sim_info_t* simPtr = pa.GetSimContext((taf_pa_sim_Id_t)currentSlot);
    telaf_pa_sim_pa_response_event_t simResponsePtr;
    simResponsePtr.simId = (taf_pa_sim_Id_t) currentSlot;
    simResponsePtr.responseType = TAF_PA_CHANGE_PIN;
    if(error != telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Change Card Pin Request failed with errorCode: %d",(int) error);
        PA_INFO("Change Card Pin Request failed retryCount:%d",retryCount);
        simPtr->pinTryCount = retryCount;
        simResponsePtr.result = TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        PA_INFO("Change Card Pin Request successful retryCount:%d",retryCount);
        simPtr->pinTryCount = retryCount;
        simResponsePtr.result = TAF_PA_SIM_RESULT_OK;
    }
    auto simReponseData = std::make_shared<taf_pa_sim_ResponseInfo_t>();
    simReponseData->simId = simResponsePtr.simId;
    simReponseData->responseType = simResponsePtr.responseType;
    simReponseData->result = simResponsePtr.result;

    taf_pa_sim_EventListener* listener3 = nullptr;
    {
        std::lock_guard<std::mutex> elLock(pa.eventListenerMutex_);
        listener3 = pa.eventListener_;
    }
    if(listener3 && listener3->ChangeCardPinResponseCb)
    {
        PA_INFO("ChangeCardPinResponseCb->ChangeCardPinResponseCb");
        listener3->ChangeCardPinResponseCb(simReponseData);
    }
    else
    {
        PA_ERROR("unable to find event Listener for ChangeCardPinResponseCb");
    }
}

void tafPaAuthenticationResponseCallback:: unlockCardByPinResponseCb
(
    int retryCount,
    telux::common::ErrorCode error
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("unlockCardByPinResponseCb");
    // read pa.slot under simSlotMutex_ (shared lock).
    int currentSlot = pa.GetCurrentSlot();
    taf_pa_sim_info_t* simPtr = pa.GetSimContext((taf_pa_sim_Id_t)currentSlot);
    telaf_pa_sim_pa_response_event_t simResponsePtr;
    simResponsePtr.simId = (taf_pa_sim_Id_t) currentSlot;
    simResponsePtr.responseType = TAF_PA_UNLOCK_BY_PIN;
    if(error != telux::common::ErrorCode::SUCCESS)
    {
        simPtr->pinTryCount = retryCount;
        PA_INFO("Unlock Card By Pin Request failed with errorCode: %d ",(int)error);
        PA_INFO( "Unlock Card By Pin Request failed retryCount: %d",retryCount);
        simResponsePtr.result = TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        simPtr->pinTryCount = retryCount;
        simResponsePtr.result = TAF_PA_SIM_RESULT_OK;
        PA_INFO( "Unlock Card By Pin Request successful retryCount: %d",retryCount);
    }
    auto simReponseData = std::make_shared<taf_pa_sim_UnlockCardResponseInfo_t>();
    simReponseData->simId = simResponsePtr.simId;
    simReponseData->responseType = simResponsePtr.responseType;
    simReponseData->result = simResponsePtr.result;

    taf_pa_sim_EventListener* listener4 = nullptr;
    {
        std::lock_guard<std::mutex> elLock(pa.eventListenerMutex_);
        listener4 = pa.eventListener_;
    }
    if(listener4 && listener4->unlockCardByPinResponseCb)
    {
        PA_INFO("unlockCardByPinResponseCb is triggered");
        listener4->unlockCardByPinResponseCb(simReponseData);
    }
    else
    {
        PA_ERROR("unable to find event Listener for unlockCardByPinResponseCb");
    }

}
void tafPaAuthenticationResponseCallback:: unlockCardByPukResponseCb
(
    int retryCount,
    telux::common::ErrorCode error
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("unlockCardByPukResponseCb");
    // read pa.slot under simSlotMutex_ (shared lock).
    int currentSlot = pa.GetCurrentSlot();
    taf_pa_sim_info_t* simPtr = pa.GetSimContext((taf_pa_sim_Id_t)currentSlot);
    telaf_pa_sim_pa_response_event_t simResponsePtr;
    simResponsePtr.simId = (taf_pa_sim_Id_t) currentSlot;
    simResponsePtr.responseType = TAF_PA_UNLOCK_BY_PUK;

    if(error != telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Unlock Card By Puk Request failed with errorCode:%d ",(int)error);
        PA_INFO("Unlock Card By Puk request failed retryCount:%d",retryCount);
        simPtr->pukTryCount = retryCount;
        simResponsePtr.result = TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        PA_INFO("Unlock Card By Puk request successful retryCount:%d",retryCount);
        simPtr->pinTryCount = 3;
        simPtr->pukTryCount = 10;
        simResponsePtr.result = TAF_PA_SIM_RESULT_OK;
    }
    auto simReponseData = std::make_shared<taf_pa_sim_UnlockCardPukResponseInfo_t>();
    simReponseData->simId = simResponsePtr.simId;
    simReponseData->responseType = simResponsePtr.responseType;
    simReponseData->result = simResponsePtr.result;

    taf_pa_sim_EventListener* listener5 = nullptr;
    {
        std::lock_guard<std::mutex> elLock(pa.eventListenerMutex_);
        listener5 = pa.eventListener_;
    }
    if(listener5 && listener5->unlockCardByPukResponseCb)
    {
        PA_INFO("unlockCardByPukResponseCb is triggered");
        listener5->unlockCardByPukResponseCb(simReponseData);
    }
    else
    {
        PA_ERROR("unable to find event Listener for unlockCardByPukResponseCb");
    }
}

void tafPaAuthenticationResponseCallback::setCardLockResponseCb
(
    int retryCount,
    telux::common::ErrorCode error
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("setCardLockResponseCb");
    // read pa.slot under simSlotMutex_ (shared lock).
    int currentSlot = pa.GetCurrentSlot();
    telaf_pa_sim_pa_response_event_t simResponsePtr;
    simResponsePtr.simId = (taf_pa_sim_Id_t) currentSlot;
    simResponsePtr.responseType = TAF_PA_SET_LOCK;
    taf_pa_sim_info_t* simPtr = pa.GetSimContext((taf_pa_sim_Id_t)currentSlot);
    if(error != telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Set card lock Request failed with errorCode: %d ",(int)error);
        PA_INFO( "Set card lock Request failed retryCount: %d",retryCount);
        simPtr->pinTryCount = retryCount;
        simResponsePtr.result = TAF_PA_SIM_RESULT_FAULT ;
    }
    else
    {
        PA_INFO( "Set card lock Request successful retryCount: %d",retryCount);
        simPtr->pinTryCount = retryCount;
        simResponsePtr.result = TAF_PA_SIM_RESULT_OK;
    }
    auto simReponseData = std::make_shared<taf_pa_sim_CardLockResponseInfo_t>();
    simReponseData->simId = simResponsePtr.simId;
    simReponseData->responseType = simResponsePtr.responseType;
    simReponseData->result = simResponsePtr.result;

    taf_pa_sim_EventListener* listener6 = nullptr;
    {
        std::lock_guard<std::mutex> elLock(pa.eventListenerMutex_);
        listener6 = pa.eventListener_;
    }
    if(listener6 && listener6->setCardLockResponseCb)
    {
        PA_INFO("setCardLockResponseCb is triggered");
        listener6->setCardLockResponseCb(simReponseData);
    }
    else
    {
        PA_ERROR("unable to find event Listener for unlockCardByPukResponseCb");
    }
}

void tafPaOpenLogicalChannelCallback::onChannelResponse
(
   int channel,
   telux::tel::IccResult result,
   telux::common::ErrorCode error
)
{
   auto& pa = PlatformAdaptor::GetInstance();
   std::unique_lock<std::mutex> lock(pa.eventMutex);
   pa.errorCode = error;
   pa.openChannel = (uint8_t)channel;
   pa.cardRespReceived = true;
   PA_INFO("onChannelResponse");
   if(pa.cardEventExpected == CardEvent::TAF_PA_OPEN_LOGICAL_CHANNEL)
   {
       PA_INFO("callback response sw1: %d, sw2: %d", (uint8_t)result.sw1, (uint8_t)result.sw2);
       if(error == telux::common::ErrorCode::SUCCESS && (uint8_t)result.sw1 == 0x90
       && (uint8_t)result.sw2 == 0x00)
       {
           PA_INFO("local OpenLogicalChannel successful channel = %d", channel);
           promise_->set_value(PA_OK);
           PA_INFO("openChannelPromise.set_value->PA_OK");
       }
       else
       {
           pa.errorCode = telux::common::ErrorCode::SIM_BUSY;
           promise_->set_value(PA_FAULT);
           PA_INFO("local openChannelPromise.set_value->PA_FAULT");
           PA_INFO("OpenLogicalChannel failed");
       }
       PA_INFO("Card Event TAF_PA_OPEN_LOGICAL_CHANNEL found with code : %d", int(error));
       pa.eventCV.notify_one();
   }
}

void tafPaCloseLogicalChannelCallback::commandResponse
(
    telux::common::ErrorCode error
)
{
   auto& pa = PlatformAdaptor::GetInstance();
   PA_INFO("commandResponse");
   if(error == telux::common::ErrorCode::SUCCESS)
   {
      PA_INFO("local closeChannelPromise.set_value successful.");
      promise_->set_value(PA_OK);
   }
   else
   {
      PA_INFO("local closeChannelPromise.set_value failed\n error: %d ", static_cast<int>(error));
      promise_->set_value(PA_FAULT);
   }
   std::unique_lock<std::mutex> lock(pa.eventMutex);
   pa.errorCode = error;
   pa.cardRespReceived = true;
   if(pa.cardEventExpected == CardEvent::TAF_PA_CLOSE_LOGICAL_CHANNEL)
   {
      PA_INFO("Card Event TAF_PA_CLOSE_LOGICAL_CHANNEL found with code : %d", int(error));
      pa.eventCV.notify_one();
   }
}

void tafPaTransmitApduResponseCallback::onResponse
(
    telux::tel::IccResult result,
    telux::common::ErrorCode error
)
{
   PA_INFO("onResponse, error: %d ",(int)error);
   auto& pa = PlatformAdaptor::GetInstance();
   std::unique_lock<std::mutex> lock(pa.eventMutex);
   pa.errorCode = error;
   pa.apduResponse = result;
   pa.cardRespReceived = true;
   PA_INFO("onResponse: %s " , result.toString().c_str());
   if(error == telux::common::ErrorCode::SUCCESS)
   {
       PA_INFO("local sendApduPromise set_value successful.");
       promise_->set_value(PA_OK);
   }
   else
   {
       PA_INFO("local sendApduPromise set_value failed\n error: %d ", static_cast<int>(error));
       promise_->set_value(PA_FAULT);
   }
   if(pa.cardEventExpected == CardEvent::TAF_PA_TRANSMIT_APDU_CHANNEL)
   {
       PA_INFO("Card Event TAF_PA_TRANSMIT_APDU_CHANNEL found with code : %d", int(error));
       pa.eventCV.notify_one();
   }
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
    taf_pa_sim_RefreshChangeHandlerFunc_t handler;
    void* ctx;
    {
        std::lock_guard<std::mutex> lock(pa.refreshHandlerMutex_);
        handler = (taf_pa_sim_RefreshChangeHandlerFunc_t)
                    pa.indicators.refreshSvcStatus.handlerFuncPtr;
        ctx = pa.indicators.refreshSvcStatus.contextPtr;
    }
    if (handler != nullptr)
    {
        // Convert prop types to PA types and call the registered handler
        taf_pa_sim_RefreshChangeInd_t paInd;

        // Perform type conversion here
        paInd.sessionType = Utility::Convert::SessionType(indication.sessionType);
        paInd.refreshMode = Utility::Convert::RefreshMode(indication.refreshMode);
        paInd.refreshStage = Utility::Convert::RefreshStage(indication.refreshStage);
        paInd.filesLen = indication.filesLen;

        // Copy files array (limit to MAX_SIM_REFRESH_FILES)
        uint32_t filesToCopy = (indication.filesLen < MAX_SIM_REFRESH_FILES) ?
                               indication.filesLen : MAX_SIM_REFRESH_FILES;
        if (filesToCopy > 0 && indication.files != nullptr)
        {
            memcpy(paInd.files, indication.files,
                   filesToCopy * sizeof(taf_pa_sim_RefreshFile_t));
        }

        handler(paInd, ctx);
    }
}

pa_result_t taf_pa_sim_Init()
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& phoneFactory = tel::PhoneFactory::getInstance();
    uint32_t slots = 0;
    PA_INFO("taf_pa_sim_Init");
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

    //Initialize subsription manager
    pa.subMgr = phoneFactory.getSubscriptionManager();
    PA_INFO("Initializing subscription manager...");

    if(pa.subMgr == nullptr)
    {
        PA_INFO("pa.subMgr is null, so get subscription manager...");
        auto& phoneFactory = tel::PhoneFactory::getInstance();
        pa.subMgr = phoneFactory.getSubscriptionManager();
    }
    if(pa.subMgr == nullptr )
    {
        PA_INFO("Sim manager initialize error...");
        return TAF_PA_SIM_RESULT_FAULT ;
    }
    else
    {
        telux::common::ServiceStatus subMgrStatus = pa.subMgr->getServiceStatus();
        if (subMgrStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Subscription subsystem is not ready, waiting for it to be ready...");
            std::promise<telux::common::ServiceStatus> subMgrProm;
            pa.subMgr = phoneFactory.getSubscriptionManager(
                        [&](telux::common::ServiceStatus status)
            {
                PA_INFO("Getting status:%d from subscription manager", (int)status);
                if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                {
                    subMgrProm.set_value(telux::common::ServiceStatus::SERVICE_AVAILABLE);
                }
                else
                {
                    subMgrProm.set_value(telux::common::ServiceStatus::SERVICE_FAILED);
                }
            });
            std::future<telux::common::ServiceStatus> initFuture = subMgrProm.get_future();
            std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(
                TAF_PA_SIM_SUBSYSTEM_TIMEOUT));
            if (std::future_status::timeout == waitStatus)
            {
                PA_CRIT("Timeout waiting for subscription susbsytem");
                pa.subMgr = nullptr;
            }
            else
            {
                subMgrStatus = initFuture.get();
            }
        }
        if (subMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Subscription subsystem is ready.");
        }
        else
        {
            PA_ERROR("Fail to init subscription subsystem");
            pa.subMgr = nullptr;
            return TAF_PA_SIM_RESULT_FAULT;
        }
    }

    //Initializing card manager
    PA_INFO("Initializing card manager...");
    if(pa.cardManager == nullptr)
    {
        auto& phoneFactory = tel::PhoneFactory::getInstance();
        pa.cardManager = phoneFactory.getCardManager();
    }
    if(pa.cardManager == nullptr )
    {
        PA_INFO("card manager initialize error...");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        telux::common::ServiceStatus cardMgrStatus = pa.cardManager->getServiceStatus();
        if (cardMgrStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Card subsystem is not ready, waiting for it to be ready...");
            std::promise<telux::common::ServiceStatus> cardMgrProm;
            pa.cardManager = phoneFactory.getCardManager([&](telux::common::ServiceStatus status)
            {
                PA_INFO("Getting status:%d from card manager", (int)status);
                if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                {
                    cardMgrProm.set_value(telux::common::ServiceStatus::SERVICE_AVAILABLE);
                }
                else
                {
                    cardMgrProm.set_value(telux::common::ServiceStatus::SERVICE_FAILED);
                }
            });
            std::future<telux::common::ServiceStatus> initFuture = cardMgrProm.get_future();
            std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(
                TAF_PA_SIM_SUBSYSTEM_TIMEOUT));
            if (std::future_status::timeout == waitStatus)
            {
                PA_CRIT ("Timeout waiting for card susbsytem");
                pa.cardManager = nullptr;
            }
            else
            {
                cardMgrStatus = initFuture.get();
            }
        }
        if (cardMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Card subsystem is ready.");
        }
        else
        {
            PA_INFO("Fail to init card subsystem");
            pa.cardManager = nullptr;
            return TAF_PA_SIM_RESULT_FAULT;
        }
    }

    //Initializing multi sim manager
    PA_INFO("Initializing multi sim manager...");
    if(pa.multiSimMgr == nullptr)
    {
        auto& phoneFactory = tel::PhoneFactory::getInstance();
        pa.multiSimMgr = phoneFactory.getMultiSimManager();
    }
    if(pa.multiSimMgr == nullptr )
    {
        PA_INFO("multi manager initialize error...");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        telux::common::ServiceStatus multiSimMgrStatus = pa.multiSimMgr->getServiceStatus();
        if (multiSimMgrStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Multi sim subsystem is not ready, waiting for it to be ready...");
            std::promise<telux::common::ServiceStatus> multiSimMgrProm;
            pa.multiSimMgr = phoneFactory.getMultiSimManager(
                             [&](telux::common::ServiceStatus status)
            {
                PA_INFO("Getting status:%d from multi sim manager", (int)status);
                if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                {
                    multiSimMgrProm.set_value(telux::common::ServiceStatus::SERVICE_AVAILABLE);
                }
                else
                {
                    multiSimMgrProm.set_value(telux::common::ServiceStatus::SERVICE_FAILED);
                }
            });
            std::future<telux::common::ServiceStatus> initFuture = multiSimMgrProm.get_future();
            std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(
                TAF_PA_SIM_SUBSYSTEM_TIMEOUT));
            if (std::future_status::timeout == waitStatus)
            {
                PA_CRIT ("Timeout waiting for multi sim susbsytem");
                pa.multiSimMgr = nullptr;
            }
            else
            {
                multiSimMgrStatus = initFuture.get();
            }
        }
        if (multiSimMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("Multi sim subsystem is ready.");
            auto slotStatusCbPromise = std::make_shared<std::promise<telux::common::ErrorCode>>();
            std::future<telux::common::ErrorCode> errorStatus = slotStatusCbPromise->get_future();
            PA_INFO("slotStatusCbPromise->get_future.");
            auto callback = [slotStatusCbPromise](std::map<SlotId,
                    telux::tel::SlotStatus> slotStatus, telux::common::ErrorCode error)
            {
               PA_INFO("callback = [slotStatusCbPromise].");
               auto &sim = PlatformAdaptor::GetInstance();
               if (error == telux::common::ErrorCode::SUCCESS){
                   sim.requestsSlotsStatusResponse(slotStatus, error);
                }
                else
                {
                    PA_ERROR("Request slot status failed with error: %d", (int)error);
                }
                try{
                    slotStatusCbPromise->set_value(error);
                }
                catch (const std::exception &e) {
                    PA_ERROR("Exception in UpdateProfileHandler: %s", e.what());
                }
            };
            PA_INFO(" multiSimManager->requestSlotStatus");
            auto ret = pa.multiSimMgr->requestSlotStatus(callback);
            if(ret != telux::common::Status::SUCCESS)
            {
                PA_CRIT("Request slot status failed with error: %d", (int)ret);
            }
            try{
                telux::common::ErrorCode errorCode = errorStatus.get();
                if(errorCode != telux::common::ErrorCode::SUCCESS){
                    PA_CRIT("Initialize slot card map failed with error:%d",(int)errorCode);
                }
            }
            catch (const std::exception &e) {
                PA_ERROR("Exception in requestSlotStatus: %s", e.what());
            }
        }
        else
        {
            PA_INFO("Fail to init multi sim subsystem");
            pa.multiSimMgr = nullptr;
            return TAF_PA_SIM_RESULT_FAULT;
        }

    }

    PA_INFO("multi sim subsystem is ready.");

   // Initialize simList
    for (auto i = 0; i < TAF_PA_SIM_ID_MAX; i++)
    {
        PA_INFO("Initialize pa.simContext : %p",pa.simContext);
        pa.simContext.simList[i].simId = (taf_pa_sim_Id_t)(i + 1);
        memset(pa.simContext.simList[i].ICCID, 0, TAF_PA_SIM_ICCID_BYTES);
        memset(pa.simContext.simList[i].IMSI, 0, TAF_PA_SIM_IMSI_BYTES);
        memset(pa.simContext.simList[i].phoneNumber, 0, TAF_PA_SIM_PHONE_NUM_MAX_BYTES);
        pa.simContext.simList[i].pinTryCount = 3;
        pa.simContext.simList[i].pukTryCount = 10;
    }

    SERVICE_PROMISE_AND_CALLBACK(phone)
    pa.managers.phone = phoneFactory.getPhoneManager(phoneCallback);
    SERVICE_READY(phone,pa.managers.phone)
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
    g_propSimInitialized.store(true, std::memory_order_release);
    g_simPaInitialized.store(true, std::memory_order_release);
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_Deinit()
{
    PA_INFO("Starting SIM platform adaptor deinitialization...");

    // Step 0: Check if Init() was called successfully
    if (!g_simPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before Init() was successfully called");
        return PA_FAULT;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    pa_result_t overallResult = TAF_PA_SIM_RESULT_OK;

    // Step 1: Clear the refresh change handler so no further refresh indications
    // are dispatched after this point.  Hold refreshHandlerMutex_ so the clear
    // is mutually exclusive with RefreshSvcStatusHandler which reads the same
    // pointer under the same mutex.
    PA_INFO("Clearing refreshSvcStatus handler and context");
    {
        std::lock_guard<std::mutex> lock(pa.refreshHandlerMutex_);
        pa.indicators.refreshSvcStatus.handlerFuncPtr = nullptr;
        pa.indicators.refreshSvcStatus.contextPtr     = nullptr;
    }

    // Step 2: Deregister subscription, card and multi-sim listeners from their
    // respective SDK managers.
    PA_INFO("Deregistering SDK listeners");
    pa_result_t deregResult = taf_pa_sim_DeregisterListeners();
    if (deregResult != TAF_PA_SIM_RESULT_OK)
    {
        PA_ERROR("taf_pa_sim_DeregisterListeners failed: err(%d) - continuing cleanup",
                 deregResult);
        overallResult = deregResult;  // Track the failure
    }

    // Step 3: Clear the PA-level event listener pointer so no further event
    // callbacks are dispatched.  Hold eventListenerMutex_ so the clear is
    // mutually exclusive with any SB callback that reads eventListener_ under
    // the same mutex.
    PA_INFO("Clearing eventListener_");
    {
        std::lock_guard<std::mutex> lock(pa.eventListenerMutex_);
        pa.eventListener_ = nullptr;
    }

    // Step 4: Reset all listener shared pointers so the listener objects are
    // released once no other owners remain.
    PA_INFO("Resetting listener shared pointers");
    pa.subscriptionListener.reset();
    pa.tafSubListener.reset();
    pa.cardListener.reset();
    pa.tafCardListener.reset();
    pa.multiSimListener.reset();
    pa.tafMultiSimListener.reset();

    // Step 5: Clear the card map so stale ICard references are released.
    PA_INFO("Clearing managers.cards map");
    pa.managers.cards.clear();

    // Step 6: Reset manager shared pointers so the underlying SDK objects are
    // released once no other owners remain.
    PA_INFO("Resetting subMgr, cardManager, multiSimMgr and managers.phone");
    pa.subMgr.reset();
    pa.cardManager.reset();
    pa.multiSimMgr.reset();
    pa.managers.phone.reset();

    // Step 7: Deinitialize the proprietary SIM platform adaptor if it was initialized.
    if (g_propSimInitialized.load(std::memory_order_acquire))
    {
        int32_t res = taf_prop_sim_Deinit();
        if (res == 0)
        {
            PA_INFO("taf_prop_sim_Deinit() completed successfully.");
        }
        else if (res == -EINVAL)
        {
            PA_WARN("taf_prop_sim_Deinit() called before Init() was successfully called.");
        }
        else
        {
            PA_ERROR("taf_prop_sim_Deinit() failed with result %d.", res);
            overallResult = TAF_PA_SIM_RESULT_FAULT;
        }
        g_propSimInitialized.store(false, std::memory_order_release);
    }

    // Step 8: Reset the initialization flag
    PA_INFO("Resetting initialization flag");
    g_simPaInitialized.store(false, std::memory_order_release);

    PA_INFO("SIM platform adaptor deinitialization complete.");
    return overallResult;  // Return aggregated status;
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
    taf_prop_sim_SessionType_t propSessionType = Utility::Convert::SessionType(sessionType);
    taf_prop_sim_Result_t result = taf_prop_sim_RefreshRegister(propSessionType, filesLen,(taf_prop_sim_RefreshFile_t*)files);
    return Utility::Convert::Result(result);
}

pa_result_t taf_pa_sim_RefreshUnregister(
    taf_pa_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_pa_sim_RefreshFile_t* files
)
{
    if(sessionType == TAF_PA_SIM_SESSION_TYPE_UNKNOWN)
    {
        PA_ERROR("Invalid parameter: sessionType is UNKNOWN.");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    taf_prop_sim_SessionType_t propSessionType = Utility::Convert::SessionType(sessionType);
    taf_prop_sim_Result_t result = taf_prop_sim_RefreshDeregister(propSessionType,
                                   filesLen, (taf_prop_sim_RefreshFile_t*)files);
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

pa_result_t taf_pa_sim_AddRefreshChangeHandler
(
    taf_pa_sim_RefreshChangeHandlerFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_sim_RefreshChangeHandlerRef_t* handlerRefPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::lock_guard<std::mutex> lock(pa.refreshHandlerMutex_);
    pa.indicators.refreshSvcStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.refreshSvcStatus.contextPtr = contextPtr;
    if (handlerRefPtr) *handlerRefPtr = (taf_pa_sim_RefreshChangeHandlerRef_t)&pa.indicators.refreshSvcStatus;
    return PA_OK;
}

pa_result_t taf_pa_sim_RemoveRefreshChangeHandler
(
   taf_pa_sim_RefreshChangeHandlerRef_t handlerRef ///< [IN] Handler reference.
)
{
  PA_INFO("taf_pa_sim_RemoveRefreshChangeHandler");
  auto& pa = PlatformAdaptor::GetInstance();
  std::lock_guard<std::mutex> lock(pa.refreshHandlerMutex_);
  // Reset the stored handler details
  pa.indicators.refreshSvcStatus.handlerFuncPtr = nullptr;
  pa.indicators.refreshSvcStatus.contextPtr = nullptr;
  return PA_OK;
}

pa_result_t taf_pa_sim_GetProfileNum
(
    taf_pa_sim_SlotId_t slot,
    uint8_t* countPtr
)
{
    if (slot == TAF_PA_SIM_SLOT_UNKNOWN)
    {
        PA_ERROR("invalid slot");
        return PA_BAD_PARAMETER;
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
        return paRes;
    }

    if (countPtr) *countPtr = (uint8_t)count;
    return PA_OK;
}

pa_result_t taf_pa_sim_GetProfile
(
    taf_pa_sim_SlotId_t slot,
    uint8_t index,
    taf_pa_sim_ProfileInfo_t* profileInfoPtr
)
{
    if (slot == TAF_PA_SIM_SLOT_UNKNOWN)
    {
        PA_ERROR("invalid slot");
        return PA_BAD_PARAMETER;
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
        return paRes;
    }

    if (index >= count)
    {
        PA_ERROR("index %u out of range (count=%u)", (unsigned)index, (unsigned)count);
        return PA_BAD_PARAMETER;
    }

    if (profileInfoPtr)
    {
        auto &src = propProfiles[index];
        profileInfoPtr->profileId = ProfileIdToEnum(src.profileId);
        profileInfoPtr->type      = ConvertToPaProfileType(src.profileType);
        profileInfoPtr->state     = src.isActive
                                    ? TAF_PA_SIM_PROFILE_STATE_ACTIVE
                                    : TAF_PA_SIM_PROFILE_STATE_INACTIVE;
    }
    return PA_OK;
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
pa_result_t taf_pa_sim_RegisterListeners
(
    void
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& phoneFactory = tel::PhoneFactory::getInstance();

    std::lock_guard<std::mutex> lock(pa.listenerRegistrationMutex_);

    //Register subscription listener
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    // Register sub listener for each slot it
    if (subListenerRegistered)
    {
        PA_INFO("Sub listener already registered.");
    }
    else
    {
        if (pa.subMgr)
        {
            pa.tafSubListener = std::make_shared<tafPaSubscriptionListener>();
            pa.subscriptionListener = pa.tafSubListener;
            if (pa.subMgr-> registerListener(pa.subscriptionListener) ==
                                             telux::common::Status::SUCCESS)
            {
                PA_INFO("Sub listener registered successfully.");
                subListenerRegistered = true;
            }
            else
            {
                PA_ERROR("Fail to register subscription listener.");
                return TAF_PA_SIM_RESULT_FAULT;
            }
        }
    }

    //Register card listener
    if (!pa.cardManager)
    {
        PA_ERROR("cardManager manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    // Register sub listener for each slot it
    if (cardListenerRegistered)
    {
        PA_INFO("card listener already registered.");
    }
    else
    {
        if (pa.cardManager)
        {
            pa.tafCardListener = std::make_shared<tafPaCardListener>();
            pa.cardListener = pa.tafCardListener;
            if (pa.cardManager-> registerListener(pa.cardListener) ==
                                        telux::common::Status::SUCCESS)
            {
                PA_INFO("card listener registered successfully.");
                cardListenerRegistered = true;
            }
            else
            {
                PA_ERROR("Fail to register card listener.");
                return TAF_PA_SIM_RESULT_FAULT;
            }
        }
    }

    //Register multi listener
    if (!pa.multiSimMgr)
    {
        PA_ERROR("multi sim manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    // Register sub listener for each slot it
    if (multiSimListenerRegistered)
    {
        PA_INFO("multi sim listener already registered.");
    }
    else
    {
        if (pa.multiSimMgr)
        {
            pa.tafMultiSimListener = std::make_shared<tafPaMultiSimListener>();
            pa.multiSimListener = pa.tafMultiSimListener;
            if (pa.multiSimMgr-> registerListener(pa.multiSimListener) ==
                                           telux::common::Status::SUCCESS)
            {
                PA_INFO("multi sim listener registered successfully.");
                multiSimListenerRegistered = true;
            }
            else
            {
                PA_ERROR("Fail to register multi sim listener.");
                return TAF_PA_SIM_RESULT_FAULT;
            }
        }
    }

    return TAF_PA_SIM_RESULT_OK;

}

pa_result_t taf_pa_sim_DeregisterListeners
(
    void
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("taf_pa_sim_DeregisterListeners");
    std::lock_guard<std::mutex> lock(pa.listenerRegistrationMutex_);

    //deregister subscription listener
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    // Deregister subscription listener for each slot it
    if (!subListenerRegistered)
    {
        PA_INFO("Sub listeners already deregistered.");
    }
    else
    {
        if (pa.subMgr)
        {
            if (pa.subMgr->removeListener(pa.subscriptionListener) ==
                                          telux::common::Status::SUCCESS)
            {
                PA_INFO("Subscription listener deregistered successfully.");
                subListenerRegistered = false;
            }
            else
            {
                PA_ERROR("Fail to deregister serving system listener.");
                return TAF_PA_SIM_RESULT_FAULT;
            }
       }
    }

    //deregister card listener
    if (!pa.cardManager)
    {
        PA_ERROR("card manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (!cardListenerRegistered)
    {
        PA_INFO("card listener already registered.");
    }
    else
    {
        if (pa.cardManager)
        {
            if (pa.cardManager->removeListener(pa.cardListener) ==
                                            telux::common::Status::SUCCESS)
            {
                PA_INFO("card listener deregistered successfully.");
                cardListenerRegistered = false;
            }
            else
            {
                PA_ERROR("Fail to deregister serving system listener.");
                return TAF_PA_SIM_RESULT_FAULT;
            }
        }
    }

    // Deregister multi sim listener
    if (!pa.multiSimMgr)
    {
        PA_ERROR("multi manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (!multiSimListenerRegistered)
    {
        PA_INFO("multi sim listeners already deregistered.");
    }
    else
    {
        if (pa.multiSimMgr)
        {
            if (pa.multiSimMgr->deregisterListener(pa.multiSimListener) ==
                                            telux::common::Status::SUCCESS)
            {
                PA_INFO("multi listener deregistered successfully.");
                multiSimListenerRegistered = false;
            }
            else
            {
                PA_ERROR("Fail to deregister serving system listener.");
                return TAF_PA_SIM_RESULT_FAULT;
            }
        }
    }
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_GetIccid(taf_pa_sim_Id_t simId,
                                std::string& iccIdStr)
{
    if (!&iccIdStr) {
        PA_ERROR("Invalid output parameter: iccIdStr is null");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    auto& pa = PlatformAdaptor::GetInstance();
    telux::common::Status status;
    taf_pa_sim_info_t* simPtr = NULL;
    std::shared_ptr<telux::tel::ISubscription> subscription;
    PA_INFO("taf_pa_sim_GetIccid");
    simPtr = pa.GetSimContext((taf_pa_sim_Id_t)simId);
    if(simPtr == nullptr)
    {
        PA_INFO("simPtr is nullptr");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (simPtr->ICCID[0] != 0)
    {
        PA_INFO("simPtr->iccIdStr");
        iccIdStr = simPtr->ICCID;
        return TAF_PA_SIM_RESULT_OK;
    }
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(simId == TAF_PA_DEFAULT_SLOT_ID)
    {
        subscription = pa.subMgr->getSubscription(TAF_PA_DEFAULT_SLOT_ID, &status);
    }
    else
    {
        subscription = pa.subMgr->getSubscription(simId, &status);
    }

    if (!subscription)
    {
        PA_ERROR("subscription is null for slot1");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        iccIdStr = subscription->getIccId();

        // Validate ICCID
        if (iccIdStr.empty())
        {
            PA_ERROR("ICCID is empty");
            return TAF_PA_SIM_RESULT_FAULT;
        }

        // Copy ICCID to cache using std::std::snprintf
        int iccidLen = std::snprintf(simPtr->ICCID, TAF_PA_SIM_ICCID_BYTES,"%s",
                       iccIdStr.c_str());
        if (iccidLen < 0)
        {
            PA_ERROR("std::std::snprintf failed for ICCID");
            return TAF_PA_SIM_RESULT_FAULT;
        }
        if (iccidLen >= TAF_PA_SIM_ICCID_BYTES)
        {
            PA_WARN("ICCID truncated: original length %d, buffer size %d",
                    iccidLen, TAF_PA_SIM_ICCID_BYTES);
        }
        PA_DEBUG("iccIdStr: %s",iccIdStr.c_str());
    }

    PA_INFO("taf_pa_sim_GetIccid is completed successfully.");
    return TAF_PA_SIM_RESULT_OK;
}
pa_result_t taf_pa_sim_GetSubscriberPhoneNumber(taf_pa_sim_Id_t simId,
                                                std::string& phoneNumber)
{
    auto& pa = PlatformAdaptor::GetInstance();
    telux::common::Status status;
    taf_pa_sim_info_t* simPtr = NULL;
    std::shared_ptr<telux::tel::ISubscription> subscription;
    PA_INFO("taf_pa_sim_GetSubscriberPhoneNumber");
    simPtr = pa.GetSimContext((taf_pa_sim_Id_t)simId);
    if(simPtr == nullptr)
    {
        PA_INFO("simPtr is nullptr");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (simPtr->phoneNumber[0] != 0)
    {
        PA_INFO("simPtr->iccIdStr");
		phoneNumber = simPtr->phoneNumber;
        return TAF_PA_SIM_RESULT_OK;
    }
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(simId == TAF_PA_DEFAULT_SLOT_ID)
    {
        subscription = pa.subMgr->getSubscription(TAF_PA_DEFAULT_SLOT_ID, &status);
    }
    else
    {
        subscription = pa.subMgr->getSubscription(simId, &status);
    }

    if (!subscription)
    {
        PA_ERROR("subscription is null for slot1");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        phoneNumber = subscription->getPhoneNumber();

        // Validate phone number
        if (phoneNumber.empty())
        {
            PA_WARN("Phone number is empty for slot %d", simId);
            // Empty phone number might be valid (not provisioned), so continue
            simPtr->phoneNumber[0] = '\0';
            return TAF_PA_SIM_RESULT_FAULT;
        }

        // Copy phone number to cache using std::snprintf
        int phoneLen = std::snprintf(simPtr->phoneNumber, TAF_PA_SIM_PHONE_NUM_MAX_BYTES,
                           "%s", phoneNumber.c_str());

        if (phoneLen < 0)
        {
            PA_ERROR("std::snprintf failed for phone number");
            return TAF_PA_SIM_RESULT_FAULT;
        }
        if (phoneLen >= TAF_PA_SIM_PHONE_NUM_MAX_BYTES)
        {
            PA_WARN("Phone number truncated: original length %zu, buffer size %d",
                                        phoneLen, TAF_PA_SIM_PHONE_NUM_MAX_BYTES);
        }
        PA_DEBUG("phoneNumber : %s",phoneNumber.c_str());
    }

    PA_INFO("Get phone number successfully.");
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_GetImsi(taf_pa_sim_Id_t simId,
                               std::string& imsi)
{
    auto& pa = PlatformAdaptor::GetInstance();
    telux::common::Status status;
    taf_pa_sim_info_t* simPtr = NULL;
    std::shared_ptr<telux::tel::ISubscription> subscription;
    PA_INFO("taf_pa_sim_GetImsi");
    simPtr = pa.GetSimContext((taf_pa_sim_Id_t)simId);
    if(simPtr == nullptr)
    {
        PA_INFO("simPtr is nullptr");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (simPtr->IMSI[0] != 0)
    {
        PA_INFO("simPtr->IMSI");
        imsi = simPtr->IMSI;
        return TAF_PA_SIM_RESULT_OK;
    }
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(simId == TAF_PA_DEFAULT_SLOT_ID)
    {
        subscription = pa.subMgr->getSubscription(TAF_PA_DEFAULT_SLOT_ID, &status);
    }
    else
    {
        subscription = pa.subMgr->getSubscription(simId, &status);
    }

    if (!subscription)
    {
        PA_ERROR("subscription is null for slot1");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        imsi = subscription->getImsi();
        int imsiLen = std::snprintf(simPtr->IMSI, TAF_PA_SIM_IMSI_BYTES, "%s", imsi.c_str());

        // Validate IMSI
        if (imsi.empty())
        {
            PA_WARN("imsir is empty for slot %d", simId);
            // Empty phone number might be valid (not provisioned), so continue
            simPtr->IMSI[0] = '\0';
            return TAF_PA_SIM_RESULT_FAULT;
        }

        if (imsiLen < 0)
        {
            PA_ERROR("std::snprintf failed for IMSI");
            return TAF_PA_SIM_RESULT_FAULT;
        }
        if (imsiLen >= TAF_PA_SIM_IMSI_BYTES)
        {
            PA_WARN("IMSI truncated: original length %zu, buffer size %d",
                                          imsiLen, TAF_PA_SIM_IMSI_BYTES);
        }
        PA_DEBUG("imsi : %s",imsi.c_str());
    }

    PA_INFO("Get IMSI successfully.");
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_GetCarrierName(taf_pa_sim_Id_t simId,
                                    std::string& nameString)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_sim_info_t* simPtr = NULL;
    telux::common::Status status;
    std::shared_ptr<telux::tel::ISubscription> subscription;
    PA_INFO("taf_pa_sim_GetCarrierName");
    simPtr = pa.GetSimContext((taf_pa_sim_Id_t)simId);
    if(simPtr == nullptr)
    {
        PA_INFO("simPtr is nullptr");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(simId == TAF_PA_DEFAULT_SLOT_ID)
    {
        subscription = pa.subMgr->getSubscription(TAF_PA_DEFAULT_SLOT_ID, &status);
    }
    else
    {
        subscription = pa.subMgr->getSubscription(simId, &status);
    }

    if (!subscription)
    {
        PA_ERROR("subscription is null for slot1");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    else
    {
        nameString = subscription->getCarrierName();
        PA_INFO("nameString : %s",nameString.c_str());
    }

    PA_INFO("Get carrier name successfully.");
    return TAF_PA_SIM_RESULT_OK;
}

void PlatformAdaptor::InitializeSimInfo
(
    std::shared_ptr<telux::tel::ISubscription> subscription,
    taf_pa_sim_Id_t simId
)
{
    PA_INFO("InitializeSimInfo for simId: %d", (int)simId);
    taf_pa_sim_info_t* simPtr = NULL;
    simPtr = GetSimContext(simId);
    if(simPtr == NULL)
    {
        PA_INFO("simPtr is null");
        return;
    }

    if (subscription)
    {
        simPtr->simId = simId;
        int iccidLen = std::snprintf(simPtr->ICCID, TAF_PA_SIM_ICCID_BYTES, "%s",
                       subscription->getIccId().c_str());

        if (iccidLen < 0)
        {
            PA_ERROR("std::snprintf failed for ICCID");
            memset(simPtr->ICCID, 0, TAF_PA_SIM_ICCID_BYTES);
        }
        else if (iccidLen >= TAF_PA_SIM_ICCID_BYTES)
        {
            PA_WARN("ICCID truncated: original length %d, buffer size %d",
                    iccidLen, TAF_PA_SIM_ICCID_BYTES);
        }
        else
        {
            PA_DEBUG("ICCID: %s (length: %d)", simPtr->ICCID, iccidLen);
        }

        int imsiLen = std::snprintf(simPtr->IMSI, TAF_PA_SIM_IMSI_BYTES, "%s",
                      subscription->getImsi().c_str());

        if (imsiLen < 0)
        {
            PA_ERROR("std::snprintf failed for IMSI");
            memset(simPtr->IMSI, 0, TAF_PA_SIM_IMSI_BYTES);
        }
        else if (imsiLen >= TAF_PA_SIM_IMSI_BYTES)
        {
            PA_WARN("IMSI truncated: original length %d, buffer size %d",
                    imsiLen, TAF_PA_SIM_IMSI_BYTES);
        }
        else
        {
            PA_DEBUG("IMSI: %s (length: %d)", simPtr->IMSI, imsiLen);
        }

        int phoneLen = std::snprintf(simPtr->phoneNumber, TAF_PA_SIM_PHONE_NUM_MAX_BYTES,
                                "%s", subscription->getPhoneNumber().c_str());
        if (phoneLen < 0)
        {
            PA_ERROR("std::snprintf failed for phone number");
            memset(simPtr->phoneNumber, 0, TAF_PA_SIM_PHONE_NUM_MAX_BYTES);
        }
        else if (phoneLen >= TAF_PA_SIM_PHONE_NUM_MAX_BYTES)
        {
            PA_WARN("Phone number truncated: original length %d, buffer size %d",
                    phoneLen, TAF_PA_SIM_PHONE_NUM_MAX_BYTES);
        }
        else if (phoneLen == 0)
        {
            PA_INFO("Phone number is empty (not provisioned)");
        }
        else
        {
            PA_DEBUG("Phone number: %s (length: %d)", simPtr->phoneNumber, phoneLen);
        }
    }
    else
    {
        PA_INFO("Subscription is null for simId: %d - resetting info", (int)simId);
        simPtr->simId = simId;
        memset(simPtr->ICCID, 0, TAF_PA_SIM_ICCID_BYTES);
        memset(simPtr->IMSI, 0, TAF_PA_SIM_IMSI_BYTES);
        memset(simPtr->phoneNumber, 0, TAF_PA_SIM_PHONE_NUM_MAX_BYTES);
        simPtr->pinTryCount = 3;
        simPtr->pukTryCount = 10;
    }
}

taf_pa_sim_info_t* PlatformAdaptor::GetSimContext
(
    taf_pa_sim_Id_t simId
)
{
    PA_INFO("GetSimContext simId:%d",simId);
    // Handle TAF_PA_SIM_UNSPECIFIED by defaulting to slot 1
    if (simId == TAF_PA_SIM_UNSPECIFIED)
    {
        simId = TAF_PA_SIM_SLOT_ID_1;
    }

    // Return the appropriate sim context based on simId
    // simId is 1-based, array is 0-based
    if (simId > 0 && simId < TAF_PA_SIM_ID_MAX)
    {
        return &simContext.simList[simId - 1];
    }

    // Default to first slot if invalid
    PA_INFO("Invalid simId: %d, defaulting to slot 0", (int)simId);
    return &simContext.simList[0];
}

taf_pa_sim_Id_t PlatformAdaptor::GetSelectedCard(void)
{
    PA_INFO("GetSelectedCard");
    // read pa.slot under simSlotMutex_ (shared lock).
    return (taf_pa_sim_Id_t)GetCurrentSlot();
}

void PlatformAdaptor::requestsSlotsStatusResponse(
    std::map<SlotId,telux::tel::SlotStatus> slotStatus, telux::common::ErrorCode error)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& phoneFactory = tel::PhoneFactory::getInstance();
    pa.cardManager = phoneFactory.getCardManager();
    slotCount = slotStatus.size();
    PA_INFO("requestsSlotsStatusResponse: slotCount: %d", slotCount);
    telux::common::Status status;
    int activeSlots = 0;
    for(auto it = slotStatus.begin(); it != slotStatus.end(); ++it)
    {
        auto slotStatus = it->second;
        if (slotStatus.slotState == telux::tel::SlotState::ACTIVE)
        {
            activeSlots++;
        }
    }
    int activeSlotCount = 0;
    PA_INFO("cardManager->getSlotCount");
    pa.cardManager->getSlotCount(activeSlotCount);
    if(activeSlotCount == 1)
    {
        pa.isSingleActive = true;
    }
    PA_INFO("activeSlotCount: %d, isSingleActive: %d", activeSlots, (int)pa.isSingleActive);

    // Acquire write lock before modifying managers.cards
    std::unique_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);

    for(auto it = slotStatus.begin(); it != slotStatus.end(); ++it)
    {
        auto slotId = it->first;
        auto slotStatus = it->second;
        PA_INFO("Slot: %d, slotState: %d, cardState: %d, cardError: %d", slotId,
                (int) slotStatus.slotState, (int) slotStatus.cardState,
                (int) slotStatus.cardError);
        if(activeSlots == 1)
        {
            if (slotStatus.slotState == telux::tel::SlotState::ACTIVE)
            {
                // write pa.slot under simSlotMutex_ (unique lock).
                SetCurrentSlot(slotId);
            }
            if (slotStatus.cardState != telux::tel::CardState::CARDSTATE_UNKNOWN
                && slotStatus.cardState != telux::tel::CardState::CARDSTATE_ABSENT)
            {
                PA_INFO("Find card for single active in slot: %d", slotId);
                auto card = pa.cardManager->getCard(TAF_PA_DEFAULT_SLOT_ID, &status);
                pa.managers.cards[slotId] = card;
                PA_INFO("Put card as %s in cards[%d]",
                card == nullptr ? "null" : "non-null", slotId);
            }
            else
            {
                pa.managers.cards[slotId] = nullptr;
                PA_INFO("Update card as null in cards[%d]", slotId);
            }
        }
        else if((slotStatus.slotState == telux::tel::SlotState::ACTIVE)
                   && (activeSlots == 2))
        {
            PA_INFO("Find card for dual active in slot: %d", slotId);
            auto card = pa.cardManager->getCard(slotId, &status);
            pa.managers.cards.emplace(slotId, card);
        }
        else
        {
            PA_INFO("Find no card for slot: %d", slotId);
            pa.managers.cards.emplace(slotId, nullptr);
        }
    }
}

bool PlatformAdaptor::WaitForCardEvent(CardEvent cardEvent)
{
   auto& pa = PlatformAdaptor::GetInstance();
   std::unique_lock<std::mutex> lock(eventMutex);
   pa.cardEventExpected = cardEvent;
   PA_INFO("WaitForCardEvent");
   if (pa.cardRespReceived)
   {
       PA_INFO("Card response already received before wait");
       pa.cardRespReceived = false;
       return true;
   }

   auto cvStatus = eventCV.wait_for(lock, std::chrono::seconds(
                                    TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS));
   if(cvStatus == std::cv_status::timeout)
   {
      PA_INFO("Event: %d not found with in %d second(s)",  (int)cardEvent,
                                       TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS);
   }
   pa.cardEventExpected = (CardEvent)0; // reset message id to avoid further notifications
   pa.cardRespReceived = false;
   if(cvStatus != std::cv_status::timeout)
   {
     if(cardEvent == CardEvent::TAF_PA_OPEN_LOGICAL_CHANNEL
         || cardEvent == CardEvent::TAF_PA_CLOSE_LOGICAL_CHANNEL
         || cardEvent == CardEvent::TAF_PA_TRANSMIT_APDU_CHANNEL)
        {

            if(errorCode == telux::common::ErrorCode::SUCCESS)
                  return true;
        }
    }
    else
    {
      PA_INFO("Unable to get the events, so timing out");
      return false;
    }
    return false;
}

pa_result_t taf_pa_sim_GetHomeNetworkMccMnc(taf_pa_sim_Id_t simId,
                                            int* mcc,int* mnc)
{
    auto& pa = PlatformAdaptor::GetInstance();
    telux::common::Status status;
    taf_pa_sim_info_t* simPtr = NULL;
    std::shared_ptr<telux::tel::ISubscription> subscription;
    PA_INFO("taf_pa_sim_GetHomeNetworkMccMnc");
    // Validate input parameters
    if (mcc == nullptr || mnc == nullptr)
    {
        PA_ERROR("Invalid parameters: mcc or mnc pointer is null");
        return PA_BAD_PARAMETER;
    }
    simPtr = pa.GetSimContext((taf_pa_sim_Id_t)simId);
    if(simPtr == nullptr)
    {
        PA_INFO("simPtr is nullptr");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(simId == TAF_PA_DEFAULT_SLOT_ID)
    {
        subscription = pa.subMgr->getSubscription(TAF_PA_DEFAULT_SLOT_ID, &status);
    }
    else
    {
        subscription = pa.subMgr->getSubscription(simId, &status);
    }

    if (!subscription)
    {
        PA_ERROR("subscription is null for slot1");
    }
    else
    {
        *mcc = subscription->getMcc();
        *mnc = subscription->getMnc();
        PA_DEBUG("*mcc : %d",*mcc);
        PA_DEBUG("*mcc : %d",*mnc);
    }

    PA_INFO("Get carrier name successfully.");
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_GetHomeNetworkMccMncStr(taf_pa_sim_Id_t simId,
                                            std::string& mcc,std::string& mnc)
{
    auto& pa = PlatformAdaptor::GetInstance();
    telux::common::Status status;
    taf_pa_sim_info_t* simPtr = NULL;
    std::shared_ptr<telux::tel::ISubscription> subscription;
    PA_INFO("taf_pa_sim_GetHomeNetworkMccMnc");
    simPtr = pa.GetSimContext((taf_pa_sim_Id_t)simId);
    if(simPtr == nullptr)
    {
        PA_INFO("simPtr is nullptr");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (!pa.subMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(simId == TAF_PA_DEFAULT_SLOT_ID)
    {
        subscription = pa.subMgr->getSubscription(TAF_PA_DEFAULT_SLOT_ID, &status);
    }
    else
    {
        subscription = pa.subMgr->getSubscription(simId, &status);
    }

    if (!subscription)
    {
        PA_ERROR("subscription is null for slot1");
    }
    else
    {
        mcc = subscription->getMobileCountryCode();
        mnc = subscription->getMobileNetworkCode();
        PA_DEBUG("*mcc string : %s",mcc.c_str());
        PA_DEBUG("*mnc string : %s",mnc.c_str());
    }

    PA_INFO("Get MCC/MNC successfully.");
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_getSlotCount(int* count)
{
    auto& pa = PlatformAdaptor::GetInstance();
    telux::common::Status status;
    int slotCount;
    PA_INFO("taf_pa_sim_getSlotCount");
    if (!pa.multiSimMgr)
    {
        PA_ERROR("Subscription manager is not initialized.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if (telux::common::Status::SUCCESS == pa.multiSimMgr->getSlotCount(slotCount))
    {
        *count = slotCount;
        PA_INFO("getSlotCount: success, Slot Count: %d", slotCount);
        return TAF_PA_SIM_RESULT_OK;
    }
    else
    {
        PA_ERROR("GetSlotCount failed!!!");
        *count = 0;
        return TAF_PA_SIM_RESULT_FAULT;
    }
}

std::shared_ptr<telux::tel::ICard> PlatformAdaptor::GetCard
(
    int slotId
)
{
    // Acquire read lock to safely read managers.cards
    std::shared_lock<std::shared_mutex> cardsLock(cardsMutex_);
    auto it = managers.cards.find(slotId);
    PA_INFO("GetCard");
    if (it != managers.cards.end())
    {
        return it->second;
    }
    PA_INFO("Card not found for slot %d", slotId);
    return nullptr;
}

// thread-safe slot accessors.
int PlatformAdaptor::GetCurrentSlot()
{
    std::shared_lock<std::shared_mutex> lock(simSlotMutex_);
    return slot;
}

void PlatformAdaptor::SetCurrentSlot(int newSlot)
{
    std::unique_lock<std::shared_mutex> lock(simSlotMutex_);
    slot = newSlot;
}

std::shared_ptr<telux::tel::ICard> PlatformAdaptor::GetCurrentCard()
{
    return GetCard(slot);
}

pa_result_t taf_pa_sim_GetState
(
    taf_pa_sim_Id_t simId,
	taf_pa_sim_States_t* state
)
{
    PA_INFO("Input sim Id: %d", (int)simId);
    auto& pa = PlatformAdaptor::GetInstance();
    if (state == nullptr)
    {
        PA_ERROR("Invalid parameter: state pointer is null");
        return PA_BAD_PARAMETER;
    }
    if (simId >= TAF_PA_SIM_ID_MAX || simId <= 0)
    {
        PA_INFO("Invalid sim Id");
        *state = TAF_PA_SIM_STATE_UNKNOWN;
        return TAF_PA_SIM_RESULT_OK;
    }

    if (simId != TAF_PA_SIM_UNSPECIFIED)
    {
        if (simId > pa.managers.cards.size())
        {
            *state = TAF_PA_SIM_STATE_UNKNOWN;
            PA_INFO("TAF_PA_SIM_STATE_UNKNOWN");
            return TAF_PA_SIM_RESULT_OK;
        }
    }
    if(simId == TAF_PA_SIM_UNSPECIFIED)
    {
        PA_INFO("Sim Id as Unknown");
        simId = pa.GetSelectedCard();
    }

    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(simId);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    telux::tel::CardState cardState = telux::tel::CardState::CARDSTATE_UNKNOWN;
    if(card != nullptr)
    {
        card->getState(cardState);
        if(cardState == telux::tel::CardState::CARDSTATE_PRESENT)
        {
            std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
            applications = card->getApplications();
            if(applications.size() != 0)
            {
                for(auto cardApp : applications)
                {
                    if(cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
                    {
                        auto appState = cardApp->getAppState();
                        if (appState == telux::tel::AppState::APPSTATE_READY)
                        {
                            *state = TAF_PA_SIM_READY;
                            PA_INFO("TAF_PA_SIM_READY");
                            return TAF_PA_SIM_RESULT_OK;
                        }
                        else if (appState == telux::tel::AppState::APPSTATE_ILLEGAL)
                        {
                            *state = TAF_PA_SIM_ERROR;
                            PA_INFO("TAF_PA_SIM_ERROR");
                            return TAF_PA_SIM_RESULT_OK;
                        }
                    }
                }
            }
            *state = TAF_PA_SIM_PRESENT;
            PA_INFO("TAF_PA_SIM_PRESENT");
            return TAF_PA_SIM_RESULT_OK;
        }
        else if(cardState == telux::tel::CardState::CARDSTATE_ABSENT)
        {
            PA_INFO("Card State is Absent" );
            *state = TAF_PA_SIM_ABSENT;
            return TAF_PA_SIM_RESULT_OK;
        }
        else if(cardState == telux::tel::CardState::CARDSTATE_ERROR)
        {
            PA_INFO("Card State is Error");
            *state = TAF_PA_SIM_ERROR;
            return TAF_PA_SIM_RESULT_OK;
        }
        else if(cardState == telux::tel::CardState::CARDSTATE_RESTRICTED)
        {
            PA_INFO("Card State is Restricted");
            *state = TAF_PA_SIM_RESTRICTED;
            return TAF_PA_SIM_RESULT_OK;
        }
        else
        {
            PA_INFO("Card State is Unknown");
            *state = TAF_PA_SIM_STATE_UNKNOWN;
            return TAF_PA_SIM_RESULT_OK;
        }
    }
    else
    {
        *state = TAF_PA_SIM_STATE_UNKNOWN;
        PA_INFO("TAF_PA_SIM_STATE_UNKNOWN (card is null)");
        return TAF_PA_SIM_RESULT_OK;
    }
}

pa_result_t taf_pa_sim_SetPower
(
    taf_pa_sim_Id_t simId,
    taf_pa_sim_power_state_t powerState
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    std::shared_lock<std::shared_mutex> lock(pa.powerMutex_);
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    PA_UNUSED(simId);
    if (card == nullptr) {
        PA_ERROR("Card not found so set power failed!");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    SlotId slotId_for_card = SlotId(card->getSlotId());

    //Sdk Callback
    auto setPowerResponseCb = [promisePtr,&pa](telux::common::ErrorCode error)
    {
        try
        {
            if(error == telux::common::ErrorCode::SUCCESS)
            {
                promisePtr->set_value(PA_OK);
                PA_INFO("promisePtr->set_value");
            }
            else
            {
                pa_result_t res = pa.MapErrorCode(error);
                PA_INFO("res: %d", (pa_result_t) res);
                promisePtr->set_value(res);
            }
        }
        catch (const std::future_error& e)
        {
            PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in callback.");
        }
    };

    PA_INFO("taf_pa_sim_SetPower powerState: %d",(taf_pa_sim_power_state_t)powerState);
    telux::common::Status status = telux::common::Status::FAILED;
    status = (powerState==PA_SIM_POWER_OFF)?pa.cardManager->cardPowerDown(
              slotId_for_card, setPowerResponseCb):
            pa.cardManager->cardPowerUp(slotId_for_card, setPowerResponseCb);
    if(status == telux::common::Status::SUCCESS)
    {
        auto futResult = promisePtr->get_future();
        if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
            == std::future_status::ready)
        {
            pa_result_t selfResult = futResult.get();
            PA_INFO("selfResult: %d", (pa_result_t) selfResult);
            if(selfResult == PA_OK)
            {
                PA_INFO("taf_pa_sim_SetPower is success");
                return TAF_PA_SIM_RESULT_OK;
            }
            else
            {
                PA_INFO("taf_pa_sim_SetPower is failed");;
                return TAF_PA_SIM_RESULT_FAULT;
            }
        }
        else
        {
            PA_ERROR("Timeout waiting for result..");
            return TAF_PA_SIM_RESULT_FAULT ;
        }
    }
    return TAF_PA_SIM_RESULT_FAULT ;
}

pa_result_t taf_pa_sim_IsSubsystemReady
(
    bool* isReady
)
{
    PA_INFO("taf_pa_sim_IsSubsystemReady");
    auto& pa = PlatformAdaptor::GetInstance();
    if(pa.multiSimMgr == nullptr)
    {
        return TAF_PA_SIM_RESULT_FAULT;
    }

    telux::common::ServiceStatus serviceStatus = pa.multiSimMgr->getServiceStatus();
    *isReady = (serviceStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE);

    PA_INFO("taf_pa_sim_IsSubsystemReady: %d",*isReady);
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_selectSimSlot
(
    taf_pa_sim_Id_t simId
)
{
    PA_INFO("taf_pa_sim_selectSimSlot");
    auto& pa = PlatformAdaptor::GetInstance();

    // read pa.slot under shared lock via GetCurrentSlot() instead of
    // holding a shared lock for the entire function (which was wrong for writes anyway).
    int currentSlot = pa.GetCurrentSlot();
    if (simId == TAF_PA_SIM_UNSPECIFIED || simId == currentSlot)
    {
        PA_INFO("No slot switch needed. Requested: %d, Current: %d", (int)simId, currentSlot);
        return TAF_PA_SIM_RESULT_OK;
    }

    PA_INFO("Switch slot to %d, current slot: %d", (int)simId, currentSlot);
    if (pa.isSingleActive)
    {
        auto cbPromise = std::make_shared<std::promise<telux::common::ErrorCode>>();
        pa.multiSimMgr->switchActiveSlot(SlotId((int)simId),
        [cbPromise](telux::common::ErrorCode error)
       {
            try
            {
                cbPromise->set_value(error);
            }
            catch (const std::future_error &e)
            {
                PA_ERROR("Promise already satisfied or broken: %s", e.what());
            }
        });
        try
        {
            std::future<telux::common::ErrorCode> future = cbPromise->get_future();
            if (future.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS)) ==
                std::future_status::ready)
                {
                    telux::common::ErrorCode errorStatus = future.get();
                    if (errorStatus == telux::common::ErrorCode::SUCCESS ||
                        errorStatus == telux::common::ErrorCode::NO_EFFECT)
                    {
                            PA_INFO("Select slot: %d successfully", (int)simId);
                            // write pa.slot under unique lock.
                            pa.SetCurrentSlot(simId);
                            return TAF_PA_SIM_RESULT_OK;
                    }
                    else
                    {
                        PA_ERROR("Failed to switch to slot %d. Error: %d", int(simId),
                        (int)errorStatus);
                        return TAF_PA_SIM_RESULT_FAULT;
                    }
                }
            else
            {
                PA_ERROR("Timeout waiting for slot switch response");
                return TAF_PA_SIM_RESULT_TIMEOUT;
            }
        }
        catch (const std::future_error &e)
        {
            PA_ERROR("Future error while getting result: %s", e.what());
            return TAF_PA_SIM_RESULT_FAULT;
        }
        catch (const std::exception &e)
        {
            PA_ERROR("Exception while getting future result: %s", e.what());
            return TAF_PA_SIM_RESULT_FAULT;
        }
    }
    else
    {
        // write pa.slot under unique lock.
        pa.SetCurrentSlot(simId);
        return TAF_PA_SIM_RESULT_OK;
    }
}

pa_result_t taf_pa_sim_ChangeCardPin
(
    taf_pa_sim_LockType_t lockType,
    const char* oldpinPtr,
    const char* newpinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    telux::tel::CardLockType cardLockType;
    PA_INFO("taf_pa_sim_ChangeCardPin");
    PA_UNUSED(callback);
    PA_UNUSED(context);
    if(!card)
    {
        PA_INFO( "ERROR: Unable to change card pin");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }

    if(lockType == TAF_PA_SIM_PIN1 || lockType == TAF_PA_SIM_PIN2)
    {
        cardLockType = (telux::tel::CardLockType)lockType;
    }
    else
    {
        cardLockType = telux::tel::CardLockType::PIN1;
    }

    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    applications = card->getApplications();
    if(applications.size() != 0)
    {
        for(auto cardApp : applications)
        {
            if((cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
                    && (cardApp->getAppState() == telux::tel::AppState::APPSTATE_READY))
            {
                auto ret
                   = cardApp->changeCardPassword(cardLockType,(string)oldpinPtr,
                  (string)newpinPtr,tafPaAuthenticationResponseCallback::ChangeCardPinResponseCb);
                if(ret == telux::common::Status::SUCCESS)
                {
                    PA_INFO( "Change card PIN request sent successfully\n");
                    return TAF_PA_SIM_RESULT_OK;
                }
                else
                {
                    PA_INFO( "Change card PIN request failed\n");
                    return TAF_PA_SIM_RESULT_FAULT;
                }
            }
        }
    }
    else
    {
        PA_INFO("Change card PIN request failed");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_FAULT;
}

pa_result_t taf_pa_sim_UnlockCardByPin
(
    taf_pa_sim_LockType_t lockType,
    const char* pinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    string newPin = (string) pinPtr;
    telux::tel::CardLockType cardLockType;
    PA_INFO("taf_pa_sim_UnlockCardByPin");
    PA_UNUSED(callback);
    PA_UNUSED(context);
    if(!card)
    {
        PA_ERROR( "ERROR: Unable to unlock card pin");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }

    if(lockType == TAF_PA_SIM_PIN1 || lockType == TAF_PA_SIM_PIN2)
    {
        cardLockType = (telux::tel::CardLockType)lockType;
    }
    else
    {
        cardLockType = telux::tel::CardLockType::PIN1;
    }

    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    applications = card->getApplications();
    if(applications.size() != 0)
    {
        for(auto cardApp : applications)
        {
            if(cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM
                    && cardApp->getAppState() == telux::tel::AppState::APPSTATE_PIN)
                {
                    auto ret = cardApp->unlockCardByPin(cardLockType, newPin,
                            tafPaAuthenticationResponseCallback::unlockCardByPinResponseCb);
                if(ret == telux::common::Status::SUCCESS)
                {
                    PA_INFO("Unlock card by pin request sent successfully\n");
                    return TAF_PA_SIM_RESULT_OK;
                }
                else
                {
                    PA_INFO("Unlock card by pin request failed\n");
                    return TAF_PA_SIM_RESULT_FAULT;
               }
            }
        }
    }
    else
    {
        PA_INFO("Unlock card by PIN request failed\n");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_FAULT;
}

pa_result_t taf_pa_sim_UnlockCardByPuk
(
    taf_pa_sim_LockType_t lockType,
    const char* pukPtr,
    const char* newpinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    telux::tel::CardLockType cardLockType;
    PA_INFO("taf_pa_sim_UnlockCardByPuk");
    PA_UNUSED(callback);
    PA_UNUSED(context);
    if(!card)
    {
        PA_ERROR( "ERROR: Unable to get card instance");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }
    if(lockType == TAF_PA_SIM_PUK1 || lockType == TAF_PA_SIM_PUK2)
    {
        cardLockType = (telux::tel::CardLockType)lockType;
    }
    else
    {
        cardLockType = telux::tel::CardLockType::PUK1;
    }

    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    applications = card->getApplications();
    if(applications.size() != 0)
    {
        for(auto cardApp : applications)
        {
            if(cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
            {
                if (cardApp->getAppState() == telux::tel::AppState::APPSTATE_PUK)
                {
                    auto ret = cardApp->unlockCardByPuk(cardLockType,(string) pukPtr,
                    newpinPtr,tafPaAuthenticationResponseCallback::unlockCardByPukResponseCb);
                    if(ret == telux::common::Status::SUCCESS)
                    {
                        PA_INFO("Unlock card by PUK request sent successfully\n");
                        return TAF_PA_SIM_RESULT_OK;
                    }
                    else
                    {
                        PA_INFO("Unlock card by PUK request failed\n");
                        return TAF_PA_SIM_RESULT_FAULT;
                    }
                }
                else
                {
                    PA_INFO("Unlock card by PUK request failed\n");
                    return TAF_PA_SIM_RESULT_FAULT;
                }
            }
        }
    }
    else
    {
        PA_INFO("Unlock card by PUK request failed\n");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_FAULT;
}

pa_result_t taf_pa_sim_SetCardLock
(
    taf_pa_sim_LockType_t lockType,
    const char* pinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    bool lockEnable = true; //for locking sim card
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    telux::tel::CardLockType cardLockType;
    PA_UNUSED(callback);
    PA_UNUSED(context);
    if(!card)
    {
        PA_ERROR( "ERROR: Unable to get card instance");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }
    if(lockType == TAF_PA_SIM_PIN1 || lockType == TAF_PA_SIM_FDN)
    {
        cardLockType = (telux::tel::CardLockType)lockType;
    }
    else
    {
        cardLockType = telux::tel::CardLockType::PIN1;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();

    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    applications = card->getApplications();
    if(applications.size() != 0)
    {
        for(auto cardApp : applications)
        {
            if(cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
            {
                auto ret = cardApp->setCardLock(cardLockType, pinPtr, lockEnable,
                        tafPaAuthenticationResponseCallback::setCardLockResponseCb);
                if(ret == telux::common::Status::SUCCESS)
                {
                    PA_INFO("Set card lock request sent successfully\n");
                    return TAF_PA_SIM_RESULT_OK;
                }
                else
                {
                    PA_INFO("Set card lock request failed\n");
                    return TAF_PA_SIM_RESULT_FAULT;
                }
            }
        }
    } else {
        PA_INFO("Set card lock request failed\n");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_FAULT;
}

pa_result_t taf_pa_sim_SetCardUnLock
(
    taf_pa_sim_LockType_t lockType,
    const char* pinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    bool lockEnable = false; //for unlocking sim card
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    telux::tel::CardLockType cardLockType;
    PA_INFO("taf_pa_sim_SetCardUnLock");
    PA_UNUSED(callback);
    PA_UNUSED(context);
    if(!card)
    {
        PA_ERROR( "ERROR: Unable to get card instance");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }
    if(lockType == TAF_PA_SIM_PIN1 || lockType == TAF_PA_SIM_FDN)
    {
        cardLockType = (telux::tel::CardLockType)lockType;
    }
    else
    {
        cardLockType = telux::tel::CardLockType::PIN1;
    }

    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    applications = card->getApplications();
    if(applications.size() != 0)
    {
        for(auto cardApp : applications)
        {
            if(cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
            {
                auto ret = cardApp->setCardLock(cardLockType, pinPtr, lockEnable,
                        tafPaAuthenticationResponseCallback::setCardLockResponseCb);
                if(ret == telux::common::Status::SUCCESS)
                {
                    PA_INFO("Set card unlock request sent successfully\n");
                    return TAF_PA_SIM_RESULT_OK;
                }
                else
                {
                    PA_INFO("Set card unlock request failed\n");
                    return TAF_PA_SIM_RESULT_FAULT;
                }
            }
        }
    } else {
        PA_INFO("Set card unlock request failed\n");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_FAULT;
}

pa_result_t taf_pa_sim_GetAppTypes
(
    taf_pa_sim_AppType_t* appTypePtr,
    size_t* appTypeNumElementsPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    PA_INFO("taf_pa_sim_GetAppTypes");

    if(card)
    {
        std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
        applications = card->getApplications();
        PA_INFO("Card found with given simId. num of cardApps: %d", (int) applications.size());
        int i = 0;
        for(auto cardApp : applications)
        {
            if (i < TAF_PA_SIM_MAX_APP_TYPE)
            {
                appTypePtr[i] = (taf_pa_sim_AppType_t) cardApp->getAppType();
                PA_INFO("Card Application type: %d", (int) appTypePtr[i]);
                i++;
            }
        }
        *appTypeNumElementsPtr = i;
        return TAF_PA_SIM_RESULT_OK;
    }
    else
    {
        PA_ERROR("No Card. Error to get app types!");
        return TAF_PA_SIM_RESULT_FAULT;
    }
}

pa_result_t taf_pa_sim_OpenLogicalChannel
(
    taf_pa_sim_AppType_t appType,
    uint8_t* channelPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto futResult  = promisePtr->get_future();
    auto openLogicalCb = std::make_shared<tafPaOpenLogicalChannelCallback>(promisePtr);
    std::string aid;
    PA_INFO("taf_pa_sim_OpenLogicalChannel");
    if(!card)
    {
        PA_INFO("Card not found!");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }
    PA_INFO("card found with given simId");
    applications = card->getApplications();
    for(auto cardApp : applications)
    {
        PA_INFO("Applications exist for given card");
        if(cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
        {
            aid = cardApp->getAppId();
            break;
        }
    }
    if (aid.empty())
    {
        PA_INFO("Getting app id failed");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    card->openLogicalChannel(aid, openLogicalCb);
    if(!pa.WaitForCardEvent(CardEvent::TAF_PA_OPEN_LOGICAL_CHANNEL))
    {
        PA_INFO("Opening Logical Channel failed ");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
         == std::future_status::ready)
    {
        PA_INFO("future is ready");
        pa_result_t selfResult = futResult.get();
        if(callback)
        {
            PA_INFO("callback triggered");
            callback(selfResult,context);
        }
    }
    else
    {
        PA_ERROR("Timeout waiting for result..");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    PA_INFO("Open Logical channel done channel = %d", pa.openChannel);
    *channelPtr = pa.openChannel;
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_OpenLogicalChannelByAid
(
    const char* aid,
    uint8_t* channelIdPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto futResult  = promisePtr->get_future();
    auto openLogicalCb = std::make_shared<tafPaOpenLogicalChannelCallback>(promisePtr);
    PA_INFO("taf_pa_sim_OpenLogicalChannelByAid");
    if(!card) {
        PA_INFO("Card not found!");
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }

    card->openLogicalChannel(aid, openLogicalCb);
    if(!pa.WaitForCardEvent(CardEvent::TAF_PA_OPEN_LOGICAL_CHANNEL))
    {
        PA_INFO("Opening Logical Channel by AID failed!");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
         == std::future_status::ready)
    {
        PA_INFO("future is ready");
        pa_result_t selfResult = futResult.get();
        if(callback)
        {
            PA_INFO("callback triggered");
            callback(selfResult,context);
        }
    }
    else
    {
        PA_ERROR("Timeout waiting for result..");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    PA_INFO("Open Logical channel by AID success channel = %d", pa.openChannel);
    *channelIdPtr = pa.openChannel;
    return TAF_PA_SIM_RESULT_OK;

}

pa_result_t taf_pa_sim_CloseLogicalChannel
(
    uint8_t channelId,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto futResult  = promisePtr->get_future();
    auto closeLogicalChannelCb = std::make_shared<tafPaCloseLogicalChannelCallback>(promisePtr);

    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    PA_INFO("taf_pa_sim_CloseLogicalChannel");
    if(card)
    {
        auto ret = card->closeLogicalChannel(channelId,closeLogicalChannelCb);
        if(ret != telux::common::Status::SUCCESS)
        {
            return TAF_PA_SIM_RESULT_FAULT;
        }
        if(!pa.WaitForCardEvent(CardEvent::TAF_PA_CLOSE_LOGICAL_CHANNEL))
        {
            PA_INFO("Closing Logical Channel failed ");
            return TAF_PA_SIM_RESULT_FAULT;
        }
        if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
                                                   == std::future_status::ready)
        {
            PA_INFO("future is ready");
            pa_result_t selfResult = futResult.get();
            if(callback)
            {
                PA_INFO("callback triggered");
                callback(selfResult,context);
            }
        }
        else
        {
            PA_ERROR("Timeout waiting for result..");
            return TAF_PA_SIM_RESULT_FAULT;
        }
        PA_INFO("Closing Logical Channel is success ");
        return TAF_PA_SIM_RESULT_OK;
    }
    else
    {
        return TAF_PA_SIM_RESULT_FAULT;
    }
}

pa_result_t taf_pa_sim_SendApduOnLogicalChannel
(
    uint8_t channel,
    uint8_t* responseApduPtr,
    size_t* responseApduNumElementsPtr,
    uint8_t p1, uint8_t p2, uint8_t p3,
    uint8_t cla,uint8_t instruction,
    std::vector<uint8_t> data,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto futResult  = promisePtr->get_future();
    auto tafTransmitApduCb = std::make_shared<tafPaTransmitApduResponseCallback>(promisePtr);
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    PA_INFO("taf_pa_sim_SendApduOnLogicalChannel");
    if (card == nullptr)
    {
        PA_ERROR("Card not found so SendApduOnChannel failed!");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }
    auto ret = card->transmitApduLogicalChannel(channel, cla, instruction,
                                                p1, p2, p3, data,tafTransmitApduCb);
    if (ret != Status::SUCCESS)
    {
        return TAF_PA_SIM_RESULT_FAULT;
    }

    if(!pa.WaitForCardEvent(CardEvent::TAF_PA_TRANSMIT_APDU_CHANNEL))
    {
        PA_INFO("Transmit APDU failed ");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    PA_INFO("sw1: %d, sw2: %d, payload: %s", (uint8_t)pa.apduResponse.sw1,
    (uint8_t)pa.apduResponse.sw2, pa.apduResponse.payload.c_str());

    if ((pa.apduResponse.data.size()) > (TAF_PA_SIM_RESPONSE_MAX_BYTES-2))
    {
        PA_ERROR("The size of APDU response exceeds the max length.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    size_t i = 0;
    for (i=0; i<(pa.apduResponse.data.size()); i++)
    {
        responseApduPtr[i] = pa.apduResponse.data[i];
        PA_INFO("Response APDU data = %d", responseApduPtr[i]);
    }

    responseApduPtr[i++] = (uint8_t)pa.apduResponse.sw1;
    responseApduPtr[i++] = (uint8_t)pa.apduResponse.sw2;
    *responseApduNumElementsPtr = i;
    PA_INFO("Response APDU length = %ld", (size_t)i);

    if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
         == std::future_status::ready)
    {
        PA_INFO("future is ready");
        pa_result_t selfResult = futResult.get();
        if(callback)
        {
            PA_INFO("callback triggered");
            callback(selfResult,context);
        }
    }
    else
    {
        PA_ERROR("Timeout waiting for result..");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_SendApdu
(
    uint8_t* responseApduPtr,
    size_t* responseApduNumElementsPtr,
    uint8_t p1, uint8_t p2, uint8_t p3,
    uint8_t cla, uint8_t instruction,
    std::vector<uint8_t> data,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto futResult  = promisePtr->get_future();
    auto tafTransmitApduCb = std::make_shared<tafPaTransmitApduResponseCallback>(promisePtr);
    PA_INFO("taf_pa_sim_SendApdu");
    if (card == nullptr)
    {
        PA_ERROR("Card not found so SendApdu failed!");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }

    auto ret = card->transmitApduBasicChannel(cla, instruction,
                                             p1, p2, p3, data,tafTransmitApduCb);
    if (ret != Status::SUCCESS)
    {
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(!pa.WaitForCardEvent(CardEvent::TAF_PA_TRANSMIT_APDU_CHANNEL))
    {
        PA_ERROR("Transmit APDU failed failed ");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    PA_DEBUG("sw1: %d, sw2: %d, payload: %s", (uint8_t)pa.apduResponse.sw1,
    (uint8_t)pa.apduResponse.sw2, pa.apduResponse.payload.c_str());

    if ((pa.apduResponse.data.size()) > (TAF_PA_SIM_RESPONSE_MAX_BYTES-2))
    {
        PA_ERROR("The size of APDU response exceeds the max length.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    size_t i = 0;
    for (i=0; i<(pa.apduResponse.data.size()); i++)
    {
        responseApduPtr[i] = pa.apduResponse.data[i];
        PA_INFO("Response APDU data = %d", responseApduPtr[i]);
    }

    responseApduPtr[i++] = (uint8_t)pa.apduResponse.sw1;
    responseApduPtr[i++] = (uint8_t)pa.apduResponse.sw2;
    *responseApduNumElementsPtr = i;
    PA_INFO("Response APDU length = %ld", (size_t)i);

    if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
         == std::future_status::ready)
    {
        PA_INFO("future is ready");
        pa_result_t selfResult = futResult.get();
        if(callback)
        {
            PA_INFO("callback triggered");
            callback(selfResult,context);
        }
    }
    else
    {
        PA_ERROR("Timeout waiting for result..");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_ExchangeSimIO
(
    taf_pa_sim_Command_t command,
    uint8_t *p1, uint8_t *p2,
    uint8_t *p3, const uint8_t* dataPtr,
    size_t dataNumElements,const char* pathPtr,
    uint8_t *sw1,uint8_t *sw2,
    uint8_t* responsePtr, size_t* responseNumElementsPtr,
    uint16_t field,
    taf_pa_sim_GeneralCb callback,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    std::shared_ptr<telux::tel::ICard> card;
    {
        std::shared_lock<std::shared_mutex> cardsLock(pa.cardsMutex_);
        auto it = pa.managers.cards.find(pa.slot);
        card = (it != pa.managers.cards.end()) ? it->second : nullptr;
    }
    std::string aid;
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto futResult  = promisePtr->get_future();
    PA_INFO("taf_pa_sim_ExchangeSimIO");
    if (card == nullptr)
    {
        PA_ERROR("Card not found so SendCommand failed!");
        return TAF_PA_SIM_RESULT_UNSUPPORTED;
    }
    PA_INFO("card found with given simId");
    std::vector<std::shared_ptr<telux::tel::ICardApp>> applications;
    applications = card->getApplications();
    for (auto cardApp : applications)
    {
        PA_INFO("Applications exist for given card");
        if (cardApp->getAppType() == telux::tel::AppType::APPTYPE_USIM)
        {
            PA_INFO("Application type is APPTYPE_USIM");
            aid = cardApp->getAppId();
            break;
        }
    }
    if(aid.empty())
    {
        PA_ERROR("AID is NULL");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    string filePath = std::string(pathPtr);
    std::vector<uint8_t> data(dataPtr, dataPtr+dataNumElements);
    auto tafTransmitApduCb = std::make_shared<tafPaTransmitApduResponseCallback>(promisePtr);
    auto returnStatus = card->exchangeSimIO(field,
                                            command,
                                            *p1,
                                            *p2,
                                            *p3,
                                            filePath,
                                            data,
                                            "",
                                            aid,
                                            tafTransmitApduCb);
    if(returnStatus != Status::SUCCESS)
    {
        PA_INFO("returnStatus-> TAF_PA_SIM_RESULT_FAULT");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    if(!pa.WaitForCardEvent(CardEvent::TAF_PA_TRANSMIT_APDU_CHANNEL))
    {
        PA_INFO("Command SIM IO failed");;
        return TAF_PA_SIM_RESULT_FAULT;
    }
    *sw1 = (uint8_t)pa.apduResponse.sw1;
    *sw2 = (uint8_t)pa.apduResponse.sw2;

    PA_INFO("sw1: %d, sw2: %d, payload: %s", (uint8_t)pa.apduResponse.sw1,
    (uint8_t)pa.apduResponse.sw2, pa.apduResponse.payload.c_str());

    if(futResult.wait_for(std::chrono::seconds(TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS))
         == std::future_status::ready)
    {
        PA_INFO("future is ready");
        pa_result_t selfResult = futResult.get();
        if(callback)
        {
            PA_INFO("callback triggered");
            callback(selfResult,context);
        }
    }
    else
    {
        PA_ERROR("Timeout waiting for result..");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    if ((pa.apduResponse.data.size()) > (TAF_PA_SIM_RESPONSE_MAX_BYTES-2))
    {
        PA_ERROR("The size of APDU response exceeds the max length.");
        return TAF_PA_SIM_RESULT_FAULT;
    }

    size_t i = 0;
    for (i=0; i<(pa.apduResponse.data.size()); i++)
    {
        responsePtr[i] = pa.apduResponse.data[i];
        PA_INFO("Response APDU data = %d", responsePtr[i]);
    }

    *responseNumElementsPtr = i;
    PA_INFO("Response APDU length = %ld", (size_t)i);

    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_RegisterEventListener
(
    taf_pa_sim_EventListener* eventListener,
    std::any context
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("taf_pa_sim_RegisterEventListener");
    pa_result_t result = pa.RegisterEventListener(eventListener,context);
    if (result == PA_OK)
    {
        return PA_OK;
    }
    return PA_FAULT;
}

pa_result_t taf_pa_sim_GetRemainingPINTries
(
    taf_pa_sim_Id_t simId,
    int32_t* retryCount
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("taf_pa_sim_GetRemainingPINTries");

    pa_result_t paResult = taf_pa_sim_selectSimSlot(simId);
    if (paResult != TAF_PA_SIM_RESULT_OK)
    {
        PA_ERROR("Fail to select Sim Slot via PA OSS API.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    taf_pa_sim_info_t* simPtr = NULL;
    simPtr = pa.GetSimContext(simId);
    *retryCount = simPtr->pinTryCount;
    PA_INFO("taf_pa_sim_GetRemainingPINTries *retryCount: %d",*retryCount);
    return TAF_PA_SIM_RESULT_OK;
}

pa_result_t taf_pa_sim_GetRemainingPukTries
(
    taf_pa_sim_Id_t simId,
    uint32_t*  remainingPukTries
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    PA_INFO("taf_pa_sim_GetRemainingPukTries");

    pa_result_t paResult = taf_pa_sim_selectSimSlot(simId);
    if (paResult != TAF_PA_SIM_RESULT_OK)
    {
        PA_ERROR("Fail to select Sim Slot via PA OSS API.");
        return TAF_PA_SIM_RESULT_FAULT;
    }
    taf_pa_sim_info_t* simPtr = NULL;
    simPtr = pa.GetSimContext(simId);
    *remainingPukTries = simPtr->pukTryCount;
    PA_INFO("taf_pa_sim_GetRemainingPukTries *remainingPukTries : %d",*remainingPukTries);
    return TAF_PA_SIM_RESULT_OK;
}
pa_result_t taf_pa_sim_GetEID
(
    taf_pa_sim_Id_t simId,
    std::string&  eidStr
)
{
    if (simId != TAF_PA_SIM_UNSPECIFIED &&
       (simId <= 0 || simId >= TAF_PA_SIM_ID_MAX))
    {
        PA_ERROR("Invalid simId: %d", (int)simId);
        return TAF_PA_SIM_RESULT_BAD_PARAMETER;
    }
    auto& pa = PlatformAdaptor::GetInstance();
    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto eidResultPtr = std::make_shared<std::string>();

    std::shared_ptr<telux::tel::ICard> card;
    {
        // Use cardsMutex_ (shared read lock) - same mutex used by all writers
        std::shared_lock<std::shared_mutex> lock(pa.cardsMutex_);
        int effectiveSlot = (simId == TAF_PA_SIM_UNSPECIFIED) ? pa.slot : (int)simId;
        auto it = pa.managers.cards.find(effectiveSlot);
        if (it == pa.managers.cards.end() || !it->second)
        {
            PA_ERROR("ERROR: card is not found");
            return TAF_PA_SIM_RESULT_UNSUPPORTED;
        }
        PA_INFO("effectiveSlot  :%d",effectiveSlot );
        card = it->second;
    }

    // SDK Callback
    auto callback = [promisePtr, eidResultPtr](const std::string& eid,
                     telux::common::ErrorCode errorCode) {
        try {
            if (errorCode == telux::common::ErrorCode::SUCCESS) {
                *eidResultPtr = eid;
                promisePtr->set_value(PA_OK);
            } else {
                PA_ERROR("requestEid failed with errorCode: %d", static_cast<int>(errorCode));

                // Map specific error codes
                pa_result_t result = PA_FAULT;
                switch(errorCode) {
                    case telux::common::ErrorCode::INFO_UNAVAILABLE:
                        PA_ERROR("EID information is not available");
                        result = TAF_PA_SIM_RESULT_UNSUPPORTED;
                        break;
                    case telux::common::ErrorCode::INVALID_ARGUMENTS:
                        PA_ERROR("EID information->invalid arguments");
                        result = TAF_PA_SIM_RESULT_BAD_PARAMETER;
                        break;
                    case telux::common::ErrorCode::TIMEOUT_ERROR:
                        PA_ERROR("EID information->time out error");
                        result = TAF_PA_SIM_RESULT_TIMEOUT;
                        break;
                    case telux::common::ErrorCode::REQUEST_NOT_SUPPORTED:
                        PA_ERROR("EID information->request not supported");
                        result = TAF_PA_SIM_RESULT_UNSUPPORTED;
                        break;
                    default:
                        PA_ERROR("EID information->Fault");
                        result = TAF_PA_SIM_RESULT_FAULT;
                        break;
                }
                 promisePtr->set_value(result);
            }
        }
        catch (const std::future_error& e) {
            PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e) {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...) {
            PA_ERROR("Unknown error in callback");
        }
    };

    telux::common::Status status = card->requestEid(callback);
    if (status != telux::common::Status::SUCCESS) {
        PA_ERROR("requestEid API call failed with status: %d", static_cast<int>(status));
        return TAF_PA_SIM_RESULT_FAULT;
    }

    // Wait for the callback result
    auto futResult = promisePtr->get_future();
    if (futResult.wait_for(std::chrono::seconds(REQUEST_TIMEOUT)) == std::future_status::ready) {
        pa_result_t result = futResult.get();
        if (result == PA_OK) {
            eidStr = *eidResultPtr;
            PA_DEBUG("EID retrieved successfully: %s", eidStr.c_str());
        }
        return result;
    } else {
        PA_ERROR("Timeout waiting for EID response");
        return TAF_PA_SIM_RESULT_TIMEOUT;
    }
}