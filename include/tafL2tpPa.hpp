/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_L2TP_PA_HPP
#define TAF_L2TP_PA_HPP

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include "tafCommonPa.h"
#include <mutex>
#include <condition_variable>
#include <future>


#define DEFAULT_MTU_SIZE       1422

//--------------------------------------------------------------------------------------------------
/**
 * The IPv4 address maximum length.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_NET_IPV4_ADDR_MAX_LEN 16

//--------------------------------------------------------------------------------------------------
/**
 * The IPv6 address maximum length.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_NET_IPV6_ADDR_MAX_LEN 46
//--------------------------------------------------------------------------------------------------
/**
 * The interface name maximum length.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_NET_INTERFACE_NAME_MAX_LEN 30

//--------------------------------------------------------------------------------------------------
/**
 * L2TP encapsulation protocols.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_NET_L2TP_NONE = 0,

    TAF_PA_NET_L2TP_IP = 1,

    TAF_PA_NET_L2TP_UDP = 2

}
taf_pa_net_L2tpEncapProtocol_t;

//--------------------------------------------------------------------------------------------------
/**
 * Preferred IP family for the connection.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_NET_L2TP_UNKNOWN = -1,

    TAF_PA_NET_L2TP_IPV4 = 4,

    TAF_PA_NET_L2TP_IPV6 = 6,

    TAF_PA_NET_L2TP_IPV4V6 = 10

}
taf_pa_net_IpFamilyType_t;

typedef struct
{
    uint32_t locId;
    uint32_t peerId;
} taf_pa_net_L2tpSessionConfig_t;

/*
 * @brief The struct of Tunnel.
 */
typedef struct
{
    uint32_t locId;  // Local tunnel Id
    uint32_t peerId;  // Peer tunnel Id
    uint32_t localUdpPort;  // Local udp port
    uint32_t peerUdpPort;  // Peer udp port
    taf_pa_net_L2tpEncapProtocol_t prot;  // Encapsulation protocol
    char peerIpv4Addr[TAF_PA_NET_IPV4_ADDR_MAX_LEN];  // Peer ipv4 address
    char peerIpv6Addr[TAF_PA_NET_IPV6_ADDR_MAX_LEN];  // Peer ipv6 address
    char locIface[TAF_PA_NET_INTERFACE_NAME_MAX_LEN]; // Interface name to create L2TP tunnel on
    taf_pa_net_IpFamilyType_t ipType;       // Ip family type
    uint16_t sessionNum;                 ///Session number
    std::vector<taf_pa_net_L2tpSessionConfig_t> sessionConfig;  // Session list
} taf_pa_net_L2tpTunnel_t;


/*
 * @brief The struct of l2tp configuration.
 */
typedef struct
{
    bool enableL2tp;
    bool enableMtu;
    bool enableTcpMss;
    uint32_t mtuSize;
    std::vector<taf_pa_net_L2tpTunnel_t> configList;
} taf_pa_net_L2tpConfig_t;

// Typedef for asynchronous callbacks
typedef void (*taf_pa_l2tp_CallCb)(pa_result_t result, void *contextPtr);

PA_SHARED pa_result_t taf_pa_l2tp_Init();
PA_SHARED pa_result_t taf_pa_l2tp_Deinit();

//--------------------------------------------------------------------------------------------------
/**
 * Add Tunnel Asynchronously
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_AddTunnelAsync
(
    const taf_pa_net_L2tpTunnel_t& addTunnelConfig,   // IN
    taf_pa_l2tp_CallCb callback,           // IN
    void* contextPtr                    // IN
);

//--------------------------------------------------------------------------------------------------
/**
 * Add Tunnel Synchronously
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_AddTunnelSync
(
    const taf_pa_net_L2tpTunnel_t& addTunnelConfig   // IN
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove Tunnel Asynchronously
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_RemoveTunnelAsync
(
    const uint32_t tunnelId,   // IN
    taf_pa_l2tp_CallCb callback,          // IN
    void* contextPtr                    // IN
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove Tunnel Synchronously
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_RemoveTunnelSync
(
    const uint32_t tunnelId  // IN
);

//--------------------------------------------------------------------------------------------------
/**
 * Request L2TP Configuration
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_RequestL2tpConfig
(
    taf_pa_net_L2tpConfig_t& L2tpConfig  // OUT
);

//--------------------------------------------------------------------------------------------------
/**
 * Set L2TP Configuration Asynchronously
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_SetL2tpConfigAsync
(
    const taf_pa_net_L2tpConfig_t& L2tpConfig,  // IN
    taf_pa_l2tp_CallCb callback,     // IN
    void* contextPtr                                   // IN
);

//--------------------------------------------------------------------------------------------------
/**
 * Set L2TP Configuration Synchronously
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED pa_result_t taf_pa_net_SetL2tpConfigSync
(
    taf_pa_net_L2tpConfig_t& L2tpConfig  // IN
);

#endif /* TAF_L2TP_PA_HPP */
