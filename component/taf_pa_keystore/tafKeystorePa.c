/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafKeystorePa.h"
#include "taf_prop_keystore.h"
#include "taf_prop_file.h"
#include "tafFilePa.h"
#include "tafInternalCommonPa.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

//--------------------------------------------------------------------------------------------------
// Type conversion functions: taf_pa_ks_* → taf_prop_ks_*
//
// Each function maps enum values explicitly via a switch statement so that any
// future divergence between the two type sets is caught at compile time (missing
// case) rather than silently at runtime.
//--------------------------------------------------------------------------------------------------

static taf_prop_ks_RsaKeySize_t ConvertRsaKeySize(taf_pa_ks_RsaKeySize_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_RSA_SIZE_1024: return TAF_PROP_KS_RSA_SIZE_1024;
        case TAF_PA_KS_RSA_SIZE_2048: return TAF_PROP_KS_RSA_SIZE_2048;
        case TAF_PA_KS_RSA_SIZE_3072: return TAF_PROP_KS_RSA_SIZE_3072;
        case TAF_PA_KS_RSA_SIZE_4096: return TAF_PROP_KS_RSA_SIZE_4096;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_RsaKeySize_t value %d", (int)paValue);
            return TAF_PROP_KS_RSA_SIZE_2048;
    }
}

static taf_prop_ks_EncPurpose_t ConvertEncPurpose(taf_pa_ks_EncPurpose_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_ENCRYPT_DECRYPT: return TAF_PROP_KS_ENCRYPT_DECRYPT;
        case TAF_PA_KS_ENCRYPT_ONLY:    return TAF_PROP_KS_ENCRYPT_ONLY;
        case TAF_PA_KS_DECRYPT_ONLY:    return TAF_PROP_KS_DECRYPT_ONLY;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_EncPurpose_t value %d", (int)paValue);
            return TAF_PROP_KS_ENCRYPT_DECRYPT;
    }
}

static taf_prop_ks_RsaEncPadding_t ConvertRsaEncPadding(taf_pa_ks_RsaEncPadding_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_RSA_ENC_PAD_NONE:          return TAF_PROP_KS_RSA_ENC_PAD_NONE;
        case TAF_PA_KS_RSA_ENC_PAD_PKCS1_V15:     return TAF_PROP_KS_RSA_ENC_PAD_PKCS1_V15;
        case TAF_PA_KS_RSA_ENC_PAD_OAEP_MD5:      return TAF_PROP_KS_RSA_ENC_PAD_OAEP_MD5;
        case TAF_PA_KS_RSA_ENC_PAD_OAEP_SHA1:     return TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA1;
        case TAF_PA_KS_RSA_ENC_PAD_OAEP_SHA2_224: return TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_224;
        case TAF_PA_KS_RSA_ENC_PAD_OAEP_SHA2_256: return TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_256;
        case TAF_PA_KS_RSA_ENC_PAD_OAEP_SHA2_384: return TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_384;
        case TAF_PA_KS_RSA_ENC_PAD_OAEP_SHA2_512: return TAF_PROP_KS_RSA_ENC_PAD_OAEP_SHA2_512;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_RsaEncPadding_t value %d", (int)paValue);
            return TAF_PROP_KS_RSA_ENC_PAD_NONE;
    }
}

static taf_prop_ks_SigPurpose_t ConvertSigPurpose(taf_pa_ks_SigPurpose_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_SIGN_VERIFY: return TAF_PROP_KS_SIGN_VERIFY;
        case TAF_PA_KS_SIGN_ONLY:   return TAF_PROP_KS_SIGN_ONLY;
        case TAF_PA_KS_VERIFY_ONLY: return TAF_PROP_KS_VERIFY_ONLY;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_SigPurpose_t value %d", (int)paValue);
            return TAF_PROP_KS_SIGN_VERIFY;
    }
}

static taf_prop_ks_RsaSigPadding_t ConvertRsaSigPadding(taf_pa_ks_RsaSigPadding_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_RSA_SIG_PAD_NONE:
            return TAF_PROP_KS_RSA_SIG_PAD_NONE;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_MD5:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_MD5;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_SHA1:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA1;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_224:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_224;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_256:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_256;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_384:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_384;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_512:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_SHA2_512;
        case TAF_PA_KS_RSA_SIG_PAD_PSS_MD5:
            return TAF_PROP_KS_RSA_SIG_PAD_PSS_MD5;
        case TAF_PA_KS_RSA_SIG_PAD_PSS_SHA1:
            return TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA1;
        case TAF_PA_KS_RSA_SIG_PAD_PSS_SHA2_224:
            return TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_224;
        case TAF_PA_KS_RSA_SIG_PAD_PSS_SHA2_256:
            return TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_256;
        case TAF_PA_KS_RSA_SIG_PAD_PSS_SHA2_384:
            return TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_384;
        case TAF_PA_KS_RSA_SIG_PAD_PSS_SHA2_512:
            return TAF_PROP_KS_RSA_SIG_PAD_PSS_SHA2_512;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_MD5:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_MD5;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA1:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA1;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_224:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_224;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_256:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_256;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_384:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_384;
        case TAF_PA_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_512:
            return TAF_PROP_KS_RSA_SIG_PAD_PKCS1_V15_AND_PSS_SHA2_512;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_RsaSigPadding_t value %d", (int)paValue);
            return TAF_PROP_KS_RSA_SIG_PAD_NONE;
    }
}

static taf_prop_ks_EccKeySize_t ConvertEccKeySize(taf_pa_ks_EccKeySize_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_ECC_SIZE_224: return TAF_PROP_KS_ECC_SIZE_224;
        case TAF_PA_KS_ECC_SIZE_256: return TAF_PROP_KS_ECC_SIZE_256;
        case TAF_PA_KS_ECC_SIZE_384: return TAF_PROP_KS_ECC_SIZE_384;
        case TAF_PA_KS_ECC_SIZE_521: return TAF_PROP_KS_ECC_SIZE_521;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_EccKeySize_t value %d", (int)paValue);
            return TAF_PROP_KS_ECC_SIZE_256;
    }
}

static taf_prop_ks_Digest_t ConvertDigest(taf_pa_ks_Digest_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_DIGEST_MD5:      return TAF_PROP_KS_DIGEST_MD5;
        case TAF_PA_KS_DIGEST_SHA1:     return TAF_PROP_KS_DIGEST_SHA1;
        case TAF_PA_KS_DIGEST_SHA2_224: return TAF_PROP_KS_DIGEST_SHA2_224;
        case TAF_PA_KS_DIGEST_SHA2_256: return TAF_PROP_KS_DIGEST_SHA2_256;
        case TAF_PA_KS_DIGEST_SHA2_384: return TAF_PROP_KS_DIGEST_SHA2_384;
        case TAF_PA_KS_DIGEST_SHA2_512: return TAF_PROP_KS_DIGEST_SHA2_512;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_Digest_t value %d", (int)paValue);
            return TAF_PROP_KS_DIGEST_SHA2_256;
    }
}

static taf_prop_ks_AesKeySize_t ConvertAesKeySize(taf_pa_ks_AesKeySize_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_AES_SIZE_128: return TAF_PROP_KS_AES_SIZE_128;
        case TAF_PA_KS_AES_SIZE_192: return TAF_PROP_KS_AES_SIZE_192;
        case TAF_PA_KS_AES_SIZE_256: return TAF_PROP_KS_AES_SIZE_256;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_AesKeySize_t value %d", (int)paValue);
            return TAF_PROP_KS_AES_SIZE_256;
    }
}

static taf_prop_ks_AesBlockMode_t ConvertAesBlockMode(taf_pa_ks_AesBlockMode_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_AES_MODE_ECB_PAD_NONE:  return TAF_PROP_KS_AES_MODE_ECB_PAD_NONE;
        case TAF_PA_KS_AES_MODE_ECB_PAD_PKCS7: return TAF_PROP_KS_AES_MODE_ECB_PAD_PKCS7;
        case TAF_PA_KS_AES_MODE_CBC_PAD_NONE:  return TAF_PROP_KS_AES_MODE_CBC_PAD_NONE;
        case TAF_PA_KS_AES_MODE_CBC_PAD_PKCS7: return TAF_PROP_KS_AES_MODE_CBC_PAD_PKCS7;
        case TAF_PA_KS_AES_MODE_CTR:           return TAF_PROP_KS_AES_MODE_CTR;
        case TAF_PA_KS_AES_MODE_GCM:           return TAF_PROP_KS_AES_MODE_GCM;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_AesBlockMode_t value %d", (int)paValue);
            return TAF_PROP_KS_AES_MODE_GCM;
    }
}

static taf_prop_ks_KeyUsage_t ConvertKeyUsage(taf_pa_ks_KeyUsage_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_RSA_ENCRYPT_DECRYPT: return TAF_PROP_KS_RSA_ENCRYPT_DECRYPT;
        case TAF_PA_KS_RSA_ENCRYPT_ONLY:    return TAF_PROP_KS_RSA_ENCRYPT_ONLY;
        case TAF_PA_KS_RSA_DECRYPT_ONLY:    return TAF_PROP_KS_RSA_DECRYPT_ONLY;
        case TAF_PA_KS_RSA_SIGN_VERIFY:     return TAF_PROP_KS_RSA_SIGN_VERIFY;
        case TAF_PA_KS_RSA_SIGN_ONLY:       return TAF_PROP_KS_RSA_SIGN_ONLY;
        case TAF_PA_KS_RSA_VERIFY_ONLY:     return TAF_PROP_KS_RSA_VERIFY_ONLY;
        case TAF_PA_KS_AES_ENCRYPT_DECRYPT: return TAF_PROP_KS_AES_ENCRYPT_DECRYPT;
        case TAF_PA_KS_AES_ENCRYPT_ONLY:    return TAF_PROP_KS_AES_ENCRYPT_ONLY;
        case TAF_PA_KS_AES_DECRYPT_ONLY:    return TAF_PROP_KS_AES_DECRYPT_ONLY;
        case TAF_PA_KS_ECDSA_SIGN_VERIFY:   return TAF_PROP_KS_ECDSA_SIGN_VERIFY;
        case TAF_PA_KS_ECDSA_SIGN_ONLY:     return TAF_PROP_KS_ECDSA_SIGN_ONLY;
        case TAF_PA_KS_ECDSA_VERIFY_ONLY:   return TAF_PROP_KS_ECDSA_VERIFY_ONLY;
        case TAF_PA_KS_HMAC_SIGN_VERIFY:    return TAF_PROP_KS_HMAC_SIGN_VERIFY;
        case TAF_PA_KS_HMAC_SIGN_ONLY:      return TAF_PROP_KS_HMAC_SIGN_ONLY;
        case TAF_PA_KS_HMAC_VERIFY_ONLY:    return TAF_PROP_KS_HMAC_VERIFY_ONLY;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_KeyUsage_t value %d", (int)paValue);
            return TAF_PROP_KS_RSA_ENCRYPT_DECRYPT;
    }
}

static taf_pa_ks_KeyUsage_t ConvertKeyUsageBack(taf_prop_ks_KeyUsage_t propValue)
{
    switch (propValue)
    {
        case TAF_PROP_KS_RSA_ENCRYPT_DECRYPT: return TAF_PA_KS_RSA_ENCRYPT_DECRYPT;
        case TAF_PROP_KS_RSA_ENCRYPT_ONLY:    return TAF_PA_KS_RSA_ENCRYPT_ONLY;
        case TAF_PROP_KS_RSA_DECRYPT_ONLY:    return TAF_PA_KS_RSA_DECRYPT_ONLY;
        case TAF_PROP_KS_RSA_SIGN_VERIFY:     return TAF_PA_KS_RSA_SIGN_VERIFY;
        case TAF_PROP_KS_RSA_SIGN_ONLY:       return TAF_PA_KS_RSA_SIGN_ONLY;
        case TAF_PROP_KS_RSA_VERIFY_ONLY:     return TAF_PA_KS_RSA_VERIFY_ONLY;
        case TAF_PROP_KS_AES_ENCRYPT_DECRYPT: return TAF_PA_KS_AES_ENCRYPT_DECRYPT;
        case TAF_PROP_KS_AES_ENCRYPT_ONLY:    return TAF_PA_KS_AES_ENCRYPT_ONLY;
        case TAF_PROP_KS_AES_DECRYPT_ONLY:    return TAF_PA_KS_AES_DECRYPT_ONLY;
        case TAF_PROP_KS_ECDSA_SIGN_VERIFY:   return TAF_PA_KS_ECDSA_SIGN_VERIFY;
        case TAF_PROP_KS_ECDSA_SIGN_ONLY:     return TAF_PA_KS_ECDSA_SIGN_ONLY;
        case TAF_PROP_KS_ECDSA_VERIFY_ONLY:   return TAF_PA_KS_ECDSA_VERIFY_ONLY;
        case TAF_PROP_KS_HMAC_SIGN_VERIFY:    return TAF_PA_KS_HMAC_SIGN_VERIFY;
        case TAF_PROP_KS_HMAC_SIGN_ONLY:      return TAF_PA_KS_HMAC_SIGN_ONLY;
        case TAF_PROP_KS_HMAC_VERIFY_ONLY:    return TAF_PA_KS_HMAC_VERIFY_ONLY;
        default:
            TAF_PA_WARN("Unknown taf_prop_ks_KeyUsage_t value %d", (int)propValue);
            return TAF_PA_KS_RSA_ENCRYPT_DECRYPT;
    }
}

static taf_prop_ks_CryptoPurpose_t ConvertCryptoPurpose(taf_pa_ks_CryptoPurpose_t paValue)
{
    switch (paValue)
    {
        case TAF_PA_KS_CRYPTO_ENCRYPT: return TAF_PROP_KS_CRYPTO_ENCRYPT;
        case TAF_PA_KS_CRYPTO_DECRYPT: return TAF_PROP_KS_CRYPTO_DECRYPT;
        case TAF_PA_KS_CRYPTO_SIGN:    return TAF_PROP_KS_CRYPTO_SIGN;
        case TAF_PA_KS_CRYPTO_VERIFY:  return TAF_PROP_KS_CRYPTO_VERIFY;
        default:
            TAF_PA_WARN("Unknown taf_pa_ks_CryptoPurpose_t value %d", (int)paValue);
            return TAF_PROP_KS_CRYPTO_ENCRYPT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert a taf_pa_ks_Tag_t array to taf_prop_ks_Tag_t array.
 * Writes up to tagListSize entries into outTags[].
 */
//--------------------------------------------------------------------------------------------------
static void ConvertTagList
(
    const taf_pa_ks_Tag_t** paTagList,     ///< [IN]  Array of PA-layer tag pointers to convert
    size_t tagListSize,                    ///< [IN]  Number of entries in paTagList
    taf_prop_ks_Tag_t* propTagBuf,         ///< [OUT] Caller-allocated buffer for converted tags
    const taf_prop_ks_Tag_t** propTagList  ///< [OUT] Array of pointers into propTagBuf
)
{
    for (size_t i = 0; i < tagListSize; i++)
    {
        const taf_pa_ks_Tag_t* src = paTagList[i];
        taf_prop_ks_Tag_t* dst = &propTagBuf[i];
        memset(dst, 0, sizeof(*dst));
        switch (src->id)
        {
            case TAF_PA_KS_TAG_MAX_USES_PER_BOOT:
                dst->id = TAF_PROP_KS_TAG_MAX_USES_PER_BOOT;
                dst->maxUsesPerBoot = src->maxUsesPerBoot;
                break;
            case TAF_PA_KS_TAG_MIN_SECONDS_BETWEEN_OPS:
                dst->id = TAF_PROP_KS_TAG_MIN_SECONDS_BETWEEN_OPS;
                dst->minSecondsBetweenOps = src->minSecondsBetweenOps;
                break;
            case TAF_PA_KS_TAG_ACTIVE_DATETIME:
                dst->id = TAF_PROP_KS_TAG_ACTIVE_DATETIME;
                dst->activeDateTime = src->activeDateTime;
                break;
            case TAF_PA_KS_TAG_ORIGINATION_EXPIRE_DATETIME:
                dst->id = TAF_PROP_KS_TAG_ORIGINATION_EXPIRE_DATETIME;
                dst->originationExpireDateTime = src->originationExpireDateTime;
                break;
            case TAF_PA_KS_TAG_USAGE_EXPIRE_DATETIME:
                dst->id = TAF_PROP_KS_TAG_USAGE_EXPIRE_DATETIME;
                dst->usageExpireDateTime = src->usageExpireDateTime;
                break;
            case TAF_PA_KS_TAG_APPLICATION_DATA:
                dst->id = TAF_PROP_KS_TAG_APPLICATION_DATA;
                /* appDataPtr points into caller-owned memory; share the pointer */
                dst->appDataPtr = (taf_prop_ks_Data_t*)src->appDataPtr;
                break;
            default:
                TAF_PA_WARN("Unknown taf_pa_ks_TagId_t value %d", (int)src->id);
                dst->id = (taf_prop_ks_TagId_t)src->id;
                break;
        }
        propTagList[i] = dst;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert a taf_pa_ks_Param_t array to taf_prop_ks_Param_t array.
 * Writes up to paramListSize entries into propParamBuf[].
 */
//--------------------------------------------------------------------------------------------------
static void ConvertParamList
(
    const taf_pa_ks_Param_t** paParamList,     ///< [IN]  Array of PA-layer param pointers
    size_t paramListSize,                      ///< [IN]  Number of entries in paParamList
    taf_prop_ks_Param_t* propParamBuf,         ///< [OUT] Caller-allocated buffer for converted
                                               ///<       params (one entry per paParamList element)
    const taf_prop_ks_Param_t** propParamList  ///< [OUT] Array of pointers into propParamBuf
)
{
    for (size_t i = 0; i < paramListSize; i++)
    {
        const taf_pa_ks_Param_t* src = paParamList[i];
        taf_prop_ks_Param_t* dst = &propParamBuf[i];
        memset(dst, 0, sizeof(*dst));
        switch (src->id)
        {
            case TAF_PA_KS_PARAM_NONCE:
                dst->id = TAF_PROP_KS_PARAM_NONCE;
                dst->nonceDataPtr = (taf_prop_ks_Nonce_t*)src->nonceDataPtr;
                break;
            case TAF_PA_KS_PARAM_APPLICATION_DATA:
                dst->id = TAF_PROP_KS_PARAM_APPLICATION_DATA;
                dst->appDataPtr = (taf_prop_ks_Data_t*)src->appDataPtr;
                break;
            case TAF_PA_KS_PARAM_RSA_PADDING_TYPE:
                dst->id = TAF_PROP_KS_PARAM_RSA_PADDING_TYPE;
                dst->rsaPaddingType = (taf_prop_ks_RsaPaddingType_t)src->rsaPaddingType;
                break;
            default:
                TAF_PA_WARN("Unknown taf_pa_ks_ParamId_t value %d", (int)src->id);
                dst->id = (taf_prop_ks_ParamId_t)src->id;
                break;
        }
        propParamList[i] = dst;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy a taf_prop_ks_sharedAppList_t result back into a taf_pa_ks_sharedAppList_t.
 */
//--------------------------------------------------------------------------------------------------
static void ConvertSharedAppListBack
(
    const taf_prop_ks_sharedAppList_t* propList,  ///< [IN]  Prop-layer result to convert back
    taf_pa_ks_sharedAppList_t* paList             ///< [OUT] PA-layer struct to populate
)
{
    for (int i = 0; i < TAF_PA_KS_MAX_SHARED_APPS; i++)
    {
        paList->appInfo[i].keyCap = ConvertKeyUsageBack(propList->appInfo[i].keyCap);
        paList->appInfo[i].appCap = propList->appInfo[i].appCap;
        memcpy(paList->appInfo[i].appName, propList->appInfo[i].appName,
               TAF_PA_KS_MAX_APP_NAME_SIZE + 1);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Convert function pointer types for key creation / sharing handlers.
 * Both sides use KeyMgt_KeyFileRef_t (void*) so the signatures are ABI-compatible;
 * a union is used to avoid a direct function-pointer cast.
 */
//--------------------------------------------------------------------------------------------------
static taf_prop_ks_KeyCreationHandler_t ConvertKeyCreationHandler
(
    taf_pa_ks_KeyCreationHandler_t paHandler
)
{
    union { taf_pa_ks_KeyCreationHandler_t pa; taf_prop_ks_KeyCreationHandler_t prop; } u;
    u.pa = paHandler;
    return u.prop;
}

static taf_prop_ks_KeySharingHandler_t ConvertKeySharingHandler
(
    taf_pa_ks_KeySharingHandler_t paHandler
)
{
    union { taf_pa_ks_KeySharingHandler_t pa; taf_prop_ks_KeySharingHandler_t prop; } u;
    u.pa = paHandler;
    return u.prop;
}


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
taf_pa_result_t taf_pa_ks_Init(void)
{
    TAF_PA_INFO("Telaf keyStore PA initializing ...");

    // Initialize RFS vtable injection first

    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_file_vtable_initialized, &expected, true))
    {
        TAF_PA_WARN("RFS vtable already initialized");
        return TAF_PA_OK;
    }

    TAF_PA_INFO("Injecting RFS vtable into keystore noship component");

    // Get the vtable from taf_pa_file.c and inject it into noship component
    taf_prop_result_t underlyingResult = taf_prop_file_vtable_Bind(&g_file_vtable);
    if (underlyingResult != TAF_PROP_OK && underlyingResult != TAF_PROP_NOT_IMPLEMENTED)
    {
        TAF_PA_ERROR("Failed to bind RFS vtable. Error: %d", (int)underlyingResult);
        atomic_store(&g_file_vtable_initialized, false);
        return PropResultToPaResult(underlyingResult, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    TAF_PA_INFO("RFS vtable injection completed successfully");

    // Initialize the proprietary keystore
    taf_prop_result_t result = taf_prop_ks_Init();
    if (result == TAF_PROP_OK)
    {
        atomic_store(&g_keystore_initialized, true);
    }

    return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * PA deinitialization.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_Deinit(void)
{
    // Check if initialization was successful before proceeding with deinitialization
    if (!atomic_load(&g_keystore_initialized))
    {
        TAF_PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return TAF_PA_FAULT;
    }

     if (!atomic_load(&g_file_vtable_initialized))
    {
        TAF_PA_WARN("RFS vtable not initialized - ignoring deinit request");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Unbinding RFS vtable from keystore noship component");

    // Unbind the vtable by passing NULL vtable
    taf_prop_result_t result = taf_prop_file_vtable_Bind(NULL);
    if (result != TAF_PROP_OK && result != TAF_PROP_NOT_IMPLEMENTED)
    {
        TAF_PA_ERROR("Failed to unbind RFS vtable. Error: %d", (int)result);
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    // Reset initialization flag
    atomic_store(&g_file_vtable_initialized, false);
    TAF_PA_INFO("RFS vtable unbinding completed successfully");

    atomic_store(&g_keystore_initialized, false);
    TAF_PA_INFO("Telaf keyStore PA deinitialized.");
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA encryption key and return a key file reference
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GenerateRsaEncKey
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
    taf_prop_ks_Tag_t   propTags[tagListSize];
    const taf_prop_ks_Tag_t* propTagList[tagListSize];
    ConvertTagList(tagListPtr, tagListSize, propTags, propTagList);

    taf_prop_result_t rc = taf_prop_ks_GenerateRsaEncKey(
        clientSessionFd, keyName,
        ConvertRsaKeySize(keySize),
        ConvertEncPurpose(purpose),
        ConvertRsaEncPadding(padding),
        propTagList, tagListSize,
        impDataPtr, impDataSize,
        keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a RSA signature key and return a key file reference.
 *
 * The impData must be a PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GenerateRsaSigKey
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
    taf_prop_ks_Tag_t   propTags[tagListSize];
    const taf_prop_ks_Tag_t* propTagList[tagListSize];
    ConvertTagList(tagListPtr, tagListSize, propTags, propTagList);

    taf_prop_result_t rc = taf_prop_ks_GenerateRsaSigKey(
        clientSessionFd, keyName,
        ConvertRsaKeySize(keySize),
        ConvertSigPurpose(purpose),
        ConvertRsaSigPadding(padding),
        propTagList, tagListSize,
        impDataPtr, impDataSize,
        keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an ECDSA key and return a key file reference.
 *
 * The impData must be PKCS#8 der bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GenerateEcdsaKey
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
    taf_prop_ks_Tag_t   propTags[tagListSize];
    const taf_prop_ks_Tag_t* propTagList[tagListSize];
    ConvertTagList(tagListPtr, tagListSize, propTags, propTagList);

    taf_prop_result_t rc = taf_prop_ks_GenerateEcdsaKey(
        clientSessionFd, keyName,
        ConvertEccKeySize(keySize),
        ConvertSigPurpose(purpose),
        ConvertDigest(digest),
        propTagList, tagListSize,
        impDataPtr, impDataSize,
        keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import an AES key and return a key file reference.
 *
 * The impData must be raw key bytes if provided.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GenerateAesKey
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
    taf_prop_ks_Tag_t   propTags[tagListSize];
    const taf_prop_ks_Tag_t* propTagList[tagListSize];
    ConvertTagList(tagListPtr, tagListSize, propTags, propTagList);

    taf_prop_result_t rc = taf_prop_ks_GenerateAesKey(
        clientSessionFd, keyName,
        ConvertAesKeySize(keySize),
        ConvertEncPurpose(purpose),
        ConvertAesBlockMode(mode),
        propTagList, tagListSize,
        impDataPtr, impDataSize,
        keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or import a HMAC key and return a key file reference.
 *
 * Currently only digest DIGEST_SHA2_256 is supported. The impData must be raw key bytes if provided
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GenerateHmacKey
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
    taf_prop_ks_Tag_t   propTags[tagListSize];
    const taf_prop_ks_Tag_t* propTagList[tagListSize];
    ConvertTagList(tagListPtr, tagListSize, propTags, propTagList);

    taf_prop_result_t rc = taf_prop_ks_GenerateHmacKey(
        clientSessionFd, keyName, keySize,
        ConvertSigPurpose(purpose),
        ConvertDigest(digest),
        propTagList, tagListSize,
        impDataPtr, impDataSize,
        keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Export a key into specified key data format.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_ExportKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    const uint8_t* appDataPtr,
    size_t appDataSize,
    uint8_t* expDataPtr,
    size_t* expDataSizePtr
)
{
    taf_prop_result_t rc = taf_prop_ks_ExportKey(clientSessionFd, keyFileRef, appDataPtr,
                                                  appDataSize, expDataPtr, expDataSizePtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Share a key.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_ShareKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_KeyUsage_t keyCap,
    uint32_t appCap,
    const char* appName
)
{
    taf_prop_result_t rc = taf_prop_ks_ShareKey(
        clientSessionFd, keyFileRef,
        ConvertKeyUsage(keyCap),
        appCap, appName);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file by key name.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_DeleteKey
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef
)
{
    taf_prop_result_t rc = taf_prop_ks_DeleteKey(clientSessionFd, keyFileRef);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by key name.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GetKey
(
    int clientSessionFd,
    const char* keyName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    taf_prop_result_t rc = taf_prop_ks_GetKey(clientSessionFd, keyName, keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared key file reference by key name and app name.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GetSharedKey
(
    int clientSessionFd,
    const char* keyName,
    const char* appName,
    KeyMgt_KeyFileRef_t* keyFileRefPtr
)
{
    taf_prop_result_t rc = taf_prop_ks_GetSharedKey(clientSessionFd, keyName, appName,
                                                     keyFileRefPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Cancel key sharing to an application.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_CancelKeySharing
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    const char* appName
)
{
    taf_prop_result_t rc = taf_prop_ks_CancelKeySharing(clientSessionFd, keyFileRef, appName);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a shared app list for a shared key.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GetSharedAppList
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_sharedAppList_t* appListPtr
)
{
    taf_prop_ks_sharedAppList_t propAppList;
    taf_prop_result_t rc = taf_prop_ks_GetSharedAppList(clientSessionFd, keyFileRef, &propAppList);
    if (rc == TAF_PROP_OK)
    {
        ConvertSharedAppListBack(&propAppList, appListPtr);
    }
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get key usage
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_GetKeyUsage
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_KeyUsage_t* keyUsagePtr
)
{
    taf_prop_ks_KeyUsage_t propKeyUsage;
    taf_prop_result_t rc = taf_prop_ks_GetKeyUsage(clientSessionFd, keyFileRef, &propKeyUsage);
    if (rc == TAF_PROP_OK)
    {
        *keyUsagePtr = ConvertKeyUsageBack(propKeyUsage);
    }
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the session for the given crypto operation.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_CryptoSessionStart
(
    int clientSessionFd,
    KeyMgt_KeyFileRef_t keyFileRef,
    taf_pa_ks_CryptoPurpose_t cryptoPurpose,
    const taf_pa_ks_Param_t** paramListPtr,
    size_t paramListSize,
    uint64_t* opHandlePtr
)
{
    taf_prop_ks_Param_t   propParams[paramListSize];
    const taf_prop_ks_Param_t* propParamList[paramListSize];
    ConvertParamList(paramListPtr, paramListSize, propParams, propParamList);

    taf_prop_result_t rc = taf_prop_ks_CryptoSessionStart(
        clientSessionFd, keyFileRef,
        ConvertCryptoPurpose(cryptoPurpose),
        propParamList, paramListSize,
        opHandlePtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides AES AEAD to the running crypto session started with CryptoSessionStart API
 * for AES GCM mode.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_CryptoSessionProcessAead
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize
)
{
    taf_prop_result_t rc = taf_prop_ks_CryptoSessionProcessAead(opHandle, inputDataPtr,
                                                                  inputDataSize);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Provides data to, and possibly receives output from, a running crypto operation.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_CryptoSessionProcess
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize,
    uint8_t* outputDataPtr,
    size_t* outputDataSizePtr
)
{
    taf_prop_result_t rc = taf_prop_ks_CryptoSessionProcess(opHandle, inputDataPtr, inputDataSize,
                                                             outputDataPtr, outputDataSizePtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Finalizes and stops a crypto operation session.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_CryptoSessionEnd
(
    uint64_t opHandle,
    const uint8_t* inputDataPtr,
    size_t inputDataSize,
    uint8_t* outputDataPtr,
    size_t* outputDataSizePtr
)
{
    taf_prop_result_t rc = taf_prop_ks_CryptoSessionEnd(opHandle, inputDataPtr, inputDataSize,
                                                         outputDataPtr, outputDataSizePtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Abort crypto operation session.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_CryptoSessionAbort
(
    uint64_t opHandle
)
{
    taf_prop_result_t rc = taf_prop_ks_CryptoSessionAbort(opHandle);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register Key creation handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_RegKeyCreationHandler
(
    taf_pa_ks_KeyCreationHandler_t handlerFunc
)
{
    taf_prop_result_t rc = taf_prop_ks_RegKeyCreationHandler(
        ConvertKeyCreationHandler(handlerFunc));
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Register Key sharing state change handler in PA layer
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_ks_RegKeySharingHandler
(
    taf_pa_ks_KeySharingHandler_t handlerFunc
)
{
    taf_prop_result_t rc = taf_prop_ks_RegKeySharingHandler(
        ConvertKeySharingHandler(handlerFunc));
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}
