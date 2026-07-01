/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafKeystorePa.h"
#include "taf_prop_keystore.h"
#include "taf_prop_file.h"
#include "tafFilePa.h"
#include <stdatomic.h>
#include <stdbool.h>

// Thread-safe initialization flag
static _Atomic(bool) g_keystore_initialized = false;
static _Atomic(bool) g_file_vtable_initialized = false;

// Forward declarations for file functions (defined in taf_pa_file.c)
//--------------------------------------------------------------------------------------------------
/**
 * The file vtable with actual file function pointers
 * This will be injected into the noship keystore component
 */
//--------------------------------------------------------------------------------------------------
static const taf_prop_file_vtable_t g_file_vtable = {
    .abi_version = 1,
    .size = sizeof(taf_prop_file_vtable_t),

    // RFS File Operations
    .prop_open = taf_pa_file_Open,
    .prop_close = taf_pa_file_Close,
    .prop_read = taf_pa_file_Read,
    .prop_write = taf_pa_file_Write,
    .prop_delete = taf_pa_file_Delete,
    .prop_copy = taf_pa_file_Copy,
    .prop_rename = taf_pa_file_Rename,
};

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 *
 * @return
 *      LE_OK if successful.
 *      LE_FAULT if there was some other error.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_Init(void)
{
    PA_INFO("Telaf keyStore PA initializing ...");

    // Initialize RFS vtable injection first

    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_file_vtable_initialized, &expected, true))
    {
        PA_WARN("RFS vtable already initialized");
        return PA_OK;
    }

    PA_INFO("Injecting RFS vtable into keystore noship component");

    // Get the vtable from taf_pa_file.c and inject it into noship component
    taf_prop_file_vtable_Bind(&g_file_vtable);

    PA_INFO("RFS vtable injection completed successfully");

    // Initialize the proprietary keystore
    pa_result_t result = taf_prop_ks_Init();
    if (result == PA_OK)
    {
        atomic_store(&g_keystore_initialized, true);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * PA deinitialization.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_Deinit(void)
{
    // Check if initialization was successful before proceeding with deinitialization
    if (!atomic_load(&g_keystore_initialized))
    {
        PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return PA_FAULT;
    }

     if (!atomic_load(&g_file_vtable_initialized))
    {
        PA_WARN("RFS vtable not initialized - ignoring deinit request");
        return PA_FAULT;
    }

    PA_INFO("Unbinding RFS vtable from keystore noship component");

    // Unbind the vtable
    taf_prop_file_vtable_Bind(NULL);

    // Reset initialization flag
    atomic_store(&g_file_vtable_initialized, false);
    PA_INFO("RFS vtable unbinding completed successfully");

    atomic_store(&g_keystore_initialized, false);
    PA_INFO("Telaf keyStore PA deinitialized.");
    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA encryption key and return a key file reference
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GenerateRsaEncKey
(
    int clientSessionFd,
    const char* keyName,
    taf_pa_ks_RsaKeySize_t keySize,
    taf_pa_ks_EncPurpose_t purpose,
    taf_pa_ks_RsaEncPadding_t padding,
    const taf_pa_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateRsaEncKey(clientSessionFd, keyName, keySize, purpose, padding,
               tagListPtr, tagListSize, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA signature key and return a key file reference.
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GenerateRsaSigKey
(
    int clientSessionFd,
    const char* keyName,
    taf_pa_ks_RsaKeySize_t keySize,
    taf_pa_ks_SigPurpose_t purpose,
    taf_pa_ks_RsaSigPadding_t padding,
    const taf_pa_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateRsaSigKey(clientSessionFd, keyName, keySize, purpose, padding,
               tagListPtr, tagListSize, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an ECDSA key and return a key file reference.
 *
 * The impData must be PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GenerateEcdsaKey
(
    int clientSessionFd,
    const char* keyName,
    taf_pa_ks_EccKeySize_t keySize,
    taf_pa_ks_SigPurpose_t purpose,
    taf_pa_ks_Digest_t digest,
    const taf_pa_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateEcdsaKey(clientSessionFd, keyName, keySize, purpose, digest,
               tagListPtr, tagListSize, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an AES key and return a key file reference.
 *
 * The impData must be raw key bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GenerateAesKey
(
    int clientSessionFd,
    const char* keyName,
    taf_pa_ks_AesKeySize_t keySize,
    taf_pa_ks_EncPurpose_t purpose,
    taf_pa_ks_AesBlockMode_t mode,
    const taf_pa_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateAesKey(clientSessionFd, keyName, keySize, purpose, mode,
               tagListPtr, tagListSize, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a HMAC key and return a key file reference.
 *
 * Currently only digest DIGEST_SHA2_256 is supported. The impData must be raw key bytes if provided
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GenerateHmacKey
(
    int clientSessionFd,
    const char* keyName,
    uint32_t keySize,
    taf_pa_ks_SigPurpose_t purpose,
    taf_pa_ks_Digest_t digest,
    const taf_pa_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateHmacKey(clientSessionFd, keyName, keySize, purpose, digest,
               tagListPtr, tagListSize, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_ExportKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    const uint8_t* appDataPtr,
    size_t appDataSize,
    uint8_t* expDataPtr,
    size_t* expDataSizePtr
)
{
    return taf_prop_ks_ExportKey(clientSessionFd, keyFileRef, appDataPtr, appDataSize,
                                   expDataPtr, expDataSizePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Share a key.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_ShareKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_KeyUsage_t keyCap,
    uint32_t appCap,
    const char* appName
)
{
    return taf_prop_ks_ShareKey(clientSessionFd, keyFileRef, keyCap, appCap, appName);
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file by key name.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_DeleteKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef
)
{
    return taf_prop_ks_DeleteKey(clientSessionFd, keyFileRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GetKey
(
    int clientSessionFd,
    const char* keyName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GetKey(clientSessionFd, keyName, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared key file reference by key name and app name.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GetSharedKey
(
    int clientSessionFd,
    const char* keyName,
    const char* appName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GetSharedKey(clientSessionFd, keyName, appName, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_CancelKeySharing
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    const char* appName
)
{
    return taf_prop_ks_CancelKeySharing(clientSessionFd, keyFileRef, appName);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared app list for a shared key.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GetSharedAppList
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_sharedAppList_t* appListPtr
)
{
    return taf_prop_ks_GetSharedAppList(clientSessionFd, keyFileRef, appListPtr);
}
//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_GetKeyUsage
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_KeyUsage_t* keyUsagePtr
)
{
    return taf_prop_ks_GetKeyUsage(clientSessionFd, keyFileRef, keyUsagePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_CryptoSessionStart
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_CryptoPurpose_t cryptoPurpose,
    const taf_pa_ks_Param_t** paramListPtr,
    size_t paramListSize,
    uint64_t* opHandlePtr
)
{
    return taf_prop_ks_CryptoSessionStart(clientSessionFd, keyFileRef, cryptoPurpose,
                                          paramListPtr, paramListSize, opHandlePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session started with CryptoSessionStart API for AES GCM mode.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_CryptoSessionProcessAead
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize
)
{
    return taf_prop_ks_CryptoSessionProcessAead(opHandle, inputDataPtr, inputDataSize);
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides data to, and possibly receives output from, a running crypto operation.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_CryptoSessionProcess
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize,
    uint8_t* outputDataPtr,
    size_t* outputDataSizePtr
)
{
    return taf_prop_ks_CryptoSessionProcess(opHandle, inputDataPtr, inputDataSize, outputDataPtr, outputDataSizePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Finalizes and stops a crypto operation session.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_CryptoSessionEnd
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize,
    uint8_t* outputDataPtr,
    size_t* outputDataSizePtr
)
{
    return taf_prop_ks_CryptoSessionEnd(opHandle, inputDataPtr, inputDataSize, outputDataPtr, outputDataSizePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort crypto operation session.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_CryptoSessionAbort
(
    uint64_t opHandle
)
{
    return taf_prop_ks_CryptoSessionAbort(opHandle);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register Key creation handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_RegKeyCreationHandler
(
    taf_pa_ks_KeyCreationHandler_t handlerFunc
)
{
    return taf_prop_ks_RegKeyCreationHandler(handlerFunc);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register Key sharing state change handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_ks_RegKeySharingHandler
(
    taf_pa_ks_KeySharingHandler_t handlerFunc
)
{
    return taf_prop_ks_RegKeySharingHandler(handlerFunc);
}
