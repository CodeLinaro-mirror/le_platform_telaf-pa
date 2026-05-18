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

#include "tafRadioPa.hpp"
#include "taf_prop_radio.h"

using namespace std;
using namespace telux;

#define MAX_INSTANCE 2
#define SERVICE_TIMEOUT 5
#define REQUEST_TIMEOUT 5
#define DISABLE_INDICATION 0
#define ENABLE_INDICATION 1
#define LTE_BAND_NUM_PER_GROUP 64

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

#define SERVICE_READY(name,manager)                                                      \
    future<common::ServiceStatus> name##Future = name##Promise->get_future();    \
    future_status name##Status = name##Future.wait_for(                          \
        chrono::seconds(SERVICE_TIMEOUT));                                       \
        common::ServiceStatus name##ServiceStatus;                               \
        if (future_status::timeout == name##Status)                              \
        {                                                                        \
            PA_CRIT("Timeout for %s.", #name);                                   \
            manager = nullptr;			                                         \
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
    uint32_t instance;
    void* handlerFuncPtr;
    void* contextPtr;
} Handler_t;

typedef struct
{
    shared_ptr<tel::IPhoneManager> phone;
    shared_ptr<tel::IImsSettingsManager> imsSetting;
    shared_ptr<tel::INetworkSelectionManager> networkSelections[MAX_INSTANCE];
    shared_ptr<tel::IServingSystemManager> telephonyServingSystems[MAX_INSTANCE];
    shared_ptr<tel::IImsServingSystemManager> imsServingSystems[MAX_INSTANCE];
    shared_ptr<data::IServingSystemManager> dataServingSystems[MAX_INSTANCE];
} Manager_t;

class Utility
{
    public:
        class Convert
        {
            public:
                static SlotId SlotToSlotId
                (
                    int slot
                );

                static int InstanceToSlot
                (
                    uint32_t instance
                );

                static int InstanceToPhone
                (
                    uint32_t instance
                );

                static uint32_t PhoneToInstance
                (
                    int phone
                );

                static pa_result_t StringToU16
                (
                    const string& str,
                    uint16_t* valuePtr
                );

                static void String
                (
                    const string& str,
                    uint8_t* validPtr,
                    char* strPtr,
                    size_t size
                );

                static taf_prop_radio_RatBitMask_t Rat
                (
                    taf_pa_radio_RatBitMask_t bitmask
                );

                static taf_pa_radio_RatBitMask_t TelRatPreferenceToRat
                (
                    tel::RatPreference bitmask
                );

                static tel::RatMask RatToTelRat
                (
                    taf_pa_radio_RatBitMask_t bitmask
                );

                static tel::RatPreference RatToTelRatPreference
                (
                    taf_pa_radio_RatBitMask_t bitmask
                );

                static taf_pa_radio_RatBitMask_t Rat
                (
                    tel::RatMask bitmask
                );

                static taf_prop_radio_Rat_t Rat
                (
                    taf_pa_radio_Rat_t rat
                );

                static taf_pa_radio_Rat_t Rat
                (
                    taf_prop_radio_Rat_t rat
                );

                static taf_pa_radio_Rat_t Rat
                (
                    tel::RadioTechnology rat
                );

                static pa_result_t Rat
                (
                    int slot,
                    vector<tel::DeviceRatCapability> capabilities,
                    taf_pa_radio_RatBitMask_t* bitmaskPtr
                );

                static tel::RadioTechnology RatToTelRat
                (
                    taf_pa_radio_Rat_t rat
                );

                static taf_pa_radio_ServiceDomain_t ServiceDomain
                (
                    tel::ServiceDomain domain
                );

                static taf_pa_radio_ServiceDomainBitMask_t ServiceDomainPreference
                (
                    tel::ServiceDomainPreference domain
                );

                static taf_pa_radio_RatServiceStatus_t RatServiceStatus
                (
                    tel::ServiceRegistrationState state
                );

                static tel::ServiceDomainPreference ServiceDomainPreference
                (
                    taf_pa_radio_ServiceDomainBitMask_t bitmask
                );

                static void VoiceServiceInfo
                (
                    tel::VoiceServiceState state,
                    taf_pa_radio_VoiceServiceInfo_t* infoPtr
                );

                static taf_pa_radio_SignalStrengthLevel_t SignalStrengthLevel
                (
                    tel::SignalStrengthLevel level
                );

                static taf_pa_radio_SignalStrengthLevel_t SignalStrengthLevel
                (
                    taf_pa_radio_Rat_t rat,
                    shared_ptr<tel::SignalStrength> strengthPtr
                );

                static void SignalStrengthInfo
                (
                    shared_ptr<tel::SignalStrength> strengthPtr,
                    taf_pa_radio_SignalStrengthInfo_t* infoPtr
                );

                static tel::SignalStrengthMeasurementType SignalMetric
                (
                    taf_pa_radio_SignalMetric_t metric
                );

                static void SignalStrengthIndConfig
                (
                    taf_pa_radio_SignalStrengthIndConfig_t* configPtr,
                    vector<tel::SignalStrengthConfigEx>& config
                );

                static taf_pa_radio_BandBitMask_t Band
                (
                    shared_ptr<tel::IRFBandList> listPtr
                );

                static void Band
                (
                    shared_ptr<tel::IRFBandList> listPtr,
                    taf_pa_radio_LteBand_t* bandPtr
                );

                static taf_pa_radio_DataServiceState_t ServiceState
                (
                    data::DataServiceState state
                );

                static void CellInfoList
                (
                    vector<shared_ptr<tel::CellInfo>> infoPtrList,
                    taf_pa_radio_CellLocationListInfo_t* listInfoPtr
                );

                static taf_pa_radio_ImsRegistrationStatus_t ImsRegistrationStatus
                (
                    tel::RegistrationStatus status
                );

                static taf_pa_radio_LteCsCapability_t LteCsCapability
                (
                    tel::LteCsCapability capability
                );

                static taf_pa_radio_ImsServiceStatus_t ImsServiceStatus
                (
                    tel::CellularServiceStatus status
                );

                static pa_result_t ImsServiceStatus
                (
                    taf_pa_radio_ImsService_t service,
                    tel::ImsServiceInfo info,
                    taf_pa_radio_ImsServiceStatus_t* statusPtr
                );

                static taf_pa_radio_ImsPdpFailureErrorCode_t ImsPdpFailureErrorCode
                (
                    tel::PdpFailureCode code
                );

                static void ImsServiceConfig
                (
                    taf_pa_radio_ImsServiceSettingBitMask_t bitmask,
                    bool enable,
                    tel::ImsServiceConfig* configPtr
                );

                static void ImsService
                (
                    tel::ImsServiceConfig config,
                    taf_pa_radio_ImsServiceSettingBitMask_t* bitmaskPtr
                );

                static taf_pa_radio_EndcAvailability_t EndcAvailability
                (
                    tel::EndcAvailability availability
                );

                static taf_pa_radio_DcnrRestriction_t DcnrRestriction
                (
                    tel::DcnrRestriction restriction
                );

                static taf_pa_radio_BandBitMask_t ActiveBand
                (
                    tel::RFBand band
                );

                static uint32_t LteActiveBand
                (
                    tel::RFBand band
                );

                static uint32_t Nr5gActiveBand
                (
                    tel::RFBand band
                );

                static taf_pa_radio_Bandwidth_t BandWidth
                (
                    tel::RFBandWidth bandwidth
                );

                static pa_result_t RFBandInfo
                (
                    tel::RFBandInfo info,
                    taf_pa_radio_ServingCellBandInfo_t* infoPtr
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

                static pa_result_t OperatingMode
                (
                    taf_pa_radio_OperatingMode_t mode,
                    tel::OperatingMode* modePtr
                );

                static taf_pa_radio_NrIcon_t NrIcon
                (
                    data::NrIconType type
                );

        };

        class WaitCallback
        {
            public:
                static void Request
                (
                    void
                );

                static void Scan
                (
                    uint32_t instance,
                    uint32_t timeout
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
    public tel::IOperatingModeCallback,
    public tel::IVoiceServiceStateCallback,
    public tel::ISignalStrengthCallback,
    public tel::ICellularCapabilityCallback
{
    public:
        tel::OperatingMode operatingMode;
        tel::NetworkModeInfo networkModeInfo;
        vector<tel::PreferredNetworkInfo> preferredNetworksInfo;
        tel::RatPreference ratPreference;
        tel::ServiceDomainPreference serviceDomainPreference;
        tel::VoiceServiceState voiceServiceState;
        data::DataServiceState dataServiceState;
        shared_ptr<tel::SignalStrength> signalStrengthPtr;
        vector<shared_ptr<tel::CellInfo>> cellInfoPtrList;
        tel::PlmnInfo operatorInfo;
        shared_ptr<tel::IRFBandList> rfBandCapabilityPtr;
        shared_ptr<tel::IRFBandList> rfBandPreferencePtr;
        tel::ImsRegistrationInfo imsRegistrationInfo;
        tel::ImsServiceInfo imsServiceInfo;
        tel::ImsPdpStatusInfo imsPdpStatusInfo;
        tel::ImsServiceConfig imsServiceConfig;
        common::ErrorCode imsServiceConfigError;
        bool isVoNREnabled;
        string imsSipUserAgent;
        tel::CellularCapabilityInfo cellularCapabilityInfo;
        tel::RFBandInfo rfBandInfo;
        data::NrIconType nrIconType;

        void CommonResponse
        (
            common::ErrorCode error
        );

        void DataServiceStatusResponse
        (
            data::ServiceStatus status,
            common::ErrorCode error
        );

        void NetworkModeInfoResponse
        (
            tel::NetworkModeInfo info,
            common::ErrorCode error
        );

        void PreferredNetworksResponse
        (
            vector<tel::PreferredNetworkInfo> nonStaticInfo,
            vector<tel::PreferredNetworkInfo> staticInfo,
            common::ErrorCode error
        );

        void RatPreferenceResponse
        (
            tel::RatPreference preference,
            common::ErrorCode error
        );

        void ServiceDomainPreferenceResponse
        (
            tel::ServiceDomainPreference preference,
            common::ErrorCode error
        );

        void operatingModeResponse
        (
            tel::OperatingMode mode,
            common::ErrorCode error
        ) override;

        void voiceServiceStateResponse
        (
            const shared_ptr<tel::VoiceServiceInfo>& infoPtr,
            common::ErrorCode error
        ) override;

        void signalStrengthResponse
        (
            shared_ptr<tel::SignalStrength> strengthPtr,
            common::ErrorCode error
        ) override;

        void CellInfoListResponse
        (
            vector<shared_ptr<tel::CellInfo>> infoPtrList,
            common::ErrorCode error
        );

        void OperatorInfoResponse
        (
            tel::PlmnInfo info,
            common::ErrorCode error
        );

        void RfBandCapabilityResponse
        (
            shared_ptr<tel::IRFBandList> listPtr,
            common::ErrorCode error
        );

        void RfBandPreferenceResponse
        (
            shared_ptr<tel::IRFBandList> listPtr,
            common::ErrorCode error
        );

        void ImsRegistrationInfoResponse
        (
            tel::ImsRegistrationInfo info,
            common::ErrorCode error
        );

        void ImsServiceInfoResponse
        (
            tel::ImsServiceInfo info,
            common::ErrorCode error
        );

        void ImsPdpStatusResponse
        (
            tel::ImsPdpStatusInfo info,
            common::ErrorCode error
        );

        void ImsVonrStatusResponse
        (
            SlotId id,
            bool enable,
            common::ErrorCode error
        );

        void ImsServiceConfigResponse
        (
            SlotId id,
            tel::ImsServiceConfig config,
            common::ErrorCode error
        );

        void ImsSigUserAgentResponse
        (
            SlotId id,
            string str,
            common::ErrorCode error
        );

        void cellularCapabilityResponse
        (
            tel::CellularCapabilityInfo info,
            common::ErrorCode error
        ) override;

        void RFBandInfoResponse
        (
            tel::RFBandInfo info,
            common::ErrorCode error
        );

        void NrIconTypeResponse
        (
            data::NrIconType type,
            common::ErrorCode error
        );
};

class Listener
{
    public:
        class BaseListener
        {
            public:
                uint32_t instance;

                BaseListener
                (
                    uint32_t instance
                ) : instance(instance)
                {
                }
        };

        class PhoneListener :
            public tel::IPhoneListener
        {
            public:
                void onVoiceServiceStateChanged
                (
                    int phone,
                    const shared_ptr<tel::VoiceServiceInfo>& infoPtr
                ) override;

                void onSignalStrengthChanged
                (
                    int phone,
                    shared_ptr<tel::SignalStrength> strengthPtr
                ) override;

                void onOperatingModeChanged
                (
                    tel::OperatingMode mode
                ) override;

                void onCellInfoListChanged
                (
                    int phone,
                    vector<shared_ptr<tel::CellInfo>> cellInfoList
                ) override;
        };

        class TelephonyServingSystemListener :
            public BaseListener,
            public tel::IServingSystemListener
        {
            public:
                TelephonyServingSystemListener
                (
                    uint32_t instance
                ) : BaseListener(instance)
                {
                }

                void onNetworkRejection
                (
                    tel::NetworkRejectInfo info
                ) override;

                void onSystemInfoChanged
                (
                   tel::ServingSystemInfo info
                ) override;

                void onLteCsCapabilityChanged
                (
                    tel::LteCsCapability capability
                ) override;
        };

        class ImsServingSystemListener :
            public BaseListener,
            public tel::IImsServingSystemListener
        {
            public:
                ImsServingSystemListener
                (
                    uint32_t instance
                ) : BaseListener(instance)
                {
                }

                void onImsRegStatusChange
                (
                    tel::ImsRegistrationInfo info
                ) override;

                void onImsServiceInfoChange
                (
                    tel::ImsServiceInfo info
                ) override;

                void onImsPdpStatusInfoChange
                (
                    tel::ImsPdpStatusInfo info
                ) override;
        };

        class NetworkSelectionListener :
            public BaseListener,
            public tel::INetworkSelectionListener
        {
            public:
                sem_t semaphore;
                pa_result_t result;
                vector<tel::OperatorInfo> operatorInfoList;

                NetworkSelectionListener
                (
                    uint32_t instance
                ) : BaseListener(instance)
                {
                    result = 0;
                    sem_init(&semaphore, 0, 0);
                }

                ~NetworkSelectionListener
                (
                   void
                )
                {
                    sem_destroy(&semaphore);
                }

                void onNetworkScanResults
                (
                    tel::NetworkScanStatus status,
                    vector<tel::OperatorInfo> infoList
                ) override;
        };

        class DataServingSystemListener :
            public BaseListener,
            public data::IServingSystemListener
        {
            public:
                DataServingSystemListener
                (
                    uint32_t instance
                ) : BaseListener(instance)
                {
                }

                void onServiceStateChanged
                (
                    data::ServiceStatus status
                ) override;

                void onRoamingStatusChanged
                (
                    data::RoamingStatus status
                ) override;

                void onNrIconTypeChanged
                (
                    telux::data::NrIconType type
                ) override;
        };
};

typedef struct
{
    Handler_t networkReject;
    Handler_t ratChange;
    Handler_t voiceServiceInfo;
    Handler_t dataServiceStatus;
    Handler_t dataRoamingStatus;
    Handler_t signalStrengthInfoChange;
    Handler_t ratSvcStatus;
    Handler_t lteCphyCa;
    Handler_t dataAvailSysStatus;
    Handler_t imsRegStatusChange;
    Handler_t operatingModeChange;
    Handler_t serviceDomain;
    Handler_t lteCsCapability;
    Handler_t imsServiceInfo;
    Handler_t imsPdpError;
    Handler_t cellInfoChange;
    Handler_t nrIconChange;
} Indicator_t;

typedef struct
{
    shared_ptr<Listener::PhoneListener> phone;
    shared_ptr<Listener::TelephonyServingSystemListener> telephonyServingSystems[MAX_INSTANCE];
    shared_ptr<Listener::NetworkSelectionListener> networkSelections[MAX_INSTANCE];
    shared_ptr<Listener::DataServingSystemListener> dataServingSystems[MAX_INSTANCE];
    shared_ptr<Listener::ImsServingSystemListener> imsServingSystems[MAX_INSTANCE];
} Listener_t;

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
        Listener_t listeners;

        static PlatformAdaptor& GetInstance
        (
            void
        );
};

pa_result_t Utility::Convert::StringToU16
(
    const string& str,
    uint16_t* valuePtr
)
{
    if (valuePtr == nullptr)
    {
        PA_ERROR("valuePtr is nullptr.");
        return -EINVAL;
    }

    if (str.empty())
    {
        PA_ERROR("str is empty.");
        return -EINVAL;
    }

    for (unsigned char c : str)
    {
        if (!std::isdigit(c))
        {
            PA_ERROR("str contains non-digit %d.", static_cast<int>(c));
            return -EINVAL;
        }
    }

    long value = stoi(str, nullptr, 0);
    if (value < 0 || value > 999)
    {
        PA_ERROR("value %ld is out of range [0, 999]", value);
        return -ERANGE;
    }

    *valuePtr = static_cast<uint16_t>(value);

    return 0;
}

void Utility::Convert::String
(
    const string& str,
    uint8_t* validPtr,
    char* strPtr,
    size_t size
)
{
    if (validPtr == nullptr)
    {
        PA_ERROR("validPtr is nullptr.");
        return;
    }

    if (strPtr == nullptr)
    {
        PA_ERROR("strPtr is nullptr.");
        return;
    }

    if (str.empty())
        *validPtr = 0;
    else if (*validPtr)
    {
        if (size == 0)
            return;
        else if (size == 1)
        {
            *strPtr = '\0';
            return;
        }

        size_t bytes = str.size();
        if (bytes < size)
        {
            memcpy(strPtr, str.c_str(), bytes);
            strPtr[bytes] = '\0';
        }
        else
        {
            memcpy(strPtr, str.c_str(), size - 1);
            strPtr[size - 1] = '\0';
        }
    }
}

SlotId Utility::Convert::SlotToSlotId
(
    int slot
)
{
    switch (slot)
    {
        case 1:
            return SLOT_ID_1;
        case 2:
            return SLOT_ID_2;
        default:
            PA_ERROR("Invalid slot %d.", slot);
    }

    return INVALID_SLOT_ID;
}

int Utility::Convert::InstanceToSlot
(
    uint32_t instance
)
{
    if (instance >= MAX_INSTANCE)
        return -1;

    auto& pa = PlatformAdaptor::GetInstance();
    if(pa.managers.phone != nullptr)
        return pa.managers.phone->getSlotIdFromPhoneId(instance + 1);
    else
        return -1;
}

int Utility::Convert::InstanceToPhone
(
    uint32_t instance
)
{
    if (instance >= MAX_INSTANCE)
        return -1;

    return instance + 1;
}

uint32_t Utility::Convert::PhoneToInstance
(
    int phone
)
{
    switch (phone)
    {
        case 1:
            return 0;
        case 2:
            return 1;
        default:
            PA_ERROR("Invalid phone %d.", phone);
    }

    return MAX_INSTANCE;
}

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

taf_pa_radio_RatBitMask_t Utility::Convert::TelRatPreferenceToRat
(
    tel::RatPreference bitmask
)
{
    taf_pa_radio_RatBitMask_t result = 0x0;

    if (bitmask[tel::PREF_GSM])
        result |= TAF_PROP_RADIO_BITMASK_RAT_GSM;

    if (bitmask[tel::PREF_CDMA_1X] || bitmask[tel::PREF_CDMA_EVDO])
        result |= TAF_PROP_RADIO_BITMASK_RAT_CDMA;

    if (bitmask[tel::PREF_WCDMA])
        result |= TAF_PROP_RADIO_BITMASK_RAT_UMTS;

    if (bitmask[tel::PREF_TDSCDMA])
        result |= TAF_PROP_RADIO_BITMASK_RAT_TDSCDMA;

    if (bitmask[tel::PREF_LTE])
        result |= TAF_PROP_RADIO_BITMASK_RAT_LTE;

    if (bitmask[tel::PREF_NR5G])
        result |= TAF_PROP_RADIO_BITMASK_RAT_NR5G;

    return result;
}

pa_result_t Utility::Convert::Rat
(
    int slot,
    vector<tel::DeviceRatCapability> capabilities,
    taf_pa_radio_RatBitMask_t* bitmaskPtr
)
{
	if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return -EINVAL;
    }

    for (auto capability : capabilities)
    {
        if (capability.slotId == slot)
        {
            taf_pa_radio_RatBitMask_t bitmask = 0x0;

            if (capability.capabilities[(uint16_t)tel::RATCapability::GSM])
                bitmask |= TAF_PA_RADIO_BITMASK_RAT_GSM;

            if (capability.capabilities[(uint16_t)tel::RATCapability::CDMA] ||
               capability.capabilities[(uint16_t)tel::RATCapability::HDR])
                bitmask |= TAF_PA_RADIO_BITMASK_RAT_CDMA;

            if (capability.capabilities[(uint16_t)tel::RATCapability::WCDMA])
                bitmask |= TAF_PA_RADIO_BITMASK_RAT_UMTS;

            if (capability.capabilities[(uint16_t)tel::RATCapability::TDS])
                bitmask |= TAF_PA_RADIO_BITMASK_RAT_TDSCDMA;

            if (capability.capabilities[(uint16_t)tel::RATCapability::LTE])
                bitmask |= TAF_PA_RADIO_BITMASK_RAT_LTE;

            if (capability.capabilities[(uint16_t)tel::RATCapability::NR5G] ||
                capability.capabilities[(uint16_t)tel::RATCapability::NR5GSA])
                bitmask |= TAF_PA_RADIO_BITMASK_RAT_NR5G;

            *bitmaskPtr = bitmask;

            return 0;
        }
    }

    return -ENOENT;
}

tel::RatMask Utility::Convert::RatToTelRat
(
    taf_pa_radio_RatBitMask_t bitmask
)
{
    tel::RatMask result;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_GSM)
        result.set(tel::RatType::GSM);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_UMTS)
        result.set(tel::RatType::UMTS);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_LTE)
        result.set(tel::RatType::LTE);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_NR5G)
        result.set(tel::RatType::NR5G);

    return result;
}

tel::RatPreference Utility::Convert::RatToTelRatPreference
(
    taf_pa_radio_RatBitMask_t bitmask
)
{
    tel::RatPreference result;

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_GSM)
        result.set(tel::PREF_GSM);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_CDMA)
    {
        result.set(tel::PREF_CDMA_1X);
        result.set(tel::PREF_CDMA_EVDO);
    }

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_UMTS)
        result.set(tel::PREF_WCDMA);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_TDSCDMA)
        result.set(tel::PREF_TDSCDMA);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_LTE)
        result.set(tel::PREF_LTE);

    if (bitmask & TAF_PA_RADIO_BITMASK_RAT_NR5G)
        result.set(tel::PREF_NR5G);

    return result;
}

taf_pa_radio_RatBitMask_t Utility::Convert::Rat
(
    tel::RatMask bitmask
)
{
    taf_pa_radio_RatBitMask_t result = 0;

    if (bitmask[tel::RatType::GSM])
        result |= TAF_PA_RADIO_BITMASK_RAT_GSM;

    if (bitmask[tel::RatType::UMTS])
        result |= TAF_PA_RADIO_BITMASK_RAT_UMTS;

    if (bitmask[tel::RatType::LTE])
        result |= TAF_PA_RADIO_BITMASK_RAT_LTE;

    if (bitmask[tel::RatType::NR5G])
        result |= TAF_PA_RADIO_BITMASK_RAT_NR5G;

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

taf_pa_radio_Rat_t Utility::Convert::Rat
(
    tel::RadioTechnology rat
)
{
    switch (rat)
    {
        case tel::RadioTechnology::RADIO_TECH_GSM:
        case tel::RadioTechnology::RADIO_TECH_GPRS:
        case tel::RadioTechnology::RADIO_TECH_EDGE:
            return TAF_PA_RADIO_RAT_GSM;
        case tel::RadioTechnology::RADIO_TECH_IS95A:
        case tel::RadioTechnology::RADIO_TECH_IS95B:
        case tel::RadioTechnology::RADIO_TECH_1xRTT:
        case tel::RadioTechnology::RADIO_TECH_EVDO_0:
        case tel::RadioTechnology::RADIO_TECH_EVDO_A:
        case tel::RadioTechnology::RADIO_TECH_EVDO_B:
        case tel::RadioTechnology::RADIO_TECH_EHRPD:
            return TAF_PA_RADIO_RAT_CDMA;
        case tel::RadioTechnology::RADIO_TECH_UMTS:
        case tel::RadioTechnology::RADIO_TECH_HSDPA:
        case tel::RadioTechnology::RADIO_TECH_HSUPA:
        case tel::RadioTechnology::RADIO_TECH_HSPA:
        case tel::RadioTechnology::RADIO_TECH_HSPAP:
            return TAF_PA_RADIO_RAT_UMTS;
        case tel::RadioTechnology::RADIO_TECH_TD_SCDMA:
            return TAF_PA_RADIO_RAT_TDSCDMA;
        case tel::RadioTechnology::RADIO_TECH_LTE:
        case tel::RadioTechnology::RADIO_TECH_LTE_CA:
            return TAF_PA_RADIO_RAT_LTE;
        case tel::RadioTechnology::RADIO_TECH_NR5G:
            return TAF_PA_RADIO_RAT_NR5G;
        default:
            PA_DEBUG("Unknown RAT %d.", rat);
    }

    return TAF_PA_RADIO_RAT_UNKNOWN;
}

tel::RadioTechnology Utility::Convert::RatToTelRat
(
    taf_pa_radio_Rat_t rat
)
{
    switch (rat)
    {
        case TAF_PA_RADIO_RAT_GSM:
            return tel::RadioTechnology::RADIO_TECH_GSM;
        case TAF_PA_RADIO_RAT_UMTS:
            return tel::RadioTechnology::RADIO_TECH_UMTS;
        case TAF_PA_RADIO_RAT_TDSCDMA:
            return tel::RadioTechnology::RADIO_TECH_TD_SCDMA;
        case TAF_PA_RADIO_RAT_LTE:
            return tel::RadioTechnology::RADIO_TECH_LTE;
        case TAF_PA_RADIO_RAT_NR5G:
            return tel::RadioTechnology::RADIO_TECH_NR5G;
        default:
            PA_DEBUG("Unknown RAT %d.", rat);
    }

    return tel::RadioTechnology::RADIO_TECH_UNKNOWN;
}

taf_pa_radio_ServiceDomain_t Utility::Convert::ServiceDomain
(
    tel::ServiceDomain domain
)
{
    switch (domain)
    {
        case tel::ServiceDomain::NO_SRV:
            return TAF_PA_RADIO_SERVICE_DOMAIN_NO_SERVICE;
        case tel::ServiceDomain::CS_ONLY:
            return TAF_PA_RADIO_SERVICE_DOMAIN_CS_ONLY;
        case tel::ServiceDomain::PS_ONLY:
            return TAF_PA_RADIO_SERVICE_DOMAIN_PS_ONLY;
        case tel::ServiceDomain::CS_PS:
            return TAF_PA_RADIO_SERVICE_DOMAIN_CS_AND_PS;
        case tel::ServiceDomain::CAMPED:
            return TAF_PA_RADIO_SERVICE_DOMAIN_CAMPED;
    }

    return TAF_PA_RADIO_SERVICE_DOMAIN_UNKNOWN;
}

taf_pa_radio_RatServiceStatus_t Utility::Convert::RatServiceStatus
(
    tel::ServiceRegistrationState state
)
{
    switch (state)
    {
        case tel::ServiceRegistrationState::NO_SERVICE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_NO_SERVICE;
        case tel::ServiceRegistrationState::LIMITED_SERVICE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_LIMITED;
        case tel::ServiceRegistrationState::IN_SERVICE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_SERVICE;
        case tel::ServiceRegistrationState::LIMITED_REGIONAL:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_LIMITED_REGIONAL;
        case tel::ServiceRegistrationState::POWER_SAVE:
            return TAF_PA_RADIO_RAT_SERVICE_STATUS_POWER_SAVE;
        default:
            PA_DEBUG("Unknown RAT service status.");
    }

    return TAF_PA_RADIO_RAT_SERVICE_STATUS_UNKNOWN;
}

taf_pa_radio_ServiceDomainBitMask_t Utility::Convert::ServiceDomainPreference
(
    tel::ServiceDomainPreference domain
)
{
    switch (domain)
    {
        case tel::ServiceDomainPreference::CS_ONLY:
            return TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_CS_ONLY;
        case tel::ServiceDomainPreference::PS_ONLY:
            return TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_PS_ONLY;
        case tel::ServiceDomainPreference::CS_PS:
            return TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_CS_AND_PS;
        default:
            PA_ERROR("Unknown service domain preference.");
    }

    return 0;
}

tel::ServiceDomainPreference Utility::Convert::ServiceDomainPreference
(
    taf_pa_radio_ServiceDomainBitMask_t bitmask
)
{
    if (bitmask & TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_CS_ONLY)
        return tel::ServiceDomainPreference::CS_ONLY;

    if (bitmask & TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_PS_ONLY)
        return tel::ServiceDomainPreference::PS_ONLY;

    if (bitmask & TAF_PA_RADIO_BITMASK_SERVICE_DOMAIN_CS_AND_PS)
        return tel::ServiceDomainPreference::CS_PS;

    return tel::ServiceDomainPreference::UNKNOWN;
}

void Utility::Convert::VoiceServiceInfo
(
    tel::VoiceServiceState state,
    taf_pa_radio_VoiceServiceInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return;
    }

    switch (state)
    {
        case tel::VoiceServiceState::NOT_REG_AND_NOT_SEARCHING:
            infoPtr->emergModeValid = 0;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_NOT_REGISTERED;
            break;
        case tel::VoiceServiceState::REG_HOME:
            infoPtr->roamingIndicatorValid = 0;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_REGISTERED;
            break;
        case tel::VoiceServiceState::NOT_REG_AND_SEARCHING:
            infoPtr->emergModeValid = 0;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING;
            break;
        case tel::VoiceServiceState::REG_DENIED:
            infoPtr->emergModeValid = 0;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_DENIED;
            break;
        case tel::VoiceServiceState::REG_ROAMING:
            infoPtr->roamingIndicatorValid = 1;
            infoPtr->roamingIndicator = TAF_PA_RADIO_ROAMING_INDICATOR_ON;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_REGISTERED;
            break;
        case tel::VoiceServiceState::NOT_REG_AND_EMERGENCY_AVAILABLE_AND_NOT_SEARCHING:
            infoPtr->emergModeValid = 1;
            infoPtr->emergMode = TAF_PA_RADIO_EMERGENCY_MODE_ON;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_NOT_REGISTERED;
            break;
        case tel::VoiceServiceState::NOT_REG_AND_EMERGENCY_AVAILABLE_AND_SEARCHING:
            infoPtr->emergModeValid = 1;
            infoPtr->emergMode = TAF_PA_RADIO_EMERGENCY_MODE_ON;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_NOT_REGISTERED_SEARCHING;
            break;
        case tel::VoiceServiceState::REG_DENIED_AND_EMERGENCY_AVAILABLE:
            infoPtr->emergModeValid = 1;
            infoPtr->emergMode = TAF_PA_RADIO_EMERGENCY_MODE_ON;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_DENIED;
            break;
        case tel::VoiceServiceState::UNKNOWN_AND_EMERGENCY_AVAILABLE:
            infoPtr->emergModeValid = 1;
            infoPtr->emergMode = TAF_PA_RADIO_EMERGENCY_MODE_ON;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_UNKNOWN;
            break;
        default:
            infoPtr->emergModeValid = 0;
            infoPtr->roamingIndicatorValid = 0;
            infoPtr->regState = TAF_PA_RADIO_REGISTRATION_STATE_UNKNOWN;
    }
}

taf_pa_radio_SignalStrengthLevel_t Utility::Convert::SignalStrengthLevel
(
    tel::SignalStrengthLevel level
)
{
    switch (level)
    {
        case tel::SignalStrengthLevel::LEVEL_1:
            return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_1;
        case tel::SignalStrengthLevel::LEVEL_2:
            return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_2;
        case tel::SignalStrengthLevel::LEVEL_3:
            return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_3;
        case tel::SignalStrengthLevel::LEVEL_4:
            return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_4;
        case tel::SignalStrengthLevel::LEVEL_5:
            return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_5;
        default:
            PA_DEBUG("Unknown level %d.", level);
    }

    return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_UNKNOWN;
}

taf_pa_radio_SignalStrengthLevel_t Utility::Convert::SignalStrengthLevel
(
    taf_pa_radio_Rat_t rat,
    shared_ptr<tel::SignalStrength> strengthPtr
)
{
    if (strengthPtr == nullptr)
    {
        PA_ERROR("strengthPtr is nullptr.");
        return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_UNKNOWN;
    }

    switch (rat)
    {
        case TAF_PA_RADIO_RAT_GSM:
            if (strengthPtr->getGsmSignalStrength() != nullptr)
                return Utility::Convert::SignalStrengthLevel(
                    strengthPtr->getGsmSignalStrength()->getLevel());
            break;
        case TAF_PA_RADIO_RAT_CDMA:
            // deprecated TelSDK API usage
            // if (strengthPtr->getCdmaSignalStrength() != nullptr)
            //     return Utility::Convert::SignalStrengthLevel(
            //         strengthPtr->getCdmaSignalStrength()->getLevel());
            return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_UNKNOWN;
        case TAF_PA_RADIO_RAT_UMTS:
            if (strengthPtr->getWcdmaSignalStrength() != nullptr)
                return Utility::Convert::SignalStrengthLevel(
                    strengthPtr->getWcdmaSignalStrength()->getLevel());
            break;
        case TAF_PA_RADIO_RAT_LTE:
            if (strengthPtr->getLteSignalStrength() != nullptr)
                return Utility::Convert::SignalStrengthLevel(
                    strengthPtr->getLteSignalStrength()->getLevel());
            break;
        case TAF_PA_RADIO_RAT_NR5G:
            if (strengthPtr->getNr5gSignalStrength() != nullptr)
                return Utility::Convert::SignalStrengthLevel(
                    strengthPtr->getNr5gSignalStrength()->getLevel());
            break;
        default:
            PA_DEBUG("Unknown RAT %d.", rat);
    }

    return TAF_PA_RADIO_SIGNAL_STRENGTH_LEVEL_UNKNOWN;
}

void Utility::Convert::SignalStrengthInfo
(
    shared_ptr<tel::SignalStrength> strengthPtr,
    taf_pa_radio_SignalStrengthInfo_t* infoPtr
)
{
    if (strengthPtr == nullptr)
    {
        PA_ERROR("strengthPtr is nullptr.");
        return;
    }

    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return;
    }

    infoPtr->bitmask = 0x0;

    if (strengthPtr->getGsmSignalStrength() != nullptr &&
        strengthPtr->getGsmSignalStrength()->getGsmSignalStrength() !=
        INVALID_SIGNAL_STRENGTH_VALUE)
    {
        infoPtr->bitmask |= TAF_PA_RADIO_BITMASK_RAT_GSM;
        infoPtr->gsmInfo.rssi = strengthPtr->getGsmSignalStrength()->getDbm();
        // infoPtr->gsmInfo.ber = strengthPtr->getGsmSignalStrength()->getGsmBitErrorRate();
        infoPtr->gsmInfo.ber = INVALID_SIGNAL_STRENGTH_VALUE;
    }

    /*
    // deprecated TelSDK API usage
    if (strengthPtr->getCdmaSignalStrength() != nullptr &&
        strengthPtr->getCdmaSignalStrength()->getDbm() != INVALID_SIGNAL_STRENGTH_VALUE)
    {
        infoPtr->bitmask |= TAF_PA_RADIO_BITMASK_RAT_CDMA;
        infoPtr->cdmaInfo.ss = strengthPtr->getCdmaSignalStrength()->getDbm();
        infoPtr->cdmaInfo.ecio = strengthPtr->getCdmaSignalStrength()->getCdmaEcio();
        infoPtr->cdmaInfo.io = strengthPtr->getCdmaSignalStrength()->getEvdoEcio();
        infoPtr->cdmaInfo.sinr = strengthPtr->getCdmaSignalStrength()->getEvdoSignalNoiseRatio();
    }
    */

    if (strengthPtr->getWcdmaSignalStrength() != nullptr &&
        strengthPtr->getWcdmaSignalStrength()->getSignalStrength() !=
        INVALID_SIGNAL_STRENGTH_VALUE)
    {
        infoPtr->bitmask |= TAF_PA_RADIO_BITMASK_RAT_UMTS;
        infoPtr->umtsInfo.ss = strengthPtr->getWcdmaSignalStrength()->getDbm();
        // infoPtr->umtsInfo.ber = strengthPtr->getWcdmaSignalStrength()->getBitErrorRate();
        infoPtr->umtsInfo.ber = INVALID_SIGNAL_STRENGTH_VALUE;
        infoPtr->umtsInfo.rscp = strengthPtr->getWcdmaSignalStrength()->getRscp();
    }

    /*
    // deprecated TelSDK API usage
    if (strengthPtr->getTdscdmaSignalStrength() != nullptr &&
        strengthPtr->getTdscdmaSignalStrength()->getRscp() != INVALID_SIGNAL_STRENGTH_VALUE)
    {
        infoPtr->bitmask |= TAF_PA_RADIO_BITMASK_RAT_TDSCDMA;
        infoPtr->tdscdmaInfo.rscp = strengthPtr->getTdscdmaSignalStrength()->getRscp();
    }
    */

    if (strengthPtr->getLteSignalStrength() != nullptr &&
        strengthPtr->getLteSignalStrength()->getLteSignalStrength() !=
        INVALID_SIGNAL_STRENGTH_VALUE)
    {
        infoPtr->bitmask |= TAF_PA_RADIO_BITMASK_RAT_LTE;
        infoPtr->lteInfo.rssi = strengthPtr->getLteSignalStrength()->getDbm();
        infoPtr->lteInfo.rsrq =
            strengthPtr->getLteSignalStrength()->getLteReferenceSignalReceiveQuality();
        infoPtr->lteInfo.rsrp = strengthPtr->getLteSignalStrength()->getDbm();
        infoPtr->lteInfo.snr = strengthPtr->getLteSignalStrength()->getLteReferenceSignalSnr();
    }

    if (strengthPtr->getNr5gSignalStrength() != nullptr &&
        strengthPtr->getNr5gSignalStrength()->getDbm() != INVALID_SIGNAL_STRENGTH_VALUE)
    {
        infoPtr->bitmask |= TAF_PA_RADIO_BITMASK_RAT_NR5G;
        infoPtr->nr5gInfo.rsrq =
            strengthPtr->getNr5gSignalStrength()->getReferenceSignalReceiveQuality();
        infoPtr->nr5gInfo.rsrp = strengthPtr->getNr5gSignalStrength()->getDbm();
        infoPtr->nr5gInfo.snr = strengthPtr->getNr5gSignalStrength()->getReferenceSignalSnr();
    }
}

tel::SignalStrengthMeasurementType Utility::Convert::SignalMetric
(
    taf_pa_radio_SignalMetric_t metric
)
{
    switch (metric)
    {
        case TAF_PA_RADIO_SIGNAL_METRIC_RSSI:
            return tel::SignalStrengthMeasurementType::RSSI;
        case TAF_PA_RADIO_SIGNAL_METRIC_ECIO:
            return tel::SignalStrengthMeasurementType::ECIO;
        case TAF_PA_RADIO_SIGNAL_METRIC_IO:
            return tel::SignalStrengthMeasurementType::IO;
        case TAF_PA_RADIO_SIGNAL_METRIC_SINR:
            return tel::SignalStrengthMeasurementType::SINR;
        case TAF_PA_RADIO_SIGNAL_METRIC_RSCP:
            return tel::SignalStrengthMeasurementType::RSCP;
        case TAF_PA_RADIO_SIGNAL_METRIC_RSRP:
            return tel::SignalStrengthMeasurementType::RSRP;
        case TAF_PA_RADIO_SIGNAL_METRIC_RSRQ:
            return tel::SignalStrengthMeasurementType::RSRQ;
        case TAF_PA_RADIO_SIGNAL_METRIC_SNR:
            return tel::SignalStrengthMeasurementType::SNR;
        default:
            PA_ERROR("Unknown metric %d.", metric);
    }

    return static_cast<tel::SignalStrengthMeasurementType>(-1);
}

void Utility::Convert::SignalStrengthIndConfig
(
    taf_pa_radio_SignalStrengthIndConfig_t* configPtr,
    vector<tel::SignalStrengthConfigEx>& config
)
{
    if (configPtr == nullptr)
    {
        PA_ERROR("configPtr is nullptr.");
        return;
    }

    if (configPtr->thresholdValid && configPtr->thresholdCount != 0)
    {
        tel::SignalStrengthConfigEx configEx;

        tel::SignalStrengthConfigData data;
        data.sigMeasType = Utility::Convert::SignalMetric(configPtr->metric);
        for (uint32_t i = 0; i < THRESHOLD_LIST_MAX; i++)
            data.thresholdList[i] = 0;
        for (uint32_t i = 0; i < TAF_PA_RADIO_SIGNAL_STRENGTH_THRESHOLD_MAX_COUNT &&
            i < configPtr->thresholdCount && i < THRESHOLD_LIST_MAX; i++)
            data.thresholdList[i] = configPtr->thresholds[i];
        if (configPtr->hysteresisDeltaValid)
        {
            configEx.configTypeMask.set(tel::SignalStrengthConfigExType::HYSTERESIS_DB);
            data.hysteresisDb = configPtr->hysteresisDelta;
        }

        configEx.sigConfigData.emplace_back(data);
        configEx.configTypeMask.set(tel::SignalStrengthConfigExType::THRESHOLD);
        configEx.radioTech = Utility::Convert::RatToTelRat(configPtr->rat);
        config.emplace_back(configEx);
    }

    if (configPtr->deltaValid)
    {
        tel::SignalStrengthConfigEx configEx;

        tel::SignalStrengthConfigData data;
        data.sigMeasType = Utility::Convert::SignalMetric(configPtr->metric);
        data.delta = configPtr->delta;

        configEx.sigConfigData.emplace_back(data);
        configEx.configTypeMask.set(tel::SignalStrengthConfigExType::DELTA);
        configEx.radioTech = Utility::Convert::RatToTelRat(configPtr->rat);
        config.emplace_back(configEx);
    }
}

taf_pa_radio_BandBitMask_t Utility::Convert::Band
(
    shared_ptr<tel::IRFBandList> listPtr
)
{
    taf_pa_radio_BandBitMask_t bitmask = 0x0;

    if (listPtr == nullptr)
    {
        PA_ERROR("listPtr is nullptr.");
        return bitmask;
    }

    vector<tel::GsmRFBand> gsmBands = listPtr->getGsmBands();
    if(!gsmBands.empty())
    {
        for (auto gsmBand : gsmBands)
        {
            switch (gsmBand)
            {
                case tel::GsmRFBand::GSM_450:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_450;
                    break;
                case tel::GsmRFBand::GSM_480:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_480;
                    break;
                case tel::GsmRFBand::GSM_750:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_750;
                    break;
                case tel::GsmRFBand::GSM_850:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_850;
                    break;
                case tel::GsmRFBand::GSM_900_EXTENDED:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_CLASS_E_GSM_900_BAND;
                    break;
                case tel::GsmRFBand::GSM_900_PRIMARY:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_CLASS_P_GSM_900_BAND;
                    break;
                case tel::GsmRFBand::GSM_900_RAILWAYS:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_RAILWAYS_900_BAND;
                    break;
                case tel::GsmRFBand::GSM_1800:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_CLASS_GSM_DCS_1800_BAND;
                    break;
                case tel::GsmRFBand::GSM_1900:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_GSM_PCS_1900_BAND;
                    break;
                default:
                    PA_ERROR("Unknown GSM band.");
                    break;
            }
        }
    }

    vector<tel::WcdmaRFBand> wcdmaBands = listPtr->getWcdmaBands();
    if(!wcdmaBands.empty())
    {
        for (auto wcdmaBand : wcdmaBands)
        {
            switch (wcdmaBand)
            {
                case tel::WcdmaRFBand::WCDMA_2100:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_CH_IMT_2100_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_PCS_1900:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_PCS_1900_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_DCS_1800:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_CH_DCS_1800_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_1700_US:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_1700_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_850:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_850_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_800:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_800_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_2600:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_2600_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_900:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_900_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_1700_JAPAN:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_1700_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_1500_JAPAN:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_1500_BAND;
                    break;
                case tel::WcdmaRFBand::WCDMA_850_JAPAN:
                    bitmask |= TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_850_BAND;
                    break;
                default:
                    PA_ERROR("Unknown WCDMA band.");
                    break;
            }
        }
    }

    return bitmask;
}

void Utility::Convert::Band
(
    shared_ptr<tel::IRFBandList> listPtr,
    taf_pa_radio_LteBand_t* bandPtr
)
{
    if (listPtr == nullptr)
    {
        PA_ERROR("listPtr is nullptr.");
        return;
    }

    if (bandPtr == nullptr)
    {
        PA_ERROR("bandPtr is nullptr.");
        return;
    }

    for (uint32_t i = 0; i < TAF_PA_RADIO_LTE_BAND_GROUP_COUNT; i++)
        bandPtr->bitmask[i] = 0;

    for (auto band : listPtr->getLteBands())
    {
        if (band < tel::LteRFBand::E_UTRA_BAND_1 || band > tel::LteRFBand::E_UTRA_BAND_256)
            PA_ERROR("Invalid LTE RF band.");
        else
        {
            uint8_t bandIndex = static_cast<uint8_t>(band) - 1;
            uint8_t groupIndex = bandIndex / LTE_BAND_NUM_PER_GROUP;
            uint8_t bitIndex = bandIndex % LTE_BAND_NUM_PER_GROUP;

            if (groupIndex < TAF_PA_RADIO_LTE_BAND_GROUP_COUNT)
                bandPtr->bitmask[groupIndex] |= (uint64_t)0x1 << bitIndex;
            else
                PA_ERROR("Invalid group %d.", groupIndex);
        }
    }
}

taf_pa_radio_DataServiceState_t Utility::Convert::ServiceState
(
    data::DataServiceState state
)
{
    switch (state)
    {
        case data::DataServiceState::IN_SERVICE:
            return TAF_PA_RADIO_DATA_SERVICE_STATE_IN_SERVICE;
        case data::DataServiceState::OUT_OF_SERVICE:
            return TAF_PA_RADIO_DATA_SERVICE_STATE_OUT_OF_SERVICE;
        default:
            PA_DEBUG("Unknown data service state.");
    }

    return TAF_PA_RADIO_DATA_SERVICE_STATE_UNKNOWN;
}

taf_pa_radio_ImsRegistrationStatus_t Utility::Convert::ImsRegistrationStatus
(
     tel::RegistrationStatus status
)
{
    switch (status)
    {
        case tel::RegistrationStatus::NOT_REGISTERED:
            return TAF_PA_RADIO_IMS_REGISTRATION_STATUS_NOT_REGISTERED;
        case tel::RegistrationStatus::REGISTERING:
            return TAF_PA_RADIO_IMS_REGISTRATION_STATUS_REGISTRERING;
        case tel::RegistrationStatus::REGISTERED:
            return TAF_PA_RADIO_IMS_REGISTRATION_STATUS_REGISTERED;
        case tel::RegistrationStatus::LIMITED_REGISTERED:
            return TAF_PA_RADIO_IMS_REGISTRATION_STATUS_LIMITED_REGISTERED;
        default:
            PA_DEBUG("Unknown IMS registration state.");
    }

    return TAF_PA_RADIO_IMS_REGISTRATION_STATUS_UNKNOWN;
}


taf_pa_radio_LteCsCapability_t Utility::Convert::LteCsCapability
(
    tel::LteCsCapability capability
)
{
    switch (capability)
    {
        case tel::LteCsCapability::FULL_SERVICE:
            return TAF_PA_RADIO_LTE_CS_CAPABILITY_FULL_SERVICE;
        case tel::LteCsCapability::CSFB_NOT_PREFERRED:
            return TAF_PA_RADIO_LTE_CS_CAPABILITY_CSFB_NOT_PREFERRED;
        case tel::LteCsCapability::SMS_ONLY:
            return TAF_PA_RADIO_LTE_CS_CAPABILITY_SMS_ONLY;
        case tel::LteCsCapability::LIMITED:
            return TAF_PA_RADIO_LTE_CS_CAPABILITY_LIMITED;
        case tel::LteCsCapability::BARRED:
            return TAF_PA_RADIO_LTE_CS_CAPABILITY_BARRED;
        default:
            PA_INFO("Unknown LTE CS capability.");
    }

    return TAF_PA_RADIO_LTE_CS_CAPABILITY_UNKNOWN;
}

taf_pa_radio_ImsServiceStatus_t Utility::Convert::ImsServiceStatus
(
    tel::CellularServiceStatus status
)
{
    switch (status)
    {
        case tel::CellularServiceStatus::NO_SERVICE:
            return TAF_PA_RADIO_IMS_SERVICE_STATUS_NO_SERVICE;
        case tel::CellularServiceStatus::LIMITED_SERVICE:
            return TAF_PA_RADIO_IMS_SERVICE_STATUS_LIMITED_SERVICE;
        case tel::CellularServiceStatus::FULL_SERVICE:
            return TAF_PA_RADIO_IMS_SERVICE_STATUS_FULL_SERVICE;
        default:
            PA_INFO("Unknown IMS service status.");
    }

    return TAF_PA_RADIO_IMS_SERVICE_STATUS_UNKNOWN;
}

pa_result_t Utility::Convert::ImsServiceStatus
(
    taf_pa_radio_ImsService_t service,
    tel::ImsServiceInfo info,
    taf_pa_radio_ImsServiceStatus_t* statusPtr
)
{
    if (statusPtr == nullptr)
    {
        PA_ERROR("statusPtr is nullptr.");
        return -EINVAL;
    }

    switch (service)
    {
        case TAF_PA_RADIO_IMS_SERVICE_SMS:
            *statusPtr = Utility::Convert::ImsServiceStatus(info.sms);
            return 0;
        case TAF_PA_RADIO_IMS_SERVICE_VOIP:
            *statusPtr = Utility::Convert::ImsServiceStatus(info.voice);
            return 0;
        default:
            PA_ERROR("Unsupported IMS service %d.", service);
    }

    return -ENOTSUP;
}

taf_pa_radio_ImsPdpFailureErrorCode_t Utility::Convert::ImsPdpFailureErrorCode
(
    tel::PdpFailureCode code
)
{
    switch (code)
    {
        case tel::PdpFailureCode::OTHER_FAILURE:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_OTHER_FAILURE;
        case tel::PdpFailureCode::OPTION_UNSUBSCRIBED:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_OPTION_UNSUBSCRIBED;
        case tel::PdpFailureCode::UNKNOWN_PDP:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_UNKNOWN_PDP;
        case tel::PdpFailureCode::REASON_NOT_SPECIFIED:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_REASON_NOT_SPECIFIED;
        case tel::PdpFailureCode::CONNECTION_BRINGUP_FAILURE:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_CONNECTION_BRINGUP_FAILURE;
        case tel::PdpFailureCode::CONNECTION_IKE_AUTH_FAILURE:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_CONNECTION_IKE_AUTH_FAILURE;
        case tel::PdpFailureCode::USER_AUTH_FAILED:
            return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_USER_AUTH_FAILED;
        default:
            PA_INFO("Unknown IMS PDP failure error code");
    }

    return TAF_PA_RADIO_IMS_PDP_FAILURE_ERROR_CODE_UNKNOWN;
}

void Utility::Convert::ImsServiceConfig
(
    taf_pa_radio_ImsServiceSettingBitMask_t bitmask,
    bool enable,
    tel::ImsServiceConfig* configPtr
)
{
    if (configPtr == nullptr)
    {
        PA_ERROR("configPtr is nullptr.");
        return;
    }

    if (bitmask & TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_SMS)
    {
        configPtr->configValidityMask.set(tel::ImsServiceConfigType::IMSSETTINGS_SMS);
        configPtr->smsEnabled = enable;
    }

    if (bitmask & TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_RTT)
    {
        configPtr->configValidityMask.set(tel::ImsServiceConfigType::IMSSETTINGS_RTT);
        configPtr->rttEnabled = enable;
    }

    if (bitmask & TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VOLTE)
    {
        configPtr->configValidityMask.set(tel::ImsServiceConfigType::IMSSETTINGS_VOIMS);
        configPtr->voImsEnabled = enable;
    }

    if (bitmask & TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_IMS_SERVICE)
    {
        configPtr->configValidityMask.set(tel::ImsServiceConfigType::IMSSETTINGS_IMS_SERVICE);
        configPtr->imsServiceEnabled = enable;
    }
}

void Utility::Convert::ImsService
(
    tel::ImsServiceConfig config,
    taf_pa_radio_ImsServiceSettingBitMask_t* bitmaskPtr
)
{
    if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return;
    }

    if (config.configValidityMask[tel::ImsServiceConfigType::IMSSETTINGS_SMS] && config.smsEnabled)
        *bitmaskPtr |= TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_SMS;

    if (config.configValidityMask[tel::ImsServiceConfigType::IMSSETTINGS_RTT] && config.rttEnabled)
        *bitmaskPtr |= TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_RTT;

    if (config.configValidityMask[tel::ImsServiceConfigType::IMSSETTINGS_VOIMS] &&
        config.voImsEnabled)
        *bitmaskPtr |= TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VOLTE;

    if (config.configValidityMask[tel::ImsServiceConfigType::IMSSETTINGS_IMS_SERVICE] &&
       config.imsServiceEnabled)
        *bitmaskPtr |= TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_IMS_SERVICE;
}

taf_pa_radio_EndcAvailability_t Utility::Convert::EndcAvailability
(
    tel::EndcAvailability availability
)
{
    switch (availability)
    {
        case tel::EndcAvailability::AVAILABLE:
            return TAF_PA_RADIO_ENDC_AVAILABILITY_AVAILABLE;
        case tel::EndcAvailability::UNAVAILABLE:
            return TAF_PA_RADIO_ENDC_AVAILABILITY_UNAVAILABLE;
        default:
            PA_DEBUG("Unknown ENDC availability.");
    }

    return TAF_PA_RADIO_ENDC_AVAILABILITY_UNKNOWN;
}

taf_pa_radio_DcnrRestriction_t Utility::Convert::DcnrRestriction
(
    tel::DcnrRestriction restriction
)
{
    switch (restriction)
    {
        case tel::DcnrRestriction::RESTRICTED:
            return TAF_PA_RADIO_DCNR_RESTRICTION_RESTRICTED;
        case tel::DcnrRestriction::UNRESTRICTED:
            return TAF_PA_RADIO_DCNR_RESTRICTION_NOT_RESTRICTED;
        default:
            PA_DEBUG("Unknown DCNR restriction.");
    }

    return TAF_PA_RADIO_DCNR_RESTRICTION_UNKNOWN;
}

taf_pa_radio_BandBitMask_t Utility::Convert::ActiveBand
(
    tel::RFBand band
)
{
    switch (band)
    {
        case tel::RFBand::GSM_450:
            return TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_450;
        case tel::RFBand::GSM_480:
            return TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_480;
        case tel::RFBand::GSM_750:
            return TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_750;
        case tel::RFBand::GSM_850:
            return TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_850;
        case tel::RFBand::GSM_900_EXTENDED:
            return TAF_PA_RADIO_BITMASK_BAND_CLASS_E_GSM_900_BAND;
        case tel::RFBand::GSM_900_PRIMARY:
            return TAF_PA_RADIO_BITMASK_BAND_CLASS_P_GSM_900_BAND;
        case tel::RFBand::GSM_900_RAILWAYS:
            return TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_RAILWAYS_900_BAND;
        case tel::RFBand::GSM_1800:
            return TAF_PA_RADIO_BITMASK_BAND_CLASS_GSM_DCS_1800_BAND;
        case tel::RFBand::GSM_1900:
            return TAF_PA_RADIO_BITMASK_BAND_GSM_PCS_1900_BAND;
        case tel::RFBand::WCDMA_2100:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_CH_IMT_2100_BAND;
        case tel::RFBand::WCDMA_PCS_1900:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_PCS_1900_BAND;
        case tel::RFBand::WCDMA_DCS_1800:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_CH_DCS_1800_BAND;
        case tel::RFBand::WCDMA_1700_US:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_1700_BAND;
        case tel::RFBand::WCDMA_850:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_850_BAND;
        case tel::RFBand::WCDMA_800:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_800_BAND;
        case tel::RFBand::WCDMA_2600:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_2600_BAND;
        case tel::RFBand::WCDMA_900:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_900_BAND;
        case tel::RFBand::WCDMA_1700_JAPAN:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_1700_BAND;
        case tel::RFBand::WCDMA_1500_JAPAN:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_1500_BAND;
        case tel::RFBand::WCDMA_850_JAPAN:
            return TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_850_BAND;
        case tel::RFBand::TDSCDMA_BAND_A:
            return TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_A;
        case tel::RFBand::TDSCDMA_BAND_B:
            return TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_B;
        case tel::RFBand::TDSCDMA_BAND_C:
            return TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_C;
        case tel::RFBand::TDSCDMA_BAND_D:
            return TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_D;
        case tel::RFBand::TDSCDMA_BAND_E:
            return TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_E;
        case tel::RFBand::TDSCDMA_BAND_F:
            return TAF_PA_RADIO_BITMASK_BAND_TDSCDMA_BAND_F;
        default:
            PA_DEBUG("Unknown active band.");
    }

    return 0x0;
}

uint32_t Utility::Convert::LteActiveBand
(
    tel::RFBand band
)
{
    switch (band)
    {
        case tel::RFBand::E_UTRA_OPERATING_BAND_1:
            return 1;
        case tel::RFBand::E_UTRA_OPERATING_BAND_2:
            return 2;
        case tel::RFBand::E_UTRA_OPERATING_BAND_3:
            return 3;
        case tel::RFBand::E_UTRA_OPERATING_BAND_4:
            return 4;
        case tel::RFBand::E_UTRA_OPERATING_BAND_5:
            return 5;
        case tel::RFBand::E_UTRA_OPERATING_BAND_6:
            return 6;
        case tel::RFBand::E_UTRA_OPERATING_BAND_7:
            return 7;
        case tel::RFBand::E_UTRA_OPERATING_BAND_8:
            return 8;
        case tel::RFBand::E_UTRA_OPERATING_BAND_9:
            return 9;
        case tel::RFBand::E_UTRA_OPERATING_BAND_10:
            return 10;
        case tel::RFBand::E_UTRA_OPERATING_BAND_11:
            return 11;
        case tel::RFBand::E_UTRA_OPERATING_BAND_12:
            return 12;
        case tel::RFBand::E_UTRA_OPERATING_BAND_13:
            return 13;
        case tel::RFBand::E_UTRA_OPERATING_BAND_14:
            return 14;
        case tel::RFBand::E_UTRA_OPERATING_BAND_17:
            return 17;
        case tel::RFBand::E_UTRA_OPERATING_BAND_18:
            return 18;
        case tel::RFBand::E_UTRA_OPERATING_BAND_19:
            return 19;
        case tel::RFBand::E_UTRA_OPERATING_BAND_20:
            return 20;
        case tel::RFBand::E_UTRA_OPERATING_BAND_21:
            return 21;
        case tel::RFBand::E_UTRA_OPERATING_BAND_23:
            return 23;
        case tel::RFBand::E_UTRA_OPERATING_BAND_24:
            return 24;
        case tel::RFBand::E_UTRA_OPERATING_BAND_25:
            return 25;
        case tel::RFBand::E_UTRA_OPERATING_BAND_26:
            return 26;
        case tel::RFBand::E_UTRA_OPERATING_BAND_27:
            return 27;
        case tel::RFBand::E_UTRA_OPERATING_BAND_28:
            return 28;
        case tel::RFBand::E_UTRA_OPERATING_BAND_29:
            return 29;
        case tel::RFBand::E_UTRA_OPERATING_BAND_30:
            return 30;
        case tel::RFBand::E_UTRA_OPERATING_BAND_31:
            return 31;
        case tel::RFBand::E_UTRA_OPERATING_BAND_32:
            return 32;
        case tel::RFBand::E_UTRA_OPERATING_BAND_33:
            return 33;
        case tel::RFBand::E_UTRA_OPERATING_BAND_34:
            return 34;
        case tel::RFBand::E_UTRA_OPERATING_BAND_35:
            return 35;
        case tel::RFBand::E_UTRA_OPERATING_BAND_36:
            return 36;
        case tel::RFBand::E_UTRA_OPERATING_BAND_37:
            return 37;
        case tel::RFBand::E_UTRA_OPERATING_BAND_38:
            return 38;
        case tel::RFBand::E_UTRA_OPERATING_BAND_39:
            return 39;
        case tel::RFBand::E_UTRA_OPERATING_BAND_40:
            return 40;
        case tel::RFBand::E_UTRA_OPERATING_BAND_41:
            return 41;
        case tel::RFBand::E_UTRA_OPERATING_BAND_42:
            return 42;
        case tel::RFBand::E_UTRA_OPERATING_BAND_43:
            return 43;
        case tel::RFBand::E_UTRA_OPERATING_BAND_46:
            return 46;
        case tel::RFBand::E_UTRA_OPERATING_BAND_47:
            return 47;
        case tel::RFBand::E_UTRA_OPERATING_BAND_48:
            return 48;
        case tel::RFBand::E_UTRA_OPERATING_BAND_49:
            return 49;
        case tel::RFBand::E_UTRA_OPERATING_BAND_53:
            return 53;
        case tel::RFBand::E_UTRA_OPERATING_BAND_66:
            return 66;
        case tel::RFBand::E_UTRA_OPERATING_BAND_67:
            return 67;
        case tel::RFBand::E_UTRA_OPERATING_BAND_68:
            return 68;
        case tel::RFBand::E_UTRA_OPERATING_BAND_70:
            return 70;
        case tel::RFBand::E_UTRA_OPERATING_BAND_71:
            return 71;
        case tel::RFBand::E_UTRA_OPERATING_BAND_72:
            return 72;
        case tel::RFBand::E_UTRA_OPERATING_BAND_73:
            return 73;
        case tel::RFBand::E_UTRA_OPERATING_BAND_85:
            return 85;
        case tel::RFBand::E_UTRA_OPERATING_BAND_86:
            return 86;
        case tel::RFBand::E_UTRA_OPERATING_BAND_87:
            return 87;
        case tel::RFBand::E_UTRA_OPERATING_BAND_88:
            return 88;
        case tel::RFBand::E_UTRA_OPERATING_BAND_125:
            return 125;
        case tel::RFBand::E_UTRA_OPERATING_BAND_126:
            return 126;
        case tel::RFBand::E_UTRA_OPERATING_BAND_127:
            return 127;
        case tel::RFBand::E_UTRA_OPERATING_BAND_250:
            return 250;
        default:
            PA_DEBUG("Unknown LTE active band.");
    }

    return 0;
}

uint32_t Utility::Convert::Nr5gActiveBand
(
    tel::RFBand band
)
{
    switch (band)
    {
        case tel::RFBand::NR5G_BAND_1:
            return 1;
        case tel::RFBand::NR5G_BAND_2:
            return 2;
        case tel::RFBand::NR5G_BAND_3:
            return 3;
        case tel::RFBand::NR5G_BAND_5:
            return 5;
        case tel::RFBand::NR5G_BAND_7:
            return 7;
        case tel::RFBand::NR5G_BAND_8:
            return 8;
        case tel::RFBand::NR5G_BAND_12:
            return 12;
        case tel::RFBand::NR5G_BAND_13:
            return 13;
        case tel::RFBand::NR5G_BAND_14:
            return 14;
        case tel::RFBand::NR5G_BAND_18:
            return 18;
        case tel::RFBand::NR5G_BAND_20:
            return 20;
        case tel::RFBand::NR5G_BAND_25:
            return 25;
        case tel::RFBand::NR5G_BAND_26:
            return 26;
        case tel::RFBand::NR5G_BAND_28:
            return 28;
        case tel::RFBand::NR5G_BAND_29:
            return 29;
        case tel::RFBand::NR5G_BAND_30:
            return 30;
        case tel::RFBand::NR5G_BAND_34:
            return 34;
        case tel::RFBand::NR5G_BAND_38:
            return 38;
        case tel::RFBand::NR5G_BAND_39:
            return 39;
        case tel::RFBand::NR5G_BAND_40:
            return 40;
        case tel::RFBand::NR5G_BAND_41:
            return 41;
        case tel::RFBand::NR5G_BAND_46:
            return 46;
        case tel::RFBand::NR5G_BAND_48:
            return 48;
        case tel::RFBand::NR5G_BAND_50:
            return 50;
        case tel::RFBand::NR5G_BAND_51:
            return 51;
        case tel::RFBand::NR5G_BAND_53:
            return 53;
        case tel::RFBand::NR5G_BAND_65:
            return 65;
        case tel::RFBand::NR5G_BAND_66:
            return 66;
        case tel::RFBand::NR5G_BAND_70:
            return 70;
        case tel::RFBand::NR5G_BAND_71:
            return 71;
        case tel::RFBand::NR5G_BAND_74:
            return 74;
        case tel::RFBand::NR5G_BAND_75:
            return 75;
        case tel::RFBand::NR5G_BAND_76:
            return 76;
        case tel::RFBand::NR5G_BAND_77:
            return 77;
        case tel::RFBand::NR5G_BAND_78:
            return 78;
        case tel::RFBand::NR5G_BAND_79:
            return 79;
        case tel::RFBand::NR5G_BAND_80:
            return 80;
        case tel::RFBand::NR5G_BAND_81:
            return 81;
        case tel::RFBand::NR5G_BAND_82:
            return 82;
        case tel::RFBand::NR5G_BAND_83:
            return 83;
        case tel::RFBand::NR5G_BAND_84:
            return 84;
        case tel::RFBand::NR5G_BAND_85:
            return 85;
        case tel::RFBand::NR5G_BAND_86:
            return 86;
        case tel::RFBand::NR5G_BAND_91:
            return 91;
        case tel::RFBand::NR5G_BAND_92:
            return 92;
        case tel::RFBand::NR5G_BAND_93:
            return 93;
        case tel::RFBand::NR5G_BAND_94:
            return 94;
        case tel::RFBand::NR5G_BAND_257:
            return 257;
        case tel::RFBand::NR5G_BAND_258:
            return 258;
        case tel::RFBand::NR5G_BAND_259:
            return 259;
        case tel::RFBand::NR5G_BAND_260:
            return 260;
        case tel::RFBand::NR5G_BAND_261:
            return 261;
        default:
            PA_DEBUG("Unknown NR5G active band.");
    }

    return 0;
}

taf_pa_radio_Bandwidth_t Utility::Convert::BandWidth
(
    tel::RFBandWidth bandwidth
)
{
    switch (bandwidth)
    {
        case tel::RFBandWidth::GSM_BW_NRB_2:
            return TAF_PA_RADIO_BANDWIDTH_GSM_BW_NRB_2;
        case tel::RFBandWidth::WCDMA_BW_NRB_5:
            return TAF_PA_RADIO_BANDWIDTH_WCDMA_BW_NRB_5;
        case tel::RFBandWidth::WCDMA_BW_NRB_10:
            return TAF_PA_RADIO_BANDWIDTH_WCDMA_BW_NRB_10;
        case tel::RFBandWidth::TDSCDMA_BW_NRB_2:
            return TAF_PA_RADIO_BANDWIDTH_TDSCDMA_BW_NRB_2;
        case tel::RFBandWidth::LTE_BW_NRB_6:
            return TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_6;
        case tel::RFBandWidth::LTE_BW_NRB_15:
            return TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_15;
        case tel::RFBandWidth::LTE_BW_NRB_25:
            return TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_25;
        case tel::RFBandWidth::LTE_BW_NRB_50:
            return TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_50;
        case tel::RFBandWidth::LTE_BW_NRB_75:
            return TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_75;
        case tel::RFBandWidth::LTE_BW_NRB_100:
            return TAF_PA_RADIO_BANDWIDTH_LTE_BW_NRB_100;
        case tel::RFBandWidth::NR5G_BW_NRB_5:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_5;
        case tel::RFBandWidth::NR5G_BW_NRB_10:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_10;
        case tel::RFBandWidth::NR5G_BW_NRB_15:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_15;
        case tel::RFBandWidth::NR5G_BW_NRB_20:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_20;
        case tel::RFBandWidth::NR5G_BW_NRB_25:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_25;
        case tel::RFBandWidth::NR5G_BW_NRB_30:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_30;
        case tel::RFBandWidth::NR5G_BW_NRB_40:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_40;
        case tel::RFBandWidth::NR5G_BW_NRB_50:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_50;
        case tel::RFBandWidth::NR5G_BW_NRB_60:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_60;
        case tel::RFBandWidth::NR5G_BW_NRB_70:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_70;
        case tel::RFBandWidth::NR5G_BW_NRB_80:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_80;
        case tel::RFBandWidth::NR5G_BW_NRB_90:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_90;
        case tel::RFBandWidth::NR5G_BW_NRB_100:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_100;
        case tel::RFBandWidth::NR5G_BW_NRB_200:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_200;
        case tel::RFBandWidth::NR5G_BW_NRB_400:
            return TAF_PA_RADIO_BANDWIDTH_NR5G_BW_NRB_400;

        default:
            PA_DEBUG("Unknown bandwidth.");
            return TAF_PA_RADIO_BANDWIDTH_UNKNOWN;
    }
}

pa_result_t Utility::Convert::RFBandInfo
(
    tel::RFBandInfo info,
    taf_pa_radio_ServingCellBandInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    if (infoPtr->bandInfoValid)
    {
        infoPtr->activeBand = Utility::Convert::ActiveBand(info.band);
        infoPtr->bandwidth = Utility::Convert::BandWidth(info.bandWidth);
    }

    if (infoPtr->lteBandInfoValid)
    {
        if (infoPtr->lteActiveBandPtr != nullptr)
            *(infoPtr->lteActiveBandPtr) = Utility::Convert::LteActiveBand(info.band);
        else
        {
            PA_ERROR("lteActiveBandPtr is nullptr.");
            return -EINVAL;
        }

        if (infoPtr->lteBandwidthPtr != nullptr)
            *(infoPtr->lteBandwidthPtr) = Utility::Convert::BandWidth(info.bandWidth);
        else
        {
            PA_ERROR("lteBandwidthPtr is nullptr.");
            return -EINVAL;
        }
    }

    if (infoPtr->nr5gBandInfoValid)
    {
        if (infoPtr->nr5gActiveBandPtr != nullptr)
            *(infoPtr->nr5gActiveBandPtr) = Utility::Convert::Nr5gActiveBand(info.band);
        else
        {
            PA_ERROR("nr5gActiveBandPtr is nullptr.");
            return -EINVAL;
        }

        if (infoPtr->nr5gBandwidthPtr != nullptr)
            *(infoPtr->nr5gBandwidthPtr) = Utility::Convert::BandWidth(info.bandWidth);
        else
        {
            PA_ERROR("nr5gBandwidthPtr is nullptr.");
            return -EINVAL;
        }
    }

    return 0;
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

pa_result_t Utility::Convert::OperatingMode
(
    taf_pa_radio_OperatingMode_t mode,
    tel::OperatingMode* modePtr
)
{
    switch (mode)
    {
        case TAF_PA_RADIO_OPERATING_MODE_ONLINE:
            *modePtr = tel::OperatingMode::ONLINE;
            break;
        case TAF_PA_RADIO_OPERATING_MODE_LOW_POWER:
            *modePtr = tel::OperatingMode::AIRPLANE;
            break;
        case TAF_PA_RADIO_OPERATING_MODE_FACTORY_TEST_MODE:
            *modePtr = tel::OperatingMode::FACTORY_TEST;
            break;
        case TAF_PA_RADIO_OPERATING_MODE_OFFLINE:
            *modePtr = tel::OperatingMode::OFFLINE;
            break;
        case TAF_PA_RADIO_OPERATING_MODE_RESETTING:
            *modePtr = tel::OperatingMode::RESETTING;
            break;
        case TAF_PA_RADIO_OPERATING_MODE_SHUTTING_DOWN:
            *modePtr = tel::OperatingMode::SHUTTING_DOWN;
            break;
        case TAF_PA_RADIO_OPERATING_MODE_PERSISTENT_LOW_POWER:
            *modePtr = tel::OperatingMode::PERSISTENT_LOW_POWER;
            break;
        default:
            PA_ERROR("Unknown operating mode %d.", mode);
            return -EINVAL;
    }

    return 0;
}

void Utility::Convert::CellInfoList
(
    vector<shared_ptr<tel::CellInfo>> infoPtrList,
    taf_pa_radio_CellLocationListInfo_t* listInfoPtr
)
{
    if (listInfoPtr == nullptr)
    {
        PA_ERROR("listInfoPtr is nullptr.");
        return;
    }

    uint32_t infoCount = 0;
    for (auto infoPtr : infoPtrList)
    {
        if (infoPtr != nullptr)
        {
            switch (infoPtr->getType())
            {
                case tel::CellType::GSM:
                {
                    auto cellInfoPtr = static_pointer_cast<tel::GsmCellInfo>(infoPtr);
                    if (cellInfoPtr != nullptr)
                    {
                        listInfoPtr->cellLocInfo[infoCount].rat = TAF_PA_RADIO_RAT_GSM;
                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.bsic =
                            cellInfoPtr->getCellIdentity().getBaseStationIdentityCode();
                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.cid =
                            cellInfoPtr->getCellIdentity().getIdentity();
                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.lac =
                            cellInfoPtr->getCellIdentity().getLac();
                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.arfcn =
                            cellInfoPtr->getCellIdentity().getArfcn();
                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.ta =
                            cellInfoPtr->getSignalStrengthInfo().getTimingAdvance();
                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.rssi =
                            cellInfoPtr->getSignalStrengthInfo().getDbm();

                        listInfoPtr->cellLocInfo[infoCount].gsmInfo.plmnIdValid = 0;
                        if (!cellInfoPtr->getCellIdentity().getMobileCountryCode().empty() &&
                            !cellInfoPtr->getCellIdentity().getMobileNetworkCode().empty())
                        {
                            pa_result_t result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileCountryCode(),
                                &listInfoPtr->cellLocInfo[infoCount].gsmInfo.plmnId.mcc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MCC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileCountryCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].gsmInfo.plmnIdValid = 0;
                            }
                            result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileNetworkCode(),
                                &listInfoPtr->cellLocInfo[infoCount].gsmInfo.plmnId.mnc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MNC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileNetworkCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].gsmInfo.plmnIdValid = 0;
                            }
                            listInfoPtr->cellLocInfo[infoCount].gsmInfo.plmnIdValid = 1;
                        }

                        if (cellInfoPtr->isRegistered())
                        {
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_SERVING;
                        }
                        else
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR;

                        infoCount++;
                    }
                    break;
                }
                case tel::CellType::CDMA:
                {
                    auto cellInfoPtr = static_pointer_cast<tel::CdmaCellInfo>(infoPtr);
                    if (cellInfoPtr != nullptr)
                    {
                        listInfoPtr->cellLocInfo[infoCount].rat = TAF_PA_RADIO_RAT_CDMA;
                        listInfoPtr->cellLocInfo[infoCount].cdmaInfo.sid =
                            cellInfoPtr->getCellIdentity().getSid();
                        listInfoPtr->cellLocInfo[infoCount].cdmaInfo.nid =
                            cellInfoPtr->getCellIdentity().getNid();
                        listInfoPtr->cellLocInfo[infoCount].cdmaInfo.bsid =
                            cellInfoPtr->getCellIdentity().getBaseStationId();
                        listInfoPtr->cellLocInfo[infoCount].cdmaInfo.bslat =
                            cellInfoPtr->getCellIdentity().getLatitude();
                        listInfoPtr->cellLocInfo[infoCount].cdmaInfo.bslong =
                            cellInfoPtr->getCellIdentity().getLongitude();
                        listInfoPtr->cellLocInfo[infoCount].cdmaInfo.ss =
                            cellInfoPtr->getSignalStrengthInfo().getDbm();

                        if (cellInfoPtr->isRegistered())
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_SERVING;
                        else
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR;

                        infoCount++;
                    }
                    break;
                }
                case tel::CellType::WCDMA:
                {
                    auto cellInfoPtr = static_pointer_cast<tel::WcdmaCellInfo>(infoPtr);
                    if (cellInfoPtr != nullptr)
                    {
                        listInfoPtr->cellLocInfo[infoCount].rat = TAF_PA_RADIO_RAT_UMTS;
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.cid =
                            cellInfoPtr->getCellIdentity().getIdentity();
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.lac =
                            cellInfoPtr->getCellIdentity().getLac();
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.uarfcn =
                            cellInfoPtr->getCellIdentity().getUarfcn();
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.psc =
                            cellInfoPtr->getCellIdentity().getPrimaryScramblingCode();
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.rscp =
                            cellInfoPtr->getSignalStrengthInfo().getRscp();
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.ecio =
                            cellInfoPtr->getSignalStrengthInfo().getEcio();
                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.ss =
                            cellInfoPtr->getSignalStrengthInfo().getDbm();

                        listInfoPtr->cellLocInfo[infoCount].umtsInfo.plmnIdValid = 0;
                        if (!cellInfoPtr->getCellIdentity().getMobileCountryCode().empty() &&
                            !cellInfoPtr->getCellIdentity().getMobileNetworkCode().empty())
                        {
                            pa_result_t result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileCountryCode(),
                                &listInfoPtr->cellLocInfo[infoCount].umtsInfo.plmnId.mcc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MCC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileCountryCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].umtsInfo.plmnIdValid = 0;
                            }
                            result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileNetworkCode(),
                                &listInfoPtr->cellLocInfo[infoCount].umtsInfo.plmnId.mnc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MNC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileNetworkCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].umtsInfo.plmnIdValid = 0;
                            }
                            listInfoPtr->cellLocInfo[infoCount].umtsInfo.plmnIdValid = 1;
                        }

                        if (cellInfoPtr->isRegistered())
                        {
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_SERVING;
                        }
                        else
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR;

                        infoCount++;
                    }
                    break;
                }
                case tel::CellType::TDSCDMA:
                {
                    auto cellInfoPtr = static_pointer_cast<tel::TdscdmaCellInfo>(infoPtr);
                    if (cellInfoPtr != nullptr)
                    {
                        listInfoPtr->cellLocInfo[infoCount].rat = TAF_PA_RADIO_RAT_TDSCDMA;
                        listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.lac =
                            cellInfoPtr->getCellIdentity().getLac();
                        listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.cid =
                            cellInfoPtr->getCellIdentity().getIdentity();
                        listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.cpid =
                            cellInfoPtr->getCellIdentity().getParametersId();
                        listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.rscp =
                            cellInfoPtr->getSignalStrengthInfo().getRscp();

                        listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.plmnIdValid = 0;
                        if (!cellInfoPtr->getCellIdentity().getMobileCountryCode().empty() &&
                            !cellInfoPtr->getCellIdentity().getMobileNetworkCode().empty())
                        {
                            pa_result_t result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileCountryCode(),
                                &listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.plmnId.mcc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MCC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileCountryCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.plmnIdValid = 0;
                            }
                            result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileNetworkCode(),
                                &listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.plmnId.mnc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MNC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileNetworkCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.plmnIdValid = 0;
                            }
                            listInfoPtr->cellLocInfo[infoCount].tdscdmaInfo.plmnIdValid = 1;
                        }

                        if (cellInfoPtr->isRegistered())
                        {
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_SERVING;
                        }
                        else
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR;

                        infoCount++;
                    }
                    break;
                }
                case tel::CellType::LTE:
                {
                    auto cellInfoPtr = static_pointer_cast<tel::LteCellInfo>(infoPtr);
                    if (cellInfoPtr != nullptr)
                    {
                        listInfoPtr->cellLocInfo[infoCount].rat = TAF_PA_RADIO_RAT_LTE;
                        listInfoPtr->cellLocInfo[infoCount].lteInfo.cid =
                            cellInfoPtr->getCellIdentity().getIdentity();
                        listInfoPtr->cellLocInfo[infoCount].lteInfo.pcid =
                            cellInfoPtr->getCellIdentity().getPhysicalCellId();
                        listInfoPtr->cellLocInfo[infoCount].lteInfo.tac =
                            cellInfoPtr->getCellIdentity().getTrackingAreaCode();
                        listInfoPtr->cellLocInfo[infoCount].lteInfo.earfcn =
                            cellInfoPtr->getCellIdentity().getEarfcn();
                        listInfoPtr->cellLocInfo[infoCount].lteInfo.ta =
                            cellInfoPtr->getSignalStrengthInfo().getTimingAdvance();
                        listInfoPtr->cellLocInfo[infoCount].lteInfo.rssi =
                            cellInfoPtr->getSignalStrengthInfo().getDbm();

                        listInfoPtr->cellLocInfo[infoCount].lteInfo.plmnIdValid = 0;
                        if (!cellInfoPtr->getCellIdentity().getMobileCountryCode().empty() &&
                            !cellInfoPtr->getCellIdentity().getMobileNetworkCode().empty())
                        {
                            pa_result_t result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileCountryCode(),
                                &listInfoPtr->cellLocInfo[infoCount].lteInfo.plmnId.mcc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MCC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileCountryCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].lteInfo.plmnIdValid = 0;
                            }
                            result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileNetworkCode(),
                                &listInfoPtr->cellLocInfo[infoCount].lteInfo.plmnId.mnc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MNC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileNetworkCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].lteInfo.plmnIdValid = 0;
                            }
                            listInfoPtr->cellLocInfo[infoCount].lteInfo.plmnIdValid = 1;
                        }

                        if (cellInfoPtr->isRegistered())
                        {
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_SERVING;
                        }
                        else
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR;

                        infoCount++;
                    }
                    break;
                }
                case tel::CellType::NR5G:
                {
                    auto cellInfoPtr = static_pointer_cast<tel::Nr5gCellInfo>(infoPtr);
                    if (cellInfoPtr != nullptr)
                    {
                        listInfoPtr->cellLocInfo[infoCount].rat = TAF_PA_RADIO_RAT_NR5G;
                        listInfoPtr->cellLocInfo[infoCount].nr5gInfo.cid =
                            cellInfoPtr->getCellIdentity().getIdentity();
                        listInfoPtr->cellLocInfo[infoCount].nr5gInfo.pcid =
                            cellInfoPtr->getCellIdentity().getPhysicalCellId();
                        listInfoPtr->cellLocInfo[infoCount].nr5gInfo.tac =
                            cellInfoPtr->getCellIdentity().getTrackingAreaCode();
                        listInfoPtr->cellLocInfo[infoCount].nr5gInfo.arfcn =
                            cellInfoPtr->getCellIdentity().getArfcn();

                        listInfoPtr->cellLocInfo[infoCount].nr5gInfo.plmnIdValid = 0;
                        if (!cellInfoPtr->getCellIdentity().getMobileCountryCode().empty() &&
                            !cellInfoPtr->getCellIdentity().getMobileNetworkCode().empty())
                        {
                            pa_result_t result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileCountryCode(),
                                &listInfoPtr->cellLocInfo[infoCount].nr5gInfo.plmnId.mcc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MCC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileCountryCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].nr5gInfo.plmnIdValid = 0;
                            }
                            result = Utility::Convert::StringToU16(
                                cellInfoPtr->getCellIdentity().getMobileNetworkCode(),
                                &listInfoPtr->cellLocInfo[infoCount].nr5gInfo.plmnId.mnc);
                            if (result != 0)
                            {
                                PA_ERROR("Failed to convert MNC %s.",
                                    cellInfoPtr->getCellIdentity().getMobileNetworkCode().c_str());
                                listInfoPtr->cellLocInfo[infoCount].nr5gInfo.plmnIdValid = 0;
                            }
                            listInfoPtr->cellLocInfo[infoCount].nr5gInfo.plmnIdValid = 1;
                        }

                        if (cellInfoPtr->isRegistered())
                        {
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_SERVING;
                        }
                        else
                            listInfoPtr->cellLocInfo[infoCount].location =
                                TAF_PA_RADIO_CELL_LOCATION_NEIGHBOR;

                        infoCount++;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    listInfoPtr->cellLocInfoCount = infoCount;
}

taf_pa_radio_NrIcon_t Utility::Convert::NrIcon
(
    data::NrIconType type
)
{
    switch (type)
    {
        case data::NrIconType::BASIC:
            return TAF_PA_RADIO_NR_ICON_BASIC;
        case data::NrIconType::UWB:
            return TAF_PA_RADIO_NR_ICON_UWB;
        default:
            break;
    }

    return TAF_PA_RADIO_NR_ICON_NONE;
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

void Utility::WaitCallback::Scan
(
    uint32_t instance,
    uint32_t timeout
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout;

    int result = sem_timedwait(&pa.listeners.networkSelections[instance]->semaphore, &ts);
    if (result != 0)
    {
        if (errno == ETIMEDOUT)
        {
            PA_ERROR("Timeout to request.");
            pa.listeners.networkSelections[instance]->result = -ETIMEDOUT;
        }
        else
        {
            PA_ERROR("Wait on semaphore with error code: %d.", result);
            pa.listeners.networkSelections[instance]->result = -EFAULT;
        }
    }
}

void RequestCallback::CommonResponse
(
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::operatingModeResponse
(
    tel::OperatingMode mode,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        operatingMode = mode;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::NetworkModeInfoResponse
(
    tel::NetworkModeInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        networkModeInfo = info;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::PreferredNetworksResponse
(
    vector<tel::PreferredNetworkInfo> nonStaticInfo,
    vector<tel::PreferredNetworkInfo> staticInfo,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        preferredNetworksInfo.clear();
        preferredNetworksInfo.assign(nonStaticInfo.begin(), nonStaticInfo.end());
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::RatPreferenceResponse
(
    tel::RatPreference preference,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        ratPreference = preference;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ServiceDomainPreferenceResponse
(
    tel::ServiceDomainPreference preference,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        serviceDomainPreference = preference;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::voiceServiceStateResponse
(
    const shared_ptr<tel::VoiceServiceInfo>& infoPtr,
    common::ErrorCode error
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        result = -EFAULT;
    }
    else if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        voiceServiceState = infoPtr->getVoiceServiceState();
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::signalStrengthResponse
(
    shared_ptr<tel::SignalStrength> strengthPtr,
    common::ErrorCode error
)
{
    if (strengthPtr == nullptr)
    {
        PA_ERROR("strengthPtr is nullptr.");
        result = -EFAULT;
    }
    else if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        signalStrengthPtr = strengthPtr;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::DataServiceStatusResponse
(
    data::ServiceStatus status,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        dataServiceState = status.serviceState;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::CellInfoListResponse
(
    vector<shared_ptr<tel::CellInfo>> infoPtrList,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        cellInfoPtrList.clear();
        cellInfoPtrList.assign(infoPtrList.begin(), infoPtrList.end());

        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::OperatorInfoResponse
(
    tel::PlmnInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        operatorInfo = info;
        result = 0;
    }
    sem_post(&semaphore);
}

void RequestCallback::RfBandCapabilityResponse
(
    shared_ptr<tel::IRFBandList> listPtr,
    common::ErrorCode error
)
{
    if (listPtr == nullptr)
    {
        PA_ERROR("listPtr is nullptr.");
        result = -EFAULT;
    }
    else if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        rfBandCapabilityPtr = listPtr;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::RfBandPreferenceResponse
(
    shared_ptr<tel::IRFBandList> listPtr,
    common::ErrorCode error
)
{
    if (listPtr == nullptr)
    {
        PA_ERROR("listPtr is nullptr.");
        result = -EFAULT;
    }
    else if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        rfBandPreferencePtr = listPtr;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ImsRegistrationInfoResponse
(
    tel::ImsRegistrationInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        imsRegistrationInfo = info;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ImsServiceInfoResponse
(
    tel::ImsServiceInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        imsServiceInfo = info;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ImsPdpStatusResponse
(
    tel::ImsPdpStatusInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        imsPdpStatusInfo = info;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ImsVonrStatusResponse
(
    SlotId id,
    bool enable,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        isVoNREnabled = enable;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ImsServiceConfigResponse
(
    SlotId id,
    tel::ImsServiceConfig config,
    common::ErrorCode error
)
{
    imsServiceConfigError = error;
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        imsServiceConfig = config;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::ImsSigUserAgentResponse
(
    SlotId id,
    string str,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        imsSipUserAgent = str;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::cellularCapabilityResponse
(
    tel::CellularCapabilityInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        cellularCapabilityInfo = info;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::RFBandInfoResponse
(
    tel::RFBandInfo info,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        rfBandInfo = info;
        result = 0;
    }

    sem_post(&semaphore);
}

void RequestCallback::NrIconTypeResponse
(
    data::NrIconType type,
    common::ErrorCode error
)
{
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Error: %s.", common::Utils::getErrorCodeAsString(error).c_str());
        result = -EFAULT;
    }
    else
    {
        nrIconType = type;
        result = 0;
    }

    sem_post(&semaphore);
}

void Listener::TelephonyServingSystemListener::onNetworkRejection
(
    tel:: NetworkRejectInfo info
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_NetworkRejectHdlrFunc_t handlerFunc =
        (taf_pa_radio_NetworkRejectHdlrFunc_t)pa.indicators.networkReject.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_NetworkRejectIndication_t indication;
        indication.rat = Utility::Convert::Rat(info.rejectSrvInfo.rat);
        indication.domain = Utility::Convert::ServiceDomain(info.rejectSrvInfo.domain);
        indication.cause = info.rejectCause;
        PA_DEBUG("Network reject cause %d.", info.rejectCause);
        indication.plmnIdValid = 1;
        pa_result_t result = Utility::Convert::StringToU16(info.mcc, &indication.plmnId.mcc);
        if (result != 0)
        {
            PA_ERROR("Failed to convert MCC %s.",info.mcc.c_str());
            indication.plmnIdValid = 0;
        }
        result = Utility::Convert::StringToU16(info.mnc, &indication.plmnId.mnc);
        if (result != 0)
        {
            PA_ERROR("Failed to convert MNC %s.",info.mnc.c_str());
            indication.plmnIdValid = 0;
        }

        if (indication.plmnIdValid)
            handlerFunc(instance, indication, pa.indicators.networkReject.contextPtr);
    }
}

void Listener::TelephonyServingSystemListener::onSystemInfoChanged
(
    tel::ServingSystemInfo info
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_radio_RatChangeHdlrFunc_t handlerFunc1 =
        (taf_pa_radio_RatChangeHdlrFunc_t)pa.indicators.ratChange.handlerFuncPtr;
    if (handlerFunc1 != nullptr)
    {
        taf_pa_radio_RatChangeIndication_t indication;
        indication.rat = Utility::Convert::Rat(info.rat);
        handlerFunc1(instance, indication, pa.indicators.ratChange.contextPtr);
    }

    taf_pa_radio_ServiceDomainHdlrFunc_t handlerFunc2 =
        (taf_pa_radio_ServiceDomainHdlrFunc_t)pa.indicators.serviceDomain.handlerFuncPtr;
    if (handlerFunc2 != nullptr)
    {
        taf_pa_radio_ServiceDomainIndication_t indication;
        indication.domain = Utility::Convert::ServiceDomain(info.domain);
        handlerFunc2(instance, indication, pa.indicators.serviceDomain.contextPtr);
    }
}

void Listener::TelephonyServingSystemListener::onLteCsCapabilityChanged
(
    tel::LteCsCapability capability
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    taf_pa_radio_LteCsCapabilityHdlrFunc_t handlerFunc =
        (taf_pa_radio_LteCsCapabilityHdlrFunc_t)pa.indicators.lteCsCapability.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_LteCsCapabilityIndication_t indication;
        indication.capability = Utility::Convert::LteCsCapability(capability);
        handlerFunc(instance, indication, pa.indicators.lteCsCapability.contextPtr);
    }
}

void Listener::NetworkSelectionListener::onNetworkScanResults
(
    tel::NetworkScanStatus status,
    vector<tel::OperatorInfo> infoList
)
{
    if (status == tel::NetworkScanStatus::FAILED)
    {
        PA_ERROR("Network scan failed.");
        result = -EFAULT;
        sem_post(&semaphore);
    }
    else
    {
        for (auto info : infoList)
            operatorInfoList.emplace_back(info);

        if (status == tel::NetworkScanStatus::COMPLETE)
        {
            PA_INFO("Network scan completed.");
            result = 0;
            sem_post(&semaphore);
        }
    }
}

void Listener::PhoneListener::onVoiceServiceStateChanged
(
    int phone,
    const shared_ptr<tel::VoiceServiceInfo>& infoPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_VoiceServiceInfoHdlrFunc_t handlerFunc =
        (taf_pa_radio_VoiceServiceInfoHdlrFunc_t)pa.indicators.voiceServiceInfo.handlerFuncPtr;
    if (handlerFunc != nullptr && infoPtr != nullptr)
    {
        taf_pa_radio_VoiceServiceInfoIndication_t indication;
        uint32_t instance = Utility::Convert::PhoneToInstance(phone);
        Utility::Convert::VoiceServiceInfo(infoPtr->getVoiceServiceState(), &indication.info);
        handlerFunc(instance, indication, pa.indicators.voiceServiceInfo.contextPtr);
    }
}

void Listener::PhoneListener::onSignalStrengthChanged
(
    int phone,
    shared_ptr<tel::SignalStrength> strengthPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_SignalStrengthInfoChangeHdlrFunc_t handlerFunc =
        (taf_pa_radio_SignalStrengthInfoChangeHdlrFunc_t)pa.indicators.signalStrengthInfoChange.handlerFuncPtr;
    if (handlerFunc != nullptr && strengthPtr != nullptr)
    {
        taf_pa_radio_SignalStrengthInfoChangeIndication_t indication;
        uint32_t instance = Utility::Convert::PhoneToInstance(phone);
        Utility::Convert::SignalStrengthInfo(strengthPtr, &indication.info);
        handlerFunc(instance, indication, pa.indicators.signalStrengthInfoChange.contextPtr);
    }
}

void Listener::PhoneListener::onOperatingModeChanged
(
    tel::OperatingMode mode
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_OperatingModeChangeHdlrFunc_t handlerFunc =
        (taf_pa_radio_OperatingModeChangeHdlrFunc_t)pa.indicators.operatingModeChange.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_OperatingModeChangeIndication_t indication;
        indication.mode = Utility::Convert::OperatingMode(mode);
        handlerFunc(0, indication, pa.indicators.operatingModeChange.contextPtr);
    }
}

void Listener::PhoneListener::onCellInfoListChanged
(
    int phone,
    vector<shared_ptr<tel::CellInfo>> cellInfoList
)
{
    taf_pa_radio_CellRoleBitMask_t bitmask = 0x0;

    for (auto cellInfo : cellInfoList)
    {
        if (cellInfo != nullptr)
        {
            if (cellInfo->isRegistered())
                bitmask |= TAF_PA_RADIO_BITMASK_CELL_ROLE_SERVING;
            else
                bitmask |= TAF_PA_RADIO_BITMASK_CELL_ROLE_NEIGHBOR;
        }
    }

    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_CellInfoChangeHdlrFunc_t handlerFunc =
        (taf_pa_radio_CellInfoChangeHdlrFunc_t)pa.indicators.cellInfoChange.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_CellInfoChangeIndication_t indication;
        indication.cellRoleValid = 1;
        indication.cellRole = bitmask;
        handlerFunc(0, indication, pa.indicators.cellInfoChange.contextPtr);
    }
}

void Listener::DataServingSystemListener::onServiceStateChanged
(
    data::ServiceStatus status
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_DataServiceStatusHdlrFunc_t handlerFunc =
        (taf_pa_radio_DataServiceStatusHdlrFunc_t)pa.indicators.dataServiceStatus.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_DataServiceStatusIndication_t indication;
        indication.state = Utility::Convert::ServiceState(status.serviceState);
        handlerFunc(instance, indication, pa.indicators.dataServiceStatus.contextPtr);
    }
}

void Listener::DataServingSystemListener::onRoamingStatusChanged
(
    data::RoamingStatus status
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_DataRoamingStatusHdlrFunc_t handlerFunc =
        (taf_pa_radio_DataRoamingStatusHdlrFunc_t)pa.indicators.dataRoamingStatus.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_DataRoamingStatusIndication_t indication;
        if (status.isRoaming)
            indication.status = TAF_PA_RADIO_DATA_ROAMING_STATUS_ON;
        else
            indication.status = TAF_PA_RADIO_DATA_ROAMING_STATUS_OFF;
        handlerFunc(instance, indication, pa.indicators.dataRoamingStatus.contextPtr);
    }
}

void Listener::DataServingSystemListener::onNrIconTypeChanged
(
    telux::data::NrIconType type
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_NrIconChangeHdlrFunc_t handlerFunc =
        (taf_pa_radio_NrIconChangeHdlrFunc_t)pa.indicators.nrIconChange.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_NrIconChangeIndication_t indication;
        indication.icon = Utility::Convert::NrIcon(type);
        handlerFunc(instance, indication, pa.indicators.nrIconChange.contextPtr);
    }
}

void Listener::ImsServingSystemListener::onImsRegStatusChange
(
    tel::ImsRegistrationInfo info
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_ImsRegStatusChangeHdlrFunc_t handlerFunc =
        (taf_pa_radio_ImsRegStatusChangeHdlrFunc_t)pa.indicators.imsRegStatusChange.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_ImsRegStatusChangeIndication_t indication;
        indication.status = Utility::Convert::ImsRegistrationStatus(info.imsRegStatus);
        handlerFunc(instance, indication, pa.indicators.imsRegStatusChange.contextPtr);
    }
}

void Listener::ImsServingSystemListener::onImsServiceInfoChange
(
    tel::ImsServiceInfo info
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_ImsServiceInfoHdlrFunc_t handlerFunc =
        (taf_pa_radio_ImsServiceInfoHdlrFunc_t)pa.indicators.imsServiceInfo.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_ImsServiceInfoIndication_t indication;
        indication.voipServiceStatusValid = 1;
        indication.voipServiceStatus = Utility::Convert::ImsServiceStatus(info.voice);
        indication.smsServiceStatusValid = 1;
        indication.smsServiceStatus = Utility::Convert::ImsServiceStatus(info.sms);
        handlerFunc(instance, indication, pa.indicators.imsServiceInfo.contextPtr);
    }
}

void Listener::ImsServingSystemListener::onImsPdpStatusInfoChange
(
    tel::ImsPdpStatusInfo info
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    taf_pa_radio_ImsPdpErrorHdlrFunc_t handlerFunc =
        (taf_pa_radio_ImsPdpErrorHdlrFunc_t)pa.indicators.imsPdpError.handlerFuncPtr;
    if (handlerFunc != nullptr)
    {
        taf_pa_radio_ImsPdpErrorIndication_t indication;
        indication.failureErrorCodeValid = 1;
        indication.failureErrorCode = Utility::Convert::ImsPdpFailureErrorCode(info.failureCode);
        handlerFunc(instance, indication, pa.indicators.imsPdpError.contextPtr);
    }
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
    auto& dataFactory = data::DataFactory::getInstance();
    uint32_t instances = 0;

    if (common::DeviceConfig::isMultiSimSupported())
    {
        instances = 2;
        PA_INFO("MultiSim supported.");
    }
    else
    {
        instances = 1;
        PA_INFO("MultiSim not supported.");
    }

    pa.callbacks.request = make_shared<RequestCallback>();

    SERVICE_PROMISE_AND_CALLBACK(phone)
    pa.managers.phone = phoneFactory.getPhoneManager(phoneCallback);
    SERVICE_READY(phone,pa.managers.phone)
    pa.listeners.phone = make_shared<Listener::PhoneListener>();

    SERVICE_PROMISE_AND_CALLBACK(imsSetting)
    pa.managers.imsSetting = phoneFactory.getImsSettingsManager(imsSettingCallback);
    SERVICE_READY(imsSetting,pa.managers.imsSetting)

    for (uint32_t i = 0; i < instances; i++)
    {
        int slot = Utility::Convert::InstanceToSlot(i);
        SlotId slotId = Utility::Convert::SlotToSlotId(slot);

        SERVICE_PROMISE_AND_CALLBACK(networkSelection)
        pa.managers.networkSelections[i] = phoneFactory.getNetworkSelectionManager(slot,
            networkSelectionCallback);
        SERVICE_READY(networkSelection,pa.managers.networkSelections[i])
        pa.listeners.networkSelections[i] = make_shared<Listener::NetworkSelectionListener>(i);

        SERVICE_PROMISE_AND_CALLBACK(telephonyServingSystem)
        pa.managers.telephonyServingSystems[i] = phoneFactory.getServingSystemManager(slot,
            telephonyServingSystemCallback);
        SERVICE_READY(telephonyServingSystem, pa.managers.imsServingSystems[i])
        pa.listeners.telephonyServingSystems[i] =
            make_shared<Listener::TelephonyServingSystemListener>(i);

        SERVICE_PROMISE_AND_CALLBACK(imsServingSystem)
        pa.managers.imsServingSystems[i] = phoneFactory.getImsServingSystemManager(slotId,
            imsServingSystemCallback);
        SERVICE_READY(imsServingSystem,pa.managers.imsServingSystems[i])
        pa.listeners.imsServingSystems[i] = make_shared<Listener::ImsServingSystemListener>(i);

        SERVICE_PROMISE_AND_CALLBACK(dataServingSystem)
         pa.managers.dataServingSystems[i] = dataFactory.getServingSystemManager(slotId,
            dataServingSystemCallback);
        SERVICE_READY(dataServingSystem,pa.managers.dataServingSystems[i])
        pa.listeners.dataServingSystems[i] = make_shared<Listener::DataServingSystemListener>(i);
    }

    int32_t result = taf_prop_radio_Init();
    if (result == -ENOSYS)
        PA_INFO("Radio private platform adaptor is not implemented.");
    else if (result == 0)
    {
        for (uint32_t i = 0; i < instances; i++)
        {
            result = taf_prop_radio_InitInstance(i);
            if (result != 0)
                PA_ERROR("Failed to initializate private instance %d, result = %d.", i, result);
            else
                PA_INFO("Radio private instance %d initialization is done.", i);
        }

        taf_prop_radio_AddRatSvcStatusHandler(0, RatSvcStatusHandler, nullptr);
        taf_prop_radio_AddLteCphyCaHandler(0, LteCphyCaHandler, nullptr);
        taf_prop_radio_AddDataAvailSysStatusHandler(0, DataAvailSysStatusHandler, nullptr);

        PA_INFO("Radio private platform adaptor initialization is done.");
    }

    PA_INFO("Radio platform adaptor initialization is done.");

    return 0;
}

pa_result_t taf_pa_radio_Deinit()
{
    PA_INFO("Starting radio platform adaptor deinitialization...");

    auto& pa = PlatformAdaptor::GetInstance();

    // Step 1: Clear all indicator handler function pointers and context pointers
    // so no further indication callbacks are dispatched after this point.
    PA_INFO("Clearing all indicator handlers and contexts");
    pa.indicators.networkReject.handlerFuncPtr      = nullptr;
    pa.indicators.networkReject.contextPtr          = nullptr;
    pa.indicators.ratChange.handlerFuncPtr          = nullptr;
    pa.indicators.ratChange.contextPtr              = nullptr;
    pa.indicators.voiceServiceInfo.handlerFuncPtr   = nullptr;
    pa.indicators.voiceServiceInfo.contextPtr       = nullptr;
    pa.indicators.dataServiceStatus.handlerFuncPtr  = nullptr;
    pa.indicators.dataServiceStatus.contextPtr      = nullptr;
    pa.indicators.dataRoamingStatus.handlerFuncPtr  = nullptr;
    pa.indicators.dataRoamingStatus.contextPtr      = nullptr;
    pa.indicators.signalStrengthInfoChange.handlerFuncPtr = nullptr;
    pa.indicators.signalStrengthInfoChange.contextPtr     = nullptr;
    pa.indicators.ratSvcStatus.handlerFuncPtr       = nullptr;
    pa.indicators.ratSvcStatus.contextPtr           = nullptr;
    pa.indicators.lteCphyCa.handlerFuncPtr          = nullptr;
    pa.indicators.lteCphyCa.contextPtr              = nullptr;
    pa.indicators.dataAvailSysStatus.handlerFuncPtr = nullptr;
    pa.indicators.dataAvailSysStatus.contextPtr     = nullptr;
    pa.indicators.imsRegStatusChange.handlerFuncPtr = nullptr;
    pa.indicators.imsRegStatusChange.contextPtr     = nullptr;
    pa.indicators.operatingModeChange.handlerFuncPtr = nullptr;
    pa.indicators.operatingModeChange.contextPtr    = nullptr;
    pa.indicators.serviceDomain.handlerFuncPtr      = nullptr;
    pa.indicators.serviceDomain.contextPtr          = nullptr;
    pa.indicators.lteCsCapability.handlerFuncPtr    = nullptr;
    pa.indicators.lteCsCapability.contextPtr        = nullptr;
    pa.indicators.imsServiceInfo.handlerFuncPtr     = nullptr;
    pa.indicators.imsServiceInfo.contextPtr         = nullptr;
    pa.indicators.imsPdpError.handlerFuncPtr        = nullptr;
    pa.indicators.imsPdpError.contextPtr            = nullptr;
    pa.indicators.cellInfoChange.handlerFuncPtr     = nullptr;
    pa.indicators.cellInfoChange.contextPtr         = nullptr;
    pa.indicators.nrIconChange.handlerFuncPtr       = nullptr;
    pa.indicators.nrIconChange.contextPtr           = nullptr;

    // Step 2: Deregister per-instance listeners from their SDK managers so the
    // SDK stops delivering events to them.
    PA_INFO("Deregistering per-instance listeners");
    for (uint32_t i = 0; i < MAX_INSTANCE; i++)
    {
        if (pa.managers.telephonyServingSystems[i] != nullptr &&
            pa.listeners.telephonyServingSystems[i] != nullptr &&
            pa.managers.telephonyServingSystems[i]->getServiceStatus() ==
            common::ServiceStatus::SERVICE_AVAILABLE)
        {
            pa.managers.telephonyServingSystems[i]->deregisterListener(
                pa.listeners.telephonyServingSystems[i]);
        }
        else
        {
            PA_WARN("Skipping telephonyServingSystem[%d] deregister - manager not available", i);
        }

        if (pa.managers.imsServingSystems[i] != nullptr &&
            pa.listeners.imsServingSystems[i] != nullptr &&
            pa.managers.imsServingSystems[i]->getServiceStatus() ==
            common::ServiceStatus::SERVICE_AVAILABLE)
        {
            pa.managers.imsServingSystems[i]->deregisterListener(
                pa.listeners.imsServingSystems[i]);
        }
        else
        {
            PA_WARN("Skipping imsServingSystem[%d] deregister - manager not available", i);
        }

        if (pa.managers.dataServingSystems[i] != nullptr &&
            pa.listeners.dataServingSystems[i] != nullptr &&
            pa.managers.dataServingSystems[i]->getServiceStatus() ==
            common::ServiceStatus::SERVICE_AVAILABLE)
        {
            pa.managers.dataServingSystems[i]->deregisterListener(
                pa.listeners.dataServingSystems[i]);
        }
        else
        {
            PA_WARN("Skipping dataServingSystem[%d] deregister - manager not available", i);
        }

        if (pa.managers.networkSelections[i] != nullptr &&
            pa.listeners.networkSelections[i] != nullptr &&
            pa.managers.networkSelections[i]->getServiceStatus() ==
            common::ServiceStatus::SERVICE_AVAILABLE)
        {
            pa.managers.networkSelections[i]->deregisterListener(
                pa.listeners.networkSelections[i]);
        }
        else
        {
            PA_WARN("Skipping networkSelection[%d] deregister - manager not available", i);
        }
    }

    // Deregister the phone listener (shared across all instances).
    if (pa.managers.phone != nullptr &&
        pa.listeners.phone != nullptr &&
        pa.managers.phone->getServiceStatus() ==
        common::ServiceStatus::SERVICE_AVAILABLE)
    {
        pa.managers.phone->removeListener(pa.listeners.phone);
    }
    else
    {
        PA_WARN("Skipping phone listener deregister - manager not available");
    }

    // Step 3: Reset all listener shared pointers so the listener objects are
    // released once no other owners remain.
    PA_INFO("Resetting listener shared pointers");
    pa.listeners.phone.reset();
    for (uint32_t i = 0; i < MAX_INSTANCE; i++)
    {
        pa.listeners.telephonyServingSystems[i].reset();
        pa.listeners.networkSelections[i].reset();
        pa.listeners.imsServingSystems[i].reset();
        pa.listeners.dataServingSystems[i].reset();
    }

    // Step 4: Reset all manager shared pointers so the underlying SDK objects
    // are released once no other owners remain.
    PA_INFO("Resetting manager shared pointers");
    pa.managers.phone.reset();
    pa.managers.imsSetting.reset();
    for (uint32_t i = 0; i < MAX_INSTANCE; i++)
    {
        pa.managers.networkSelections[i].reset();
        pa.managers.telephonyServingSystems[i].reset();
        pa.managers.imsServingSystems[i].reset();
        pa.managers.dataServingSystems[i].reset();
    }

    // Step 5: Reset the request callback object.
    PA_INFO("Resetting request callback");
    pa.callbacks.request.reset();

    PA_INFO("Radio platform adaptor deinitialization complete.");
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
    if (request->result != 0)
        return request->result;

    *modePtr = Utility::Convert::OperatingMode(pa.callbacks.request->operatingMode);

    return 0;
}

pa_result_t taf_pa_radio_SetOperatingMode
(
    uint32_t instance,
    taf_pa_radio_OperatingMode_t mode
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto& request = pa.callbacks.request;

    tel::OperatingMode operatingMode = tel::OperatingMode::ONLINE;
    pa_result_t paResult = Utility::Convert::OperatingMode(mode, &operatingMode);
    if (paResult != 0)
    {
        PA_ERROR("Failed to convert operating mode.");
        return paResult;
    }

    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.phone->setOperatingMode(operatingMode, callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set operating mode with phone manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_SetNetworkSelectionPreference
(
    uint32_t instance,
    taf_pa_radio_NetworkSelectionPreference_t* preferencePtr
)
{
    if (preferencePtr == nullptr)
    {
        PA_ERROR("preferencePtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.networkSelections[instance] == nullptr)
    {
        PA_ERROR("Network selection manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::NetworkSelectionMode mode = tel::NetworkSelectionMode::UNKNOWN;
    string mcc, mnc;
    switch (preferencePtr->mode)
    {
        case TAF_PA_RADIO_NETWORK_SELECTION_MODE_MANUAL:
            mcc = to_string(preferencePtr->mcc);
            mnc = to_string(preferencePtr->mnc);
            mode = tel::NetworkSelectionMode::MANUAL;
            break;
        case TAF_PA_RADIO_NETWORK_SELECTION_MODE_AUTOMATIC:
            mcc = to_string(0);
            mnc = to_string(0);
            mode = tel::NetworkSelectionMode::AUTOMATIC;
            break;
        default:
            PA_ERROR("Invalid network selection mode %d.", preferencePtr->mode);
            return -EINVAL;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.networkSelections[instance]->setNetworkSelectionMode(mode, mcc, mnc,
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set network selection preference with network selection manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetNetworkSelectionPreference
(
    uint32_t instance,
    taf_pa_radio_NetworkSelectionPreference_t* preferencePtr
)
{
    if (preferencePtr == nullptr)
    {
        PA_ERROR("preferencePtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.networkSelections[instance] == nullptr)
    {
        PA_ERROR("Network selection manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::NetworkModeInfoResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.networkSelections[instance]->requestNetworkSelectionMode(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get network selection preference with network selection manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    pa_result_t paResult = 0;
    switch (request->networkModeInfo.mode)
    {
        case tel::NetworkSelectionMode::MANUAL:
            preferencePtr->mode = TAF_PA_RADIO_NETWORK_SELECTION_MODE_MANUAL;
            paResult = Utility::Convert::StringToU16(request->networkModeInfo.mcc,
                &preferencePtr->mcc);
            if (paResult != 0)
            {
                PA_ERROR("Failed to convert MCC %s.", request->networkModeInfo.mcc.c_str());
                return paResult;
            }
            paResult = Utility::Convert::StringToU16(request->networkModeInfo.mnc,
                &preferencePtr->mnc);
            if (paResult != 0)
            {
                PA_ERROR("Failed to convert MNC %s.", request->networkModeInfo.mnc.c_str());
                return paResult;
            }
            break;
        case tel::NetworkSelectionMode::AUTOMATIC:
            preferencePtr->mode = TAF_PA_RADIO_NETWORK_SELECTION_MODE_AUTOMATIC;
            preferencePtr->mcc = 0;
            preferencePtr->mnc = 0;
            break;
        default:
            PA_ERROR("Unknown mode %d.", static_cast<int>(request->networkModeInfo.mode));
            return -EINVAL;
    }

    return request->result;
}

pa_result_t taf_pa_radio_SetPreferredNetwork
(
    uint32_t instance,
    taf_pa_radio_PreferredNetworkConfig_t* configPtr
)
{
    if (configPtr == nullptr)
    {
        PA_ERROR("configPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.networkSelections[instance] == nullptr)
    {
        PA_ERROR("Network selection manager %d is nullptr.", instance);
        return -EFAULT;
    }

    vector<tel::PreferredNetworkInfo> networks;
    for (uint32_t i = 0; i < configPtr->networkCount && i < TAF_PA_RADIO_PREFERRED_NETWORK_MAX_COUNT;
        i++)
    {
        tel::PreferredNetworkInfo info;
        info.mcc = configPtr->networks[i].mcc;
        info.mnc = configPtr->networks[i].mnc;
        info.ratMask = Utility::Convert::RatToTelRat(configPtr->networks[i].bitmask);

        networks.push_back(info);
    }

    auto& request = pa.callbacks.request;
    common::Status result = common::Status::SUCCESS;
    if (configPtr->clearPrevious == 0)
    {
        auto callback1 = bind(&RequestCallback::PreferredNetworksResponse, request,
            placeholders::_1, placeholders::_2, placeholders::_3);
        result = pa.managers.networkSelections[instance]->requestPreferredNetworks(callback1);
        if (result != common::Status::SUCCESS)
        {
            PA_ERROR("Failed to get preferred networks with network selection manager %d.", instance);
            return -EFAULT;
        }

        Utility::WaitCallback::Request();

        if (request->result != 0)
            return request->result;

        for (auto info : request->preferredNetworksInfo)
            networks.push_back(info);
    }

    auto callback2 = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    result = pa.managers.networkSelections[instance]->setPreferredNetworks(networks, true,
        callback2);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set preferred networks with network selection manager %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetPreferredNetwork
(
    uint32_t instance,
    taf_pa_radio_PreferredNetworks_t* networksPtr
)
{
    if (networksPtr == nullptr)
    {
        PA_ERROR("networksPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.networkSelections[instance] == nullptr)
    {
        PA_ERROR("Network selection manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::PreferredNetworksResponse, request, placeholders::_1,
        placeholders::_2, placeholders::_3);
    auto result = pa.managers.networkSelections[instance]->requestPreferredNetworks(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get preferred networks with network selection manager %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    uint32_t i;
    for (i = 0; i < request->preferredNetworksInfo.size() &&
       i < TAF_PA_RADIO_PREFERRED_NETWORK_MAX_COUNT; i++)
    {
        networksPtr->nonStaticNetworks[i].mcc = request->preferredNetworksInfo[i].mcc;
        networksPtr->nonStaticNetworks[i].mnc = request->preferredNetworksInfo[i].mnc;
        networksPtr->nonStaticNetworks[i].bitmask = Utility::Convert::Rat(
            request->preferredNetworksInfo[i].ratMask);
    }

    networksPtr->nonStaticNetworksValid = 1;
    networksPtr->nonStaticNetworkCount = i;

    return request->result;
}

PA_SHARED PA_WEAK pa_result_t taf_pa_radio_SetPreferredRat
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t bitmask
)
{
    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::RatPreference rat = Utility::Convert::RatToTelRatPreference(bitmask);
    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.telephonyServingSystems[instance]->setRatPreference(rat, callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set RAT preference with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetPreferredRat
(
    uint32_t instance,
    taf_pa_radio_RatBitMask_t* bitmaskPtr
)
{
    if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::RatPreferenceResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestRatPreference(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get RAT preference with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    *bitmaskPtr = Utility::Convert::TelRatPreferenceToRat(request->ratPreference);

    return 0;
}

pa_result_t taf_pa_radio_GetVoiceServiceInfo
(
    uint32_t instance,
    taf_pa_radio_VoiceServiceInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    int phone = Utility::Convert::InstanceToPhone(instance);
    if (pa.managers.phone->getPhone(phone) == nullptr)
    {
        PA_ERROR("Invalid phone %d.", phone);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto result = pa.managers.phone->getPhone(phone)->requestVoiceServiceState(request);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get voice service state with phone %d.", phone);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    Utility::Convert::VoiceServiceInfo(request->voiceServiceState, infoPtr);

    return 0;
}

pa_result_t taf_pa_radio_GetDataServieState
(
    uint32_t instance,
    taf_pa_radio_DataServiceState_t* statePtr
)
{
    if (statePtr == nullptr)
    {
        PA_ERROR("statePtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.dataServingSystems[instance] == nullptr)
    {
        PA_ERROR("Data serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::DataServiceStatusResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.dataServingSystems[instance]->requestServiceStatus(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get data service status with data serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    *statePtr = Utility::Convert::ServiceState(request->dataServiceState);

    return 0;
}

pa_result_t taf_pa_radio_GetServiceDomain
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_ServiceDomain_t* domainPtr
)
{
    if (domainPtr == nullptr)
    {
        PA_ERROR("domainPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::ServingSystemInfo info;
    auto result = pa.managers.telephonyServingSystems[instance]->getSystemInfo(info);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get serving system information with telephony serving system manager"
            " %d.", instance);
        return -EFAULT;
    }

    *domainPtr = Utility::Convert::ServiceDomain(info.domain);

    return 0;
}

pa_result_t taf_pa_radio_GetServiceDomainPreferences
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainBitMask_t* bitmaskPtr
)
{
    if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::ServiceDomainPreferenceResponse, request,
        placeholders::_1, placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestServiceDomainPreference(
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get service domain preference with telephony serving system manager"
            " %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    *bitmaskPtr = Utility::Convert::ServiceDomainPreference(request->serviceDomainPreference);

    return 0;
}

pa_result_t taf_pa_radio_SetServiceDomainPreferences
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainBitMask_t bitmask
)
{
    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::ServiceDomainPreference preference = Utility::Convert::ServiceDomainPreference(bitmask);
    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.telephonyServingSystems[instance]->setServiceDomainPreference(
        preference, callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set service domain preference with telephony serving system manager"
            " %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetSignalStrengthLevel
(
    uint32_t instance,
    taf_pa_radio_Rat_t rat,
    taf_pa_radio_SignalStrengthLevel_t* levelPtr
)
{
    if (levelPtr == nullptr)
    {
        PA_ERROR("levelPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    int phone = Utility::Convert::InstanceToPhone(instance);
    if (pa.managers.phone->getPhone(phone) == nullptr)
    {
        PA_ERROR("Invalid phone %d.", phone);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto result = pa.managers.phone->getPhone(phone)->requestSignalStrength(request);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get signal strength with phone %d.", phone);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    *levelPtr = Utility::Convert::SignalStrengthLevel(rat, request->signalStrengthPtr);

    return 0;
}

pa_result_t taf_pa_radio_GetSignalStrengthInfo
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    int phone = Utility::Convert::InstanceToPhone(instance);
    if (pa.managers.phone->getPhone(phone) == nullptr)
    {
        PA_ERROR("Invalid phone %d.", phone);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto result = pa.managers.phone->getPhone(phone)->requestSignalStrength(request);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get signal strength with phone %d.", phone);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    Utility::Convert::SignalStrengthInfo(request->signalStrengthPtr, infoPtr);

    return 0;
}

pa_result_t taf_pa_radio_SetSignalStrengthInd
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthIndConfig_t* configPtr
)
{
    if (configPtr == nullptr)
    {
        PA_ERROR("configPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    int phone = Utility::Convert::InstanceToPhone(instance);
    if (pa.managers.phone->getPhone(phone) == nullptr)
    {
        PA_ERROR("Invalid phone %d.", phone);
        return -EFAULT;
    }

    vector<tel::SignalStrengthConfigEx> config;
    vector<tel::SignalStrengthConfigData> configData;
    Utility::Convert::SignalStrengthIndConfig(configPtr, config);
    uint16_t time = 0;
    if (configPtr->hysteresisTimeValid)
        time = configPtr->hysteresisTime;

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.phone->getPhone(phone)->configureSignalStrength(config, time,
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to configure signal strength with phone %d.", phone);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetCellLocationListInfo
(
    uint32_t instance,
    taf_pa_radio_CellLocationListInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    int phone = Utility::Convert::InstanceToPhone(instance);
    if (pa.managers.phone->getPhone(phone) == nullptr)
    {
        PA_ERROR("Invalid phone %d.", phone);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CellInfoListResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.phone->getPhone(phone)->requestCellInfo(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get cell information with phone %d.", phone);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    Utility::Convert::CellInfoList(request->cellInfoPtrList, infoPtr);

    return 0;
}

pa_result_t taf_pa_radio_GetCurrNetworkName
(
    uint32_t instance,
    taf_pa_radio_CurrNetworkName_t* namePtr
)
{
    if (namePtr == nullptr)
    {
        PA_ERROR("namePtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    int phone = Utility::Convert::InstanceToPhone(instance);
    if (pa.managers.phone->getPhone(phone) == nullptr)
    {
        PA_ERROR("Invalid phone %d.", phone);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::OperatorInfoResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.phone->getPhone(phone)->requestOperatorInfo(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get operator name with phone %d.", phone);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    Utility::Convert::String(request->operatorInfo.longName, &namePtr->fullNameValid,
        namePtr->fullNamePtr, namePtr->fullNameSize);
    Utility::Convert::String(request->operatorInfo.shortName, &namePtr->shortNameValid,
        namePtr->shortNamePtr, namePtr->shortNameSize);

    return 0;
}

pa_result_t taf_pa_radio_PerformPlmnNetworkScan
(
    uint32_t instance,
    taf_pa_radio_PlmnNetworkScanConfig_t* configPtr,
    taf_pa_radio_PlmnScanInformation_t* informationPtr
)
{
    if (configPtr == nullptr)
    {
        PA_ERROR("configPtr is nullptr.");
        return -EINVAL;
    }

    if (informationPtr == nullptr)
    {
        PA_ERROR("informationPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.networkSelections[instance] == nullptr)
    {
        PA_ERROR("Network selection manager %d is nullptr.", instance);
        return -EFAULT;
    }
    if (pa.managers.networkSelections[instance] == nullptr)
    {
        PA_ERROR("Network selection listener %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::NetworkScanInfo info;
    info.scanType = tel::NetworkScanType::USER_SPECIFIED_RAT;
    info.ratMask = Utility::Convert::RatToTelRat(configPtr->bitmask);

    auto status = pa.managers.networkSelections[instance]->registerListener(
        pa.listeners.networkSelections[instance]);
    if (status != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to register network selection listner %d.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.networkSelections[instance]->performNetworkScan(info, callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to perform network scan with network selection manager %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
    {
        PA_ERROR("Error occured when getting response with network selection manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Scan(instance, configPtr->timeout);

    status = pa.managers.networkSelections[instance]->deregisterListener(
        pa.listeners.networkSelections[instance]);
    if (status != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to deregister network selection listner %d.", instance);
        return -EFAULT;
    }

    if (pa.listeners.networkSelections[instance]->result != 0)
        return pa.listeners.networkSelections[instance]->result;

    uint32_t i = 0;
    for (auto info : pa.listeners.networkSelections[instance]->operatorInfoList)
    {
        size_t bytes = info.getName().size();
        if (bytes < TAF_PA_RADIO_PLMN_NETWORK_DESCRIPTION_MAX_BYTES)
        {
            memcpy(informationPtr->plmnInfo[i].description, info.getName().c_str(), bytes);
            informationPtr->plmnInfo[i].description[bytes] = '\0';
        }
        else
        {
            memcpy(informationPtr->plmnInfo[i].description, info.getName().c_str(),
               TAF_PA_RADIO_PLMN_NETWORK_DESCRIPTION_MAX_BYTES -1);
            informationPtr->plmnInfo[i].description[TAF_PA_RADIO_PLMN_NETWORK_DESCRIPTION_MAX_BYTES - 1] = '\0';
        }

        informationPtr->plmnInfo[i].plmnIdValid = 1;
        pa_result_t result = Utility::Convert::StringToU16(info.getMcc(),
            &informationPtr->plmnInfo[i].plmnId.mcc);
        if (result != 0)
        {
            PA_ERROR("Failed to convert MCC %s.", info.getMcc().c_str());
            informationPtr->plmnInfo[i].plmnIdValid = 0;
        }

        result = Utility::Convert::StringToU16(info.getMnc(),
            &informationPtr->plmnInfo[i].plmnId.mnc);
        if (result != 0)
        {
            PA_ERROR("Failed to convert MNC %s.", info.getMnc().c_str());
            informationPtr->plmnInfo[i].plmnIdValid = 0;
        }

        informationPtr->plmnInfo[i].rat = Utility::Convert::Rat(info.getRat());

        switch (info.getStatus().inUse)
        {
            case tel::InUseStatus::CURRENT_SERVING:
                informationPtr->plmnInfo[i].inUseStatus =
                   TAF_PA_RADIO_NETWORK_IN_USE_STATUS_CURRENT_SERVING;
                break;
            case tel::InUseStatus::AVAILABLE:
                informationPtr->plmnInfo[i].inUseStatus =
                   TAF_PA_RADIO_NETWORK_IN_USE_STATUS_AVAILABLE;
                break;
            default:
                informationPtr->plmnInfo[i].inUseStatus =
                   TAF_PA_RADIO_NETWORK_IN_USE_STATUS_UNKNOWN;
                break;
        }

        switch (info.getStatus().roaming)
        {
            case tel::RoamingStatus::HOME:
                informationPtr->plmnInfo[i].roamingStatus =
                   TAF_PA_RADIO_NETWORK_ROAMING_STATUS_HOME;
                break;
            case tel::RoamingStatus::ROAM:
                informationPtr->plmnInfo[i].roamingStatus =
                   TAF_PA_RADIO_NETWORK_ROAMING_STATUS_ROAMING;
                break;
            default:
                informationPtr->plmnInfo[i].roamingStatus =
                   TAF_PA_RADIO_NETWORK_ROAMING_STATUS_UNKNOWN;
                break;
        }

        switch (info.getStatus().forbidden)
        {
            case tel::ForbiddenStatus::FORBIDDEN:
                informationPtr->plmnInfo[i].forbiddenStatus =
                   TAF_PA_RADIO_NETWORK_FORBIDDEN_STATUS_FORBIDDEN;
                break;
            case tel::ForbiddenStatus::NOT_FORBIDDEN:
                informationPtr->plmnInfo[i].forbiddenStatus =
                   TAF_PA_RADIO_NETWORK_FORBIDDEN_STATUS_NOT_FORBIDDEN;
                break;
            default:
                informationPtr->plmnInfo[i].forbiddenStatus =
                   TAF_PA_RADIO_NETWORK_FORBIDDEN_STATUS_UNKNOWN;
                break;
        }

        switch (info.getStatus().preferred)
        {
            case tel::PreferredStatus::PREFERRED:
                informationPtr->plmnInfo[i].preferredStatus =
                   TAF_PA_RADIO_NETWORK_PREFERRED_STATUS_PREFERRED;
                break;
            case tel::PreferredStatus::NOT_PREFERRED:
                informationPtr->plmnInfo[i].preferredStatus =
                   TAF_PA_RADIO_NETWORK_PREFERRED_STATUS_NOT_PREFERRED;
                break;
            default:
                informationPtr->plmnInfo[i].preferredStatus =
                   TAF_PA_RADIO_NETWORK_PREFERRED_STATUS_UNKNOWN;
                break;
        }

        i++;
        if (i >= TAF_PA_RADIO_PLMN_SCAN_NETWORK_MAX_COUNT)
            break;
    }

    informationPtr->plmnCount = i;

    return 0;
}

pa_result_t taf_pa_radio_GetBandCapabilities
(
    uint32_t instance,
    taf_pa_radio_BandBitMask_t* bitmaskPtr
)
{
    if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::RfBandCapabilityResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestRFBandCapability(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get RF band capability with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    *bitmaskPtr = Utility::Convert::Band(request->rfBandCapabilityPtr);

    return 0;
}

pa_result_t taf_pa_radio_GetLteBandCapabilities
(
    uint32_t instance,
    taf_pa_radio_LteBand_t* bandPtr
)
{
    if (bandPtr == nullptr)
    {
        PA_ERROR("bandPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::RfBandCapabilityResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestRFBandCapability(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get RF band capability with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    Utility::Convert::Band(request->rfBandCapabilityPtr, bandPtr);

    return 0;
}

pa_result_t taf_pa_radio_SetBandPreferences
(
    uint32_t instance,
    taf_pa_radio_BandBitMask_t bitmask
)
{
    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    vector<tel::GsmRFBand> gsmBands;
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_450)
        gsmBands.emplace_back(tel::GsmRFBand::GSM_450);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_480)
        gsmBands.emplace_back(tel::GsmRFBand::GSM_480);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_750)
        gsmBands.emplace_back(tel::GsmRFBand::GSM_750);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_850)
        gsmBands.emplace_back(tel::GsmRFBand::GSM_850);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_CLASS_E_GSM_900_BAND)
        gsmBands.push_back(tel::GsmRFBand::GSM_900_EXTENDED);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_CLASS_P_GSM_900_BAND)
        gsmBands.push_back(tel::GsmRFBand::GSM_900_PRIMARY);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_GSM_BAND_RAILWAYS_900_BAND)
        gsmBands.push_back(tel::GsmRFBand::GSM_900_RAILWAYS);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_CLASS_GSM_DCS_1800_BAND)
        gsmBands.push_back(tel::GsmRFBand::GSM_1800);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_GSM_PCS_1900_BAND)
        gsmBands.push_back(tel::GsmRFBand::GSM_1900);

    vector<tel::WcdmaRFBand> wcdmaBands;
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_CH_IMT_2100_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_2100);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_PCS_1900_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_PCS_1900);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_CH_DCS_1800_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_DCS_1800);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_1700_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_1700_US);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_US_850_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_850);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_800_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_800);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_2600_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_2600);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_900_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_900);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_EU_J_1700_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_1700_JAPAN);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_1500_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_1500_JAPAN);
    if (bitmask & TAF_PA_RADIO_BITMASK_BAND_WCDMA_JAPAN_850_BAND)
        wcdmaBands.push_back(tel::WcdmaRFBand::WCDMA_850_JAPAN);

    auto builder = make_shared<tel::RFBandListBuilder>();
    common::ErrorCode error = common::ErrorCode::UNKNOWN;
    shared_ptr<tel::IRFBandList> peferences =
        builder->addGsmRFBands(gsmBands).addWcdmaRFBands(wcdmaBands).build(error);
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to build band list.");
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.telephonyServingSystems[instance]->setRFBandPreferences(peferences,
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set RF band preferences with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetBandPreferences
(
    uint32_t instance,
    taf_pa_radio_BandBitMask_t* bitmaskPtr
)
{
    if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::RfBandPreferenceResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestRFBandPreferences(
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get RF band preferences with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    *bitmaskPtr = Utility::Convert::Band(request->rfBandPreferencePtr);

    return 0;
}

pa_result_t taf_pa_radio_SetLteBandPreferences
(
    uint32_t instance,
    taf_pa_radio_LteBand_t* bandPtr
)
{
    if (bandPtr == nullptr)
    {
        PA_ERROR("bandPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    vector<tel::LteRFBand> bands;
    for (uint32_t i = 0; i < TAF_PA_RADIO_LTE_BAND_GROUP_COUNT; i++)
    {
        for (uint32_t j = 0; j < LTE_BAND_NUM_PER_GROUP; j++)
        {
            if (bandPtr->bitmask[i] & (uint64_t)0x1 << j)
                bands.emplace_back(static_cast<tel::LteRFBand>(
                    i * LTE_BAND_NUM_PER_GROUP + j + 1));
        }
    }

    auto builder = make_shared<tel::RFBandListBuilder>();
    common::ErrorCode error = common::ErrorCode::UNKNOWN;
    shared_ptr<tel::IRFBandList> peferences = builder->addLteRFBands(bands).build(error);
    if (error != common::ErrorCode::SUCCESS)
    {
        PA_ERROR("Failed to build band list.");
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.telephonyServingSystems[instance]->setRFBandPreferences(peferences,
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set RF band preferences with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetLteBandPreferences
(
    uint32_t instance,
    taf_pa_radio_LteBand_t* bandPtr
)
{
    if (bandPtr == nullptr)
    {
        PA_ERROR("bandPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::RfBandPreferenceResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestRFBandPreferences(
        callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get RF band preferences with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    Utility::Convert::Band(request->rfBandPreferencePtr, bandPtr);

    return 0;
}

pa_result_t taf_pa_radio_GetImsRegistrationStatus
(
    uint32_t instance,
    taf_pa_radio_ImsRegistrationStatus_t* statusPtr
)
{
    if (statusPtr == nullptr)
    {
        PA_ERROR("statusPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsServingSystems[instance] == nullptr)
    {
        PA_ERROR("IMS serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::ImsRegistrationInfoResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.imsServingSystems[instance]->requestRegistrationInfo(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get IMS registration infomration with IMS serving manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    *statusPtr = Utility::Convert::ImsRegistrationStatus(request->imsRegistrationInfo.imsRegStatus);

    return 0;
}

pa_result_t taf_pa_radio_GetLteCsCapability
(
    uint32_t instance,
    taf_pa_radio_LteCsCapability_t* capabilityPtr
)
{
    if (capabilityPtr == nullptr)
    {
        PA_ERROR("capabilityPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }


    tel::LteCsCapability capability = tel::LteCsCapability::UNKNOWN;
    auto result = pa.managers.telephonyServingSystems[instance]->getLteCsCapability(capability);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get LTE CS capability with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    *capabilityPtr = Utility::Convert::LteCsCapability(capability);

    return 0;
}

pa_result_t taf_pa_radio_GetImsServiceStatus
(
    uint32_t instance,
    taf_pa_radio_ImsService_t service,
    taf_pa_radio_ImsServiceStatus_t* statusPtr
)
{
    if (statusPtr == nullptr)
    {
        PA_ERROR("statusPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsServingSystems[instance] == nullptr)
    {
        PA_ERROR("IMS serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::ImsServiceInfoResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.imsServingSystems[instance]->requestServiceInfo(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get IMS service infomration with IMS serving manager %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    return Utility::Convert::ImsServiceStatus(service, request->imsServiceInfo, statusPtr);
}

pa_result_t taf_pa_radio_GetImsPdpFailureErrorCode
(
    uint32_t instance,
    taf_pa_radio_ImsPdpFailureErrorCode_t* codePtr
)
{
    if (codePtr == nullptr)
    {
        PA_ERROR("codePtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsServingSystems[instance] == nullptr)
    {
        PA_ERROR("IMS serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::ImsPdpStatusResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.imsServingSystems[instance]->requestPdpStatus(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get IMS PDP status with IMS serving manager %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    *codePtr = Utility::Convert::ImsPdpFailureErrorCode(request->imsPdpStatusInfo.failureCode);

    return 0;
}

pa_result_t taf_pa_radio_ToggleImsService
(
    uint32_t instance,
    taf_pa_radio_ImsServiceSettingBitMask_t bitmask,
    bool enable
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsSetting == nullptr)
    {
        PA_ERROR("Invalid IMS setting manager.");
        return -EFAULT;
    }

    int slot = Utility::Convert::InstanceToSlot(instance);
    SlotId slotId = Utility::Convert::SlotToSlotId(slot);

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    if (bitmask & TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VONR)
    {
        auto result1 = pa.managers.imsSetting->toggleVonr(slotId, enable, callback);
        if (result1 != common::Status::SUCCESS)
        {
            PA_ERROR("Failed to toggle VoNR with IMS setting manager.");
            return -EFAULT;
        }

        Utility::WaitCallback::Request();

        if (request->result != 0)
            return request->result;
    }

    taf_pa_radio_ImsServiceSettingBitMask_t nonVonrBitmask =
        bitmask & ~TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VONR;
    if (nonVonrBitmask == 0)
        return 0;

    tel::ImsServiceConfig config;
    Utility::Convert::ImsServiceConfig(nonVonrBitmask, enable, &config);
    auto result2 = pa.managers.imsSetting->setServiceConfig(slotId, config, callback);
    if (result2 != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to toggle IMS service with IMS setting manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    return request->result;
}

pa_result_t taf_pa_radio_GetEnabledImsService
(
    uint32_t instance,
    taf_pa_radio_ImsServiceSettingBitMask_t* bitmaskPtr
)
{
    if (bitmaskPtr == nullptr)
    {
        PA_ERROR("bitmaskPtr is nullptr.");
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsSetting == nullptr)
    {
        PA_ERROR("Invalid IMS setting manager.");
        return -EFAULT;
    }

    int slot = Utility::Convert::InstanceToSlot(instance);
    SlotId slotId = Utility::Convert::SlotToSlotId(slot);

    auto& request = pa.callbacks.request;
    auto callback1 = bind(&RequestCallback::ImsVonrStatusResponse, request, placeholders::_1,
        placeholders::_2, placeholders::_3);
    auto result = pa.managers.imsSetting->requestVonrStatus(slotId, callback1);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get IMS VoNR status with IMS setting manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    taf_pa_radio_ImsServiceSettingBitMask_t bitmask = 0x0;
    if (request->isVoNREnabled)
        bitmask |= TAF_PA_RADIO_BITMASK_IMS_SERVICE_SETTING_VONR;

    auto callback2 = bind(&RequestCallback::ImsServiceConfigResponse, request, placeholders::_1,
        placeholders::_2, placeholders::_3);
    result = pa.managers.imsSetting->requestServiceConfig(slotId, callback2);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get IMS service toggle status with IMS setting manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
    {
        if (request->imsServiceConfigError == common::ErrorCode::NOT_SUPPORTED)
        {
            PA_ERROR("IMS service config is not supported; returning VoNR status only.");
            *bitmaskPtr = bitmask;
            return 0;
        }

        return request->result;
    }

    Utility::Convert::ImsService(request->imsServiceConfig, &bitmask);

    *bitmaskPtr = bitmask;

    return 0;
}

pa_result_t taf_pa_radio_SetImsUserAgent
(
    uint32_t instance,
    const char* namePtr
)
{
    if (namePtr == nullptr)
    {
        PA_ERROR("namePtr is nullptr.");
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsSetting == nullptr)
    {
        PA_ERROR("Invalid IMS setting manager.");
        return -EFAULT;
    }

    int slot = Utility::Convert::InstanceToSlot(instance);
    SlotId slotId = Utility::Convert::SlotToSlotId(slot);

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::CommonResponse, request, placeholders::_1);
    auto result = pa.managers.imsSetting->setSipUserAgent(slotId, namePtr, callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to set IMS user agent with IMS setting manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();

    if (request->result != 0)
        return request->result;

    return request->result;
}

pa_result_t taf_pa_radio_GetImsUserAgent
(
    uint32_t instance,
    char* namePtr,
    size_t namePtrSize
)
{
    if (namePtr == nullptr)
    {
        PA_ERROR("namePtr is nullptr.");
        return -EINVAL;
    }

    if (namePtrSize <= 1)
    {
        PA_ERROR("Invalid namePtrSize %d.", namePtrSize);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.imsSetting == nullptr)
    {
        PA_ERROR("Invalid IMS setting manager.");
        return -EFAULT;
    }

    int slot = Utility::Convert::InstanceToSlot(instance);
    SlotId slotId = Utility::Convert::SlotToSlotId(slot);

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::ImsSigUserAgentResponse, request, placeholders::_1,
        placeholders::_2, placeholders::_3);
    auto result = pa.managers.imsSetting->requestSipUserAgent(slotId, callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get IMS sip user agent with IMS setting manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    size_t bytes = request->imsSipUserAgent.size();
    if (bytes < namePtrSize)
    {
        memcpy(namePtr, request->imsSipUserAgent.c_str(), bytes);
        namePtr[bytes] = '\0';
    }
    else
    {
        memcpy(namePtr, request->imsSipUserAgent.c_str(), namePtrSize - 1);
        namePtr[namePtrSize - 1] = '\0';
    }

    return 0;
}

pa_result_t taf_pa_radio_GetEndcAvailability
(
    uint32_t instance,
    taf_pa_radio_EndcAvailability_t* availabilityPtr
)
{
    if (availabilityPtr == nullptr)
    {
        PA_ERROR("availabilityPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::DcStatus status = pa.managers.telephonyServingSystems[instance]->getDcStatus();

    *availabilityPtr = Utility::Convert::EndcAvailability(status.endcAvailability);

    return 0;
}

pa_result_t taf_pa_radio_GetDcnrRestriction
(
    uint32_t instance,
    taf_pa_radio_DcnrRestriction_t* restrictionPtr
)
{
    if (restrictionPtr == nullptr)
    {
        PA_ERROR("restrictionPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::DcStatus status = pa.managers.telephonyServingSystems[instance]->getDcStatus();

    *restrictionPtr = Utility::Convert::DcnrRestriction(status.dcnrRestriction);

    return 0;
}

pa_result_t taf_pa_radio_GetSimCapacityInfo
(
    taf_pa_radio_SimCapabilityInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto result = pa.managers.phone->requestCellularCapabilityInfo(request);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get cellular capability information with phone manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    infoPtr->totalCount = request->cellularCapabilityInfo.simCount;
    infoPtr->maxActiveCount = request->cellularCapabilityInfo.maxActiveSims;

    return 0;
}

pa_result_t taf_pa_radio_GetDeviceAndSimCardRatCapability
(
    uint32_t instance,
    taf_pa_radio_DeviceAndSimCardRatCapability_t* capabilityPtr
)
{
    if (capabilityPtr == nullptr)
    {
        PA_ERROR("capabilityPtr is nullptr.");
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.phone == nullptr)
    {
        PA_ERROR("Invalid phone manager.");
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto result = pa.managers.phone->requestCellularCapabilityInfo(request);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get cellular capability information with phone manager.");
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    int slot = Utility::Convert::InstanceToSlot(instance);

    pa_result_t paResult = Utility::Convert::Rat(slot,
        request->cellularCapabilityInfo.deviceRatCapability, &capabilityPtr->devBitmask);
    if (paResult != 0)
        return paResult;

    paResult = Utility::Convert::Rat(slot, request->cellularCapabilityInfo.simRatCapabilities,
        &capabilityPtr->simBitmask);

    return paResult;
}

pa_result_t taf_pa_radio_GetServingCellBandInfo
(
    uint32_t instance,
    taf_pa_radio_ServingCellBandInfo_t* infoPtr
)
{
    if (infoPtr == nullptr)
    {
        PA_ERROR("infoPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::RFBandInfoResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.telephonyServingSystems[instance]->requestRFBandInfo(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get RF band information with telephony serving system manager %d.",
            instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    return Utility::Convert::RFBandInfo(request->rfBandInfo, infoPtr);
}

pa_result_t taf_pa_radio_GetNrIcon
(
    uint32_t instance,
    taf_pa_radio_NrIcon_t* iconPtr
)
{
    if (iconPtr == nullptr)
    {
        PA_ERROR("iconPtr is nullptr.");
        return -EINVAL;
    }

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.dataServingSystems[instance] == nullptr)
    {
        PA_ERROR("Data serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    auto& request = pa.callbacks.request;
    auto callback = bind(&RequestCallback::NrIconTypeResponse, request, placeholders::_1,
        placeholders::_2);
    auto result = pa.managers.dataServingSystems[instance]->requestNrIconType(callback);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get NR icon type with data serving system manager %d.", instance);
        return -EFAULT;
    }

    Utility::WaitCallback::Request();
    if (request->result != 0)
        return request->result;

    *iconPtr = Utility::Convert::NrIcon(request->nrIconType);

    return 0;
}

taf_pa_radio_NetworkRejectHandlerRef_t taf_pa_radio_AddNetworkRejectHandler
(
    uint32_t instance,
    taf_pa_radio_NetworkRejectHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.networkReject.instance = instance;
    pa.indicators.networkReject.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.networkReject.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_RatChangeHandlerRef_t taf_pa_radio_AddRatChangeHandler
(
    uint32_t instance,
    taf_pa_radio_RatChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.ratChange.instance = instance;
    pa.indicators.ratChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.ratChange.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_VoiceServiceInfoHandlerRef_t taf_pa_radio_AddVoiceServiceInfoHandler
(
    uint32_t instance,
    taf_pa_radio_VoiceServiceInfoHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.voiceServiceInfo.instance = instance;
    pa.indicators.voiceServiceInfo.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.voiceServiceInfo.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_DataServiceStatusHandlerRef_t taf_pa_radio_AddDataServiceStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataServiceStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.dataServiceStatus.instance = instance;
    pa.indicators.dataServiceStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.dataServiceStatus.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_DataRoamingStatusHandlerRef_t taf_pa_radio_AddDataRoamingStatusHandler
(
    uint32_t instance,
    taf_pa_radio_DataRoamingStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.dataRoamingStatus.instance = instance;
    pa.indicators.dataRoamingStatus.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.dataRoamingStatus.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_SignalStrengthInfoChangeHandlerRef_t taf_pa_radio_AddSignalStrengthInfoChangeHandler
(
    uint32_t instance,
    taf_pa_radio_SignalStrengthInfoChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.signalStrengthInfoChange.instance = instance;
    pa.indicators.signalStrengthInfoChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.signalStrengthInfoChange.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_ImsRegStatusChangeHandlerRef_t taf_pa_radio_AddImsRegStatusChangeHandler
(
    uint32_t instance,
    taf_pa_radio_ImsRegStatusChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.imsRegStatusChange.instance = instance;
    pa.indicators.imsRegStatusChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.imsRegStatusChange.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_OperatingModeChangeHandlerRef_t taf_pa_radio_AddOperatingModeChangeHandler
(
    uint32_t instance,
    taf_pa_radio_OperatingModeChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.operatingModeChange.instance = instance;
    pa.indicators.operatingModeChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.operatingModeChange.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_ServiceDomainHandlerRef_t taf_pa_radio_AddServiceDomainHandler
(
    uint32_t instance,
    taf_pa_radio_ServiceDomainHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.serviceDomain.instance = instance;
    pa.indicators.serviceDomain.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.serviceDomain.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_LteCsCapabilityHandlerRef_t taf_pa_radio_AddLteCsCapabilityHandler
(
    uint32_t instance,
    taf_pa_radio_LteCsCapabilityHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.lteCsCapability.instance = instance;
    pa.indicators.lteCsCapability.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.lteCsCapability.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_ImsServiceInfoHandlerRef_t taf_pa_radio_AddImsServiceInfoHandler
(
    uint32_t instance,
    taf_pa_radio_ImsServiceInfoHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.imsServiceInfo.instance = instance;
    pa.indicators.imsServiceInfo.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.imsServiceInfo.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_ImsPdpErrorHandlerRef_t taf_pa_radio_AddImsPdpErrorHandler
(
    uint32_t instance,
    taf_pa_radio_ImsPdpErrorHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.imsPdpError.instance = instance;
    pa.indicators.imsPdpError.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.imsPdpError.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_CellInfoChangeHandlerRef_t taf_pa_radio_AddCellInfoChangeHandler
(
    uint32_t instance,
    taf_pa_radio_CellInfoChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.cellInfoChange.instance = instance;
    pa.indicators.cellInfoChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.cellInfoChange.contextPtr = contextPtr;

    return nullptr;
}

taf_pa_radio_NrIconChangeHandlerRef_t taf_pa_radio_AddNrIconChangeHandler
(
    uint32_t instance,
    taf_pa_radio_NrIconChangeHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();

    pa.indicators.nrIconChange.instance = instance;
    pa.indicators.nrIconChange.handlerFuncPtr = (void*)handlerFuncPtr;
    pa.indicators.nrIconChange.contextPtr = contextPtr;

    return nullptr;
}

pa_result_t taf_pa_radio_RegisterIndication
(
    uint32_t instance,
    uint8_t registration
)
{
    if (!common::DeviceConfig::isMultiSimSupported() && instance > 0)
        return -ENOTSUP;

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    int32_t result = taf_prop_radio_RegisterIndication(instance, registration);
    if (result != 0)
        PA_ERROR("Failed to control radio proprietary indications for instance %d.", instance);

    auto& pa = PlatformAdaptor::GetInstance();
    switch (registration)
    {
        case ENABLE_INDICATION:
        {
               if (instance == 0 && pa.managers.phone != nullptr)
                pa.managers.phone->registerListener(pa.listeners.phone);

               if (pa.managers.telephonyServingSystems[instance] != nullptr)
                    pa.managers.telephonyServingSystems[instance]->registerListener(
                        pa.listeners.telephonyServingSystems[instance]);

               if (pa.managers.imsServingSystems[instance] != nullptr)
                   pa.managers.imsServingSystems[instance]->registerListener(
                       pa.listeners.imsServingSystems[instance]);

                if (pa.managers.dataServingSystems[instance] != nullptr)
                    pa.managers.dataServingSystems[instance]->registerListener(
                        pa.listeners.dataServingSystems[instance]);

            break;
        }
        case DISABLE_INDICATION:
        {
            if (instance == 0 && pa.managers.phone != nullptr)
                pa.managers.phone->removeListener(pa.listeners.phone);

            if (pa.managers.telephonyServingSystems[instance] != nullptr)
                pa.managers.telephonyServingSystems[instance]->deregisterListener(
                    pa.listeners.telephonyServingSystems[instance]);

            if (pa.managers.imsServingSystems[instance] != nullptr)
                pa.managers.imsServingSystems[instance]->deregisterListener(
                    pa.listeners.imsServingSystems[instance]);

            if (pa.managers.dataServingSystems[instance] != nullptr)
                pa.managers.dataServingSystems[instance]->deregisterListener(
                    pa.listeners.dataServingSystems[instance]);

            break;
        }
        default:
        {
            PA_ERROR("Invalid registration %d.", registration);
            return -EINVAL;
        }
    }

    return 0;
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

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::ServingSystemInfo info;
    auto result = pa.managers.telephonyServingSystems[instance]->getSystemInfo(info);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get serving system information with telephony serving system manager"
            " %d.", instance);
        return -EFAULT;
    }

    *ratPtr = Utility::Convert::Rat(info.rat);

    PA_INFO("Serving RAT for instance %d from getSystemInfo: telux RAT %d -> PA RAT %d.",
        instance, static_cast<int>(info.rat), *ratPtr);

    return 0;
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

    if (instance >= MAX_INSTANCE)
    {
        PA_ERROR("Invalid instance %d.", instance);
        return -EINVAL;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    if (pa.managers.telephonyServingSystems[instance] == nullptr)
    {
        PA_ERROR("Telephony serving system manager %d is nullptr.", instance);
        return -EFAULT;
    }

    tel::ServingSystemInfo info;
    auto result = pa.managers.telephonyServingSystems[instance]->getSystemInfo(info);
    if (result != common::Status::SUCCESS)
    {
        PA_ERROR("Failed to get serving system information with telephony serving system manager"
            " %d.", instance);
        return -EFAULT;
    }

    taf_pa_radio_Rat_t servingRat = Utility::Convert::Rat(info.rat);
    if (rat != TAF_PA_RADIO_RAT_UNKNOWN && rat != servingRat)
    {
        *statusPtr = TAF_PA_RADIO_RAT_SERVICE_STATUS_NO_SERVICE;
    }
    else
    {
        *statusPtr = Utility::Convert::RatServiceStatus(info.state);
    }

    PA_INFO("RAT service status for instance %d: requested PA RAT %d, serving PA RAT %d, "
        "telux state %d -> PA status %d.",
        instance, rat, servingRat, static_cast<int>(info.state), *statusPtr);

    return 0;
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
