/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include <cstring>
#include <future>
#include <condition_variable>
#include <atomic>
#include <mutex>

#include <telux/common/CommonDefines.hpp>
#include <telux/platform/PlatformFactory.hpp>
#include <telux/platform/FsDefines.hpp>
#include <telux/platform/FsManager.hpp>

#include "tafMrcPa.hpp"

#include "taf_prop_mrc.h"

#define SERVICE_TIMEOUT 5

using namespace std;
using namespace telux::common;
using namespace telux::platform;

// Thread-safe initialization flag
static std::atomic<bool> gMrcPaInitialized(false);
static std::mutex gMrcPaMutex;

#define SERVICE_PROMISE_AND_CALLBACK(name)                                \
    auto name##Promise = make_shared<promise<ServiceStatus>>();           \
    auto name##Callback = [name##Promise](ServiceStatus status)           \
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

#define SERVICE_READY(name)                                              \
    future<ServiceStatus> name##Future = name##Promise->get_future();    \
    future_status name##Status = name##Future.wait_for(                  \
        chrono::seconds(SERVICE_TIMEOUT));                               \
        ServiceStatus name##ServiceStatus;                               \
        if (future_status::timeout == name##Status)                      \
            PA_CRIT("Timeout for %s.", #name);                           \
        else                                                             \
        {                                                                \
            name##ServiceStatus = name##Future.get();                    \
            if (name##ServiceStatus != ServiceStatus::SERVICE_AVAILABLE) \
                PA_CRIT("%s is not available.", #name);                  \
            else                                                         \
                PA_INFO("%s is available.", #name);                      \
        }

typedef struct
{
    void* handlerFuncPtr;
    void* contextPtr;
} Handler_t;

typedef struct
{
    Handler_t processStatus;
    Handler_t scrubStatus;
} Indicator_t;

typedef struct
{
    shared_ptr<IFsManager> fs;
} Manager_t;

class PlatformAdaptor
{
    public:
        Indicator_t indicators;
        Manager_t managers;
        std::mutex indicatorMutex;
        // protects the shared_ptr itself against concurrent read/write
        // (Init/Deinit write vs NB read). Do not hold this while waiting on SDK callbacks.
        std::mutex fsMutex;

        // Serializes IFsManager operations (OTA/ABSync) without blocking Init/Deinit
        // on long SDK waits. Each operation takes a local shared_ptr under fsMutex,
        // then waits using the local manager reference.
        std::mutex fsOperationMutex;

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
                static taf_pa_mrc_Result_t Result
                (
                    taf_prop_mrc_Result_t result
                );

                static taf_prop_mrc_Process_t Process
                (
                    taf_pa_mrc_Process_t process
                );

                static taf_pa_mrc_Process_t Process
                (
                    taf_prop_mrc_Process_t process
                );

                static taf_prop_mrc_Status_t Status
                (
                    taf_pa_mrc_Status_t status
                );

                static taf_pa_mrc_Status_t Status
                (
                    taf_prop_mrc_Status_t status
                );

                static taf_prop_mrc_Timer_t Timer
                (
                    taf_pa_mrc_Timer_t timer
                );
        };
};

taf_pa_mrc_Result_t Utility::Convert::Result
(
    taf_prop_mrc_Result_t result
)
{
    switch (result)
    {
        case TAF_PROP_MRC_RESULT_SUCCESS:
            return TAF_PA_MRC_RESULT_SUCCESS;
        case TAF_PROP_MRC_RESULT_FAILURE:
            return TAF_PA_MRC_RESULT_FAILURE;
        default:
            PA_DEBUG("Unknown result %d.", result);
    }

    return TAF_PA_MRC_RESULT_UNKNOWN;
}

taf_prop_mrc_Process_t Utility::Convert::Process
(
    taf_pa_mrc_Process_t process
)
{
    switch (process)
    {
        case TAF_PA_MRC_PROCESS_ABSYNC:
            return TAF_PROP_MRC_PROCESS_ABSYNC;
        case TAF_PA_MRC_PROCESS_OTA:
            return TAF_PROP_MRC_PROCESS_OTA;
        default:
            PA_DEBUG("Unknown process %d.", process);
    }

    return TAF_PROP_MRC_PROCESS_UNKNOWN;
}

taf_pa_mrc_Process_t Utility::Convert::Process
(
    taf_prop_mrc_Process_t process
)
{
    switch (process)
    {
        case TAF_PROP_MRC_PROCESS_ABSYNC:
            return TAF_PA_MRC_PROCESS_ABSYNC;
        case TAF_PROP_MRC_PROCESS_OTA:
            return TAF_PA_MRC_PROCESS_OTA;
        default:
            PA_DEBUG("Unknown process %d.", process);
    }

    return TAF_PA_MRC_PROCESS_UNKNOWN;
}

taf_prop_mrc_Status_t Utility::Convert::Status
(
    taf_pa_mrc_Status_t status
)
{
    switch (status)
    {
        case TAF_PA_MRC_STATUS_INITIATED:
            return TAF_PROP_MRC_STATUS_INITIATED;
        case TAF_PA_MRC_STATUS_RESUMED:
            return TAF_PROP_MRC_STATUS_RESUMED;
        case TAF_PA_MRC_STATUS_SUCCEEDED:
            return TAF_PROP_MRC_STATUS_SUCCEEDED;
        case TAF_PA_MRC_STATUS_FAILED:
            return TAF_PROP_MRC_STATUS_FAILED;
        default:
            PA_DEBUG("Unknown status %d.", status);
    }

    return TAF_PROP_MRC_STATUS_UNKNOWN;
}

taf_pa_mrc_Status_t Utility::Convert::Status
(
    taf_prop_mrc_Status_t status
)
{
    switch (status)
    {
        case TAF_PROP_MRC_STATUS_INITIATED:
            return TAF_PA_MRC_STATUS_INITIATED;
        case TAF_PROP_MRC_STATUS_RESUMED:
            return TAF_PA_MRC_STATUS_RESUMED;
        case TAF_PROP_MRC_STATUS_SUCCEEDED:
            return TAF_PA_MRC_STATUS_SUCCEEDED;
        case TAF_PROP_MRC_STATUS_FAILED:
            return TAF_PA_MRC_STATUS_FAILED;
        default:
            PA_DEBUG("Unknown status %d.", status);
    }

    return TAF_PA_MRC_STATUS_UNKNOWN;
}

taf_prop_mrc_Timer_t Utility::Convert::Timer
(
    taf_pa_mrc_Timer_t timer
)
{
    switch (timer)
    {
        case TAF_PA_MRC_TIMER_SCRUB:
            return TAF_PROP_MRC_TIMER_SCRUB;
        case TAF_PA_MRC_TIMER_EFS_BACKUP:
            return TAF_PROP_MRC_TIMER_EFS_BACKUP;
        case TAF_PA_MRC_TIMER_SUSPEND:
            return TAF_PROP_MRC_TIMER_SUSPEND;
        case TAF_PA_MRC_TIMER_DEFER:
            return TAF_PROP_MRC_TIMER_DEFER;
        default:
            PA_DEBUG("Unknown timer %d.", timer);
    }

    return TAF_PROP_MRC_TIMER_UNKNOWN;
}

PlatformAdaptor& PlatformAdaptor::GetInstance
(
    void
)
{
    static PlatformAdaptor instance;
    return instance;
}

static void ProcessStatusHandler
(
    taf_prop_mrc_ProcessStatusIndication_t indication,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFunc;
    void* ctx;
    {
        std::lock_guard<std::mutex> lock(pa.indicatorMutex);
        handlerFunc = (taf_pa_mrc_ProcessStatusHdlrFunc_t)pa.indicators.processStatus.handlerFuncPtr;
        ctx = pa.indicators.processStatus.contextPtr;
    }

    if (handlerFunc != nullptr)
    {
        taf_pa_mrc_ProcessStatusIndication_t paIndication;

        paIndication.processValid = indication.processValid;
        if (paIndication.processValid)
            paIndication.process = Utility::Convert::Process(indication.process);

        paIndication.statusValid = indication.statusValid;
        if (paIndication.statusValid)
            paIndication.status = Utility::Convert::Status(indication.status);

        paIndication.resultValid = indication.resultValid;
        if (paIndication.resultValid)
            paIndication.result = Utility::Convert::Result(indication.result);

        handlerFunc(paIndication, ctx);
    }
}

static void ScrubStatusHandler
(
    taf_prop_mrc_ScrubStatusIndication_t indication,
    void* contextPtr
)
{
    PA_INFO("Scrub GPIO toggle requested");
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_mrc_ScrubStatusHdlrFunc_t handlerFunc;
    void* ctx;
    {
        std::lock_guard<std::mutex> lock(pa.indicatorMutex);
        handlerFunc = (taf_pa_mrc_ScrubStatusHdlrFunc_t)pa.indicators.scrubStatus.handlerFuncPtr;
        ctx = pa.indicators.scrubStatus.contextPtr;
    }

    if (handlerFunc != nullptr)
    {
        taf_pa_mrc_ScrubStatusIndication_t paIndication;
        paIndication.slotToggleRequested = indication.slotToggleRequested;
        handlerFunc(paIndication, ctx);
    }
}

pa_result_t taf_pa_mrc_Init()
{
    PA_DEBUG("PA implementation.");
    std::lock_guard<std::mutex> paLock(gMrcPaMutex);

    // Check if already initialized (idempotent pattern)
    if (gMrcPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("MRC platform adaptor already initialized");
        return PA_OK;  // Idempotent - safe to call multiple times
    }

    auto& pa = PlatformAdaptor::GetInstance();
    auto& platformFactory = PlatformFactory::getInstance();

    SERVICE_PROMISE_AND_CALLBACK(fs)
    // assign pa.managers.fs under fsMutex so that a concurrent NB call
    // that reads pa.managers.fs cannot observe a null or partially-constructed shared_ptr.
    {
        std::lock_guard<std::mutex> lock(pa.fsMutex);
        pa.managers.fs = platformFactory.getFsManager(fsCallback);
    }
    SERVICE_READY(fs)

    PA_INFO("MRC platform adaptor initialization is done.");
    int32_t result = taf_prop_mrc_Init();
    if (result == -ENOSYS)
        PA_INFO("MRC proprietary platform adaptor is not implemented.");
    else if (result == 0)
    {
        taf_prop_mrc_AddProcessStatusHandler(ProcessStatusHandler, nullptr);
        taf_prop_mrc_AddScrubStatusHandler(ScrubStatusHandler, nullptr);
        PA_INFO("MRC proprietary platform adaptor initialization is done.");
    }

    gMrcPaInitialized.store(true, std::memory_order_release);
    PA_INFO("MRC platform adaptor initialization flag set to true.");
    return 0;
}

pa_result_t taf_pa_mrc_RegisterIndication
(
    uint8_t registration
)
{
    return taf_prop_mrc_RegisterIndication(registration);
}

pa_result_t taf_pa_mrc_SetProcessStatus
(
    taf_pa_mrc_Process_t process,
    taf_pa_mrc_Status_t status
)
{
    pa_result_t result = 0;
    switch (process)
    {
        case TAF_PA_MRC_PROCESS_ABSYNC:
        {
            taf_prop_mrc_Status_t nsStatus = Utility::Convert::Status(status);
            result = taf_prop_mrc_SetProcessStatus(TAF_PROP_MRC_PROCESS_ABSYNC, nsStatus);
            break;
        }
        case TAF_PA_MRC_PROCESS_OTA:
        {
            Status paStatus = Status::SUCCESS;
            auto promisePtr = make_shared<promise<ErrorCode>>();
            auto callback = [promisePtr](ErrorCode error)
            {
                try
                {
                    promisePtr->set_value(error);
                }
                catch (const future_error& e)
                {
                    PA_ERROR("Future error in callback: %s", e.what());
                }
                catch (const exception& e)
                {
                    PA_ERROR("Exception in callback: %s", e.what());
                }
                catch (...)
                {
                    PA_ERROR("Unknown error in callback.");
                }
            };

            auto& pa = PlatformAdaptor::GetInstance();

            // serialize IFsManager operations, but do not hold fsMutex
            // across the blocking wait. fsMutex only protects pa.managers.fs itself.
            // The local shared_ptr keeps IFsManager alive even if Deinit resets the
            // global pointer while this asynchronous operation is in flight.
            std::shared_ptr<IFsManager> fsManager;
            {
                std::lock_guard<std::mutex> operationLock(pa.fsOperationMutex);
                {
                    std::lock_guard<std::mutex> fsLock(pa.fsMutex);
                    fsManager = pa.managers.fs;
                }

                if (!fsManager)
                {
                    PA_ERROR("FsManager not initialized — cannot set OTA status %d.", status);
                    return PA_FAULT;
                }

                switch (status)
                {
                    case TAF_PA_MRC_STATUS_INITIATED:
                        paStatus = fsManager->prepareForOta(OtaOperation::START, callback);
                        break;
                    case TAF_PA_MRC_STATUS_RESUMED:
                        paStatus = fsManager->prepareForOta(OtaOperation::RESUME, callback);
                        break;
                    case TAF_PA_MRC_STATUS_SUCCEEDED:
                        paStatus = fsManager->otaCompleted(OperationStatus::SUCCESS, callback);
                        break;
                    case TAF_PA_MRC_STATUS_FAILED:
                        paStatus = fsManager->otaCompleted(OperationStatus::FAILURE, callback);
                        break;
                    default:
                        PA_ERROR("Unknown status %d.", status);
                        return -EINVAL;
                }

                ErrorCode error = promisePtr->get_future().get();
                if (paStatus != Status::SUCCESS || error != ErrorCode::SUCCESS)
                {
                    PA_ERROR("Failed to set OTA status %d, ret = %d, error = %d.", status,
                        (int)paStatus, (int)error);
                    return -EFAULT;
                }
            }

            result = 0;
            break;
        }
        default:
            PA_ERROR("Unknown process %d.", process);
            return -EINVAL;
    }

    return result;
}

pa_result_t taf_pa_mrc_PerformABSync
(
    void
)
{
    auto promisePtr = make_shared<promise<ErrorCode>>();
    auto callback = [promisePtr](ErrorCode error)
    {
        try
        {
            promisePtr->set_value(error);
        }
        catch (const future_error& e)
        {
            PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in callback.");
        }
    };

    auto& pa = PlatformAdaptor::GetInstance();

    // serialize IFsManager operations, but do not hold fsMutex
    // across the blocking wait. fsMutex only protects pa.managers.fs itself.
    // The local shared_ptr keeps IFsManager alive even if Deinit resets the
    // global pointer while this asynchronous operation is in flight.
    std::shared_ptr<IFsManager> fsManager;
    telux::common::Status status = telux::common::Status::SUCCESS;
    ErrorCode error = ErrorCode::SUCCESS;
    {
        std::lock_guard<std::mutex> operationLock(pa.fsOperationMutex);
        {
            std::lock_guard<std::mutex> fsLock(pa.fsMutex);
            fsManager = pa.managers.fs;
        }

        if (!fsManager)
        {
            PA_ERROR("FsManager not initialized — cannot perform ABSync.");
            return PA_FAULT;
        }

        status = fsManager->startAbSync(callback);
        error = promisePtr->get_future().get();
    }
    if (status != Status::SUCCESS || error != ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to set OTA status %d, ret = %d, error = %d.", status, (int)status,
            (int)error);
        return -EFAULT;
    }

    return 0;
}

pa_result_t taf_pa_mrc_AddProcessStatusHandler
(
    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_mrc_ProcessStatusHandlerRef_t* handlerRefPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    {
        std::lock_guard<std::mutex> lock(pa.indicatorMutex);
        pa.indicators.processStatus.handlerFuncPtr = (void*)handlerFuncPtr;
        pa.indicators.processStatus.contextPtr = contextPtr;
    }

    if (handlerRefPtr) *handlerRefPtr = nullptr;
    return PA_OK;
}

pa_result_t taf_pa_mrc_GetEfsPeStatus
(
    taf_pa_mrc_EfsPeStatus_t* statusPtr
)
{
    taf_prop_mrc_EfsPeStatus_t status;
    int32_t result = taf_prop_mrc_GetEfsPeStatus(&status);
    uint32_t i;
    for (i = 0; i < TAF_PA_MRC_EFS_PARTITION_BLOCKS && i < TAF_PROP_MRC_EFS_PARTITION_BLOCKS
        && i < status.peCountLen; i++)
        statusPtr->peCount[i] = status.peCount[i];

    statusPtr->peCountLen = i;

    return result;
}

pa_result_t taf_pa_mrc_GetEfsBlockStatus
(
    taf_pa_mrc_EfsBlockStatus_t* statusPtr
)
{
    taf_prop_mrc_EfsBlockStatus_t status;
    int32_t result = taf_prop_mrc_GetEfsBlockStatus(&status);
    statusPtr->maxEraseCount = status.maxEraseCount;
    statusPtr->totalBadBlocks = status.totalBadBlocks;

    return result;
}

pa_result_t taf_pa_mrc_GetEfsUsageStats
(
    taf_pa_mrc_EfsUsageStats_t* statsPtr
)
{
    if (statsPtr == nullptr)
    {
        PA_ERROR("statsPtr is nullptr.");
        return -EINVAL;
    }

    memset(statsPtr, 0, sizeof(*statsPtr));
    PA_INFO("statsPtr = %p", (void *)statsPtr);

    taf_prop_mrc_EfsUsageStats_t stats;
    memset(&stats, 0, sizeof(stats));

    int32_t result = taf_prop_mrc_GetEfsUsageStats(&stats);
    PA_INFO("result from taf_prop_mrc_GetEfsUsageStats = %d", result);
    if (result != 0)
        return result;

    uint32_t i;
    for (i = 0; i < TAF_PA_MRC_EFS_PARTITION_BLOCKS && i < TAF_PROP_MRC_EFS_PARTITION_BLOCKS
        && i < stats.blockStatsLen; i++)
    {
        statsPtr->blockStats[i].blockReadStats = stats.blockStats[i].blockReadStats;
        statsPtr->blockStats[i].blockEraseStats = stats.blockStats[i].blockEraseStats;
        statsPtr->blockStats[i].blockEccBitflipStats = stats.blockStats[i].blockEccBitflipStats;
        PA_INFO("EFS blockStats[%u]: read=%u erase=%u ecc=%u", i,
            (unsigned int)statsPtr->blockStats[i].blockReadStats,
            (unsigned int)statsPtr->blockStats[i].blockEraseStats,
            (unsigned int)statsPtr->blockStats[i].blockEccBitflipStats);
    }
    statsPtr->blockStatsLen = i;
    PA_INFO("EFS blockStatsLen = %u", statsPtr->blockStatsLen);

    for (i = 0; i < TAF_PA_MRC_EFS_WRITE_TASKS && i < TAF_PROP_MRC_EFS_WRITE_TASKS
        && i < stats.clientListLen; i++)
    {
        statsPtr->clientList[i].writeCallCounters = stats.clientList[i].writeCallCounters;
        statsPtr->clientList[i].maxNbyte = stats.clientList[i].maxNbyte;
        statsPtr->clientList[i].taskNameLen = stats.clientList[i].taskNameLen;
        memcpy(statsPtr->clientList[i].taskName, stats.clientList[i].taskName,
            TAF_PA_MRC_EFS_TASK_NAME_LEN);
        PA_INFO("EFS clientList[%u]: writeCalls=%u maxNbyte=%u taskNameLen=%u taskName=%.*s", i,
            (unsigned int)statsPtr->clientList[i].writeCallCounters,
            (unsigned int)statsPtr->clientList[i].maxNbyte,
            (unsigned int)statsPtr->clientList[i].taskNameLen,
            (int)statsPtr->clientList[i].taskNameLen,
            statsPtr->clientList[i].taskName);
    }
    statsPtr->clientListLen = i;
    PA_INFO("result = %d, EFS clientListLen = %u", result, statsPtr->clientListLen);
    return result;
}

pa_result_t taf_pa_mrc_SetTimerPeriod
(
    taf_pa_mrc_Timer_t timer,
    uint32_t period
)
{
    taf_prop_mrc_Timer_t nsTimer = Utility::Convert::Timer(timer);
    return taf_prop_mrc_SetTimerPeriod(nsTimer, period);
}

pa_result_t taf_pa_mrc_AddScrubStatusHandler
(
    taf_pa_mrc_ScrubStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_mrc_ScrubStatusHandlerRef_t* handlerRefPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    {
        std::lock_guard<std::mutex> lock(pa.indicatorMutex);
        pa.indicators.scrubStatus.handlerFuncPtr = (void*)handlerFuncPtr;
        pa.indicators.scrubStatus.contextPtr = contextPtr;
    }
    if (handlerRefPtr) *handlerRefPtr = nullptr;
    return PA_OK;
}

pa_result_t taf_pa_mrc_AckSlotToggle
(
    int32_t success
)
{
    return taf_prop_mrc_AckSlotToggle(success);
}

pa_result_t taf_pa_mrc_Deinit()
{
    PA_DEBUG("PA implementation.");
    std::lock_guard<std::mutex> paLock(gMrcPaMutex);

    // Check if initialized before attempting deinit
    if (!gMrcPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return PA_FAULT;
    }

    PA_INFO("Starting MRC platform adaptor deinitialization...");

    auto& pa = PlatformAdaptor::GetInstance();

    // Step 1: Clear the PA-level process status handler so no further callbacks
    // are dispatched. The ns layer has no remove-handler API, so nulling the
    // PA-level pointer is the only way to suppress further delivery.
    // Hold indicatorMutex so the clear is mutually exclusive with any in-flight
    // SB callback (ProcessStatusHandler / ScrubStatusHandler) that reads the
    // same pointers under the same mutex.
    PA_INFO("Clearing processStatus and scrubStatus handlers and contexts");
    {
        std::lock_guard<std::mutex> lock(pa.indicatorMutex);
        pa.indicators.processStatus.handlerFuncPtr = nullptr;
        pa.indicators.processStatus.contextPtr = nullptr;
        pa.indicators.scrubStatus.handlerFuncPtr = nullptr;
        pa.indicators.scrubStatus.contextPtr = nullptr;
    }

    // Step 2: Ask the ns-layer to release QMI clients and clear its handlers.
    int32_t nsResult = taf_prop_mrc_Deinit();
    if (nsResult == -ENOSYS)
    {
        PA_INFO("MRC proprietary platform adaptor is not implemented.");
    }
    else if (nsResult == -EINVAL)
    {
        PA_WARN("MRC proprietary platform adaptor was not initialized.");
    }
    else if (nsResult != 0)
    {
        PA_ERROR("taf_prop_mrc_Deinit failed: err(%d)", nsResult);
    }

    // Step 3: Reset the IFsManager shared pointer.
    // Take fsMutex so reset is mutually exclusive with NB calls copying pa.managers.fs.
    // NB operations keep a local shared_ptr after releasing fsMutex, so Deinit does not
    // block on long SDK operation waits and in-flight operations keep IFsManager alive.
    PA_INFO("Resetting managers.fs");
    {
        std::lock_guard<std::mutex> lock(pa.fsMutex);
        pa.managers.fs.reset();
    }

    gMrcPaInitialized.store(false, std::memory_order_release);
    PA_INFO("MRC platform adaptor initialization flag reset to false.");
    PA_INFO("MRC platform adaptor deinitialization complete.");
    return 0;
}

