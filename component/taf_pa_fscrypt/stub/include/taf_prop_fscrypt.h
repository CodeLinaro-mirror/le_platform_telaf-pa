/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_FSCRYPT_H
#define TAF_PROP_FSCRYPT_H

#include "taf_prop_common.h"
#include <stddef.h>

typedef void* KeyMgt_KeyFileRef_t;

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
PROP_SHARED prop_result_t taf_prop_fsc_GetKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference.
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
);

//--------------------------------------------------------------------------------------------------
/**
 * Create AES key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
PROP_SHARED prop_result_t taf_prop_fsc_GenerateAesKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file.
 */
//--------------------------------------------------------------------------------------------------
PROP_SHARED prop_result_t taf_prop_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * The FSCrypt initialization function.
 */
//--------------------------------------------------------------------------------------------------
PROP_SHARED void taf_prop_fsc_Init
(
    void* cryptoFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * The FSCrypt component initialization function.
 */
//--------------------------------------------------------------------------------------------------
PROP_SHARED void taf_prop_fsc_Component_Init
(
    void
);

#endif // TAF_PROP_FSCRYPT_H
