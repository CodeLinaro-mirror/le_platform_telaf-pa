/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "legato.h"
#include "interfaces.h"
#include "taf_pa_fscrypt.h"
#include "taf_prop_fscrypt.h"

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
void taf_pa_fsc_Init
(
    void* cryptoFunc
)
{
    taf_prop_fsc_Init(cryptoFunc);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by directory name.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_fsc_GetKey
(
    le_msg_SessionRef_t clientSessionRef,   ///< [IN] Client session reference
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference.
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    return taf_prop_fsc_GetKey(clientSessionRef, dirName, keyFileRefPtr, key, keyLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create AES key and return a key file reference.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_fsc_GenerateAesKey
(
    le_msg_SessionRef_t clientSessionRef,   ///< [IN] Client session reference
    const char* dirName,                    ///< [IN] dir Name
    KeyMgt_KeyFileRef_t* keyFileRefPtr,     ///< [OUT] Key file reference
    uint8_t* key,                           ///< [OUT] Raw key
    size_t keyLen                           ///< [OUT] Length of raw key
)
{
    return taf_prop_fsc_GenerateAesKey(clientSessionRef, dirName, keyFileRefPtr, key, keyLen);
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a key file.
 */
//--------------------------------------------------------------------------------------------------
le_result_t taf_pa_fsc_DeleteKey
(
    le_msg_SessionRef_t clientSessionRef, ///< [IN] Client session reference
    KeyMgt_KeyFileRef_t keyFileRef        ///< [IN] Key file reference
)
{
    return taf_prop_fsc_DeleteKey(clientSessionRef, keyFileRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * The PA initialization function.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    taf_prop_fsc_Component_Init();
    LE_INFO("Telaf fscrypt PA initialized.");
}
