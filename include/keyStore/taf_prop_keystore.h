/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_KEYSTORAGE_H
#define TAF_PROP_KEYSTORAGE_H

#include "legato.h"
#include "taf_pa_keystore.h"

//--------------------------------------------------------------------------------------------------
/**
 * PROP component initialization.
 *
 * @return
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void taf_prop_ks_Component_Init
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
LE_SHARED le_result_t taf_prop_ks_Init
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
LE_SHARED le_result_t taf_prop_ks_GenerateRsaEncKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    taf_ks_RsaKeySize_t keySize,          ///< [IN] Key Size, ignored if impData is provided
    taf_pa_ks_EncPurpose_t purpose,       ///< [IN] Encryption purpose
    taf_ks_RsaEncPadding_t padding,       ///< [IN] RSA encryption padding type
    le_dls_List_t* tagListPtr,            ///< [IN] List of taf_prop_ks_Tag_t
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
LE_SHARED le_result_t taf_prop_ks_GenerateRsaSigKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    taf_ks_RsaKeySize_t keySize,          ///< [IN] Key Size, ignored if impData is provided
    taf_pa_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_ks_RsaSigPadding_t padding,       ///< [IN] RSA signature padding type
    le_dls_List_t* tagListPtr,            ///< [IN] List of taf_prop_ks_Tag_t
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
LE_SHARED le_result_t taf_prop_ks_GenerateEcdsaKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    taf_ks_EccKeySize_t keySize,          ///< [IN] ECC curve, ignored if impData is provided
    taf_pa_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_ks_Digest_t digest,               ///< [IN] Digest
    le_dls_List_t* tagListPtr,            ///< [IN] List of taf_prop_ks_Tag_t
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
LE_SHARED le_result_t taf_prop_ks_GenerateAesKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    taf_ks_AesKeySize_t keySize,          ///< [IN] AES key size, ignored if impData is provided
    taf_pa_ks_EncPurpose_t purpose,       ///< [IN] Encryption purpose
    taf_ks_AesBlockMode_t mode,           ///< [IN] AES block mode
    le_dls_List_t* tagListPtr,            ///< [IN] List of taf_prop_ks_Tag_t
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
LE_SHARED le_result_t taf_prop_ks_GenerateHmacKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    uint32_t keySize,                     ///< [IN] HMAC Key Size, ignored if impData is provided
    taf_pa_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_ks_Digest_t digest,               ///< [IN] digest
    le_dls_List_t* tagListPtr,            ///< [IN] List of taf_prop_ks_Tag_t
    const uint8_t* impDataPtr,            ///< [IN] Imported key data
    size_t impDataSize,                   ///< [IN] less than TAF_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_ExportKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
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
LE_SHARED le_result_t taf_prop_ks_ShareKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_ks_KeyUsage_t keyCap,             ///< [IN] Shared capability
    taf_ks_AppCapMask_t appCap,           ///< [IN] Shared app capability.
    const char* appName                   ///< [IN] Shared application name
);

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_CancelKeySharing
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    const char* appName                   ///< [IN] Shared application name
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file by key name.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_DeleteKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef        ///< [IN] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_GetKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference of a shared key by key name and app name.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_GetSharedKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    const char* keyName,                  ///< [IN] Key Name
    const char* appName,                  ///< [IN] App Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr    ///< [OUT] Key file reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared app list for a shared key.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_GetSharedAppList
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_pa_ks_sharedAppList_t* appListPtr ///< [OUT] Shared app list.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_GetKeyUsage
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_ks_KeyUsage_t*    keyUsagePtr     ///< [OUT] Key usage
);

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_CryptoSessionStart
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t     keyFileRef,   ///< [IN] Key file reference
    taf_ks_CryptoPurpose_t  cryptoPurpose,///< [IN] Crypto purpose
    le_dls_List_t*           paramListPtr,///< [IN] List of taf_prop_ks_Param_t
    uint64_t*                 opHandlePtr ///< [OUT]Cyrpto operation handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session started with CryptoSessionStart API for AES GCM
 * mode.
 *
 * This API can be called for multiple times but must before CryptoSessionProcess API.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_CryptoSessionProcessAead
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
LE_SHARED le_result_t taf_prop_ks_CryptoSessionProcess
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
LE_SHARED le_result_t taf_prop_ks_CryptoSessionEnd
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
LE_SHARED le_result_t taf_prop_ks_CryptoSessionAbort
(
    uint64_t                opHandle      ///< [IN] Cyrpto operation handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Register Key creation handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_RegKeyCreationHandler
(
    taf_pa_ks_KeyCreationHandler_t handlerFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * Register Key sharing state change handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_ks_RegKeySharingHandler
(
    taf_pa_ks_KeySharingHandler_t handlerFunc
);

#endif // TAF_PROP_KEYSTORAGE_H
