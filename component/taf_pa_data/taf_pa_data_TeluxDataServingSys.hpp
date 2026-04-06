/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


/**
 * @file taf_pa_data_TeluxDataServingSys.hpp
 * @brief Telux Data serving system subsystem management.
 *
 */

#ifndef __TAF_PA_DATA_TELUXDATA_SERVINGSYS_HPP__
#define __TAF_PA_DATA_TELUXDATA_SERVINGSYS_HPP__

#include "telux/common/CommonDefines.hpp"
#include "telux/data/DataDefines.hpp"
#include "tafDataPa.hpp"

namespace taf
{
namespace pa
{
namespace data
{

/**
 * The maximum number of slots
 */
class TafPaTeluxDataServingSysListener : public telux::data::IServingSystemListener
{
public:
    TafPaTeluxDataServingSysListener(SlotId slot);
    ~TafPaTeluxDataServingSysListener();
    void onRoamingStatusChanged(telux::data::RoamingStatus status) override;
    void onServiceStatusChange(telux::common::ServiceStatus status) override;

private:
    // Common variables
    SlotId slotId_;
};

} //data
} //pa
} //taf

#endif //__TAF_PA_DATA_TELUXDATA_SERVINGSYS_HPP__
