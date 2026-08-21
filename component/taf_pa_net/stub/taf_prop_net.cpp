/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_net.hpp"

//--------------------------------------------------------------------------------------------------
/**
 * Init this component
 */
//--------------------------------------------------------------------------------------------------

int32_t taf_prop_net_Init
(
    void
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NET_RESULT_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deinit this component
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_Deinit
(
    void
)
{
    PROP_INFO("Function is not implemented in stub PA.");
    return TAF_PROP_NET_RESULT_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 *
 * @return LE_TAF_PROP_NET_RESULT_FAULT                      Failed
 *         TAF_PROP_NET_RESULT_BAD_PARAMETER              Invalid deviceMode
 *         TAF_PROP_NET_RESULT_OK                         Succeeded
 *
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_SetDeviceMode
(
    taf_prop_net_DeviceMode_t deviceMode  ///< [IN] Device mode
)
{
    PROP_INFO("-----default stub Impl SetDeviceMode---");
    return TAF_PROP_NET_RESULT_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 *
 * @return taf_prop_net_DeviceMode_t          Device mode
 *
 */
//--------------------------------------------------------------------------------------------------
taf_prop_net_DeviceMode_t taf_prop_net_GetDeviceMode
(
)
{
    PROP_INFO("-----default stub Impl GetDeviceMode---");
    return TAF_PROP_NET_DEVICE_NONE;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_SetSocksAuthMethod
(
    taf_prop_net_AuthMethod_t authMethod
)
{
    PROP_INFO("-----default stub Impl SetSocksAuthMethod---");
    return TAF_PROP_NET_RESULT_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
taf_prop_net_AuthMethod_t taf_prop_net_GetSocksAuthMethod
(
)
{
    PROP_INFO("-----default stub Impl GetSocksAuthMethod---");
    return TAF_PROP_NET_SOCKS_UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_SetSocksLanInterface
(
    const char* ifName
)
{
    PROP_INFO("-----default stub Impl SetSocksLanInterface---");
    return TAF_PROP_NET_RESULT_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_GetSocksLanInterface
(
    char* ifName,
    size_t ifNameSize
)
{
    PROP_INFO("-----default stub Impl GetSocksLanInterface---");
    return TAF_PROP_NET_RESULT_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_AddSocksAssociation
(
    const char* userName,
    uint32_t profileId
)
{
    PROP_INFO("-----default stub Impl AddSocksAssociation---");
    return TAF_PROP_NET_RESULT_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
int32_t taf_prop_net_RemoveSocksAssociation
(
    const char* userName
)
{
    PROP_INFO("-----default stub Impl RemoveSocksAssociation---");
    return TAF_PROP_NET_RESULT_OK;
}
