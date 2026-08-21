/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PA_FILE_H
#define TAF_PA_FILE_H

#include "tafCommonPa.h"
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
