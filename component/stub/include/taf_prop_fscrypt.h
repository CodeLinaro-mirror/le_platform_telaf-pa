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

#ifndef TAF_PROP_FSCRYPT_H
#define TAF_PROP_FSCRYPT_H

#include <stdint.h>
#include <stddef.h>
#include "taf_prop_common.h"

/* Opaque key-file reference — mirrors the definition in tafKeystorePa.h without
 * pulling in the full OSS PA header. The guard prevents redefinition if a
 * translation unit also includes tafKeystorePa.h directly. */
#ifndef KEYMGT_KEY_FILE_REF_T_DEFINED
#define KEYMGT_KEY_FILE_REF_T_DEFINED
typedef void* KeyMgt_KeyFileRef_t;
#endif

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_fsc_GetKey
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_fsc_GenerateAesKey
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * The FSCrypt initialization function.
 *
 * @return TAF_PROP_OK on success, or a taf_prop_result_t error code on failure.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_fsc_Init
(
    void* cryptoFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * The FSCrypt component initialization function.
 *
 * @return TAF_PROP_OK on success, or a taf_prop_result_t error code on failure.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_fsc_Component_Init
(
    void
);

#endif // TAF_PROP_FSCRYPT_H
