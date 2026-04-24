/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NAT_PA_HPP
#define TAF_NAT_PA_HPP

#ifdef TAF_PA_DEFAULT
#define PA_WEAK __attribute__((weak))
#else
#define PA_WEAK
#endif

#include "tafCommonPa.h"

#include <vector>

//--------------------------------------------------------------------------------------------------
/**
 * NAT configuration structure for PA layer
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char addr[46];           ///< IP address (IPv4 or IPv6)
    uint16_t port;           ///< Private port
    uint16_t globalPort;     ///< Global port
    uint8_t proto;           ///< IP protocol number
} taf_pa_net_NatConfig_t;

typedef enum
{
    TAF_PA_NET_IPPROTO_TCP = 6,
    TAF_PA_NET_IPPROTO_UDP = 17
}
taf_pa_net_ipproto_t;


//--------------------------------------------------------------------------------------------------
/**
 * Initialize NAT platform adapter
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_nat_Init();

//--------------------------------------------------------------------------------------------------
/**
 * Add a destination NAT entry
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_nat_AddDestNatEntry(
    uint32_t profileId,
    uint8_t slotId,
    const taf_pa_net_NatConfig_t *natConfig
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove a destination NAT entry
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_nat_RemoveDestNatEntry(
    uint32_t profileId,
    uint8_t slotId,
    const taf_pa_net_NatConfig_t *natConfig
);

//--------------------------------------------------------------------------------------------------
/**
 * Query destination NAT entry list for a profile
 *
 * @return
 *      - LE_OK on success
 *      - LE_FAULT on failure
 *      - LE_TIMEOUT on timeout
 */
//--------------------------------------------------------------------------------------------------
PA_SHARED PA_WEAK pa_result_t taf_pa_nat_QueryDestNatEntryList(
    uint32_t profileId,
    uint8_t slotId,
    std::vector<taf_pa_net_NatConfig_t> &natEntryInfo
);

#endif /* TAF_NAT_PA_HPP */

