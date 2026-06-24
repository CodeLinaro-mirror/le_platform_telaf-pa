/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_FILE_H
#define TAF_PROP_FILE_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * RFS Error types
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    RFS_ERR_SET_HASH = 0,
    RFS_ERR_NO_MEMORY = 1,
    RFS_ERR_BACKUP = 2,
    RFS_ERR_RESTORE = 3,
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
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t taf_prop_file_vtable_Bind(const taf_prop_file_vtable_t* vtable);

LE_SHARED int taf_prop_file_vtable_IsBound(void);

int taf_prop_file_Open
(
    const char *filePathPtr,
    int flags,
    mode_t mode
);

int taf_prop_file_Close
(
    int fd
);

ssize_t taf_prop_file_Read
(
    int fd,
    uint8_t* bufPtr,
    size_t size
);

ssize_t taf_prop_file_Write
(
    int fd,
    const uint8_t* bufPtr,
    size_t size
);

void taf_prop_file_Delete
(
    const char* filePathPtr
);

int taf_prop_file_Copy
(
    const char *sourcePath,
    const char *destPath
);

int taf_prop_file_Rename
(
    const char *sourcePath,
    const char *destPath
);

#endif // TAF_PROP_FILE_H
