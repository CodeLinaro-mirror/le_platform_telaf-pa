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

#ifndef TAF_PROP_FILE_H
#define TAF_PROP_FILE_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "taf_prop_common.h"

//--------------------------------------------------------------------------------------------------
/**
 * RFS Error types
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    TAF_PROP_FILE_ERR_SET_HASH = 0,
    TAF_PROP_FILE_ERR_NO_MEMORY = 1,
    TAF_PROP_FILE_ERR_BACKUP = 2,
    TAF_PROP_FILE_ERR_RESTORE = 3,
}
taf_prop_file_Error_t;

//--------------------------------------------------------------------------------------------------
/**
 * RFS Error Handler function pointer type
 */
//--------------------------------------------------------------------------------------------------
typedef void (*taf_prop_ErrorHandler_t)(taf_prop_file_Error_t error, const char* filePath);

//--------------------------------------------------------------------------------------------------
/**
 * RFS vtable structure containing function pointers for file operations
 */
//--------------------------------------------------------------------------------------------------
typedef struct taf_prop_file_vtable
{
    uint32_t abi_version;
    size_t size;

    // RFS File Operations
    int (*prop_open)(const char *filePathPtr, int flags, mode_t mode);
    int (*prop_close)(int fd);
    ssize_t (*prop_read)(int fd, uint8_t* bufPtr, size_t size);
    ssize_t (*prop_write)(int fd, const uint8_t* bufPtr, size_t size);
    void (*prop_delete)(const char* filePathPtr);
    int (*prop_copy)(const char *sourcePath, const char *destPath);
    int (*prop_rename)(const char *sourcePath, const char *destPath);
} taf_prop_file_vtable_t;

//--------------------------------------------------------------------------------------------------
/**
 * Bind the RFS vtable
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_vtable_Bind(const taf_prop_file_vtable_t* vtable);

TAF_PROP_SHARED int taf_prop_file_vtable_IsBound(void);

//--------------------------------------------------------------------------------------------------
/**
 * Open a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *         TAF_PROP_BAD_PARAMETER if filePathPtr or fdPtr is NULL.
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Open
(
    const char *filePathPtr,
    int flags,
    mode_t mode,
    int* fdPtr,
    uint32_t* underlyingErrPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Close a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Close
(
    int fd,
    uint32_t* underlyingErrPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Read from a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *         TAF_PROP_BAD_PARAMETER if bufPtr or bytesReadPtr is NULL.
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Read
(
    int fd,
    uint8_t* bufPtr,
    size_t size,
    size_t* bytesReadPtr,
    uint32_t* underlyingErrPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Write to a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *         TAF_PROP_BAD_PARAMETER if bufPtr or bytesWrittenPtr is NULL.
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Write
(
    int fd,
    const uint8_t* bufPtr,
    size_t size,
    size_t* bytesWrittenPtr,
    uint32_t* underlyingErrPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *         TAF_PROP_BAD_PARAMETER if filePathPtr is NULL.
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Delete
(
    const char* filePathPtr,
    uint32_t* underlyingErrPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Copy a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *         TAF_PROP_BAD_PARAMETER if sourcePath or destPath is NULL.
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Copy
(
    const char *sourcePath,
    const char *destPath,
    uint32_t* underlyingErrPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Rename (move) a file.
 *
 * @return TAF_PROP_OK on success.
 *         TAF_PROP_SYSTEM_ERROR if the underlying system call fails (check *underlyingErrPtr).
 *         TAF_PROP_BAD_PARAMETER if sourcePath or destPath is NULL.
 *
 * @param underlyingErrPtr [OUT] Optional. If non-NULL, receives the raw underlying error value
 *                          (e.g. errno) when this function returns TAF_PROP_SYSTEM_ERROR. For
 *                          all other return values, *underlyingErrPtr is set to
 *                          TAF_PROP_UNDERLYING_ERR_NONE. Pass NULL if not needed.
 */
//--------------------------------------------------------------------------------------------------
TAF_PROP_SHARED taf_prop_result_t taf_prop_file_Rename
(
    const char *sourcePath,
    const char *destPath,
    uint32_t* underlyingErrPtr
);

#endif // TAF_PROP_FILE_H
