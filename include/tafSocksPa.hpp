/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_SOCKS_PA_HPP
#define TAF_SOCKS_PA_HPP

#ifdef TAF_PA_DEFAULT
#define PA_WEAK __attribute__((weak))
#else
#define PA_WEAK
#endif

#include "tafCommonPa.h"

//--------------------------------------------------------------------------------------------------
/**
 * The device mode.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_NET_DEVICE_UNKNOWN = -1,
    ///< Unknown.
    TAF_PA_NET_DEVICE_NONE = 0,
    ///< None.
    TAF_PA_NET_DEVICE_L2L = 1,
    ///< Device LAN-to-LAN mode.
    TAF_PA_NET_DEVICE_E2E = 2
    ///< Device end-to-end mode.
}
taf_pa_net_DeviceMode_t;

//--------------------------------------------------------------------------------------------------
/**
 * The SOCKS authentication type.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_NET_SOCKS_UNKNOWN = -1,
        ///< Unknown.
    TAF_PA_NET_SOCKS_NONE = 0,
        ///< No authentication.
    TAF_PA_NET_SOCKS_USER_PASSWD = 1
        ///< Username and password.
}
taf_pa_net_AuthMethod_t;

typedef void (*taf_pa_socks_CallCb)(pa_result_t result,void *contextPtr);

PA_SHARED PA_WEAK pa_result_t taf_pa_socks_Init();

//--------------------------------------------------------------------------------------------------
/**
 * Set device mode
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetDeviceMode
(
    taf_pa_net_DeviceMode_t deviceMode
);

//--------------------------------------------------------------------------------------------------
/**
 * Get device mode
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK taf_pa_net_DeviceMode_t taf_pa_net_GetDeviceMode
(
);

//--------------------------------------------------------------------------------------------------
/**
 * Set SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetSocksAuthMethod
(
    taf_pa_net_AuthMethod_t authMethod
);

//--------------------------------------------------------------------------------------------------
/**
 * Get SOCKS authentication method
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK taf_pa_net_AuthMethod_t taf_pa_net_GetSocksAuthMethod
(
);

//--------------------------------------------------------------------------------------------------
/**
 * Sets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetSocksLanInterface
(
    const char* ifName
);

//--------------------------------------------------------------------------------------------------
/**
 * Gets SOCKS LAN interface
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetSocksLanInterface
(
    char* ifName,
    size_t ifNameSize
);

//--------------------------------------------------------------------------------------------------
/**
 * Adds username/profile association
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_AddSocksAssociation
(
    const char* userName,
    uint32_t profileId
);

//--------------------------------------------------------------------------------------------------
/**
 * Deletes username/profile association
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_net_RemoveSocksAssociation
(
    const char* userName
);

/*======================================================================

 FUNCTION        EnableSocksCmdSync

======================================================================*/
PA_SHARED PA_WEAK pa_result_t taf_pa_net_EnableSocksCmdSync();

/*======================================================================

 FUNCTION        DisableSocksCmdSync

======================================================================*/
PA_SHARED PA_WEAK pa_result_t taf_pa_net_DisableSocksCmdSync();

/*======================================================================

 FUNCTION        EnableSocksCmdASync

======================================================================*/
PA_SHARED PA_WEAK pa_result_t taf_pa_net_EnableSocksCmdASync
(
   taf_pa_socks_CallCb callback,void *contextPtr
);

/*======================================================================

 FUNCTION        DisableSocksCmdASync

======================================================================*/
PA_SHARED PA_WEAK pa_result_t taf_pa_net_DisableSocksCmdASync
(
   taf_pa_socks_CallCb callback,void *contextPtr
);

#endif /* TAF_SOCKS_PA_HPP */

