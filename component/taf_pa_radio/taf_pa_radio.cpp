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
#include <telux/tel/ImsServingSystemManager.hpp>
#include <telux/data/DataFactory.hpp>
#include <telux/data/ServingSystemManager.hpp>

#include "taf_pa_radio.hpp"
#include "taf_prop_radio.h"

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
    uint32_t instance;
    void* handlerFuncPtr;
    void* contextPtr;
} Handler_t;

typedef struct
{
    shared_ptr<tel::IPhoneManager> phone;
} Manager_t;

class Utility
{
    public:
        class Convert
        {
            public:
                static taf_prop_radio_RatBitMask_t Rat
                (
                    taf_pa_radio_RatBitMask_t bitmask
                );

                static taf_prop_radio_Rat_t Rat
                (
                    taf_pa_radio_Rat_t rat
                );

                static taf_pa_radio_Rat_t Rat
                (
                    taf_prop_radio_Rat_t rat
                );

                static taf_pa_radio_RatServiceStatus_t RatServiceStatus
                (
                    taf_prop_radio_RatServiceStatus_t status
                );

                static taf_pa_radio_SoBitMask_t SoMask
                (
                    taf_prop_radio_SoBitMask_t bitmask
                );

                static taf_pa_radio_LteCphyCaBandwidth_t LteCphyCaBandwidth
                (
                    taf_prop_radio_LteCphyCaBandwidth_t bandwidth
                );

                static taf_pa_radio_LteCphyScellState_t LteCphyScellState
                (
                    taf_prop_radio_LteCphyScellState_t state
                );

                static taf_pa_radio_DataRoamingStatus_t RoamingStatus
                (
                    taf_prop_radio_DataRoamingStatus_t status
                );

                static taf_pa_radio_OperatingMode_t OperatingMode
                (
                    tel::OperatingMode mode
                );
        };

        class WaitCallback
        {
            public:
                static void Request
                (
                    void
                );
        };
};

class BaseCallback
{
    public:
        sem_t semaphore;
        pa_result_t result;

        BaseCallback
        (
            void
        ) : result(0)
        {
            sem_init(&semaphore, 0, 0);
        }

        ~BaseCallback
        (
            void
        )
        {
            sem_destroy(&semaphore);
        }
};

class RequestCallback :
    public BaseCallback,
    public tel::IOperatingModeCallback
{
    public:
        tel::OperatingMode operatingMode;

        void operatingModeResponse
        (
            tel::OperatingMode mode,
            common::ErrorCode error
        ) override;
};

typedef struct
{
    Handler_t ratSvcStatus;
    Handler_t lteCphyCa;
    Handler_t dataAvailSysStatus;
} Indicator_t;

typedef struct
{
    shared_ptr<RequestCallback> request;
} Callback_t;

class PlatformAdaptor
{
    public:
        Indicator_t indicators;
        Callback_t callbacks;
        Manager_t managers;

        static PlatformAdaptor& GetInstance
        (
            void
        );
};

taf_prop_radio_RatBitMask_t Utility::Convert::Rat
(
    taf_pa_radio_RatBitMask_t bitmask
)
{
    taf_prop_radio_RatBitMask_t result = 0x0;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_GSM)
        result |= TAF_PROP_RADIO_BITMASK_RAT_GSM;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_CDMA)
        result |= TAF_PROP_RADIO_BITMASK_RAT_CDMA;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_UMTS)
        result |= TAF_PROP_RADIO_BITMASK_RAT_UMTS;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_TDSCDMA)
        result |= TAF_PROP_RADIO_BITMASK_RAT_TDSCDMA;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_LTE)
        result |= TAF_PROP_RADIO_BITMASK_RAT_LTE;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_NR5G)
        result |= TAF_PROP_RADIO_BITMASK_RAT_NR5G;

    return result;
}

taf_prop_radio_Rat_t Utility::Convert::Rat
(
    taf_pa_radio_Rat_t rat
)
{
    switch (rat)
    {
        case TAF_PA_RADIO_RAT_GSM:
            return TAF_PROP_RADIO_RAT_GSM;
        case TAF_PA_RADIO_RAT_CDMA:
            return TAF_PROP_RADIO_RAT_CDMA;
        case TAF_PA_RADIO_RAT_UMTS:
            return TAF_PROP_RADIO_RAT_UMTS;
        case TAF_PA_RADIO_RAT_TDSCDMA:
            return TAF_PROP_RADIO_RAT_TDSCDMA;
        case TAF_PA_RADIO_RAT_LTE:
            return TAF_PROP_RADIO_RAT_LTE;
        case TAF_PA_RADIO_RAT_NR5G:
            return TAF_PROP_RADIO_RAT_NR5G;
        default:
            PA_DEBUG("Unknown RAT.");
    }

    return TAF_PROP_RADIO_RAT_UNKNOWN;
}

taf_pa_radio_Rat_t Utility::Convert::Rat
(
    taf_prop_radio_Rat_t rat
)
{
    switch (rat)
    {
        case TAF_PROP_RADIO_RAT_GSM:
            return TAF_PA_RADIO_RAT_GSM;
        case TAF_PROP_RADIO_RAT_CDMA:
            return TAF_PA_RADIO_RAT_CDMA;
        case TAF_PROP_RADIO_RAT_UMTS:
            return TAF_PA_RADIO_RAT_UMTS;
        case TAF_PROP_RADIO_RAT_TDSCDMA:
            return TAF_PA_RADIO_RAT_TDSCDMA;
        case TAF_PROP_RADIO_RAT_LTE:
            return TAF_PA_RADIO_RAT_LTE;
        case TAF_PROP_RADIO_RAT_NR5G:
            return TAF_PA_RADIO_RAT_NR5G;
        default:
            PA_DEBUG("Unknown RAT.");
    }

    return TAF_PA_RADIO_RAT_UNKNOWN;
}

taf_pa_radio_RatServiceStatus_t Utility::Convert::RatServiceStatus
(
    taf_prop_radio_RatServiceStatus_t status
)
{
    switch (status)
    {
        case TAF_PROP_RADIO_RAT_SERVICE_STATUS_NO_SERVICE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_NO_SERVICE;
        case TAF_PROP_RADIO_RAT_SERVICE_STATUS_LIMITED:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_LIMITED;
        case TAF_PROP_RADIO_RAT_SERVICE_STATUS_SERVICE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_SERVICE;
        case TAF_PROP_RADIO_RAT_SERVICE_STATUS_LIMITED_REGIONAL:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_LIMITED_REGIONAL;
        case TAF_PROP_RADIO_RAT_SERVICE_STATUS_POWER_SAVE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_POWER_SAVE;
        default:
            PA_DEBUG("Unknown RAT service status.");
    }

    return TAF_PA_RADIO_RAT_SERVICE_STATUS_UNKNOWN;
}

taf_pa_radio_SoBitMask_t Utility::Convert::SoMask
(
    taf_prop_radio_SoBitMask_t bitmask
)
{
    taf_pa_radio_SoBitMask_t result = 0x0;

    if (bitmask & TAF_PROP_RADIO_BITMASK_SO_5G_NSA)
        result |= TAF_PA_RADIO_BITMASK_SO_5G_NSA;

    return result;
}

taf_pa_radio_LteCphyCaBandwidth_t Utility::Convert::LteCphyCaBandwidth
(
    taf_prop_radio_LteCphyCaBandwidth_t bandwidth
)
{
    switch (bandwidth)
    {
        case TAF_PROP_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_6:
            return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_6;
        case TAF_PROP_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_15:
            return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_15;
        case TAF_PROP_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_25:
            return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_25;
        case TAF_PROP_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_50:
            return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_50;
        case TAF_PROP_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_75:
            return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_75;
        case TAF_PROP_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_100:
            return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_NRB_100;
        default:
            PA_DEBUG("Unknown LTE bandwidth.");
    }

    return TAF_PA_RADIO_LTE_CPHY_CA_BANDWIDTH_UNKNOWN;
}

taf_pa_radio_LteCphyScellState_t Utility::Convert::LteCphyScellState
(
    taf_prop_radio_LteCphyScellState_t state
)
{
    switch (state)
    {
        case TAF_PROP_RADIO_LTE_CPHY_SCELL_STATE_DECONFIGURED:
            return TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_DECONFIGURED;
        case TAF_PROP_RADIO_LTE_CPHY_SCELL_STATE_CONFIGURED_DEACTIVATED:
            return TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_CONFIGURED_DEACTIVATED;
        case TAF_PROP_RADIO_LTE_CPHY_SCELL_STATE_CONFIGURED_ACTIVATED:
            return TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_CONFIGURED_ACTIVATED;
        default:
            PA_DEBUG("Unknown LTE Control PHY Scell state.");
    }

    return TAF_PA_RADIO_LTE_CPHY_SCELL_STATE_UNKNOWN;
}

taf_pa_radio_DataRoamingStatus_t Utility::Convert::RoamingStatus
(
    taf_prop_radio_DataRoamingStatus_t status
)
{
    switch (status)
    {
        case TAF_PROP_RADIO_DATA_ROAMING_STATUS_ON:
            return TAF_PA_RADIO_DATA_ROAMING_STATUS_ON;
        case TAF_PROP_RADIO_DATA_ROAMING_STATUS_OFF:
            return TAF_PA_RADIO_DATA_ROAMING_STATUS_OFF;
        default:
            PA_DEBUG("Unknown roaming status.");
    }

    return TAF_PA_RADIO_DATA_ROAMING_STATUS_UNKNOWN;
}

taf_pa_radio_OperatingMode_t Utility::Convert::OperatingMode
(
    tel::OperatingMode mode
)
{
    switch (mode)
    {
        case tel::OperatingMode::ONLINE:
            return TAF_PA_RADIO_OPERATING_MODE_ONLINE;
        case tel::OperatingMode::AIRPLANE:
            return TAF_PA_RADIO_OPERATING_MODE_LOW_POWER;
        case tel::OperatingMode::FACTORY_TEST:
            return TAF_PA_RADIO_OPERATING_MODE_FACTORY_TEST_MODE;
        case tel::OperatingMode::OFFLINE:
            return TAF_PA_RADIO_OPERATING_MODE_OFFLINE;
        case tel::OperatingMode::RESETTING:
            return TAF_PA_RADIO_OPERATING_MODE_RESETTING;
        case tel::OperatingMode::SHUTTING_DOWN:
            return TAF_PA_RADIO_OPERATING_MODE_SHUTTING_DOWN;
        case tel::OperatingMode::PERSISTENT_LOW_POWER:
            return TAF_PA_RADIO_OPERATING_MODE_PERSISTENT_LOW_POWER;
        default:
            PA_DEBUG("Unknown operating mode.");
    }

    return TAF_PA_RADIO_OPERATING_MODE_UNKNOWN;
}

void Utility::WaitCallback::Request
(
    void
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += REQUEST_TIMEOUT;

    int result = sem_timedwait(&pa.callbacks.request->semaphore, &ts);
    if (result != 0)
    {
        if (errno == ETIMEDOUT)
        {
            PA_ERROR("Timeout to request.");
            pa.callbacks.request->result = -ETIMEDOUT;
        }
        else
        {
            PA_ERROR("Wait on semaphore with error code: %d.", result);
            pa.callbacks.request->result = -EFAULT;
        }
    }
}

void RequestCallback::operatingModeResponse
(
    tel::OperatingMode mode,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error));
        result = -EFAULT;
    }
    else
    {
        operatingMode = mode;
        result = 0;
    }

    sem_post(&semaphore);
}

PlatformAdaptor& PlatformAdaptor::GetInstance
(
    void
)
{
    static PlatformAdaptor instance;
    return instance;
}

static void RatSvcStatusHandler
(
    uint32_t instance,
    taf_prop_radio_RatSvcStatusIndication_t indication,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_radio_RatSvcStatusHdlrFunc_t handlerFunc =
        (taf_pa_radio_RatSvcStatusHdlrFunc_t)pa.indicators.ratSvcStatus.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_RatSvcStatusIndication_t paIndication;

        paIndication.gsmSvcStatusValid = indication.gsmSvcStatusValid;
        if (paIndication.gsmSvcStatusValid)
            paIndication.gsmSvcStatus = Utility::Convert::RatServiceStatus(
                indication.gsmSvcStatus);

        paIndication.cdmaSvcStatusValid = indication.cdmaSvcStatusValid;
        if (paIndication.cdmaSvcStatusValid)
            paIndication.cdmaSvcStatus = Utility::Convert::RatServiceStatus(
                indication.cdmaSvcStatus);

        paIndication.umtsSvcStatusValid = indication.umtsSvcStatusValid;
        if (paIndication.umtsSvcStatusValid)
            paIndication.umtsSvcStatus = Utility::Convert::RatServiceStatus(
                indication.umtsSvcStatus);

        paIndication.tdscdmaSvcStatusValid = indication.tdscdmaSvcStatusValid;
        if (paIndication.tdscdmaSvcStatusValid)
            paIndication.tdscdmaSvcStatus = Utility::Convert::RatServiceStatus(
                indication.tdscdmaSvcStatus);

        paIndication.lteSvcStatusValid = indication.lteSvcStatusValid;
        if (paIndication.lteSvcStatusValid)
            paIndication.lteSvcStatus = Utility::Convert::RatServiceStatus(
                indication.lteSvcStatus);

        paIndication.nr5gSvcStatusValid = indication.nr5gSvcStatusValid;
        if (paIndication.nr5gSvcStatusValid)
            paIndication.nr5gSvcStatus = Utility::Convert::RatServiceStatus(
                indication.nr5gSvcStatus);

        handlerFunc(instance, paIndication, pa.indicators.ratSvcStatus.contextPtr);
    }
}

static void LteCphyCaHandler
(
    uint32_t instance,
    taf_prop_radio_LteCphyCaIndication_t indication,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_radio_LteCphyCaHdlrFunc_t handlerFunc =
        (taf_pa_radio_LteCphyCaHdlrFunc_t)pa.indicators.lteCphyCa.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_LteCphyCaIndication_t paIndication;

        paIndication.pcellInfoValid = indication.pcellInfoValid;
        if (paIndication.pcellInfoValid)
        {
            paIndication.pcellInfo.pci = indication.pcellInfo.pci;
            paIndication.pcellInfo.freq = indication.pcellInfo.freq;
            paIndication.pcellInfo.cphyCaDlBandwidth = Utility::Convert::LteCphyCaBandwidth(
                indication.pcellInfo.cphyCaDlBandwidth);
            paIndication.pcellInfo.band = indication.pcellInfo.band;
        }

        paIndication.scellInfoValid = indication.scellInfoValid;
        if (paIndication.scellInfoValid)
        {
            uint32_t i;
            for (i = 0; i < indication.scellInfoCount &&
                i < TAF_PA_RADIO_LTE_CPHY_SCELL_INFO_MAX_COUNT; i++)
            {
                paIndication.scellInfo[i].pci = indication.scellInfo[i].pci;
                paIndication.scellInfo[i].freq = indication.scellInfo[i].freq;
                paIndication.scellInfo[i].cphyCaDlBandwidth = Utility::Convert::LteCphyCaBandwidth(
                    indication.scellInfo[i].cphyCaDlBandwidth);
                paIndication.scellInfo[i].band = indication.scellInfo[i].band;
                paIndication.scellInfo[i].scellState = Utility::Convert::LteCphyScellState(
                    indication.scellInfo[i].scellState);
                paIndication.scellInfo[i].scellIndex = indication.scellInfo[i].scellIndex;
                paIndication.scellInfo[i].ulConfigured = indication.scellInfo[i].ulConfigured;
            }

            paIndication.scellInfoCount = i;
        }

        handlerFunc(instance, paIndication, pa.indicators.lteCphyCa.contextPtr);
    }
}

static void DataAvailSysStatusHandler
(
    uint32_t instance,
    taf_prop_radio_DataAvailSysStatusIndication_t indication,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_radio_DataAvailSysStatusHdlrFunc_t handlerFunc =
        (taf_pa_radio_DataAvailSysStatusHdlrFunc_t)pa.indicators.dataAvailSysStatus.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_DataAvailSysStatusIndication_t paIndication;

        paIndication.availSysValid = indication.availSysValid;
        if (paIndication.availSysValid)
        {
            uint32_t i;
            for (i = 0; i < indication.availSys.availSysCount &&
                i < TAF_PA_RADIO_DATA_AVAIL_SYS_MAX_COUNT; i++)
            {
                paIndication.availSys.availSysStatusInfo[i].rat =
                    Utility::Convert::Rat(indication.availSys.availSysStatusInfo[i].rat);
                paIndication.availSys.availSysStatusInfo[i].soMask =
                    Utility::Convert::SoMask(indication.availSys.availSysStatusInfo[i].soMask);
            }

            paIndication.availSys.availSysCount = i;
        }

        handlerFunc(instance, paIndication, pa.indicators.dataAvailSysStatus.contextPtr);
    }
}

pa_result_t taf_pa_radio_Init()
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

    pa.callbacks.request = make_shared<RequestCallback>();

    PA_INFO("Radio platform adaptor initialization is done.");

    int32_t result = taf_prop_radio_Init();
    if (result == -ENOSYS)
        PA_INFO("Radio proprietary platform adaptor is not implemented.");
    else if (result == 0)
    {
        for (uint32_t i = 0; i < slots; i++)
        {
            result = taf_prop_radio_InitInstance(i);
            if (result != 0)
                PA_ERROR("Failed to initializate proprietary instance %d, result = %d.", i,
                    result);
            else
                PA_INFO("Radio proprietary instance %d initialization is done.", i);
        }

        taf_prop_radio_AddRatSvcStatusHandler(0, RatSvcStatusHandler, nullptr);
        taf_prop_radio_AddLteCphyCaHandler(0, LteCphyCaHandler, nullptr);
        taf_prop_radio_AddDataAvailSysStatusHandler(0, DataAvailSysStatusHandler, nullptr);

        PA_INFO("Radio proprietary platform adaptor initialization is done.");
    }

    return 0;
}

pa_result_t taf_pa_radio_GetOperatingMode
(
    uint32_t instance,
    taf_pa_radio_OperatingMode_t* modePtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& request = pa.callbacks.request;

    if (modePtr == nullptr)
    {
        PA_ERROR("modePtr is nullptr.");
        return -EINVAL;
    }

    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    auto result = pa.managers.phone->requestOperatingMode(request);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to request operating mode with phone manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != PA_OK)
        return request->result;

    *modePtr = Utility::Convert::OperatingMode(pa.callbacks.request->operatingMode);
    
    return 0;
}

pa_result_t taf_pa_radio_RegisterIndication
(
    uint32_t instance,
    uint8_t registration
)
{
    if (!common::DeviceConfig::isMultiSimSupported() && instance > 0)
        return -ENOTSUP;

    int32_t result = taf_prop_radio_RegisterIndication(instance, registration);
    return result;
}

pa_result_t taf_pa_radio_PerformPciNetworkScan
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t bitmask,
    taf_pa_radio_PciScanInformation_t* informationPtr
)
{
    if (informationPtr == nullptr)
    {
        PA_ERROR("informationPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_PciScanInformation_t information;
    taf_prop_radio_RatBitMask_t rat = Utility::Convert::Rat(bitmask);
    int32_t result = taf_prop_radio_PerformPciNetworkScan(instance, rat, &information);

    uint32_t i, j;
    for (i = 0; i < information.pciCellCount && i < TAF_PA_RADIO_PCI_SCAN_CELL_MAX_COUNT; i++)
    {
        informationPtr->pciCellInfo[i].cellId = information.pciCellInfo[i].cellId;
        informationPtr->pciCellInfo[i].globalCellId = information.pciCellInfo[i].globalCellId;

        for (j = 0; j < information.pciCellInfo[i].plmnCount &&
            j < TAF_PA_RADIO_PCI_SCAN_PLMN_ID_MAX_COUNT; j++)
        {
            informationPtr->pciCellInfo[i].plmnId[j].mcc =
                information.pciCellInfo[i].plmnId[j].mcc;
            informationPtr->pciCellInfo[i].plmnId[j].mnc =
                information.pciCellInfo[i].plmnId[j].mnc;
            informationPtr->pciCellInfo[i].plmnId[j].mncIncludesPcsDigit =
                information.pciCellInfo[i].plmnId[j].mncIncludesPcsDigit;
        }

        informationPtr->pciCellInfo[i].plmnCount = j;
    }

    informationPtr->pciCellCount = i;

    return result;
}

pa_result_t taf_pa_radio_GetServingRat
(
    uint32_t instance,
    taf_pa_radio_Rat_t* ratPtr
)
{
    if (ratPtr == nullptr)
    {
        PA_ERROR("ratPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_Rat_t rat = TAF_PROP_RADIO_RAT_UNKNOWN;
    int32_t result = taf_prop_radio_GetServingRat(instance, &rat);

    *ratPtr = Utility::Convert::Rat(rat);

    return result;
}

pa_result_t taf_pa_radio_GetRatSvcStatus
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_RatServiceStatus_t* statusPtr
)
{
    if (statusPtr == nullptr)
    {
        PA_ERROR("statusPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_Rat_t propRat = Utility::Convert::Rat(rat);
    taf_prop_radio_RatServiceStatus_t status = TAF_PROP_RADIO_RAT_SERVICE_STATUS_UNKNOWN;
    int32_t result = taf_prop_radio_GetRatSvcStatus(instance, propRat, &status);

    *statusPtr = Utility::Convert::RatServiceStatus(status);

    return result;
}

pa_result_t taf_pa_radio_GetServingCellRac
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    uint8_t* racPtr
)
{
    if (racPtr == nullptr)
    {
        PA_ERROR("racPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_Rat_t propRat = Utility::Convert::Rat(rat);
    int32_t result = taf_prop_radio_GetServingCellRac(instance, propRat, racPtr);

    return result;
}

pa_result_t taf_pa_radio_GetDataAvailSysStatus
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatus_t* statusPtr
)
{
    if (statusPtr == nullptr)
    {
        PA_ERROR("statusPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_DataAvailSysStatus_t status;
    int32_t result = taf_prop_radio_GetDataAvailSysStatus(instance, &status);

    uint32_t i;
    for (i = 0; i < status.availSysCount && i < TAF_PA_RADIO_DATA_AVAIL_SYS_MAX_COUNT; i++)
    {
        statusPtr->availSysStatusInfo[i].rat =
            Utility::Convert::Rat(status.availSysStatusInfo[i].rat);
        statusPtr->availSysStatusInfo[i].soMask =
            Utility::Convert::SoMask(status.availSysStatusInfo[i].soMask);
    }

    statusPtr->availSysCount = i;

    return result;
}

pa_result_t taf_pa_radio_GetLteCphyCaInfo
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_LteCphyCaInfo_t info;
    int32_t result = taf_prop_radio_GetLteCphyCaInfo(instance, &info);

    infoPtr->pcellInfo.pci = info.pcellInfo.pci;
    infoPtr->pcellInfo.freq = info.pcellInfo.freq;
    infoPtr->pcellInfo.cphyCaDlBandwidth = Utility::Convert::LteCphyCaBandwidth(
        info.pcellInfo.cphyCaDlBandwidth);
    infoPtr->pcellInfo.band = info.pcellInfo.band;

    uint32_t i;
    for (i = 0; i < info.scellInfoCount && i < TAF_PA_RADIO_LTE_CPHY_SCELL_INFO_MAX_COUNT; i++)
    {
        infoPtr->scellInfo[i].pci = info.scellInfo[i].pci;
        infoPtr->scellInfo[i].freq = info.scellInfo[i].freq;
        infoPtr->scellInfo[i].cphyCaDlBandwidth = Utility::Convert::LteCphyCaBandwidth(
            info.scellInfo[i].cphyCaDlBandwidth);
        infoPtr->scellInfo[i].band = info.scellInfo[i].band;
        infoPtr->scellInfo[i].scellState = Utility::Convert::LteCphyScellState(
            info.scellInfo[i].scellState);
        infoPtr->scellInfo[i].scellIndex = info.scellInfo[i].scellIndex;
        infoPtr->scellInfo[i].ulConfigured = info.scellInfo[i].ulConfigured;
    }

    infoPtr->scellInfoCount = i;

    return result;
}

taf_pa_radio_RatSvcStatusHandlerRef_t taf_pa_radio_AddRatSvcStatusHandler
(
    uint32_t instance,
    taf_pa_radio_RatSvcStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.ratSvcStatus.instance = instance;
    pa.indicators.ratSvcStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.ratSvcStatus.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_LteCphyCaHandlerRef_t taf_pa_radio_AddLteCphyCaHandler
(
    uint32_t instance,
    taf_pa_radio_LteCphyCaHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.lteCphyCa.instance = instance;
    pa.indicators.lteCphyCa.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.lteCphyCa.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_DataAvailSysStatusHandlerRef_t taf_pa_radio_AddDataAvailSysStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataAvailSysStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.dataAvailSysStatus.instance = instance;
    pa.indicators.dataAvailSysStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.dataAvailSysStatus.contextPtr = contextPtr;

    return nullptr;
}

pa_result_t taf_pa_radio_GetDataCurrRoamingStatus
(
    uint32_t instance,
    taf_pa_radio_DataRoamingStatus_t* statusPtr
)
{
    if (statusPtr == nullptr)
    {
        PA_ERROR("statusPtr is nullptr.");
        return -EINVAL;
    }

    taf_prop_radio_DataRoamingStatus_t status = TAF_PROP_RADIO_DATA_ROAMING_STATUS_UNKNOWN;
    int32_t result = taf_prop_radio_GetDataCurrRoamingStatus(instance, &status);

    *statusPtr = Utility::Convert::RoamingStatus(status);

    return result;
}