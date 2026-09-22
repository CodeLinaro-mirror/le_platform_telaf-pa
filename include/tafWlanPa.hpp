/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_WLAN_PA_HPP
#define TAF_WLAN_PA_HPP

#include "tafCommonPa.h"
#include <any>
#include <functional>
#include <string>

namespace taf::pa::wlan {

enum class Mode_e
{
    UNKNOWN,           ///< Unknown mode or not set.
    ROUTER,            ///< Router mode.
    BRIDGE             ///< Bridge mode.
};

enum class IPType_e
{
    UNKNOWN,         ///< Unknown IP type or not set.
    DYNAMIC,         ///< Station is configured with dynamic IP.
    STATIC           ///< Station is configured with Static IP.
};

enum class BandIntPriority_e
{
    UNKNOWN,      ///< Unknown band priority.
    N79,          ///< The 5G N79 band.
    WLAN_5_GHZ    ///< The Wifi 5 GHz band.
};

enum class ServiceState_e
{
    SERVICE_UNAVAILABLE,
    SERVICE_AVAILABLE,
    SERVICE_READY,
    SERVICE_FAILED
};

enum class StaId_e
{
    ONE,
    TWO // Reserved for future use
};

struct StaIpConfig_t
{
    std::string ipAddr;
    std::string gwIpAddr;
    std::string dnsAddr;
    std::string netMask;
};

struct BandInterferenceConfig_t
{
    BandIntPriority_e prioBand;
    uint32_t wlanWaitTimeInSec;
    uint32_t n79WaitTimeInSec;
};

// Device listener: (enabled, serviceStatus, context)
using DeviceListener = std::function<void(bool enabled, ServiceState_e serviceStatus, std::any)>;

TAF_PA_SHARED taf_pa_result_t Init();

TAF_PA_SHARED taf_pa_result_t Deinit();

TAF_PA_SHARED taf_pa_result_t RegisterDeviceListener
(
    DeviceListener listener,
    std::any ctx
);

TAF_PA_SHARED taf_pa_result_t EnableDevice
(
    bool enable
);

TAF_PA_SHARED taf_pa_result_t GetStatus
(
    bool &enabled
);

TAF_PA_SHARED taf_pa_result_t SetDeviceMode
(
    int numAP,
    int numSTA
);

TAF_PA_SHARED taf_pa_result_t GetDeviceMode
(
    int &numAP,
    int &numSTA
);

TAF_PA_SHARED taf_pa_result_t SetStaBridgeMode
(
    StaId_e staId,
    taf::pa::wlan::Mode_e tafStaMode
);

TAF_PA_SHARED taf_pa_result_t GetStaBridgeMode
(
    StaId_e staId,
    taf::pa::wlan::Mode_e &tafStaModeOut
);

TAF_PA_SHARED taf_pa_result_t SetStaIpConfig
(
    StaId_e staId,
    taf::pa::wlan::IPType_e tafIpType         // taf_wlanSta_IPType_t
);

TAF_PA_SHARED taf_pa_result_t SetStaIpConfig
(
    StaId_e staId,
    taf::pa::wlan::IPType_e tafIpType,        // taf_wlanSta_IPType_t
    const StaIpConfig_t &cfg
);

TAF_PA_SHARED taf_pa_result_t GetStaIpConfig
(
    StaId_e staId,
    taf::pa::wlan::IPType_e &tafIpTypeOut,    // taf_wlanSta_IPType_t
    StaIpConfig_t &cfgOut
);

TAF_PA_SHARED taf_pa_result_t GetBandInterferenceConfig
(
    bool &enabled,
    BandInterferenceConfig_t &cfgOut
);

TAF_PA_SHARED taf_pa_result_t SetBandInterferenceConfig
(
    bool enable,
    const BandInterferenceConfig_t &cfg
);

} // namespace taf::pa::wlan

#endif // TAF_WLAN_PA_HPP
