/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_fscrypt.h"
#include "tafInternalCommonPa.h"
#include <atomic>

#ifdef __cplusplus
extern "C" {
#endif

// Thread-safe initialization flag
static std::atomic<bool> g_fscrypt_initialized{false};

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_Init
(
    void* cryptoFunc
)
{
    if (cryptoFunc == NULL)
    {
        TAF_PA_ERROR("taf_pa_fsc_Init: cryptoFunc is NULL.");
        return TAF_PA_BAD_PARAMETER;
    }

    taf_prop_fsc_Component_Init();
    taf_prop_fsc_Init(cryptoFunc);

    g_fscrypt_initialized.store(true);
    TAF_PA_INFO("Telaf fscrypt PA initialized.");
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * PA deinitialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_Deinit
(
    void
)
{
    // Check if initialization was successful before proceeding with deinitialization
    if (!g_fscrypt_initialized.load())
    {
        TAF_PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return TAF_PA_FAULT;
    }

    // The fscrypt PA is a stateless pass-through wrapper: it holds no shared pointers,
    // maps, or open handles of its own. The underlying taf_prop_fscrypt layer does not
    // expose a Deinit API. This function provides the symmetric counterpart to
    // taf_pa_fsc_Init() so callers can follow a consistent Init/Deinit lifecycle.
    g_fscrypt_initialized.store(false);
    TAF_PA_INFO("Telaf fscrypt PA deinitialized.");
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by directory name.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_GetKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference.
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    taf_prop_result_t rc = taf_prop_fsc_GetKey(clientSessionFd, dirName, keyFileRefPtr, key,
                                               keyLen);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

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
)
{
    taf_prop_result_t rc = taf_prop_fsc_GenerateAesKey(clientSessionFd, dirName, keyFileRefPtr,
                                                       key, keyLen);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
)
{
    taf_prop_result_t rc = taf_prop_fsc_DeleteKey(clientSessionFd, keyFileRef);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}


#ifdef __cplusplus
}
#endif // extern "C"
