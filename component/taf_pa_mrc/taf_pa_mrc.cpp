/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include <future>
#include <condition_variable>

#include <telux/common/CommonDefines.hpp>
#include <telux/platform/PlatformFactory.hpp>
#include <telux/platform/FsDefines.hpp>
#include <telux/platform/FsManager.hpp>

#include "taf_pa_mrc.hpp"

#include "taf_ns_mrc.h"

#define SERVICE_TIMEOUT 5

using namespace std;
using namespace telux::common;
using namespace telux::platform;

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
                    taf_ns_mrc_Result_t result
                );

                static taf_ns_mrc_Process_t Process
                (
                    taf_pa_mrc_Process_t process
                );

                static taf_pa_mrc_Process_t Process
                (
                    taf_ns_mrc_Process_t process
                );

                static taf_ns_mrc_Status_t Status
                (
                    taf_pa_mrc_Status_t status
                );

                static taf_pa_mrc_Status_t Status
                (
                    taf_ns_mrc_Status_t status
                );

                static taf_ns_mrc_Timer_t Timer
                (
                    taf_pa_mrc_Timer_t timer
                );
        };
};

taf_pa_mrc_Result_t Utility::Convert::Result
(
    taf_ns_mrc_Result_t result
)
{
    switch (result)
    {
        case TAF_NS_MRC_RESULT_SUCCESS:
            return TAF_PA_MRC_RESULT_SUCCESS;
        case TAF_NS_MRC_RESULT_FAILURE:
            return TAF_PA_MRC_RESULT_FAILURE;
        default:
            PA_DEBUG("Unknown result %d.", result);
    }

    return TAF_PA_MRC_RESULT_UNKNOWN;
}

taf_ns_mrc_Process_t Utility::Convert::Process
(
    taf_pa_mrc_Process_t process
)
{
    switch (process)
    {
        case TAF_PA_MRC_PROCESS_ABSYNC:
            return TAF_NS_MRC_PROCESS_ABSYNC;
        case TAF_PA_MRC_PROCESS_OTA:
            return TAF_NS_MRC_PROCESS_OTA;
        default:
            PA_DEBUG("Unknown process %d.", process);
    }

    return TAF_NS_MRC_PROCESS_UNKNOWN;
}

taf_pa_mrc_Process_t Utility::Convert::Process
(
    taf_ns_mrc_Process_t process
)
{
    switch (process)
    {
        case TAF_NS_MRC_PROCESS_ABSYNC:
            return TAF_PA_MRC_PROCESS_ABSYNC;
        case TAF_NS_MRC_PROCESS_OTA:
            return TAF_PA_MRC_PROCESS_OTA;
        default:
            PA_DEBUG("Unknown process %d.", process);
    }

    return TAF_PA_MRC_PROCESS_UNKNOWN;
}

taf_ns_mrc_Status_t Utility::Convert::Status
(
    taf_pa_mrc_Status_t status
)
{
    switch (status)
    {
        case TAF_PA_MRC_STATUS_INITIATED:
            return TAF_NS_MRC_STATUS_INITIATED;
        case TAF_PA_MRC_STATUS_RESUMED:
            return TAF_NS_MRC_STATUS_RESUMED;
        case TAF_PA_MRC_STATUS_SUCCEEDED:
            return TAF_NS_MRC_STATUS_SUCCEEDED;
        case TAF_PA_MRC_STATUS_FAILED:
            return TAF_NS_MRC_STATUS_FAILED;
        default:
            PA_DEBUG("Unknown status %d.", status);
    }

    return TAF_NS_MRC_STATUS_UNKNOWN;
}

taf_pa_mrc_Status_t Utility::Convert::Status
(
    taf_ns_mrc_Status_t status
)
{
    switch (status)
    {
        case TAF_NS_MRC_STATUS_INITIATED:
            return TAF_PA_MRC_STATUS_INITIATED;
        case TAF_NS_MRC_STATUS_RESUMED:
            return TAF_PA_MRC_STATUS_RESUMED;
        case TAF_NS_MRC_STATUS_SUCCEEDED:
            return TAF_PA_MRC_STATUS_SUCCEEDED;
        case TAF_NS_MRC_STATUS_FAILED:
            return TAF_PA_MRC_STATUS_FAILED;
        default:
            PA_DEBUG("Unknown status %d.", status);
    }

    return TAF_PA_MRC_STATUS_UNKNOWN;
}

taf_ns_mrc_Timer_t Utility::Convert::Timer
(
    taf_pa_mrc_Timer_t timer
)
{
    switch (timer)
    {
        case TAF_PA_MRC_TIMER_SCRUB:
            return TAF_NS_MRC_TIMER_SCRUB;
        case TAF_PA_MRC_TIMER_EFS_BACKUP:
            return TAF_NS_MRC_TIMER_EFS_BACKUP;
        case TAF_PA_MRC_TIMER_SUSPEND:
            return TAF_NS_MRC_TIMER_SUSPEND;
        case TAF_PA_MRC_TIMER_DEFER:
            return TAF_NS_MRC_TIMER_DEFER;
        default:
            PA_DEBUG("Unknown timer %d.", timer);
    }

    return TAF_NS_MRC_TIMER_UNKNOWN;
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
    taf_ns_mrc_ProcessStatusIndication_t indication,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFunc =
        (taf_pa_mrc_ProcessStatusHdlrFunc_t)pa.indicators.processStatus.handlerFuncPtr;
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

        handlerFunc(paIndication, pa.indicators.processStatus.contextPtr);
    }
}

pa_result_t taf_pa_mrc_Init()
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& platformFactory = PlatformFactory::getInstance();

    SERVICE_PROMISE_AND_CALLBACK(fs)
    pa.managers.fs = platformFactory.getFsManager(fsCallback);
    SERVICE_READY(fs)

    PA_INFO("MRC platform adaptor initialization is done.");
    int32_t result = taf_ns_mrc_Init();
    if (result == -ENOSYS)
        PA_INFO("MRC proprietary platform adaptor is not implemented.");
    else if (result == 0)
    {
        taf_ns_mrc_AddProcessStatusHandler(ProcessStatusHandler, nullptr);
        PA_INFO("MRC proprietary platform adaptor initialization is done.");
    }

    return 0;
}

pa_result_t taf_pa_mrc_RegisterIndication
(
    uint8_t registration
)
{
    return taf_ns_mrc_RegisterIndication(registration);
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
            taf_ns_mrc_Status_t nsStatus = Utility::Convert::Status(status);
            result = taf_ns_mrc_SetProcessStatus(TAF_NS_MRC_PROCESS_ABSYNC, nsStatus);
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

            switch (status)
            {
                case TAF_PA_MRC_STATUS_INITIATED:
                    paStatus = pa.managers.fs->prepareForOta(OtaOperation::START, callback);
                    break;
                case TAF_PA_MRC_STATUS_RESUMED:
                    paStatus = pa.managers.fs->prepareForOta(OtaOperation::RESUME, callback);
                    break;
                case TAF_PA_MRC_STATUS_SUCCEEDED:
                    paStatus = pa.managers.fs->otaCompleted(OperationStatus::SUCCESS, callback);
                    break;
                case TAF_PA_MRC_STATUS_FAILED:
                    paStatus = pa.managers.fs->otaCompleted(OperationStatus::FAILURE, callback);
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
    auto status = pa.managers.fs->startAbSync(callback);
    auto error = promisePtr->get_future().get();
    if (status != Status::SUCCESS || error != ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to set OTA status %d, ret = %d, error = %d.", status, (int)status,
            (int)error);
        return -EFAULT;
    }

    return 0;
}

taf_pa_mrc_ProcessStatusHandlerRef_t taf_pa_mrc_AddProcessStatusHandler
(
    taf_pa_mrc_ProcessStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.processStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.processStatus.contextPtr = contextPtr;

    return nullptr;
}

pa_result_t taf_pa_mrc_GetEfsPeStatus
(
    taf_pa_mrc_EfsPeStatus_t* statusPtr
)
{
    taf_ns_mrc_EfsPeStatus_t status;
    int32_t result = taf_ns_mrc_GetEfsPeStatus(&status);
    uint32_t i;
    for (i = 0; i < TAF_PA_MRC_EFS_PARTITION_BLOCKS && i < TAF_NS_MRC_EFS_PARTITION_BLOCKS
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
    taf_ns_mrc_EfsBlockStatus_t status;
    int32_t result = taf_ns_mrc_GetEfsBlockStatus(&status);
    statusPtr->maxEraseCount = status.maxEraseCount;
    statusPtr->totalBadBlocks = status.totalBadBlocks;

    return result;
}

pa_result_t taf_pa_mrc_SetTimerPeriod
(
    taf_pa_mrc_Timer_t timer,
    uint32_t period
)
{
    taf_ns_mrc_Timer_t nsTimer = Utility::Convert::Timer(timer);
    return taf_ns_mrc_SetTimerPeriod(nsTimer, period);
}