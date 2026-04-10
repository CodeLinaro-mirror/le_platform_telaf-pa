/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <any>
#include <glib.h>

#include "telux/tel/PhoneFactory.hpp"
#include "telux/tel/PhoneManager.hpp"
#include "telux/common/CommonDefines.hpp"
#include "taf_pa_common.h"
#include "taf_pa_voicecall.hpp"

#define MAX_INIT_TIMEOUT 5

#define VoiceCallInfoConfFile "/tmp/.VoiceCallInfo"

using namespace tafpa::voicecall;

std::map<telux::common::ErrorCode, std::string> paErrorCodeToStringMap =
{
   {telux::common::ErrorCode::SUCCESS, "SUCCESS"},
   {telux::common::ErrorCode::RADIO_NOT_AVAILABLE, "RADIO_NOT_AVAILABLE"},
   {telux::common::ErrorCode::GENERIC_FAILURE, "GENERIC_FAILURE"},
   {telux::common::ErrorCode::PASSWORD_INCORRECT, "PASSWORD_INCORRECT"},
   {telux::common::ErrorCode::SIM_PIN2, "SIM_PIN2"},
   {telux::common::ErrorCode::SIM_PUK2, "SIM_PUK2"},
   {telux::common::ErrorCode::REQUEST_NOT_SUPPORTED, "REQUEST_NOT_SUPPORTED"},
   {telux::common::ErrorCode::CANCELLED, "CANCELLED"},
   {telux::common::ErrorCode::OP_NOT_ALLOWED_DURING_VOICE_CALL, "OP_NOT_ALLOWED_DURING_VOICE_CALL"},
   {telux::common::ErrorCode::OP_NOT_ALLOWED_BEFORE_REG_TO_NW, "OP_NOT_ALLOWED_BEFORE_REG_TO_NW"},
   {telux::common::ErrorCode::SMS_SEND_FAIL_RETRY, "SMS_SEND_FAIL_RETRY"},
   {telux::common::ErrorCode::SIM_ABSENT, "SIM_ABSENT"},
   {telux::common::ErrorCode::SUBSCRIPTION_NOT_AVAILABLE, "SUBSCRIPTION_NOT_AVAILABLE"},
   {telux::common::ErrorCode::MODE_NOT_SUPPORTED, "MODE_NOT_SUPPORTED"},
   {telux::common::ErrorCode::FDN_CHECK_FAILURE, "FDN_CHECK_FAILURE"},
   {telux::common::ErrorCode::ILLEGAL_SIM_OR_ME, "ILLEGAL_SIM_OR_ME"},
   {telux::common::ErrorCode::MISSING_RESOURCE, "MISSING_RESOURCE"},
   {telux::common::ErrorCode::NO_SUCH_ELEMENT, "NO_SUCH_ELEMENT"},
   {telux::common::ErrorCode::DIAL_MODIFIED_TO_USSD, "DIAL_MODIFIED_TO_USSD"},
   {telux::common::ErrorCode::DIAL_MODIFIED_TO_SS, "DIAL_MODIFIED_TO_SS"},
   {telux::common::ErrorCode::DIAL_MODIFIED_TO_DIAL, "DIAL_MODIFIED_TO_DIAL"},
   {telux::common::ErrorCode::USSD_MODIFIED_TO_DIAL, "USSD_MODIFIED_TO_DIAL"},
   {telux::common::ErrorCode::USSD_MODIFIED_TO_SS, "USSD_MODIFIED_TO_SS"},
   {telux::common::ErrorCode::USSD_MODIFIED_TO_USSD, "USSD_MODIFIED_TO_USSD"},
   {telux::common::ErrorCode::SS_MODIFIED_TO_DIAL, "SS_MODIFIED_TO_DIAL"},
   {telux::common::ErrorCode::SS_MODIFIED_TO_USSD, "SS_MODIFIED_TO_USSD"},
   {telux::common::ErrorCode::SUBSCRIPTION_NOT_SUPPORTED, "SUBSCRIPTION_NOT_SUPPORTED"},
   {telux::common::ErrorCode::SS_MODIFIED_TO_SS, "SS_MODIFIED_TO_SS"},
   {telux::common::ErrorCode::LCE_NOT_SUPPORTED, "LCE_NOT_SUPPORTED"},
   {telux::common::ErrorCode::NO_MEMORY, "NO_MEMORY"},
   {telux::common::ErrorCode::INTERNAL_ERR, "INTERNAL_ERR"},
   {telux::common::ErrorCode::SYSTEM_ERR, "SYSTEM_ERR"},
   {telux::common::ErrorCode::MODEM_ERR, "MODEM_ERR"},
   {telux::common::ErrorCode::INVALID_STATE, "INVALID_STATE"},
   {telux::common::ErrorCode::NO_RESOURCES, "NO_RESOURCES"},
   {telux::common::ErrorCode::SIM_ERR, "SIM_ERR"},
   {telux::common::ErrorCode::INVALID_ARGUMENTS, "INVALID_ARGUMENTS"},
   {telux::common::ErrorCode::INVALID_SIM_STATE, "INVALID_SIM_STATE"},
   {telux::common::ErrorCode::INVALID_MODEM_STATE, "INVALID_MODEM_STATE"},
   {telux::common::ErrorCode::INVALID_CALL_ID, "INVALID_CALL_ID"},
   {telux::common::ErrorCode::NO_SMS_TO_ACK, "NO_SMS_TO_ACK"},
   {telux::common::ErrorCode::NETWORK_ERR, "NETWORK_ERR"},
   {telux::common::ErrorCode::REQUEST_RATE_LIMITED, "REQUEST_RATE_LIMITED"},
   {telux::common::ErrorCode::SIM_BUSY, "SIM_BUSY"},
   {telux::common::ErrorCode::SIM_FULL, "SIM_FULL"},
   {telux::common::ErrorCode::NETWORK_REJECT, "NETWORK_REJECT"},
   {telux::common::ErrorCode::OPERATION_NOT_ALLOWED, "OPERATION_NOT_ALLOWED"},
   {telux::common::ErrorCode::EMPTY_RECORD, "EMPTY_RECORD"},
   {telux::common::ErrorCode::INVALID_SMS_FORMAT, "INVALID_SMS_FORMAT"},
   {telux::common::ErrorCode::ENCODING_ERR, "ENCODING_ERR"},
   {telux::common::ErrorCode::INVALID_SMSC_ADDRESS, "INVALID_SMSC_ADDRESS"},
   {telux::common::ErrorCode::NO_SUCH_ENTRY, "NO_SUCH_ENTRY"},
   {telux::common::ErrorCode::NETWORK_NOT_READY, "NETWORK_NOT_READY"},
   {telux::common::ErrorCode::NOT_PROVISIONED, "NOT_PROVISIONED"},
   {telux::common::ErrorCode::NO_SUBSCRIPTION, "NO_SUBSCRIPTION"},
   {telux::common::ErrorCode::NO_NETWORK_FOUND, "NO_NETWORK_FOUND"},
   {telux::common::ErrorCode::DEVICE_IN_USE, "DEVICE_IN_USE"},
   {telux::common::ErrorCode::ABORTED, "ABORTED"},
   {telux::common::ErrorCode::INCOMPATIBLE_STATE, "INCOMPATIBLE_STATE"},
   {telux::common::ErrorCode::NO_EFFECT, "NO_EFFECT"},
   {telux::common::ErrorCode::DEVICE_NOT_READY, "DEVICE_NOT_READY"},
   {telux::common::ErrorCode::MISSING_ARGUMENTS, "MISSING_ARGUMENTS"},
   {telux::common::ErrorCode::MALFORMED_MSG, "MALFORMED_MSG"},
   {telux::common::ErrorCode::INTERNAL, "INTERNAL"},
   {telux::common::ErrorCode::CLIENT_IDS_EXHAUSTED, "CLIENT_IDS_EXHAUSTED"},
   {telux::common::ErrorCode::UNABORTABLE_TRANSACTION, "UNABORTABLE_TRANSACTION"},
   {telux::common::ErrorCode::INVALID_CLIENT_ID, "INVALID_CLIENT_ID"},
   {telux::common::ErrorCode::NO_THRESHOLDS, "NO_THRESHOLDS"},
   {telux::common::ErrorCode::INVALID_HANDLE, "INVALID_HANDLE"},
   {telux::common::ErrorCode::INVALID_PROFILE, "INVALID_PROFILE"},
   {telux::common::ErrorCode::INVALID_PINID, "INVALID_PINID"},
   {telux::common::ErrorCode::INCORRECT_PIN, "INCORRECT_PIN"},
   {telux::common::ErrorCode::CALL_FAILED, "CALL_FAILED"},
   {telux::common::ErrorCode::OUT_OF_CALL, "OUT_OF_CALL"},
   {telux::common::ErrorCode::MISSING_ARG, "MISSING_ARG"},
   {telux::common::ErrorCode::ARG_TOO_LONG, "ARG_TOO_LONG"},
   {telux::common::ErrorCode::INVALID_TX_ID, "INVALID_TX_ID"},
   {telux::common::ErrorCode::OP_NETWORK_UNSUPPORTED, "OP_NETWORK_UNSUPPORTED"},
   {telux::common::ErrorCode::OP_DEVICE_UNSUPPORTED, "OP_DEVICE_UNSUPPORTED"},
   {telux::common::ErrorCode::NO_FREE_PROFILE, "NO_FREE_PROFILE"},
   {telux::common::ErrorCode::INVALID_PDP_TYPE, "INVALID_PDP_TYPE"},
   {telux::common::ErrorCode::INVALID_TECH_PREF, "INVALID_TECH_PREF"},
   {telux::common::ErrorCode::INVALID_PROFILE_TYPE, "INVALID_PROFILE_TYPE"},
   {telux::common::ErrorCode::INVALID_SERVICE_TYPE, "INVALID_SERVICE_TYPE"},
   {telux::common::ErrorCode::INVALID_REGISTER_ACTION, "INVALID_REGISTER_ACTION"},
   {telux::common::ErrorCode::INVALID_PS_ATTACH_ACTION, "INVALID_PS_ATTACH_ACTION"},
   {telux::common::ErrorCode::AUTHENTICATION_FAILED, "AUTHENTICATION_FAILED"},
   {telux::common::ErrorCode::PIN_BLOCKED, "PIN_BLOCKED"},
   {telux::common::ErrorCode::PIN_PERM_BLOCKED, "PIN_PERM_BLOCKED"},
   {telux::common::ErrorCode::SIM_NOT_INITIALIZED, "SIM_NOT_INITIALIZED"},
   {telux::common::ErrorCode::MAX_QOS_REQUESTS_IN_USE, "MAX_QOS_REQUESTS_IN_USE"},
   {telux::common::ErrorCode::INCORRECT_FLOW_FILTER, "INCORRECT_FLOW_FILTER"},
   {telux::common::ErrorCode::NETWORK_QOS_UNAWARE, "NETWORK_QOS_UNAWARE"},
   {telux::common::ErrorCode::INVALID_ID, "INVALID_ID"},
   {telux::common::ErrorCode::REQUESTED_NUM_UNSUPPORTED, "REQUESTED_NUM_UNSUPPORTED"},
   {telux::common::ErrorCode::INTERFACE_NOT_FOUND, "INTERFACE_NOT_FOUND"},
   {telux::common::ErrorCode::FLOW_SUSPENDED, "FLOW_SUSPENDED"},
   {telux::common::ErrorCode::INVALID_DATA_FORMAT, "INVALID_DATA_FORMAT"},
   {telux::common::ErrorCode::GENERAL, "GENERAL"},
   {telux::common::ErrorCode::UNKNOWN, "UNKNOWN"},
   {telux::common::ErrorCode::INVALID_ARG, "INVALID_ARG"},
   {telux::common::ErrorCode::INVALID_INDEX, "INVALID_INDEX"},
   {telux::common::ErrorCode::NO_ENTRY, "NO_ENTRY"},
   {telux::common::ErrorCode::DEVICE_STORAGE_FULL, "DEVICE_STORAGE_FULL"},
   {telux::common::ErrorCode::CAUSE_CODE, "CAUSE_CODE"},
   {telux::common::ErrorCode::MESSAGE_NOT_SENT, "MESSAGE_NOT_SENT"},
   {telux::common::ErrorCode::MESSAGE_DELIVERY_FAILURE, "MESSAGE_DELIVERY_FAILURE"},
   {telux::common::ErrorCode::INVALID_MESSAGE_ID, "INVALID_MESSAGE_ID"},
   {telux::common::ErrorCode::ENCODING, "ENCODING"},
   {telux::common::ErrorCode::AUTHENTICATION_LOCK, "AUTHENTICATION_LOCK"},
   {telux::common::ErrorCode::INVALID_TRANSITION, "INVALID_TRANSITION"},
   {telux::common::ErrorCode::NOT_A_MCAST_IFACE, "NOT_A_MCAST_IFACE"},
   {telux::common::ErrorCode::MAX_MCAST_REQUESTS_IN_USE, "MAX_MCAST_REQUESTS_IN_USE"},
   {telux::common::ErrorCode::INVALID_MCAST_HANDLE, "INVALID_MCAST_HANDLE"},
   {telux::common::ErrorCode::INVALID_IP_FAMILY_PREF, "INVALID_IP_FAMILY_PREF"},
   {telux::common::ErrorCode::SESSION_INACTIVE, "SESSION_INACTIVE"},
   {telux::common::ErrorCode::SESSION_INVALID, "SESSION_INVALID"},
   {telux::common::ErrorCode::SESSION_OWNERSHIP, "SESSION_OWNERSHIP"},
   {telux::common::ErrorCode::INSUFFICIENT_RESOURCES, "INSUFFICIENT_RESOURCES"},
   {telux::common::ErrorCode::DISABLED, "DISABLED"},
   {telux::common::ErrorCode::INVALID_OPERATION, "INVALID_OPERATION"},
   {telux::common::ErrorCode::INVALID_QMI_CMD, "INVALID_QMI_CMD"},
   {telux::common::ErrorCode::TPDU_TYPE, "TPDU_TYPE"},
   {telux::common::ErrorCode::SMSC_ADDR, "SMSC_ADDR"},
   {telux::common::ErrorCode::INFO_UNAVAILABLE, "INFO_UNAVAILABLE"},
   {telux::common::ErrorCode::SEGMENT_TOO_LONG, "SEGMENT_TOO_LONG"},
   {telux::common::ErrorCode::SEGMENT_ORDER, "SEGMENT_ORDER"},
   {telux::common::ErrorCode::BUNDLING_NOT_SUPPORTED, "BUNDLING_NOT_SUPPORTED"},
   {telux::common::ErrorCode::OP_PARTIAL_FAILURE, "OP_PARTIAL_FAILURE"},
   {telux::common::ErrorCode::POLICY_MISMATCH, "POLICY_MISMATCH"},
   {telux::common::ErrorCode::SIM_FILE_NOT_FOUND, "SIM_FILE_NOT_FOUND"},
   {telux::common::ErrorCode::FILE_NOT_FOUND, "FILE_NOT_FOUND"},
   {telux::common::ErrorCode::EXTENDED_INTERNAL, "EXTENDED_INTERNAL"},
   {telux::common::ErrorCode::ACCESS_DENIED, "ACCESS_DENIED"},
   {telux::common::ErrorCode::HARDWARE_RESTRICTED, "HARDWARE_RESTRICTED"},
   {telux::common::ErrorCode::ACK_NOT_SENT, "ACK_NOT_SENT"},
   {telux::common::ErrorCode::INJECT_TIMEOUT, "INJECT_TIMEOUT"},
   {telux::common::ErrorCode::FDN_RESTRICT, "FDN_RESTRICT"},
   {telux::common::ErrorCode::SUPS_FAILURE_CAUSE, "SUPS_FAILURE_CAUSE"},
   {telux::common::ErrorCode::NO_RADIO, "NO_RADIO"},
   {telux::common::ErrorCode::NOT_SUPPORTED, "NOT_SUPPORTED"},
   {telux::common::ErrorCode::CARD_CALL_CONTROL_FAILED, "CARD_CALL_CONTROL_FAILED"},
   {telux::common::ErrorCode::NETWORK_ABORTED, "NETWORK_ABORTED"},
   {telux::common::ErrorCode::MSG_BLOCKED, "MSG_BLOCKED"},
   {telux::common::ErrorCode::INVALID_SESSION_TYPE, "INVALID_SESSION_TYPE"},
   {telux::common::ErrorCode::INVALID_PB_TYPE, "INVALID_PB_TYPE"},
   {telux::common::ErrorCode::NO_SIM, "NO_SIM"},
   {telux::common::ErrorCode::PB_NOT_READY, "PB_NOT_READY"},
   {telux::common::ErrorCode::PIN_RESTRICTION, "PIN_RESTRICTION"},
   {telux::common::ErrorCode::PIN2_RESTRICTION, "PIN2_RESTRICTION"},
   {telux::common::ErrorCode::PUK_RESTRICTION, "PUK_RESTRICTION"},
   {telux::common::ErrorCode::PUK2_RESTRICTION, "PUK2_RESTRICTION"},
   {telux::common::ErrorCode::PB_ACCESS_RESTRICTED, "PB_ACCESS_RESTRICTED"},
   {telux::common::ErrorCode::PB_DELETE_IN_PROG, "PB_DELETE_IN_PROG"},
   {telux::common::ErrorCode::PB_TEXT_TOO_LONG, "PB_TEXT_TOO_LONG"},
   {telux::common::ErrorCode::PB_NUMBER_TOO_LONG, "PB_NUMBER_TOO_LONG"},
   {telux::common::ErrorCode::PB_HIDDEN_KEY_RESTRICTION, "PB_HIDDEN_KEY_RESTRICTION"},
   {telux::common::ErrorCode::PB_NOT_AVAILABLE, "PB_NOT_AVAILABLE"},
   {telux::common::ErrorCode::DEVICE_MEMORY_ERROR, "DEVICE_MEMORY_ERROR"},
   {telux::common::ErrorCode::NO_PERMISSION, "NO_PERMISSION"},
   {telux::common::ErrorCode::TOO_SOON, "TOO_SOON"},
   {telux::common::ErrorCode::TIME_NOT_ACQUIRED, "TIME_NOT_ACQUIRED"},
   {telux::common::ErrorCode::OP_IN_PROGRESS, "OP_IN_PROGRESS"},
   {telux::common::ErrorCode::INTERNAL_ERROR, "INTERNAL_ERROR"},
   {telux::common::ErrorCode::SERVICE_ERROR, "SERVICE_ERROR"},
   {telux::common::ErrorCode::TIMEOUT_ERROR, "TIMEOUT_ERROR"},
   {telux::common::ErrorCode::EXTENDED_ERROR, "EXTENDED_ERROR"},
   {telux::common::ErrorCode::PORT_NOT_OPEN_ERROR, "PORT_NOT_OPEN_ERROR"},
   {telux::common::ErrorCode::MEMCOPY_ERROR, "MEMCOPY_ERROR"},
   {telux::common::ErrorCode::INVALID_TRANSACTION, "INVALID_TRANSACTION"},
   {telux::common::ErrorCode::ALLOCATION_FAILURE, "ALLOCATION_FAILURE"},
   {telux::common::ErrorCode::TRANSPORT_ERROR, "TRANSPORT_ERROR"},
   {telux::common::ErrorCode::PARAM_ERROR, "PARAM_ERROR"},
   {telux::common::ErrorCode::INVALID_CLIENT, "INVALID_CLIENT"},
   {telux::common::ErrorCode::FRAMEWORK_NOT_READY, "FRAMEWORK_NOT_READY"},
   {telux::common::ErrorCode::INVALID_SIGNAL, "INVALID_SIGNAL"},
   {telux::common::ErrorCode::TRANSPORT_BUSY_ERROR, "TRANSPORT_BUSY_ERROR"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_FAIL, "DS_PROFILE_REG_RESULT_FAIL"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_HNDL,
    "DS_PROFILE_REG_RESULT_ERR_INVAL_HNDL"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_OP,
    "DS_PROFILE_REG_RESULT_ERR_INVAL_OP"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_TYPE,
    "DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_TYPE"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM,
    "DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_IDENT,
    "DS_PROFILE_REG_RESULT_ERR_INVAL_IDENT"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL, "DS_PROFILE_REG_RESULT_ERR_INVAL"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_LIB_NOT_INITED,
    "DS_PROFILE_REG_RESULT_ERR_LIB_NOT_INITED"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_LEN_INVALID,
    "DS_PROFILE_REG_RESULT_ERR_LEN_INVALID"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_LIST_END, "DS_PROFILE_REG_RESULT_LIST_END"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_SUBS_ID,
    "DS_PROFILE_REG_RESULT_ERR_INVAL_SUBS_ID"},
   {telux::common::ErrorCode::DS_PROFILE_REG_INVAL_PROFILE_FAMILY,
    "DS_PROFILE_REG_INVAL_PROFILE_FAMILY"},
   {telux::common::ErrorCode::DS_PROFILE_REG_PROFILE_VERSION_MISMATCH,
    "DS_PROFILE_REG_PROFILE_VERSION_MISMATCH"},
   {telux::common::ErrorCode::REG_RESULT_ERR_OUT_OF_MEMORY, "REG_RESULT_ERR_OUT_OF_MEMORY"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_FILE_ACCESS,
    "DS_PROFILE_REG_RESULT_ERR_FILE_ACCESS"},
   {telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_EOF, "DS_PROFILE_REG_RESULT_ERR_EOF"},
   {telux::common::ErrorCode::REG_RESULT_ERR_VALID_FLAG_NOT_SET,
    "REG_RESULT_ERR_VALID_FLAG_NOT_SET"},
   {telux::common::ErrorCode::REG_RESULT_ERR_OUT_OF_PROFILES, "REG_RESULT_ERR_OUT_OF_PROFILES"},
   {telux::common::ErrorCode::REG_RESULT_NO_EMERGENCY_PDN_SUPPORT,
    "REG_RESULT_NO_EMERGENCY_PDN_SUPPORT"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP_INVAL_PROFILE_FAMILY,
    "DS_PROFILE_3GPP_INVAL_PROFILE_FAMILY"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP_ACCESS_ERR, "DS_PROFILE_3GPP_ACCESS_ERR"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP_CONTEXT_NOT_DEFINED,
    "DS_PROFILE_3GPP_CONTEXT_NOT_DEFINED"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP_VALID_FLAG_NOT_SET,
    "DS_PROFILE_3GPP_VALID_FLAG_NOT_SET"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP_READ_ONLY_FLAG_SET,
    "DS_PROFILE_3GPP_READ_ONLY_FLAG_SET"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP_ERR_OUT_OF_PROFILES,
    "DS_PROFILE_3GPP_ERR_OUT_OF_PROFILES"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP2_ERR_INVALID_IDENT_FOR_PROFILE,
    "DS_PROFILE_3GPP2_ERR_INVALID_IDENT_FOR_PROFILE"},
   {telux::common::ErrorCode::DS_PROFILE_3GPP2_ERR_OUT_OF_PROFILE,
    "DS_PROFILE_3GPP2_ERR_OUT_OF_PROFILE"}
};

class VoiceCallPAController {
public:
    static std::shared_ptr<VoiceCallPAController> getInstance()
    {
        static std::shared_ptr<VoiceCallPAController> instance(new VoiceCallPAController());
        return instance;
    }

    pa_result_t initialize();

    std::shared_ptr<telux::tel::ICallManager> getCallManager()
    {
        return callManager_;
    }

    pa_result_t registerEventListener(taf_pa_voicecall_EventListener listener, std::any context)
    {
        if (listener)
        {
            eventListener_ = listener;
        }
        else
        {
            PA_ERROR("Listener is NULL");
            return PA_NOT_FOUND;
        }

        if (context.has_value())
        {
            eventListenerContext_ = std::move(context);
        }

        return PA_OK;
    }

    const char* stateToStr(telux::tel::CallState state);

    pa_result_t setTermination(taf_pa_voicecall_CallInfo_t *callInfoPtr, taf_pa_voicecall_termination_t termination);

    taf_pa_voicecall_termination_t convertToPaTermination(telux::tel::CallEndCause endCause);

    taf_pa_voicecall_dir_t directionToPaDirection(telux::tel::CallDirection direction);

    taf_pa_voicecall_event_t stateToEvent(telux::tel::CallState state);

    std::string paErrorToString(telux::common::ErrorCode error);

    std::shared_ptr<taf_pa_voicecall_CallInfo_t> createCallInfo(std::shared_ptr<telux::tel::ICall> iCall);

    void destructorCallInfo(void* objPtr);

    VoiceCallPAController() = default;
    ~VoiceCallPAController() = default;

    class VoiceCallListener : public telux::tel::ICallListener
    {
        public:
            VoiceCallListener(VoiceCallPAController* controller) : controller_(controller) {}
            void onIncomingCall(std::shared_ptr<telux::tel::ICall> tafCall) override;
            void onCallInfoChange(std::shared_ptr<telux::tel::ICall> tafCall) override;
            ~VoiceCallListener() = default;
        private:
            VoiceCallPAController* controller_;
    };

    class CommandCallback : public telux::common::ICommandResponseCallback
    {
        public:
            CommandCallback() = default;
            ~CommandCallback() = default;

            CommandCallback(
                std::shared_ptr<telux::tel::ICallManager> callMgr,
                std::unique_ptr<taf_pa_voicecall_CallInfo_t> callInfoPtr,
                taf_pa_voicecall_CallCb callback,
                std::any context)
                : callMgr_(callMgr), callback_(callback), context_(context), callInfoPtr_(std::move(callInfoPtr))
            {
                return;
            }

            void commandResponse(telux::common::ErrorCode error) override
            {
                PA_INFO("Cmd resp: %d", static_cast<int>(error));
                callProm_.set_value(error);
                if (callback_) {
                    callback_(PA_OK, *callInfoPtr_, context_);
                }
            }

            std::future<telux::common::ErrorCode> getFuture() {
                return callProm_.get_future();
            }

        protected:
            std::promise<telux::common::ErrorCode> callProm_;
            std::shared_ptr<telux::tel::ICallManager> callMgr_;
            taf_pa_voicecall_CallCb callback_;
            std::any context_;
            std::unique_ptr<taf_pa_voicecall_CallInfo_t> callInfoPtr_;
    };

    class MakeCallCallback : public telux::tel::IMakeCallCallback {
    public:
        MakeCallCallback(
            std::shared_ptr<telux::tel::ICallManager> callMgr,
            std::unique_ptr<taf_pa_voicecall_CallInfo_t> callInfoPtr,
            taf_pa_voicecall_CallCb callback,
            std::any context)
            : callMgr_(callMgr), callback_(callback), context_(context), callInfoPtr_(std::move(callInfoPtr)) {}

        void makeCallResponse(
            telux::common::ErrorCode error,
            std::shared_ptr<telux::tel::ICall> iCall) override
        {
            auto pACtrl = VoiceCallPAController::getInstance();
            PA_INFO("Make call resp: %s", pACtrl->paErrorToString(error).c_str());

            callObj_ = std::move(iCall);
            callProm_.set_value(error);

            if (callback_) {
                pa_result_t errorCode = (error == telux::common::ErrorCode::SUCCESS) ? PA_OK : PA_FAULT;
                callback_(errorCode, *callInfoPtr_, context_);
            }
        }

        std::future<telux::common::ErrorCode> getFuture() {
            return callProm_.get_future();
        }

        std::shared_ptr<telux::tel::ICall> getCallObj() {
            return callObj_;
        }

    private:
        std::promise<telux::common::ErrorCode> callProm_;
        std::shared_ptr<telux::tel::ICall> callObj_;
        std::shared_ptr<telux::tel::ICallManager> callMgr_;
        taf_pa_voicecall_CallCb callback_;
        std::any context_;
        std::unique_ptr<taf_pa_voicecall_CallInfo_t> callInfoPtr_;
    };


private:
    VoiceCallPAController(const VoiceCallPAController&) = delete;
    VoiceCallPAController& operator=(const VoiceCallPAController&) = delete;
    std::shared_ptr<telux::tel::ICallManager> callManager_;
    std::shared_ptr<VoiceCallListener> callListener_;
    taf_pa_voicecall_EventListener eventListener_;
    std::any eventListenerContext_;
    std::vector<std::shared_ptr<telux::tel::ICall>> obsoletedIcalls;

    static std::shared_ptr<VoiceCallPAController> instance;
};

std::string VoiceCallPAController::paErrorToString(telux::common::ErrorCode error)
{
   if(paErrorCodeToStringMap.find(error) != std::end(paErrorCodeToStringMap))
   {
      return paErrorCodeToStringMap[error];
   }
   return "UNKNOWN_ERROR";
}

taf_pa_voicecall_event_t VoiceCallPAController::stateToEvent(telux::tel::CallState state)
{
    taf_pa_voicecall_event_t event = TAF_PA_VOICECALL_EVENT_ENDED;

    switch (state)
    {
        case telux::tel::CallState::CALL_ACTIVE:
            event = TAF_PA_VOICECALL_EVENT_ACTIVE;
        break;

        case telux::tel::CallState::CALL_ON_HOLD:
            event = TAF_PA_VOICECALL_EVENT_ONHOLD;
        break;

        case telux::tel::CallState::CALL_DIALING:
            event = TAF_PA_VOICECALL_EVENT_DIALING;
        break;

        case telux::tel::CallState::CALL_INCOMING:
            event = TAF_PA_VOICECALL_EVENT_INCOMING;
        break;

        case telux::tel::CallState::CALL_WAITING:
            event = TAF_PA_VOICECALL_EVENT_WAITING;
        break;

        case telux::tel::CallState::CALL_ALERTING:
            event = TAF_PA_VOICECALL_EVENT_ALERTING;
        break;

        case telux::tel::CallState::CALL_ENDED:
            event = TAF_PA_VOICECALL_EVENT_ENDED;
        break;

        default:
        break;
    }

    return event;
}

const char* VoiceCallPAController::stateToStr(telux::tel::CallState state)
{
    switch (state)
    {
        case telux::tel::CallState::CALL_IDLE:
            return "CALL_IDLE";
        case telux::tel::CallState::CALL_ACTIVE:
            return "CALL_ACTIVE";
        case telux::tel::CallState::CALL_ON_HOLD:
            return "CALL_ON_HOLD";
        case telux::tel::CallState::CALL_DIALING:
            return "CALL_DIALING";
        case telux::tel::CallState::CALL_INCOMING:
            return "CALL_INCOMING";
        case telux::tel::CallState::CALL_WAITING:
            return "CALL_WAITING";
        case telux::tel::CallState::CALL_ALERTING:
            return "CALL_ALERTING";
        case telux::tel::CallState::CALL_ENDED:
            return "CALL_ENDED";
        default:
            return "CALL_STATE_UNKNOWN";
    }
}

pa_result_t VoiceCallPAController::setTermination(taf_pa_voicecall_CallInfo_t* callInfoPtr, taf_pa_voicecall_termination_t termination)
{
    if (callInfoPtr == NULL)
    {
        PA_ERROR("Cannot found call info from reference: %p", callInfoPtr);
        return PA_NOT_FOUND;
    }

    callInfoPtr->termination = termination;
    return PA_OK;
}

taf_pa_voicecall_termination_t VoiceCallPAController::convertToPaTermination(telux::tel::CallEndCause endCause)
{
    taf_pa_voicecall_termination_t termination = TAF_PA_VOICECALL_TERM_UNDEFINED;

    switch(endCause) {
        case telux::tel::CallEndCause::UNOBTAINABLE_NUMBER:
        case telux::tel::CallEndCause::NUMBER_CHANGED:
        case telux::tel::CallEndCause::DESTINATION_OUT_OF_ORDER:
        case telux::tel::CallEndCause::INVALID_NUMBER_FORMAT:
        case telux::tel::CallEndCause::INCOMPATIBLE_DESTINATION:
        case telux::tel::CallEndCause::SIP_BAD_ADDRESS:
        case telux::tel::CallEndCause::NOT_REACHABLE:
            termination = TAF_PA_VOICECALL_TERM_UNOBTAINABLE_NUMBER;
        break;

        case telux::tel::CallEndCause::NO_ROUTE_TO_DESTINATION:
        case telux::tel::CallEndCause::CHANNEL_UNACCEPTABLE:
        case telux::tel::CallEndCause::RESP_TO_STATUS_ENQUIRY:
        case telux::tel::CallEndCause::REQUESTED_FACILITY_NOT_SUBSCRIBED:
        case telux::tel::CallEndCause::BEARER_CAPABILITY_NOT_AUTHORIZED:
        case telux::tel::CallEndCause::BEARER_CAPABILITY_UNAVAILABLE:
        case telux::tel::CallEndCause::SERVICE_OPTION_NOT_AVAILABLE:
        case telux::tel::CallEndCause::BEARER_SERVICE_NOT_IMPLEMENTED:
        case telux::tel::CallEndCause::REQUESTED_FACILITY_NOT_IMPLEMENTED:
        case telux::tel::CallEndCause::SERVICE_OR_OPTION_NOT_IMPLEMENTED:
        case telux::tel::CallEndCause::INVALID_TRANSACTION_IDENTIFIER:
        case telux::tel::CallEndCause::USER_NOT_MEMBER_OF_CUG:
        case telux::tel::CallEndCause::CALL_BARRED:
        case telux::tel::CallEndCause::FDN_BLOCKED:
        case telux::tel::CallEndCause::IMSI_UNKNOWN_IN_VLR:
        case telux::tel::CallEndCause::IMEI_NOT_ACCEPTED:
        case telux::tel::CallEndCause::DIAL_MODIFIED_TO_USSD:
        case telux::tel::CallEndCause::DIAL_MODIFIED_TO_SS:
        case telux::tel::CallEndCause::DIAL_MODIFIED_TO_DIAL:
        case telux::tel::CallEndCause::OPERATOR_DETERMINED_BARRING:
        case telux::tel::CallEndCause::NETWORK_OUT_OF_ORDER:
            termination = TAF_PA_VOICECALL_TERM_NETWORK_FAIL;
        break;

        case telux::tel::CallEndCause::NORMAL:
        case telux::tel::CallEndCause::NORMAL_UNSPECIFIED:
        case telux::tel::CallEndCause::CLIENT_END:
            termination = TAF_PA_VOICECALL_TERM_NORMAL;
        break;

        case telux::tel::CallEndCause::BUSY:
        case telux::tel::CallEndCause::NO_ANSWER_FROM_USER:
        case telux::tel::CallEndCause::PREEMPTION:
        case telux::tel::CallEndCause::FACILITY_REJECTED:
        case telux::tel::CallEndCause::CONGESTION:
        case telux::tel::CallEndCause::SWITCHING_EQUIPMENT_CONGESTION:
        case telux::tel::CallEndCause::REQUESTED_CIRCUIT_OR_CHANNEL_NOT_AVAILABLE:
        case telux::tel::CallEndCause::RESOURCES_UNAVAILABLE_OR_UNSPECIFIED:
            termination = TAF_PA_VOICECALL_TERM_BUSY;
        break;

        case telux::tel::CallEndCause::CALL_REJECTED:
        case telux::tel::CallEndCause::SIP_REQUEST_CANCELLED:
            termination = TAF_PA_VOICECALL_TERM_REJECTED;
        break;

        case telux::tel::CallEndCause::NO_USER_RESPONDING:
            termination = TAF_PA_VOICECALL_TERM_NORESPONSE;
        break;

        case telux::tel::CallEndCause::TEMPORARY_FAILURE:
        case telux::tel::CallEndCause::ACCESS_INFORMATION_DISCARDED:
        case telux::tel::CallEndCause::QOS_UNAVAILABLE:
        case telux::tel::CallEndCause::INCOMING_CALLS_BARRED_WITHIN_CUG:
        case telux::tel::CallEndCause::ACM_LIMIT_EXCEEDED:
        case telux::tel::CallEndCause::ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE:
        case telux::tel::CallEndCause::INVALID_TRANSIT_NW_SELECTION:
        case telux::tel::CallEndCause::SEMANTICALLY_INCORRECT_MESSAGE:
        case telux::tel::CallEndCause::INVALID_MANDATORY_INFORMATION:
        case telux::tel::CallEndCause::MESSAGE_TYPE_NON_IMPLEMENTED:
        case telux::tel::CallEndCause::MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
        case telux::tel::CallEndCause::INFORMATION_ELEMENT_NON_EXISTENT:
        case telux::tel::CallEndCause::CONDITIONAL_IE_ERROR:
        case telux::tel::CallEndCause::MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
        case telux::tel::CallEndCause::RECOVERY_ON_TIMER_EXPIRED:
        case telux::tel::CallEndCause::PROTOCOL_ERROR_UNSPECIFIED:
        case telux::tel::CallEndCause::INTERWORKING_UNSPECIFIED:
        case telux::tel::CallEndCause::CDMA_LOCKED_UNTIL_POWER_CYCLE:
        case telux::tel::CallEndCause::CDMA_DROP:
        case telux::tel::CallEndCause::CDMA_INTERCEPT:
        case telux::tel::CallEndCause::CDMA_REORDER:
        case telux::tel::CallEndCause::CDMA_SO_REJECT:
        case telux::tel::CallEndCause::CDMA_RETRY_ORDER:
        case telux::tel::CallEndCause::CDMA_ACCESS_FAILURE:
        case telux::tel::CallEndCause::CDMA_PREEMPTED:
        case telux::tel::CallEndCause::CDMA_NOT_EMERGENCY:
        case telux::tel::CallEndCause::CDMA_ACCESS_BLOCKED:
        case telux::tel::CallEndCause::ERROR_UNSPECIFIED:
            termination = TAF_PA_VOICECALL_TERM_NETWORK_FAIL;
        break;

        default:
            termination = TAF_PA_VOICECALL_TERM_UNDEFINED;
        break;
    }

    PA_INFO("iCall end cause: %d, PA end cause: %d", (int)endCause, termination);
    return termination;
}

taf_pa_voicecall_dir_t VoiceCallPAController::directionToPaDirection(telux::tel::CallDirection direction)
{
    switch (direction)
    {
        case telux::tel::CallDirection::INCOMING:
            return TAF_PA_VOICECALL_DIR_INCOMING;
        case telux::tel::CallDirection::OUTGOING:
            return TAF_PA_VOICECALL_DIR_OUTGOING;
        default:
            return TAF_PA_VOICECALL_DIR_NONE;
    }
}

std::shared_ptr<taf_pa_voicecall_CallInfo_t> VoiceCallPAController::createCallInfo(std::shared_ptr<telux::tel::ICall> iCall)
{
    auto callInfoPtr = std::make_shared<taf_pa_voicecall_CallInfo_t>();
    callInfoPtr->phoneId = static_cast<int8_t>(iCall->getPhoneId());

    const std::string& remote = iCall->getRemotePartyNumber();
    gsize src_len = g_strlcpy(
        callInfoPtr->destId,
        remote.c_str(),
        PA_MAX_DESTINATION_LEN_BYTE
    );
    if (src_len >= PA_MAX_DESTINATION_LEN_BYTE)
    {
        PA_ERROR("destId truncated: src_len=%zu, buf_size=%u, value='%s'",
                 static_cast<size_t>(src_len),
                 static_cast<unsigned>(PA_MAX_DESTINATION_LEN_BYTE),
                 remote.c_str());
    }

    callInfoPtr->direction = static_cast<taf_pa_voicecall_dir_t>(iCall->getCallDirection());
    callInfoPtr->termination = taf_pa_voicecall_termination_t::TAF_PA_VOICECALL_TERM_NORMAL;

    return callInfoPtr;
}


void VoiceCallPAController::destructorCallInfo(void* objPtr)
{
    return;
}

void VoiceCallPAController::VoiceCallListener::onCallInfoChange(std::shared_ptr<telux::tel::ICall> iCall)
{
    auto pACtrl = VoiceCallPAController::getInstance();
    telux::tel::CallState state = iCall->getCallState();

    if (state == telux::tel::CallState::CALL_DIALING)
    {
        int callInfoFd = open(VoiceCallInfoConfFile, O_CREAT | O_RDWR | O_APPEND, 0644);
        if (callInfoFd < 0)
        {
            PA_ERROR("Failed to open voice call info file!");
        }
        else
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Phone ID: %d, Remote Party Number: %s, Call Direction: %d\n",
                 iCall->getPhoneId(), iCall->getRemotePartyNumber().c_str(), (int)iCall->getCallDirection());
            if (write(callInfoFd, buffer, strlen(buffer)) < 0)
            {
                PA_ERROR("Failed to write to voice call info file");
            }
            close(callInfoFd);
        }
    }

    if (state == telux::tel::CallState::CALL_ENDED)
    {
        std::ifstream inFile(VoiceCallInfoConfFile);
        if (inFile)
        {
            std::stringstream buffer;
            buffer << inFile.rdbuf();
            inFile.close();

            std::string line;
            std::string content;
            bool found = false;

            while (std::getline(buffer, line))
            {
                std::string callInfo = "Phone ID: " + std::to_string(iCall->getPhoneId()) + ", Remote Party Number: " + iCall->getRemotePartyNumber() + ", Call Direction: " + std::to_string((int)iCall->getCallDirection());
                if (line.find(callInfo) != std::string::npos)
                {
                    found = true;
                }
                else
                {
                    content += line + "\n";
                }
            }

            if (found)
            {
                if (content.empty())
                {
                    if (unlink(VoiceCallInfoConfFile) < 0)
                    {
                        PA_ERROR("Faild to delete %s: %s", VoiceCallInfoConfFile, strerror(errno));
                    }
                }
                else
                {
                    std::ofstream outFile(VoiceCallInfoConfFile, std::ios::trunc);
                    outFile << content;
                    outFile.close();
                }

                auto& obsoletedIcalls = pACtrl->obsoletedIcalls;
                auto it = std::remove_if(obsoletedIcalls.begin(), obsoletedIcalls.end(), [iCall](std::shared_ptr<telux::tel::ICall> obsoletedIcall) {
                        return obsoletedIcall->getPhoneId() == iCall->getPhoneId() &&
                               obsoletedIcall->getRemotePartyNumber() == iCall->getRemotePartyNumber();
                    });
                if (it != obsoletedIcalls.end())
                {
                    PA_INFO("Found an endded event from obsoleted icall %p, skip it..", iCall.get());
                    obsoletedIcalls.erase(it, obsoletedIcalls.end());
                    return;
                }

            }
        }
    }

    PA_INFO("On call state change dest: %s, state: %s",
        iCall->getRemotePartyNumber().c_str(), pACtrl->stateToStr(state));
    // call platform listener
    if (controller_ && controller_->eventListener_)
    {
        taf_pa_voicecall_CallInfo_t callInfo;
        callInfo.phoneId = iCall->getPhoneId();

        const std::string& remote = iCall->getRemotePartyNumber();
        gsize src_len = g_strlcpy(callInfo.destId,
                              remote.c_str(),
                              sizeof(callInfo.destId));
        if (src_len >= sizeof(callInfo.destId))
        {
            PA_ERROR("destId truncated: src_len=%zu, buf_size=%zu, value='%s'",
            static_cast<size_t>(src_len),
            sizeof(callInfo.destId),
            remote.c_str());
        }

        callInfo.direction = pACtrl->directionToPaDirection(iCall->getCallDirection());
        if (state == telux::tel::CallState::CALL_ENDED)
        {
            callInfo.termination = pACtrl->convertToPaTermination(iCall->getCallEndCause());
        }
        controller_->eventListener_(callInfo, pACtrl->stateToEvent(state), controller_->eventListenerContext_);
    }
    else
    {
        PA_ERROR("No listener is registered!, skip state: %s", pACtrl->stateToStr(state));
    }
}


void VoiceCallPAController::VoiceCallListener::onIncomingCall(std::shared_ptr<telux::tel::ICall> iCall)
{
    telux::tel::CallState state = iCall->getCallState();
    auto pACtrl = VoiceCallPAController::getInstance();

    PA_INFO("On call state change dest: %s, state: %s",
        iCall->getRemotePartyNumber().c_str(), pACtrl->stateToStr(state));

    if (iCall->getCallDirection() != telux::tel::CallDirection::INCOMING)
    {
        PA_ERROR("Call is not incoming type: %d", (int)iCall->getCallDirection());
        return;
    }

    int callInfoFd = open(VoiceCallInfoConfFile, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (callInfoFd < 0)
    {
        PA_ERROR("Failed to open voice call info file!");
    }
    else
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Phone ID: %d, Remote Party Number: %s, Call Direction: %d\n",
             iCall->getPhoneId(), iCall->getRemotePartyNumber().c_str(), (int)iCall->getCallDirection());
        if (write(callInfoFd, buffer, strlen(buffer)) < 0)
        {
            PA_ERROR("Failed to write to voice call info file");
        }
        close(callInfoFd);
    }

    // call platform listener
    if (controller_ && controller_->eventListener_)
    {
        taf_pa_voicecall_CallInfo_t callInfo;
        callInfo.phoneId = static_cast<int8_t>(iCall->getPhoneId());

        const std::string& remote = iCall->getRemotePartyNumber();
        gsize src_len = g_strlcpy(callInfo.destId,
                              remote.c_str(),
                              sizeof(callInfo.destId));
        if (src_len >= sizeof(callInfo.destId))
        {
            PA_ERROR("destId truncated: src_len=%zu, buf_size=%zu, value='%s'",
                static_cast<size_t>(src_len),
                sizeof(callInfo.destId),
                remote.c_str());
        }
        callInfo.direction = static_cast<taf_pa_voicecall_dir_t>(iCall->getCallDirection());
        callInfo.termination = taf_pa_voicecall_termination_t::TAF_PA_VOICECALL_TERM_NORMAL;
        controller_->eventListener_(callInfo, pACtrl->stateToEvent(state), controller_->eventListenerContext_);
    }
    else
    {
        PA_ERROR("No listener is registered!, skip state: %s", pACtrl->stateToStr(state));
    }
}

/* Implementation */
pa_result_t tafpa::voicecall::taf_pa_voicecall_Make(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
) {
    auto pACtrl = VoiceCallPAController::getInstance();
    auto callMgr = pACtrl->getCallManager();

    auto callInfoPtr = std::make_unique<taf_pa_voicecall_CallInfo_t>(callInfo);
    auto cbObj = std::make_shared<VoiceCallPAController::MakeCallCallback>(
        callMgr, std::move(callInfoPtr), callback, context
    );

    PA_INFO("Starting voice call for phone %d num: %s", callInfo.phoneId, callInfo.destId);

    telux::common::Status status = callMgr->makeCall(callInfo.phoneId, std::string(callInfo.destId), cbObj);
    telux::common::ErrorCode errorCode = cbObj->getFuture().get();

    if (status == telux::common::Status::SUCCESS &&
        errorCode == telux::common::ErrorCode::SUCCESS)
    {
        PA_INFO("Success to make call");
        return PA_OK;
    }

    PA_ERROR("Failed to make call, status: %d, error: %s", static_cast<int>(status),
        pACtrl->paErrorToString(errorCode).c_str());

    return PA_FAULT;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_Stop(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
)
{
    auto pACtrl = VoiceCallPAController::getInstance();
    auto callMgr = pACtrl->getCallManager();
    telux::common::Status status;

    auto activeCall = callMgr->getInProgressCalls();
    if (activeCall.empty()) {
        PA_ERROR("Cannot find any valid call");
        return PA_NOT_FOUND;
    }

    for (auto& iCall : activeCall) {
        if ((strncmp(iCall->getRemotePartyNumber().c_str(), callInfo.destId, PA_MAX_DESTINATION_LEN_BYTE) == 0) &&
            (iCall->getPhoneId() == callInfo.phoneId) &&
            (pACtrl->directionToPaDirection(iCall->getCallDirection()) == callInfo.direction))
        {
            if (iCall->getCallState() == telux::tel::CallState::CALL_ENDED) {
                PA_ERROR("Call for phone %d, dest: %s is already ended", callInfo.phoneId, callInfo.destId);
                return PA_DUPLICATE;
            }

            auto callInfoPtr = std::make_unique<taf_pa_voicecall_CallInfo_t>(callInfo);
            auto cbObj = std::make_shared<VoiceCallPAController::CommandCallback>(
                callMgr, std::move(callInfoPtr), callback, context);

            if (iCall->getCallState() == telux::tel::CallState::CALL_INCOMING) {
                status = iCall->reject(cbObj);
            } else {
                status = iCall->hangup(cbObj);
            }

            if ((status == telux::common::Status::SUCCESS) &&
                (cbObj->getFuture().get() == telux::common::ErrorCode::SUCCESS))
            {
                PA_INFO("Success to stop call");
                return PA_OK;
            }

            PA_ERROR("Failed to stop call, status: %d", static_cast<int>(status));
            return PA_FAULT;
        }
    }

    PA_ERROR("Cannot find valid iCall for phone %d, dest: %s", callInfo.phoneId, callInfo.destId);
    return PA_NOT_FOUND;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_Hold(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
)
{
    auto pACtrl = VoiceCallPAController::getInstance();
    auto callMgr = pACtrl->getCallManager();
    telux::common::Status status;

    auto activeCall = callMgr->getInProgressCalls();
    if (activeCall.empty()) {
        PA_ERROR("No call is in progress");
        return PA_NOT_FOUND;
    }

    for (auto& iCall : activeCall) {
        PA_INFO("Holding call status: %s", pACtrl->stateToStr(iCall->getCallState()));

        if ((strncmp(iCall->getRemotePartyNumber().c_str(), callInfo.destId, PA_MAX_DESTINATION_LEN_BYTE) == 0) &&
            (iCall->getPhoneId() == callInfo.phoneId) &&
            (pACtrl->directionToPaDirection(iCall->getCallDirection()) == callInfo.direction))
        {
            if (iCall->getCallState() == telux::tel::CallState::CALL_ON_HOLD)
            {
                PA_INFO("Already on HOLD status");
                return PA_DUPLICATE;
            }
            auto callInfoPtr = std::make_unique<taf_pa_voicecall_CallInfo_t>(callInfo);
            auto cbObj = std::make_shared<VoiceCallPAController::CommandCallback>(
                callMgr, std::move(callInfoPtr), callback, context);

            status = iCall->hold(cbObj);

            if ((status == telux::common::Status::SUCCESS) &&
                (cbObj->getFuture().get() == telux::common::ErrorCode::SUCCESS))
            {
                PA_INFO("Success to hold call");
                return PA_OK;
            }

            PA_ERROR("Failed to hold call, ret: %d, status: %s",
                static_cast<int>(status), pACtrl->stateToStr(iCall->getCallState()));
            return PA_FAULT;
        }
    }

    PA_ERROR("Cannot find valid iCall for phone %d, dest: %s", callInfo.phoneId, callInfo.destId);
    return PA_NOT_FOUND;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_Resume(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
)
{
    auto pACtrl = VoiceCallPAController::getInstance();
    auto callMgr = pACtrl->getCallManager();
    telux::common::Status status;

    auto activeCall = callMgr->getInProgressCalls();
    if (activeCall.empty()) {
        PA_ERROR("No call is in progress");
        return PA_NOT_FOUND;
    }

    for (auto& iCall : activeCall) {
        if ((strncmp(iCall->getRemotePartyNumber().c_str(), callInfo.destId, PA_MAX_DESTINATION_LEN_BYTE) == 0) &&
            (iCall->getPhoneId() == callInfo.phoneId) &&
            (pACtrl->directionToPaDirection(iCall->getCallDirection()) == callInfo.direction))
        {
            auto callInfoPtr = std::make_unique<taf_pa_voicecall_CallInfo_t>(callInfo);
            auto cbObj = std::make_shared<VoiceCallPAController::CommandCallback>(
                callMgr, std::move(callInfoPtr), callback, context);

            status = iCall->resume(cbObj);

            if ((status == telux::common::Status::SUCCESS) &&
                (cbObj->getFuture().get() == telux::common::ErrorCode::SUCCESS))
            {
                PA_INFO("Success to resume call");
                return PA_OK;
            }

            PA_ERROR("Failed to resume call, status: %d", static_cast<int>(status));
            return PA_FAULT;
        }
    }

    PA_ERROR("Cannot find valid iCall for phone %d, dest: %s", callInfo.phoneId, callInfo.destId);
    return PA_NOT_FOUND;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_Answer(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
)
{
    auto pACtrl = VoiceCallPAController::getInstance();
    auto callMgr = pACtrl->getCallManager();
    telux::common::Status status;

    auto activeCall = callMgr->getInProgressCalls();
    if (activeCall.empty()) {
        PA_ERROR("No call is in progress");
        return PA_NOT_FOUND;
    }

    for (auto& iCall : activeCall) {
        if ((strncmp(iCall->getRemotePartyNumber().c_str(), callInfo.destId, PA_MAX_DESTINATION_LEN_BYTE) == 0) &&
            (iCall->getPhoneId() == callInfo.phoneId) &&
            (pACtrl->directionToPaDirection(iCall->getCallDirection()) == callInfo.direction))
        {
            auto callInfoPtr = std::make_unique<taf_pa_voicecall_CallInfo_t>(callInfo);
            auto cbObj = std::make_shared<VoiceCallPAController::CommandCallback>(
                callMgr, std::move(callInfoPtr), callback, context);

            status = iCall->answer(cbObj);

            if ((status == telux::common::Status::SUCCESS) &&
                (cbObj->getFuture().get() == telux::common::ErrorCode::SUCCESS))
            {
                PA_INFO("Success to answer call");
                return PA_OK;
            }

            PA_ERROR("Failed to answer call, status: %d", static_cast<int>(status));
            return PA_FAULT;
        }
    }

    PA_ERROR("Cannot find valid iCall for phone %d, dest: %s", callInfo.phoneId, callInfo.destId);
    return PA_NOT_FOUND;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_Swap(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
)
{
    auto pACtrl = VoiceCallPAController::getInstance();
    auto callMgr = pACtrl->getCallManager();
    telux::common::Status status;

    auto activeCall = callMgr->getInProgressCalls();
    if (activeCall.size() < 2) {
        PA_ERROR("Call list does not have 2 calls");
        return PA_NOT_FOUND;
    }

    std::shared_ptr<telux::tel::ICall> iCall1, iCall2;
    uint8_t iCall1PhoneId = 0, iCall2PhoneId = 0;

    for (const auto& call : activeCall) {
        if (call->getCallState() == telux::tel::CallState::CALL_ACTIVE) {
            iCall1 = call;
            iCall1PhoneId = call->getPhoneId();
            continue;
        }
        if (call->getCallState() == telux::tel::CallState::CALL_ON_HOLD) {
            iCall2 = call;
            iCall2PhoneId = call->getPhoneId();
        }
        if (iCall1 && iCall2) {
            break;
        }
    }

    if (iCall1 && iCall2 && iCall1PhoneId == iCall2PhoneId) {
        auto callInfoPtr = std::make_unique<taf_pa_voicecall_CallInfo_t>(callInfo);
        auto cbObj = std::make_shared<VoiceCallPAController::CommandCallback>(
            callMgr, std::move(callInfoPtr), callback, context);

        status = callMgr->swap(iCall1, iCall2, cbObj);

        if (status == telux::common::Status::SUCCESS &&
            cbObj->getFuture().get() == telux::common::ErrorCode::SUCCESS)
        {
            PA_INFO("Success to swap calls");
            return PA_OK;
        }

        PA_ERROR("Failed to swap calls, status: %d", static_cast<int>(status));
        return PA_FAULT;
    }
    else {
        PA_ERROR("Call list does not have 2 calls or phone IDs do not match");
        return PA_FAULT;
    }

    PA_ERROR("Cannot find valid iCall for phone %d, dest: %s", callInfo.phoneId, callInfo.destId);
    return PA_NOT_FOUND;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_RegisterEventListener
(
    taf_pa_voicecall_EventListener listener, std::any context
)
{
    auto pACtrl = VoiceCallPAController::getInstance();

    return pACtrl->registerEventListener(listener, context);
}

pa_result_t VoiceCallPAController::initialize()
{
    auto& phoneFactory = telux::tel::PhoneFactory::getInstance();
    std::promise<telux::common::ServiceStatus> promise;

    callManager_ = phoneFactory.getCallManager([&](telux::common::ServiceStatus status) {
        PA_INFO("Getting status: %d from call manager", static_cast<int>(status));
        if (status != telux::common::ServiceStatus::SERVICE_UNAVAILABLE)
        {
            promise.set_value(status);
        }
    });

    if (!callManager_) {
        PA_CRIT("*** ERROR - callManager is NULL");
        return PA_FAULT;
    }

    auto initFuture = promise.get_future();
    auto waitStatus = initFuture.wait_for(std::chrono::seconds(MAX_INIT_TIMEOUT));
    if (waitStatus == std::future_status::timeout) {
        PA_CRIT("*** ERROR - Timeout to get call manager ready");
        return PA_TIMEOUT;
    } else {
        auto serviceStatus = initFuture.get();
        if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            PA_CRIT("*** ERROR - call manager is unavailable : %d", static_cast<int>(serviceStatus));
            return PA_UNAVAILABLE;
        }
    }

    struct stat st;
    if (stat(VoiceCallInfoConfFile, &st) == 0)
    {
        PA_INFO("voice call was not closed normal");
        std::shared_ptr<telux::tel::ICall> iCall = nullptr;
        telux::common::Status status = telux::common::Status::FAILED;
        std::vector<std::shared_ptr<telux::tel::ICall>> inProgressCalls
            = getCallManager()->getInProgressCalls();

        std::ifstream inFile(VoiceCallInfoConfFile);
        std::vector<std::string> fileLines;
        if (inFile)
        {
            std::string line;
            while (std::getline(inFile, line))
            {
                fileLines.push_back(line);
            }
            inFile.close();
        }

        for (auto callIterator = std::begin(inProgressCalls);
             callIterator != std::end(inProgressCalls); ++callIterator)
        {
            iCall = *callIterator;
            if (iCall)
            {
                if (iCall->getCallState() != telux::tel::CallState::CALL_ENDED)
                {
                    bool matchFound = false;
                    for (const auto& fileLine : fileLines)
                    {
                        if (fileLine.find(std::to_string(iCall->getPhoneId())) != std::string::npos &&
                            fileLine.find(iCall->getRemotePartyNumber()) != std::string::npos)
                        {
                            matchFound = true;
                            break;
                        }
                    }

                    if (matchFound)
                    {
                        PA_INFO("There's obsoleted voice call active");
                        if (iCall->getCallState() == telux::tel::CallState::CALL_INCOMING)
                            status = iCall->reject(std::shared_ptr<telux::common::ICommandResponseCallback>(nullptr));
                        else
                            status = iCall->hangup(nullptr);

                        if (status == telux::common::Status::SUCCESS)
                        {
                            auto it = std::find_if(obsoletedIcalls.begin(), obsoletedIcalls.end(),
                                [iCall](std::shared_ptr<telux::tel::ICall> obsoletedIcall) {
                                    return obsoletedIcall->getPhoneId() == iCall->getPhoneId() &&
                                           obsoletedIcall->getRemotePartyNumber() == iCall->getRemotePartyNumber();
                                });

                            if (it == obsoletedIcalls.end())
                            {
                                PA_INFO("Pushing obsoleted voice call %p for backup", iCall.get());
                                obsoletedIcalls.push_back(iCall);
                            }
                        }
                        else
                        {
                            PA_ERROR("voice call hangup failed for iCall %p!", iCall.get());
                        }
                    }
                }
            }
        }
    }

    callListener_ = std::make_shared<VoiceCallListener>(this);
    auto ret = callManager_->registerListener(callListener_);
    if (ret != telux::common::Status::SUCCESS) {
        PA_CRIT("*** ERROR - Cannot register Listener for call event");
        return PA_FAULT;
    }

    return PA_OK;
}

pa_result_t tafpa::voicecall::taf_pa_voicecall_Init()
{
    auto pACtrl = VoiceCallPAController::getInstance();

    pa_result_t result = pACtrl->initialize();
    if (result == PA_OK)
    {
        PA_INFO("Voice call platform adapter initialization is done");
    }
    else
    {
        PA_CRIT("Failed to initialize voice call platform adapter, ret: %d", result);
    }

    return result;
}
