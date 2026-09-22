/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NAT_PA_HPP
#define TAF_NAT_PA_HPP

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
 *      - TAF_PA_OK on success
 *      - TAF_PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_nat_Init();

//--------------------------------------------------------------------------------------------------
/**
 * Deinitialize NAT platform adapter
 *
 * @return
 *      - TAF_PA_OK on success
 *      - TAF_PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_nat_Deinit();

//--------------------------------------------------------------------------------------------------
/**
 * Add a destination NAT entry
 *
 * @return
 *      - TAF_PA_OK on success
 *      - TAF_PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_nat_AddDestNatEntry(
    uint32_t profileId,
    uint8_t slotId,
    const taf_pa_net_NatConfig_t *natConfig
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove a destination NAT entry
 *
 * @return
 *      - TAF_PA_OK on success
 *      - TAF_PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_nat_RemoveDestNatEntry(
    uint32_t profileId,
    uint8_t slotId,
    const taf_pa_net_NatConfig_t *natConfig
);

//--------------------------------------------------------------------------------------------------
/**
 * Query destination NAT entry list for a profile
 *
 * @return
 *      - TAF_PA_OK on success
 *      - TAF_PA_FAULT on failure
 *      - TAF_PA_TIMEOUT on timeout
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_nat_QueryDestNatEntryList(
    uint32_t profileId,
    uint8_t slotId,
    std::vector<taf_pa_net_NatConfig_t> &natEntryInfo
);

#endif /* TAF_NAT_PA_HPP */
