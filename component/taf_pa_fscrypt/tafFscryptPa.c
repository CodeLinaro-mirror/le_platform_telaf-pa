/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafFscryptPa.h"
#include "taf_prop_fscrypt.h"
#include "taf_prop_file.h"
#include "tafFilePa.h"
#include "tafInternalCommonPa.h"
#include <stdatomic.h>
#include <stdbool.h>

//--------------------------------------------------------------------------------------------------
/**
 * Directory used to store the backup copies of the fscrypt key files.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FSC_FILE_BACKUP_STORAGE   "/data/persist/tafFscryptSvc/AppKey/"

// Thread-safe initialization flags
static _Atomic(bool) g_fscrypt_initialized = false;
static _Atomic(bool) g_file_vtable_initialized = false;

//--------------------------------------------------------------------------------------------------
/**
 * The file vtable injected into the fscrypt noship component.
 */
//--------------------------------------------------------------------------------------------------
static const taf_prop_file_vtable_t g_file_vtable = {
    .abi_version = 1,
    .size = sizeof(taf_prop_file_vtable_t),

    // File Operations
    .prop_open   = taf_pa_file_Open,
    .prop_close  = taf_pa_file_Close,
    .prop_read   = taf_pa_file_Read,
    .prop_write  = taf_pa_file_Write,
    .prop_delete = taf_pa_file_Delete,
    .prop_copy   = taf_pa_file_Copy,
    .prop_rename = taf_pa_file_Rename,
};

//--------------------------------------------------------------------------------------------------
/**
 * PA initialization.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_fsc_Init
(
    void* cryptoFunc
)
{
    TAF_PA_INFO("Telaf fscrypt PA initializing ...");

    // Create the fscrypt storage directory before any file I/O takes place.
    taf_prop_result_t rc = taf_prop_fsc_Component_Init();
    if (rc != TAF_PROP_OK)
    {
        TAF_PA_ERROR("taf_prop_fsc_Component_Init failed: %d", rc);
        return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    // Guard against double-initialization of the vtable.
    bool expected = false;
    if (!atomic_compare_exchange_strong(&g_file_vtable_initialized, &expected, true))
    {
        TAF_PA_WARN("file vtable already initialized");
        return TAF_PA_OK;
    }

    // Initialize the file PA (backup storage directory, checksum/backup feature) before
    // it gets used through the injected vtable below.
    if (taf_pa_file_Init(true, NULL) != TAF_PA_OK)
    {
        TAF_PA_ERROR("Failed to initialize file PA.");
        atomic_store(&g_file_vtable_initialized, false);
        return TAF_PA_FAULT;
    }

    // Keep the fscrypt backups in their own data directory.
    if (taf_pa_file_SetBackupStorage(TAF_PA_FSC_FILE_BACKUP_STORAGE) != TAF_PA_OK)
    {
        TAF_PA_ERROR("Failed to set file backup storage to %s.", TAF_PA_FSC_FILE_BACKUP_STORAGE);
        atomic_store(&g_file_vtable_initialized, false);
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Injecting file vtable into fscrypt noship component");

    taf_prop_result_t bindResult = taf_prop_file_vtable_Bind(&g_file_vtable);
    if (bindResult != TAF_PROP_OK && bindResult != TAF_PROP_NOT_IMPLEMENTED)
    {
        TAF_PA_ERROR("Failed to bind RFS vtable. Error: %d", (int)bindResult);
        atomic_store(&g_file_vtable_initialized, false);
        return PropResultToPaResult(bindResult, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    TAF_PA_INFO("file vtable injection completed successfully");

    // Load all persisted fscrypt key files from storage.
    rc = taf_prop_fsc_Init(cryptoFunc);
    if (rc != TAF_PROP_OK)
    {
        TAF_PA_ERROR("taf_prop_fsc_Init failed: %d", rc);

        taf_prop_result_t unbindResult = taf_prop_file_vtable_Bind(NULL);
        if (unbindResult != TAF_PROP_OK && unbindResult != TAF_PROP_NOT_IMPLEMENTED)
        {
            TAF_PA_ERROR("Failed to unbind RFS vtable during cleanup. Error: %d",
                         (int)unbindResult);
            // The vtable bind may have leaked, but Init() has failed so the system
            // is not in a usable state regardless. Always clear the flag so that a
            // subsequent Init() call is not permanently blocked.
        }
        atomic_store(&g_file_vtable_initialized, false);

        return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    atomic_store(&g_fscrypt_initialized, true);
    TAF_PA_INFO("Telaf fscrypt PA initialized.");
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * PA deinitialization.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_fsc_Deinit
(
    void
)
{
    // Check if initialization was successful before proceeding with deinitialization.
    if (!atomic_load(&g_fscrypt_initialized))
    {
        TAF_PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return TAF_PA_FAULT;
    }

    if (!atomic_load(&g_file_vtable_initialized))
    {
        TAF_PA_WARN("file vtable not initialized - ignoring deinit request");
        return TAF_PA_FAULT;
    }

    TAF_PA_INFO("Unbinding file vtable from fscrypt noship component");

    // Unbind the vtable by passing NULL.
    taf_prop_result_t result = taf_prop_file_vtable_Bind(NULL);
    if (result != TAF_PROP_OK && result != TAF_PROP_NOT_IMPLEMENTED)
    {
        TAF_PA_ERROR("Failed to unbind RFS vtable. Error: %d", (int)result);
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    // Reset initialization flags.
    atomic_store(&g_file_vtable_initialized, false);
    TAF_PA_INFO("file vtable unbinding completed successfully");

    atomic_store(&g_fscrypt_initialized, false);
    TAF_PA_INFO("Telaf fscrypt PA deinitialized.");
    return TAF_PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get a key file reference by directory name.
 */
//--------------------------------------------------------------------------------------------------
taf_pa_result_t taf_pa_fsc_GetKey
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
taf_pa_result_t taf_pa_fsc_GenerateAesKey
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
taf_pa_result_t taf_pa_fsc_DeleteKey
(
    int clientSessionFd,                    ///< [IN] Client session Fd
    KeyMgt_KeyFileRef_t keyFileRef          ///< [IN] Key file reference
)
{
    taf_prop_result_t rc = taf_prop_fsc_DeleteKey(clientSessionFd, keyFileRef);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}
