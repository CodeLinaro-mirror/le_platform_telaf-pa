/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_pa_pms.hpp"

#include <assert.h>
#include <telux/power/PowerFactory.hpp>
#include <telux/power/TcuActivityDefines.hpp>
#include <telux/power/TcuActivityListener.hpp>
#include <telux/power/TcuActivityManager.hpp>

#include "taf_ns_pa_pms.hpp"

static SendEventFunc_t SendEvent;

static inline void RaiseEvent
(
    PaEvent_t * ev
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

struct PaType(RefStruct)
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

static PaType(RefStruct) pa;

static std::promise<telux::common::ServiceStatus> masterMgrP;
static std::promise<telux::common::ServiceStatus> slaveMgrP;
static std::promise<telux::common::ServiceStatus> wakeupMgrP;

static bool IsTimeout
(
    std::future<telux::common::ServiceStatus> &fu,
    uint32_t timeoutMs
)
{
    return (fu.wait_for(std::chrono::seconds(5))
            !=
            std::future_status::ready);
}

static bool CheckResultIsOK
(
    std::future<telux::common::ServiceStatus> &fu,
    const char * mgrName
)
{
    try
    {
        if (fu.get() !=  telux::common::ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_ERROR("Failed to init %s: Servaice is unavailable", mgrName);
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

            static PaEvent_t ev = {
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
        static ServiceStatus_t status_ = SVC_AVAILABLE;

        if(status == ServiceStatus::SERVICE_UNAVAILABLE)
        {
            PA_ERROR( "SDK [master-service] status : UNAVAILABLE" );
            status_ = SVC_UNAVAILABLE;
        }
        else if(status == ServiceStatus::SERVICE_AVAILABLE)
        {
            PA_INFO( "SDK [master-service] status : AVAILABLE" );
        }

        static PaEvent_t ev = {
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

        static PowerUpdateEvent_t pwrUpdate;

        if(state == TcuActivityState::SUSPEND)
        {
            pwrUpdate.state = PaPwrState(SUSPEND);
        }
        else if(state == TcuActivityState::SHUTDOWN)
        {
            pwrUpdate.state = PaPwrState(SHUTDOWN);
        }
        else if(state == TcuActivityState::RESUME)
        {
            pwrUpdate.state = PaPwrState(RESUME);
        }
        else
        {
            PA_ERROR("Unknown state from SDK");
            pwrUpdate.state = PaPwrState(UNKNOWN);
            return; // Don't need to send out event
        }

        snprintf(
            pwrUpdate.machineName,
            MAX_MACHINE_NAME,
            "%s",
            machineName.c_str()
        );

        static PaEvent_t ev = {
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

        static MachineUpdateEvent_t machUpdate;

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

        static PaEvent_t ev = {
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
        static PaType(ConsolidatedInfo) info;

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

        static PaEvent_t ev = {
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
    PaType(PowerState) state
)
{
    switch (state)
    {
        case PaPwrState(SUSPEND):
            return TcuActivityState::SUSPEND;
        case PaPwrState(RESUME):
            return TcuActivityState::RESUME;
        case PaPwrState(SHUTDOWN):
            return TcuActivityState::SHUTDOWN;
        case PaPwrState(UNKNOWN):
        default:
            return TcuActivityState::UNKNOWN;
    }
}

static std::promise<int> SetPowerStatePromise;

static void CommandCallback
(
    ErrorCode errorCode
)
{
    PA_INFO("Calling %s", __FUNCTION__);

    if(errorCode == telux::common::ErrorCode::SUCCESS)
    {
        SetPowerStatePromise.set_value(0);
    }
    else
    {
        PA_ERROR("Failed to set power state, err: %d, mark as FAULT",
                 static_cast<int>(errorCode));

        SetPowerStatePromise.set_value(1);
    }
}

/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
/* -------------------( PaFn - Fcall ) ------------------ */
/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
PaType(Result) PaFn(Init)
(
    PaType(Reference)  *paRefPtr,
    SendEventFunc_t     fnSendEvent,
    uint32_t            timeoutMs
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

    pa.masterMgr_ = powerFactory.getTcuActivityManager(
                    masterConfig,
                    [](telux::common::ServiceStatus status)
                    {
                        masterMgrP.set_value(status);
                    });

    if (nullptr == pa.masterMgr_)
    {
        PA_ERROR("Failed to getTcuActivityManager for [masterMgr]");
        return PaResult(FAULT);
    }

    std::future<telux::common::ServiceStatus> fuMaster = masterMgrP.get_future();

    if (IsTimeout(fuMaster, timeoutMs))
    {
        PA_ERROR("Timeout for preparing the /masterMgr [%u](ms)", timeoutMs);
        return PaResult(TIMEOUT);
    }

    if (! CheckResultIsOK(fuMaster, "/masterMgr"))
    {
        return PaResult(FAULT);
    }

    ClientInstanceConfig slaveConfig;
    slaveConfig.clientType = ClientType::SLAVE;
    slaveConfig.clientName = "tafPMSvc";
    slaveConfig.machineName =  ALL_MACHINES;

    pa.slaveMgr_ = powerFactory.getTcuActivityManager(
                    slaveConfig,
                    [](telux::common::ServiceStatus status)
                    {
                        slaveMgrP.set_value(status);
                    });
    if (nullptr == pa.slaveMgr_)
    {
        PA_ERROR("Failed to getTcuActivityManager for [slaveMgr]");
        return PaResult(FAULT);
    }
    std::future<telux::common::ServiceStatus> fuSlave = slaveMgrP.get_future();

    if (IsTimeout(fuSlave, timeoutMs))
    {
        PA_ERROR("Timeout for preparing the /slaveMgr [%u](ms)", timeoutMs);
        return PaResult(TIMEOUT);
    }

    if (! CheckResultIsOK(fuSlave, "/slaveMgr"))
    {
        return PaResult(FAULT);
    }

    pa.wakeupMgr_ = powerFactory.getWakeupManager(
                        [](telux::common::ServiceStatus status)
                        {
                            wakeupMgrP.set_value(status);
                        });

    if (nullptr == pa.wakeupMgr_)
    {
        PA_ERROR("Failed to getWakeupManager for [wakeupMgr]");
        return PaResult(FAULT);
    }

    std::future<telux::common::ServiceStatus> fuWakeup = wakeupMgrP.get_future();

    if (IsTimeout(fuWakeup, timeoutMs))
    {
        PA_ERROR("Timeout for preparing the /wakeupMgr [%u](ms)", timeoutMs);
        return PaResult(TIMEOUT);
    }

    if (! CheckResultIsOK(fuWakeup, "/wakeupMgr"))
    {
        return PaResult(FAULT);
    }

    // All SDK service managers are ready.

    telux::common::Status sdkStatus = telux::common::Status::FAILED;

    pa.wakeupReasonListener_ = std::make_shared<WakeupReasonListener>();

    telux::common::ErrorCode err =
        pa.wakeupMgr_->registerListener(pa.wakeupReasonListener_);

    if (err != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to register wakeup listener");
        return PaResult(FAULT);
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
        return PaResult(FAULT);
    }

    pa.slaveStateUpdateListener_ = std::make_shared<StateUpdateListener>();

    sdkStatus = pa.slaveMgr_->registerListener(pa.slaveStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to register the ITcuActivityListener for /slaveMgr");
        return PaResult(FAULT);
    }

    NsPaType(Result) result =
        taf_ns_pa_pms_Init(
            &pa.mpssRef,
            PaMpssErrorCallback,
            NULL);

    if (result != NsPaResult(OK))
    {
        PA_ERROR("Failed to taf_ns_pa_pms_Init: err(%d)", result);
        return PaResult(FAULT);
    }

    *paRefPtr = &pa;

    PA_INFO("PMS [ACTUAL] PA loaded");
    return PaResult(OK);
}

void PaFn(Deinit)
(
    PaType(Reference) *paRefPtr
)
{
    if (paRefPtr == NULL || *paRefPtr != &pa)
    {
        PA_ERROR("Bad paRefPtr");
        return;
    }


    telux::common::ErrorCode err = pa.wakeupMgr_->deRegisterListener(pa.wakeupReasonListener_);
    if (err != telux::common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deRegisterListener [wakeupMgr]");
    }

    telux::common::Status sdkStatus = telux::common::Status::FAILED;

    sdkStatus = pa.masterMgr_->deregisterServiceStateListener(pa.masterSvcListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deregisterServiceStateListener [masterMgr]");
    }

    sdkStatus = pa.masterMgr_->deregisterListener(pa.masterStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deregisterListener [masterMgr]");
    }

    sdkStatus = pa.slaveMgr_->deregisterListener(pa.slaveStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        PA_ERROR("Failed to call SDK deregisterListener [slaveMgr]");
    }

    SendEvent = NULL;

    *paRefPtr = NULL; // Reset caller reference pointer
}

PaType(Result) PaFn(SetPowerStateAsMaster)
(
    PaType(Reference)         paRef,
    PaType(PowerState)        state,
    const char               *name
)
{
    telux::power::TcuActivityState sdkState = to_SdkPowerState(state);

    telux::common::Status status = telux::common::Status::FAILED;
    PaType(Result) rst = PaResult(OK);

    SetPowerStatePromise = std::promise<int>();

    status = pa.masterMgr_->setActivityState(
                                sdkState,
                                std::string(name),
                                CommandCallback);

    if(status == telux::common::Status::SUCCESS)
    {
        std::future<int> fu = SetPowerStatePromise.get_future();

        #define SET_STATE_TIMEOUT 5
        std::future_status waitStatus =
            fu.wait_for(std::chrono::seconds(SET_STATE_TIMEOUT));

        if (std::future_status::timeout == waitStatus)
        {
            PA_ERROR("waiting promise timeout for %d seconds",
                     SET_STATE_TIMEOUT);
            rst = PaResult(FAULT);
        }
        else
        {
            int r = fu.get();
            rst = r == 0 ? PaResult(OK) : PaResult(FAULT);
        }
    }
    else
    {
        PA_ERROR("Failed to call SDK setActivityState");
        rst = PaResult(FAULT);
    }

    return rst;
}

PaType(Result) PaFn(SendAckForStateUpdate)
(
    PaType(Reference)  paRef,
    PaType(PowerState) state,
    PaType(Ack)        ack
)
{
    telux::common::Status status = telux::common::Status::FAILED;

    if (state == PaPwrState(SUSPEND))
    {
        status =
            pa.slaveMgr_->sendActivityStateAck(
                ack == PaAck(ACK)
                    ? StateChangeResponse::ACK
                    : StateChangeResponse::NACK,
                TcuActivityState::SUSPEND);

        if (status != Status::SUCCESS)
        {
            PA_ERROR("Failed to send [%s] for [%s] state",
                     ack == PaAck(ACK) ? "ACK": "NACK",
                     "SUSPEND");
            return PaResult(FAULT);
        }
    }
    else if (state == PaPwrState(SHUTDOWN))
    {
        status =
            pa.slaveMgr_->sendActivityStateAck(
                ack == PaAck(ACK)
                    ? StateChangeResponse::ACK
                    : StateChangeResponse::NACK,
                TcuActivityState::SHUTDOWN);

        if (status != Status::SUCCESS)
        {
            PA_ERROR("Failed to send [%s] for [%s] state",
                     ack == PaAck(ACK) ? "ACK": "NACK",
                     "SHUTDOWN");
            return PaResult(FAULT);
        }
    }
    else
    {
        PA_WARN("Unsupported state to be set: %d, rejected", state);
        return PaResult(FAULT);
    }

    PA_INFO("Successfully send [%s] to PMD with [%s] state",
            ack == PaAck(ACK) ? "ACK": "NACK",
            state == PaPwrState(SUSPEND)
                ? "SUSPEND"
                : "SHUTDOWN");

    return PaResult(OK);
}

PaType(Result) PaFn(GetAllMachineNames)
(
    PaType(Reference)  paRef,
    std::vector<std::string> & machineNames
)
{
    telux::common::Status status =
        pa.masterMgr_->getAllMachineNames(machineNames);

    if (status != Status::SUCCESS)
    {
        PA_ERROR("Failed to getAllMachineNames");
        return PaResult(FAULT);
    }

    return PaResult(OK);
}

PaType(Result) PaFn(SetModemWakeupFilter)
(
    PaType(Reference)         paRef,
    uint32_t                  wsBitmask
)
{
    NsPaType(Result) result =
        taf_ns_pa_pms_SetWsFilter(pa.mpssRef,
                (taf_ns_pa_pms_ModemWakeupSource_t) wsBitmask);

    if (result != NsPaResult(OK))
    {
        PA_ERROR("Failed to taf_ns_pa_pms_SetWsFilter");
        return PaResult(FAULT);
    }

    return PaResult(OK);
}

PaType(Result) PaFn(GetModemWakeupFilter)
(
    PaType(Reference)         paRef,
    uint32_t                 *wsBitmaskPtr
)
{
    NsPaType(Result) result =
        taf_ns_pa_pms_GetWsFilter(pa.mpssRef,
                (taf_ns_pa_pms_ModemWakeupSource_t *) wsBitmaskPtr);

    if (result != NsPaResult(OK))
    {
        *wsBitmaskPtr = 0;
        PA_ERROR("Failed to taf_ns_pa_pms_GetWsFilter");
        return PaResult(FAULT);
    }

    return PaResult(OK);
}
