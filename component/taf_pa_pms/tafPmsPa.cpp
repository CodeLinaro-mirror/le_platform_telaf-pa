/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafPmsPa.hpp"

#include <assert.h>
#include <telux/power/PowerFactory.hpp>
#include <telux/power/TcuActivityDefines.hpp>
#include <telux/power/TcuActivityListener.hpp>
#include <telux/power/TcuActivityManager.hpp>

#include "taf_ns_pa_pms.hpp"

static SendEventFunc_t SendEvent;

static inline void RaiseEvent
(
    taf_pa_pms_Event_t * ev
)
{
    if (SendEvent != NULL)
    {
        SendEvent(ev);
    }
}

using namespace telux::power;
using namespace telux::common;
using namespace std;

#define TAF_MODEM_WMS_SVC_ID                          (0x05)
#define TAF_MODEM_VOICE_CALL_SVC_ID                   (0x09)
#define TAF_MODEM_SIM_SVC_ID                          (0x0B)

#define TAF_MODEM_SMS_COMING_MSG_ID                   (0x0001)
#define TAF_MODEM_VCALL_COMING_MSG_ID                 (0x002E)
#define TAF_MODEM_SIM_PROFILE_SWAP_MSG_ID             (0x0033)

#define TAF_MODEM_WS_BIT_MASK_SMS                     (0x0001)
#define TAF_MODEM_WS_BIT_MASK_VOICE_CALL              (0x0002)
#define TAF_MODEM_WS_BIT_MASK_REMOTE_SIM_PROFILE_SWAP (0x0004)

struct taf_pa_pms_RefStruct_t
{
    taf_ns_pa_pms_MpssRef_t mpssRef;

    std::shared_ptr<telux::power::ITcuActivityManager>     masterMgr_;
    std::shared_ptr<telux::power::ITcuActivityManager>     slaveMgr_;
    std::shared_ptr<telux::power::IWakeupManager>          wakeupMgr_;

    std::shared_ptr<telux::power::ITcuActivityListener>    masterStateUpdateListener_;
    std::shared_ptr<telux::power::ITcuActivityListener>    slaveStateUpdateListener_;
    std::shared_ptr<telux::common::IServiceStatusListener> masterSvcListener_;
    std::shared_ptr<telux::power::IWakeupListener>         wakeupReasonListener_;
};

static taf_pa_pms_RefStruct_t pa;

static const char * to_StateString(TcuActivityState state)
{
    switch(state) {
        case TcuActivityState::RESUME: return "RESUME";
        case TcuActivityState::SUSPEND: return "SUSPEND";
        case TcuActivityState::SHUTDOWN: return "SHOTDOWN";
        default : return "UNKNOWN";
    }
}

class WakeupReasonListener : public telux::power::IWakeupListener {
public:
    void onWakeup(telux::power::WakeupInfo wakeupInfo) override {

        PA_INFO("SDK wakeup event is coming");

        static uint32_t wsBitset = 0;

        if (wakeupInfo.wakeupType != telux::power::WakeupType::QMI) {
            PA_WARN("Bad info type from wakeup event");
            return;
        }

        PA_INFO("svc_id : 0x%04x", wakeupInfo.qmiWakeupInfo.serviceId);
        PA_INFO("sourceNodeId : 0x%04x", wakeupInfo.qmiWakeupInfo.sourceNodeId);
        PA_INFO("destinationNodeId : 0x%04x", wakeupInfo.qmiWakeupInfo.destinationNodeId);

        if (wakeupInfo.qmiWakeupInfo.isPIDValid)
        {
            PA_INFO("pid : 0x%04x", wakeupInfo.qmiWakeupInfo.pid);
        }

        if (wakeupInfo.qmiWakeupInfo.isProcessNameValid)
        {
            PA_INFO("processName : %s", wakeupInfo.qmiWakeupInfo.processName.c_str());
        }

        if (wakeupInfo.qmiWakeupInfo.isMsgIdValid)
        {
            PA_INFO("msg_id : 0x%04x", wakeupInfo.qmiWakeupInfo.msgId);

            if (TAF_MODEM_WMS_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
            &&  TAF_MODEM_SMS_COMING_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
            {
                PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                        wakeupInfo.qmiWakeupInfo.serviceId,
                        wakeupInfo.qmiWakeupInfo.msgId);
                wsBitset = TAF_MODEM_WS_BIT_MASK_SMS;
            }
            else if (TAF_MODEM_VOICE_CALL_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
            &&       TAF_MODEM_VCALL_COMING_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
            {
                PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                        wakeupInfo.qmiWakeupInfo.serviceId,
                        wakeupInfo.qmiWakeupInfo.msgId);
                wsBitset = TAF_MODEM_WS_BIT_MASK_VOICE_CALL;
            }
            else if (TAF_MODEM_SIM_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
            &&       TAF_MODEM_SIM_PROFILE_SWAP_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
            {
                PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                        wakeupInfo.qmiWakeupInfo.serviceId,
                        wakeupInfo.qmiWakeupInfo.msgId);
                wsBitset = TAF_MODEM_WS_BIT_MASK_REMOTE_SIM_PROFILE_SWAP;
            }
            else
            {
                PA_WARN("Combo [svc_id:0x%04x, msg_id:0x%04x] is NOT in range <-",
                        wakeupInfo.qmiWakeupInfo.serviceId,
                        wakeupInfo.qmiWakeupInfo.msgId);

                return; // No need to raise the event
            }

            static taf_pa_pms_Event_t ev = {
                .evType = EV_WAKEUP_INFO,
                .evPayload = &wsBitset,
                .evPsize = sizeof(wsBitset),
            };

            RaiseEvent(&ev);
        }
        else
        {
            PA_ERROR("No valid msg_id for svc_id: 0x%04x",
                     wakeupInfo.qmiWakeupInfo.serviceId);
        }
    }
};

class ServiceStatusListener : public telux::common::IServiceStatusListener {
public:
    void onServiceStatusChange(ServiceStatus status) override
    {
        static taf_pa_pms_ServiceStatus_t status_ = SVC_AVAILABLE;

        if(status == ServiceStatus::SERVICE_UNAVAILABLE)
        {
            PA_ERROR( "SDK [master-service] status : UNAVAILABLE" );
            status_ = SVC_UNAVAILABLE;
        }
        else if(status == ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO( "SDK [master-service] status : AVAILABLE" );
        }

        static taf_pa_pms_Event_t ev = {
            .evType = EV_SVC_STATUS,
            .evPayload = &status_,
            .evPsize = sizeof(status_),
        };

        RaiseEvent(&ev);
    }
};

class StateUpdateListener : public telux::power::ITcuActivityListener {
public :
    /* Only Slave clients*/
    void onTcuActivityStateUpdate
    (
        TcuActivityState state,
        string machineName
    ) override
    {
        PA_INFO("%s state is %s", __FUNCTION__, to_StateString(state));

        static taf_pa_pms_PowerUpdateEvent_t pwrUpdate;

        if(state == TcuActivityState::SUSPEND)
        {
            pwrUpdate.state = taf_pa_pms_PwrSts_SUSPEND;
        }
        else if(state == TcuActivityState::SHUTDOWN)
        {
            pwrUpdate.state = taf_pa_pms_PwrSts_SHUTDOWN;
        }
        else if(state == TcuActivityState::RESUME)
        {
            pwrUpdate.state = taf_pa_pms_PwrSts_RESUME;
        }
        else
        {
            PA_ERROR("Unknown state from SDK");
            pwrUpdate.state = taf_pa_pms_PwrSts_UNKNOWN;
            return; // Don't need to send out event
        }

        snprintf(
            pwrUpdate.machineName,
            MAX_MACHINE_NAME,
            "%s",
            machineName.c_str()
        );

        static taf_pa_pms_Event_t ev = {
            .evType = EV_POWER_STATE_UPDATE,
            .evPayload = &pwrUpdate,
            .evPsize = sizeof(pwrUpdate),
        };

        // Anyway, raise the evet to all PMS clients
        RaiseEvent(&ev);
    }

    /* As both master & slave clients */
    void onMachineUpdate
    (
        const string machineName,
        const MachineEvent machineEvent
    ) override
    {
        PA_INFO("%s", __FUNCTION__);

        static taf_pa_pms_MachineUpdateEvent_t machUpdate;

        if (machineEvent == MachineEvent::AVAILABLE)
        {
            machUpdate.machineEvent = MACHINE_AVAILABLE;
        }
        else
        {
            machUpdate.machineEvent = MACHINE_UNAVAILABLE;
        }

        snprintf(
            machUpdate.machineName,
            MAX_MACHINE_NAME,
            "%s",
            machineName.c_str()
        );

        static taf_pa_pms_Event_t ev = {
            .evType = EV_MACHINE_UPDATE,
            .evPayload = &machUpdate,
            .evPsize = sizeof(machUpdate),
        };

        RaiseEvent(&ev);
    }

    /* Only Master client */
    void onSlaveAckStatusUpdate
    (
        const telux::common::Status status,
        const string machineName,
        const vector<ClientInfo> unresponsiveClients,
        const vector<ClientInfo> nackResponseClients
    ) override
    {
        if(status == telux::common::Status::SUCCESS)
        {
            PA_INFO("All slave apps successfully acknowledged the state transition");
        }
        else if(status == telux::common::Status::EXPIRED)
        {
            PA_INFO("Timeout occurred while waiting for ACK from slave apps");
        }
        else // telux::common::Status::FAILED
        {
            PA_ERROR("Failed to receive ACK from slave apps");
        }

        // Allocate the 'info-struct' on stack
        static taf_pa_pms_ConsolidatedInfo_t info;

        if (nackResponseClients.size() > TAF_CONSOLIDATED_CLNT_SIZE)
        {
            PA_WARN("N-ACK client number is overlap");
            info.nackResponseClntSize = TAF_CONSOLIDATED_CLNT_SIZE;
        }
        else
        {
            info.nackResponseClntSize = nackResponseClients.size();
        }

        if (unresponsiveClients.size() > TAF_CONSOLIDATED_CLNT_SIZE)
        {
            PA_WARN("Unresponsive client number is overlap");
            info.unresponsiveClntSize = TAF_CONSOLIDATED_CLNT_SIZE;
        }
        else
        {
            info.unresponsiveClntSize = unresponsiveClients.size();
        }

        PA_INFO("size: [nack]/(%d) [unresp]/(%d)",
                info.nackResponseClntSize,
                info.unresponsiveClntSize);

        if (info.nackResponseClntSize != 0)
        {
            for (uint32_t i = 0; i < info.nackResponseClntSize; i++)
            {
                auto itNack = nackResponseClients[i];

                PA_INFO("Client: %s, Machine: %s",
                        itNack.first.c_str(),
                        itNack.second.c_str());

                snprintf(
                    info.nackResponseClntData[i].clientName,
                    sizeof(info.nackResponseClntData[i].clientName),
                    "%s",
                    itNack.first.c_str()
                );

                snprintf(
                    info.nackResponseClntData[i].machineName,
                    sizeof(info.nackResponseClntData[i].machineName),
                    "%s",
                    itNack.second.c_str()
                );
            }
        }

        if (info.unresponsiveClntSize != 0)
        {
            for (uint32_t i = 0; i < info.unresponsiveClntSize; i++)
            {
                auto itUnresp = unresponsiveClients[i];

                PA_INFO("Client: %s, Machine: %s",
                        itUnresp.first.c_str(),
                        itUnresp.second.c_str());

                snprintf(
                    info.unresponsiveClntData[i].clientName,
                    sizeof(info.unresponsiveClntData[i].clientName),
                    "%s",
                    itUnresp.first.c_str()
                );

                snprintf(
                    info.unresponsiveClntData[i].machineName,
                    sizeof(info.unresponsiveClntData[i].machineName),
                    "%s",
                    itUnresp.second.c_str()
                );

            }
        }

        static taf_pa_pms_Event_t ev = {
            .evType = EV_CONSOLIDATED_INFO,
            .evPayload = &info,
            .evPsize = sizeof(info),
        };

        RaiseEvent(&ev);
    }
};

static void PaMpssErrorCallback
(
    taf_ns_pa_pms_ErrCode_t errCode,
    void * cbCtx
)
{
    switch (errCode)
    {
        case TAF_NS_PA_PMS_ERR_SVC_GONE:
        {
            PA_INFO("prop-pms: SVC_GONE event captured");
        }
        break;

        default:
        {
            PA_WARN("Unknown error captured: 0x%02x", errCode);
        }
    }
}

static inline telux::power::TcuActivityState to_SdkPowerState
(
    taf_pa_pms_PowerState_t state
)
{
    switch (state)
    {
        case taf_pa_pms_PwrSts_SUSPEND:
            return TcuActivityState::SUSPEND;
        case taf_pa_pms_PwrSts_RESUME:
            return TcuActivityState::RESUME;
        case taf_pa_pms_PwrSts_SHUTDOWN:
            return TcuActivityState::SHUTDOWN;
        case taf_pa_pms_PwrSts_UNKNOWN:
        default:
            return TcuActivityState::UNKNOWN;
    }
}

static void cb_SetPowerState
(
    std::weak_ptr<std::promise<int>> wp,
    ErrorCode errorCode,
    const char * description
)
{
    auto valid = wp.lock();
    int rst = 0;

    PA_INFO("Calling %s", __FUNCTION__);

    if (valid)
    {
        if (errorCode != telux::common::ErrorCode::SUCCESS)
        {
            PA_WARN("Failed to %s, err: %d, mark as N-OK",
                    description,
                    static_cast<int>(errorCode));
            rst = 1;
        }
        else
        {
            PA_INFO("[%s] PMD says OK", description);
            rst = 0; // OK
        }

        try
        {
            valid->set_value(rst);
        }
        catch (const std::future_error& e)
        {
            if (e.code() == std::make_error_code(std::future_errc::promise_already_satisfied)
            ||  e.code() == std::make_error_code(std::future_errc::no_state))
            {
                PA_WARN("[%s] Already set by another one, ignore", description);
            }
            else
            {
                PA_ERROR("[%s] Report the exception to the top level", description);
                throw; // Rare cases
            }
        }
    }
    else
    {
        PA_ERROR("[%s] Got one invalid 'promise', ignore", description);
    }
}

static void cb_ServiceStatus
(
    std::weak_ptr<std::promise<telux::common::ServiceStatus>> wp,
    telux::common::ServiceStatus status,
    const char * description
)
{
    auto valid = wp.lock();

    if (valid)
    {
        if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO("[%s] service is available", description);
        }
        else
        {
            PA_ERROR("[%s] service is UN-available", description);
        }

        // Ensure the graceful exit from exceptions.
        try
        {
            valid->set_value(status);
        }
        catch (const std::future_error& e)
        {
            // The 'set_xxx' functions already were invoked
            if (e.code() == std::make_error_code(std::future_errc::promise_already_satisfied)
            ||  e.code() == std::make_error_code(std::future_errc::no_state))
            {
                PA_WARN("[%s] Already set by another one, ignore", description);
            }
            else
            {
                PA_ERROR("[%s] Report the exception to the top level", description);
                throw; // Rare cases
            }
        }
    }
    else
    {
        PA_ERROR("[%s] Got one invalid 'promise', ignore", description);
    }
}

static bool IsOkForCheckingFuture
(
    std::future<telux::common::ServiceStatus> &fu,
    uint32_t timeoutMs,
    const char * mgrName
)
{
    auto status = fu.wait_for(std::chrono::milliseconds(timeoutMs));

    if (status == std::future_status::ready)
    {
        try
        {
            if (fu.get() !=  telux::common::ServiceStatus::SERVICE_AVAILABLE)
            {
                PA_ERROR("Failed to init %s: Service is unavailable", mgrName);
                return false;
            }
        }
        catch (const std::future_error& fe)
        {
            using std::future_errc;

            if (fe.code() == std::make_error_code(future_errc::broken_promise))
            {
                PA_ERROR("No provider & can NOT set_value");
                return false;
            }
            else if (fe.code() == std::make_error_code(future_errc::no_state))
            {
                PA_ERROR("Already got the future ever");
                return false;
            }
            else
            {
                PA_ERROR("future_error: %s (%d) - %s",
                        fe.what(), fe.code().value(), fe.code().message().c_str());

                return false;
            }
        }

        PA_INFO("Service [%s] is AVAIABLE", mgrName);
        return true;
    }
    else
    {
        PA_ERROR("Timeout for preparing the %s [%u](ms)", mgrName, timeoutMs);
        return false;
    }
}

/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
/* -------------------( PaFn - Fcall ) ------------------ */
/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
pa_result_t taf_pa_pms_Init
(
    taf_pa_pms_Reference_t  *paRefPtr,
    SendEventFunc_t          fnSendEvent,
    uint32_t                 timeoutMs
)
{
    // Loaded the Event-Reporter for the PA layer
    SendEvent = fnSendEvent;

    PA_INFO("Loading PMS [ACTUAL] PA ...");

    auto &powerFactory = PowerFactory::getInstance();

    ClientInstanceConfig masterConfig;
    masterConfig.clientType = ClientType::MASTER;
    masterConfig.clientName = "tafPMSvc";
    masterConfig.machineName =  ALL_MACHINES;

    auto masterMgrP =
        std::make_shared<
            std::promise<
                telux::common::ServiceStatus>>();

    std::weak_ptr<
        std::promise<
            telux::common::ServiceStatus>> masterMgrWP = masterMgrP;

    pa.masterMgr_ = powerFactory.getTcuActivityManager(
                    masterConfig,
                    [masterMgrWP](telux::common::ServiceStatus status)
                    {
                        cb_ServiceStatus(masterMgrWP, status, "masterMgr");
                    });

    if (nullptr == pa.masterMgr_)
    {
        PA_ERROR("Failed to getTcuActivityManager for [masterMgr]");
        return PA_FAULT;
    }

    std::future<telux::common::ServiceStatus> fuMaster = masterMgrP->get_future();

    if (! IsOkForCheckingFuture(fuMaster, timeoutMs, "/masterMgr"))
    {
        return PA_FAULT;
    }

    ClientInstanceConfig slaveConfig;
    slaveConfig.clientType = ClientType::SLAVE;
    slaveConfig.clientName = "tafPMSvc";
    slaveConfig.machineName =  ALL_MACHINES;

    auto slaveMgrP =
        std::make_shared<
            std::promise<
                telux::common::ServiceStatus>>();

    std::weak_ptr<
        std::promise<
            telux::common::ServiceStatus>> slaveMgrWP = slaveMgrP;

    pa.slaveMgr_ = powerFactory.getTcuActivityManager(
                    slaveConfig,
                    [slaveMgrWP](telux::common::ServiceStatus status)
                    {
                        cb_ServiceStatus(slaveMgrWP, status, "slaveMgr");
                    });

    std::future<telux::common::ServiceStatus> fuSlave = slaveMgrP->get_future();

    if (! IsOkForCheckingFuture(fuSlave, timeoutMs, "/slaveMgr"))
    {
        return PA_FAULT;
    }

    auto wakeupMgrP =
        std::make_shared<
            std::promise<
                telux::common::ServiceStatus>>();

    std::weak_ptr<
        std::promise<
            telux::common::ServiceStatus>> wakeupMgrWP = wakeupMgrP;

    pa.wakeupMgr_ = powerFactory.getWakeupManager(
                        [wakeupMgrWP](telux::common::ServiceStatus status)
                        {
                            cb_ServiceStatus(wakeupMgrWP, status, "wakeupMgr");
                        });

    if (nullptr == pa.wakeupMgr_)
    {
        PA_ERROR("Failed to getWakeupManager for [wakeupMgr]");
        return PA_FAULT;
    }

    std::future<telux::common::ServiceStatus> fuWakeup = wakeupMgrP->get_future();

    if (! IsOkForCheckingFuture(fuWakeup, timeoutMs, "/wakeupMgr"))
    {
        return PA_FAULT;
    }

    // All SDK service managers are ready.

    telux::common::Status sdkStatus = telux::common::Status::FAILED;

    pa.wakeupReasonListener_ = std::make_shared<WakeupReasonListener>();

    telux::common::ErrorCode err =
        pa.wakeupMgr_->registerListener(pa.wakeupReasonListener_);

    if (err != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to register wakeup listener");
        return PA_FAULT;
    }

    pa.masterSvcListener_ = std::make_shared<ServiceStatusListener>();

    sdkStatus = pa.masterMgr_->registerServiceStateListener(pa.masterSvcListener_);

    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to register for /masterMgr svc avaiable listener");
        // Non-fatal failure, continue ...
    }

    pa.masterStateUpdateListener_ = std::make_shared<StateUpdateListener>();

    sdkStatus = pa.masterMgr_->registerListener(pa.masterStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to register the ITcuActivityListener for /masterMgr");
        return PA_FAULT;
    }

    pa.slaveStateUpdateListener_ = std::make_shared<StateUpdateListener>();

    sdkStatus = pa.slaveMgr_->registerListener(pa.slaveStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to register the ITcuActivityListener for /slaveMgr");
        return PA_FAULT;
    }

    taf_ns_pa_pms_Result_t result =
        taf_ns_pa_pms_Init(
            &pa.mpssRef,
            PaMpssErrorCallback,
            NULL);

    if (result != taf_ns_pa_pms_Result_OK)
    {
        PA_ERROR("Failed to taf_ns_pa_pms_Init: err(%d)", result);
        return PA_FAULT;
    }

    *paRefPtr = &pa;

    PA_INFO("PMS [ACTUAL] PA loaded");
    return PA_OK;
}

void taf_pa_pms_Deinit
(
    taf_pa_pms_Reference_t *paRefPtr
)
{
    if (paRefPtr == NULL || *paRefPtr != &pa)
    {
        PA_ERROR("Bad paRefPtr");
        return;
    }

    PA_INFO("Starting PMS PA deinitialization...");

    // Step 1: Deregister wakeup listener and release its shared pointer so no
    // further wakeup callbacks are dispatched after this point.
    PA_INFO("Deregistering wakeup listener [wakeupMgr]");
    telux::common::ErrorCode err = pa.wakeupMgr_->deRegisterListener(pa.wakeupReasonListener_);
    if (err != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deRegisterListener [wakeupMgr]");
    }
    pa.wakeupReasonListener_.reset();

    telux::common::Status sdkStatus = telux::common::Status::FAILED;

    // Step 2: Deregister service state listener from master manager and release
    // its shared pointer.
    PA_INFO("Deregistering service state listener [masterMgr]");
    sdkStatus = pa.masterMgr_->deregisterServiceStateListener(pa.masterSvcListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deregisterServiceStateListener [masterMgr]");
    }
    pa.masterSvcListener_.reset();

    // Step 3: Deregister TCU activity listener from master manager and release
    // its shared pointer.
    PA_INFO("Deregistering state update listener [masterMgr]");
    sdkStatus = pa.masterMgr_->deregisterListener(pa.masterStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deregisterListener [masterMgr]");
    }
    pa.masterStateUpdateListener_.reset();

    // Step 4: Deregister TCU activity listener from slave manager and release
    // its shared pointer.
    PA_INFO("Deregistering state update listener [slaveMgr]");
    sdkStatus = pa.slaveMgr_->deregisterListener(pa.slaveStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deregisterListener [slaveMgr]");
    }
    pa.slaveStateUpdateListener_.reset();

    // Step 5: Deinitialize the ns-layer QMI client to release modem resources.
    PA_INFO("Deinitializing ns-layer PMS QMI client");
    taf_ns_pa_pms_Result_t nsResult = taf_ns_pa_pms_Deinit(&pa.mpssRef);
    if (nsResult != taf_ns_pa_pms_Result_OK)
    {
        PA_ERROR("taf_ns_pa_pms_Deinit failed: err(%d)", nsResult);
        // Continue cleanup even if ns-layer deinit failed
    }

    // Step 6: Reset SDK manager shared pointers so the underlying SDK objects
    // are released once no other owners remain.
    PA_INFO("Resetting wakeupMgr_, masterMgr_ and slaveMgr_");
    pa.wakeupMgr_.reset();
    pa.masterMgr_.reset();
    pa.slaveMgr_.reset();

    // Step 7: Clear the event-reporter callback so no further events are raised.
    PA_INFO("Clearing SendEvent callback");
    SendEvent = NULL;

    *paRefPtr = NULL; // Reset caller reference pointer

    PA_INFO("PMS PA deinitialization complete.");
}

pa_result_t taf_pa_pms_SetPowerStateAsMaster
(
    taf_pa_pms_Reference_t         paRef,
    taf_pa_pms_PowerState_t        state,
    const char               *name
)
{
    telux::power::TcuActivityState sdkState = to_SdkPowerState(state);

    telux::common::Status status = telux::common::Status::FAILED;
    pa_result_t rst = PA_OK;

    auto pwrSetP = std::make_shared<std::promise<int>>();

    std::weak_ptr<std::promise<int>> pwrSetWP = pwrSetP;

    status = pa.masterMgr_->setActivityState(
                                sdkState,
                                std::string(name),
                                [pwrSetWP](ErrorCode errorCode){
                                    cb_SetPowerState(
                                        pwrSetWP,
                                        errorCode,
                                        "/SetPowerState");
                                });

    if(status == telux::common::Status::SUCCESS)
    {
        std::future<int> fu = pwrSetP->get_future();

        #define SET_STATE_TIMEOUT 5
        std::future_status waitStatus =
            fu.wait_for(std::chrono::seconds(SET_STATE_TIMEOUT));

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("waiting promise timeout for %d seconds",
                     SET_STATE_TIMEOUT);
            rst = PA_FAULT;
        }
        else
        {
            rst = fu.get() == 0 ? PA_OK : PA_FAULT;
        }
    }
    else
    {
        PA_ERROR("Failed to call SDK setActivityState");
        rst = PA_FAULT;
    }

    return rst;
}

pa_result_t taf_pa_pms_SendAckForStateUpdate
(
    taf_pa_pms_Reference_t  paRef,
    taf_pa_pms_PowerState_t state,
    taf_pa_pms_Ack_t        ack
)
{
    telux::common::Status status = telux::common::Status::FAILED;

    if (state == taf_pa_pms_PwrSts_SUSPEND)
    {
        status =
            pa.slaveMgr_->sendActivityStateAck(
                ack == taf_pa_pms_ACK
                    ? StateChangeResponse::ACK
                    : StateChangeResponse::NACK,
                TcuActivityState::SUSPEND);

        if (status != Status::SUCCESS)
        {
            PA_ERROR("Failed to send [%s] for [%s] state",
                     ack == taf_pa_pms_ACK ? "ACK": "NACK",
                     "SUSPEND");
            return PA_FAULT;
        }
    }
    else if (state == taf_pa_pms_PwrSts_SHUTDOWN)
    {
        status =
            pa.slaveMgr_->sendActivityStateAck(
                ack == taf_pa_pms_ACK
                    ? StateChangeResponse::ACK
                    : StateChangeResponse::NACK,
                TcuActivityState::SHUTDOWN);

        if (status != Status::SUCCESS)
        {
            PA_ERROR("Failed to send [%s] for [%s] state",
                     ack == taf_pa_pms_ACK ? "ACK": "NACK",
                     "SHUTDOWN");
            return PA_FAULT;
        }
    }
    else
    {
        PA_WARN("Unsupported state to be set: %d, rejected", state);
        return PA_FAULT;
    }

    PA_INFO("Successfully send [%s] to PMD with [%s] state",
            ack == taf_pa_pms_ACK ? "ACK": "NACK",
            state == taf_pa_pms_PwrSts_SUSPEND
                ? "SUSPEND"
                : "SHUTDOWN");

    return PA_OK;
}

pa_result_t taf_pa_pms_GetAllMachineNames
(
    taf_pa_pms_Reference_t    paRef,
    std::vector<std::string> &machineNames
)
{
    telux::common::Status status =
        pa.masterMgr_->getAllMachineNames(machineNames);

    if (status != Status::SUCCESS)
    {
        PA_ERROR("Failed to getAllMachineNames");
        return PA_FAULT;
    }

    return PA_OK;
}

pa_result_t taf_pa_pms_SetModemWakeupFilter
(
    taf_pa_pms_Reference_t    paRef,
    uint32_t                  wsBitmask
)
{
    taf_ns_pa_pms_Result_t result =
        taf_ns_pa_pms_SetWsFilter(pa.mpssRef,
                (taf_ns_pa_pms_ModemWakeupSource_t) wsBitmask);

    if (result != taf_ns_pa_pms_Result_OK)
    {
        PA_ERROR("Failed to taf_ns_pa_pms_SetWsFilter");
        return PA_FAULT;
    }

    return PA_OK;
}

pa_result_t taf_pa_pms_GetModemWakeupFilter
(
    taf_pa_pms_Reference_t    paRef,
    uint32_t                 *wsBitmaskPtr
)
{
    taf_ns_pa_pms_Result_t result =
        taf_ns_pa_pms_GetWsFilter(pa.mpssRef,
                (taf_ns_pa_pms_ModemWakeupSource_t *) wsBitmaskPtr);

    if (result != taf_ns_pa_pms_Result_OK)
    {
        *wsBitmaskPtr = 0;
        PA_ERROR("Failed to taf_ns_pa_pms_GetWsFilter");
        return PA_FAULT;
    }

    return PA_OK;
}
