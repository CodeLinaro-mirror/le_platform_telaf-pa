/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_FSCRYPT_PA_H
#define TAF_FSCRYPT_PA_H

#include "tafCommonPa.h"

//--------------------------------------------------------------------------------------------------
/**
 * FS-Crypt key definitions
 */
//--------------------------------------------------------------------------------------------------
#define FSC_MAX_KEY_SIZE 64

//--------------------------------------------------------------------------------------------------
/**
 * Reference to a key file object
 */
//--------------------------------------------------------------------------------------------------
typedef void* KeyMgt_KeyFileRef_t;

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_Init
(
    void* cryptoFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * PA deinitialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_Deinit
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_GetKey
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
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_GenerateAesKey
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
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
);

#endif // TAF_FSCRYPT_PA_H
