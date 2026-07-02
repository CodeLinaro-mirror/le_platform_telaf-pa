/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#include "taf_prop_radio.h"

taf_prop_result_t taf_prop_radio_Init
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_Deinit
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_InitInstance
(
    uint32_t instance
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_RegisterIndication
(
    uint32_t instance,
    uint8_t registration,
    taf_prop_radio_DisableIndicationMode_t mode
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_SetSysInfoIndLimit
(
    uint32_t instance,
    taf_prop_radio_SysInfoIndLimitMask_t limitMask
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetServiceStatus
(
    uint32_t instance,
    taf_prop_radio_Rat_t *servingRat,
    taf_prop_radio_RatServiceStatus_t* status
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetSysInfoIndLimit
(
    uint32_t instance,
    taf_prop_radio_SysInfoIndLimitMask_t *limitMask
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_PerformPciNetworkScan
(
    uint32_t instance,
    taf_prop_radio_RatBitMask_t bitmask,
    taf_prop_radio_PciScanInformation_t* informationPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetServingRat
(
    uint32_t instance,
    taf_prop_radio_Rat_t* ratPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetRatSvcStatus
(
    uint32_t instance,
    taf_prop_radio_Rat_t rat,
    taf_prop_radio_RatServiceStatus_t* statusPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetServingCellRac
(
    uint32_t instance,
    taf_prop_radio_Rat_t rat,
    uint8_t* racPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetDataAvailSysStatus
(
    uint32_t instance,
    taf_prop_radio_DataAvailSysStatus_t* statusPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_radio_GetLteCphyCaInfo
(
    uint32_t instance,
    taf_prop_radio_LteCphyCaInfo_t* infoPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_radio_RatSvcStatusHandlerRef_t taf_prop_radio_AddRatSvcStatusHandler
(
    uint32_t instance,
    taf_prop_radio_RatSvcStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return nullptr;
}

taf_prop_radio_LteCphyCaHandlerRef_t taf_prop_radio_AddLteCphyCaHandler
(
    uint32_t instance,
    taf_prop_radio_LteCphyCaHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return nullptr;
}

taf_prop_radio_DataAvailSysStatusHandlerRef_t taf_prop_radio_AddDataAvailSysStatusHandler
(
    uint32_t instance,
    taf_prop_radio_DataAvailSysStatusHdlrFunc_t handlerFuncPtr,
    void* contextPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return nullptr;
}

taf_prop_result_t taf_prop_radio_GetDataCurrRoamingStatus
(
    uint32_t instance,
    taf_prop_radio_DataRoamingStatus_t* statusPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}
