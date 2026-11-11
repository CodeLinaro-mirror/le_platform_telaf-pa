/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_net.h"

//--------------------------------------------------------------------------------------------------
/**
 * Init this component
 */
//--------------------------------------------------------------------------------------------------

taf_prop_result_t taf_prop_net_Init
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deinit this component
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_Deinit
(
    void
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_SetDeviceMode
(
    taf_prop_net_DeviceMode_t deviceMode
)
{
    TAF_PROP_INFO("-----default stub Impl SetDeviceMode---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_GetDeviceMode
(
    taf_prop_net_DeviceMode_t* deviceMode
)
{
    TAF_PROP_INFO("-----default stub Impl GetDeviceMode---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_SetSocksAuthMethod
(
    taf_prop_net_AuthMethod_t authMethod
)
{
    TAF_PROP_INFO("-----default stub Impl SetSocksAuthMethod---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_GetSocksAuthMethod
(
    taf_prop_net_AuthMethod_t* authMethod
)
{
    TAF_PROP_INFO("-----default stub Impl GetSocksAuthMethod---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_SetSocksLanInterface
(
    const char* ifName
)
{
    TAF_PROP_INFO("-----default stub Impl SetSocksLanInterface---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_GetSocksLanInterface
(
    char* ifName,
    size_t ifNameSize
)
{
    TAF_PROP_INFO("-----default stub Impl GetSocksLanInterface---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_AddSocksAssociation
(
    const char* userName,
    uint32_t profileId
)
{
    TAF_PROP_INFO("-----default stub Impl AddSocksAssociation---");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_net_RemoveSocksAssociation
(
    const char* userName
)
{
    TAF_PROP_INFO("-----default stub Impl RemoveSocksAssociation---");
    return TAF_PROP_NOT_IMPLEMENTED;
}
