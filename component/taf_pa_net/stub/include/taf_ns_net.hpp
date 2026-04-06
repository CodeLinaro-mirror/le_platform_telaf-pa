/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NS_NET_HPP
#define TAF_NS_NET_HPP

#include "tafCommonPa.h"

#define TAF_NS_NET_RESULT_OK 0
#define TAF_NS_NET_RESULT_FAULT -6
#define TAF_NS_NET_RESULT_BAD_PARAMETER -15
#define TAF_NS_NET_RESULT_NOT_IMPLEMENTED -20

//--------------------------------------------------------------------------------------------------
/**
 * The device mode.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PROP_NET_DEVICE_UNKNOWN = -1,
    ///< Unknown.
    TAF_PROP_NET_DEVICE_NONE = 0,
    ///< None.
    TAF_PROP_NET_DEVICE_L2L = 1,
    ///< Device LAN-to-LAN mode.
    TAF_PROP_NET_DEVICE_E2E = 2
    ///< Device end-to-end mode.
}
taf_prop_net_DeviceMode_t;

//--------------------------------------------------------------------------------------------------
/**
 * The SOCKS authentication type.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PROP_NET_SOCKS_UNKNOWN = -1,
        ///< Unknown.
    TAF_PROP_NET_SOCKS_NONE = 0,
        ///< No authentication.
    TAF_PROP_NET_SOCKS_USER_PASSWD = 1
        ///< Username and password.
}
taf_prop_net_AuthMethod_t;

PA_SHARED int32_t taf_ns_net_Init();


//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED int32_t taf_prop_net_SetDeviceMode(taf_prop_net_DeviceMode_t deviceMode);

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED taf_prop_net_DeviceMode_t taf_prop_net_GetDeviceMode();

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED int32_t taf_prop_net_SetSocksAuthMethod(taf_prop_net_AuthMethod_t authMethod);

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED taf_prop_net_AuthMethod_t taf_prop_net_GetSocksAuthMethod();

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED int32_t taf_prop_net_SetSocksLanInterface(const char* ifName);

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED int32_t taf_prop_net_GetSocksLanInterface(char* ifName, size_t ifNameSize);

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED int32_t taf_prop_net_AddSocksAssociation(const char* userName, uint32_t profileId);

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED int32_t taf_prop_net_RemoveSocksAssociation(const char* userName);



#endif /* TAF_NS_NET_HPP_ */
