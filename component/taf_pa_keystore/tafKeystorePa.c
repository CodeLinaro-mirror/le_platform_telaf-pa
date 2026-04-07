/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafKeystorePa.h"
#include "taf_prop_keystore.h"

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
    return taf_prop_ks_Init();
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
