/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_VLAN_PA_HPP
#define TAF_VLAN_PA_HPP

#ifdef TAF_PA_DEFAULT
#define PA_WEAK __attribute__((weak))
#else
#define PA_WEAK
#endif

#include <map>
#include <vector>
#include <list>

#include "tafCommonPa.h"

//--------------------------------------------------------------------------------------------------
/**
 * VLAN Hardware acceleration service state.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_HW_ACC_STATE_INACTIVE = 0,
    TAF_PA_VLAN_HW_ACC_STATE_ACTIVE
} taf_pa_vlan_hwacc_state_t;

//--------------------------------------------------------------------------------------------------
/**
 * Network type
 */
 //-------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_NETWORK_UNKNOWN = -1,      ///< UNKNOWN network type.
    TAF_PA_VLAN_NETWORK_LAN = 1,                   ///< LAN network type.
    TAF_PA_VLAN_NETWORK_WAN = 2,                   ///< WAN network type.
} taf_pa_vlan_network_type_t;

//--------------------------------------------------------------------------------------------------
/**
 * Backhaul type
 */
 //-------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_BH_ETH = 0,///< Ethernet backhaul.
    TAF_PA_VLAN_BH_USB,    ///< USB backhaul.
    TAF_PA_VLAN_BH_WLAN,   ///< WLAN backhaul.
    TAF_PA_VLAN_BH_WWAN,   ///< WWAN backhaul with default profile ID.
    TAF_PA_VLAN_BH_BLE,    ///< Bluetooth backhaul.
    TAF_PA_VLAN_BH_MAX     ///< Maximum supported.
} taf_pa_vlan_backhaul_type_t;

//--------------------------------------------------------------------------------------------------
/**
 * The VLAN interface type.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_IFACE_UNKNOWN,           ///< Unknown interface.
    TAF_PA_VLAN_IFACE_WLAN,              ///< Wireless local area network (WLAN).
    TAF_PA_VLAN_IFACE_ETH,               ///< Ethernet (ETH).
    TAF_PA_VLAN_IFACE_ECM,               ///< Ethernet control model (ECM).
    TAF_PA_VLAN_IFACE_RNDIS,             ///< Remote network driver interface specification (RNDIS).
    TAF_PA_VLAN_IFACE_MHI                ///< Modem host interface (MHI).
} taf_pa_vlan_iface_type_t;

//--------------------------------------------------------------------------------------------------
/**
 * Callback for hardware acceleration notification handler.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_vlan_HardwareAccelerationHandler_t)
(
   taf_pa_vlan_hwacc_state_t state        //The VLAN hardware acceleration state.
);

/*
 * @brief The struct of vlan bind config.
 */
typedef struct
{
    uint8_t slotId;                  //optional slotId , otherwise default slotId
    int32_t profileId;               //mandatory if backhaul type is WWAN.
    taf_pa_vlan_backhaul_type_t backhaulType; //VLAN bind config item for backhaul type.
    int16_t vlanIdBackhaul;          //optional VLAN bind config item if Backhaul is not WWAN.
    int16_t vlanId;
} taf_pa_VlanBindConfig_t;

/*
 * @brief The struct of vlan.
 */
typedef struct
{
    int16_t vlanId;
    bool isAccelerated;
    uint8_t priority;
    taf_pa_vlan_network_type_t nwType;
    taf_pa_vlan_iface_type_t   iface;
    bool isBridgeEnabled = true;
} taf_pa_Vlan_t;


//--------------------------------------------------------------------------------------------------
/**
 * Register hardware acceleration notification handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_vlan_RegHwAccelarationUpdateHandler
(
    taf_pa_vlan_HardwareAccelerationHandler_t handlerFunc
);

PA_SHARED PA_WEAK pa_result_t taf_pa_vlan_Init();

PA_SHARED PA_WEAK pa_result_t taf_pa_net_AddVlanInterface
(
     const taf_pa_Vlan_t vlanConfig,
     const taf_pa_vlan_iface_type_t iftype
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_RemoveVlanInterface
(
     const taf_pa_Vlan_t vlanConfig,
     const taf_pa_vlan_iface_type_t iftype
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_BindWithBackhaul
(
     const taf_pa_Vlan_t vlan,
     const taf_pa_VlanBindConfig_t vlanBindConfig
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_UnbindWithBackhaul
(
     const taf_pa_Vlan_t vlan,                        // IN
     const taf_pa_VlanBindConfig_t vlanBindConfig     // IN
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_QueryVlanInfo
(
    std::vector<taf_pa_Vlan_t> &vlanEntryInfo    // OUT
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_QueryVlanToBackhaulMappingList
(
    const uint8_t slotID,                          // IN
    const taf_pa_vlan_backhaul_type_t backhaulType,  // IN
    std::vector<taf_pa_VlanBindConfig_t>& vlanEntryInfo  // OUT
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_RegVlanListener();

PA_SHARED PA_WEAK pa_result_t taf_pa_net_DeregVlanListener();

//Data Settings API

PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetBackhaulPreference
(
    std::vector<taf_pa_vlan_backhaul_type_t> &vlanEntryInfo    // OUT
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetBackhaulPreference
(
    std::vector<taf_pa_vlan_backhaul_type_t> vlanEntryInfo    // IN
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetIPPassThroughNatConfig
(
    bool &isEnabledPtr                         // OUT
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetIPPassThroughNatConfig
(
    bool isEnabledPtr                         // IN
);

//--------------------------------------------------------------------------------------------------
/**
 * MAC address maximum length.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_NET_MAC_ADDR_MAX_LEN 17

//--------------------------------------------------------------------------------------------------
/**
 * IP Pass Through operation type
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_IPPT_UNKNOWN = -1,   ///< Unknown operation.
    TAF_PA_VLAN_IPPT_DISABLE = 0,  ///< Disable IP pass through.
    TAF_PA_VLAN_IPPT_ENABLE = 1   ///< Enable IP pass through.
} taf_pa_vlan_ippt_operation_t;

//--------------------------------------------------------------------------------------------------
/**
 * IP Pass Through input parameters (for identifying the configuration)
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint32_t profileId;                      ///< Profile ID
    uint8_t slotId;                          ///< Slot ID
    uint16_t vlanId;                         ///< VLAN ID
} taf_pa_IpptConfigIn_t;

//--------------------------------------------------------------------------------------------------
/**
 * IP Pass Through output parameters (configuration data)
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    taf_pa_vlan_ippt_operation_t operation;  ///< Operation (enable/disable)
    taf_pa_vlan_iface_type_t ifType;         ///< Interface type
    char macAddr[TAF_PA_NET_MAC_ADDR_MAX_LEN+1];     ///< MAC address (format: XX:XX:XX:XX:XX:XX)
} taf_pa_IpptConfigOut_t;

PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetIPPassThroughConfig
(
    const taf_pa_IpptConfigIn_t *ipptConfigIn,    // IN
    const taf_pa_IpptConfigOut_t *ipptConfigOut   // IN
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetIPPassThroughConfig
(
    const taf_pa_IpptConfigIn_t *ipptConfigIn,    // IN
    taf_pa_IpptConfigOut_t *ipptConfigOut         // OUT
);

//--------------------------------------------------------------------------------------------------
/**
 * IP assignment operation type
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_IP_OPR_UNKNOWN = -1,   ///< Unknown operation.
    TAF_PA_VLAN_IP_OPR_ENABLE = 0,      ///< Add IP configuration.
    TAF_PA_VLAN_IP_OPR_DISABLE = 1,   ///< Delete IP configuration.
    TAF_PA_VLAN_IP_OPR_RECONFIGURE = 2   ///< Unknown operation.
} taf_pa_vlan_ip_assign_operation_t;

//--------------------------------------------------------------------------------------------------
/**
 * IP assignment type
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_IP_UNKNOWN = -1,   ///< Unknown IP assignment type.
    TAF_PA_VLAN_STATIC_IP = 0,   ///< Static IP assignment.
    TAF_PA_VLAN_DYNAMIC_IP = 1  ///< Dynamic IP assignment.
} taf_pa_vlan_ip_assign_type_t;

//--------------------------------------------------------------------------------------------------
/**
 * IP family type
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_VLAN_IPV4 = 0,  ///< IPv4 family.
    TAF_PA_VLAN_IPV6 = 1   ///< IPv6 family.
} taf_pa_vlan_ip_family_type_t;

//--------------------------------------------------------------------------------------------------
/**
 * The IP address maximum length.
 */
//--------------------------------------------------------------------------------------------------
#define IP_PA_NET_ADDR_MAX_LEN 46

//--------------------------------------------------------------------------------------------------
/**
 * IP address information structure
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char interfaceAddress[IP_PA_NET_ADDR_MAX_LEN];      ///< Interface IP address
    uint8_t interfaceMask;          ///< Interface subnet mask
    char gwAddress[IP_PA_NET_ADDR_MAX_LEN];             ///< Gateway address
    char primaryDnsAddress[IP_PA_NET_ADDR_MAX_LEN];     ///< Primary DNS address
    char secondaryDnsAddress[IP_PA_NET_ADDR_MAX_LEN];   ///< Secondary DNS address
} taf_pa_IpAddressInfo_t;

//--------------------------------------------------------------------------------------------------
/**
 * IP configuration parameters
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    taf_pa_vlan_ip_assign_operation_t ipOpr;  ///< IP operation (enable/disable/reconfigure)
    taf_pa_vlan_ip_assign_type_t ipAssignType; ///< IP assignment type (static/dynamic)
    taf_pa_IpAddressInfo_t ipAddrInfo;         ///< IP address information (for static IP)
} taf_pa_IpConfig_t;

//--------------------------------------------------------------------------------------------------
/**
 * IP configuration input parameters
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    taf_pa_vlan_iface_type_t ifType;           ///< Interface type
    uint16_t vlanId;                           ///< VLAN ID
    taf_pa_vlan_ip_family_type_t ipFamilyType; ///< IP family type (IPv4/IPv6)
} taf_pa_IpConfigParams_t;

PA_SHARED PA_WEAK pa_result_t taf_pa_net_SetIPConfig
(
    const taf_pa_IpConfigParams_t *ipConfigParams,  // IN
    const taf_pa_IpConfig_t *ipConfig               // IN
);

PA_SHARED PA_WEAK pa_result_t taf_pa_net_GetIPConfig
(
    const taf_pa_IpConfigParams_t *ipConfigParams,  // IN
    taf_pa_IpConfig_t *ipConfig                     // OUT
);

#endif /* TAF_VLAN_PA_HPP */
