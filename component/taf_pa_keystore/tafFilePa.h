/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_FILE_H
#define TAF_PA_FILE_H

#include "tafCommonPa.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

//--------------------------------------------------------------------------------------------------
/**
 * Buffer size used for file copy operations.
 */
//--------------------------------------------------------------------------------------------------
#define RFS_COPY_BUFFER_SIZE 4096

//--------------------------------------------------------------------------------------------------
/**
 * Default backup storage location.
 *
 * This generic default is not tied to any particular user of this PA. A user that needs its
 * backups elsewhere (e.g. in its own data directory) must call taf_pa_file_SetBackupStorage()
 * after taf_pa_file_Init().
 *
 * Only the backup copies are managed by this PA, the primary file path is always the one given to
 * taf_pa_file_Open().
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_BACKUP_STORAGE         "/data/persist/FilePa/"

//--------------------------------------------------------------------------------------------------
/**
 * Prefix of the backup file names.
 *
 * A backup file is named "<prefix><hash of the primary file path>", so a backup can always be told
 * apart from any other file sharing the backup directory.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_BACKUP_PREFIX          "bak_"

//--------------------------------------------------------------------------------------------------
/**
 * Backup storage limits and maximum length of a backup file path.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_MAX_BACKUP_FILENAME    1024
#define TAF_PA_FILE_DEFAULT_MAX_SIZE       10240
#define TAF_PA_FILE_DEFAULT_MAX_COUNT      100

//--------------------------------------------------------------------------------------------------
/**
 * Name of the extended attribute holding the CRC32C checksum of a file.
 *
 * @note Must stay in the "user." namespace: unlike "user.*", the "security.*" namespace is
 *       reserved for names a security module (e.g. SELinux) recognizes, and setting an
 *       arbitrary/unknown "security.*" name is rejected with ENOTSUP on this target.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_EXTENDED_ATTR_CRC32C   "user.crc32c"

//--------------------------------------------------------------------------------------------------
/**
 * Errors reported through the registered error handler.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PA_FILE_ERR_SET_HASH = 0,   ///< Failed to store the checksum of a file.
    TAF_PA_FILE_ERR_NO_MEMORY,      ///< Backup storage capacity exceeded.
    TAF_PA_FILE_ERR_BACKUP,         ///< Failed to create/update the backup of a file.
    TAF_PA_FILE_ERR_RESTORE,        ///< Failed to restore a file from its backup.
    TAF_PA_FILE_ERR_CORRUPTED       ///< File content does not match its stored checksum.
} taf_pa_file_Error_t;

//--------------------------------------------------------------------------------------------------
/**
 * Handler called when a backup/restore/integrity error is detected.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_pa_file_ErrorHandler_t)
(
    taf_pa_file_Error_t error,      ///< [IN] Detected error.
    const char* filePathPtr         ///< [IN] Path of the file concerned by the error.
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the file PA and optionally enable the backup/restore feature.
 *
 * When the backup feature is enabled, the backup storage directory is created (if needed) and each
 * file opened/closed through this PA is checksum protected and mirrored into the backup storage.
 *
 * The backup storage defaults to TAF_PA_FILE_BACKUP_STORAGE. Call taf_pa_file_SetBackupStorage()
 * after this function to use another directory.
 *
 * @note This PA keeps its state (backup configuration, CRC lookup table, etc.) in plain static
 *       variables with no locking. This is safe only because the sole caller wiring this module
 *       in - the KeyStore PA (tafKeystorePa.c, via taf_pa_ks_Init()) - is a single-threaded
 *       service and never invokes these entry points concurrently. Do not call taf_pa_file_Init()
 *       or any taf_pa_file_*() function from more than one thread, and revisit this file's
 *       synchronization if a second concurrent caller is ever added.
 *
 * @return
 *      PA_OK on success.
 *      PA_FAULT if the backup storage cannot be created.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_file_Init
(
    bool enableBackup,                      ///< [IN] Enable the backup/restore feature.
    taf_pa_file_ErrorHandler_t handlerFunc  ///< [IN] Error handler, NULL to unregister.
);

//--------------------------------------------------------------------------------------------------
/**
 * Override the directory used to store the file backups.
 *
 * The directory is created if it does not exist yet. This replaces the
 * TAF_PA_FILE_BACKUP_STORAGE default and should be called by any user that needs its backups
 * in its own directory.
 *
 * @return
 *      PA_OK on success.
 *      PA_BAD_PARAMETER if the path is invalid or cannot be created.
 *      PA_BUSY if the path exists but is not a directory.
 *      PA_OVERFLOW if the path is too long.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_file_SetBackupStorage
(
    const char* pathPtr       ///< [IN] Path to the backup storage directory.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the backup storage capacity limits.
 *
 * @return
 *      PA_OK on success.
 *      PA_BAD_PARAMETER if one of the limits is zero.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_file_SetBackupCapacity
(
    uint32_t maxFileSizeBytes,  ///< [IN] Maximum size of a single backed up file, in bytes.
    uint16_t maxFileCount       ///< [IN] Maximum number of backed up files.
);

//--------------------------------------------------------------------------------------------------
/**
 * Verify the integrity of a file against its stored checksum.
 *
 * @return
 *      PA_OK if the file content matches its stored checksum.
 *      PA_BAD_PARAMETER if the path is invalid.
 *      PA_FAULT if the file is corrupted or has no stored checksum.
 */
//--------------------------------------------------------------------------------------------------
pa_result_t taf_pa_file_Verify
(
    const char* filePathPtr   ///< [IN] Path to the file to verify.
);

//--------------------------------------------------------------------------------------------------
/**
 * Open a file.
 *
 * @return
 *      File descriptor on success.
 *      -1 on failure, with errno set.
 */
//--------------------------------------------------------------------------------------------------
int taf_pa_file_Open
(
    const char *filePathPtr,  ///< [IN] Path to the file
    int flags,                ///< [IN] File access flags (e.g. O_RDONLY, O_WRONLY)
    mode_t mode               ///< [IN] Permission bits used when creating a new file
);

//--------------------------------------------------------------------------------------------------
/**
 * Close a file descriptor.
 *
 * @return
 *      0 on success.
 *      -1 on failure, with errno set.
 */
//--------------------------------------------------------------------------------------------------
int taf_pa_file_Close
(
    int fd                    ///< [IN] File descriptor to close
);

//--------------------------------------------------------------------------------------------------
/**
 * Read from a file descriptor.
 *
 * @return
 *      Number of bytes read on success.
 *      -1 on failure, with errno set.
 */
//--------------------------------------------------------------------------------------------------
ssize_t taf_pa_file_Read
(
    int fd,                   ///< [IN]  File descriptor to read from
    uint8_t* bufPtr,          ///< [OUT] Buffer to store the read data
    size_t size               ///< [IN]  Number of bytes to read
);

//--------------------------------------------------------------------------------------------------
/**
 * Write to a file descriptor.
 *
 * @return
 *      Number of bytes written on success.
 *      -1 on failure, with errno set.
 */
//--------------------------------------------------------------------------------------------------
ssize_t taf_pa_file_Write
(
    int fd,                   ///< [IN] File descriptor to write to
    const uint8_t* bufPtr,    ///< [IN] Buffer containing data to write
    size_t size               ///< [IN] Number of bytes to write
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a file.
 *
 * @return
 *      0 on success.
 *      -1 on failure, with errno set.
 */
//--------------------------------------------------------------------------------------------------
void taf_pa_file_Delete
(
    const char* filePathPtr   ///< [IN] Path to the file to delete
);

//--------------------------------------------------------------------------------------------------
/**
 * Copy a file from sourcePath to destPath.
 * The destination file is created if it does not exist, or truncated if it does.
 *
 * @return
 *      0 on success.
 *      errno value on failure.
 */
//--------------------------------------------------------------------------------------------------
int taf_pa_file_Copy
(
    const char *sourcePath,   ///< [IN] Path to the source file
    const char *destPath      ///< [IN] Path to the destination file
);

//--------------------------------------------------------------------------------------------------
/**
 * Rename (or move) a file from sourcePath to destPath.
 *
 * @return
 *      0 on success.
 *      -1 on failure, with errno set.
 */
//--------------------------------------------------------------------------------------------------
int taf_pa_file_Rename
(
    const char *sourcePath,   ///< [IN] Path to the source file
    const char *destPath      ///< [IN] Path to the destination file
);

#endif // TAF_PA_FILE_H
