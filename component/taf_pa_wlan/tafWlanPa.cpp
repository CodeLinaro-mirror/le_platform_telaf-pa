/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafWlanPa.hpp"
#include "tafCommonPa.h"

#include <any>
#include <cstring>
#include <future>
#include <atomic>
#include <telux/data/DataFactory.hpp>
#include <telux/wlan/WlanDefines.hpp>
#include <telux/wlan/WlanDeviceManager.hpp>
#include <telux/wlan/WlanFactory.hpp>

// Global initialization state for WLAN PA
static std::atomic<bool> g_wlanPaInitialized(false);

class WlanPAController
{
public:
    static WlanPAController *getInstance();

    taf_pa_result_t initialize();
    taf_pa_result_t deinitialize();

    taf_pa_result_t registerDeviceListener
    (
        taf::pa::wlan::DeviceListener listener,
        std::any ctx
    );

    taf_pa_result_t enableDevice
    (
        bool enable
    );

    taf_pa_result_t getStatus
    (
        bool &enabled
    );

    taf_pa_result_t setDeviceMode
    (
        int numAP,
        int numSTA
    );

    taf_pa_result_t getDeviceMode
    (
        int &numAP,
        int &numSTA
    );

    taf_pa_result_t setStaBridgeMode
    (
        taf::pa::wlan::StaId_e staId,
        taf::pa::wlan::Mode_e tafStaMode
    );

    taf_pa_result_t getStaBridgeMode
    (
        taf::pa::wlan::StaId_e staId,
        taf::pa::wlan::Mode_e &tafStaModeOut
    );

    taf_pa_result_t setStaIpConfig
    (
        taf::pa::wlan::StaId_e staId,
        taf::pa::wlan::IPType_e tafIpType
    );

    taf_pa_result_t setStaIpConfig
    (
        taf::pa::wlan::StaId_e staId,
        taf::pa::wlan::IPType_e tafIpType,
        const taf::pa::wlan::StaIpConfig_t &cfg
    );

    taf_pa_result_t getStaIpConfig
    (
        taf::pa::wlan::StaId_e staId,
        taf::pa::wlan::IPType_e &tafIpTypeOut,
        taf::pa::wlan::StaIpConfig_t &cfgOut
    );

    taf_pa_result_t getBandInterferenceConfig
    (
        bool &enabled,
        taf::pa::wlan::BandInterferenceConfig_t &cfgOut
    );

    taf_pa_result_t setBandInterferenceConfig
    (
        bool enable,
        const taf::pa::wlan::BandInterferenceConfig_t &cfg
    );

    class DevListener : public telux::wlan::IWlanListener
    {
    public:
        explicit DevListener
        (
            WlanPAController *paController
        );

        void onServiceStatusChange
        (
            telux::common::ServiceStatus status
        ) override;

        void onEnableChanged
        (
            bool enable
        ) override;

    private:
        WlanPAController *paController_;
    };

private:
    WlanPAController() = default;

    std::shared_ptr<telux::wlan::IWlanDeviceManager> devMgr_;
    std::shared_ptr<telux::wlan::IStaInterfaceManager> staMgr_;
    std::shared_ptr<telux::data::IDataSettingsManager> dataMgr_;
    std::shared_ptr<DevListener> devListener_;

    // Registered PA device listener
    taf::pa::wlan::DeviceListener devCb_;
    std::any devCtx_;

    std::mutex cbMutex_; // protects devCb_ and devCtx_
};

static telux::wlan::Id ToTeluxStaId(taf::pa::wlan::StaId_e id)
{
    int res = (id == taf::pa::wlan::StaId_e::ONE) ? 1 : 2;
    return static_cast<telux::wlan::Id>(res);
}

WlanPAController *WlanPAController::getInstance
(
    void
)
{
    static WlanPAController ctrl;
    return &ctrl;
}

taf_pa_result_t WlanPAController::initialize
(
    void
)
{
    auto &fac = telux::wlan::WlanFactory::getInstance();

    // Device manager
    struct State
    {
        std::promise<telux::common::ServiceStatus> prom;
    };
    auto state = std::make_shared<State>();

    devMgr_ = fac.getWlanDeviceManager([state](telux::common::ServiceStatus st)
    {
        TAF_PA_INFO("device manager callback status = %d", (int)st);
        try
        {
            state->prom.set_value(st);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("prom.set_value exception: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("prom.set_value unknown exception");
        }
    });

    if (!devMgr_)
    {
        TAF_PA_ERROR("devMgr_ is null");
        return TAF_PA_FAULT;
    }

    telux::common::ServiceStatus st = telux::common::ServiceStatus::SERVICE_FAILED;
    auto fut = state->prom.get_future();
    if (fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
    {
        TAF_PA_ERROR("timeout waiting for devMgr status");
        return TAF_PA_FAULT;
    }
    try
    {
        st = fut.get();
    }
    catch (const std::exception &e)
    {
        TAF_PA_ERROR("exception waiting for devMgr status: %s", e.what());
        return TAF_PA_FAULT;
    }
    catch (...)
    {
        TAF_PA_ERROR("unknown exception waiting for devMgr status");
        return TAF_PA_FAULT;
    }

    // Validate service availability before proceeding
    if (st != telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        TAF_PA_ERROR("Device manager service not available: %d", (int)st);
        return TAF_PA_FAULT;
    }

    // Listener
    devListener_ = std::make_shared<DevListener>(this);
    auto rc = devMgr_->registerListener(devListener_);
    if (rc != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("Device listener registration failed: %d", static_cast<int>(rc));
    }

    // STA interface manager
    staMgr_ = fac.getStaInterfaceManager();
    if (!staMgr_)
    {
        TAF_PA_ERROR("STA Interface Manager unavailable");
    }

    // Data settings manager
    auto &df = telux::data::DataFactory::getInstance();
    dataMgr_ = df.getDataSettingsManager(telux::data::OperationType::DATA_LOCAL);

    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::registerDeviceListener
(
    taf::pa::wlan::DeviceListener listener,
    std::any ctx
)
{
    std::lock_guard<std::mutex> lock(cbMutex_);
    devCb_ = std::move(listener);
    devCtx_ = ctx;
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::enableDevice
(
    bool enable
)
{
    if (!devMgr_)
    {
        TAF_PA_ERROR("devMgr_ is null");
        return TAF_PA_FAULT;
    }

    auto res = devMgr_->enable(enable);
    if (res == telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_INFO("enable(%d) succeeded", (int)enable);
        return TAF_PA_OK;
    }

    TAF_PA_ERROR("enable(%d) failed, errorcode: %d", (int)enable, (int)res);

    // Handle platforms that return NOT_SUPPORTED when the desired state is already applied.
    if (res == telux::common::ErrorCode::NOT_SUPPORTED)
    {
        // Query current status; if it matches requested enable, treat as success (no-op).
        bool en = false;
        std::vector<telux::wlan::InterfaceStatus> ifs;
        auto sres = devMgr_->getStatus(en, ifs);
        if (sres == telux::common::ErrorCode::SUCCESS)
        {
            if (enable && (en || !ifs.empty()))
            {
                TAF_PA_INFO("Device already enabled; no effect");
                return TAF_PA_OK;
            }
            if (!enable)
            {
                // Fallback OFF: remove all interfaces if hard-disable is unsupported.
                TAF_PA_INFO("enable(false) NOT_SUPPORTED; falling back to setMode(0,0)");
                auto mrc = devMgr_->setMode(/*numAP*/ 0, /*numSTA*/ 0);
                if (mrc == telux::common::ErrorCode::SUCCESS)
                {
                    // Verify OFF state after fallback
                    bool en2 = false;
                    std::vector<telux::wlan::InterfaceStatus> ifs2;
                    auto vrc = devMgr_->getStatus(en2, ifs2);
                    if (vrc == telux::common::ErrorCode::SUCCESS && ifs2.empty())
                    {
                        TAF_PA_INFO("setMode(0,0) succeeded and no interfaces active; "
                            "treating device as OFF");
                        return TAF_PA_OK;
                    }
                    TAF_PA_ERROR("setMode(0,0) succeeded but interfaces still present or status check "
                        "failed (vrc = %d, ifs = %zu)", (int)vrc, ifs2.size());
                }
                else
                {
                    TAF_PA_ERROR("setMode(0,0) fallback failed, errorcode: %d", (int)mrc);
                }
            }
        }
        else
        {
            TAF_PA_ERROR("getStatus failed during NOT_SUPPORTED handling, errorcode: %d", (int)sres);
        }
    }

    TAF_PA_ERROR("Failed to enable/disable WLAN device (enable=%d)", (int)enable);
    return TAF_PA_FAULT;
}

taf_pa_result_t WlanPAController::getStatus
(
    bool &enabled
)
{
    if (!devMgr_)
    {
        TAF_PA_ERROR("devMgr_ is null");
        return TAF_PA_FAULT;
    }

    bool en = false;
    std::vector<telux::wlan::InterfaceStatus> ifs;
    auto res = devMgr_->getStatus(en, ifs);
    if (res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("getStatus failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }
    if (ifs.empty())
        en = false;
    enabled = en;
    TAF_PA_INFO("getStatus succeeded (enabled=%d, ifs=%zu)", en ? 1 : 0, ifs.size());
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::setDeviceMode
(
    int numAP,
    int numSTA
)
{
    if (!devMgr_)
    {
        TAF_PA_ERROR("devMgr_ is null");
        return TAF_PA_FAULT;
    }

    auto res = devMgr_->setMode(numAP, numSTA);
    if (res == telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_INFO("setMode succeeded");
        return TAF_PA_OK;
    }
    if (res == telux::common::ErrorCode::NOT_SUPPORTED)
    {
        TAF_PA_ERROR("setMode NOT_SUPPORTED for configuration (AP=%d, STA=%d)", numAP, numSTA);
        return TAF_PA_UNSUPPORTED;
    }

    TAF_PA_ERROR("setMode failed, errorcode: %d", (int)res);
    return TAF_PA_FAULT;
}

taf_pa_result_t WlanPAController::getDeviceMode
(
    int &numAP,
    int &numSTA
)
{
    if (!devMgr_)
    {
        TAF_PA_ERROR("devMgr_ is null");
        return TAF_PA_FAULT;
    }

    int ap = 0, sta = 0;
    auto res = devMgr_->getConfig(ap, sta);
    if (res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("getConfig failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }
    TAF_PA_INFO("getConfig succeeded");
    numAP = ap;
    numSTA = sta;
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::setStaBridgeMode
(
    taf::pa::wlan::StaId_e staId,
    taf::pa::wlan::Mode_e tafStaMode
)
{
    if (!staMgr_)
    {
        TAF_PA_ERROR("staMgr_ is null");
        return TAF_PA_FAULT;
    }

    telux::wlan::StaBridgeMode bm = (tafStaMode == taf::pa::wlan::Mode_e::BRIDGE)
        ? telux::wlan::StaBridgeMode::BRIDGE
        : telux::wlan::StaBridgeMode::ROUTER;

    auto res = staMgr_->setBridgeMode(ToTeluxStaId(staId), bm);
    if(res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("setBridgeMode failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }
    TAF_PA_INFO("setBridgeMode succeeded");
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::getStaBridgeMode
(
    taf::pa::wlan::StaId_e staId,
    taf::pa::wlan::Mode_e &tafStaModeOut
)
{
    if (!staMgr_)
    {
        TAF_PA_ERROR("staMgr_ is null");
        return TAF_PA_FAULT;
    }

    std::vector<telux::wlan::StaConfig> cfg;
    auto res = staMgr_->getConfig(cfg);
    if (res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("getConfig failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }

    for (auto &c : cfg)
    {
        if (c.staId == ToTeluxStaId(staId))
        {
            tafStaModeOut = (c.bridgeMode == telux::wlan::StaBridgeMode::BRIDGE)
                                ? taf::pa::wlan::Mode_e::BRIDGE
                                : taf::pa::wlan::Mode_e::ROUTER;
            TAF_PA_INFO("getConfig succeeded");
            return TAF_PA_OK;
        }
    }
    TAF_PA_ERROR("getConfig not found error");
    return TAF_PA_NOT_FOUND;
}

taf_pa_result_t WlanPAController::setStaIpConfig
(
    taf::pa::wlan::StaId_e staId,
    taf::pa::wlan::IPType_e tafIpType
)
{
    if (!staMgr_)
    {
        TAF_PA_ERROR("staMgr_ is null");
        return TAF_PA_FAULT;
    }

    auto ip = (tafIpType == taf::pa::wlan::IPType_e::STATIC)
                  ? telux::wlan::StaIpConfig::STATIC_IP
                  : telux::wlan::StaIpConfig::DYNAMIC_IP;

    telux::wlan::StaStaticIpConfig s{};
    auto res = staMgr_->setIpConfig(ToTeluxStaId(staId), ip, s);
    if (res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("setIpConfig failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }
    TAF_PA_INFO("setIpConfig succeeded");
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::setStaIpConfig
(
    taf::pa::wlan::StaId_e staId,
    taf::pa::wlan::IPType_e tafIpType,
    const taf::pa::wlan::StaIpConfig_t &cfg
)
{
    if (!staMgr_)
    {
        TAF_PA_ERROR("staMgr_ is null");
        return TAF_PA_FAULT;
    }

    auto ip = (tafIpType == taf::pa::wlan::IPType_e::STATIC)
                  ? telux::wlan::StaIpConfig::STATIC_IP
                  : telux::wlan::StaIpConfig::DYNAMIC_IP;

    telux::wlan::StaStaticIpConfig s{};
    if (ip == telux::wlan::StaIpConfig::STATIC_IP)
    {
        s.ipAddr   = cfg.ipAddr;
        s.gwIpAddr = cfg.gwIpAddr;
        s.netMask  = cfg.netMask;
        s.dnsAddr  = cfg.dnsAddr;
    }

    auto res = staMgr_->setIpConfig(ToTeluxStaId(staId), ip, s);
    if (res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("setIpConfig failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }
    TAF_PA_INFO("setIpConfig succeeded");
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::getStaIpConfig
(
    taf::pa::wlan::StaId_e staId,
    taf::pa::wlan::IPType_e &tafIpTypeOut,
    taf::pa::wlan::StaIpConfig_t &cfgOut
)
{
    if (!staMgr_)
    {
        TAF_PA_ERROR("staMgr_ is null");
        return TAF_PA_FAULT;
    }

    std::vector<telux::wlan::StaConfig> cfg;
    auto res = staMgr_->getConfig(cfg);
    if (res != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("getConfig failed, errorcode: %d", (int)res);
        return TAF_PA_FAULT;
    }

    for (auto &c : cfg)
    {
        if (c.staId == ToTeluxStaId(staId))
        {
            tafIpTypeOut = (c.ipConfig == telux::wlan::StaIpConfig::STATIC_IP)
                               ? taf::pa::wlan::IPType_e::STATIC
                               : taf::pa::wlan::IPType_e::DYNAMIC;

            if (tafIpTypeOut == taf::pa::wlan::IPType_e::STATIC)
            {
                cfgOut.ipAddr   = c.staticIpConfig.ipAddr;
                cfgOut.gwIpAddr = c.staticIpConfig.gwIpAddr;
                cfgOut.netMask  = c.staticIpConfig.netMask;
                cfgOut.dnsAddr  = c.staticIpConfig.dnsAddr;
            }
            else
            {
                cfgOut = {};
            }
            TAF_PA_INFO("getConfig succeeded");
            return TAF_PA_OK;
        }
    }

    TAF_PA_ERROR("getConfig not found error");
    return TAF_PA_NOT_FOUND;
}

taf_pa_result_t WlanPAController::getBandInterferenceConfig
(
    bool &enabled,
    taf::pa::wlan::BandInterferenceConfig_t &cfgOut
)
{
    if (!dataMgr_)
    {
        TAF_PA_ERROR("dataMgr_ is null");
        return TAF_PA_FAULT;
    }

    // Shared state to avoid dangling references on unexpected late callbacks.
    struct State {
        bool en = false;
        std::shared_ptr<telux::data::BandInterferenceConfig> dcfg;
        std::promise<telux::common::ErrorCode> prom;
    };
    auto state = std::make_shared<State>();

    auto cb = [state](bool isEnabled,
                      std::shared_ptr<telux::data::BandInterferenceConfig> cfg,
                      telux::common::ErrorCode err)
    {
        TAF_PA_INFO("isEnabled=%d err=%d", (int)isEnabled, (int)err);
        state->en = isEnabled;
        state->dcfg = std::move(cfg);
        try
        {
            state->prom.set_value(err);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("prom.set_value exception: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("prom.set_value unknown exception");
        }
    };

    auto res = dataMgr_->requestBandInterferenceConfig(cb);
    if (res != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("requestBandInterferenceConfig failed, status: %d", (int)res);
        return TAF_PA_FAULT;
    }

    auto fut = state->prom.get_future();
    if (fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
    {
        TAF_PA_ERROR("requestBandInterferenceConfig timed out");
        return TAF_PA_FAULT;
    }

    telux::common::ErrorCode err;
    try
    {
        err = fut.get();
    }
    catch (const std::exception &e)
    {
        TAF_PA_ERROR("exception waiting callback: %s", e.what());
        return TAF_PA_FAULT;
    }
    catch (...)
    {
        TAF_PA_ERROR("unknown exception waiting callback");
        return TAF_PA_FAULT;
    }

    if (err != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("requestBandInterferenceConfig callback error: %d", (int)err);
        return TAF_PA_FAULT;
    }

    enabled = state->en;
    if (state->dcfg)
    {
        cfgOut.prioBand = (state->dcfg->priority == telux::data::BandPriority::N79)
            ? taf::pa::wlan::BandIntPriority_e::N79
            : taf::pa::wlan::BandIntPriority_e::WLAN_5_GHZ;
        cfgOut.wlanWaitTimeInSec = state->dcfg->wlanWaitTimeInSec;
        cfgOut.n79WaitTimeInSec  = state->dcfg->n79WaitTimeInSec;
    }
    else
    {
        cfgOut.prioBand = taf::pa::wlan::BandIntPriority_e::WLAN_5_GHZ;
        cfgOut.wlanWaitTimeInSec = 0;
        cfgOut.n79WaitTimeInSec  = 0;
    }

    TAF_PA_INFO("getBandInterferenceConfig succeeded");
    return TAF_PA_OK;
}

taf_pa_result_t WlanPAController::setBandInterferenceConfig
(
    bool enable,
    const taf::pa::wlan::BandInterferenceConfig_t &cfg
)
{
    if (!dataMgr_)
    {
        TAF_PA_ERROR("dataMgr_ is null");
        return TAF_PA_FAULT;
    }

    std::shared_ptr<telux::data::BandInterferenceConfig> tcfg = nullptr;
    if (enable)
    {
        tcfg = std::make_shared<telux::data::BandInterferenceConfig>();
        tcfg->priority = (cfg.prioBand == taf::pa::wlan::BandIntPriority_e::N79)
            ? telux::data::BandPriority::N79
            : telux::data::BandPriority::WLAN;
        tcfg->wlanWaitTimeInSec = cfg.wlanWaitTimeInSec;
        tcfg->n79WaitTimeInSec  = cfg.n79WaitTimeInSec;
    }

    struct State {
        std::promise<telux::common::ErrorCode> prom;
    };
    auto state = std::make_shared<State>();

    auto cb = [state](telux::common::ErrorCode err)
    {
        TAF_PA_INFO("setBandInterferenceConfig cb err=%d", (int)err);
        try
        {
            state->prom.set_value(err);
        }
        catch (const std::exception &e)
        {
            TAF_PA_ERROR("prom.set_value exception: %s", e.what());
        }
        catch (...)
        {
            TAF_PA_ERROR("prom.set_value unknown exception");
        }
    };

    auto res = dataMgr_->setBandInterferenceConfig(enable, tcfg, cb);
    if (res != telux::common::Status::SUCCESS)
    {
        TAF_PA_ERROR("setBandInterferenceConfig dispatch failed, status: %d", (int)res);
        return TAF_PA_FAULT;
    }

    auto fut = state->prom.get_future();
    if (fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
    {
        TAF_PA_ERROR("setBandInterferenceConfig timed out waiting for callback");
        return TAF_PA_FAULT;
    }

    telux::common::ErrorCode err;
    try
    {
        err = fut.get();
    }
    catch (const std::exception &e)
    {
        TAF_PA_ERROR("exception waiting callback: %s", e.what());
        return TAF_PA_FAULT;
    }
    catch (...)
    {
        TAF_PA_ERROR("unknown exception waiting callback");
        return TAF_PA_FAULT;
    }

    if (err != telux::common::ErrorCode::SUCCESS)
    {
        TAF_PA_ERROR("setBandInterferenceConfig callback error: %d", (int)err);
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("setBandInterferenceConfig succeeded");
    return TAF_PA_OK;
}

WlanPAController::DevListener::DevListener(WlanPAController* paController)
    : paController_(paController)
{
}

static taf::pa::wlan::ServiceState_e TeluxToPAServiceState
(
    telux::common::ServiceStatus st
)
{
    taf::pa::wlan::ServiceState_e res = taf::pa::wlan::ServiceState_e::SERVICE_FAILED;

    if (st == telux::common::ServiceStatus::SERVICE_UNAVAILABLE)
    {
        res = taf::pa::wlan::ServiceState_e::SERVICE_UNAVAILABLE;
    }
    else if (st == telux::common::ServiceStatus::SERVICE_AVAILABLE)
    {
        res = taf::pa::wlan::ServiceState_e::SERVICE_AVAILABLE;
    }
    else if (st == telux::common::ServiceStatus::SERVICE_FAILED)
    {
        res = taf::pa::wlan::ServiceState_e::SERVICE_FAILED;
    }
    return res;
}

void WlanPAController::DevListener::onServiceStatusChange
(
    telux::common::ServiceStatus status
)
{
    TAF_PA_INFO("WlanPAController::DevListener::onServiceStatusChange: status = %d", (int)status);
    if (!paController_)
        return;

    taf::pa::wlan::DeviceListener cb;
    std::any ctx;
    {
        std::lock_guard<std::mutex> lock(paController_->cbMutex_);
        cb = paController_->devCb_;
        ctx = paController_->devCtx_;
    }
    if (cb)
    {
        // Report enable=false along with mapped service status
        cb(false, TeluxToPAServiceState(status), ctx);
    }
}

void WlanPAController::DevListener::onEnableChanged
(
    bool enable
)
{
    TAF_PA_INFO("WlanPAController::DevListener::onEnableChanged: enable = %d", (int)enable);
    if (!paController_)
        return;

    taf::pa::wlan::DeviceListener cb;
    std::any ctx;
    {
        std::lock_guard<std::mutex> lock(paController_->cbMutex_);
        cb = paController_->devCb_;
        ctx = paController_->devCtx_;
    }
    if (cb)
    {
        // Treat enable changes as available (service running and responding)
        cb(enable, taf::pa::wlan::ServiceState_e::SERVICE_AVAILABLE, ctx);
    }
}

taf_pa_result_t taf::pa::wlan::Init
(
    void
)
{
    TAF_PA_INFO("Init called");

    // Check if already initialized
    if (g_wlanPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Init() called multiple times; already initialized");
        return TAF_PA_OK;
    }

    auto result = WlanPAController::getInstance()->initialize();
    if (result == TAF_PA_OK)
    {
        g_wlanPaInitialized.store(true, std::memory_order_release);
    }
    return result;
}

taf_pa_result_t taf::pa::wlan::RegisterDeviceListener
(
    DeviceListener listener,
    std::any ctx
)
{
    TAF_PA_INFO("RegisterDeviceListener called");
    return WlanPAController::getInstance()->registerDeviceListener(listener, std::move(ctx));
}

taf_pa_result_t taf::pa::wlan::EnableDevice
(
    bool enable
)
{
    TAF_PA_INFO("EnableDevice: enable = %d", (int)enable);
    return WlanPAController::getInstance()->enableDevice(enable);
}

taf_pa_result_t taf::pa::wlan::GetStatus
(
    bool &enabled
)
{
    TAF_PA_INFO("GetStatus called");
    return WlanPAController::getInstance()->getStatus(enabled);
}

taf_pa_result_t taf::pa::wlan::SetDeviceMode
(
    int numAP,
    int numSTA
)
{
    TAF_PA_INFO("SetDeviceMode: numAP = %d numSTA = %d", numAP, numSTA);
    return WlanPAController::getInstance()->setDeviceMode(numAP, numSTA);
}

taf_pa_result_t taf::pa::wlan::GetDeviceMode
(
    int &numAP,
    int &numSTA
)
{
    TAF_PA_INFO("GetDeviceMode called");
    return WlanPAController::getInstance()->getDeviceMode(numAP, numSTA);
}

taf_pa_result_t taf::pa::wlan::SetStaBridgeMode
(
    StaId_e staId,
    taf::pa::wlan::Mode_e tafStaMode
)
{
    TAF_PA_INFO("SetStaBridgeMode: staId = %s mode = %d",
            (staId == StaId_e::ONE ? "ONE" : "TWO"), (int)tafStaMode);
    return WlanPAController::getInstance()->setStaBridgeMode(staId, tafStaMode);
}

taf_pa_result_t taf::pa::wlan::GetStaBridgeMode
(
    StaId_e staId,
    Mode_e &tafStaModeOut
)
{
    TAF_PA_INFO("GetStaBridgeMode: staId=%s", (staId == StaId_e::ONE ? "ONE" : "TWO"));
    return WlanPAController::getInstance()->getStaBridgeMode(staId, tafStaModeOut);
}

taf_pa_result_t taf::pa::wlan::SetStaIpConfig
(
    StaId_e staId,
    IPType_e ipType
)
{
    TAF_PA_INFO("SetStaIpConfig (dynamic): staId=%s ipType=%d",
            (staId == StaId_e::ONE ? "ONE" : "TWO"), (int)ipType);
    return WlanPAController::getInstance()->setStaIpConfig(staId, ipType);
}

taf_pa_result_t taf::pa::wlan::SetStaIpConfig
(
    StaId_e staId,
    IPType_e ipType,
    const StaIpConfig_t &cfg
)
{
    TAF_PA_INFO("SetStaIpConfig (static): staId=%s ipType=%d",
            (staId == StaId_e::ONE ? "ONE" : "TWO"), (int)ipType);
    return WlanPAController::getInstance()->setStaIpConfig(staId, ipType, cfg);
}

taf_pa_result_t taf::pa::wlan::GetStaIpConfig
(
    StaId_e staId,
    taf::pa::wlan::IPType_e &tafIpTypeOut,
    StaIpConfig_t &cfgOut
)
{
    TAF_PA_INFO("GetStaIpConfig: staId=%s", (staId == StaId_e::ONE ? "ONE" : "TWO"));
    return WlanPAController::getInstance()->getStaIpConfig(staId, tafIpTypeOut, cfgOut);
}

taf_pa_result_t taf::pa::wlan::GetBandInterferenceConfig
(
    bool &enabled,
    BandInterferenceConfig_t &cfgOut
)
{
    TAF_PA_INFO("GetBandInterferenceConfig called");
    return WlanPAController::getInstance()->getBandInterferenceConfig(enabled, cfgOut);
}

taf_pa_result_t taf::pa::wlan::SetBandInterferenceConfig
(
    bool enable,
    const BandInterferenceConfig_t &cfg
)
{
    TAF_PA_INFO("SetBandInterferenceConfig: enable=%d", (int)enable);
    return WlanPAController::getInstance()->setBandInterferenceConfig(enable, cfg);
}

taf_pa_result_t WlanPAController::deinitialize
(
    void
)
{
    TAF_PA_INFO("Starting WLAN platform adaptor deinitialization...");

    // Step 1: Clear the PA-level device callback and context under the mutex
    // so no further device enable/service-status notifications are dispatched.
    TAF_PA_INFO("Clearing devCb_ and devCtx_");
    {
        std::lock_guard<std::mutex> lock(cbMutex_);
        devCb_ = nullptr;
        devCtx_.reset();
    }

    // Step 2: Deregister the device listener from the device manager so the SDK
    // stops delivering WLAN events to it.
    TAF_PA_INFO("Deregistering devListener_ from devMgr_");
    if (devMgr_ && devListener_)
    {
        auto rc = devMgr_->deregisterListener(devListener_);
        if (rc != telux::common::ErrorCode::SUCCESS)
        {
            TAF_PA_ERROR("Failed to deregister device listener, errorcode: %d", (int)rc);
            // Continue cleanup even if deregistration failed
        }
    }

    // Step 3: Reset the device listener shared pointer so the listener object is
    // released once no other owners remain.
    TAF_PA_INFO("Resetting devListener_");
    devListener_.reset();

    // Step 4: Reset all manager shared pointers so the underlying SDK objects
    // are released once no other owners remain.
    TAF_PA_INFO("Resetting staMgr_, dataMgr_ and devMgr_");
    staMgr_.reset();
    dataMgr_.reset();
    devMgr_.reset();

    TAF_PA_INFO("WLAN platform adaptor deinitialization complete.");
    return TAF_PA_OK;
}

taf_pa_result_t taf::pa::wlan::Deinit
(
    void
)
{
    TAF_PA_INFO("Deinit called");

    // Step 0: Check if Init() was called successfully
    if (!g_wlanPaInitialized.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Deinit() called before Init() was successfully called");
        return TAF_PA_FAULT;
    }
    auto result = WlanPAController::getInstance()->deinitialize();
    if (result == TAF_PA_OK)
    {
        g_wlanPaInitialized.store(false, std::memory_order_release);
    }
    return result;
}
