/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_fscrypt.h"

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_fsc_Init
(
    void* cryptoFunc
)
{
    TAF_PROP_INFO("Stub function.");
    return TAF_PROP_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by directory name.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_fsc_GetKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference.
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    TAF_PROP_INFO("Stub function.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create AES key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_fsc_GenerateAesKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    TAF_PROP_INFO("Stub function.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
)
{
    TAF_PROP_INFO("Stub function.");
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Component initialization.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_fsc_Component_Init
(
    void
)
{
    TAF_PROP_INFO("Stub function.");
    return TAF_PROP_OK;
}
