/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_FSCRYPT_H
#define TAF_PROP_FSCRYPT_H

#include "legato.h"
#include "taf_pa_fscrypt.h"

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_fsc_GetKey
(
    le_msg_SessionRef_t clientSessionRef,   ///< [IN] Client session reference
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
LE_SHARED le_result_t taf_prop_fsc_GenerateAesKey
(
    le_msg_SessionRef_t clientSessionRef,   ///< [IN] Client session reference
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
LE_SHARED le_result_t taf_prop_fsc_DeleteKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef        ///< [IN] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * The FSCrypt initialization function.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void taf_prop_fsc_Init
(
    void* cryptoFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * The FSCrypt component initialization function.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void taf_prop_fsc_Component_Init
(
    void
);

#endif // TAF_PROP_FSCRYPT_H
