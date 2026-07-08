/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafEcallPa.hpp"
#include <telux/tel/PhoneFactory.hpp>
#include "telux/common/CommonDefines.hpp"
#include <telux/platform/SubsystemFactory.hpp>
#include <telux/platform/SubsystemManager.hpp>
#include <atomic>

#define MAX_INIT_TIMEOUT 5
#define NETWORK_COMMAND_TIMEOUT 30

using namespace telux::tel;
using namespace telux::common;
using namespace tafpa::ecall;

class EcallPaController
{
public:
    static std::shared_ptr<EcallPaController> getInstance()
    {
        static std::shared_ptr<EcallPaController> instance(new EcallPaController());
        return instance;
    }

    std::shared_ptr<telux::tel::ICallManager> getCallManager(){
        return CallManager_;
    }

    std::shared_ptr<telux::platform::ISubsystemManager> getSubsystemManager(){
        return subsystemMgr_;
    }

    std::shared_ptr<telux::tel::IPhoneManager> getPhoneManager(){
        return PhoneManager_;
    }

    pa_result_t MapStatus(telux::common::Status status);
    pa_result_t MapErrorCode(telux::common::ErrorCode errorCode);
    pa_result_t ConvertMsd(ECallMsdData& msdData,const taf_pa_ecall_msd_data_t& msd);
    taf_pa_ecall_dir_t directionToPaDirection(telux::tel::CallDirection direction);
    taf_pa_ecall_termination_t convertToPaTermination(telux::tel::CallEndCause endCause);
    taf_pa_ecall_call_status_t stateToEvent(telux::tel::CallState state);
    pa_result_t initialize();
    pa_result_t deinitialize();
    pa_result_t InitializeSDKSubsystem();

    uint8_t getPhoneListSize(){
        return static_cast<uint8_t>(Phones.size());
    }

    std::shared_ptr<telux::tel::IPhone> getPhone(uint8_t index){
        return Phones[index];
    }

    pa_result_t registerListener(const taf_pa_ecall_event_listener_t* eventListener,
        std::any context)
    {
        if (eventListener != nullptr)
        {
            std::lock_guard<std::mutex> lock(listenerMutex_);
            eventListener_ = eventListener;
            contextPtr_ = context;
        }
        else
        {
            PA_ERROR("Listener is NULL");
            return PA_NOT_FOUND;
        }

        return PA_OK;
    }

    EcallPaController() = default;
    ~EcallPaController() = default;

    class tafPaECallPhoneListener : public telux::tel::IPhoneListener {
        public:
        tafPaECallPhoneListener(EcallPaController* controller) : controller_(controller){}
        void onECallOperatingModeChange(int phoneId, telux::tel::ECallModeInfo info);
        ~tafPaECallPhoneListener() = default;
        private:
        EcallPaController* controller_;
    };

    class tafPaECallModemEvtListener : public telux::platform::ISubsystemListener {
        public:
        tafPaECallModemEvtListener(EcallPaController* controller) : controller_(controller){}
        void onStateChange(telux::common::SubsystemInfo subsystemInfo,
            telux::common::OperationalStatus newOperationalStatus) override;
        ~tafPaECallModemEvtListener() = default;
        private:
        EcallPaController* controller_;
    };

    class tafPaECallListener : public telux::tel::ICallListener {
        public:
        tafPaECallListener(EcallPaController* controller) : controller_(controller){}
        void onIncomingCall(std::shared_ptr<telux::tel::ICall> call) override;
        void onCallInfoChange(std::shared_ptr<telux::tel::ICall> call) override;
        void onECallMsdTransmissionStatus(
            int phoneId, telux::tel::ECallMsdTransmissionStatus msdTransmissionStatus) override;
        #if defined(TARGET_SA515M) || defined(TARGET_SA525M)
        void onEmergencyNetworkScanFail(int phoneId) override;
        #endif
        void onECallHlapTimerEvent(int phoneId, ECallHlapTimerEvents timerEvents) override;
        void OnMsdUpdateRequest(int phoneId);
        void onECallRedial(int phoneId, ECallRedialInfo info) override;
        ~tafPaECallListener() = default;
        private:
        EcallPaController* controller_;
    };

    class CommandCallback : public telux::common::ICommandResponseCallback,
                            public std::enable_shared_from_this<CommandCallback>
    {
        public:
        CommandCallback() = default;
        ~CommandCallback() = default;
        CommandCallback(std::shared_ptr<telux::tel::ICallManager> callMngr,
            taf_pa_ecall_CommandCb callback,
            std::any ctxPtr):callMngr(callMngr),callback_(callback),ctxPtr_(ctxPtr)
        {
            return;
        }

        void commandResponse(telux::common::ErrorCode error) override
        {
                // Hold a self-reference so this object is not destroyed while its members
                // are still in use, even if the NB thread has already returned and released
                // its shared_ptr (e.g. on an early-failure path).
                auto self = shared_from_this();
                PA_INFO("Command response trigger %d",(int)error);
                auto paCtrl =  EcallPaController::getInstance();
                pa_result_t result = paCtrl->MapErrorCode(error);
                if(callback_)
                {
                    callback_(result,ctxPtr_);
                }
                // Set the promise last: the NB thread is only unblocked after we have
                // finished using all members of this object.
                try
                {
                    callProm_.set_value(error);
                }
                catch (const std::future_error &e)
                {
                    PA_ERROR("Future error while setting command promise: %s", e.what());
                }
                catch (const std::exception &e)
                {
                    PA_ERROR("Exception while setting command promise: %s", e.what());
                }
                catch (...)
                {
                    PA_ERROR("Unknown error while setting command promise");
                }
        }

        std::future<telux::common::ErrorCode> getFuture() {
            return callProm_.get_future();
        }

        protected:
        taf_pa_ecall_CommandCb callback_;
	std::promise<telux::common::ErrorCode> callProm_;
        std::shared_ptr<telux::tel::ICallManager> callMngr;
        std::any ctxPtr_;
    };

    class MakeCallCallback : public telux::tel::IMakeCallCallback,
                             public std::enable_shared_from_this<MakeCallCallback>
    {
        public:
        MakeCallCallback() = default;
        ~MakeCallCallback() = default;
        MakeCallCallback(std::shared_ptr<telux::tel::ICallManager> callMngr,
            taf_pa_ecall_MakeEcallCb callback,
            std::any ctxPtr):callMngr(callMngr),callback_(callback),ctxPtr_(ctxPtr)
        {
            return;
        }

        void makeCallResponse(telux::common::ErrorCode error,std::shared_ptr<telux::tel::ICall> icall) override
        {
                // Hold a self-reference so this object is not destroyed while its members
                // are still in use, even if the NB thread has already returned and released
                // its shared_ptr (e.g. on an early-failure path).
                auto self = shared_from_this();
                PA_INFO("Call response trigger %d",(int)error);
                auto paCtrl = EcallPaController::getInstance();
                pa_result_t result = paCtrl->MapErrorCode(error);
                std::shared_ptr<taf_pa_ecall_CallInfo_t>  callInfo =
                std::make_shared<taf_pa_ecall_CallInfo_t>();
                callInfo->phoneId = icall->getPhoneId();
                callInfo->callIndex = icall->getCallIndex();
                callInfo->callState = paCtrl->stateToEvent(icall->getCallState());
                callInfo->dir =  paCtrl->directionToPaDirection(icall->getCallDirection());
                callInfo->remotePartyNumber = icall->getRemotePartyNumber();
                callInfo->endCause = paCtrl->convertToPaTermination(icall->getCallEndCause());
                if(callback_){
                    callback_(callInfo,result,ctxPtr_);
                }
                // Set the promise last: the NB thread is only unblocked after we have
                // finished using all members of this object.
                try
                {
                    callProm_.set_value(error);
                }
                catch (const std::future_error &e)
                {
                    PA_ERROR("Future error while setting make-call promise: %s", e.what());
                }
                catch (const std::exception &e)
                {
                    PA_ERROR("Exception while setting make-call promise: %s", e.what());
                }
                catch (...)
                {
                    PA_ERROR("Unknown error while setting make-call promise");
                }
        }

        std::future<telux::common::ErrorCode> getFuture() {
            return callProm_.get_future();
        }

        protected:
        taf_pa_ecall_MakeEcallCb callback_;
        std::shared_ptr<telux::tel::ICallManager> callMngr;
	std::promise<telux::common::ErrorCode> callProm_;
        std::any ctxPtr_;
    };

private:
    EcallPaController(const EcallPaController &other) =delete;
    EcallPaController &operator=(const EcallPaController &other) =delete;
    std::vector<std::shared_ptr<telux::tel::IPhone>> Phones;
    std::shared_ptr<telux::tel::ICallManager> CallManager_;
    std::shared_ptr<telux::platform::ISubsystemManager> subsystemMgr_;
    std::shared_ptr<telux::tel::IPhoneManager> PhoneManager_;
    static std::shared_ptr<EcallPaController> instance;
    std::shared_ptr<tafPaECallListener> ecallListener_;
    std::shared_ptr<tafPaECallPhoneListener> ecallPhoneListener_;
    std::shared_ptr<tafPaECallModemEvtListener> ecallModemListener_;
    std::mutex listenerMutex_;
    const taf_pa_ecall_event_listener_t* eventListener_;
    std::any contextPtr_;
    std::atomic<bool> isInitialized_{false};
};

void EcallPaController::tafPaECallModemEvtListener::onStateChange(telux::common::SubsystemInfo
    subsystemInfo,telux::common::OperationalStatus newOperationalStatus)
{
    taf_pa_ecall_operational_status_t status = static_cast<taf_pa_ecall_operational_status_t>
        (static_cast<int>(newOperationalStatus));
    std::shared_ptr<taf_pa_ecall_subsystem_info_t> info = std::make_shared<taf_pa_ecall_subsystem_info_t>();
    info->location = static_cast<taf_pa_ecall_proc_type_t>(static_cast<int>(subsystemInfo.location));
    info->subsystems = subsystemInfo.subsystems;
    if(newOperationalStatus == telux::common::OperationalStatus::OPERATIONAL){
        auto paCtrl = EcallPaController::getInstance();
        if(paCtrl->InitializeSDKSubsystem() == PA_OK){
            PA_INFO("Subsystem intialize after reboot");
        }
    }
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener){
            listener->onStateChange(info,status,context);
        }
    }
}

void EcallPaController::tafPaECallPhoneListener::onECallOperatingModeChange(int phoneId,
    telux::tel::ECallModeInfo info)
{
    std::shared_ptr<taf_pa_ecall_mode_info_t> modeinfo  = std::make_shared<taf_pa_ecall_mode_info_t>();
    modeinfo->mode = static_cast<taf_pa_ecall_mode_t>(static_cast<int>(info.mode));
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener){
            listener->onEcallOperatingModeChange(phoneId, modeinfo, context);
        }
    }
}

void EcallPaController::tafPaECallListener::onIncomingCall(std::shared_ptr<telux::tel::ICall> icall)
{
    auto paCtrl = EcallPaController::getInstance();

    if (!icall) {
        PA_ERROR("Null icall received");
        return;
    }

    if (icall->getCallDirection() != telux::tel::CallDirection::INCOMING)
    {
        PA_ERROR("eCall is not incoming type: %d", (int)icall->getCallDirection());
        return;
    }

    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener)
        {
            std::shared_ptr<taf_pa_ecall_CallInfo_t>  callInfo =
                    std::make_shared<taf_pa_ecall_CallInfo_t>();
            callInfo->phoneId = icall->getPhoneId();
            callInfo->callIndex = icall->getCallIndex();
            callInfo->callState = paCtrl->stateToEvent(icall->getCallState());
            callInfo->dir =  paCtrl->directionToPaDirection(icall->getCallDirection());
            callInfo->remotePartyNumber = icall->getRemotePartyNumber();
            callInfo->endCause = paCtrl->convertToPaTermination(icall->getCallEndCause());
            listener->onIncomingCall(callInfo, PA_OK, context);
        }
        else
        {
            PA_ERROR("No listener is registered!, skip state");
        }
    }
}

void EcallPaController::tafPaECallListener::onCallInfoChange(
    std::shared_ptr<telux::tel::ICall> icall)
{
    auto paCtrl = EcallPaController::getInstance();
    if (!icall) {
        PA_ERROR("Null icall received");
        return;
    }
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener)
        {
            std::shared_ptr<taf_pa_ecall_CallInfo_t>  callInfo =
                    std::make_shared<taf_pa_ecall_CallInfo_t>();
            callInfo->phoneId = icall->getPhoneId();
            callInfo->callIndex = icall->getCallIndex();
            callInfo->callState = paCtrl->stateToEvent(icall->getCallState());
            callInfo->dir =  paCtrl->directionToPaDirection(icall->getCallDirection());
            callInfo->remotePartyNumber = icall->getRemotePartyNumber();
            callInfo->endCause = paCtrl->convertToPaTermination(icall->getCallEndCause());
            listener->onCallInfoChange(callInfo, PA_OK, context);
        }
        else
        {
            PA_ERROR("No listener is registered!, skip state");
        }
    }

}

void EcallPaController::tafPaECallListener::onECallMsdTransmissionStatus(
    int phoneId, telux::tel::ECallMsdTransmissionStatus msdTransmissionStatus)
{
    PA_INFO("onECallMsdTransmissionStatus response trigger with phone id %d",phoneId);
    taf_pa_ecall_msd_status_t msdStatus =
        static_cast<taf_pa_ecall_msd_status_t>(static_cast<int>(msdTransmissionStatus));
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener){
            listener->onMsdTransmissionStatus(phoneId,msdStatus,context);
        }
    }
}

#if defined(TARGET_SA515M) || defined(TARGET_SA525M)
void EcallPaController::tafPaECallListener::onEmergencyNetworkScanFail(int phoneId)
{

}
#endif

void EcallPaController::tafPaECallListener::onECallHlapTimerEvent(int phoneId,
     ECallHlapTimerEvents timerEvents){
    PA_INFO("onECallHlapTimerEvent response trigger with phone id %d",phoneId);
    std::shared_ptr<taf_pa_ecall_hlap_timer_events_t> event =
        std::make_shared<taf_pa_ecall_hlap_timer_events_t>();
    event->t2 = static_cast<taf_pa_ecall_hlap_event_t>(static_cast<int>(timerEvents.t2));
    event->t5 = static_cast<taf_pa_ecall_hlap_event_t>(static_cast<int>(timerEvents.t5));
    event->t6 = static_cast<taf_pa_ecall_hlap_event_t>(static_cast<int>(timerEvents.t6));
    event->t7 = static_cast<taf_pa_ecall_hlap_event_t>(static_cast<int>(timerEvents.t7));
    event->t9 = static_cast<taf_pa_ecall_hlap_event_t>(static_cast<int>(timerEvents.t9));
    event->t10 = static_cast<taf_pa_ecall_hlap_event_t>(static_cast<int>(timerEvents.t10));
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener){
            listener->onHlapTimerEvent(phoneId,event,context);
        }
    }
}

void EcallPaController::tafPaECallListener::OnMsdUpdateRequest(int phoneId)
{
    PA_INFO("OnMsdUpdateRequest response trigger with phone id %d",phoneId);
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener){
            listener->onMsdUpdateRequest(phoneId,context);
        }
    }
}

void EcallPaController::tafPaECallListener::onECallRedial(int phoneId,
    ECallRedialInfo info)
{
    PA_INFO("onECallRedial response trigger with phone id %d",phoneId);
    std::shared_ptr<taf_pa_ecall_redial_info_t> redialInfo =
        std::make_shared<taf_pa_ecall_redial_info_t>();
    redialInfo->willEcallRedial = info.willECallRedial;
    redialInfo->reason = static_cast<taf_pa_ecall_reason_type_t>(static_cast<int>(info.reason));
    if(controller_)
    {
        const taf_pa_ecall_event_listener_t* listener = nullptr;
        std::any context;
        {
            std::lock_guard<std::mutex> lock(controller_->listenerMutex_);
            listener = controller_->eventListener_;
            context = controller_->contextPtr_;
        }
        if(listener){
            listener->onRedial(phoneId,redialInfo,context);
        }
    }
}

pa_result_t tafpa::ecall::taf_pa_ecall_RegisterListener(
    const taf_pa_ecall_event_listener_t* eventListener,
    std::any context
)
{
    auto paCtrl = EcallPaController::getInstance();
    pa_result_t res = paCtrl->registerListener(eventListener ,context);
    if(res != PA_OK){
        PA_ERROR("unable to register listener");
    }
    return res;
}

taf_pa_ecall_dir_t EcallPaController::directionToPaDirection(
    telux::tel::CallDirection direction)
{
    switch (direction)
    {
        case telux::tel::CallDirection::INCOMING:
            return taf_pa_ecall_dir_t::INCOMING;
        case telux::tel::CallDirection::OUTGOING:
            return taf_pa_ecall_dir_t::OUTGOING;
        default:
            return taf_pa_ecall_dir_t::NONE;
    }
}

taf_pa_ecall_call_status_t EcallPaController::stateToEvent(telux::tel::CallState state)
{
    taf_pa_ecall_call_status_t event = taf_pa_ecall_call_status_t::ENDED;
    switch (state)
    {
        case telux::tel::CallState::CALL_ACTIVE:
            event = taf_pa_ecall_call_status_t::ACTIVE;
            break;

        case telux::tel::CallState::CALL_ON_HOLD:
            event = taf_pa_ecall_call_status_t::ON_HOLD;
            break;

        case telux::tel::CallState::CALL_DIALING:
            event = taf_pa_ecall_call_status_t::DIALING;
            break;

        case telux::tel::CallState::CALL_INCOMING:
            event = taf_pa_ecall_call_status_t::INCOMING;
            break;

        case telux::tel::CallState::CALL_WAITING:
            event = taf_pa_ecall_call_status_t::WAITING;
            break;

        case telux::tel::CallState::CALL_ALERTING:
            event = taf_pa_ecall_call_status_t::ALERTING;
            break;

        case telux::tel::CallState::CALL_ENDED:
            event = taf_pa_ecall_call_status_t::ENDED;
            break;

        default:
            break;
    }

    return event;
}

taf_pa_ecall_termination_t EcallPaController::convertToPaTermination(
    telux::tel::CallEndCause endCause){
    switch(endCause) {
        case telux::tel::CallEndCause::UNOBTAINABLE_NUMBER:
            return taf_pa_ecall_termination_t::UNOBTAINABLE_NUMBER;
        case telux::tel::CallEndCause::NO_ROUTE_TO_DESTINATION:
            return taf_pa_ecall_termination_t::NO_ROUTE_TO_DESTINATION;
        case telux::tel::CallEndCause::CHANNEL_UNACCEPTABLE:
            return taf_pa_ecall_termination_t::CHANNEL_UNACCEPTABLE;
        case telux::tel::CallEndCause::OPERATOR_DETERMINED_BARRING:
            return taf_pa_ecall_termination_t::OPERATOR_DETERMINED_BARRING;
        case telux::tel::CallEndCause::NORMAL:
            return taf_pa_ecall_termination_t::NORMAL;
        case telux::tel::CallEndCause::BUSY:
        case telux::tel::CallEndCause::USER_BUSY:
        case telux::tel::CallEndCause::SIP_BUSY:
            return taf_pa_ecall_termination_t::BUSY;
        case telux::tel::CallEndCause::NO_USER_RESPONDING:
            return taf_pa_ecall_termination_t::NO_USER_RESPONDING;
        case telux::tel::CallEndCause::NO_ANSWER_FROM_USER:
            return taf_pa_ecall_termination_t::NO_ANSWER_FROM_USER;
        case telux::tel::CallEndCause::CALL_REJECTED:
        case telux::tel::CallEndCause::USER_REJECT:
        case telux::tel::CallEndCause::SIP_USER_REJECTED:
        case telux::tel::CallEndCause::SIP_REQUEST_CANCELLED:
            return taf_pa_ecall_termination_t::CALL_REJECTED;
        case telux::tel::CallEndCause::NUMBER_CHANGED:
            return taf_pa_ecall_termination_t::NUMBER_CHANGED;
        case telux::tel::CallEndCause::PREEMPTION:
            return taf_pa_ecall_termination_t::PREEMPTION;
        case telux::tel::CallEndCause::DESTINATION_OUT_OF_ORDER:
            return taf_pa_ecall_termination_t::DESTINATION_OUT_OF_ORDER;
        case telux::tel::CallEndCause::INVALID_NUMBER_FORMAT:
            return taf_pa_ecall_termination_t::INVALID_NUMBER_FORMAT;
        case telux::tel::CallEndCause::FACILITY_REJECTED:
            return taf_pa_ecall_termination_t::FACILITY_REJECTED;
        case telux::tel::CallEndCause::RESP_TO_STATUS_ENQUIRY:
            return taf_pa_ecall_termination_t::RESP_TO_STATUS_ENQUIRY;
        case telux::tel::CallEndCause::NORMAL_UNSPECIFIED:
            return taf_pa_ecall_termination_t::NORMAL_UNSPECIFIED;
        case telux::tel::CallEndCause::CONGESTION:
            return taf_pa_ecall_termination_t::CONGESTION;
        case telux::tel::CallEndCause::NETWORK_OUT_OF_ORDER:
            return taf_pa_ecall_termination_t::NETWORK_OUT_OF_ORDER;
        case telux::tel::CallEndCause::TEMPORARY_FAILURE:
            return taf_pa_ecall_termination_t::TEMPORARY_FAILURE;
        case telux::tel::CallEndCause::SWITCHING_EQUIPMENT_CONGESTION:
            return taf_pa_ecall_termination_t::SWITCHING_EQUIPMENT_CONGESTION;
        case telux::tel::CallEndCause::ACCESS_INFORMATION_DISCARDED:
            return taf_pa_ecall_termination_t::ACCESS_INFORMATION_DISCARDED;
        case telux::tel::CallEndCause::REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE:
            return taf_pa_ecall_termination_t::REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE;
        case telux::tel::CallEndCause::RESOURCES_UNAVAILABLE_OR_UNSPECIFIED:
            return taf_pa_ecall_termination_t::RESOURCES_UNAVAILABLE_OR_UNSPECIFIED;
        case telux::tel::CallEndCause::QOS_UNAVAILABLE:
            return taf_pa_ecall_termination_t::QOS_UNAVAILABLE;
        case telux::tel::CallEndCause::REQUESTED_FACILITY_NOT_SUBSCRIBED:
            return taf_pa_ecall_termination_t::REQUESTED_FACILITY_NOT_SUBSCRIBED;
        case telux::tel::CallEndCause::INCOMING_CALLS_BARRED_WITHIN_CUG:
            return taf_pa_ecall_termination_t::INCOMING_CALLS_BARRED_WITHIN_CUG;
        case telux::tel::CallEndCause::BEARER_CAPABILITY_NOT_AUTHORIZED:
            return taf_pa_ecall_termination_t::BEARER_CAPABILITY_NOT_AUTHORIZED;
        case telux::tel::CallEndCause::BEARER_CAPABILITY_UNAVAILABLE:
            return taf_pa_ecall_termination_t::BEARER_CAPABILITY_UNAVAILABLE;
        case telux::tel::CallEndCause::SERVICE_OPTION_NOT_AVAILABLE:
            return taf_pa_ecall_termination_t::SERVICE_OPTION_NOT_AVAILABLE;
        case telux::tel::CallEndCause::BEARER_SERVICE_NOT_IMPLEMENTED:
            return taf_pa_ecall_termination_t::BEARER_SERVICE_NOT_IMPLEMENTED;
        case telux::tel::CallEndCause::ACM_LIMIT_EXCEEDED:
            return taf_pa_ecall_termination_t::ACM_LIMIT_EXCEEDED;
        case telux::tel::CallEndCause::REQUESTED_FACILITY_NOT_IMPLEMENTED:
            return taf_pa_ecall_termination_t::REQUESTED_FACILITY_NOT_IMPLEMENTED;
        case telux::tel::CallEndCause::ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE:
            return taf_pa_ecall_termination_t::ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE;
        case telux::tel::CallEndCause::SERVICE_OR_OPTION_NOT_IMPLEMENTED:
            return taf_pa_ecall_termination_t::SERVICE_OR_OPTION_NOT_IMPLEMENTED;
        case telux::tel::CallEndCause::INVALID_TRANSACTION_IDENTIFIER:
            return taf_pa_ecall_termination_t::INVALID_TRANSACTION_IDENTIFIER;
        case telux::tel::CallEndCause::USER_NOT_MEMBER_OF_CUG:
            return taf_pa_ecall_termination_t::USER_NOT_MEMBER_OF_CUG;
        case telux::tel::CallEndCause::INCOMPATIBLE_DESTINATION:
            return taf_pa_ecall_termination_t::INCOMPATIBLE_DESTINATION;
        case telux::tel::CallEndCause::INVALID_TRANSIT_NW_SELECTION:
            return taf_pa_ecall_termination_t::INVALID_TRANSIT_NW_SELECTION;
        case telux::tel::CallEndCause::SEMANTICALLY_INCORRECT_MESSAGE:
            return taf_pa_ecall_termination_t::SEMANTICALLY_INCORRECT_MESSAGE;
        case telux::tel::CallEndCause::INVALID_MANDATORY_INFORMATION:
            return taf_pa_ecall_termination_t::INVALID_MANDATORY_INFORMATION;
        case telux::tel::CallEndCause::MESSAGE_TYPE_NON_IMPLEMENTED:
            return taf_pa_ecall_termination_t::MESSAGE_TYPE_NON_IMPLEMENTED;
        case telux::tel::CallEndCause::MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
            return taf_pa_ecall_termination_t::MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE;
        case telux::tel::CallEndCause::INFORMATION_ELEMENT_NON_EXISTENT:
            return taf_pa_ecall_termination_t::INFORMATION_ELEMENT_NON_EXISTENT;
        case telux::tel::CallEndCause::CONDITIONAL_IE_ERROR:
            return taf_pa_ecall_termination_t::CONDITIONAL_IE_ERROR;
        case telux::tel::CallEndCause::MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
            return taf_pa_ecall_termination_t::MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE;
        case telux::tel::CallEndCause::RECOVERY_ON_TIMER_EXPIRED:
            return taf_pa_ecall_termination_t::RECOVERY_ON_TIMER_EXPIRED;
        case telux::tel::CallEndCause::PROTOCOL_ERROR_UNSPECIFIED:
            return taf_pa_ecall_termination_t::PROTOCOL_ERROR_UNSPECIFIED;
        case telux::tel::CallEndCause::INTERWORKING_UNSPECIFIED:
            return taf_pa_ecall_termination_t::INTERWORKING_UNSPECIFIED;
        case telux::tel::CallEndCause::CALL_BARRED:
            return taf_pa_ecall_termination_t::CALL_BARRED;
        case telux::tel::CallEndCause::FDN_BLOCKED:
            return taf_pa_ecall_termination_t::FDN_BLOCKED;
        case telux::tel::CallEndCause::IMSI_UNKNOWN_IN_VLR:
            return taf_pa_ecall_termination_t::IMSI_UNKNOWN_IN_VLR;
        case telux::tel::CallEndCause::IMEI_NOT_ACCEPTED:
            return taf_pa_ecall_termination_t::IMEI_NOT_ACCEPTED;
        case telux::tel::CallEndCause::DIAL_MODIFIED_TO_USSD:
            return taf_pa_ecall_termination_t::DIAL_MODIFIED_TO_USSD;
        case telux::tel::CallEndCause::DIAL_MODIFIED_TO_SS:
            return taf_pa_ecall_termination_t::DIAL_MODIFIED_TO_SS;
        case telux::tel::CallEndCause::DIAL_MODIFIED_TO_DIAL:
            return taf_pa_ecall_termination_t::DIAL_MODIFIED_TO_DIAL;
        case telux::tel::CallEndCause::RADIO_OFF:
            return taf_pa_ecall_termination_t::RADIO_OFF;
        case telux::tel::CallEndCause::OUT_OF_SERVICE:
            return taf_pa_ecall_termination_t::OUT_OF_SERVICE;
        case telux::tel::CallEndCause::NO_VALID_SIM:
            return taf_pa_ecall_termination_t::NO_VALID_SIM;
        case telux::tel::CallEndCause::RADIO_INTERNAL_ERROR:
            return taf_pa_ecall_termination_t::RADIO_INTERNAL_ERROR;
        case telux::tel::CallEndCause::NETWORK_RESP_TIMEOUT:
            return taf_pa_ecall_termination_t::NETWORK_RESP_TIMEOUT;
        case telux::tel::CallEndCause::NETWORK_REJECT:
            return taf_pa_ecall_termination_t::NETWORK_REJECT;
        case telux::tel::CallEndCause::RADIO_ACCESS_FAILURE:
            return taf_pa_ecall_termination_t::RADIO_ACCESS_FAILURE;
        case telux::tel::CallEndCause::RADIO_LINK_FAILURE:
            return taf_pa_ecall_termination_t::RADIO_LINK_FAILURE;
        case telux::tel::CallEndCause::RADIO_LINK_LOST:
            return taf_pa_ecall_termination_t::RADIO_LINK_LOST;
        case telux::tel::CallEndCause::RADIO_UPLINK_FAILURE:
            return taf_pa_ecall_termination_t::RADIO_UPLINK_FAILURE;
        case telux::tel::CallEndCause::RADIO_SETUP_FAILURE:
            return taf_pa_ecall_termination_t::RADIO_SETUP_FAILURE;
        case telux::tel::CallEndCause::RADIO_RELEASE_NORMAL:
            return taf_pa_ecall_termination_t::RADIO_RELEASE_NORMAL;
        case telux::tel::CallEndCause::RADIO_RELEASE_ABNORMAL:
            return taf_pa_ecall_termination_t::RADIO_RELEASE_ABNORMAL;
        case telux::tel::CallEndCause::ACCESS_CLASS_BLOCKED:
            return taf_pa_ecall_termination_t::ACCESS_CLASS_BLOCKED;
        case telux::tel::CallEndCause::NETWORK_DETACH:
            return taf_pa_ecall_termination_t::NETWORK_DETACH;
        case telux::tel::CallEndCause::CDMA_LOCKED_UNTIL_POWER_CYCLE:
            return taf_pa_ecall_termination_t::CDMA_LOCKED_UNTIL_POWER_CYCLE;
        case telux::tel::CallEndCause::CDMA_DROP:
            return taf_pa_ecall_termination_t::CDMA_DROP;
        case telux::tel::CallEndCause::CDMA_INTERCEPT:
            return taf_pa_ecall_termination_t::CDMA_INTERCEPT;
        case telux::tel::CallEndCause::CDMA_REORDER:
            return taf_pa_ecall_termination_t::CDMA_REORDER;
        case telux::tel::CallEndCause::CDMA_SO_REJECT:
            return taf_pa_ecall_termination_t::CDMA_SO_REJECT;
        case telux::tel::CallEndCause::CDMA_RETRY_ORDER:
            return taf_pa_ecall_termination_t::CDMA_RETRY_ORDER;
        case telux::tel::CallEndCause::CDMA_ACCESS_FAILURE:
            return taf_pa_ecall_termination_t::CDMA_ACCESS_FAILURE;
        case telux::tel::CallEndCause::CDMA_PREEMPTED:
            return taf_pa_ecall_termination_t::CDMA_PREEMPTED;
        case telux::tel::CallEndCause::CDMA_NOT_EMERGENCY:
            return taf_pa_ecall_termination_t::CDMA_NOT_EMERGENCY;
        case telux::tel::CallEndCause::CDMA_ACCESS_BLOCKED:
            return taf_pa_ecall_termination_t::CDMA_ACCESS_BLOCKED;
        case telux::tel::CallEndCause::EMERGENCY_TEMP_FAILURE:
            return taf_pa_ecall_termination_t::EMERGENCY_TEMP_FAILURE;
        case telux::tel::CallEndCause::EMERGENCY_PERM_FAILURE:
            return taf_pa_ecall_termination_t::EMERGENCY_PERM_FAILURE;
        case telux::tel::CallEndCause::HO_NOT_FEASIBLE:
            return taf_pa_ecall_termination_t::HO_NOT_FEASIBLE;
        case telux::tel::CallEndCause::LOW_BATTERY:
            return taf_pa_ecall_termination_t::LOW_BATTERY;
        case telux::tel::CallEndCause::BLACKLISTED_CALL_ID:
            return taf_pa_ecall_termination_t::BLACKLISTED_CALL_ID;
        case telux::tel::CallEndCause::CS_RETRY_REQUIRED:
            return taf_pa_ecall_termination_t::CS_RETRY_REQUIRED;
        case telux::tel::CallEndCause::NETWORK_UNAVAILABLE:
            return taf_pa_ecall_termination_t::NETWORK_UNAVAILABLE;
        case telux::tel::CallEndCause::FEATURE_UNAVAILABLE:
            return taf_pa_ecall_termination_t::FEATURE_UNAVAILABLE;
        case telux::tel::CallEndCause::SIP_ERROR:
            return taf_pa_ecall_termination_t::SIP_ERROR;
        case telux::tel::CallEndCause::MISC:
            return taf_pa_ecall_termination_t::MISC;
        case telux::tel::CallEndCause::ANSWERED_ELSEWHERE:
            return taf_pa_ecall_termination_t::ANSWERED_ELSEWHERE;
        case telux::tel::CallEndCause::PULL_OUT_OF_SYNC:
            return taf_pa_ecall_termination_t::PULL_OUT_OF_SYNC;
        case telux::tel::CallEndCause::CAUSE_CALL_PULLED:
            return taf_pa_ecall_termination_t::CAUSE_CALL_PULLED;
        case telux::tel::CallEndCause::SIP_REDIRECTED:
            return taf_pa_ecall_termination_t::SIP_REDIRECTED;
        case telux::tel::CallEndCause::SIP_BAD_REQUEST:
            return taf_pa_ecall_termination_t::SIP_BAD_REQUEST;
        case telux::tel::CallEndCause::SIP_FORBIDDEN:
            return taf_pa_ecall_termination_t::SIP_FORBIDDEN;
        case telux::tel::CallEndCause::SIP_NOT_FOUND:
            return taf_pa_ecall_termination_t::SIP_NOT_FOUND;
        case telux::tel::CallEndCause::SIP_NOT_SUPPORTED:
            return taf_pa_ecall_termination_t::SIP_NOT_SUPPORTED;
        case telux::tel::CallEndCause::SIP_REQUEST_TIMEOUT:
            return taf_pa_ecall_termination_t::SIP_REQUEST_TIMEOUT;
        case telux::tel::CallEndCause::SIP_TEMPORARILY_UNAVAILABLE:
            return taf_pa_ecall_termination_t::SIP_TEMPORARILY_UNAVAILABLE;
        case telux::tel::CallEndCause::SIP_BAD_ADDRESS:
            return taf_pa_ecall_termination_t::SIP_BAD_ADDRESS;
        case telux::tel::CallEndCause::SIP_NOT_ACCEPTABLE:
            return taf_pa_ecall_termination_t::SIP_NOT_ACCEPTABLE;
        case telux::tel::CallEndCause::SIP_NOT_REACHABLE:
            return taf_pa_ecall_termination_t::SIP_NOT_REACHABLE;
        case telux::tel::CallEndCause::SIP_SERVER_INTERNAL_ERROR:
            return taf_pa_ecall_termination_t::SIP_SERVER_INTERNAL_ERROR;
        case telux::tel::CallEndCause::SIP_SERVER_NOT_IMPLEMENTED:
            return taf_pa_ecall_termination_t::SIP_SERVER_NOT_IMPLEMENTED;
        case telux::tel::CallEndCause::SIP_SERVER_BAD_GATEWAY:
            return taf_pa_ecall_termination_t::SIP_SERVER_BAD_GATEWAY;
        case telux::tel::CallEndCause::SIP_SERVICE_UNAVAILABLE:
            return taf_pa_ecall_termination_t::SIP_SERVICE_UNAVAILABLE;
        case telux::tel::CallEndCause::SIP_SERVER_TIMEOUT:
            return taf_pa_ecall_termination_t::SIP_SERVER_TIMEOUT;
        case telux::tel::CallEndCause::SIP_SERVER_VERSION_UNSUPPORTED:
            return taf_pa_ecall_termination_t::SIP_SERVER_VERSION_UNSUPPORTED;
        case telux::tel::CallEndCause::SIP_SERVER_MESSAGE_TOOLARGE:
            return taf_pa_ecall_termination_t::SIP_SERVER_MESSAGE_TOOLARGE;
        case telux::tel::CallEndCause::SIP_SERVER_PRECONDITION_FAILURE:
            return taf_pa_ecall_termination_t::SIP_SERVER_PRECONDITION_FAILURE;
        case telux::tel::CallEndCause::SIP_GLOBAL_ERROR:
            return taf_pa_ecall_termination_t::SIP_GLOBAL_ERROR;
        case telux::tel::CallEndCause::MEDIA_INIT_FAILED:
            return taf_pa_ecall_termination_t::MEDIA_INIT_FAILED;
        case telux::tel::CallEndCause::MEDIA_NO_DATA:
            return taf_pa_ecall_termination_t::MEDIA_NO_DATA;
        case telux::tel::CallEndCause::MEDIA_NOT_ACCEPTABLE:
            return taf_pa_ecall_termination_t::MEDIA_NOT_ACCEPTABLE;
        case telux::tel::CallEndCause::MEDIA_UNSPECIFIED_ERROR:
            return taf_pa_ecall_termination_t::MEDIA_UNSPECIFIED_ERROR;
        case telux::tel::CallEndCause::HOLD_RESUME_FAILED:
            return taf_pa_ecall_termination_t::HOLD_RESUME_FAILED;
        case telux::tel::CallEndCause::HOLD_RESUME_CANCELED:
            return taf_pa_ecall_termination_t::HOLD_RESUME_CANCELED;
        case telux::tel::CallEndCause::HOLD_REINVITE_COLLISION:
            return taf_pa_ecall_termination_t::HOLD_REINVITE_COLLISION;
        case telux::tel::CallEndCause::SIP_ALTERNATE_EMERGENCY_CALL:
            return taf_pa_ecall_termination_t::SIP_ALTERNATE_EMERGENCY_CALL;
        case telux::tel::CallEndCause::NO_CSFB_IN_CS_ROAM:
            return taf_pa_ecall_termination_t::NO_CSFB_IN_CS_ROAM;
        case telux::tel::CallEndCause::SRV_NOT_REGISTERED:
            return taf_pa_ecall_termination_t::SRV_NOT_REGISTERED;
        case telux::tel::CallEndCause::CALL_TYPE_NOT_ALLOWED:
            return taf_pa_ecall_termination_t::CALL_TYPE_NOT_ALLOWED;
        case telux::tel::CallEndCause::EMRG_CALL_ONGOING:
            return taf_pa_ecall_termination_t::EMRG_CALL_ONGOING;
        case telux::tel::CallEndCause::CALL_SETUP_ONGOING:
            return taf_pa_ecall_termination_t::CALL_SETUP_ONGOING;
        case telux::tel::CallEndCause::MAX_CALL_LIMIT_REACHED:
            return taf_pa_ecall_termination_t::MAX_CALL_LIMIT_REACHED;
        case telux::tel::CallEndCause::UNSUPPORTED_SIP_HDRS:
            return taf_pa_ecall_termination_t::UNSUPPORTED_SIP_HDRS;
        case telux::tel::CallEndCause::CALL_TRANSFER_ONGOING:
            return taf_pa_ecall_termination_t::CALL_TRANSFER_ONGOING;
        case telux::tel::CallEndCause::PRACK_TIMEOUT:
            return taf_pa_ecall_termination_t::PRACK_TIMEOUT;
        case telux::tel::CallEndCause::QOS_FAILURE:
            return taf_pa_ecall_termination_t::QOS_FAILURE;
        case telux::tel::CallEndCause::ONGOING_HANDOVER:
            return taf_pa_ecall_termination_t::ONGOING_HANDOVER;
        case telux::tel::CallEndCause::VT_WITH_TTY_NOT_ALLOWED:
            return taf_pa_ecall_termination_t::VT_WITH_TTY_NOT_ALLOWED;
        case telux::tel::CallEndCause::CALL_UPGRADE_ONGOING:
            return taf_pa_ecall_termination_t::CALL_UPGRADE_ONGOING;
        case telux::tel::CallEndCause::CONFERENCE_WITH_TTY_NOT_ALLOWED:
            return taf_pa_ecall_termination_t::CONFERENCE_WITH_TTY_NOT_ALLOWED;
        case telux::tel::CallEndCause::CALL_CONFERENCE_ONGOING:
            return taf_pa_ecall_termination_t::CALL_CONFERENCE_ONGOING;
        case telux::tel::CallEndCause::VT_WITH_AVPF_NOT_ALLOWED:
            return taf_pa_ecall_termination_t::VT_WITH_AVPF_NOT_ALLOWED;
        case telux::tel::CallEndCause::ENCRYPTION_CALL_ONGOING:
            return taf_pa_ecall_termination_t::ENCRYPTION_CALL_ONGOING;
        case telux::tel::CallEndCause::CALL_ONGOING_CW_DISABLED:
            return taf_pa_ecall_termination_t::CALL_ONGOING_CW_DISABLED;
        case telux::tel::CallEndCause::CALL_ON_OTHER_SUB:
            return taf_pa_ecall_termination_t::CALL_ON_OTHER_SUB;
        case telux::tel::CallEndCause::ONE_X_COLLISION:
            return taf_pa_ecall_termination_t::ONE_X_COLLISION;
        case telux::tel::CallEndCause::UI_NOT_READY:
            return taf_pa_ecall_termination_t::UI_NOT_READY;
        case telux::tel::CallEndCause::CS_CALL_ONGOING:
            return taf_pa_ecall_termination_t::CS_CALL_ONGOING;
        case telux::tel::CallEndCause::REJECTED_ELSEWHERE:
            return taf_pa_ecall_termination_t::REJECTED_ELSEWHERE;
        case telux::tel::CallEndCause::USER_REJECTED_SESSION_MODIFICATION:
            return taf_pa_ecall_termination_t::USER_REJECTED_SESSION_MODIFICATION;
        case telux::tel::CallEndCause::USER_CANCELLED_SESSION_MODIFICATION:
            return taf_pa_ecall_termination_t::USER_CANCELLED_SESSION_MODIFICATION;
        case telux::tel::CallEndCause::SESSION_MODIFICATION_FAILED:
            return taf_pa_ecall_termination_t::SESSION_MODIFICATION_FAILED;
        case telux::tel::CallEndCause::SIP_UNAUTHORIZED:
            return taf_pa_ecall_termination_t::SIP_UNAUTHORIZED;
        case telux::tel::CallEndCause::SIP_PAYMENT_REQUIRED:
            return taf_pa_ecall_termination_t::SIP_PAYMENT_REQUIRED;
        case telux::tel::CallEndCause::SIP_METHOD_NOT_ALLOWED:
            return taf_pa_ecall_termination_t::SIP_METHOD_NOT_ALLOWED;
        case telux::tel::CallEndCause::SIP_PROXY_AUTHENTICATION_REQUIRED:
            return taf_pa_ecall_termination_t::SIP_PROXY_AUTHENTICATION_REQUIRED;
        case telux::tel::CallEndCause::SIP_REQUEST_ENTITY_TOO_LARGE:
            return taf_pa_ecall_termination_t::SIP_REQUEST_ENTITY_TOO_LARGE;
        case telux::tel::CallEndCause::SIP_REQUEST_URI_TOO_LARGE:
            return taf_pa_ecall_termination_t::SIP_REQUEST_URI_TOO_LARGE;
        case telux::tel::CallEndCause::SIP_EXTENSION_REQUIRED:
            return taf_pa_ecall_termination_t::SIP_EXTENSION_REQUIRED;
        case telux::tel::CallEndCause::SIP_INTERVAL_TOO_BRIEF:
            return taf_pa_ecall_termination_t::SIP_INTERVAL_TOO_BRIEF;
        case telux::tel::CallEndCause::SIP_CALL_OR_TRANS_DOES_NOT_EXIST:
            return taf_pa_ecall_termination_t::SIP_CALL_OR_TRANS_DOES_NOT_EXIST;
        case telux::tel::CallEndCause::SIP_LOOP_DETECTED:
            return taf_pa_ecall_termination_t::SIP_LOOP_DETECTED;
        case telux::tel::CallEndCause::SIP_TOO_MANY_HOPS:
            return taf_pa_ecall_termination_t::SIP_TOO_MANY_HOPS;
        case telux::tel::CallEndCause::SIP_AMBIGUOUS:
            return taf_pa_ecall_termination_t::SIP_AMBIGUOUS;
        case telux::tel::CallEndCause::SIP_REQUEST_PENDING:
            return taf_pa_ecall_termination_t::SIP_REQUEST_PENDING;
        case telux::tel::CallEndCause::SIP_UNDECIPHERABLE:
            return taf_pa_ecall_termination_t::SIP_UNDECIPHERABLE;
        case telux::tel::CallEndCause::RETRY_ON_IMS_WITHOUT_RTT:
            return taf_pa_ecall_termination_t::RETRY_ON_IMS_WITHOUT_RTT;
        case telux::tel::CallEndCause::MAX_PS_CALLS:
            return taf_pa_ecall_termination_t::MAX_PS_CALLS;
        case telux::tel::CallEndCause::SIP_MULTIPLE_CHOICES:
            return taf_pa_ecall_termination_t::SIP_MULTIPLE_CHOICES;
        case telux::tel::CallEndCause::SIP_MOVED_PERMANENTLY:
            return taf_pa_ecall_termination_t::SIP_MOVED_PERMANENTLY;
        case telux::tel::CallEndCause::SIP_MOVED_TEMPORARILY:
            return taf_pa_ecall_termination_t::SIP_MOVED_TEMPORARILY;
        case telux::tel::CallEndCause::SIP_USE_PROXY:
            return taf_pa_ecall_termination_t::SIP_USE_PROXY;
        case telux::tel::CallEndCause::SIP_ALTERNATE_SERVICE:
            return taf_pa_ecall_termination_t::SIP_ALTERNATE_SERVICE;
        case telux::tel::CallEndCause::SIP_UNSUPPORTED_URI_SCHEME:
            return taf_pa_ecall_termination_t::SIP_UNSUPPORTED_URI_SCHEME;
        case telux::tel::CallEndCause::SIP_REMOTE_UNSUPP_MEDIA_TYPE:
            return taf_pa_ecall_termination_t::SIP_REMOTE_UNSUPP_MEDIA_TYPE;
        case telux::tel::CallEndCause::SIP_BAD_EXTENSION:
            return taf_pa_ecall_termination_t::SIP_BAD_EXTENSION;
        case telux::tel::CallEndCause::DSDA_CONCURRENT_CALL_NOT_POSSIBLE:
            return taf_pa_ecall_termination_t::DSDA_CONCURRENT_CALL_NOT_POSSIBLE;
        case telux::tel::CallEndCause::EPSFB_FAILURE:
            return taf_pa_ecall_termination_t::EPSFB_FAILURE;
        case telux::tel::CallEndCause::TWAIT_EXPIRED:
            return taf_pa_ecall_termination_t::TWAIT_EXPIRED;
        case telux::tel::CallEndCause::TCP_CONNECTION_REQ:
            return taf_pa_ecall_termination_t::TCP_CONNECTION_REQ;
        case telux::tel::CallEndCause::THERMAL_EMERGENCY:
            return taf_pa_ecall_termination_t::THERMAL_EMERGENCY;
        case telux::tel::CallEndCause::ERROR_UNSPECIFIED:
            return taf_pa_ecall_termination_t::ERROR_UNSPECIFIED;
        default:
            return taf_pa_ecall_termination_t::ERROR_UNSPECIFIED;
    }
}

pa_result_t EcallPaController::InitializeSDKSubsystem()
{
    //  Get the PhoneFactory and PhoneManager instances.
    auto &phoneFactory = telux::tel::PhoneFactory::getInstance();
    auto prom = std::make_shared<std::promise<telux::common::ServiceStatus>>();

    CallManager_ = phoneFactory.getCallManager([prom](telux::common::ServiceStatus status)
    {
	    try{
            PA_INFO("Getting status: %d from call manager", (int)status);
            // If the status is SERVICE_UNAVAILABLE, the call manager will also update the status through initCB
            if (status != telux::common::ServiceStatus::SERVICE_UNAVAILABLE)
            {
                prom->set_value(status);
            }
        }catch (const std::future_error &e) {
            PA_ERROR("Future error in call manager callback: %s", e.what());
        } catch (const std::exception &e) {
            PA_ERROR("Exception in call manager callback: %s", e.what());
        } catch (...) {
            PA_ERROR("Unknown error in call manager callback.");
        }
   });
    if (!CallManager_)
    {
        PA_CRIT("Can't get call manager");
    }

    std::future<telux::common::ServiceStatus> initFuture = prom->get_future();
    std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    telux::common::ServiceStatus serviceStatus;
    if (std::future_status::timeout == waitStatus)
    {
        PA_CRIT ("Timeout waiting for susbsytem");
    }
    else
    {
        serviceStatus = initFuture.get();
        if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_CRIT(" *** ERROR - Unable to initialize call subsystem");
        }
    }

   auto phoneMgrProm = std::make_shared<std::promise<telux::common::ServiceStatus>>();
   PhoneManager_ = PhoneFactory::getInstance().getPhoneManager([phoneMgrProm] (telux::common::ServiceStatus status)
   {
        try{
	        PA_INFO("Getting status: %d from phone manager", (int)status);
            // If the status is SERVICE_UNAVAILABLE, the call manager will also update the status through initCB
            if (status != telux::common::ServiceStatus::SERVICE_UNAVAILABLE)
            {
                phoneMgrProm->set_value(status);
            }
        } catch (const std::future_error &e) {
            PA_ERROR("Future error in phone manager callback: %s", e.what());
        } catch (const std::exception &e) {
            PA_ERROR("Exception in phone manager callback: %s", e.what());
        } catch (...) {
            PA_ERROR("Unknown error in phone manager callback.");
        }
    });
    if (!PhoneManager_)
    {
        PA_CRIT("Can't get phone manager");
    }

    telux::common::ServiceStatus phoneMgrStatus = PhoneManager_->getServiceStatus();
    if (phoneMgrStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
        PA_INFO("telephony subsystem is not ready, wait for it to be ready");
        std::future<telux::common::ServiceStatus> initFuture = phoneMgrProm->get_future();
        std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
        if (std::future_status::timeout == waitStatus)
        {
            PA_CRIT("Timeout waiting for susbsytem");
        }
        else
        {
            serviceStatus = initFuture.get();
            if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                PA_CRIT(" *** ERROR - Unable to initialize phone subsystem");
            }
        }
    }
    //  Exit the service, if SDK is unable to initialize telephony subsystems
    std::vector<int> phoneIds;
    telux::common::Status status = PhoneManager_->getPhoneIds(phoneIds);
    if (status == telux::common::Status::SUCCESS)
    {
        for (auto index = 1; index <= (int)phoneIds.size(); index++)
        {
            auto phone = PhoneManager_->getPhone(index);
            if (phone != nullptr)
            {
                Phones.emplace_back(phone);
            }
        }
    }
    return PA_OK;
}

pa_result_t EcallPaController::initialize()
{

    auto paCtrl =  EcallPaController::getInstance();
    if(paCtrl->InitializeSDKSubsystem() == PA_OK){
        PA_INFO("phone and call manager intialize");
    }
    auto &subsystemFact = telux::platform::SubsystemFactory::getInstance();
    auto subsystemMgrprom = std::make_shared<std::promise<telux::common::ServiceStatus>>();

    subsystemMgr_ = subsystemFact.getSubsystemManager([subsystemMgrprom](telux::common::ServiceStatus srvStatus)
    {
        try{
            PA_INFO("Getting status: %d from subsystem manager", (int)srvStatus);
            subsystemMgrprom->set_value(srvStatus);
        }catch (const std::future_error &e) {
            PA_ERROR("Future error in subsystem manager callback: %s", e.what());
        } catch (const std::exception &e) {
            PA_ERROR("Exception in subsystem manager callback: %s", e.what());
        } catch (...) {
            PA_ERROR("Unknown error in subsystem manager callback.");
        }
    });

    if (!subsystemMgr_) {
        PA_ERROR("Couldn't get the subsystemMgr");
    }

    std::future<telux::common::ServiceStatus> initFuture = subsystemMgrprom->get_future();
    std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("Timeout waiting for subsystem");
    }
    else
    {
        telux::common::ServiceStatus serviceStatus = initFuture.get();
        if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            PA_ERROR("*** ERROR - Unable to initialize subsystem");
        } else {
            telux::common::SubsystemInfo subsysInfo{};
            std::vector<telux::common::SubsystemInfo> listOfSubsystems;
            ecallModemListener_ = std::make_shared<tafPaECallModemEvtListener>(this);

            subsysInfo.location = telux::common::ProcType::LOCAL_PROC;
            subsysInfo.subsystems = telux::common::Subsystem::MPSS;
            listOfSubsystems.push_back(subsysInfo);
            telux::common::ErrorCode ec = subsystemMgr_->registerListener(ecallModemListener_,
                listOfSubsystems);
            if (ec!= telux::common::ErrorCode::SUCCESS) {
                PA_ERROR("Can't register listener for modem event!\n");
            }
        }
    }

    ecallListener_ =  std::make_shared<tafPaECallListener>(this);
    Status ret = CallManager_->registerListener(ecallListener_);
    if(ret != Status::SUCCESS){
        PA_ERROR("Failed to register call listener");
        return PA_FAULT;
    }
    ecallPhoneListener_ = std::make_shared<tafPaECallPhoneListener>(this);
    ret = PhoneManager_->registerListener(ecallPhoneListener_);
    if(ret != Status::SUCCESS){
        PA_ERROR("Failed to register phone listener");
        return PA_FAULT;
    }
    isInitialized_.store(true, std::memory_order_release);
    return PA_OK;
}

pa_result_t EcallPaController::MapStatus(telux::common::Status status){
    switch (status) {
        case telux::common::Status::SUCCESS:
            PA_INFO("Operation processed successfully");
            return PA_OK;
        case telux::common::Status::FAILED:
            PA_INFO("Operation processing failed");
            return PA_FAULT;
        case telux::common::Status::INVALIDPARAM:
            PA_INFO("Input parameters are invalid");
            return PA_BAD_PARAMETER;
        case telux::common::Status::NOTALLOWED:
            PA_INFO("Operation not allowed");
            return PA_NOT_PERMITTED;
        case telux::common::Status::NOTIMPLEMENTED:
            PA_INFO("Feature not supported");
            return PA_NOT_IMPLEMENTED;
        case telux::common::Status::CONNECTIONLOST:
            PA_INFO("Connection to Socket server lost");
            return PA_COMM_ERROR;
        case telux::common::Status::EXPIRED:
            PA_INFO("Operation has expired");
            return PA_TIMEOUT;
        case telux::common::Status::NOTSUPPORTED:
            PA_INFO("Not supported on target platform");
            return PA_UNSUPPORTED;
        default:
            return PA_FAULT;
    }
}

pa_result_t EcallPaController::MapErrorCode(telux::common::ErrorCode errorCode)
{
     switch (errorCode) {
        case telux::common::ErrorCode::SUCCESS:
            PA_INFO("Operation processed successfully");
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

pa_result_t tafpa::ecall::taf_pa_ecall_SetOpMode(
    uint8_t phoneId,
    taf_pa_ecall_mode_t mode,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    int8_t listSize = paCtrl->getPhoneListSize();
    if (phoneId > listSize) {
        PA_ERROR("No phone found corresponding to phoneId: %d\n", phoneId);
        return PA_BAD_PARAMETER;

    }
    auto phonePtr = paCtrl->getPhone(phoneId-1);
    if(!phonePtr){
        PA_ERROR("Invalid phone Id. No corresponding phone found");
        return PA_BAD_PARAMETER;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [phonePtr,paCtrl,callback,context](telux::common::ErrorCode errorCode)
    {
        PA_INFO("taf_pa_ecall_SetOpMode response trigger %d",(int)errorCode);
        if(callback){
            pa_result_t res = paCtrl->MapErrorCode(errorCode);
            callback(res,context);
        }
    };
    telux::tel::ECallMode sdkMode;
    if(mode == taf_pa_ecall_mode_t::ONLY){
        sdkMode = telux::tel::ECallMode::ECALL_ONLY;
    }
    else if(mode ==  taf_pa_ecall_mode_t::NORMAL){
        sdkMode = telux::tel::ECallMode::NORMAL;
    }
    else{
        PA_ERROR("iNVALID MODE %d\n",(int)mode);
        return PA_BAD_PARAMETER;
    }
    auto ret = phonePtr->setECallOperatingMode(sdkMode,cb);
    if(ret != telux::common::Status::SUCCESS) {
        PA_ERROR("Set eCall operating mode %d request Failed in phoneId: %d\n",
            (int) sdkMode, phoneId);
    }
    return paCtrl->MapStatus(ret);
}

pa_result_t tafpa::ecall::taf_pa_ecall_GetOpMode(
    uint8_t phoneId,
    taf_pa_ecall_GetModeCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    int8_t listSize = paCtrl->getPhoneListSize();
    if (phoneId > listSize) {
        PA_ERROR("No phone found corresponding to phoneId: %d\n", phoneId);
        return PA_BAD_PARAMETER;

    }
    auto phonePtr = paCtrl->getPhone(phoneId-1);
    if(!phonePtr){
        PA_ERROR("Invalid phone Id. No corresponding phone found");
        return PA_BAD_PARAMETER;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    auto cb = [promisePtr,phonePtr,paCtrl,callback,context](telux::tel::ECallMode mode ,telux::common::ErrorCode errorCode)
    {
        if(callback)
        {
            PA_INFO("taf_pa_ecall_GetOpMode response trigger %d",(int)errorCode);
            pa_result_t res = paCtrl->MapErrorCode(errorCode);
                promisePtr->set_value(errorCode);
            if(mode == telux::tel::ECallMode::ECALL_ONLY){
                callback(taf_pa_ecall_mode_t::ONLY,res,context);
            }
            else if(mode == telux::tel::ECallMode::NORMAL){
                callback(taf_pa_ecall_mode_t::NORMAL,res,context);
            }
            else{
                callback(taf_pa_ecall_mode_t::INVALID,res,context);
            }
        }
    };

    auto ret = phonePtr->requestECallOperatingMode(cb);
    if(ret == telux::common::Status::SUCCESS && promisePtr->get_future().get() == ErrorCode::SUCCESS) {
        PA_INFO("Get eCall operating mode request Success in phoneId: %d\n",
            phoneId);
    }
    return paCtrl->MapStatus(ret);
}

pa_result_t tafpa::ecall::taf_pa_ecall_SetConfig(
    const taf_pa_ecall_config_t& config
)
{
    auto paCtrl =  EcallPaController::getInstance();
    EcallConfig eCallConfig;
    if(config.validityMask.test(taf_pa_ecall_config_type_t::NUM_TYPE)){
        eCallConfig.configValidityMask.set(EcallConfigType::ECALL_CONFIG_NUM_TYPE);
        if(config.numtype == taf_pa_ecall_num_type_t::DEFAULT){
            eCallConfig.numType =  ECallNumType::DEFAULT;
        }
        else if(config.numtype == taf_pa_ecall_num_type_t::OVERRIDDEN){
           eCallConfig.numType =  ECallNumType::OVERRIDDEN;
        }
    }
    if(config.validityMask.test(taf_pa_ecall_config_type_t::OVERRIDDEN_NUM)){
        eCallConfig.configValidityMask.set(EcallConfigType::ECALL_CONFIG_OVERRIDDEN_NUM);
        eCallConfig.overriddenNum = config.overriddenNum;
    }
    if(config.validityMask.test(taf_pa_ecall_config_type_t::T2_TIMER)){
        eCallConfig.configValidityMask.set(EcallConfigType::ECALL_CONFIG_T2_TIMER);
        eCallConfig.t2Timer = config.t2Timer;
    }
    if(config.validityMask.test(taf_pa_ecall_config_type_t::T7_TIMER)){
        eCallConfig.configValidityMask.set(EcallConfigType::ECALL_CONFIG_T7_TIMER);
        eCallConfig.t7Timer = config.t7Timer;
    }
    if(config.validityMask.test(taf_pa_ecall_config_type_t::T9_TIMER)){
        eCallConfig.configValidityMask.set(EcallConfigType::ECALL_CONFIG_T9_TIMER);
        eCallConfig.t9Timer = config.t9Timer;
    }
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    Status ret = callMngr->setECallConfig(eCallConfig);
    if(ret  != Status::SUCCESS){
        PA_ERROR("Failed to set ecall config");
    }
    return paCtrl->MapStatus(ret);
}

pa_result_t tafpa::ecall::taf_pa_ecall_GetConfig(
    taf_pa_ecall_config_t& config
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    EcallConfig eCallConfig = {};
    Status ret = callMngr->getECallConfig(eCallConfig);
    if(ret  != Status::SUCCESS){
        PA_ERROR("Failed to Get ecall config");
        return paCtrl->MapStatus(ret);
    }
    if(eCallConfig.configValidityMask.test(EcallConfigType::ECALL_CONFIG_NUM_TYPE)){
        config.validityMask.set(taf_pa_ecall_config_type_t::NUM_TYPE);
        if(eCallConfig.numType == ECallNumType::DEFAULT){
            config.numtype = taf_pa_ecall_num_type_t::DEFAULT;
        }
        else if(eCallConfig.numType ==  ECallNumType::OVERRIDDEN){
            config.numtype = taf_pa_ecall_num_type_t::OVERRIDDEN;
        }
    }
    if (eCallConfig.configValidityMask.test(EcallConfigType::ECALL_CONFIG_OVERRIDDEN_NUM)) {
        config.validityMask.set(taf_pa_ecall_config_type_t::OVERRIDDEN_NUM);
        config.overriddenNum = eCallConfig.overriddenNum;
    }
    if (eCallConfig.configValidityMask.test(EcallConfigType::ECALL_CONFIG_T2_TIMER)) {
        config.validityMask.set(taf_pa_ecall_config_type_t::T2_TIMER);
        config.t2Timer = eCallConfig.t2Timer;
    }
    if (eCallConfig.configValidityMask.test(EcallConfigType::ECALL_CONFIG_T7_TIMER)) {
        config.validityMask.set(taf_pa_ecall_config_type_t::T7_TIMER);
        config.t7Timer = eCallConfig.t7Timer;
    }
    if (eCallConfig.configValidityMask.test(EcallConfigType::ECALL_CONFIG_T9_TIMER)) {
        config.validityMask.set(taf_pa_ecall_config_type_t::T9_TIMER);
        config.t9Timer = eCallConfig.t9Timer;
    }
    return PA_OK;
}

pa_result_t tafpa::ecall::taf_pa_ecall_UpdateMsd(
    uint8_t phoneId,
    const std::vector<uint8_t>& msdData,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [callMngr,paCtrl,context,callback](telux::common::ErrorCode errorCode)
    {
        PA_INFO("taf_pa_ecall_UpdateMsd response trigger %d",(int)errorCode);
        if(callback){
            pa_result_t res = paCtrl->MapErrorCode(errorCode);
            callback(res,context);
        }
    };

    Status status = callMngr->updateECallMsd(phoneId, msdData, cb);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to update Msd");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_EncodeMsd(
    const taf_pa_ecall_msd_data_t& msdData,
    std::vector<uint8_t>& msdPdu
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    ECallMsdData ecallMsdData = {};
    pa_result_t res = paCtrl->ConvertMsd(ecallMsdData,msdData);
    if(res != PA_OK){
        PA_ERROR("Unable to convert msd data");
        return res;
    }
    telux::common::ErrorCode errorCode = callMngr->encodeECallMsd(ecallMsdData, msdPdu);
    if (errorCode != telux::common::ErrorCode::SUCCESS){
        PA_ERROR("Unable to encode msd pdu");
    }
    return paCtrl->MapErrorCode(errorCode);
}

pa_result_t EcallPaController::ConvertMsd(ECallMsdData &ecallMsdData,
    const taf_pa_ecall_msd_data_t& msdData)
{
    ecallMsdData.messageIdentifier = msdData.messageIdentifier;
    ecallMsdData.timestamp = msdData.timestamp;
    ecallMsdData.vehicleDirection = msdData.vehicleDirection;
    ecallMsdData.numberOfPassengers = msdData.numberOfPassengers;
    ecallMsdData.msdVersion = msdData.msdVersion;
    ecallMsdData.optionals.optionalDataPresent = msdData.optionalData.isMsdOptionalDataPresent;
    ecallMsdData.optionals.recentVehicleLocationN1Present = msdData.optionalData.recentVehicleLocationN1Present;
    ecallMsdData.optionals.recentVehicleLocationN2Present = msdData.optionalData.recentVehicleLocationN2Present;
    ecallMsdData.optionals.numberOfPassengersPresent = msdData.optionalData.numberOfPassengersPresent;
    switch (msdData.optionalData.optionalDataType) {
        case taf_pa_ecall_optional_data_type_t::DEFAULT:
            ecallMsdData.optionals.optionalDataType = ECallOptionalDataType::ECALL_DEFAULT;
            break;
    }
    ecallMsdData.control.automaticActivation  = msdData.control.automaticActivation;
    ecallMsdData.control.testCall = msdData.control.testCall;
    ecallMsdData.control.positionCanBeTrusted = msdData.control.positionCanBeTrusted;
    ecallMsdData.control.vehicleType =
        static_cast<ECallVehicleType>(static_cast<int>(msdData.control.vehicleType));
    ecallMsdData.vehicleIdentificationNumber.isowmi = msdData.vehicleIdentification.isowmi;
    ecallMsdData.vehicleIdentificationNumber.isovds = msdData.vehicleIdentification.isovds;
    ecallMsdData.vehicleIdentificationNumber.isovisModelyear = msdData.vehicleIdentification.isovisModelyear;
    ecallMsdData.vehicleIdentificationNumber.isovisSeqPlant = msdData.vehicleIdentification.isovisSeqPlant;
    ecallMsdData.vehicleLocation.positionLatitude  = msdData.vehicleLocation.positionLatitude;
    ecallMsdData.vehicleLocation.positionLongitude = msdData.vehicleLocation.positionLongitude;
    if (ecallMsdData.optionals.recentVehicleLocationN1Present) {
        ecallMsdData.recentVehicleLocationN1.latitudeDelta  = msdData.recentVehicleLocationN1.positionLatitude;
        ecallMsdData.recentVehicleLocationN1.longitudeDelta = msdData.recentVehicleLocationN1.positionLongitude;
    } else {
        ecallMsdData.recentVehicleLocationN1 = {};
    }
    if (ecallMsdData.optionals.recentVehicleLocationN2Present) {
        ecallMsdData.recentVehicleLocationN2.latitudeDelta  = msdData.recentVehicleLocationN2.positionLatitude;
        ecallMsdData.recentVehicleLocationN2.longitudeDelta = msdData.recentVehicleLocationN2.positionLongitude;
    } else {
        ecallMsdData.recentVehicleLocationN2 = {};
    }
    ecallMsdData.optionalPdu.oid = msdData.optionalPdu.oid;
    ecallMsdData.optionalPdu.data.clear();
    ecallMsdData.optionalPdu.data.reserve(msdData.optionalPdu.data.size());
    for (uint8_t b : msdData.optionalPdu.data) {
        ecallMsdData.optionalPdu.data.push_back(b);
    }
    ecallMsdData.optionalPdu.eCallDefaultOptions.optionalData =
        msdData.optionalPdu.eCallDefaultOptions.optionalData;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id1 = msdData.optionalPdu.eCallDefaultOptions.objId.id1;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id2 = msdData.optionalPdu.eCallDefaultOptions.objId.id2;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id3 = msdData.optionalPdu.eCallDefaultOptions.objId.id3;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id4 = msdData.optionalPdu.eCallDefaultOptions.objId.id4;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id5 = msdData.optionalPdu.eCallDefaultOptions.objId.id5;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id6 = msdData.optionalPdu.eCallDefaultOptions.objId.id6;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id7 = msdData.optionalPdu.eCallDefaultOptions.objId.id7;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id8 = msdData.optionalPdu.eCallDefaultOptions.objId.id8;
    ecallMsdData.optionalPdu.eCallDefaultOptions.objId.id9 = msdData.optionalPdu.eCallDefaultOptions.objId.id9;
    ecallMsdData.vehiclePropulsionStorage.gasolineTankPresent = msdData.propulsionType.gasolineTankPresent;
    ecallMsdData.vehiclePropulsionStorage.dieselTankPresent = msdData.propulsionType.dieselTankPresent;
    ecallMsdData.vehiclePropulsionStorage.compressedNaturalGas = msdData.propulsionType.compressedNaturalGas;
    ecallMsdData.vehiclePropulsionStorage.liquidPropaneGas = msdData.propulsionType.liquidPropaneGas;
    ecallMsdData.vehiclePropulsionStorage.electricEnergyStorage = msdData.propulsionType.electricEnergyStorage;
    ecallMsdData.vehiclePropulsionStorage.hydrogenStorage= msdData.propulsionType.hydrogenStorage;
    ecallMsdData.vehiclePropulsionStorage.otherStorage = msdData.propulsionType.otherStorage;
    return PA_OK;
}

pa_result_t tafpa::ecall::taf_pa_ecall_UpdateMsd(
    uint8_t phoneId,
    const taf_pa_ecall_msd_data_t& msdData,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    ECallMsdData ecallMsdData = {};
    pa_result_t res = paCtrl->ConvertMsd(ecallMsdData,msdData);
    if(res != PA_OK){
        PA_ERROR("Unable to convert msd data");
        return res;
    }

    // Create callback object with promise to keep it alive
    auto cbPtr = std::make_shared<EcallPaController::CommandCallback>(callMngr,callback,context);

    // Get future for synchronization
    auto future = cbPtr->getFuture();

    // Make SDK call
    Status status = callMngr->updateECallMsd(phoneId,ecallMsdData,cbPtr);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to update Msd");
        return paCtrl->MapStatus(status);
    }

    // Wait for callback to complete (cbPtr stays alive on stack)
    PA_DEBUG("Waiting for updateECallMsd callback...");
    std::chrono::seconds timeout(NETWORK_COMMAND_TIMEOUT);  // 30 seconds
    std::future_status waitStatus = future.wait_for(timeout);

    if (std::future_status::timeout == waitStatus) {
        PA_ERROR("updateECallMsd timeout after %d seconds", NETWORK_COMMAND_TIMEOUT);
        return PA_TIMEOUT;
    }

    // Get result from callback
    try {
        telux::common::ErrorCode errorCode = future.get();
        if (errorCode != telux::common::ErrorCode::SUCCESS) {
            PA_ERROR("updateECallMsd failed with error: %d", (int)errorCode);
            return paCtrl->MapErrorCode(errorCode);
        }
    }
    catch (const std::exception& e) {
        PA_ERROR("Exception getting future result: %s", e.what());
        return PA_FAULT;
    }

    PA_DEBUG("updateECallMsd completed successfully");
    return PA_OK;
}

pa_result_t tafpa::ecall::taf_pa_ecall_UpdateHlapTimer(
    int phoneId,
    taf_pa_ecall_hlap_timer_type_t type,
    uint32_t duration,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [callMngr,paCtrl,context,callback](telux::common::ErrorCode errorCode)
    {
        if(callback){
            PA_INFO("taf_pa_ecall_UpdateHlapTimer response trigger %d",(int)errorCode);
            pa_result_t res = paCtrl->MapErrorCode(errorCode);
            callback(res,context);
        }
    };

    HlapTimerType timerType = static_cast<HlapTimerType>(static_cast<int>(type));
    Status status = callMngr->updateEcallHlapTimer(phoneId,timerType,duration,cb);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to update Hlap Timer");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_RequestHlapTimerStatus(
    int phoneId,
    taf_pa_ecall_HlapTimerStatusCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    auto cb = [promisePtr,callMngr,paCtrl,callback,context](telux::common::ErrorCode error, int phoneId,
        ECallHlapTimerStatus hlapTimerStatus)
    {
        PA_INFO("taf_pa_ecall_RequestHlapTimerStatus response trigger %d",(int)error);
        pa_result_t res = paCtrl->MapErrorCode(error);
        auto hlapStatus = std::make_shared<taf_pa_ecall_hlap_timer_status_t>();
        hlapStatus->t2 = static_cast<taf_pa_ecall_hlap_timer_state_t>(static_cast<int>(hlapTimerStatus.t2));
        hlapStatus->t5 = static_cast<taf_pa_ecall_hlap_timer_state_t>(static_cast<int>(hlapTimerStatus.t5));
        hlapStatus->t6 = static_cast<taf_pa_ecall_hlap_timer_state_t>(static_cast<int>(hlapTimerStatus.t6));
        hlapStatus->t7 = static_cast<taf_pa_ecall_hlap_timer_state_t>(static_cast<int>(hlapTimerStatus.t7));
        hlapStatus->t9 = static_cast<taf_pa_ecall_hlap_timer_state_t>(static_cast<int>(hlapTimerStatus.t9));
        hlapStatus->t10 = static_cast<taf_pa_ecall_hlap_timer_state_t>(static_cast<int>(hlapTimerStatus.t10));
            promisePtr->set_value(error);
        if(callback){
            callback(res,phoneId,hlapStatus,context);
        }
    };

    Status status = callMngr->requestECallHlapTimerStatus(phoneId, cb);
    if(status == Status::SUCCESS && promisePtr->get_future().get() == ErrorCode::SUCCESS){
        PA_INFO("able to update Hlap Timer");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_RequestHlapTimer(
    int phoneId,
    taf_pa_ecall_hlap_timer_type_t type,
    taf_pa_ecall_HlapTimerCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [callMngr,paCtrl,callback,context](telux::common::ErrorCode error, uint32_t timeDuration){
        PA_INFO("taf_pa_ecall_RequestHlapTimer response trigger %d",(int)error);
        if(callback){
            pa_result_t res = paCtrl->MapErrorCode(error);
            callback(res,timeDuration,context);
        }
    };
    HlapTimerType timerType = static_cast<HlapTimerType>(static_cast<int>(type));
    Status status = callMngr->requestEcallHlapTimer(phoneId,timerType,cb);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to update Hlap Timer");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_SetEcallRedial(
    const std::vector<int>& timeGap,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [callMngr,paCtrl,callback,context](telux::common::ErrorCode error){
        PA_INFO("taf_pa_ecall_SetEcallRedial response trigger %d",(int)error);
        if(callback){
            pa_result_t res = paCtrl->MapErrorCode(error);
            callback(res,context);
        }
    };
    Status status = callMngr->configureECallRedial(RedialConfigType::CALL_ORIG,timeGap,cb);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to update Hlap Timer");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_GetEcallRedial(
    std::vector<int>& callOrigTimeGap,
    std::vector<int>& callDropTimeGap
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    telux::common::ErrorCode errorCode = callMngr->
        getECallRedialConfig(callOrigTimeGap, callDropTimeGap);
    if(errorCode != telux::common::ErrorCode::SUCCESS) {
        PA_ERROR("Unable to get ecall redail Info");
    }
    return paCtrl->MapErrorCode(errorCode);
}

pa_result_t tafpa::ecall::taf_pa_ecall_RestartHlapTimer(
    int phoneId,
    taf_pa_ecall_hlap_timer_id_t id,
    uint32_t duration,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [callMngr,paCtrl,callback,context](telux::common::ErrorCode error){
        PA_INFO("taf_pa_ecall_RestartHlapTimer response trigger %d",(int)error);
        if(callback){
            pa_result_t res = paCtrl->MapErrorCode(error);
            callback(res,context);
        }
    };
    EcallHlapTimerId timerId  =  static_cast<EcallHlapTimerId>(static_cast<int>(id));
    Status status = callMngr->restartECallHlapTimer(phoneId,timerId,duration,cb);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to restart Hlap Timer");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_MakeECall(
    int phoneId,
    const taf_pa_ecall_msd_data_t& msdData,
    taf_pa_ecall_category_t category,
    taf_pa_ecall_type_t type,
    taf_pa_ecall_MakeEcallCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    ECallMsdData ecallMsdData = {};
    pa_result_t res = paCtrl->ConvertMsd(ecallMsdData,msdData);
    if(res!=PA_OK) return PA_FAULT;
    ECallCategory categoryId = static_cast<ECallCategory>(static_cast<int>(category));
    ECallVariant eCallType = static_cast<ECallVariant>(static_cast<int>(type));
    auto cbPtr = std::make_shared<EcallPaController::MakeCallCallback>(callMngr,callback,context);
    Status ret = callMngr->makeECall(phoneId,ecallMsdData,(int)categoryId,(int)eCallType,cbPtr);
    if(ret != Status::SUCCESS ){
        PA_ERROR("Unable to make ecall");
    }
    if (ret == telux::common::Status::SUCCESS &&
            cbPtr->getFuture().get() == telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Success on make ecall");
        return PA_OK;
    }
    return paCtrl->MapStatus(ret);
}

pa_result_t tafpa::ecall::taf_pa_ecall_GetInProgressCalls
(
    std::vector<std::shared_ptr<taf_pa_ecall_CallInfo_t>>* callListPtr
)
{
    if (!callListPtr) {
        PA_ERROR("callListPtr is null");
        return PA_BAD_PARAMETER;
    }
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    auto activeCall = callMngr->getInProgressCalls();
    for (auto icall = std::begin(activeCall); icall != std::end(activeCall); icall++)
    {
        auto callInfo = std::make_shared<taf_pa_ecall_CallInfo_t>();
        callInfo->phoneId = (*icall)->getPhoneId();
        callInfo->callIndex = (*icall)->getCallIndex();
        callInfo->callState = paCtrl->stateToEvent((*icall)->getCallState());
        callInfo->dir =  paCtrl->directionToPaDirection((*icall)->getCallDirection());
        callInfo->remotePartyNumber = (*icall)->getRemotePartyNumber();
        callInfo->endCause = paCtrl->convertToPaTermination((*icall)->getCallEndCause());
        callListPtr->push_back(callInfo);
    }
    return PA_OK;
}

pa_result_t tafpa::ecall::taf_pa_ecall_RequestNetworkDeregistration(
    uint8_t phoneId,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }
    auto cb = [paCtrl,callback,context](telux::common::ErrorCode error){
        PA_INFO("taf_pa_ecall_RequestNetworkDeregistration response trigger %d",(int)error);
        if(callback){
            pa_result_t res = paCtrl->MapErrorCode(error);
            callback(res,context);
        }
    };
    Status status = callMngr->requestNetworkDeregistration(phoneId, cb);
    if(status != Status::SUCCESS){
        PA_ERROR("Unable to do NetworkDeregistration");
    }
    return paCtrl->MapStatus(status);
}

pa_result_t tafpa::ecall::taf_pa_ecall_MakeECall(
    int phoneId,
    const std::vector<uint8_t>& msdPdu,
    taf_pa_ecall_category_t category,
    taf_pa_ecall_type_t type,
    taf_pa_ecall_MakeEcallCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }

    ECallCategory categoryId = static_cast<ECallCategory>(static_cast<int>(category));
    ECallVariant eCallType = static_cast<ECallVariant>(static_cast<int>(type));
    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    auto cb = [promisePtr,callMngr,paCtrl,context,callback](telux::common::ErrorCode errorCode,
        std::shared_ptr<telux::tel::ICall> icall)
    {
        PA_INFO("taf_pa_ecall_MakeECall response trigger %d",(int)errorCode);
        pa_result_t result = paCtrl->MapErrorCode(errorCode);
        std::shared_ptr<taf_pa_ecall_CallInfo_t>  callInfo =
            std::make_shared<taf_pa_ecall_CallInfo_t>();
        callInfo->phoneId = icall->getPhoneId();
        callInfo->callIndex = icall->getCallIndex();
        callInfo->callState = paCtrl->stateToEvent(icall->getCallState());
        callInfo->dir =  paCtrl->directionToPaDirection(icall->getCallDirection());
        callInfo->remotePartyNumber = icall->getRemotePartyNumber();
        callInfo->endCause = paCtrl->convertToPaTermination(icall->getCallEndCause());
            promisePtr->set_value(errorCode);
        if(callback){
            callback(callInfo,result,context);
        }
    };

    Status ret = callMngr->makeECall(phoneId, msdPdu, (int)categoryId,
        (int)eCallType,cb);
    if (ret == telux::common::Status::SUCCESS &&
            promisePtr->get_future().get() == telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Success on make ecall");
        return PA_OK;
    }
    return paCtrl->MapStatus(ret);
}

pa_result_t tafpa::ecall::taf_pa_ecall_MakeECall(
    int phoneId,
    std::string dialNumber,
    const taf_pa_ecall_custom_sip_header_t& header,
    const std::vector<uint8_t>& msdPdu,
    taf_pa_ecall_MakeEcallCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    if(!callback){
        PA_ERROR("Callback is null");
        return PA_BAD_PARAMETER;
    }

    CustomSipHeader header_;
    if (header.contentType != ""){
        header_.contentType = header.contentType;
        PA_INFO("Set content type as %s", header.contentType.c_str());
    } else {
        header_.contentType= telux::tel::CONTENT_HEADER;
    }

    if (header.acceptInfo != "") {
        header_.acceptInfo = header.acceptInfo;
        PA_INFO("Set accept info as %s", header.acceptInfo.c_str());
    } else {
        header_.acceptInfo = "";
    }
    auto promisePtr = std::make_shared<std::promise<telux::common::ErrorCode>>();
    auto cb = [promisePtr,callMngr,paCtrl,context,callback](telux::common::ErrorCode errorCode,
        std::shared_ptr<telux::tel::ICall> icall)
    {
        PA_INFO("taf_pa_ecall_MakeECall response trigger %d",(int)errorCode);
        pa_result_t result = paCtrl->MapErrorCode(errorCode);
        std::shared_ptr<taf_pa_ecall_CallInfo_t>  callInfo =
            std::make_shared<taf_pa_ecall_CallInfo_t>();
        callInfo->phoneId = icall->getPhoneId();
        callInfo->callIndex = icall->getCallIndex();
        callInfo->callState = paCtrl->stateToEvent(icall->getCallState());
        callInfo->dir =  paCtrl->directionToPaDirection(icall->getCallDirection());
        callInfo->remotePartyNumber = icall->getRemotePartyNumber();
        callInfo->endCause = paCtrl->convertToPaTermination(icall->getCallEndCause());
            promisePtr->set_value(errorCode);
        if(callback){
            callback(callInfo,result,context);
        }
    };
    Status ret = callMngr->makeECall(phoneId, dialNumber, msdPdu, header_, cb);
    if (ret == telux::common::Status::SUCCESS &&
            promisePtr->get_future().get() == telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Success on make ecall");
        return PA_OK;
    }
    return paCtrl->MapStatus(ret);
}

pa_result_t tafpa::ecall::taf_pa_ecall_GetPhoneIdFromSlotId
(
    int8_t slotId,
    int8_t* phoneIdPtr
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto phnMngr = paCtrl->getPhoneManager();
    if(!phnMngr){
        PA_ERROR("phone Manager is Null");
        return PA_FAULT;
    }
    if (phoneIdPtr) *phoneIdPtr = phnMngr->getPhoneIdFromSlotId(slotId);
    return PA_OK;
}

pa_result_t tafpa::ecall::taf_pa_ecall_Answer(
    const taf_pa_ecall_CallInfo_t& callInfo,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    auto activeCall = callMngr->getInProgressCalls();
    if (activeCall.size() == 0)
    {
        PA_ERROR("No call is in progress");
        return PA_NOT_FOUND;
    }
    for (auto iCall = std::begin(activeCall); iCall != std::end(activeCall); iCall++)
    {
        if (callInfo.remotePartyNumber == (*iCall)->getRemotePartyNumber()&&
            ((*iCall)->getPhoneId() == callInfo.phoneId) &&
            (paCtrl->directionToPaDirection((*iCall)->getCallDirection())== callInfo.dir))
        {
            auto cbObj = std::make_shared<EcallPaController::CommandCallback>(callMngr,callback, context);

            // Get future for synchronization
            auto future = cbObj->getFuture();

            Status status = (*iCall)->answer(cbObj);
            if (status != telux::common::Status::SUCCESS) {
                PA_ERROR("Failed to answer call, status: %d", (int)status);
                return paCtrl->MapStatus(status);
            }

            // Wait for callback with timeout
            PA_DEBUG("Waiting for answer callback...");
            std::chrono::seconds timeout(NETWORK_COMMAND_TIMEOUT);
            std::future_status waitStatus = future.wait_for(timeout);

            if (std::future_status::timeout == waitStatus) {
                PA_ERROR("answer timeout after %d seconds", NETWORK_COMMAND_TIMEOUT);
                return PA_TIMEOUT;
            }

            // Get result from callback
            try {
                telux::common::ErrorCode errorCode = future.get();
                if (errorCode == telux::common::ErrorCode::SUCCESS) {
                    PA_INFO("Success on answer ecall");
                    return PA_OK;
                } else {
                    PA_ERROR("answer failed with error: %d", (int)errorCode);
                    return paCtrl->MapErrorCode(errorCode);
                }
            }
            catch (const std::exception& e) {
                PA_ERROR("Exception getting future result: %s", e.what());
                return PA_FAULT;
            }
        }
    }
    return PA_NOT_FOUND;
}

pa_result_t tafpa::ecall::taf_pa_ecall_Hangup(
    const taf_pa_ecall_CallInfo_t& callInfo,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    auto activeCall = callMngr->getInProgressCalls();
    if (activeCall.size() == 0)
    {
        PA_ERROR("No call is in progress");
        return PA_NOT_FOUND;
    }
    for (auto iCall = std::begin(activeCall); iCall != std::end(activeCall); iCall++)
    {
        if (callInfo.remotePartyNumber == (*iCall)->getRemotePartyNumber()&&
            ((*iCall)->getPhoneId() == callInfo.phoneId) &&
            (paCtrl->directionToPaDirection((*iCall)->getCallDirection())== callInfo.dir))
        {
            if ((*iCall)->getCallState() == telux::tel::CallState::CALL_ENDED)
            {
                PA_ERROR("Call is already ended");
                return PA_DUPLICATE;
            }

            auto cbObj = std::make_shared<EcallPaController::CommandCallback>(callMngr,callback, context);

            // Get future for synchronization
            auto future = cbObj->getFuture();

            Status status = (*iCall)->hangup(cbObj);
            if (status != telux::common::Status::SUCCESS) {
                PA_ERROR("Failed to hangup call, status: %d", (int)status);
                return paCtrl->MapStatus(status);
            }

            // Wait for callback with timeout
            PA_DEBUG("Waiting for hangup callback...");
            std::chrono::seconds timeout(NETWORK_COMMAND_TIMEOUT);
            std::future_status waitStatus = future.wait_for(timeout);

            if (std::future_status::timeout == waitStatus) {
                PA_ERROR("hangup timeout after %d seconds", NETWORK_COMMAND_TIMEOUT);
                return PA_TIMEOUT;
            }

            // Get result from callback
            try {
                telux::common::ErrorCode errorCode = future.get();
                if (errorCode == telux::common::ErrorCode::SUCCESS) {
                    PA_INFO("Success on hangup ecall");
                    return PA_OK;
                } else {
                    PA_ERROR("hangup failed with error: %d", (int)errorCode);
                    return paCtrl->MapErrorCode(errorCode);
                }
            }
            catch (const std::exception& e) {
                PA_ERROR("Exception getting future result: %s", e.what());
                return PA_FAULT;
            }
        }
    }
    return PA_NOT_FOUND;
}

pa_result_t tafpa::ecall::taf_pa_ecall_Reject(
    const taf_pa_ecall_CallInfo_t& callInfo,
    taf_pa_ecall_CommandCb callback,
    std::any context
)
{
    auto paCtrl =  EcallPaController::getInstance();
    auto callMngr =  paCtrl->getCallManager();
    if(!callMngr){
        PA_ERROR("Call Manager is Null");
        return PA_FAULT;
    }
    auto activeCall = callMngr->getInProgressCalls();
    if (activeCall.size() == 0)
    {
        PA_ERROR("No call is in progress");
        return PA_NOT_FOUND;
    }
    for (auto iCall = std::begin(activeCall); iCall != std::end(activeCall); iCall++)
    {
        if (callInfo.remotePartyNumber == (*iCall)->getRemotePartyNumber()&&
            ((*iCall)->getPhoneId() == callInfo.phoneId) &&
            (paCtrl->directionToPaDirection((*iCall)->getCallDirection())== callInfo.dir))
        {
            if ((*iCall)->getCallState() == telux::tel::CallState::CALL_ENDED)
            {
                PA_ERROR("Call is already ended");
                return PA_DUPLICATE;
            }

            auto cbObj = std::make_shared<EcallPaController::CommandCallback>(callMngr,callback, context);

            // Get future for synchronization
            auto future = cbObj->getFuture();

            Status status = (*iCall)->reject(cbObj);
            if (status != telux::common::Status::SUCCESS) {
                PA_ERROR("Failed to reject call, status: %d", (int)status);
                return paCtrl->MapStatus(status);
            }

            // Wait for callback with timeout
            PA_DEBUG("Waiting for reject callback...");
            std::chrono::seconds timeout(NETWORK_COMMAND_TIMEOUT);
            std::future_status waitStatus = future.wait_for(timeout);

            if (std::future_status::timeout == waitStatus) {
                PA_ERROR("reject timeout after %d seconds", NETWORK_COMMAND_TIMEOUT);
                return PA_TIMEOUT;
            }

            // Get result from callback
            try {
                telux::common::ErrorCode errorCode = future.get();
                if (errorCode == telux::common::ErrorCode::SUCCESS) {
                    PA_INFO("Success on reject ecall");
                    return PA_OK;
                } else {
                    PA_ERROR("reject failed with error: %d", (int)errorCode);
                    return paCtrl->MapErrorCode(errorCode);
                }
            }
            catch (const std::exception& e) {
                PA_ERROR("Exception getting future result: %s", e.what());
                return PA_FAULT;
            }
        }
    }
    return PA_NOT_FOUND;
}

pa_result_t EcallPaController::deinitialize()
{
    // Check if initialization was successful before proceeding with deinitialization
    if (!isInitialized_.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return PA_FAULT;
    }

    PA_INFO("Starting ECall PA deinitialization...");

    // Step 1: Deregister ecall call listener from CallManager
    if (CallManager_ && ecallListener_)
    {
        PA_INFO("Deregistering ecallListener_ from CallManager_");
        telux::common::Status status = CallManager_->removeListener(ecallListener_);
        if (status != telux::common::Status::SUCCESS)
        {
            PA_ERROR("Failed to deregister ecall call listener. Status: %d",
                     static_cast<int>(status));
        }
        ecallListener_.reset();
    }

    // Step 2: Deregister ecall phone listener from PhoneManager
    if (PhoneManager_ && ecallPhoneListener_)
    {
        PA_INFO("Deregistering ecallPhoneListener_ from PhoneManager_");
        telux::common::Status status = PhoneManager_->removeListener(ecallPhoneListener_);
        if (status != telux::common::Status::SUCCESS)
        {
            PA_ERROR("Failed to deregister ecall phone listener. Status: %d",
                     static_cast<int>(status));
        }
        ecallPhoneListener_.reset();
    }

    // Step 3: Deregister modem event listener from subsystem manager
    // Reconstruct the same subsystem info list used during registration.
    if (subsystemMgr_ && ecallModemListener_)
    {
        PA_INFO("Deregistering ecallModemListener_ from subsystemMgr_");
        telux::common::SubsystemInfo subsysInfo{};
        std::vector<telux::common::SubsystemInfo> listOfSubsystems;
        subsysInfo.location = telux::common::ProcType::LOCAL_PROC;
        subsysInfo.subsystems = telux::common::Subsystem::MPSS;
        listOfSubsystems.push_back(subsysInfo);
        telux::common::ErrorCode ec =
            subsystemMgr_->deRegisterListener(ecallModemListener_);
        if (ec != telux::common::ErrorCode::SUCCESS)
        {
            PA_ERROR("Failed to deregister ecall modem listener. ErrorCode: %d",
                     static_cast<int>(ec));
        }
        ecallModemListener_.reset();
    }

    // Step 4: Reset manager shared pointers
    PA_INFO("Resetting CallManager_, PhoneManager_, subsystemMgr_");
    CallManager_.reset();
    PhoneManager_.reset();
    subsystemMgr_.reset();

    // Step 5: Clear phone list
    PA_INFO("Clearing Phones vector");
    Phones.clear();

    // Step 6: Clear event listener pointer and context.
    // Hold listenerMutex_ so the clear is mutually exclusive with any in-flight
    // SB callback that reads eventListener_ under the same mutex.
    PA_INFO("Clearing eventListener_ and contextPtr_");
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        eventListener_ = nullptr;
        contextPtr_.reset();
    }

    PA_INFO("ECall PA deinitialization complete");
    isInitialized_.store(false, std::memory_order_release);
    return PA_OK;
}

pa_result_t tafpa::ecall::taf_pa_ecall_Init(){
    auto paCtrl =  EcallPaController::getInstance();
    pa_result_t result = paCtrl->initialize();
    if(result != PA_OK){
        PA_ERROR("Ecall pa controller initialization failed");
    }
    else{
        PA_INFO("Ecall pa controller initialization done");
    }
    return result;
}

pa_result_t tafpa::ecall::taf_pa_ecall_Deinit()
{
    auto paCtrl = EcallPaController::getInstance();
    pa_result_t result = paCtrl->deinitialize();
    if (result != PA_OK)
    {
        PA_ERROR("Ecall pa controller deinitialization failed");
    }
    else
    {
        PA_INFO("Ecall pa controller deinitialization done");
    }
    return result;
}
