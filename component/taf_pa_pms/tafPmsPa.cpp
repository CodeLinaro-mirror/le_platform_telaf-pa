/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafPmsPa.hpp"

#include <assert.h>
#include <atomic>
#include <mutex>
#include <telux/power/PowerFactory.hpp>
#include <telux/power/TcuActivityDefines.hpp>
#include <telux/power/TcuActivityListener.hpp>
#include <telux/power/TcuActivityManager.hpp>

#include "taf_prop_pms.h"
#include "tafInternalCommonPa.h"

static SendEventFunc_t SendEvent;
static std::mutex pmsSendEventMutex;

static inline void RaiseEvent
(
    taf_pa_pms_Event_t * ev
)
{
    SendEventFunc_t handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(pmsSendEventMutex);
        handler = SendEvent;
    }
    if (handler != NULL)
    {
        handler(ev);
    }
}

using namespace telux::power;
using namespace telux::common;
using namespace std;

// Lifecycle state for the PA layer.
//
// gPmsPaInitialized: true only when Init() completed successfully and
//   Deinit() has not yet been called.  Guarded by gPmsPaMutex.
//
// gPmsPaTerminated: latched to true the first time Init() encounters a
//   manager-init timeout.  Once set it is never cleared.  It prevents
//   any subsequent Init() call from overwriting the shared_ptr fields
//   in 'pa' while a timed-out manager's SDK callback may still be in
//   flight, and it gives Deinit() a defined path for partial state so
//   that managers which *did* confirm readiness before the timeout can
//   be released safely.
static std::atomic<bool> gPmsPaInitialized(false);
static std::atomic<bool> gPmsPaTerminated(false);
static std::mutex gPmsPaMutex;

// Mutex serializing all SDK callback bodies that write shared static
// payload storage and call RaiseEvent.  Prevents data races when the
// SDK dispatches concurrent callbacks on different threads.
static std::mutex gPmsCallbackMutex;

#define TAF_MODEM_NAS_SVC_ID                          (0x03)
#define TAF_MODEM_WMS_SVC_ID                          (0x05)
#define TAF_MODEM_VOICE_CALL_SVC_ID                   (0x09)
#define TAF_MODEM_SIM_SVC_ID                          (0x0B)

#define TAF_MODEM_SYS_INFO_MSG_ID                     (0x004E)
#define TAF_MODEM_SMS_COMING_MSG_ID                   (0x0001)
#define TAF_MODEM_VCALL_COMING_MSG_ID                 (0x002E)
#define TAF_MODEM_SIM_PROFILE_SWAP_MSG_ID             (0x0033)

#define TAF_MODEM_WS_BIT_MASK_SMS                     (0x0001)
#define TAF_MODEM_WS_BIT_MASK_VOICE_CALL              (0x0002)
#define TAF_MODEM_WS_BIT_MASK_REMOTE_SIM_PROFILE_SWAP (0x0004)
#define TAF_MODEM_WS_BIT_MASK_SYS_INFO                (0x0008)

struct taf_pa_pms_RefStruct_t
{
    taf_prop_pms_MpssRef_t mpssRef;

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

        TAF_PA_INFO("SDK wakeup event is coming");

        if (wakeupInfo.wakeupType != telux::power::WakeupType::QMI) {
            TAF_PA_WARN("Bad info type from wakeup event");
            return;
        }

        TAF_PA_INFO("svc_id : 0x%04x", wakeupInfo.qmiWakeupInfo.serviceId);
        TAF_PA_INFO("sourceNodeId : 0x%04x", wakeupInfo.qmiWakeupInfo.sourceNodeId);
        TAF_PA_INFO("destinationNodeId : 0x%04x", wakeupInfo.qmiWakeupInfo.destinationNodeId);

        if (wakeupInfo.qmiWakeupInfo.isPIDValid)
        {
            TAF_PA_INFO("pid : 0x%04x", wakeupInfo.qmiWakeupInfo.pid);
        }

        if (wakeupInfo.qmiWakeupInfo.isProcessNameValid)
        {
            TAF_PA_INFO("processName : %s", wakeupInfo.qmiWakeupInfo.processName.c_str());
        }

        if (!wakeupInfo.qmiWakeupInfo.isMsgIdValid)
        {
            TAF_PA_ERROR("No valid msg_id for svc_id: 0x%04x",
                     wakeupInfo.qmiWakeupInfo.serviceId);
            return;
        }

        TAF_PA_INFO("msg_id : 0x%04x", wakeupInfo.qmiWakeupInfo.msgId);

        uint32_t wsBitset = 0;

        if (TAF_MODEM_WMS_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
        &&  TAF_MODEM_SMS_COMING_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
        {
            TAF_PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                    wakeupInfo.qmiWakeupInfo.serviceId,
                    wakeupInfo.qmiWakeupInfo.msgId);
            wsBitset = TAF_MODEM_WS_BIT_MASK_SMS;
        }
        else if (TAF_MODEM_VOICE_CALL_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
        &&       TAF_MODEM_VCALL_COMING_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
        {
            TAF_PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                    wakeupInfo.qmiWakeupInfo.serviceId,
                    wakeupInfo.qmiWakeupInfo.msgId);
            wsBitset = TAF_MODEM_WS_BIT_MASK_VOICE_CALL;
        }
        else if (TAF_MODEM_SIM_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
        &&       TAF_MODEM_SIM_PROFILE_SWAP_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
        {
            TAF_PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                    wakeupInfo.qmiWakeupInfo.serviceId,
                    wakeupInfo.qmiWakeupInfo.msgId);
            wsBitset = TAF_MODEM_WS_BIT_MASK_REMOTE_SIM_PROFILE_SWAP;
        }
        else if (TAF_MODEM_NAS_SVC_ID == wakeupInfo.qmiWakeupInfo.serviceId
        &&       TAF_MODEM_SYS_INFO_MSG_ID == wakeupInfo.qmiWakeupInfo.msgId)
        {
            TAF_PA_INFO("Combo [svc_id:0x%04x, msg_id:0x%04x] received <-",
                     wakeupInfo.qmiWakeupInfo.serviceId,
                     wakeupInfo.qmiWakeupInfo.msgId);
            wsBitset = TAF_MODEM_WS_BIT_MASK_SYS_INFO;
        }
        else
        {
            TAF_PA_WARN("Combo [svc_id:0x%04x, msg_id:0x%04x] is NOT in range <-",
                    wakeupInfo.qmiWakeupInfo.serviceId,
                    wakeupInfo.qmiWakeupInfo.msgId);
            return; // No need to raise the event
        }

        // Serialize payload write + RaiseEvent so concurrent wakeup
        // callbacks on different SDK threads cannot interleave.
        {
            std::lock_guard<std::mutex> cbLock(gPmsCallbackMutex);

            static uint32_t wsBitsetStorage = 0;
            wsBitsetStorage = wsBitset;

            static taf_pa_pms_Event_t ev = {
                .evType = EV_WAKEUP_INFO,
                .evPayload = &wsBitsetStorage,
                .evPsize = sizeof(wsBitsetStorage),
            };

            RaiseEvent(&ev);
        }
    }
};

class ServiceStatusListener : public telux::common::IServiceStatusListener {
public:
    void onServiceStatusChange(ServiceStatus status) override
    {
        taf_pa_pms_ServiceStatus_t svcStatus;

        if(status == ServiceStatus::SERVICE_UNAVAILABLE)
        {
            TAF_PA_ERROR( "SDK [master-service] status : UNAVAILABLE" );
            svcStatus = SVC_UNAVAILABLE;
        }
        else
        {
            TAF_PA_INFO( "SDK [master-service] status : AVAILABLE" );
            svcStatus = SVC_AVAILABLE;
        }

        std::lock_guard<std::mutex> cbLock(gPmsCallbackMutex);

        static taf_pa_pms_ServiceStatus_t svcStatusStorage = SVC_AVAILABLE;
        svcStatusStorage = svcStatus;

        static taf_pa_pms_Event_t ev = {
            .evType = EV_SVC_STATUS,
            .evPayload = &svcStatusStorage,
            .evPsize = sizeof(svcStatusStorage),
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
        TAF_PA_INFO("%s state is %s", __FUNCTION__, to_StateString(state));

        taf_pa_pms_PowerState_t pwrState;

        if(state == TcuActivityState::SUSPEND)
        {
            pwrState = taf_pa_pms_PwrSts_SUSPEND;
        }
        else if(state == TcuActivityState::SHUTDOWN)
        {
            pwrState = taf_pa_pms_PwrSts_SHUTDOWN;
        }
        else if(state == TcuActivityState::RESUME)
        {
            pwrState = taf_pa_pms_PwrSts_RESUME;
        }
        else
        {
            TAF_PA_ERROR("Unknown state from SDK");
            return; // Don't need to send out event
        }

        std::lock_guard<std::mutex> cbLock(gPmsCallbackMutex);

        static taf_pa_pms_PowerUpdateEvent_t pwrUpdate;
        pwrUpdate.state = pwrState;
        snprintf(
            pwrUpdate.machineName,
            sizeof(pwrUpdate.machineName),
            "%s",
            machineName.c_str()
        );

        static taf_pa_pms_Event_t ev = {
            .evType = EV_POWER_STATE_UPDATE,
            .evPayload = &pwrUpdate,
            .evPsize = sizeof(pwrUpdate),
        };

        RaiseEvent(&ev);
    }

    /* As both master & slave clients */
    void onMachineUpdate
    (
        const string machineName,
        const MachineEvent machineEvent
    ) override
    {
        TAF_PA_INFO("%s", __FUNCTION__);

        taf_pa_pms_MachineStatusEvent_t machEvt =
            (machineEvent == MachineEvent::AVAILABLE)
                ? MACHINE_AVAILABLE
                : MACHINE_UNAVAILABLE;

        std::lock_guard<std::mutex> cbLock(gPmsCallbackMutex);

        static taf_pa_pms_MachineUpdateEvent_t machUpdate;
        machUpdate.machineEvent = machEvt;
        snprintf(
            machUpdate.machineName,
            sizeof(machUpdate.machineName),
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
            TAF_PA_INFO("All slave apps successfully acknowledged the state transition");
        }
        else if(status == telux::common::Status::EXPIRED)
        {
            TAF_PA_INFO("Timeout occurred while waiting for ACK from slave apps");
        }
        else // telux::common::Status::FAILED
        {
            TAF_PA_ERROR("Failed to receive ACK from slave apps");
        }

        std::lock_guard<std::mutex> cbLock(gPmsCallbackMutex);

        static taf_pa_pms_ConsolidatedInfo_t info;

        if (nackResponseClients.size() > TAF_CONSOLIDATED_CLNT_SIZE)
        {
            TAF_PA_WARN("N-ACK client number is overlap");
            info.nackResponseClntSize = TAF_CONSOLIDATED_CLNT_SIZE;
        }
        else
        {
            info.nackResponseClntSize = static_cast<uint32_t>(nackResponseClients.size());
        }

        if (unresponsiveClients.size() > TAF_CONSOLIDATED_CLNT_SIZE)
        {
            TAF_PA_WARN("Unresponsive client number is overlap");
            info.unresponsiveClntSize = TAF_CONSOLIDATED_CLNT_SIZE;
        }
        else
        {
            info.unresponsiveClntSize = static_cast<uint32_t>(unresponsiveClients.size());
        }

        TAF_PA_INFO("size: [nack]/(%d) [unresp]/(%d)",
                info.nackResponseClntSize,
                info.unresponsiveClntSize);

        for (uint32_t i = 0; i < info.nackResponseClntSize; i++)
        {
            const auto& itNack = nackResponseClients[i];

            TAF_PA_INFO("Client: %s, Machine: %s",
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

        for (uint32_t i = 0; i < info.unresponsiveClntSize; i++)
        {
            const auto& itUnresp = unresponsiveClients[i];

            TAF_PA_INFO("Client: %s, Machine: %s",
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
    taf_prop_result_t errCode,
    void * cbCtx
)
{
    switch (errCode)
    {
        case TAF_PROP_UNAVAILABLE:
        {
            TAF_PA_INFO("prop-pms: SVC unavailable event captured");
        }
        break;

        default:
        {
            TAF_PA_WARN("Unknown error captured: 0x%02x", errCode);
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

    TAF_PA_INFO("Calling %s", __FUNCTION__);

    if (valid)
    {
        if (errorCode != telux::common::ErrorCode::SUCCESS)
        {
            TAF_PA_WARN("Failed to %s, err: %d, mark as N-OK",
                    description,
                    static_cast<int>(errorCode));
            rst = 1;
        }
        else
        {
            TAF_PA_INFO("[%s] PMD says OK", description);
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
                TAF_PA_WARN("[%s] Already set by another one, ignore", description);
            }
            else
            {
                TAF_PA_ERROR("[%s] Report the exception to the top level", description);
                throw; // Rare cases
            }
        }
    }
    else
    {
        TAF_PA_ERROR("[%s] Got one invalid 'promise', ignore", description);
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
            TAF_PA_INFO("[%s] service is available", description);
        }
        else
        {
            TAF_PA_ERROR("[%s] service is UN-available", description);
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
                TAF_PA_WARN("[%s] Already set by another one, ignore", description);
            }
            else
            {
                TAF_PA_ERROR("[%s] Report the exception to the top level", description);
                throw; // Rare cases
            }
        }
    }
    else
    {
        TAF_PA_ERROR("[%s] Got one invalid 'promise', ignore", description);
    }
}

// Result of waiting on a manager's init future, distinguishing a
// genuine timeout (callback still in flight) from a resolved-but-failed
// status (callback has fully returned, manager is safe to reset()).
enum class FutureCheckResult
{
    OK,       // SERVICE_AVAILABLE confirmed and callback fully unwound -
              // manager is ready; safe to reset() on any subsequent failure
    FAILED,   // Future resolved but service reported unavailable -
              // callback has fully returned, manager is safe to reset()
    TIMEOUT,  // Future did not resolve within timeoutMs -
              // callback may still be in flight, do NOT reset() manager
};

// Waits for both the status future and the done future for a manager's
// init callback, then classifies the outcome as OK, FAILED, or TIMEOUT.
//
// fu     : fulfilled by cb_ServiceStatus() with the ServiceStatus value.
// doneFu : fulfilled by the init lambda immediately after cb_ServiceStatus()
//          returns, signalling that the entire SDK callback stack has unwound.
//
// Waiting for doneFu on EVERY resolved path (OK and FAILED alike) is
// required to prevent use-after-free on the manager shared_ptr:
//
//   cb_ServiceStatus() calls promise<ServiceStatus>::set_value(), which
//   unblocks fu.get() on the Init() thread.  At that exact moment the SDK
//   callback lambda has NOT yet returned: it still has to execute
//   'd->set_value()' and then unwind back through the SDK's own dispatch
//   stack.  If Init() calls reset() on a confirmed manager immediately
//   after fu.get() returns (e.g. because a later init step fails, or
//   Deinit() runs), the destructor may fire while the SDK thread is still
//   inside the lambda - a classic use-after-free / SIGSEGV.
//
//   Waiting for doneFu guarantees that the lambda - and therefore the
//   entire SDK callback stack above it - has fully returned before Init()
//   is permitted to reset() the manager, regardless of whether the
//   outcome was OK or FAILED.
//
//   The wait is always bounded: the done promise is fulfilled
//   unconditionally at the end of the lambda, so it cannot deadlock.
static FutureCheckResult CheckManagerFuture
(
    std::future<telux::common::ServiceStatus> &fu,
    std::future<void>                         &doneFu,
    uint32_t timeoutMs,
    const char * mgrName
)
{
    auto status = fu.wait_for(std::chrono::milliseconds(timeoutMs));

    if (status == std::future_status::ready)
    {
        bool serviceAvailable = false;
        try
        {
            serviceAvailable =
                (fu.get() == telux::common::ServiceStatus::SERVICE_AVAILABLE);
        }
        catch (const std::future_error& fe)
        {
            using std::future_errc;

            if (fe.code() == std::make_error_code(future_errc::broken_promise))
            {
                TAF_PA_ERROR("No provider & can NOT set_value");
            }
            else if (fe.code() == std::make_error_code(future_errc::no_state))
            {
                TAF_PA_ERROR("Already got the future ever");
            }
            else
            {
                TAF_PA_ERROR("future_error: %s (%d) - %s",
                        fe.what(), fe.code().value(), fe.code().message().c_str());
            }
            // serviceAvailable remains false - fall through to FAILED path below.
        }

        // Wait for the done signal so the SDK callback stack has fully
        // unwound before the caller is allowed to reset() the manager.
        // This is necessary on both the OK and FAILED paths: fu becoming
        // ready only means cb_ServiceStatus() called set_value(); the
        // lambda itself (and the SDK dispatch stack above it) may still
        // be executing.  reset()-ing the manager while the SDK thread is
        // still inside the lambda causes a use-after-free.  The done
        // promise is always fulfilled after cb_ServiceStatus() returns,
        // so this wait is bounded and cannot deadlock.
        doneFu.wait();

        if (!serviceAvailable)
        {
            TAF_PA_ERROR("Failed to init %s: Service is unavailable", mgrName);
            return FutureCheckResult::FAILED;
        }

        TAF_PA_INFO("Service [%s] is AVAILABLE", mgrName);
        return FutureCheckResult::OK;
    }
    else
    {
        TAF_PA_ERROR("Timeout for preparing the %s [%u](ms)", mgrName, timeoutMs);
        return FutureCheckResult::TIMEOUT;
    }
}

/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
/* -------------------( PaFn - Fcall ) ------------------ */
/* ------------------------------------------------------ */
/* ------------------------------------------------------ */
taf_pa_result_t taf_pa_pms_Init
(
    taf_pa_pms_Reference_t  *paRefPtr,
    SendEventFunc_t          fnSendEvent,
    uint32_t                 timeoutMs
)
{
    TAF_PA_DEBUG("PA implementation.");
    std::lock_guard<std::mutex> paLock(gPmsPaMutex);

    if (paRefPtr == NULL)
    {
        TAF_PA_ERROR("Bad paRefPtr");
        return TAF_PA_FAULT;
    }

    // Reject retries after a timeout: a timed-out manager's SDK
    // InitResponseCb may still be in flight, so overwriting the
    // shared_ptr fields in 'pa' would race the SDK's callback-teardown
    // path and can SIGSEGV inside the SDK destructor.
    if (gPmsPaTerminated.load(std::memory_order_acquire))
    {
        TAF_PA_ERROR("PMS platform adaptor is in a terminal state due to a "
                 "previous timeout - process must be restarted");
        return TAF_PA_TERMINATED;
    }

    // Check if already initialized (idempotent pattern)
    if (gPmsPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("PMS platform adaptor already initialized");
        *paRefPtr = &pa;
        return TAF_PA_OK;  // Idempotent - safe to call multiple times
    }

    // Loaded the Event-Reporter for the PA layer
    {
        std::lock_guard<std::mutex> lock(pmsSendEventMutex);
        SendEvent = fnSendEvent;
    }

    TAF_PA_INFO("Loading PMS [ACTUAL] PA ...");

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

    auto masterDoneP = std::make_shared<std::promise<void>>();
    std::weak_ptr<std::promise<void>> masterDoneWP = masterDoneP;

    pa.masterMgr_ = powerFactory.getTcuActivityManager(
                    masterConfig,
                    [masterMgrWP, masterDoneWP](telux::common::ServiceStatus status)
                    {
                        cb_ServiceStatus(masterMgrWP, status, "masterMgr");
                        // Signal after cb_ServiceStatus() returns so Init() knows
                        // the full callback stack has unwound before reset().
                        if (auto d = masterDoneWP.lock()) { d->set_value(); }
                    });

    if (nullptr == pa.masterMgr_)
    {
        TAF_PA_ERROR("Failed to getTcuActivityManager for [masterMgr]");
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    std::future<telux::common::ServiceStatus> fuMaster     = masterMgrP->get_future();
    std::future<void>                         fuMasterDone = masterDoneP->get_future();

    {
        FutureCheckResult fcr = CheckManagerFuture(fuMaster, fuMasterDone, timeoutMs, "/masterMgr");
        if (fcr != FutureCheckResult::OK)
        {
            // FAILED: callback has returned, manager is safe to reset().
            // TIMEOUT: callback may still be in flight, do NOT reset() -
            //   latch terminal flag to block any future Init() retry.
            if (fcr == FutureCheckResult::FAILED)
            {
                pa.masterMgr_.reset();
            }
            else // TIMEOUT
            {
                gPmsPaTerminated.store(true, std::memory_order_release);
                TAF_PA_ERROR("PMS platform adaptor entering terminal state (masterMgr timeout)");
            }
            {
                std::lock_guard<std::mutex> lock(pmsSendEventMutex);
                SendEvent = NULL;
            }
            return (fcr == FutureCheckResult::TIMEOUT) ? TAF_PA_TIMEOUT : TAF_PA_FAULT;
        }
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

    auto slaveDoneP = std::make_shared<std::promise<void>>();
    std::weak_ptr<std::promise<void>> slaveDoneWP = slaveDoneP;

    pa.slaveMgr_ = powerFactory.getTcuActivityManager(
                    slaveConfig,
                    [slaveMgrWP, slaveDoneWP](telux::common::ServiceStatus status)
                    {
                        cb_ServiceStatus(slaveMgrWP, status, "slaveMgr");
                        // Signal after cb_ServiceStatus() returns so Init() knows
                        // the full callback stack has unwound before reset().
                        if (auto d = slaveDoneWP.lock()) { d->set_value(); }
                    });

    if (nullptr == pa.slaveMgr_)
    {
        TAF_PA_ERROR("Failed to getTcuActivityManager for [slaveMgr]");
        pa.masterMgr_.reset();
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    std::future<telux::common::ServiceStatus> fuSlave     = slaveMgrP->get_future();
    std::future<void>                         fuSlaveDone = slaveDoneP->get_future();

    {
        FutureCheckResult fcr = CheckManagerFuture(fuSlave, fuSlaveDone, timeoutMs, "/slaveMgr");
        if (fcr != FutureCheckResult::OK)
        {
            // masterMgr_ confirmed SERVICE_AVAILABLE - safe to reset().
            // slaveMgr_: reset() only if FAILED (callback returned);
            //   leave non-null on TIMEOUT (callback may still be in flight).
            pa.masterMgr_.reset();
            if (fcr == FutureCheckResult::FAILED)
            {
                pa.slaveMgr_.reset();
            }
            else // TIMEOUT
            {
                gPmsPaTerminated.store(true, std::memory_order_release);
                TAF_PA_ERROR("PMS platform adaptor entering terminal state (slaveMgr timeout)");
            }
            {
                std::lock_guard<std::mutex> lock(pmsSendEventMutex);
                SendEvent = NULL;
            }
            return (fcr == FutureCheckResult::TIMEOUT) ? TAF_PA_TIMEOUT : TAF_PA_FAULT;
        }
    }

    auto wakeupMgrP =
        std::make_shared<
            std::promise<
                telux::common::ServiceStatus>>();

    std::weak_ptr<
        std::promise<
            telux::common::ServiceStatus>> wakeupMgrWP = wakeupMgrP;

    auto wakeupDoneP = std::make_shared<std::promise<void>>();
    std::weak_ptr<std::promise<void>> wakeupDoneWP = wakeupDoneP;

    pa.wakeupMgr_ = powerFactory.getWakeupManager(
                        [wakeupMgrWP, wakeupDoneWP](telux::common::ServiceStatus status)
                        {
                            cb_ServiceStatus(wakeupMgrWP, status, "wakeupMgr");
                            // Signal after cb_ServiceStatus() returns so Init() knows
                            // the full callback stack has unwound before reset().
                            if (auto d = wakeupDoneWP.lock()) { d->set_value(); }
                        });

    if (nullptr == pa.wakeupMgr_)
    {
        TAF_PA_ERROR("Failed to getWakeupManager for [wakeupMgr]");
        pa.slaveMgr_.reset();
        pa.masterMgr_.reset();
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    std::future<telux::common::ServiceStatus> fuWakeup     = wakeupMgrP->get_future();
    std::future<void>                         fuWakeupDone = wakeupDoneP->get_future();

    {
        FutureCheckResult fcr = CheckManagerFuture(fuWakeup, fuWakeupDone, timeoutMs, "/wakeupMgr");
        if (fcr != FutureCheckResult::OK)
        {
            // masterMgr_ and slaveMgr_ both confirmed SERVICE_AVAILABLE -
            // safe to reset() regardless of FAILED or TIMEOUT.
            // wakeupMgr_: reset() only if FAILED (callback returned);
            //   leave non-null on TIMEOUT (callback may still be in flight).
            pa.slaveMgr_.reset();
            pa.masterMgr_.reset();
            if (fcr == FutureCheckResult::FAILED)
            {
                pa.wakeupMgr_.reset();
            }
            else // TIMEOUT
            {
                gPmsPaTerminated.store(true, std::memory_order_release);
                TAF_PA_ERROR("PMS platform adaptor entering terminal state (wakeupMgr timeout)");
            }
            {
                std::lock_guard<std::mutex> lock(pmsSendEventMutex);
                SendEvent = NULL;
            }
            return (fcr == FutureCheckResult::TIMEOUT) ? TAF_PA_TIMEOUT : TAF_PA_FAULT;
        }
    }

    // All SDK service managers are ready.

    telux::common::Status sdkStatus = telux::common::Status::FAILED;

    pa.wakeupReasonListener_ = std::make_shared<WakeupReasonListener>();

    telux::common::ErrorCode err =
        pa.wakeupMgr_->registerListener(pa.wakeupReasonListener_);

    if (err != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("Failed to register wakeup listener");
        pa.wakeupReasonListener_.reset();
        pa.wakeupMgr_.reset();
        pa.slaveMgr_.reset();
        pa.masterMgr_.reset();
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    pa.masterSvcListener_ = std::make_shared<ServiceStatusListener>();

    sdkStatus = pa.masterMgr_->registerServiceStateListener(pa.masterSvcListener_);

    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to register for /masterMgr svc avaiable listener");
        // Non-fatal failure, continue ...
    }

    pa.masterStateUpdateListener_ = std::make_shared<StateUpdateListener>();

    sdkStatus = pa.masterMgr_->registerListener(pa.masterStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to register the ITcuActivityListener for /masterMgr");
        if (pa.masterMgr_ != nullptr && pa.masterSvcListener_ != nullptr)
        {
            (void)pa.masterMgr_->deregisterServiceStateListener(pa.masterSvcListener_);
        }
        if (pa.wakeupMgr_ != nullptr && pa.wakeupReasonListener_ != nullptr)
        {
            (void)pa.wakeupMgr_->deRegisterListener(pa.wakeupReasonListener_);
        }
        pa.masterStateUpdateListener_.reset();
        pa.masterSvcListener_.reset();
        pa.wakeupReasonListener_.reset();
        pa.wakeupMgr_.reset();
        pa.slaveMgr_.reset();
        pa.masterMgr_.reset();
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    pa.slaveStateUpdateListener_ = std::make_shared<StateUpdateListener>();

    sdkStatus = pa.slaveMgr_->registerListener(pa.slaveStateUpdateListener_);
    if (sdkStatus != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to register the ITcuActivityListener for /slaveMgr");
        if (pa.masterMgr_ != nullptr && pa.masterStateUpdateListener_ != nullptr)
        {
            (void)pa.masterMgr_->deregisterListener(pa.masterStateUpdateListener_);
        }
        if (pa.masterMgr_ != nullptr && pa.masterSvcListener_ != nullptr)
        {
            (void)pa.masterMgr_->deregisterServiceStateListener(pa.masterSvcListener_);
        }
        if (pa.wakeupMgr_ != nullptr && pa.wakeupReasonListener_ != nullptr)
        {
            (void)pa.wakeupMgr_->deRegisterListener(pa.wakeupReasonListener_);
        }
        pa.slaveStateUpdateListener_.reset();
        pa.masterStateUpdateListener_.reset();
        pa.masterSvcListener_.reset();
        pa.wakeupReasonListener_.reset();
        pa.wakeupMgr_.reset();
        pa.slaveMgr_.reset();
        pa.masterMgr_.reset();
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    taf_prop_result_t result =
        taf_prop_pms_Init(
            &pa.mpssRef,
            PaMpssErrorCallback,
            NULL);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to taf_prop_pms_Init: err(%d)", result);
        if (pa.slaveMgr_ != nullptr && pa.slaveStateUpdateListener_ != nullptr)
        {
            (void)pa.slaveMgr_->deregisterListener(pa.slaveStateUpdateListener_);
        }
        if (pa.masterMgr_ != nullptr && pa.masterStateUpdateListener_ != nullptr)
        {
            (void)pa.masterMgr_->deregisterListener(pa.masterStateUpdateListener_);
        }
        if (pa.masterMgr_ != nullptr && pa.masterSvcListener_ != nullptr)
        {
            (void)pa.masterMgr_->deregisterServiceStateListener(pa.masterSvcListener_);
        }
        if (pa.wakeupMgr_ != nullptr && pa.wakeupReasonListener_ != nullptr)
        {
            (void)pa.wakeupMgr_->deRegisterListener(pa.wakeupReasonListener_);
        }
        pa.slaveStateUpdateListener_.reset();
        pa.masterStateUpdateListener_.reset();
        pa.masterSvcListener_.reset();
        pa.wakeupReasonListener_.reset();
        pa.wakeupMgr_.reset();
        pa.slaveMgr_.reset();
        pa.masterMgr_.reset();
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }
        return TAF_PA_FAULT;
    }

    *paRefPtr = &pa;

    gPmsPaInitialized.store(true, std::memory_order_release);
    TAF_PA_INFO("PMS platform adaptor initialization flag set to true.");
    TAF_PA_INFO("PMS [ACTUAL] PA loaded");
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_pms_Deinit
(
    taf_pa_pms_Reference_t *paRefPtr
)
{
    TAF_PA_DEBUG("PA implementation.");
    std::lock_guard<std::mutex> paLock(gPmsPaMutex);

    const bool isInitialized = gPmsPaInitialized.load(std::memory_order_acquire);
    const bool isTerminated  = gPmsPaTerminated.load(std::memory_order_acquire);

    // Handle the terminal (partial-init timeout) state: release every manager
    // that confirmed SERVICE_AVAILABLE before the timeout, then return
    // PA_TERMINAL so the caller knows the process must be restarted.
    if (isTerminated)
    {
        TAF_PA_WARN("Deinit() called in terminal state - releasing confirmed managers");

        // Clear the event callback so no further events are raised.
        {
            std::lock_guard<std::mutex> lock(pmsSendEventMutex);
            SendEvent = NULL;
        }

        // Init() reset() every manager that confirmed SERVICE_AVAILABLE
        // before the timeout, leaving those pointers null.  The one
        // manager that timed out was intentionally left non-null because
        // its SDK InitResponseCb may still be in flight.
        //
        // Rule: reset() a pointer here ONLY if Init() already cleared it
        // (i.e. it is null).  Never reset() the non-null timed-out one.
        //
        // Scenario mapping (Init() cleanup before PA_TIMEOUT return):
        //   masterMgr_ timeout : Init() reset() nothing.
        //     masterMgr_ non-null (in-flight) -> skip.
        //     slaveMgr_  null (never created) -> no-op reset() safe.
        //     wakeupMgr_ null (never created) -> no-op reset() safe.
        //
        //   slaveMgr_ timeout  : Init() reset() masterMgr_.
        //     masterMgr_ null  (already cleared) -> no-op reset() safe.
        //     slaveMgr_  non-null (in-flight)    -> skip.
        //     wakeupMgr_ null  (never created)   -> no-op reset() safe.
        //
        //   wakeupMgr_ timeout : Init() reset() masterMgr_ + slaveMgr_.
        //     masterMgr_ null  (already cleared) -> no-op reset() safe.
        //     slaveMgr_  null  (already cleared) -> no-op reset() safe.
        //     wakeupMgr_ non-null (in-flight)    -> skip.
        //
        // In all three cases listener shared_ptrs were never allocated,
        // so there is nothing to deregister.
        if (pa.masterMgr_ == nullptr) { pa.masterMgr_.reset(); }
        if (pa.slaveMgr_  == nullptr) { pa.slaveMgr_.reset();  }
        if (pa.wakeupMgr_ == nullptr) { pa.wakeupMgr_.reset(); }

        return TAF_PA_TERMINATED;
    }

    // Normal path: reject if Init() was never successfully completed.
    if (!isInitialized)
    {
        TAF_PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return TAF_PA_FAULT;
    }

    if (paRefPtr == NULL || *paRefPtr != &pa)
    {
        TAF_PA_ERROR("Bad paRefPtr");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Starting PMS PA deinitialization...");

    // Clear the event-reporter callback first so callbacks racing with deinit
    // cannot raise events after teardown starts.
    TAF_PA_INFO("Clearing SendEvent callback");
    {
        std::lock_guard<std::mutex> lock(pmsSendEventMutex);
        SendEvent = NULL;
    }

    // Step 1: Deregister wakeup listener and release its shared pointer so no
    // further wakeup callbacks are dispatched after this point.
    telux::common::ErrorCode err;
    if (pa.wakeupMgr_ != nullptr && pa.wakeupReasonListener_ != nullptr)
    {
        TAF_PA_INFO("Deregistering wakeup listener [wakeupMgr]");
        err = pa.wakeupMgr_->deRegisterListener(pa.wakeupReasonListener_);
        if (err != telux::common::ErrorCode::SUCCESS)
        {
            TAF_PA_ERROR("Failed to call SDK deRegisterListener [wakeupMgr]");
        }
    }
    pa.wakeupReasonListener_.reset();

    telux::common::Status sdkStatus = telux::common::Status::FAILED;

    // Step 2: Deregister service state listener from master manager and release
    // its shared pointer.
    if (pa.masterMgr_ != nullptr && pa.masterSvcListener_ != nullptr)
    {
        TAF_PA_INFO("Deregistering service state listener [masterMgr]");
        sdkStatus = pa.masterMgr_->deregisterServiceStateListener(pa.masterSvcListener_);
        if (sdkStatus != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("Failed to call SDK deregisterServiceStateListener [masterMgr]");
        }
    }
    pa.masterSvcListener_.reset();

    // Step 3: Deregister TCU activity listener from master manager and release
    // its shared pointer.
    if (pa.masterMgr_ != nullptr && pa.masterStateUpdateListener_ != nullptr)
    {
        TAF_PA_INFO("Deregistering state update listener [masterMgr]");
        sdkStatus = pa.masterMgr_->deregisterListener(pa.masterStateUpdateListener_);
        if (sdkStatus != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("Failed to call SDK deregisterListener [masterMgr]");
        }
    }
    pa.masterStateUpdateListener_.reset();

    // Step 4: Deregister TCU activity listener from slave manager and release
    // its shared pointer.
    if (pa.slaveMgr_ != nullptr && pa.slaveStateUpdateListener_ != nullptr)
    {
        TAF_PA_INFO("Deregistering state update listener [slaveMgr]");
        sdkStatus = pa.slaveMgr_->deregisterListener(pa.slaveStateUpdateListener_);
        if (sdkStatus != telux::common::Status::SUCCESS)
        {
            TAF_PA_ERROR("Failed to call SDK deregisterListener [slaveMgr]");
        }
    }
    pa.slaveStateUpdateListener_.reset();

    // Step 5: Deinitialize the ns-layer QMI client to release modem resources.
    if (pa.mpssRef != nullptr)
    {
        TAF_PA_INFO("Deinitializing ns-layer PMS QMI client");
        taf_prop_result_t nsResult = taf_prop_pms_Deinit(&pa.mpssRef);
        if (nsResult != TAF_PROP_OK)
        {
            TAF_PA_ERROR("taf_prop_pms_Deinit failed: err(%d)", nsResult);
            // Continue cleanup even if ns-layer deinit failed
        }
    }

    // Step 6: Reset SDK manager shared pointers so the underlying SDK objects
    // are released once no other owners remain.
    TAF_PA_INFO("Resetting wakeupMgr_, masterMgr_ and slaveMgr_");
    pa.wakeupMgr_.reset();
    pa.masterMgr_.reset();
    pa.slaveMgr_.reset();

    *paRefPtr = NULL; // Reset caller reference pointer

    gPmsPaInitialized.store(false, std::memory_order_release);
    TAF_PA_INFO("PMS platform adaptor initialization flag reset to false.");
    TAF_PA_INFO("PMS PA deinitialization complete.");
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_pms_SetPowerStateAsMaster
(
    taf_pa_pms_Reference_t         paRef,
    taf_pa_pms_PowerState_t        state,
    const char               *name
)
{
    if (!gPmsPaInitialized.load(std::memory_order_acquire) || pa.masterMgr_ == nullptr)
    {
        TAF_PA_ERROR("SetPowerStateAsMaster called before Init() or after Deinit()");
        return TAF_PA_FAULT;
    }

    telux::power::TcuActivityState sdkState = to_SdkPowerState(state);

    telux::common::Status status = telux::common::Status::FAILED;
    taf_pa_result_t rst = TAF_PA_OK;

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
            TAF_PA_ERROR("waiting promise timeout for %d seconds",
                     SET_STATE_TIMEOUT);
            rst = TAF_PA_FAULT;
        }
        else
        {
            rst = fu.get() == 0 ? TAF_PA_OK : TAF_PA_FAULT;
        }
    }
    else
    {
        TAF_PA_ERROR("Failed to call SDK setActivityState");
        rst = TAF_PA_FAULT;
    }

    return rst;
}

taf_pa_result_t taf_pa_pms_SendAckForStateUpdate
(
    taf_pa_pms_Reference_t  paRef,
    taf_pa_pms_PowerState_t state,
    taf_pa_pms_Ack_t        ack
)
{
    if (!gPmsPaInitialized.load(std::memory_order_acquire) || pa.slaveMgr_ == nullptr)
    {
        TAF_PA_ERROR("SendAckForStateUpdate called before Init() or after Deinit()");
        return TAF_PA_FAULT;
    }

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
            TAF_PA_ERROR("Failed to send [%s] for [%s] state",
                     ack == taf_pa_pms_ACK ? "ACK": "NACK",
                     "SUSPEND");
            return TAF_PA_FAULT;
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
            TAF_PA_ERROR("Failed to send [%s] for [%s] state",
                     ack == taf_pa_pms_ACK ? "ACK": "NACK",
                     "SHUTDOWN");
            return TAF_PA_FAULT;
        }
    }
    else
    {
        TAF_PA_WARN("Unsupported state to be set: %d, rejected", state);
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Successfully send [%s] to PMD with [%s] state",
            ack == taf_pa_pms_ACK ? "ACK": "NACK",
            state == taf_pa_pms_PwrSts_SUSPEND
                ? "SUSPEND"
                : "SHUTDOWN");

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_pms_GetAllMachineNames
(
    taf_pa_pms_Reference_t    paRef,
    std::vector<std::string> &machineNames
)
{
    if (!gPmsPaInitialized.load(std::memory_order_acquire) || pa.masterMgr_ == nullptr)
    {
        TAF_PA_ERROR("GetAllMachineNames called before Init() or after Deinit()");
        return TAF_PA_FAULT;
    }

    telux::common::Status status =
        pa.masterMgr_->getAllMachineNames(machineNames);

    if (status != Status::SUCCESS)
    {
        TAF_PA_ERROR("Failed to getAllMachineNames");
        return TAF_PA_FAULT;
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_pms_SetModemWakeupFilter
(
    taf_pa_pms_Reference_t    paRef,
    uint32_t                  wsBitmask
)
{
    if (!gPmsPaInitialized.load(std::memory_order_acquire) || pa.mpssRef == nullptr)
    {
        TAF_PA_ERROR("SetModemWakeupFilter called before Init() or after Deinit()");
        return TAF_PA_FAULT;
    }

     taf_prop_result_t result =
        taf_prop_pms_SetWsFilter(pa.mpssRef,
                (taf_prop_pms_ModemWakeupSource_t) wsBitmask);

    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to taf_prop_pms_SetWsFilter");
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_pms_GetModemWakeupFilter
(
    taf_pa_pms_Reference_t    paRef,
    uint32_t                 *wsBitmaskPtr
)
{
    if (!gPmsPaInitialized.load(std::memory_order_acquire) || pa.mpssRef == nullptr)
    {
        if (wsBitmaskPtr != nullptr) { *wsBitmaskPtr = 0; }
        TAF_PA_ERROR("GetModemWakeupFilter called before Init() or after Deinit()");
        return TAF_PA_FAULT;
    }

     taf_prop_result_t result =
        taf_prop_pms_GetWsFilter(pa.mpssRef,
                (taf_prop_pms_ModemWakeupSource_t *) wsBitmaskPtr);

    if (result != TAF_PROP_OK)
    {
        if (wsBitmaskPtr != nullptr)
        {
            *wsBitmaskPtr = 0;
        }
        TAF_PA_ERROR("Failed to taf_prop_pa_pms_GetWsFilter");
        return TAF_PA_FAULT;
    }

    return TAF_PA_OK;
}
