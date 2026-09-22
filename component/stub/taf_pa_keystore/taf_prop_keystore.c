/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_keystore.h"

//--------------------------------------------------------------------------------------------------
/**
 * PROP component initialization.
 *
 * @return
 */
//--------------------------------------------------------------------------------------------------
void taf_prop_ks_Component_Init
(
    void
)
{
    TAF_PROP_INFO("Telaf keyStore noship stub initialized.");
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_Init
(
    void
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA encryption key and return a key file reference
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GenerateRsaEncKey
(
    int clientSessionFd,
    const char* keyName,
    taf_prop_ks_RsaKeySize_t keySize,
    taf_prop_ks_EncPurpose_t purpose,
    taf_prop_ks_RsaEncPadding_t padding,
    const taf_prop_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA signature key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GenerateRsaSigKey
(
    int clientSessionFd,
    const char* keyName,
    taf_prop_ks_RsaKeySize_t keySize,
    taf_prop_ks_SigPurpose_t purpose,
    taf_prop_ks_RsaSigPadding_t padding,
    const taf_prop_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an ECDSA key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GenerateEcdsaKey
(
    int clientSessionFd,
    const char* keyName,
    taf_prop_ks_EccKeySize_t keySize,
    taf_prop_ks_SigPurpose_t purpose,
    taf_prop_ks_Digest_t digest,
    const taf_prop_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an AES key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GenerateAesKey
(
    int clientSessionFd,
    const char* keyName,
    taf_prop_ks_AesKeySize_t keySize,
    taf_prop_ks_EncPurpose_t purpose,
    taf_prop_ks_AesBlockMode_t mode,
    const taf_prop_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a HMAC key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GenerateHmacKey
(
    int clientSessionFd,
    const char* keyName,
    uint32_t keySize,
    taf_prop_ks_SigPurpose_t purpose,
    taf_prop_ks_Digest_t digest,
    const taf_prop_ks_Tag_t** tagListPtr,
    size_t tagListSize,
    const uint8_t* impDataPtr,
    size_t impDataSize,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_ExportKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    const uint8_t* appDataPtr,
    size_t appDataSize,
    uint8_t* expDataPtr,
    size_t* expDataSizePtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Share a key.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_ShareKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_prop_ks_KeyUsage_t keyCap,
    uint32_t appCap,
    const char* appName
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file by key name.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_DeleteKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GetKey
(
    int clientSessionFd,
    const char* keyName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared key file reference by key name and app name.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GetSharedKey
(
    int clientSessionFd,
    const char* keyName,
    const char* appName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_CancelKeySharing
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    const char* appName
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared app list for a shared key.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GetSharedAppList
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_prop_ks_sharedAppList_t* appListPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_GetKeyUsage
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_prop_ks_KeyUsage_t* keyUsagePtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_CryptoSessionStart
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_prop_ks_CryptoPurpose_t cryptoPurpose,
    const taf_prop_ks_Param_t** paramListPtr,
    size_t paramListSize,
    uint64_t* opHandlePtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_CryptoSessionProcessAead
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides data to, and possibly receives output from, a running crypto operation.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_CryptoSessionProcess
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize,
    uint8_t* outputDataPtr,
    size_t* outputDataSizePtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Finalizes and stops a crypto operation session.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_CryptoSessionEnd
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize,
    uint8_t* outputDataPtr,
    size_t* outputDataSizePtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort crypto operation session.
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_CryptoSessionAbort
(
    uint64_t opHandle
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register Key creation handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_RegKeyCreationHandler
(
    taf_prop_ks_KeyCreationHandler_t handlerFunc
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------------------------------
/**
 * Register Key sharing state change handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_ks_RegKeySharingHandler
(
    taf_prop_ks_KeySharingHandler_t handlerFunc
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}