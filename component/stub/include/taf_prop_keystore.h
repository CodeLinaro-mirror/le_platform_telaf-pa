/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

/*
 * ABI STABILITY REQUIREMENT
 * This header is part of the prop interface.
 * All declarations must use C-style linkage only (no C++ classes,
 * templates, references, or overloaded functions) to guarantee ABI
 * stability across independently compiled shared libraries.
 */

#ifndef TAF_PROP_KEYSTORAGE_H
#define TAF_PROP_KEYSTORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "taf_prop_common.h"

/* ===== Constants ===== */

#define TAF_PROP_KS_MAX_SHARED_APPS    12
#define TAF_PROP_KS_MAX_APP_NAME_SIZE  128
#define TAF_PROP_KS_MAX_AES_NONCE_SIZE 16
#define TAF_PROP_KS_MAX_PACKET_SIZE    4096
#define TAF_PROP_KS_MIN_HMAC_KEY_SIZE  8
#define TAF_PROP_KS_MAX_HMAC_KEY_SIZE  128

#define TAF_PROP_KS_CAP_DELETE_KEY   0x1  ///< Shared app can delete the key.
#define TAF_PROP_KS_CAP_EXPORT_KEY   0x2  ///< Shared app can export the key.
#define TAF_PROP_KS_CAP_GET_APP_LIST 0x4  ///< Shared app can get shared application list.

/* ===== Opaque key-file reference ===== */

/* Guard allows co-existence with tafKeystorePa.h in the same translation unit */
#ifndef KEYMGT_KEY_FILE_REF_T_DEFINED
#define KEYMGT_KEY_FILE_REF_T_DEFINED
typedef void* KeyMgt_KeyFileRef_t;
#endif

/* ===== Enumerations ===== */

typedef enum
{
    TAF_PROP_KS_RSA_ENCRYPT_DECRYPT = 0,
    TAF_PROP_KS_RSA_ENCRYPT_ONLY    = 1,
    TAF_PROP_KS_RSA_DECRYPT_ONLY    = 2,
    TAF_PROP_KS_RSA_SIGN_VERIFY     = 3,
    TAF_PROP_KS_RSA_SIGN_ONLY       = 4,
    TAF_PROP_KS_RSA_VERIFY_ONLY     = 5,
    TAF_PROP_KS_AES_ENCRYPT_DECRYPT = 6,
    TAF_PROP_KS_AES_ENCRYPT_ONLY    = 7,
    TAF_PROP_KS_AES_DECRYPT_ONLY    = 8,
    TAF_PROP_KS_ECDSA_SIGN_VERIFY   = 9,
    TAF_PROP_KS_ECDSA_SIGN_ONLY     = 10,
    TAF_PROP_KS_ECDSA_VERIFY_ONLY   = 11,
    TAF_PROP_KS_HMAC_SIGN_VERIFY    = 12,
    TAF_PROP_KS_HMAC_SIGN_ONLY      = 13,
    TAF_PROP_KS_HMAC_VERIFY_ONLY    = 14,
    TAF_PROP_KS_KEYUSAGE_MAX        = 15,
} taf_prop_ks_KeyUsage_t;

typedef enum
{
    TAF_PROP_KS_SHARING_DISABLED = 0,
    TAF_PROP_KS_SHARING_ENABLED  = 1,
    TAF_PROP_KS_SHARING_UPDATED  = 2,
} taf_prop_ks_SharingState_t;

typedef enum
{
    TAF_PROP_KS_TAG_MAX_USES_PER_BOOT           = 0,
    TAF_PROP_KS_TAG_MIN_SECONDS_BETWEEN_OPS     = 1,
    TAF_PROP_KS_TAG_APPLICATION_DATA            = 2,
    TAF_PROP_KS_TAG_ACTIVE_DATETIME             = 3,
    TAF_PROP_KS_TAG_ORIGINATION_EXPIRE_DATETIME = 4,
    TAF_PROP_KS_TAG_USAGE_EXPIRE_DATETIME       = 5,
    TAF_PROP_KS_TAG_MAX_IDS                     = 6,
} taf_prop_ks_TagId_t;

typedef enum
{
    TAF_PROP_KS_PARAM_NONCE            = 0,
    TAF_PROP_KS_PARAM_APPLICATION_DATA = 1,
    TAF_PROP_KS_PARAM_RSA_PADDING_TYPE = 2,
    TAF_PROP_KS_PARAM_MAX_IDS          = 3,
} taf_prop_ks_ParamId_t;

typedef enum
{
    TAF_PROP_KS_AES_GCM_NONCE_SIZE = 12,
    TAF_PROP_KS_AES_CBC_NONCE_SIZE = 16,
    TAF_PROP_KS_AES_CTR_NONCE_SIZE = 16,
} taf_prop_ks_NonceSize_t;

typedef enum
{
    TAF_PROP_KS_RSA_SIZE_1024 = 1,
    TAF_PROP_KS_RSA_SIZE_2048 = 2,
    TAF_PROP_KS_RSA_SIZE_3072 = 3,
    TAF_PROP_KS_RSA_SIZE_4096 = 4,
    TAF_PROP_KS_RSA_SIZE_MAX  = 5,
} taf_prop_ks_RsaKeySize_t;

typedef enum
{
    TAF_PROP_KS_AES_SIZE_128 = 1,
    TAF_PROP_KS_AES_SIZE_192 = 2,
    TAF_PROP_KS_AES_SIZE_256 = 3,
    TAF_PROP_KS_AES_SIZE_MAX = 4,
} taf_prop_ks_AesKeySize_t;

typedef enum
{
    TAF_PROP_KS_ECC_SIZE_224 = 1,
    TAF_PROP_KS_ECC_SIZE_256 = 2,
    TAF_PROP_KS_ECC_SIZE_384 = 3,
    TAF_PROP_KS_ECC_SIZE_521 = 4,
    TAF_PROP_KS_ECC_SIZE_MAX = 5,
} taf_prop_ks_EccKeySize_t;

typedef enum
{
    TAF_PROP_KS_ENCRYPT_DECRYPT = 0,
    TAF_PROP_KS_ENCRYPT_ONLY    = 1,
    TAF_PROP_KS_DECRYPT_ONLY    = 2,
    TAF_PROP_KS_ENC_MAX,
} taf_prop_ks_EncPurpose_t;

typedef enum
{
    TAF_PROP_KS_SIGN_VERIFY = 0,
    TAF_PROP_KS_SIGN_ONLY   = 1,
    TAF_PROP_KS_VERIFY_ONLY = 2,
    TAF_PROP_KS_SIG_MAX,
} taf_prop_ks_SigPurpose_t;

typedef enum
{
    TAF_PROP_KS_RSA_PKCS1_V15 = 0,
    TAF_PROP_KS_RSA_PSS       = 1,
} taf_prop_ks_RsaPaddingType_t;

typedef enum
{
    TAF_PROP_KS_RSA_ENC_PAD_NONE          = 0,
    TAF_PROP_KS_RSA_ENC_PAD_PKCS1_V15     = 1,
    TAF_PROP_KS_RSA_ENC_PAD_OAEP_MD5      = 2,
    TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA1     = 3,
    TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_224 = 4,
    TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_256 = 5,
    TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_384 = 6,
    TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_512 = 7,
    TAF_PROP_KS_RSA_ENC_PAD_MAX            = 8,
} taf_prop_ks_RsaEncPadding_t;

typedef enum
{
    TAF_PROP_KS_RSA_SIG_PAD_NONE                      = 0,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_MD5             = 1,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA1            = 2,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_224        = 3,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_256        = 4,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_384        = 5,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_512        = 6,
    TAF_PROP_KS_RSA_SIG_PAD_PSS_MD5                   = 7,
    TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA1                  = 8,
    TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_224              = 9,
    TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_256              = 10,
    TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_384              = 11,
    TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_512              = 12,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_MD5     = 13,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA1    = 14,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_224 = 15,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_256 = 16,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_384 = 17,
    TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_512 = 18,
    TAF_PROP_KS_RSA_SIG_PAD_MAX                       = 19,
} taf_prop_ks_RsaSigPadding_t;

typedef enum
{
    TAF_PROP_KS_AES_MODE_ECB_PAD_NONE  = 0,
    TAF_PROP_KS_AES_MODE_ECB_PAD_PKCS7 = 1,
    TAF_PROP_KS_AES_MODE_CBC_PAD_NONE  = 2,
    TAF_PROP_KS_AES_MODE_CBC_PAD_PKCS7 = 3,
    TAF_PROP_KS_AES_MODE_CTR           = 4,
    TAF_PROP_KS_AES_MODE_GCM           = 5,
    TAF_PROP_KS_AES_MODE_MAX           = 6,
} taf_prop_ks_AesBlockMode_t;

typedef enum
{
    TAF_PROP_KS_CRYPTO_ENCRYPT = 0,
    TAF_PROP_KS_CRYPTO_DECRYPT = 1,
    TAF_PROP_KS_CRYPTO_SIGN    = 2,
    TAF_PROP_KS_CRYPTO_VERIFY  = 3,
    TAF_PROP_KS_CRYPTO_MAX     = 4,
} taf_prop_ks_CryptoPurpose_t;

typedef enum
{
    TAF_PROP_KS_DIGEST_MD5      = 1,
    TAF_PROP_KS_DIGEST_SHA1     = 2,
    TAF_PROP_KS_DIGEST_SHA2_224 = 3,
    TAF_PROP_KS_DIGEST_SHA2_256 = 4,
    TAF_PROP_KS_DIGEST_SHA2_384 = 5,
    TAF_PROP_KS_DIGEST_SHA2_512 = 6,
    TAF_PROP_KS_DIGEST_MAX      = 7,
} taf_prop_ks_Digest_t;

/* ===== Structs ===== */

typedef struct
{
    size_t  size;
    uint8_t data[TAF_PROP_KS_MAX_PACKET_SIZE];
} taf_prop_ks_Data_t;

typedef struct
{
    taf_prop_ks_NonceSize_t size;
    uint8_t                 data[TAF_PROP_KS_MAX_AES_NONCE_SIZE];
} taf_prop_ks_Nonce_t;

typedef struct
{
    taf_prop_ks_TagId_t id;
    union
    {
        uint32_t              maxUsesPerBoot;
        uint32_t              minSecondsBetweenOps;
        uint64_t              activeDateTime;
        uint64_t              originationExpireDateTime;
        uint64_t              usageExpireDateTime;
        taf_prop_ks_Data_t*   appDataPtr;
    };
} taf_prop_ks_Tag_t;

typedef struct
{
    taf_prop_ks_ParamId_t id;
    union
    {
        taf_prop_ks_Nonce_t*         nonceDataPtr;
        taf_prop_ks_Data_t*          appDataPtr;
        taf_prop_ks_RsaPaddingType_t rsaPaddingType;
    };
} taf_prop_ks_Param_t;

typedef struct
{
    taf_prop_ks_KeyUsage_t keyCap;
    uint32_t               appCap;
    char                   appName[TAF_PROP_KS_MAX_APP_NAME_SIZE + 1];
} taf_prop_ks_SharedApp_t;

typedef struct
{
    taf_prop_ks_SharedApp_t appInfo[TAF_PROP_KS_MAX_SHARED_APPS];
} taf_prop_ks_sharedAppList_t;

/* ===== Callback typedefs ===== */

typedef void (*taf_prop_ks_KeyCreationHandler_t)
(
    KeyMgt_KeyFileRef_t keyFileRef  ///< Key file reference
);

typedef void (*taf_prop_ks_KeySharingHandler_t)
(
    const char*                keyIdPtr,
    const char*                ownerAppNamePtr,
    const char*                sharedAppNamePtr,
    taf_prop_ks_SharingState_t state,
    KeyMgt_KeyFileRef_t        keyFileRef
);

/* ===== API ===== */

//--------------------------------------------------------------------------------------------------
/**
 * PROP component initialization.
 *
 * @return
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED void taf_prop_ks_Component_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_Init
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GenerateRsaEncKey
(
    int clientSessionFd,                    ///< [IN] Client session fd
    const char* keyName,                    ///< [IN] Key Name
    taf_prop_ks_RsaKeySize_t keySize,       ///< [IN] Key Size, ignored if impData is provided
    taf_prop_ks_EncPurpose_t purpose,       ///< [IN] Encryption purpose
    taf_prop_ks_RsaEncPadding_t padding,    ///< [IN] RSA encryption padding type
    const taf_prop_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_prop_ks_Tag_t
    size_t tagListSize,                     ///< [IN] number of taf_prop_ks_Tag_t
    const uint8_t* impDataPtr,              ///< [IN] Imported key data
    size_t impDataSize,                     ///< [IN] less than TAF_PROP_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr      ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA signature key and return a key file reference.
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GenerateRsaSigKey
(
    int clientSessionFd,                    ///< [IN] Client session fd
    const char* keyName,                    ///< [IN] Key Name
    taf_prop_ks_RsaKeySize_t keySize,       ///< [IN] Key Size, ignored if impData is provided
    taf_prop_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_prop_ks_RsaSigPadding_t padding,    ///< [IN] RSA signature padding type
    const taf_prop_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_prop_ks_Tag_t
    size_t tagListSize,                     ///< [IN] number of taf_prop_ks_Tag_t
    const uint8_t* impDataPtr,              ///< [IN] Imported key data
    size_t impDataSize,                     ///< [IN] less than TAF_PROP_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr      ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an ECDSA key and return a key file reference.
 *
 * The impData must be PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GenerateEcdsaKey
(
    int clientSessionFd,                    ///< [IN] Client session fd
    const char* keyName,                    ///< [IN] Key Name
    taf_prop_ks_EccKeySize_t keySize,       ///< [IN] ECC curve, ignored if impData is provided
    taf_prop_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_prop_ks_Digest_t digest,            ///< [IN] Digest
    const taf_prop_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_prop_ks_Tag_t
    size_t tagListSize,                     ///< [IN] number of taf_prop_ks_Tag_t
    const uint8_t* impDataPtr,              ///< [IN] Imported key data
    size_t impDataSize,                     ///< [IN] less than TAF_PROP_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr      ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an AES key and return a key file reference.
 *
 * The impData must be raw key bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GenerateAesKey
(
    int clientSessionFd,                    ///< [IN] Client session fd
    const char* keyName,                    ///< [IN] Key Name
    taf_prop_ks_AesKeySize_t keySize,       ///< [IN] AES key size, ignored if impData is provided
    taf_prop_ks_EncPurpose_t purpose,       ///< [IN] Encryption purpose
    taf_prop_ks_AesBlockMode_t mode,        ///< [IN] AES block mode
    const taf_prop_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_prop_ks_Tag_t
    size_t tagListSize,                     ///< [IN] number of taf_prop_ks_Tag_t
    const uint8_t* impDataPtr,              ///< [IN] Imported key data
    size_t impDataSize,                     ///< [IN] less than TAF_PROP_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr      ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a HMAC key and return a key file reference.
 *
 * Currently only digest DIGEST_SHA2_256 is supported. The impData must be raw key bytes if provided
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GenerateHmacKey
(
    int clientSessionFd,                    ///< [IN] Client session fd
    const char* keyName,                    ///< [IN] Key Name
    uint32_t keySize,                       ///< [IN] HMAC Key Size, ignored if impData is provided
    taf_prop_ks_SigPurpose_t purpose,       ///< [IN] Signature purpose
    taf_prop_ks_Digest_t digest,            ///< [IN] digest
    const taf_prop_ks_Tag_t** tagListPtr,   ///< [IN] List of taf_prop_ks_Tag_t
    size_t tagListSize,                     ///< [IN] number of taf_prop_ks_Tag_t
    const uint8_t* impDataPtr,              ///< [IN] Imported key data
    size_t impDataSize,                     ///< [IN] less than TAF_PROP_KS_MAX_PACKET_SIZE
    KeyMgt_KeyFileRef_t* keyFileRefPtr      ///< [OUT] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_ExportKey
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_ShareKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_prop_ks_KeyUsage_t keyCap,        ///< [IN] Shared capability
    uint32_t appCap,                      ///< [IN] Shared app capability.
    const char* appName                   ///< [IN] Shared application name
);

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_CancelKeySharing
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_DeleteKey
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef        ///< [IN] Key file reference
);

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GetKey
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GetSharedKey
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
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GetSharedAppList
(
    int clientSessionFd,                       ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,            ///< [IN] Key file reference
    taf_prop_ks_sharedAppList_t* appListPtr    ///< [OUT] Shared app list.
);

//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_GetKeyUsage
(
    int clientSessionFd,                  ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,       ///< [IN] Key file reference
    taf_prop_ks_KeyUsage_t* keyUsagePtr   ///< [OUT] Key usage
);

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_CryptoSessionStart
(
    int clientSessionFd,                        ///< [IN] Client session fd
    KeyMgt_KeyFileRef_t keyFileRef,             ///< [IN] Key file reference
    taf_prop_ks_CryptoPurpose_t cryptoPurpose,  ///< [IN] Crypto purpose
    const taf_prop_ks_Param_t** paramListPtr,   ///< [IN] List of taf_prop_ks_Param_t
    size_t paramListSize,                       ///< [IN] number of taf_prop_ks_Param_t
    uint64_t* opHandlePtr                       ///< [OUT] Cyrpto operation handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session started with CryptoSessionStart API for AES GCM
 * mode.
 *
 * This API can be called for multiple times but must before CryptoSessionProcess API.
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_CryptoSessionProcessAead
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
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_CryptoSessionProcess
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
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_CryptoSessionEnd
(
    uint64_t               opHandle,      ///< [IN] Cyrpto operation handle
    const uint8_t*     inputDataPtr,      ///< [IN] Signature to verify for verification session
                                          ///<      and ignored for other sessions.
    size_t            inputDataSize,      ///< [IN]
    uint8_t*          outputDataPtr,      ///< [OUT] OutputData can be one of below 4 cases:
                                          ///<       1: encrypted data for encryption session.
                                          ///<       2: decrypted data for decryption session.
                                          ///<       3: signature for signing session.
                                          ///<       4: ignore for verification session.
    size_t*        outputDataSizePtr      ///< [INOUT]
);

//--------------------------------------------------------------------------------------------------
/**
 * Abort crypto operation session started with CryptoStartSession API.
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_CryptoSessionAbort
(
    uint64_t                opHandle      ///< [IN] Cyrpto operation handle
);

//--------------------------------------------------------------------------------------------------
/**
 * Register Key creation handler in PA layer
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_RegKeyCreationHandler
(
    taf_prop_ks_KeyCreationHandler_t handlerFunc
);

//--------------------------------------------------------------------------------------------------
/**
 * Register Key sharing state change handler in PA layer
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_ks_RegKeySharingHandler
(
    taf_prop_ks_KeySharingHandler_t handlerFunc
);

#endif // TAF_PROP_KEYSTORAGE_H
