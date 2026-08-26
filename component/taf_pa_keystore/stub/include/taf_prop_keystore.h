/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_KEYSTORAGE_H
#define TAF_PROP_KEYSTORAGE_H

#include "taf_ns_common.h"
#include "tafKeystorePa.h"

//--------------------------------------------------------------------------------------------------
/**
 * PROP component initialization.
 *
 * @return
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED void taf_prop_ks_Component_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 *
 * @return
 *      LE_OK if successful.
 *      LE_FAULT if there was some other error.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA encryption key and return a key file reference
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GenerateRsaEncKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    taf_pa_ks_RsaKeySize_t keySize,       ///< [IN] Key Size, ignored if impData is provided
    taf_pa_ks_EncPurpose_t purpose,       ///< [IN] Encryption purpose
    taf_pa_ks_RsaEncPadding_t padding,    ///< [IN] RSA encryption padding type
    const taf_pa_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_pa_ks_Tag_t
    size_t tagListSize,                   ///< [IN] number of taf_pa_ks_Tag_t
    const uint8_t* impDataPtr,            ///< [IN] Imported key data
    size_t impDataSize,                   ///< [IN] less than TAF_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA signature key and return a key file reference.
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GenerateRsaSigKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    taf_pa_ks_RsaKeySize_t keySize,       ///< [IN] Key Size, ignored if impData is provided
    taf_pa_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_pa_ks_RsaSigPadding_t padding,    ///< [IN] RSA signature padding type
    const taf_pa_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_pa_ks_Tag_t
    size_t tagListSize,                   ///< [IN] number of taf_pa_ks_Tag_t
    const uint8_t* impDataPtr,            ///< [IN] Imported key data
    size_t impDataSize,                   ///< [IN] less than TAF_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an ECDSA key and return a key file reference.
 *
 * The impData must be PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GenerateEcdsaKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    taf_pa_ks_EccKeySize_t keySize,       ///< [IN] ECC curve, ignored if impData is provided
    taf_pa_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_pa_ks_Digest_t digest,            ///< [IN] Digest
    const taf_pa_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_pa_ks_Tag_t
    size_t tagListSize,                   ///< [IN] number of taf_pa_ks_Tag_t
    const uint8_t* impDataPtr,            ///< [IN] Imported key data
    size_t impDataSize,                   ///< [IN] less than TAF_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an AES key and return a key file reference.
 *
 * The impData must be raw key bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GenerateAesKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    taf_pa_ks_AesKeySize_t keySize,       ///< [IN] AES key size, ignored if impData is provided
    taf_pa_ks_EncPurpose_t purpose,       ///< [IN] Encryption purpose
    taf_pa_ks_AesBlockMode_t mode,        ///< [IN] AES block mode
    const taf_pa_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_pa_ks_Tag_t
    size_t tagListSize,                   ///< [IN] number of taf_pa_ks_Tag_t
    const uint8_t* impDataPtr,            ///< [IN] Imported key data
    size_t impDataSize,                   ///< [IN] less than TAF_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a HMAC key and return a key file reference.
 *
 * Currently only digest DIGEST_SHA2_256 is supported. The impData must be raw key bytes if provided
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GenerateHmacKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    uint32_t keySize,                     ///< [IN] HMAC Key Size, ignored if impData is provided
    taf_pa_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_pa_ks_Digest_t digest,            ///< [IN] digest
    const taf_pa_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_pa_ks_Tag_t
    size_t tagListSize,                   ///< [IN] number of taf_pa_ks_Tag_t
    const uint8_t* impDataPtr,            ///< [IN] Imported key data
    size_t impDataSize,                   ///< [IN] less than TAF_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_ExportKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    const uint8_t* appDataPtr,            ///< [IN] Application data
    size_t appDataSize,                   ///< [IN]
    uint8_t* expDataPtr,                  ///< [OUT] exported key data
    size_t* expDataSizePtr                ///< [INOUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Share a key.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_ShareKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_pa_ks_KeyUsage_t keyCap,          ///< [IN] Shared capability
    uint32_t appCap,                      ///< [IN] Shared app capability.
    const char* appName                   ///< [IN] Shared application name
);

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_CancelKeySharing
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    const char* appName                   ///< [IN] Shared application name
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file by key name.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_DeleteKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef        ///< [IN] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GetKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference of a shared key by key name and app name.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GetSharedKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    const char* keyName,                  ///< [IN] Key Name
    const char* appName,                  ///< [IN] App Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared app list for a shared key.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GetSharedAppList
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_pa_ks_sharedAppList_t* appListPtr ///< [OUT] Shared app list.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_GetKeyUsage
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_pa_ks_KeyUsage_t*    keyUsagePtr  ///< [OUT] Key usage
);

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_CryptoSessionStart
(
    int clientSessionFd,                     ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t     keyFileRef,      ///< [IN] Key file reference
    taf_pa_ks_CryptoPurpose_t  cryptoPurpose,///< [IN] Crypto purpose
    const taf_pa_ks_Param_t** paramListPtr,  ///< [IN] List of taf_pa_ks_Param_t
    size_t paramListSize,                    ///< [IN] number of taf_pa_ks_Param_t
    uint64_t*                 opHandlePtr    ///< [OUT]Cyrpto operation handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session started with CryptoSessionStart API for AES GCM
 * mode.
 *
 * This API can be called for multiple times but must before CryptoSessionProcess API.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_CryptoSessionProcessAead
(
    uint64_t               opHandle,      ///< [IN] Cyrpto operation handle
    const uint8_t*     inputDataPtr,      ///< [IN] Data buffer to hold the AEAD data
    size_t            inputDataSize       ///< [IN]

);

//--------------------------------------------------------------------------------------------------
/**
 * Provides data to, and possibly receives output from, an runing crypto operation started with
 * CryptoStartSession API. It can be called for multiple times to support streaming mode until
 * CryptoEndSession API is called.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_CryptoSessionProcess
(
    uint64_t               opHandle,      ///< [IN] Cyrpto operation handle
    const uint8_t*     inputDataPtr,      ///< [IN] InputData can be one of below 4 cases:
                                          ///<      1: plain text for encryption session.
                                          ///<      2: cipher text for decryption session.
                                          ///<      3: message to sign for signing session.
                                          ///<      4: message to verify for verification session.
    size_t            inputDataSize,      ///< [IN]
    uint8_t*          outputDataPtr,      ///< [OUT] OutputData can be one of below 3 cases:
                                          ///<       1: encrypted data for encryption session.
                                          ///<       2: decrypted data for decryption session.
                                          ///<       3: ignore for signing and verification session.
    size_t*        outputDataSizePtr      ///< [INOUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Finalizes and stop a crypto operation session started with CryptoStartSession API.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_CryptoSessionEnd
(
    uint64_t               opHandle,      ///< [IN] Cyrpto operation handle
    const uint8_t*     inputDataPtr,      ///< [IN] Signature to verify for verification session
                                          ///<      and ignored for other sessions.
    size_t            inputDataSize,      ///< [IN]
    uint8_t*          outputDataPtr,      ///< [OUT] OutputData can be one of below 4 cases:
                                          ///<       1: encrypted data for encryption session.
                                          ///<       2: decrypted data for decryption session.
                                          ///<       3: signature for signing session.
                                          ///<       4: ignore for verfication session.
    size_t*        outputDataSizePtr      ///< [INOUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Abort crypto operation session started with CryptoStartSession API.
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_CryptoSessionAbort
(
    uint64_t                opHandle      ///< [IN] Cyrpto operation handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Register Key creation handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_RegKeyCreationHandler
(
    taf_pa_ks_KeyCreationHandler_t handlerFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * Register Key sharing state change handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
NS_SHARED ns_result_t taf_prop_ks_RegKeySharingHandler
(
    taf_pa_ks_KeySharingHandler_t handlerFunc
);

#endif // TAF_PROP_KEYSTORAGE_H
