/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "legato.h"
#include "tafFilePa.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/xattr.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/evp.h>

int taf_pa_file_Open
(
    const char *filePathPtr,
    int flags,
    mode_t mode
)
{
    LE_DEBUG("%s", __FUNCTION__);
    LE_DEBUG("filePathPtr: %s", filePathPtr);
    return open(filePathPtr, flags, mode);
}

int taf_pa_file_Close
(
    int fd
)
{
    LE_DEBUG("%s", __FUNCTION__);
    return close(fd);
}

ssize_t taf_pa_file_Read
(
    int fd,
    uint8_t* bufPtr,
    size_t sizePtr
)
{
    LE_DEBUG("%s", __FUNCTION__);
    return read(fd, bufPtr, sizePtr);
}

ssize_t taf_pa_file_Write
(
    int fd,
    const uint8_t* bufPtr,
    size_t size
)
{
    LE_DEBUG("%s", __FUNCTION__);

    return write(fd, bufPtr, size);
}

void taf_pa_file_Delete
(
    const char* filePathPtr
)
{
    LE_DEBUG("%s", __FUNCTION__);
    LE_DEBUG("filePathPtr: %s", filePathPtr);

    unlink(filePathPtr);
}

int taf_pa_file_Copy
(
    const char *sourcePath,
    const char *destPath
)
{
    int srcFd, destFd;
    ssize_t bytesRead, bytesWritten;
    uint8_t buffer[RFS_COPY_BUFFER_SIZE];

    // open source file
    srcFd = taf_pa_file_Open(sourcePath, O_RDONLY, 0);
    if (srcFd < 0)
    {
        LE_ERROR("Failed to open source file");
        return errno;
    }

    // open destination file, if it doesn't exist, create it
    destFd = taf_pa_file_Open(destPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destFd < 0)
    {
        LE_ERROR("Failed to open destination file");
        taf_pa_file_Close(srcFd);
        return errno;
    }

    // read source file and write to destination file
    while ((bytesRead = read(srcFd, buffer, RFS_COPY_BUFFER_SIZE)) > 0)
    {
        bytesWritten = taf_pa_file_Write(destFd, buffer, bytesRead);
        if (bytesWritten != bytesRead)
        {
            int savedErrno = errno;
            LE_ERROR("Failed to write to destination file");
            taf_pa_file_Close(srcFd);
            taf_pa_file_Close(destFd);
            taf_pa_file_Delete(destPath);
            return savedErrno;
        }
    }

    // error check for read returned value
    if (bytesRead < 0)
    {
        int savedErrno = errno;
        LE_ERROR("Failed to read from source file");
        taf_pa_file_Close(srcFd);
        taf_pa_file_Close(destFd);
        taf_pa_file_Delete(destPath);
        return savedErrno;
    }

    taf_pa_file_Close(srcFd);
    taf_pa_file_Close(destFd);

    return 0;
}

int taf_pa_file_Rename
(
    const char *sourcePath,
    const char *destPath
)
{
    return rename(sourcePath, destPath);
}
