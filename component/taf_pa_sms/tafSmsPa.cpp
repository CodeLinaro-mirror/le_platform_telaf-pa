/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <future>
#include <string>
#include <iomanip>
#include <atomic>

#include <telux/common/CommonDefines.hpp>
#include <telux/common/DeviceConfig.hpp>
#include <telux/platform/PlatformFactory.hpp>
#include <telux/tel/CellBroadcastManager.hpp>
#include <telux/tel/PhoneFactory.hpp>
#include <telux/tel/SmsManager.hpp>

#include "tafSmsPa.hpp"

#define MAX_INIT_TIMEOUT 5
#define MIN_SIM_SLOT_COUNT 1
#define MAX_SIM_SLOT_COUNT 2

using namespace telux::common;
using namespace telux::platform;
using namespace telux::tel;
using namespace tafpa::sms;
using namespace std;

static std::atomic<bool> g_smsPaInitialized(false);

std::map<telux::common::ErrorCode, std::string> errorCodeToStringMap_ = {

    { telux::common::ErrorCode::SUCCESS, "SUCCESS" },
    { telux::common::ErrorCode::RADIO_NOT_AVAILABLE, "RADIO_NOT_AVAILABLE" },
    { telux::common::ErrorCode::GENERIC_FAILURE, "GENERIC_FAILURE" },
    { telux::common::ErrorCode::PASSWORD_INCORRECT, "PASSWORD_INCORRECT" },
    { telux::common::ErrorCode::SIM_PIN2, "SIM_PIN2" },
    { telux::common::ErrorCode::SIM_PUK2, "SIM_PUK2" },
    { telux::common::ErrorCode::REQUEST_NOT_SUPPORTED, "REQUEST_NOT_SUPPORTED" },
    { telux::common::ErrorCode::CANCELLED, "CANCELLED" },
    { telux::common::ErrorCode::OP_NOT_ALLOWED_DURING_VOICE_CALL,
        "OP_NOT_ALLOWED_DURING_VOICE_CALL" },
    { telux::common::ErrorCode::OP_NOT_ALLOWED_BEFORE_REG_TO_NW,
        "OP_NOT_ALLOWED_BEFORE_REG_TO_NW" },
    { telux::common::ErrorCode::SMS_SEND_FAIL_RETRY, "SMS_SEND_FAIL_RETRY" },
    { telux::common::ErrorCode::SIM_ABSENT, "SIM_ABSENT" },
    { telux::common::ErrorCode::SUBSCRIPTION_NOT_AVAILABLE, "SUBSCRIPTION_NOT_AVAILABLE" },
    { telux::common::ErrorCode::MODE_NOT_SUPPORTED, "MODE_NOT_SUPPORTED" },
    { telux::common::ErrorCode::FDN_CHECK_FAILURE, "FDN_CHECK_FAILURE" },
    { telux::common::ErrorCode::ILLEGAL_SIM_OR_ME, "ILLEGAL_SIM_OR_ME" },
    { telux::common::ErrorCode::MISSING_RESOURCE, "MISSING_RESOURCE" },
    { telux::common::ErrorCode::NO_SUCH_ELEMENT, "NO_SUCH_ELEMENT" },
    { telux::common::ErrorCode::DIAL_MODIFIED_TO_USSD, "DIAL_MODIFIED_TO_USSD" },
    { telux::common::ErrorCode::DIAL_MODIFIED_TO_SS, "DIAL_MODIFIED_TO_SS" },
    { telux::common::ErrorCode::DIAL_MODIFIED_TO_DIAL, "DIAL_MODIFIED_TO_DIAL" },
    { telux::common::ErrorCode::USSD_MODIFIED_TO_DIAL, "USSD_MODIFIED_TO_DIAL" },
    { telux::common::ErrorCode::USSD_MODIFIED_TO_SS, "USSD_MODIFIED_TO_SS" },
    { telux::common::ErrorCode::USSD_MODIFIED_TO_USSD, "USSD_MODIFIED_TO_USSD" },
    { telux::common::ErrorCode::SS_MODIFIED_TO_DIAL, "SS_MODIFIED_TO_DIAL" },
    { telux::common::ErrorCode::SS_MODIFIED_TO_USSD, "SS_MODIFIED_TO_USSD" },
    { telux::common::ErrorCode::SUBSCRIPTION_NOT_SUPPORTED, "SUBSCRIPTION_NOT_SUPPORTED" },
    { telux::common::ErrorCode::SS_MODIFIED_TO_SS, "SS_MODIFIED_TO_SS" },
    { telux::common::ErrorCode::LCE_NOT_SUPPORTED, "LCE_NOT_SUPPORTED" },
    { telux::common::ErrorCode::NO_MEMORY, "NO_MEMORY" },
    { telux::common::ErrorCode::INTERNAL_ERR, "INTERNAL_ERR" },
    { telux::common::ErrorCode::SYSTEM_ERR, "SYSTEM_ERR" },
    { telux::common::ErrorCode::MODEM_ERR, "MODEM_ERR" },
    { telux::common::ErrorCode::INVALID_STATE, "INVALID_STATE" },
    { telux::common::ErrorCode::NO_RESOURCES, "NO_RESOURCES" },
    { telux::common::ErrorCode::SIM_ERR, "SIM_ERR" },
    { telux::common::ErrorCode::INVALID_ARGUMENTS, "INVALID_ARGUMENTS" },
    { telux::common::ErrorCode::INVALID_SIM_STATE, "INVALID_SIM_STATE" },
    { telux::common::ErrorCode::INVALID_MODEM_STATE, "INVALID_MODEM_STATE" },
    { telux::common::ErrorCode::INVALID_CALL_ID, "INVALID_CALL_ID" },
    { telux::common::ErrorCode::NO_SMS_TO_ACK, "NO_SMS_TO_ACK" },
    { telux::common::ErrorCode::NETWORK_ERR, "NETWORK_ERR" },
    { telux::common::ErrorCode::REQUEST_RATE_LIMITED, "REQUEST_RATE_LIMITED" },
    { telux::common::ErrorCode::SIM_BUSY, "SIM_BUSY" },
    { telux::common::ErrorCode::SIM_FULL, "SIM_FULL" },
    { telux::common::ErrorCode::NETWORK_REJECT, "NETWORK_REJECT" },
    { telux::common::ErrorCode::OPERATION_NOT_ALLOWED, "OPERATION_NOT_ALLOWED" },
    { telux::common::ErrorCode::EMPTY_RECORD, "EMPTY_RECORD" },
    { telux::common::ErrorCode::INVALID_SMS_FORMAT, "INVALID_SMS_FORMAT" },
    { telux::common::ErrorCode::ENCODING_ERR, "ENCODING_ERR" },
    { telux::common::ErrorCode::INVALID_SMSC_ADDRESS, "INVALID_SMSC_ADDRESS" },
    { telux::common::ErrorCode::NO_SUCH_ENTRY, "NO_SUCH_ENTRY" },
    { telux::common::ErrorCode::NETWORK_NOT_READY, "NETWORK_NOT_READY" },
    { telux::common::ErrorCode::NOT_PROVISIONED, "NOT_PROVISIONED" },
    { telux::common::ErrorCode::NO_SUBSCRIPTION, "NO_SUBSCRIPTION" },
    { telux::common::ErrorCode::NO_NETWORK_FOUND, "NO_NETWORK_FOUND" },
    { telux::common::ErrorCode::DEVICE_IN_USE, "DEVICE_IN_USE" },
    { telux::common::ErrorCode::ABORTED, "ABORTED" },
    { telux::common::ErrorCode::INCOMPATIBLE_STATE, "INCOMPATIBLE_STATE" },
    { telux::common::ErrorCode::NO_EFFECT, "NO_EFFECT" },
    { telux::common::ErrorCode::DEVICE_NOT_READY, "DEVICE_NOT_READY" },
    { telux::common::ErrorCode::MISSING_ARGUMENTS, "MISSING_ARGUMENTS" },
    { telux::common::ErrorCode::MALFORMED_MSG, "MALFORMED_MSG" },
    { telux::common::ErrorCode::INTERNAL, "INTERNAL" },
    { telux::common::ErrorCode::CLIENT_IDS_EXHAUSTED, "CLIENT_IDS_EXHAUSTED" },
    { telux::common::ErrorCode::UNABORTABLE_TRANSACTION, "UNABORTABLE_TRANSACTION" },
    { telux::common::ErrorCode::INVALID_CLIENT_ID, "INVALID_CLIENT_ID" },
    { telux::common::ErrorCode::NO_THRESHOLDS, "NO_THRESHOLDS" },
    { telux::common::ErrorCode::INVALID_HANDLE, "INVALID_HANDLE" },
    { telux::common::ErrorCode::INVALID_PROFILE, "INVALID_PROFILE" },
    { telux::common::ErrorCode::INVALID_PINID, "INVALID_PINID" },
    { telux::common::ErrorCode::INCORRECT_PIN, "INCORRECT_PIN" },
    { telux::common::ErrorCode::CALL_FAILED, "CALL_FAILED" },
    { telux::common::ErrorCode::OUT_OF_CALL, "OUT_OF_CALL" },
    { telux::common::ErrorCode::MISSING_ARG, "MISSING_ARG" },
    { telux::common::ErrorCode::ARG_TOO_LONG, "ARG_TOO_LONG" },
    { telux::common::ErrorCode::INVALID_TX_ID, "INVALID_TX_ID" },
    { telux::common::ErrorCode::OP_NETWORK_UNSUPPORTED, "OP_NETWORK_UNSUPPORTED" },
    { telux::common::ErrorCode::OP_DEVICE_UNSUPPORTED, "OP_DEVICE_UNSUPPORTED" },
    { telux::common::ErrorCode::NO_FREE_PROFILE, "NO_FREE_PROFILE" },
    { telux::common::ErrorCode::INVALID_PDP_TYPE, "INVALID_PDP_TYPE" },
    { telux::common::ErrorCode::INVALID_TECH_PREF, "INVALID_TECH_PREF" },
    { telux::common::ErrorCode::INVALID_PROFILE_TYPE, "INVALID_PROFILE_TYPE" },
    { telux::common::ErrorCode::INVALID_SERVICE_TYPE, "INVALID_SERVICE_TYPE" },
    { telux::common::ErrorCode::INVALID_REGISTER_ACTION, "INVALID_REGISTER_ACTION" },
    { telux::common::ErrorCode::INVALID_PS_ATTACH_ACTION, "INVALID_PS_ATTACH_ACTION" },
    { telux::common::ErrorCode::AUTHENTICATION_FAILED, "AUTHENTICATION_FAILED" },
    { telux::common::ErrorCode::PIN_BLOCKED, "PIN_BLOCKED" },
    { telux::common::ErrorCode::PIN_PERM_BLOCKED, "PIN_PERM_BLOCKED" },
    { telux::common::ErrorCode::SIM_NOT_INITIALIZED, "SIM_NOT_INITIALIZED" },
    { telux::common::ErrorCode::MAX_QOS_REQUESTS_IN_USE, "MAX_QOS_REQUESTS_IN_USE" },
    { telux::common::ErrorCode::INCORRECT_FLOW_FILTER, "INCORRECT_FLOW_FILTER" },
    { telux::common::ErrorCode::NETWORK_QOS_UNAWARE, "NETWORK_QOS_UNAWARE" },
    { telux::common::ErrorCode::INVALID_ID, "INVALID_ID" },
    { telux::common::ErrorCode::REQUESTED_NUM_UNSUPPORTED, "REQUESTED_NUM_UNSUPPORTED" },
    { telux::common::ErrorCode::INTERFACE_NOT_FOUND, "INTERFACE_NOT_FOUND" },
    { telux::common::ErrorCode::FLOW_SUSPENDED, "FLOW_SUSPENDED" },
    { telux::common::ErrorCode::INVALID_DATA_FORMAT, "INVALID_DATA_FORMAT" },
    { telux::common::ErrorCode::GENERAL, "GENERAL" },
    { telux::common::ErrorCode::UNKNOWN, "UNKNOWN" },
    { telux::common::ErrorCode::INVALID_ARG, "INVALID_ARG" },
    { telux::common::ErrorCode::INVALID_INDEX, "INVALID_INDEX" },
    { telux::common::ErrorCode::NO_ENTRY, "NO_ENTRY" },
    { telux::common::ErrorCode::DEVICE_STORAGE_FULL, "DEVICE_STORAGE_FULL" },
    { telux::common::ErrorCode::CAUSE_CODE, "CAUSE_CODE" },
    { telux::common::ErrorCode::MESSAGE_NOT_SENT, "MESSAGE_NOT_SENT" },
    { telux::common::ErrorCode::MESSAGE_DELIVERY_FAILURE, "MESSAGE_DELIVERY_FAILURE" },
    { telux::common::ErrorCode::INVALID_MESSAGE_ID, "INVALID_MESSAGE_ID" },
    { telux::common::ErrorCode::ENCODING, "ENCODING" },
    { telux::common::ErrorCode::AUTHENTICATION_LOCK, "AUTHENTICATION_LOCK" },
    { telux::common::ErrorCode::INVALID_TRANSITION, "INVALID_TRANSITION" },
    { telux::common::ErrorCode::NOT_A_MCAST_IFACE, "NOT_A_MCAST_IFACE" },
    { telux::common::ErrorCode::MAX_MCAST_REQUESTS_IN_USE, "MAX_MCAST_REQUESTS_IN_USE" },
    { telux::common::ErrorCode::INVALID_MCAST_HANDLE, "INVALID_MCAST_HANDLE" },
    { telux::common::ErrorCode::INVALID_IP_FAMILY_PREF, "INVALID_IP_FAMILY_PREF" },
    { telux::common::ErrorCode::SESSION_INACTIVE, "SESSION_INACTIVE" },
    { telux::common::ErrorCode::SESSION_INVALID, "SESSION_INVALID" },
    { telux::common::ErrorCode::SESSION_OWNERSHIP, "SESSION_OWNERSHIP" },
    { telux::common::ErrorCode::INSUFFICIENT_RESOURCES, "INSUFFICIENT_RESOURCES" },
    { telux::common::ErrorCode::DISABLED, "DISABLED" },
    { telux::common::ErrorCode::INVALID_OPERATION, "INVALID_OPERATION" },
    { telux::common::ErrorCode::INVALID_QMI_CMD, "INVALID_QMI_CMD" },
    { telux::common::ErrorCode::TPDU_TYPE, "TPDU_TYPE" },
    { telux::common::ErrorCode::SMSC_ADDR, "SMSC_ADDR" },
    { telux::common::ErrorCode::INFO_UNAVAILABLE, "INFO_UNAVAILABLE" },
    { telux::common::ErrorCode::SEGMENT_TOO_LONG, "SEGMENT_TOO_LONG" },
    { telux::common::ErrorCode::SEGMENT_ORDER, "SEGMENT_ORDER" },
    { telux::common::ErrorCode::BUNDLING_NOT_SUPPORTED, "BUNDLING_NOT_SUPPORTED" },
    { telux::common::ErrorCode::OP_PARTIAL_FAILURE, "OP_PARTIAL_FAILURE" },
    { telux::common::ErrorCode::POLICY_MISMATCH, "POLICY_MISMATCH" },
    { telux::common::ErrorCode::SIM_FILE_NOT_FOUND, "SIM_FILE_NOT_FOUND" },
    { telux::common::ErrorCode::EXTENDED_INTERNAL, "EXTENDED_INTERNAL" },
    { telux::common::ErrorCode::ACCESS_DENIED, "ACCESS_DENIED" },
    { telux::common::ErrorCode::HARDWARE_RESTRICTED, "HARDWARE_RESTRICTED" },
    { telux::common::ErrorCode::ACK_NOT_SENT, "ACK_NOT_SENT" },
    { telux::common::ErrorCode::INJECT_TIMEOUT, "INJECT_TIMEOUT" },
    { telux::common::ErrorCode::FDN_RESTRICT, "FDN_RESTRICT" },
    { telux::common::ErrorCode::SUPS_FAILURE_CAUSE, "SUPS_FAILURE_CAUSE" },
    { telux::common::ErrorCode::NO_RADIO, "NO_RADIO" },
    { telux::common::ErrorCode::NOT_SUPPORTED, "NOT_SUPPORTED" },
    { telux::common::ErrorCode::CARD_CALL_CONTROL_FAILED, "CARD_CALL_CONTROL_FAILED" },
    { telux::common::ErrorCode::NETWORK_ABORTED, "NETWORK_ABORTED" },
    { telux::common::ErrorCode::MSG_BLOCKED, "MSG_BLOCKED" },
    { telux::common::ErrorCode::INVALID_SESSION_TYPE, "INVALID_SESSION_TYPE" },
    { telux::common::ErrorCode::INVALID_PB_TYPE, "INVALID_PB_TYPE" },
    { telux::common::ErrorCode::NO_SIM, "NO_SIM" },
    { telux::common::ErrorCode::PB_NOT_READY, "PB_NOT_READY" },
    { telux::common::ErrorCode::PIN_RESTRICTION, "PIN_RESTRICTION" },
    { telux::common::ErrorCode::PIN2_RESTRICTION, "PIN2_RESTRICTION" },
    { telux::common::ErrorCode::PUK_RESTRICTION, "PUK_RESTRICTION" },
    { telux::common::ErrorCode::PUK2_RESTRICTION, "PUK2_RESTRICTION" },
    { telux::common::ErrorCode::PB_ACCESS_RESTRICTED, "PB_ACCESS_RESTRICTED" },
    { telux::common::ErrorCode::PB_DELETE_IN_PROG, "PB_DELETE_IN_PROG" },
    { telux::common::ErrorCode::PB_TEXT_TOO_LONG, "PB_TEXT_TOO_LONG" },
    { telux::common::ErrorCode::PB_NUMBER_TOO_LONG, "PB_NUMBER_TOO_LONG" },
    { telux::common::ErrorCode::PB_HIDDEN_KEY_RESTRICTION, "PB_HIDDEN_KEY_RESTRICTION" },
    { telux::common::ErrorCode::PB_NOT_AVAILABLE, "PB_NOT_AVAILABLE" },
    { telux::common::ErrorCode::DEVICE_MEMORY_ERROR, "DEVICE_MEMORY_ERROR" },
    { telux::common::ErrorCode::NO_PERMISSION, "NO_PERMISSION" },
    { telux::common::ErrorCode::TOO_SOON, "TOO_SOON" },
    { telux::common::ErrorCode::TIME_NOT_ACQUIRED, "TIME_NOT_ACQUIRED" },
    { telux::common::ErrorCode::OP_IN_PROGRESS, "OP_IN_PROGRESS" },
    { telux::common::ErrorCode::INTERNAL_ERROR, "INTERNAL_ERROR" },
    { telux::common::ErrorCode::SERVICE_ERROR, "SERVICE_ERROR" },
    { telux::common::ErrorCode::TIMEOUT_ERROR, "TIMEOUT_ERROR" },
    { telux::common::ErrorCode::EXTENDED_ERROR, "EXTENDED_ERROR" },
    { telux::common::ErrorCode::PORT_NOT_OPEN_ERROR, "PORT_NOT_OPEN_ERROR" },
    { telux::common::ErrorCode::MEMCOPY_ERROR, "MEMCOPY_ERROR" },
    { telux::common::ErrorCode::INVALID_TRANSACTION, "INVALID_TRANSACTION" },
    { telux::common::ErrorCode::ALLOCATION_FAILURE, "ALLOCATION_FAILURE" },
    { telux::common::ErrorCode::TRANSPORT_ERROR, "TRANSPORT_ERROR" },
    { telux::common::ErrorCode::PARAM_ERROR, "PARAM_ERROR" },
    { telux::common::ErrorCode::INVALID_CLIENT, "INVALID_CLIENT" },
    { telux::common::ErrorCode::FRAMEWORK_NOT_READY, "FRAMEWORK_NOT_READY" },
    { telux::common::ErrorCode::INVALID_SIGNAL, "INVALID_SIGNAL" },
    { telux::common::ErrorCode::TRANSPORT_BUSY_ERROR, "TRANSPORT_BUSY_ERROR" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_FAIL, "DS_PROFILE_REG_RESULT_FAIL" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_HNDL,
        "DS_PROFILE_REG_RESULT_ERR_INVAL_HNDL" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_OP,
        "DS_PROFILE_REG_RESULT_ERR_INVAL_OP" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_TYPE,
        "DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_TYPE" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM,
        "DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_IDENT,
        "DS_PROFILE_REG_RESULT_ERR_INVAL_IDENT" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL,
        "DS_PROFILE_REG_RESULT_ERR_INVAL" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_LIB_NOT_INITED,
        "DS_PROFILE_REG_RESULT_ERR_LIB_NOT_INITED" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_LEN_INVALID,
        "DS_PROFILE_REG_RESULT_ERR_LEN_INVALID" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_LIST_END,
        "DS_PROFILE_REG_RESULT_LIST_END" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_INVAL_SUBS_ID,
        "DS_PROFILE_REG_RESULT_ERR_INVAL_SUBS_ID" },
    { telux::common::ErrorCode::DS_PROFILE_REG_INVAL_PROFILE_FAMILY,
        "DS_PROFILE_REG_INVAL_PROFILE_FAMILY" },
    { telux::common::ErrorCode::DS_PROFILE_REG_PROFILE_VERSION_MISMATCH,
        "DS_PROFILE_REG_PROFILE_VERSION_MISMATCH" },
    { telux::common::ErrorCode::REG_RESULT_ERR_OUT_OF_MEMORY, "REG_RESULT_ERR_OUT_OF_MEMORY" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_FILE_ACCESS,
        "DS_PROFILE_REG_RESULT_ERR_FILE_ACCESS" },
    { telux::common::ErrorCode::DS_PROFILE_REG_RESULT_ERR_EOF,
        "DS_PROFILE_REG_RESULT_ERR_EOF" },
    { telux::common::ErrorCode::REG_RESULT_ERR_VALID_FLAG_NOT_SET,
        "REG_RESULT_ERR_VALID_FLAG_NOT_SET" },
    { telux::common::ErrorCode::REG_RESULT_ERR_OUT_OF_PROFILES,
        "REG_RESULT_ERR_OUT_OF_PROFILES" },
    { telux::common::ErrorCode::REG_RESULT_NO_EMERGENCY_PDN_SUPPORT,
        "REG_RESULT_NO_EMERGENCY_PDN_SUPPORT" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP_INVAL_PROFILE_FAMILY,
        "DS_PROFILE_3GPP_INVAL_PROFILE_FAMILY" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP_ACCESS_ERR, "DS_PROFILE_3GPP_ACCESS_ERR" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP_CONTEXT_NOT_DEFINED,
        "DS_PROFILE_3GPP_CONTEXT_NOT_DEFINED" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP_VALID_FLAG_NOT_SET,
        "DS_PROFILE_3GPP_VALID_FLAG_NOT_SET" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP_READ_ONLY_FLAG_SET,
        "DS_PROFILE_3GPP_READ_ONLY_FLAG_SET" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP_ERR_OUT_OF_PROFILES,
        "DS_PROFILE_3GPP_ERR_OUT_OF_PROFILES" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP2_ERR_INVALID_IDENT_FOR_PROFILE,
        "DS_PROFILE_3GPP2_ERR_INVALID_IDENT_FOR_PROFILE" },
    { telux::common::ErrorCode::DS_PROFILE_3GPP2_ERR_OUT_OF_PROFILE,
        "DS_PROFILE_3GPP2_ERR_OUT_OF_PROFILE" }
};

/**
 * Error descripton
 */
static std::string getErrorCodeAsString
(
    telux::common::ErrorCode error
)
{
    if (errorCodeToStringMap_.find(error) != std::end(errorCodeToStringMap_))
    {
        return errorCodeToStringMap_[error];
    }
    return "UNKNOWN_ERROR";
}

static std::string convertTagTypeToString
(
    telux::tel::SmsTagType type
)
{
    switch (type)
    {
        case telux::tel::SmsTagType::UNKNOWN:
            return "Unknown";
        case telux::tel::SmsTagType::MT_READ:
            return "MT_READ";
        case telux::tel::SmsTagType::MT_NOT_READ:
            return "MT_NOT_READ";
        default:
            return "Unknown";
    }
}

class SmsPAController
{
public:
    static std::shared_ptr<SmsPAController> getInstance()
    {
        static std::shared_ptr<SmsPAController> instance = std::make_shared<SmsPAController>();
        return instance;
    }

    std::vector<std::shared_ptr<telux::tel::ISmsManager>> getSmsManagersList()
    {
        return smsManagers;
    }

    std::vector<std::shared_ptr<telux::tel::ICellBroadcastManager>> getCbManagersList()
    {
        return CbManagers;
    }

    pa_result_t initialize();
    pa_result_t deinitialize();

    void setIncomingSmsCallback(IncomingSmsCallback cb)
    {
        std::lock_guard<std::mutex> lk(cbMutex);
        incomingSmsCb = cb;
    }

    void setMemoryFullCallback(MemoryFullCallback cb)
    {
        std::lock_guard<std::mutex> lk(cbMutex);
        memoryFullCb = cb;
    }

    class tafSmsListener : public telux::tel::ISmsListener
    {
    public:
        void onMemoryFull(int phoneId, telux::tel::StorageType type) override;
        void onIncomingSms(
            int phoneId, std::shared_ptr<telux::tel::SmsMessage> message) override;
    };

    class tafSmscAddressCallback : public telux::tel::ISmscAddressCallback
    {
    public:
        tafSmscAddressCallback(std::shared_ptr<std::promise<pa_result_t>> promise,
                               std::shared_ptr<std::string> addrStr)
            : promise_(std::move(promise)), addrStr_(std::move(addrStr))
        {
        }

        void smscAddressResponse(
            const std::string &address, telux::common::ErrorCode error) override
        {
            try
            {
                if (error == telux::common::ErrorCode::SUCCESS)
                {
                    // Store the address; the caller will copy into its buffer after wait
                    *addrStr_ = address;
                    promise_->set_value(PA_OK);
                }
                else
                {
                    promise_->set_value(PA_FAULT);
                }
            }
            catch (const std::exception &e)
            {
                PA_ERROR("Exception in callback: %s", e.what());
                try { promise_->set_value(PA_FAULT); } catch (...) {}
            }
            catch (...)
            {
                PA_ERROR("Unknown error in SMS callback.");
                try { promise_->set_value(PA_FAULT); } catch (...) {}
            }
        }

    private:
        std::shared_ptr<std::promise<pa_result_t>> promise_;
        std::shared_ptr<std::string> addrStr_;
    };

    class tafSetSmscAddressCallback : public telux::tel::ISmscAddressCallback
    {
    public:
        tafSetSmscAddressCallback(std::promise<pa_result_t>* promise)
            : promise_(promise)
        {
        }

        void smscAddressResponse(
            const std::string& address, telux::common::ErrorCode error) override
        {
            try
            {
                if (error == telux::common::ErrorCode::SUCCESS)
                {
                    promise_->set_value(PA_OK);
                }
                else
                {
                    promise_->set_value(PA_FAULT);
                }
            }
            catch (const std::exception& e)
            {
                PA_ERROR("Exception in callback: %s", e.what());
            }
            catch (...)
            {
                PA_ERROR("Unknown error in SMS callback.");
            }
        }

    private:
        std::promise<pa_result_t>* promise_;
    };

    std::shared_ptr<tafSmsListener> mySmsListener;
    IncomingSmsCallback incomingSmsCb;
    MemoryFullCallback memoryFullCb;

    // Protects callback registration/access
    std::mutex cbMutex;

    // for cell broadcast
    std::vector<telux::tel::CellBroadcastFilter> CBFilterList;
    std::mutex cbFilterMutex;

    SmsPAController() = default;
    ~SmsPAController() = default;

private:
    SmsPAController(const SmsPAController&) = delete;

    SmsPAController& operator=(const SmsPAController&) = delete;

    uint8_t NumOfSlot = MIN_SIM_SLOT_COUNT;

    std::vector<std::shared_ptr<telux::tel::ISmsManager>> smsManagers;

    std::vector<std::shared_ptr<telux::tel::ICellBroadcastManager>> CbManagers;
};

void SmsPAController::tafSmsListener::onMemoryFull
(
    int phoneId,
    telux::tel::StorageType type
)
{
    PA_INFO("onMemoryFull, phoneId %d", phoneId);
    taf_pa_sms_StorageFullType fullType = taf_pa_sms_StorageFullType::TAF_PA_FULL_UNKNOWN;
    switch (type)
    {
        case telux::tel::StorageType::SIM:
            if (phoneId == 1)
            {
                fullType = taf_pa_sms_StorageFullType::TAF_PA_FULL_SIM;
            }
            else
            {
                fullType = taf_pa_sms_StorageFullType::TAF_PA_FULL_SIM2;
            }
            break;

        default:
            break;
    }

    auto pACtrl = SmsPAController::getInstance();
    MemoryFullCallback cbCopy;
    {
        std::lock_guard<std::mutex> lk(pACtrl->cbMutex);
        cbCopy = pACtrl->memoryFullCb;
    }
    if (cbCopy)
    {
        cbCopy(phoneId, fullType);
    }
}

void SmsPAController::tafSmsListener::onIncomingSms
(
    int phoneId,
    std::shared_ptr<telux::tel::SmsMessage> smsMsg
)
{
    if(smsMsg == nullptr)
    {
        PA_ERROR("smsMsg is nullptr!");
        return;
    }

    // getRawPdu() returns the ASCII bytes of the hex PDU string (rawPdu_ is constructed as
    // vector<uint8_t>(hexString.begin(), hexString.end()) in SmsHelper::createSmsMessage).
    // Reconstructing the hex string via string(begin, end) is equivalent to getPdu().
    telux::tel::PduBuffer rawPduBytes = smsMsg->getRawPdu();
    std::string pdu(rawPduBytes.begin(), rawPduBytes.end());
    std::string sender = smsMsg->getSender();

    PA_DEBUG("Received SMS from phone ID %d from: %s", phoneId, sender.c_str());
    PA_DEBUG("message: %s", smsMsg->getText().c_str());

    int storageIdx = -1;

    // If preferred storage is SIM, get SIM index
    telux::tel::SmsMetaInfo metaInfo;
    if (smsMsg->getMetaInfo(metaInfo) == telux::common::Status::SUCCESS)
    {
        storageIdx = metaInfo.msgIndex;
    }

    auto pACtrl = SmsPAController::getInstance();

    // Notify the registered callback (service layer)
    IncomingSmsCallback cbCopy;
    {
        std::lock_guard<std::mutex> lk(pACtrl->cbMutex);
        cbCopy = pACtrl->incomingSmsCb;
    }
    if (cbCopy)
    {
        cbCopy(phoneId, pdu, sender, storageIdx);
    }
}

pa_result_t tafpa::sms::taf_pa_sms_RegisterIncomingSmsCallback
(
    IncomingSmsCallback cb
)
{
    PA_INFO("taf_pa_sms_RegisterIncomingSmsCallback");
    auto pACtrl = SmsPAController::getInstance();
    pACtrl->setIncomingSmsCallback(cb);
  return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_RegisterMemoryFullCallback
(
    MemoryFullCallback cb
)
{
    PA_INFO("taf_pa_sms_RegisterMemoryFullCallback");
    auto pACtrl = SmsPAController::getInstance();
    pACtrl->setMemoryFullCallback(cb);
  return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_SetActivationStatus
(
    uint8_t phoneId,
    bool activate,
    uint32_t timeout
)
{
    PA_INFO("taf_pa_sms_SetActivationStatus");
    auto pACtrl = SmsPAController::getInstance();
    auto cbManagers = pACtrl->getCbManagersList();
    if (cbManagers.empty() || phoneId < 1 || phoneId > cbManagers.size())
    {
        PA_ERROR("Invalid phoneId or CellBroadcastManager not available");
        return PA_FAULT;
    }

    auto cbMgr = cbManagers[phoneId - 1];
    if (!cbMgr)
    {
        PA_ERROR("CellBroadcastManager is NULL");
        return PA_FAULT;
    }

    // Check service status before using
    telux::common::ServiceStatus status = cbMgr->getServiceStatus();
    if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_ERROR("CellBroadcastManager not available for slot %d, status: %d",
            phoneId, static_cast<int>(status));
        return PA_FAULT;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](telux::common::ErrorCode err)
    {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                promisePtr->set_value(PA_OK);
            }
            else
            {
                promisePtr->set_value(PA_FAULT);
            }
        }
        catch (...)
        {
            PA_ERROR("Exception in cell broadcast activation callback");
        }
    };

    telux::common::Status reqStatus = cbMgr->setActivationStatus(activate, cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("setActivationStatus request failed to send");
        return PA_FAULT;
    }

    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (waitStatus == std::future_status::timeout)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }
    return futResult.get();
}

pa_result_t tafpa::sms::taf_pa_sms_RequestSmsMessageList
(
    uint32_t* idxArray,
    size_t idxArraySize,
    uint32_t timeout,
    taf_pa_sms_Tag tagType,
    uint8_t phoneId,
    int32_t* countPtr
)
{
    PA_INFO("taf_pa_sms_RequestSmsMessageList");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    telux::tel::SmsTagType smsTagType;
    if (tagType == taf_pa_sms_Tag::TAF_PA_READ)
    {
        smsTagType = telux::tel::SmsTagType::MT_READ;
    }
    else if (tagType == taf_pa_sms_Tag::TAF_PA_NOT_READ)
    {
        smsTagType = telux::tel::SmsTagType::MT_NOT_READ;
    }
    else
    {
        smsTagType = telux::tel::SmsTagType::UNKNOWN;
    }

    uint32_t numOfIdx = 0;

    auto prom = std::make_shared<std::promise<std::vector<telux::tel::SmsMetaInfo>>>();
    auto cb = [prom](std::vector<telux::tel::SmsMetaInfo> infos, telux::common::ErrorCode err) {
        try
        {
            if (err != telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Request for message list failed with errorCode: %d",
                    static_cast<int>(err));
                // Ensure the promise is fulfilled even on error to avoid unnecessary timeout waits
                prom->set_value({});
                return;
            }
            PA_INFO("Request for message list sent successfully ");
            PA_INFO("SMS List Size: %zu", infos.size());
            for (auto& info : infos)
            {
                PA_INFO(" Msg Index: %d, Tag Type: %s", info.msgIndex,
                    convertTagTypeToString(info.tagType).c_str());
            }
            prom->set_value(infos);
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
            try
            {
                prom->set_value({});
            }
            catch (...)
            {
            }
        }
        catch (...)
        {
            PA_ERROR("Unknown error in SMS callback.");
            try
            {
                prom->set_value({});
            }
            catch (...)
            {
            }
        }
    };

    std::chrono::seconds span(timeout);
    auto status = smsManager->requestSmsMessageList(smsTagType, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_INFO("requestSmsMessageList failed");
        return PA_FAULT;
    }

    std::future<std::vector<telux::tel::SmsMetaInfo>> futResult = prom->get_future();
    std::future_status waitStatus = futResult.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_FAULT;
    }
    else
    {
        std::vector<telux::tel::SmsMetaInfo> infos = futResult.get();
        numOfIdx = infos.size();

        // Allow exact-size matches; prevent overflow
        if (static_cast<size_t>(numOfIdx) > idxArraySize)
        {
            PA_ERROR("Too many SMS to read %u (capacity: %zu)", numOfIdx, idxArraySize);
            return PA_FAULT;
        }

        for (uint32_t idx = 0; idx < numOfIdx; ++idx)
        {
            idxArray[idx] = infos[idx].msgIndex;
        }
    }

    if (countPtr) *countPtr = static_cast<int32_t>(numOfIdx);
    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_GetSmsCenterAddress
(
    uint8_t phoneId,
    char *addr,
    size_t len,
    uint32_t timeout
)
{
    PA_INFO("taf_pa_sms_GetSmsCenterAddress");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_ERROR("smsManager is NULL");
        return PA_FAULT;
    }

    auto prom = std::make_shared<std::promise<pa_result_t>>();
    auto addrStr = std::make_shared<std::string>();
    auto cb = std::make_shared<SmsPAController::tafSmscAddressCallback>(prom, addrStr);

    telux::common::Status reqStatus = smsManager->requestSmscAddress(cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("requestSmscAddress failed to send");
        return PA_FAULT;
    }

    std::future<pa_result_t> futResult = prom->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (waitStatus == std::future_status::timeout)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    pa_result_t rc = futResult.get();
    if (rc != PA_OK)
    {
        return rc;
    }

    // Copy into caller buffer, respecting len
    if (addr == nullptr || len == 0)
    {
        PA_ERROR("Invalid output buffer");
        return PA_FAULT;
    }

    if (addrStr->size() >= len)
    {
        PA_ERROR("Output buffer too small: need %zu, have %zu", addrStr->size() + 1, len);
        return PA_OVERFLOW;
    }

    size_t n = addrStr->copy(addr, len - 1);
    addr[n] = '\0';

    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_SetSmsCenterAddress
(
    uint8_t phoneId,
    const char* addr,
    uint32_t timeout
)
{
    PA_INFO("taf_pa_sms_SetSmsCenterAddress");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_ERROR("smsManager is NULL");
        return PA_FAULT;
    }

    std::shared_ptr<std::promise<pa_result_t>> prom = std::make_shared<std::promise<pa_result_t>>();
    telux::common::ResponseCallback callback = [prom](telux::common::ErrorCode error)
    {
        try
        {
            if (error == telux::common::ErrorCode::SUCCESS)
            {
                prom->set_value(PA_OK);
            }
            else
            {
                prom->set_value(PA_FAULT);
            }
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    telux::common::Status reqStatus = smsManager->setSmscAddress(addr, callback);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("setSmscAddress failed to send");
        return PA_FAULT;
    }

    std::future<pa_result_t> futResult = prom->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (waitStatus == std::future_status::timeout)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }
    return futResult.get();
}

pa_result_t tafpa::sms::taf_pa_sms_RequestMessageFilters
(
    uint8_t phoneId,
    uint32_t timeout
)
{
    auto pACtrl = SmsPAController::getInstance();
    auto cbManagers = pACtrl->getCbManagersList();
    if (cbManagers.empty() || phoneId < 1 || phoneId > cbManagers.size())
    {
        PA_ERROR("Invalid phoneId or CellBroadcastManager not available");
        return PA_FAULT;
    }

    auto cbMgr = cbManagers[phoneId - 1];
    if (!cbMgr)
    {
        PA_ERROR("CellBroadcastManager is NULL");
        return PA_FAULT;
    }

    // Check service status before using
    telux::common::ServiceStatus status = cbMgr->getServiceStatus();
    if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_ERROR("CellBroadcastManager not available for slot %d, status: %d",
            phoneId, static_cast<int>(status));
        return PA_FAULT;
    }

    auto prom = std::make_shared<std::promise<std::vector<telux::tel::CellBroadcastFilter>>>();
    auto cb = [prom](std::vector<telux::tel::CellBroadcastFilter> filters,
                     telux::common::ErrorCode err) {
        std::vector<telux::tel::CellBroadcastFilter> tafFilters;
        if (err == telux::common::ErrorCode::SUCCESS)
        {
            for (int i = 0; i < (int)filters.size(); ++i)
            {
                PA_INFO("Filter[%d]:", i);
                PA_INFO("Start msg id: %d", filters[i].startMessageId);
                PA_INFO("End msg id: %d", filters[i].endMessageId);

                telux::tel::CellBroadcastFilter tafF;
                tafF.startMessageId = filters[i].startMessageId;
                tafF.endMessageId = filters[i].endMessageId;
                tafFilters.push_back(tafF);
            }
            prom->set_value(std::move(tafFilters));
        }
        else
        {
            prom->set_value({});
        }
    };

    telux::common::Status reqStatus = cbMgr->requestMessageFilters(cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("requestMessageFilters request failed to send");
        return PA_FAULT;
    }

    std::future<std::vector<telux::tel::CellBroadcastFilter>> futResult = prom->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (waitStatus == std::future_status::timeout)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    auto filters = futResult.get();
    {
        std::lock_guard<std::mutex> lk(pACtrl->cbFilterMutex);
        pACtrl->CBFilterList = std::move(filters);
    }
    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_AddCellBroadcastIds
(
    uint8_t phoneId,
    uint16_t fromId,
    uint16_t toId,
    uint32_t timeout
)
{
    auto pACtrl = SmsPAController::getInstance();
    auto cbManagers = pACtrl->getCbManagersList();
    if (cbManagers.empty() || phoneId < 1 || phoneId > cbManagers.size())
    {
        PA_ERROR("Invalid phoneId or CellBroadcastManager not available");
        return PA_FAULT;
    }

    auto cbMgr = cbManagers[phoneId - 1];
    if (!cbMgr)
    {
        PA_ERROR("CellBroadcastManager is NULL");
        return PA_FAULT;
    }

    // Check service status before using
    telux::common::ServiceStatus status = cbMgr->getServiceStatus();
    if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_ERROR("CellBroadcastManager not available for slot %d, status: %d",
            phoneId, static_cast<int>(status));
        return PA_FAULT;
    }

    // Add new filter to current filter list
    telux::tel::CellBroadcastFilter filter = {};
    filter.startMessageId = fromId;
    filter.endMessageId = toId;
    std::vector<telux::tel::CellBroadcastFilter> newList;
    {
        std::lock_guard<std::mutex> lk(pACtrl->cbFilterMutex);
        newList = pACtrl->CBFilterList;
        newList.emplace_back(filter);
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](telux::common::ErrorCode err) {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Update message filters request sent successfully");
                promisePtr->set_value(PA_OK);
            }
            else
            {
                PA_INFO("Update message filters request failed with errorCode: %d",
                    static_cast<int>(err));
                promisePtr->set_value(PA_FAULT);
            }
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    telux::common::Status reqStatus = cbMgr->updateMessageFilters(newList, cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Update message filters failed");
        return PA_FAULT;
    }

    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (waitStatus == std::future_status::timeout)
    {
        PA_ERROR("Waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    pa_result_t rc = futResult.get();
    if (rc == PA_OK)
    {
        // Commit on success
        std::lock_guard<std::mutex> lk(pACtrl->cbFilterMutex);
        pACtrl->CBFilterList = std::move(newList);
    }
    return rc;
}

pa_result_t tafpa::sms::taf_pa_sms_RemoveCellBroadcastIds
(
    uint8_t phoneId,
    uint16_t fromId,
    uint16_t toId,
    uint32_t timeout
)
{
    auto pACtrl = SmsPAController::getInstance();
    auto cbManagers = pACtrl->getCbManagersList();
    if (cbManagers.empty() || phoneId < 1 || phoneId > cbManagers.size())
    {
        PA_ERROR("Invalid phoneId or CellBroadcastManager not available");
        return PA_FAULT;
    }

    auto cbMgr = cbManagers[phoneId - 1];
    if (!cbMgr)
    {
        PA_ERROR("CellBroadcastManager is NULL");
        return PA_FAULT;
    }

    // Check service status before using
    telux::common::ServiceStatus status = cbMgr->getServiceStatus();
    if (status != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        PA_ERROR("CellBroadcastManager not available for slot %d, status: %d",
             phoneId, static_cast<int>(status));
        return PA_FAULT;
    }

    // Work on a local copy under lock to avoid races
    std::vector<telux::tel::CellBroadcastFilter> current;
    {
        std::lock_guard<std::mutex> lk(pACtrl->cbFilterMutex);
        current = pACtrl->CBFilterList;
    }

    bool overlapped = false;
    std::vector<telux::tel::CellBroadcastFilter> result;
    result.reserve(current.size());

    // Robust overlap handling without under/overflow
    for (const auto &cur : current)
    {
        // No overlap: keep the filter as-is
        if (toId < cur.startMessageId || fromId > cur.endMessageId)
        {
            result.push_back(cur);
            continue;
        }

        overlapped = true;

        // Left segment: [cur.startMessageId, fromId - 1] if fromId > cur.startMessageId
        if (fromId > cur.startMessageId)
        {
            telux::tel::CellBroadcastFilter left{};
            left.startMessageId = cur.startMessageId;
            left.endMessageId = static_cast<uint16_t>(fromId - 1);
            if (left.startMessageId <= left.endMessageId)
            {
                result.emplace_back(left);
            }
        }

        // Right segment: [toId + 1, cur.endMessageId] if toId < cur.endMessageId
        if (toId < cur.endMessageId)
        {
            // Guard overflow: only add right if toId < max uint16_t
            if (toId < std::numeric_limits<uint16_t>::max())
            {
                telux::tel::CellBroadcastFilter right{};
                right.startMessageId = static_cast<uint16_t>(toId + 1);
                right.endMessageId = cur.endMessageId;
                if (right.startMessageId <= right.endMessageId)
                {
                    result.emplace_back(right);
                }
            }
            // If toId == 0xFFFF, there can be no right segment
        }
        // If input fully covers 'cur', nothing is pushed
    }

    if (!overlapped)
    {
        return PA_OK;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](telux::common::ErrorCode err)
    {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Update message filters request sent successfully");
                promisePtr->set_value(PA_OK);
            }
            else
            {
                PA_INFO("Update message filters request failed with errorCode: %d",
                        static_cast<int>(err));
                promisePtr->set_value(PA_FAULT);
            }
        }
        catch (const std::exception &e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    telux::common::Status reqStatus = cbMgr->updateMessageFilters(result, cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_INFO("Update message filters failed");
        return PA_FAULT;
    }

    // blocking here to get call event response
    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("Waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    pa_result_t rc = futResult.get();
    if (rc == PA_OK)
    {
        // Commit on success
        std::lock_guard<std::mutex> lk(pACtrl->cbFilterMutex);
        pACtrl->CBFilterList = std::move(result);
    }
    return rc;
}

pa_result_t tafpa::sms::taf_pa_sms_GetPreferredStorage
(
    taf_pa_sms_Storage* type,
    uint32_t timeout,
    uint8_t phoneId
)
{
    PA_INFO("taf_pa_sms_GetPreferredStorage");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    auto resPtr = std::make_shared<std::promise<pa_result_t>>();
    auto typePtr = std::make_shared<std::promise<taf_pa_sms_Storage>>();
    auto cb = [resPtr, typePtr](telux::tel::StorageType strType, telux::common::ErrorCode err) {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Request for get preferred storage sent successfully");
                switch (strType)
                {
                    case telux::tel::StorageType::NONE:
                        typePtr->set_value(taf_pa_sms_Storage::TAF_PA_STORAGE_NONE);
                        break;
                    case telux::tel::StorageType::SIM:
                        typePtr->set_value(taf_pa_sms_Storage::TAF_PA_STORAGE_SIM);
                        break;
                    default:
                        typePtr->set_value(taf_pa_sms_Storage::TAF_PA_STORAGE_UNKNOWN);
                        break;
                }
                resPtr->set_value(PA_OK);
            }
            else
            {
                PA_INFO("Request for get preferred storage failed with errorCode: %d",
                    static_cast<int>(err));
                resPtr->set_value(PA_FAULT);
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
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    telux::common::Status reqStatus = smsManager->requestPreferredStorage(cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_INFO("Get preferred storage failed");
        return PA_FAULT;
    }

    std::future<pa_result_t> resultFuture = resPtr->get_future();
    std::future<taf_pa_sms_Storage> typeFuture = typePtr->get_future();

    std::chrono::seconds span(timeout);
    if (resultFuture.wait_for(span) == std::future_status::ready
        && typeFuture.wait_for(span) == std::future_status::ready)
    {
        pa_result_t res = resultFuture.get();
        if (res == PA_OK)
        {
            *type = typeFuture.get();
        }
        return res;
    }
    else
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }
}

pa_result_t tafpa::sms::taf_pa_sms_SetPreferredStorage
(
    taf_pa_sms_Storage type,
    uint32_t timeout,
    uint8_t phoneId
)
{
    PA_INFO("taf_pa_sms_SetPreferredStorage");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    telux::tel::StorageType storage;
    switch (type)
    {
        case taf_pa_sms_Storage::TAF_PA_STORAGE_NONE:
        case taf_pa_sms_Storage::TAF_PA_STORAGE_HLOS:
            storage = telux::tel::StorageType::NONE;
            break;
        case taf_pa_sms_Storage::TAF_PA_STORAGE_SIM:
            storage = telux::tel::StorageType::SIM;
            break;
        default:
            return PA_UNSUPPORTED;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](telux::common::ErrorCode err) {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Request for set preferred storage sent successfully");
                promisePtr->set_value(PA_OK);
            }
            else
            {
                PA_INFO("Request for set preferred storage failed with errorCode: %d",
                    static_cast<int>(err));
                promisePtr->set_value(PA_FAULT);
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
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    telux::common::Status reqStatus = smsManager->setPreferredStorage(storage, cb);
    if (reqStatus != telux::common::Status::SUCCESS)
    {
        PA_INFO("Set preferred storage failed");
        return PA_FAULT;
    }

    // blocking here to set preferred storage
    std::chrono::seconds span(timeout);
    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::future_status waitStatus = futResult.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    return futResult.get();
}

pa_result_t tafpa::sms::taf_pa_sms_ReadMessage
(
    uint32_t readAtIdx,
    uint32_t timeout,
    uint8_t phoneId,
    taf_pa_sms_Tag *pduRxStatus,
    std::vector<uint8_t> &pduBuffer,
    uint32_t *pduMsgIndex
)
{
    PA_INFO("taf_pa_sms_ReadMessage");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    auto promisePtr = std::make_shared<std::promise<telux::tel::SmsMessage>>();
    auto cb = [promisePtr](telux::tel::SmsMessage smsMsg, telux::common::ErrorCode err)
    {
        try
        {
            if (err != telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Request for read message failed with errorCode: %d",
                        static_cast<int>(err));
                try
                {
                    promisePtr->set_exception(std::make_exception_ptr(
                        std::runtime_error("readMessage failed")));
                }
                catch (...)
                {
                }
                return;
            }
            std::shared_ptr<telux::tel::MessagePartInfo> partInfo = smsMsg.getMessagePartInfo();

            // getRawPdu() holds ASCII bytes of the hex string; reconstruct for logging.
            telux::tel::PduBuffer rawPdu = smsMsg.getRawPdu();
            std::string pduStr(rawPdu.begin(), rawPdu.end());

            if (partInfo)
            {
                PA_INFO("Multi Part Message ");
                PA_INFO("Message: %s", smsMsg.getText().c_str());
                PA_DEBUG("PDU: %s", pduStr.c_str());
                PA_DEBUG("RefNumber: %d", static_cast<int>(partInfo->refNumber));
                PA_DEBUG("NumberOfSegments: %d", static_cast<int>(partInfo->numberOfSegments));
                PA_DEBUG("SegmentNumber: %d", static_cast<int>(partInfo->segmentNumber));
            }
            else
            {
                PA_INFO("Message: %s", smsMsg.getText().c_str());
                PA_DEBUG("PDU: %s", pduStr.c_str());
            }
            promisePtr->set_value(smsMsg);
        }
        catch (const std::exception &e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
            try
            {
                promisePtr->set_exception(std::make_exception_ptr(e));
            }
            catch (...)
            {
            }
        }
        catch (...)
        {
            PA_ERROR("Unknown error in SMS callback.");
            try
            {
                promisePtr->set_exception(std::make_exception_ptr(
                    std::runtime_error("Unknown error in SMS callback")));
            }
            catch (...)
            {
            }
        }
    };

    telux::common::Status status = smsManager->readMessage(readAtIdx, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Read message request failed");
        return PA_FAULT;
    }

    PA_INFO("Read message request succeeded");
    std::future<telux::tel::SmsMessage> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    try
    {
        telux::tel::SmsMessage smsMsg = futResult.get();

        // getRawPdu() holds ASCII bytes of the hex PDU string. Decode hex pairs to binary bytes.
        telux::tel::PduBuffer rawPdu = smsMsg.getRawPdu();
        std::string pduStr(rawPdu.begin(), rawPdu.end());

        for (unsigned int i = 0; i + 1 < pduStr.length(); i += 2)
        {
            unsigned int num = 0;
            std::stringstream ss;
            ss << std::hex << pduStr.substr(i, 2);
            ss >> num;
            pduBuffer.push_back(static_cast<uint8_t>(num & 0xFF));
        }

        telux::tel::SmsMetaInfo metaInfo;
        status = smsMsg.getMetaInfo(metaInfo);
        if (status != telux::common::Status::SUCCESS)
        {
            PA_ERROR("getMetaInfo failed");
            return PA_FAULT;
        }
        *pduMsgIndex = metaInfo.msgIndex;
        switch (metaInfo.tagType)
        {
        case telux::tel::SmsTagType::MT_READ:
            *pduRxStatus = taf_pa_sms_Tag::TAF_PA_READ;
            break;
        case telux::tel::SmsTagType::MT_NOT_READ:
            *pduRxStatus = taf_pa_sms_Tag::TAF_PA_NOT_READ;
            break;
        default:
            *pduRxStatus = taf_pa_sms_Tag::TAF_PA_UNKNOWN;
        }

        return PA_OK;
    }
    catch (const std::exception &e)
    {
        PA_ERROR("readMessage failed: %s", e.what());
        return PA_FAULT;
    }
    catch (...)
    {
        PA_ERROR("readMessage failed: unknown exception");
        return PA_FAULT;
    }
}

pa_result_t tafpa::sms::taf_pa_sms_SendRawSms
(
    uint8_t* pduData,
    uint32_t pduLength,
    uint32_t timeout,
    uint8_t phoneId
)
{
    PA_INFO("taf_pa_sms_SendRawSms");
    if (pduData == nullptr || pduLength == 0)
    {
        PA_ERROR("Invalid input: pduData is null or pduLength is 0");
        return PA_FAULT;
    }

    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d", phoneId);
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (uint32_t i = 0; i < pduLength; ++i)
    {
        oss << std::setw(2) << static_cast<int>(pduData[i]);
    }

    std::string pduStr = oss.str();
    PA_DEBUG("pduStr = %s", pduStr.c_str());

    std::vector<uint8_t> buffer(pduStr.begin(), pduStr.end());
    std::vector<telux::tel::PduBuffer> rawPdus;
    rawPdus.emplace_back(buffer);

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](std::vector<int> msgIDs, telux::common::ErrorCode err) {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO(
                    "SMS sent successfully. Number of MsgIDs: %u", (unsigned int)msgIDs.size());
                for (unsigned int i = 0; i < (unsigned int)msgIDs.size(); ++i)
                {
                    PA_INFO("MsgID[%u]: %d", i, msgIDs[i]);
                }
                promisePtr->set_value(PA_OK);
            }
            else
            {
                PA_ERROR("Error Code: %s", getErrorCodeAsString(err).c_str());
                promisePtr->set_value(PA_FAULT);
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
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    auto status = smsManager->sendRawSms(rawPdus, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_INFO("SMS was not sent, a failure occured");
        return PA_FAULT;
    }

    PA_INFO("Waiting for SMS response or timeout...");
    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (waitStatus == std::future_status::timeout)
    {
        PA_ERROR("SMS send timed out after %u seconds", timeout);
        return PA_TIMEOUT;
    }

    pa_result_t res = futResult.get();
    if (res != PA_OK)
    {
        PA_INFO("SMS sending failed");
        return PA_FAULT;
    }

    PA_INFO("SMS was sent successfully");
    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_SendPDUMessageAsync
(
    uint8_t phoneId,
    const uint8_t* pduData,
    size_t pduLength,
    std::function<void(pa_result_t)> cb
)
{
    PA_INFO("taf_pa_sms_SendPDUMessageAsync");

    if (pduData == nullptr || pduLength == 0)
    {
        PA_ERROR("Invalid input: pduData is null or pduLength is 0");
        if (cb)
            cb(PA_FAULT);
        return PA_FAULT;
    }

    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        cb(PA_FAULT);
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        cb(PA_FAULT);
        return PA_FAULT;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < pduLength; ++i)
    {
        oss << std::setw(2) << static_cast<int>(pduData[i]);
    }
    std::string pduStr = oss.str();
    PA_DEBUG("pduStr = %s", pduStr.c_str());
    std::vector<uint8_t> buffer(pduStr.begin(), pduStr.end());
    std::vector<telux::tel::PduBuffer> rawPdus;
    rawPdus.emplace_back(buffer);

    smsManager->sendRawSms(
        rawPdus, [cb](std::vector<int> msgIds, telux::common::ErrorCode err) {
            pa_result_t result = (err == telux::common::ErrorCode::SUCCESS) ? PA_OK : PA_FAULT;
            if (cb)
                cb(result);
        });
    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_SetTag
(
    uint32_t msgIndex,
    taf_pa_sms_Tag tagType,
    uint32_t timeout,
    uint8_t phoneId
)
{
    PA_INFO("taf_pa_sms_SetTag");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](telux::common::ErrorCode err) {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("Set tag successfully done");
                promisePtr->set_value(PA_OK);
            }
            else
            {
                PA_INFO("Set tag failed, errorCode: %d", static_cast<int>(err));
                promisePtr->set_value(PA_FAULT);
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
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    telux::tel::SmsTagType teluxTagType;
    if (tagType == taf_pa_sms_Tag::TAF_PA_READ)
    {
        teluxTagType = telux::tel::SmsTagType::MT_READ;
    }
    else if (tagType == taf_pa_sms_Tag::TAF_PA_NOT_READ)
    {
        teluxTagType = telux::tel::SmsTagType::MT_NOT_READ;
    }
    else
    {
        teluxTagType = telux::tel::SmsTagType::UNKNOWN;
    }

    auto status = smsManager->setTag(msgIndex, teluxTagType, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_INFO("setTag failed");
        return PA_FAULT;
    }

    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    pa_result_t res = futResult.get();
    if (res == PA_OK)
    {
        PA_INFO("setTag succeeded");
    }
    return res;
}

pa_result_t tafpa::sms::taf_pa_sms_DeleteMessage
(
    uint32_t msgIndex,
    uint32_t timeout,
    uint8_t phoneId
)
{
    PA_INFO("taf_pa_sms_DeleteMessage");
    auto pACtrl = SmsPAController::getInstance();
    auto smsManagers = pACtrl->getSmsManagersList();

    if (phoneId < 1 || phoneId > smsManagers.size())
    {
        PA_ERROR("Invalid phoneId: %d (valid range: 1-%zu)", phoneId, smsManagers.size());
        return PA_FAULT;
    }

    auto smsManager = smsManagers[phoneId - 1];
    if (smsManager == nullptr)
    {
        PA_INFO("smsManager is NULL\n");
        return PA_FAULT;
    }

    telux::tel::DeleteInfo info;
    info.tagType = telux::tel::SmsTagType::UNKNOWN;
    info.delType = telux::tel::DeleteType::DELETE_MSG_AT_INDEX;
    info.msgIndex = msgIndex;

    auto promisePtr = std::make_shared<std::promise<pa_result_t>>();
    auto cb = [promisePtr](telux::common::ErrorCode err) {
        try
        {
            if (err == telux::common::ErrorCode::SUCCESS)
            {
                PA_INFO("deleteMessage successfully done");
                promisePtr->set_value(PA_OK);
            }
            else
            {
                PA_INFO("deleteMessage failed, errorCode: %d", static_cast<int>(err));
                promisePtr->set_value(PA_FAULT);
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
            PA_ERROR("Unknown error in SMS callback.");
        }
    };

    auto status = smsManager->deleteMessage(info, cb);
    if (status != telux::common::Status::SUCCESS)
    {
        PA_INFO("deleteMessage failed");
        return PA_FAULT;
    }

    std::future<pa_result_t> futResult = promisePtr->get_future();
    std::chrono::seconds span(timeout);
    std::future_status waitStatus = futResult.wait_for(span);
    if (std::future_status::timeout == waitStatus)
    {
        PA_ERROR("waiting promise timeout for %d seconds", timeout);
        return PA_TIMEOUT;
    }

    pa_result_t res = futResult.get();
    if (res == PA_OK)
    {
        PA_INFO("deleteMessage succeeded");
    }
    return res;
}

pa_result_t SmsPAController::initialize
(
    void
)
{
    PA_INFO("SmsPAController::initialize");

    auto& phoneFactory = telux::tel::PhoneFactory::getInstance();
    mySmsListener = std::make_shared<tafSmsListener>();

    NumOfSlot = MIN_SIM_SLOT_COUNT;
    if (telux::common::DeviceConfig::isMultiSimSupported())
    {
        NumOfSlot = MAX_SIM_SLOT_COUNT;
        PA_INFO("MultiSim supported");
    }

    for (auto index = 1; index <= NumOfSlot; ++index)
    {
        // Initialize SMS Manager
        auto prom = std::make_shared<std::promise<telux::common::ServiceStatus>>();
        auto smsMgr = phoneFactory.getSmsManager(
            index,
            [prom](telux::common::ServiceStatus status)
            {
                try
                {
                    prom->set_value(status);
                }
                catch (const std::future_error& e)
                {
                    PA_ERROR("Failed to set SMS Manager promise value: %s", e.what());
                }
            });
        constexpr int WAIT_TIMEOUT_SECONDS = 30;

        if (!smsMgr)
        {
            PA_ERROR("Failed to get SMS Manager instance ");
        }
        else
        {
            try
            {
                auto future = prom->get_future();
                auto waitStatus = future.wait_for(std::chrono::seconds(WAIT_TIMEOUT_SECONDS));

                if (waitStatus == std::future_status::timeout)
                {
                    PA_ERROR("Timeout waiting for SMS Manager initialization for slot %d", index);
                }
                else if (waitStatus == std::future_status::ready)
                {
                    telux::common::ServiceStatus smsMgrStatus = future.get();
                    if (smsMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                    {
                        auto status = smsMgr->registerListener(mySmsListener);
                        if (status != telux::common::Status::SUCCESS)
                        {
                            PA_ERROR("Unable to register Listener for slot %d", index);
                        }
                        else
                        {
                            smsManagers.emplace_back(smsMgr);
                            PA_INFO("SMS Manager initialized successfully for slot %d", index);
                        }
                    }
                    else
                    {
                        PA_ERROR("Unable to initialize SMS Manager for slot %d, status: %d",
                            index, static_cast<int>(smsMgrStatus));
                    }
                }
            }
            catch (const std::future_error& e)
            {
                PA_ERROR("Failed to get SMS Manager initialization status: %s", e.what());
            }
            catch (const std::exception& e)
            {
                PA_ERROR("Unexpected exception during SMS Manager initialization: %s", e.what());
            }
        }

        // Initialize CellBroadcast Manager
        auto cbProm = std::make_shared<std::promise<telux::common::ServiceStatus>>();
        auto cbMgr = phoneFactory.getCellBroadcastManager
        (
            static_cast<SlotId>(index),
            [cbProm](telux::common::ServiceStatus status)
            {
                try
                {
                    cbProm->set_value(status);
                }
                catch (const std::future_error& e)
                {
                    PA_ERROR("Failed to set CellBroadcast Manager promise value: %s", e.what());
                }
            }
        );

        if (cbMgr)
        {
            try
            {
                auto cbFuture = cbProm->get_future();
                auto cbWaitStatus = cbFuture.wait_for(std::chrono::seconds(WAIT_TIMEOUT_SECONDS));
                if (cbWaitStatus == std::future_status::timeout)
                {
                    PA_ERROR("Timeout waiting for CellBroadcast Manager initialization for "
                        "slot %d", index);
                }
                else if (cbWaitStatus == std::future_status::ready)
                {
                    telux::common::ServiceStatus cbMgrStatus = cbFuture.get();
                    if (cbMgrStatus == telux::common::ServiceStatus::SERVICE_AVAILABLE)
                    {
                        PA_INFO("CellBroadcast Manager initialized successfully for slot %d",
                            index);
                        CbManagers.emplace_back(cbMgr);
                    }
                    else
                    {
                        PA_ERROR("Unable to initialize CellBroadcast Manager for slot %d, "
                            "status: %d", index, static_cast<int>(cbMgrStatus));
                    }
                }
            }
            catch (const std::future_error& e)
            {
                PA_ERROR("Failed to get CellBroadcast Manager initialization status for slot %d: "
                    "%s", index, e.what());
            }
            catch (const std::exception& e)
            {
                PA_ERROR("Unexpected exception during CellBroadcast Manager initialization for "
                    "slot %d: %s", index, e.what());
            }
        }
        else
        {
            PA_ERROR("Failed to get CellBroadcast Manager instance for slot %d", index);
        }
    }

    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_Init
(
    void
)
{
    auto pACtrl = SmsPAController::getInstance();

    pa_result_t result = pACtrl->initialize();
    if (result == PA_OK)
    {
        PA_INFO("SMS platform adapter initialization is done");
        g_smsPaInitialized.store(true, std::memory_order_release);
    }
    else
    {
        PA_CRIT("Failed to initialize SMS platform adapter, ret: %d", result);
    }

    return result;
}

pa_result_t SmsPAController::deinitialize
(
    void
)
{
    PA_INFO("Starting SMS platform adaptor deinitialization...");

    // Step 1: Clear the incoming SMS and memory-full callbacks under the mutex
    // so no further notifications are dispatched after this point.
    PA_INFO("Clearing incomingSmsCb and memoryFullCb");
    {
        std::lock_guard<std::mutex> lk(cbMutex);
        incomingSmsCb = nullptr;
        memoryFullCb  = nullptr;
    }

    // Step 2: Deregister the SMS listener from every SMS manager so the SDK
    // stops delivering events to it.
    PA_INFO("Deregistering SMS listener from all SMS managers");
    for (auto& smsMgr : smsManagers)
    {
        if (smsMgr && mySmsListener)
        {
            telux::common::Status status = smsMgr->removeListener(mySmsListener);
            if (status != telux::common::Status::SUCCESS)
            {
                PA_ERROR("Failed to remove SMS listener from a manager");
                // Continue cleanup even if removal failed
            }
        }
    }

    // Step 3: Reset the SMS listener shared pointer so the listener object is
    // released once no other owners remain.
    PA_INFO("Resetting mySmsListener");
    mySmsListener.reset();

    // Step 4: Clear the SMS manager vector so all ISmsManager shared pointers
    // are released.
    PA_INFO("Clearing smsManagers vector");
    smsManagers.clear();

    // Step 5: Clear the CellBroadcast manager vector so all ICellBroadcastManager
    // shared pointers are released.
    PA_INFO("Clearing CbManagers vector");
    CbManagers.clear();

    // Step 6: Clear the cached cell-broadcast filter list.
    PA_INFO("Clearing CBFilterList");
    {
        std::lock_guard<std::mutex> lk(cbFilterMutex);
        CBFilterList.clear();
    }

    PA_INFO("SMS platform adaptor deinitialization complete.");
    return PA_OK;
}

pa_result_t tafpa::sms::taf_pa_sms_Deinit
(
    void
)
{
    // Step 0: Check if Init() was called successfully
    if (!g_smsPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before Init() was successfully called");
        return PA_FAULT;
    }

    auto pACtrl = SmsPAController::getInstance();

    pa_result_t result = pACtrl->deinitialize();
    if (result == PA_OK)
    {
        PA_INFO("SMS platform adapter deinitialization done.");
        g_smsPaInitialized.store(false, std::memory_order_release);
    }
    else
    {
        PA_ERROR("SMS platform adapter deinitialization failed, ret: %d", result);
    }

    return result;
}
