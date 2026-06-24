/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "legato.h"
#include "interfaces.h"
#include "taf_pa_keystore.h"
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
le_result_t taf_pa_ks_Init(void)
{
    LE_INFO("Telaf keyStore PA initializing ...");

    // Initialize RFS vtable injection first

    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_file_vtable_initialized, &expected, true))
    {
        LE_WARN("RFS vtable already initialized");
        return LE_OK;
    }

    LE_INFO("Injecting RFS vtable into keystore noship component");

    // Get the vtable from taf_pa_file.c and inject it into noship component
    taf_prop_file_vtable_Bind(&g_file_vtable);

    LE_INFO("RFS vtable injection completed successfully");

    // Initialize the proprietary keystore
    le_result_t result = taf_prop_ks_Init();
    if (result == LE_OK)
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
le_result_t taf_pa_ks_Deinit(void)
{
    // Check if initialization was successful before proceeding with deinitialization
    if (!atomic_load(&g_keystore_initialized))
    {
        LE_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return LE_FAULT;
    }

     if (!atomic_load(&g_file_vtable_initialized))
    {
        LE_WARN("RFS vtable not initialized - ignoring deinit request");
        return LE_FAULT;
    }

    LE_INFO("Unbinding RFS vtable from keystore noship component");

    // Unbind the vtable
    taf_prop_file_vtable_Bind(NULL);

    // Reset initialization flag
    atomic_store(&g_file_vtable_initialized, false);
    LE_INFO("RFS vtable unbinding completed successfully");

    atomic_store(&g_keystore_initialized, false);
    LE_INFO("Telaf keyStore PA deinitialized.");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA encryption key and return a key file reference
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GenerateRsaEncKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    taf_ks_RsaKeySize_t keySize,
    taf_pa_ks_EncPurpose_t purpose,
    taf_ks_RsaEncPadding_t padding,
    le_dls_List_t* tagListPtr,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateRsaEncKey(clientSessionRef, keyName, keySize, purpose, padding,
                                           tagListPtr, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA signature key and return a key file reference.
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GenerateRsaSigKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    taf_ks_RsaKeySize_t keySize,
    taf_pa_ks_SigPurpose_t purpose,
    taf_ks_RsaSigPadding_t padding,
    le_dls_List_t* tagListPtr,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateRsaSigKey(clientSessionRef, keyName, keySize, purpose, padding,
                                           tagListPtr, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an ECDSA key and return a key file reference.
 *
 * The impData must be PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GenerateEcdsaKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    taf_ks_EccKeySize_t keySize,
    taf_pa_ks_SigPurpose_t purpose,
    taf_ks_Digest_t digest,
    le_dls_List_t* tagListPtr,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateEcdsaKey(clientSessionRef, keyName, keySize, purpose, digest,
                                          tagListPtr, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an AES key and return a key file reference.
 *
 * The impData must be raw key bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GenerateAesKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    taf_ks_AesKeySize_t keySize,
    taf_pa_ks_EncPurpose_t purpose,
    taf_ks_AesBlockMode_t mode,
    le_dls_List_t* tagListPtr,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateAesKey(clientSessionRef, keyName, keySize, purpose, mode,
                                        tagListPtr, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a HMAC key and return a key file reference.
 *
 * Currently only digest DIGEST_SHA2_256 is supported. The impData must be raw key bytes if provided
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GenerateHmacKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    uint32_t keySize,
    taf_pa_ks_SigPurpose_t purpose,
    taf_ks_Digest_t digest,
    le_dls_List_t* tagListPtr,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GenerateHmacKey(clientSessionRef, keyName, keySize, purpose, digest,
                                         tagListPtr, impDataPtr, impDataSize, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_pa_ks_ExportKey
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef,
    const uint8_t* appDataPtr,
    size_t appDataSize,
    uint8_t* expDataPtr,
    size_t* expDataSizePtr
)
{
    return taf_prop_ks_ExportKey(clientSessionRef, keyFileRef, appDataPtr, appDataSize,
                                   expDataPtr, expDataSizePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Share a key.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_pa_ks_ShareKey
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_ks_KeyUsage_t keyCap,
    taf_ks_AppCapMask_t appCap,
    const char* appName
)
{
    return taf_prop_ks_ShareKey(clientSessionRef, keyFileRef, keyCap, appCap, appName);
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file by key name.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_DeleteKey
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef
)
{
    return taf_prop_ks_DeleteKey(clientSessionRef, keyFileRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GetKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GetKey(clientSessionRef, keyName, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared key file reference by key name and app name.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GetSharedKey
(
    le_msg_SessionRef_t clientSessionRef,
    const char* keyName,
    const char* appName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return taf_prop_ks_GetSharedKey(clientSessionRef, keyName, appName, keyFileRefPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_CancelKeySharing
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef,
    const char* appName
)
{
    return taf_prop_ks_CancelKeySharing(clientSessionRef, keyFileRef, appName);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared app list for a shared key.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_pa_ks_GetSharedAppList
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_sharedAppList_t* appListPtr
)
{
    return taf_prop_ks_GetSharedAppList(clientSessionRef, keyFileRef, appListPtr);
}
//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_GetKeyUsage
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_ks_KeyUsage_t* keyUsagePtr
)
{
    return taf_prop_ks_GetKeyUsage(clientSessionRef, keyFileRef, keyUsagePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_CryptoSessionStart
(
    le_msg_SessionRef_t clientSessionRef,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_ks_CryptoPurpose_t cryptoPurpose,
    le_dls_List_t* paramListPtr,
    uint64_t* opHandlePtr
)
{
    return taf_prop_ks_CryptoSessionStart(clientSessionRef, keyFileRef, cryptoPurpose, paramListPtr, opHandlePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session started with CryptoSessionStart API for AES GCM mode.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_ks_CryptoSessionProcessAead
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
le_result_t taf_pa_ks_CryptoSessionProcess
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
le_result_t taf_pa_ks_CryptoSessionEnd
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
le_result_t taf_pa_ks_CryptoSessionAbort
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
le_result_t taf_pa_ks_RegKeyCreationHandler
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
le_result_t taf_pa_ks_RegKeySharingHandler
(
    taf_pa_ks_KeySharingHandler_t handlerFunc
)
{
    return taf_prop_ks_RegKeySharingHandler(handlerFunc);
}

//--------------------------------------------------------------------------------------------------
/**
 * The PA initialization function.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    taf_prop_ks_Component_Init();
    LE_INFO("Telaf keyStore PA initialized.");
}
