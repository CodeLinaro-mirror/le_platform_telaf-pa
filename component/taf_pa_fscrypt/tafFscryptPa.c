/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafFscryptPa.h"
#include "taf_prop_fscrypt.h"
#include <stdatomic.h>
#include <stdbool.h>

// Thread-safe initialization flag
static _Atomic(bool) g_fscrypt_initialized = false;

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_fsc_Init
(
    void* cryptoFunc
)
{
    taf_prop_fsc_Component_Init();
    taf_prop_fsc_Init(cryptoFunc);
    atomic_store(&g_fscrypt_initialized, true);
    PA_INFO("Telaf fscrypt PA initialized.");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * PA deinitialization.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_fsc_Deinit
(
    void
)
{
    // Check if initialization was successful before proceeding with deinitialization
    if (!atomic_load(&g_fscrypt_initialized))
    {
        PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return PA_FAULT;
    }

    // The fscrypt PA is a stateless pass-through wrapper: it holds no shared pointers,
    // maps, or open handles of its own. The underlying taf_prop_fscrypt layer does not
    // expose a Deinit API. This function provides the symmetric counterpart to
    // taf_pa_fsc_Init() so callers can follow a consistent Init/Deinit lifecycle.
    atomic_store(&g_fscrypt_initialized, false);
    PA_INFO("Telaf fscrypt PA deinitialized.");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by directory name.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_fsc_GetKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference.
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    return taf_prop_fsc_GetKey(clientSessionFd, dirName, keyFileRefPtr, key, keyLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create AES key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_fsc_GenerateAesKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    return taf_prop_fsc_GenerateAesKey(clientSessionFd, dirName, keyFileRefPtr, key, keyLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
)
{
    return taf_prop_fsc_DeleteKey(clientSessionFd, keyFileRef);
}
