/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "taf_prop_file.h"

//--------------------------------------------------------------------------------------------------
/**
 * PROP component for bind.
 *
 * @return
 */
//--------------------------------------------------------------------------------------------------
taf_prop_result_t taf_prop_file_vtable_Bind
(
    const taf_prop_file_vtable_t* vtable
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

int taf_prop_file_vtable_IsBound(void)
{
    return 0;
}

taf_prop_result_t taf_prop_file_Open
(
    const char *filePathPtr,
    int flags,
    mode_t mode,
    int* fdPtr,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_file_Close
(
    int fd,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_file_Read
(
    int fd,
    uint8_t* bufPtr,
    size_t size,
    size_t* bytesReadPtr,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_file_Write
(
    int fd,
    const uint8_t* bufPtr,
    size_t size,
    size_t* bytesWrittenPtr,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_file_Delete
(
    const char* filePathPtr,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_file_Copy
(
    const char *sourcePath,
    const char *destPath,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_file_Rename
(
    const char *sourcePath,
    const char *destPath,
    uint32_t* underlyingErrPtr
)
{
    return TAF_PROP_NOT_IMPLEMENTED;
}