/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * ABI STABILITY REQUIREMENT
 * This header is part of the prop interface.
 * All declarations must use C-style linkage only (no C++ classes,
 * templates, references, or overloaded functions) to guarantee ABI
 * stability across independently compiled shared libraries.
 */

#ifndef TAF_PROP_NET_H
#define TAF_PROP_NET_H

#include <stdint.h>
#include "taf_prop_common.h"

#ifdef __cplusplus
extern "C" {
#endif


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

TAF_PROP_SHARED taf_prop_result_t taf_prop_net_Init(void);

//--------------------------------------------------------------------------------------------------
/**
 *  Net deinitialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_Deinit(void);

//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_SetDeviceMode
(
    taf_prop_net_DeviceMode_t deviceMode
);

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_GetDeviceMode(taf_prop_net_DeviceMode_t* deviceMode);

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_SetSocksAuthMethod
(
    taf_prop_net_AuthMethod_t authMethod
);

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_GetSocksAuthMethod
(
    taf_prop_net_AuthMethod_t* authMethod
);

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_SetSocksLanInterface(const char* ifName);

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_GetSocksLanInterface(char* ifName,
                                                                     size_t ifNameSize);

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_AddSocksAssociation(const char* userName,
                                                                    uint32_t profileId);

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_net_RemoveSocksAssociation(const char* userName);

#ifdef __cplusplus
}
#endif

#endif /* TAF_PROP_NET_H */
