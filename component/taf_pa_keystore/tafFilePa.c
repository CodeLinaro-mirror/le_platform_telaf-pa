/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "tafFilePa.h"
#include "tafInternalCommonPa.h"
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
#include <selinux/selinux.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

//--------------------------------------------------------------------------------------------------
/**
 * Size of the buffer used to compute checksums.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_HASH_BUFFER_SIZE    65536

//--------------------------------------------------------------------------------------------------
/**
 * Size of the string holding a CRC32C value ("%08x" + NUL).
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_CRC_STR_SIZE        9

//--------------------------------------------------------------------------------------------------
/**
 * Size of the string holding the SHA1 of a file path (hex + NUL).
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_PATH_HASH_SIZE      (SHA_DIGEST_LENGTH * 2 + 1)

//--------------------------------------------------------------------------------------------------
/**
 * Maximum length of the backup storage directory path, so that a backup path always fits in a
 * TAF_PA_FILE_MAX_BACKUP_FILENAME sized buffer.
 */
//--------------------------------------------------------------------------------------------------
#define TAF_PA_FILE_MAX_BACKUP_DIR      (TAF_PA_FILE_MAX_BACKUP_FILENAME - \
                                         sizeof(TAF_PA_FILE_BACKUP_PREFIX) - \
                                         TAF_PA_FILE_PATH_HASH_SIZE)

//--------------------------------------------------------------------------------------------------
/**
 * Backup feature state.
 *
 * Not synchronized: this module is only reachable through the taf_prop_file_vtable_t injected by
 * taf_pa_ks_Init() (tafKeystorePa.c), and the KeyStore service that owns that vtable calls into it
 * from a single thread. If a second caller/thread is ever wired to these entry points, this state
 * (and the CRC32C table below) will need real synchronization - do not assume it is safe by then.
 */
//--------------------------------------------------------------------------------------------------
static bool BackupEnabled = true;
static bool Initialized = false;
static char BackupStorage[TAF_PA_FILE_MAX_BACKUP_DIR] = {0};
static uint32_t MaxFileSize = TAF_PA_FILE_DEFAULT_MAX_SIZE;
static uint16_t MaxFileCount = TAF_PA_FILE_DEFAULT_MAX_COUNT;
static taf_pa_file_ErrorHandler_t ErrorHandlerFunc = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * CRC32C (Castagnoli) slice-by-8 lookup tables: Crc32cTable[0] is the classic byte-at-a-time
 * table, Crc32cTable[1..7] let UpdateCRC32C() fold 8 input bytes per iteration instead of 1.
 *
 * Crc32cTableInitialized is a plain (non-atomic) guard: see the single-threaded-caller note above
 * InitializeCRC32CTable() for why this is safe under the current KeyStore-only usage.
 */
//--------------------------------------------------------------------------------------------------
static uint32_t Crc32cTable[8][256];
static bool Crc32cTableInitialized = false;

//--------------------------------------------------------------------------------------------------
/**
 * Report an error through the registered handler, if any.
 */
//--------------------------------------------------------------------------------------------------
static void ReportError
(
    taf_pa_file_Error_t error,
    const char* filePathPtr
)
{
    if (ErrorHandlerFunc)
    {
        ErrorHandlerFunc(error, filePathPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Build the CRC32C lookup table on first use.
 *
 * Guarded by the plain Crc32cTableInitialized flag rather than pthread_once: every call into this
 * module happens on the single KeyStore service thread (see the note above BackupEnabled), so
 * there is no concurrent-first-call race to protect against here.
 */
//--------------------------------------------------------------------------------------------------
static void InitializeCRC32CTable
(
    void
)
{
    if (Crc32cTableInitialized)
    {
        return;
    }

    // Reflected Castagnoli polynomial (CRC32C).
    const uint32_t polynomial = 0x82F63B78U;

    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc = i;

        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ polynomial) : (crc >> 1);
        }

        Crc32cTable[0][i] = crc;
    }

    // Derive the slice-by-8 rows from row 0: Table[k][n] = (Table[k-1][n] >> 8) ^
    // Table[0][Table[k-1][n] & 0xFF]. This is the standard slice-by-8 construction and yields
    // the exact same CRC32C polynomial/reflection as the byte-at-a-time table above.
    for (uint32_t n = 0; n < 256; n++)
    {
        uint32_t crc = Crc32cTable[0][n];
        for (uint32_t k = 1; k < 8; k++)
        {
            crc = Crc32cTable[0][crc & 0xffU] ^ (crc >> 8);
            Crc32cTable[k][n] = crc;
        }
    }

    Crc32cTableInitialized = true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Update a running CRC32C value with a data block.
 *
 * Processes 8 bytes per iteration (slice-by-8) when enough data is available, falling back to a
 * byte-at-a-time tail loop for the last 0-7 bytes. The 8-byte word is loaded with memcpy() to
 * avoid alignment/strict-aliasing UB; the target is confirmed little-endian AArch64.
 */
//--------------------------------------------------------------------------------------------------
static uint32_t UpdateCRC32C
(
    uint32_t crc,
    const unsigned char* dataPtr,
    size_t length
)
{
    InitializeCRC32CTable();

    while (length >= 8)
    {
        uint32_t low, high;
        taf_pa_memscpy(&low, sizeof(low), dataPtr, sizeof(low));
        taf_pa_memscpy(&high, sizeof(high), dataPtr + 4, sizeof(high));

        low ^= crc;

        crc = Crc32cTable[7][low & 0xffU] ^
              Crc32cTable[6][(low >> 8) & 0xffU] ^
              Crc32cTable[5][(low >> 16) & 0xffU] ^
              Crc32cTable[4][(low >> 24) & 0xffU] ^
              Crc32cTable[3][high & 0xffU] ^
              Crc32cTable[2][(high >> 8) & 0xffU] ^
              Crc32cTable[1][(high >> 16) & 0xffU] ^
              Crc32cTable[0][(high >> 24) & 0xffU];

        dataPtr += 8;
        length -= 8;
    }

    while (length-- > 0)
    {
        crc = Crc32cTable[0][(crc ^ *dataPtr++) & 0xffU] ^ (crc >> 8);
    }

    return crc;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute the CRC32C of a file content.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t CalculateFileCRC32C
(
    const char* filePathPtr,
    char* crcStrPtr,
    size_t crcStrSize
)
{
    if (!filePathPtr || !crcStrPtr || crcStrSize < TAF_PA_FILE_CRC_STR_SIZE)
    {
        PA_ERROR("Invalid parameter to compute file checksum");
        return PA_FAULT;
    }

    int fd = open(filePathPtr, O_RDONLY);
    if (fd < 0)
    {
        PA_ERROR("Failed to open file %s to compute checksum, errno: %d", filePathPtr, errno);
        return PA_NOT_FOUND;
    }

    // Hint the kernel that the file will be read sequentially and in full, so it can read ahead
    // more aggressively. Purely advisory: failure to apply the hint does not affect correctness.
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    uint32_t crc = 0xFFFFFFFFU;
    unsigned char buffer[TAF_PA_FILE_HASH_BUFFER_SIZE];
    pa_result_t result = PA_OK;

    for (;;)
    {
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead == 0)
        {
            break;
        }

        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            PA_ERROR("Failed to read file %s to compute checksum, errno: %d", filePathPtr, errno);
            result = PA_FAULT;
            break;
        }

        crc = UpdateCRC32C(crc, buffer, (size_t)bytesRead);
    }

    close(fd);

    if (result != PA_OK)
    {
        return result;
    }

    snprintf(crcStrPtr, crcStrSize, "%08x", crc ^ 0xFFFFFFFFU);

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute the CRC32C of the content behind an already-open, seekable file descriptor.
 *
 * Reads from offset 0 via pread(), leaving the descriptor's file offset untouched. Used by
 * taf_pa_file_Close() to checksum a file without reopening its path.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t CalculateFdCRC32C
(
    int fd,
    char* crcStrPtr,
    size_t crcStrSize
)
{
    if ((fd < 0) || !crcStrPtr || crcStrSize < TAF_PA_FILE_CRC_STR_SIZE)
    {
        PA_ERROR("Invalid parameter to compute file descriptor checksum");
        return PA_FAULT;
    }

    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    uint32_t crc = 0xFFFFFFFFU;
    unsigned char buffer[TAF_PA_FILE_HASH_BUFFER_SIZE];
    off_t offset = 0;

    for (;;)
    {
        ssize_t bytesRead = pread(fd, buffer, sizeof(buffer), offset);
        if (bytesRead == 0)
        {
            break;
        }

        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            PA_ERROR("Failed to read fd %d to compute checksum, errno: %d", fd, errno);
            return PA_FAULT;
        }

        crc = UpdateCRC32C(crc, buffer, (size_t)bytesRead);
        offset += bytesRead;
    }

    snprintf(crcStrPtr, crcStrSize, "%08x", crc ^ 0xFFFFFFFFU);

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Store a given CRC32C value into the file extended attribute.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t SetFileCRC32CToExtendedAttrValue
(
    const char* filePathPtr,
    const char* crcStrPtr
)
{
    if (setxattr(filePathPtr, TAF_PA_FILE_EXTENDED_ATTR_CRC32C,
                 crcStrPtr, strlen(crcStrPtr), 0) < 0)
    {
        PA_ERROR("Failed to set checksum attribute, errno: %d", errno);
        ReportError(TAF_PA_FILE_ERR_SET_HASH, filePathPtr);
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read the CRC32C stored in the file extended attribute.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t GetFileCRC32CFromExtendedAttr
(
    const char* filePathPtr,
    char* crcStrPtr,
    size_t crcStrSize
)
{
    ssize_t length = getxattr(filePathPtr, TAF_PA_FILE_EXTENDED_ATTR_CRC32C,
                              crcStrPtr, crcStrSize);
    if (length <= 0)
    {
        PA_WARN("No checksum attribute found on %s, errno: %d", filePathPtr, errno);
        return PA_NOT_FOUND;
    }

    if ((size_t)length >= crcStrSize)
    {
        PA_ERROR("Checksum attribute of %s is too long", filePathPtr);
        return PA_FAULT;
    }

    crcStrPtr[length] = '\0';

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check the file content against the CRC32C stored in its extended attribute.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t ValidateFileCRC32CWithExtendedAttr
(
    const char* filePathPtr
)
{
    char storedCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    char calculatedCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};

    // Propagate the exact status (e.g. PA_NOT_FOUND when the checksum attribute is simply
    // missing) instead of collapsing it to PA_FAULT, so callers can tell a never-tracked file
    // apart from an actually corrupted one.
    pa_result_t result = GetFileCRC32CFromExtendedAttr(filePathPtr, storedCrc, sizeof(storedCrc));
    if (result != PA_OK)
    {
        return result;
    }

    if (CalculateFileCRC32C(filePathPtr, calculatedCrc, sizeof(calculatedCrc)) != PA_OK)
    {
        PA_ERROR("Failed to compute checksum of %s for integrity check", filePathPtr);
        return PA_FAULT;
    }

    if (strncmp(storedCrc, calculatedCrc, TAF_PA_FILE_CRC_STR_SIZE - 1) != 0)
    {
        PA_WARN("Checksum mismatch for %s (stored: %s, calculated: %s)",
                filePathPtr, storedCrc, calculatedCrc);
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a directory and all its missing parents.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t CreateDirTree
(
    const char* pathPtr
)
{
    char path[PATH_MAX] = {0};

    if (!pathPtr || !pathPtr[0] || (strlen(pathPtr) >= sizeof(path)))
    {
        PA_ERROR("Invalid directory path parameter");
        return PA_BAD_PARAMETER;
    }

    snprintf(path, sizeof(path), "%s", pathPtr);

    for (char* cursorPtr = path + 1; *cursorPtr; cursorPtr++)
    {
        if (*cursorPtr != '/')
        {
            continue;
        }

        *cursorPtr = '\0';
        if ((mkdir(path, 0700) != 0) && (errno != EEXIST))
        {
            PA_ERROR("Failed to create directory %s, errno: %d", path, errno);
            return PA_FAULT;
        }
        *cursorPtr = '/';
    }

    if ((mkdir(path, 0700) != 0) && (errno != EEXIST))
    {
        PA_ERROR("Failed to create directory %s, errno: %d", path, errno);
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Count the backup files contained in the backup storage.
 *
 * The backup directory may be shared with other files, so only the entries carrying the backup
 * prefix are taken into account.
 */
//--------------------------------------------------------------------------------------------------
static uint16_t CountBackupFiles
(
    const char* pathPtr
)
{
    uint16_t count = 0;
    DIR* dirPtr = opendir(pathPtr);

    if (!dirPtr)
    {
        return 0;
    }

    struct dirent* entryPtr;
    while ((entryPtr = readdir(dirPtr)) != NULL)
    {
        // Skip "*.tmp" leftovers from a CopyFileContent() that got interrupted (crash/power loss)
        // before its final rename(): they are not valid backups and must not count against
        // MaxFileCount.
        size_t nameLen = strlen(entryPtr->d_name);
        static const char tmpSuffix[] = ".tmp";
        bool isTmp = (nameLen >= sizeof(tmpSuffix) - 1) &&
                     (strcmp(entryPtr->d_name + nameLen - (sizeof(tmpSuffix) - 1),
                             tmpSuffix) == 0);

        bool isRegular = (entryPtr->d_type == DT_REG);
        if (!isRegular && (entryPtr->d_type == DT_UNKNOWN))
        {
            // Filesystem does not support d_type; fall back to stat().
            char fullPath[PATH_MAX];
            struct stat entryStat;
            if ((snprintf(fullPath, sizeof(fullPath), "%s/%s", pathPtr, entryPtr->d_name) < (int)sizeof(fullPath)) &&
                (stat(fullPath, &entryStat) == 0))
            {
                isRegular = S_ISREG(entryStat.st_mode);
            }
        }

        if (isRegular && !isTmp &&
            (strncmp(entryPtr->d_name, TAF_PA_FILE_BACKUP_PREFIX,
                     strlen(TAF_PA_FILE_BACKUP_PREFIX)) == 0))
        {
            count++;
        }
    }

    closedir(dirPtr);

    return count;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute the SHA1 of a file path, used as the backup file name.
 */
//--------------------------------------------------------------------------------------------------
static void CalculateFilePathSHA1
(
    const char* filePathPtr,
    char* outputPtr,
    size_t outputSize
)
{
    if (!filePathPtr || !outputPtr || outputSize < TAF_PA_FILE_PATH_HASH_SIZE)
    {
        if (outputPtr && outputSize)
        {
            outputPtr[0] = '\0';
        }
        return;
    }

    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)filePathPtr, strlen(filePathPtr), digest);

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        snprintf(outputPtr + (i * 2), 3, "%02x", digest[i]);
    }

    outputPtr[SHA_DIGEST_LENGTH * 2] = '\0';
}

//--------------------------------------------------------------------------------------------------
/**
 * Build the backup path corresponding to a primary file path.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t GetBackupPath
(
    const char* filePathPtr,
    char* backupPathPtr,
    size_t backupPathSize
)
{
    char pathHash[TAF_PA_FILE_PATH_HASH_SIZE] = {0};

    CalculateFilePathSHA1(filePathPtr, pathHash, sizeof(pathHash));

    if (!pathHash[0])
    {
        PA_ERROR("Failed to calculate path hash for %s", filePathPtr);
        return PA_FAULT;
    }

    int written = snprintf(backupPathPtr, backupPathSize, "%s%s%s",
                           BackupStorage, TAF_PA_FILE_BACKUP_PREFIX, pathHash);
    if ((written < 0) || ((size_t)written >= backupPathSize))
    {
        PA_ERROR("Backup path is too long");
        return PA_OVERFLOW;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Flush a file content to the storage.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t PersistFile
(
    const char* filePathPtr
)
{
    int fd = open(filePathPtr, O_RDONLY);
    if (fd < 0)
    {
        PA_ERROR("Failed to open %s to persist it, errno: %d", filePathPtr, errno);
        return PA_FAULT;
    }

    int result = fsync(fd);
    if (result != 0)
    {
        PA_ERROR("Failed to fsync %s, errno: %d", filePathPtr, errno);
    }
    close(fd);

    return (result == 0) ? PA_OK : PA_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Flush the directory entry of a file to the storage.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t PersistParentDir
(
    const char* filePathPtr
)
{
    char dirPath[PATH_MAX] = {0};

    int written = snprintf(dirPath, sizeof(dirPath), "%s", filePathPtr);
    if ((written < 0) || ((size_t)written >= sizeof(dirPath)))
    {
        PA_ERROR("Path is too long while persisting parent directory: %s", filePathPtr);
        return PA_OVERFLOW;
    }

    char* slashPtr = strrchr(dirPath, '/');
    if (!slashPtr)
    {
        return PA_OK;
    }

    if (slashPtr == dirPath)
    {
        slashPtr[1] = '\0';
    }
    else
    {
        *slashPtr = '\0';
    }

    int fd = open(dirPath, O_RDONLY | O_DIRECTORY);
    if (fd < 0)
    {
        PA_ERROR("Failed to open directory %s to persist it, errno: %d", dirPath, errno);
        return PA_FAULT;
    }

    int result = fsync(fd);
    if (result != 0)
    {
        PA_ERROR("Failed to fsync directory %s, errno: %d", dirPath, errno);
    }
    close(fd);

    return (result == 0) ? PA_OK : PA_FAULT;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy the SELinux context from one open file to another.
 *
 * Filesystems without SELinux labeling are supported, but when a source label exists it must be
 * applied successfully before the destination is published.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t CopySELinuxContext
(
    int sourceFd,
    int targetFd,
    const char* sourcePathPtr,
    const char* targetPathPtr
)
{
    char* secContextPtr = NULL;
    errno = 0;
    int contextLength = fgetfilecon(sourceFd, &secContextPtr);
    if (contextLength < 0)
    {
        if ((errno == ENODATA) || (errno == ENOTSUP) || (errno == ERANGE))
        {
            return PA_OK;
        }

        PA_ERROR("Failed to read SELinux context of %s, errno: %d", sourcePathPtr, errno);
        return PA_FAULT;
    }

    pa_result_t result = PA_OK;
    if (fsetfilecon(targetFd, secContextPtr) < 0)
    {
        PA_ERROR("Failed to set SELinux context on %s from %s, errno: %d",
                 targetPathPtr, sourcePathPtr, errno);
        result = PA_FAULT;
    }

    freecon(secContextPtr);
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy the whole content of a file descriptor into another one with a read/write loop.
 *
 * Used as a fallback when sendfile() cannot be used on the involved file systems.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t CopyByReadWrite
(
    int inputFd,
    int outputFd,
    off_t* copiedPtr
)
{
    uint8_t buffer[RFS_COPY_BUFFER_SIZE];

    for (;;)
    {
        ssize_t bytesRead = read(inputFd, buffer, sizeof(buffer));
        if (bytesRead == 0)
        {
            return PA_OK;
        }

        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            PA_ERROR("Failed to read source file, errno: %d", errno);
            return PA_FAULT;
        }

        ssize_t written = 0;
        while (written < bytesRead)
        {
            ssize_t chunk = write(outputFd, buffer + written, (size_t)(bytesRead - written));
            if (chunk < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                PA_ERROR("Failed to write target file, errno: %d", errno);
                return PA_FAULT;
            }
            written += chunk;
            *copiedPtr += chunk;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy a file content, keeping the source permissions (without execution rights).
 *
 * Written to a "<targetPath>.tmp" sibling first and atomically rename()'d onto targetPath once
 * the copy is complete and flushed. Without this, a crash/power loss mid-copy (e.g. mid-sendfile,
 * before fsync()) would leave targetPath itself truncated or partially written - if targetPath is
 * a backup, RefreshBackupIfNeeded()'s checksum check catches that and simply redoes the backup on
 * next use; if targetPath is a primary file being restored (see ReplaceFileWithBackup()), the same
 * crash would instead destroy the only remaining good copy the caller was trying to recover.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t CopyFileContent
(
    const char* sourcePath,
    const char* targetPath
)
{
    char tmpPath[PATH_MAX] = {0};
    int tmpLen = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", targetPath);
    if ((tmpLen < 0) || ((size_t)tmpLen >= sizeof(tmpPath)))
    {
        PA_ERROR("Target path %s is too long to build a temporary copy path", targetPath);
        return PA_FAULT;
    }

    int inputFd = open(sourcePath, O_RDONLY);
    if (inputFd < 0)
    {
        PA_ERROR("Failed to open source file %s, errno: %d", sourcePath, errno);
        return PA_FAULT;
    }

    struct stat statBuffer;
    if (fstat(inputFd, &statBuffer) < 0)
    {
        PA_ERROR("Failed to stat source file %s, errno: %d", sourcePath, errno);
        close(inputFd);
        return PA_FAULT;
    }

    mode_t destinationMode = statBuffer.st_mode & ~(S_IXUSR | S_IXGRP | S_IXOTH);
    int outputFd = open(tmpPath, O_WRONLY | O_CREAT | O_TRUNC, destinationMode);
    if (outputFd < 0)
    {
        PA_ERROR("Failed to open temporary file %s, errno: %d", tmpPath, errno);
        close(inputFd);
        return PA_FAULT;
    }

    // sendfile() may copy less than requested (it is not guaranteed to transfer everything in
    // one call), so loop until the whole source content has been transferred. Stopping after a
    // single call would silently produce a truncated copy, i.e. a restored/backed up file that
    // is not identical to the original one.
    pa_result_t result = PA_OK;
    off_t offset = 0;
    off_t remaining = statBuffer.st_size;
    bool useReadWrite = false;

    while (remaining > 0)
    {
        ssize_t copied = sendfile(outputFd, inputFd, &offset, (size_t)remaining);
        if (copied < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            // Some file systems do not support sendfile() on this pair of descriptors, in that
            // case fall back to a plain read/write copy instead of failing the operation.
            if ((errno == EINVAL) || (errno == ENOSYS) || (errno == EOPNOTSUPP))
            {
                PA_DEBUG("sendfile() unsupported for %s -> %s, using read/write copy",
                         sourcePath, tmpPath);
                useReadWrite = true;
                break;
            }

            PA_ERROR("Failed to copy %s to %s, errno: %d", sourcePath, tmpPath, errno);
            result = PA_FAULT;
            break;
        }

        if (copied == 0)
        {
            // A source that shrinks during the copy must not be published as a successful backup
            // or restore. The caller can retry against a stable source later.
            PA_ERROR("Source file %s ended before its expected size", sourcePath);
            result = PA_FAULT;
            break;
        }

        remaining -= copied;
    }

    if (useReadWrite)
    {
        if ((lseek(inputFd, offset, SEEK_SET) < 0) || (lseek(outputFd, offset, SEEK_SET) < 0))
        {
            PA_ERROR("Failed to rewind %s/%s for read/write copy, errno: %d",
                     sourcePath, tmpPath, errno);
            result = PA_FAULT;
            goto cleanup;
        }

        result = CopyByReadWrite(inputFd, outputFd, &offset);
    }

    // Make sure the target holds exactly the source content: the target may be an existing
    // (longer) file, and O_TRUNC alone is not enough if the copy loop stopped early.
    if ((result == PA_OK) && (ftruncate(outputFd, offset) < 0) && (errno != EINVAL))
    {
        PA_ERROR("Failed to truncate %s to %jd bytes, errno: %d",
                 tmpPath, (intmax_t)offset, errno);
        result = PA_FAULT;
    }

    // Flush the copied data before the descriptor is closed, so a later checksum computation
    // and the caller's persist step always see the complete content.
    if ((result == PA_OK) && (fsync(outputFd) != 0))
    {
        PA_ERROR("Failed to flush %s, errno: %d", tmpPath, errno);
        result = PA_FAULT;
    }

    if (result == PA_OK)
    {
        result = CopySELinuxContext(inputFd, outputFd, sourcePath, tmpPath);
    }

cleanup:
    if (close(inputFd) != 0)
    {
        PA_ERROR("Failed to close source file %s, errno: %d", sourcePath, errno);
        result = PA_FAULT;
    }
    if (close(outputFd) != 0)
    {
        PA_ERROR("Failed to close temporary file %s, errno: %d", tmpPath, errno);
        result = PA_FAULT;
    }

    if (result != PA_OK)
    {
        unlink(tmpPath);
        return result;
    }

    // Atomically publish the finished copy: a crash/power loss before this point leaves targetPath
    // untouched and tmpPath as an orphan (harmless, overwritten by the next attempt); one after
    // this point leaves targetPath fully updated. There is no state in between where targetPath
    // could be observed half-written.
    if (rename(tmpPath, targetPath) != 0)
    {
        PA_ERROR("Failed to publish %s from %s, errno: %d", targetPath, tmpPath, errno);
        unlink(tmpPath);
        return PA_FAULT;
    }

    if (PersistParentDir(targetPath) != PA_OK)
    {
        PA_ERROR("Failed to persist published file %s", targetPath);
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Copy a file and its SELinux context into the backup storage.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t BackUpFileAndSELinuxContext
(
    const char* sourcePath,
    const char* targetPath
)
{
    if (CopyFileContent(sourcePath, targetPath) != PA_OK)
    {
        PA_ERROR("Failed to copy %s to backup location %s", sourcePath, targetPath);
        return PA_FAULT;
    }

    if (PersistFile(targetPath) != PA_OK)
    {
        PA_ERROR("Failed to persist backup file %s", targetPath);
        return PA_FAULT;
    }

    if (PersistParentDir(targetPath) != PA_OK)
    {
        PA_ERROR("Failed to persist backup directory entry for %s", targetPath);
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create or refresh the backup copy of a file.
 *
 * @param filePathPtr           Path of the primary file to back up.
 * @param knownPrimaryCrcPtr    CRC32C already computed for filePathPtr's current content by the
 *                              caller (e.g. right after writing/validating it), or NULL/empty if
 *                              no such value is available. When given, it is trusted instead of
 *                              re-reading and re-checksumming the primary file: the mismatch check
 *                              below still cross-validates it against the bytes actually copied
 *                              into the backup, so a stale value is still caught (as a mismatch)
 *                              rather than silently accepted.
 *
 * @note The "does a new backup slot exist yet" check against MaxFileCount below and the actual
 *       backup creation are two separate steps with no lock spanning them, so two concurrent
 *       callers could each see room for one more backup and both create one, exceeding
 *       MaxFileCount by one. Not addressed here: see the single-threaded-caller note above
 *       taf_pa_file_Init() in tafFilePa.h - this module has exactly one caller and it never
 *       invokes these entry points concurrently, so this window cannot be hit in practice.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t BackupFileToStorage
(
    const char* filePathPtr,
    const char* knownPrimaryCrcPtr
)
{
    struct stat statBuffer;
    if ((stat(filePathPtr, &statBuffer) == 0) && ((uint64_t)statBuffer.st_size > MaxFileSize))
    {
        PA_ERROR("File too big to be backed up");
        ReportError(TAF_PA_FILE_ERR_NO_MEMORY, filePathPtr);
        return PA_NO_MEMORY;
    }

    // The backup storage directory may have been removed after initialization (e.g. wiped
    // partition, factory reset, manual cleanup). Recreate it on demand so the backup below
    // does not silently fail because its parent directory is missing.
    struct stat storageStat;
    if ((stat(BackupStorage, &storageStat) != 0) || !S_ISDIR(storageStat.st_mode))
    {
        PA_DEBUG("Backup storage directory missing, recreating: %s", BackupStorage);
        if (CreateDirTree(BackupStorage) != PA_OK)
        {
            PA_ERROR("Failed to recreate backup storage directory");
            ReportError(TAF_PA_FILE_ERR_BACKUP, filePathPtr);
            return PA_FAULT;
        }
    }

    char backupPath[TAF_PA_FILE_MAX_BACKUP_FILENAME] = {0};
    if (GetBackupPath(filePathPtr, backupPath, sizeof(backupPath)) != PA_OK)
    {
        PA_ERROR("Failed to build backup path for %s", filePathPtr);
        return PA_FAULT;
    }

    // The capacity limit only applies when a new backup entry has to be created.
    if ((access(backupPath, F_OK) != 0) && (CountBackupFiles(BackupStorage) >= MaxFileCount))
    {
        PA_ERROR("Backup storage is full");
        ReportError(TAF_PA_FILE_ERR_NO_MEMORY, filePathPtr);
        return PA_NO_MEMORY;
    }

    if (BackUpFileAndSELinuxContext(filePathPtr, backupPath) != PA_OK)
    {
        PA_ERROR("Failed to back up file");
        ReportError(TAF_PA_FILE_ERR_BACKUP, filePathPtr);
        return PA_FAULT;
    }

    // Checksum the bytes that were actually written into the backup, and cross-check them against
    // the primary. Labelling the backup with the checksum stored on the primary would be wrong
    // whenever that stored value is stale (the primary was modified without going through
    // taf_pa_file_Close()): the backup would then carry a checksum that does not describe its own
    // content, and a later restore would reinstate content that differs from the original file.
    char backupCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    if (CalculateFileCRC32C(backupPath, backupCrc, sizeof(backupCrc)) != PA_OK)
    {
        PA_ERROR("Failed to compute checksum of backup file %s", backupPath);
        return PA_FAULT;
    }

    char primaryCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    if (knownPrimaryCrcPtr && knownPrimaryCrcPtr[0])
    {
        // Trust the caller-supplied value instead of re-reading the whole primary file again:
        // if it turns out to be stale, the strncmp() below still detects the discrepancy against
        // the backup's actual content and discards the backup, exactly as a freshly computed
        // mismatch would.
        snprintf(primaryCrc, sizeof(primaryCrc), "%s", knownPrimaryCrcPtr);
    }
    else if (CalculateFileCRC32C(filePathPtr, primaryCrc, sizeof(primaryCrc)) != PA_OK)
    {
        PA_ERROR("Failed to compute checksum of %s", filePathPtr);
        return PA_FAULT;
    }

    if (strncmp(primaryCrc, backupCrc, TAF_PA_FILE_CRC_STR_SIZE - 1) != 0)
    {
        PA_ERROR("Backup %s does not match %s (primary: %s, backup: %s), discarding it",
                 backupPath, filePathPtr, primaryCrc, backupCrc);
        if ((unlink(backupPath) != 0) && (errno != ENOENT))
        {
            PA_ERROR("Failed to discard invalid backup %s, errno: %d", backupPath, errno);
        }
        else if (PersistParentDir(backupPath) != PA_OK)
        {
            PA_ERROR("Failed to persist discarded backup %s", backupPath);
        }
        ReportError(TAF_PA_FILE_ERR_BACKUP, filePathPtr);
        return PA_FAULT;
    }

    // Keep the primary's checksum in sync with its current content, so the primary and its
    // backup always advertise the same value.
    if (SetFileCRC32CToExtendedAttrValue(filePathPtr, primaryCrc) != PA_OK)
    {
        PA_ERROR("Failed to store checksum of %s", filePathPtr);
        return PA_FAULT;
    }

    if (SetFileCRC32CToExtendedAttrValue(backupPath, backupCrc) != PA_OK)
    {
        PA_ERROR("Failed to store checksum on backup file %s", backupPath);
        return PA_FAULT;
    }

    return PersistFile(backupPath);
}

//--------------------------------------------------------------------------------------------------
/**
 * Refresh the backup copy of a file when it is missing, corrupted or out of date.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t RefreshBackupIfNeeded
(
    const char* filePathPtr,
    const char* primaryCrcPtr
)
{
    if (!primaryCrcPtr || !primaryCrcPtr[0])
    {
        PA_ERROR("Invalid primary checksum parameter for %s", filePathPtr);
        return PA_BAD_PARAMETER;
    }

    char backupPath[TAF_PA_FILE_MAX_BACKUP_FILENAME] = {0};
    if (GetBackupPath(filePathPtr, backupPath, sizeof(backupPath)) != PA_OK)
    {
        PA_ERROR("Failed to build backup path for %s", filePathPtr);
        return PA_FAULT;
    }

    char backupCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    if ((GetFileCRC32CFromExtendedAttr(backupPath, backupCrc, sizeof(backupCrc)) != PA_OK) ||
        (ValidateFileCRC32CWithExtendedAttr(backupPath) != PA_OK) ||
        (strncmp(primaryCrcPtr, backupCrc, TAF_PA_FILE_CRC_STR_SIZE - 1) != 0))
    {
        PA_WARN("Backup of %s is missing, corrupted or out of date, refreshing it", filePathPtr);
        return BackupFileToStorage(filePathPtr, primaryCrcPtr);
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Restore a file from its backup copy.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t ReplaceFileWithBackup
(
    const char* filePathPtr
)
{
    char backupPath[TAF_PA_FILE_MAX_BACKUP_FILENAME] = {0};
    if (GetBackupPath(filePathPtr, backupPath, sizeof(backupPath)) != PA_OK)
    {
        PA_ERROR("Failed to build backup path for %s", filePathPtr);
        return PA_FAULT;
    }

    // Read the known-good checksum from the backup *before* restoring. The restored primary must
    // be validated against this value: recomputing a checksum from the restored file instead
    // would blindly "bless" whatever was written (e.g. a truncated or partially written copy),
    // making an invalid restore look successful.
    char backupCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    if (GetFileCRC32CFromExtendedAttr(backupPath, backupCrc, sizeof(backupCrc)) != PA_OK)
    {
        PA_ERROR("Failed to read checksum of backup file %s", backupPath);
        return PA_FAULT;
    }

    if (ValidateFileCRC32CWithExtendedAttr(backupPath) != PA_OK)
    {
        PA_WARN("Backup file %s for %s is missing or corrupted, cannot restore",
                backupPath, filePathPtr);
        return PA_FAULT;
    }

    PA_WARN("Restoring %s from its backup copy %s", filePathPtr, backupPath);

    if (CopyFileContent(backupPath, filePathPtr) != PA_OK)
    {
        PA_ERROR("Failed to copy backup %s onto %s", backupPath, filePathPtr);
        return PA_FAULT;
    }

    // Verify that the restored content really matches the backup before advertising it as valid.
    char restoredCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    if (CalculateFileCRC32C(filePathPtr, restoredCrc, sizeof(restoredCrc)) != PA_OK)
    {
        PA_ERROR("Failed to compute checksum of %s after restore", filePathPtr);
        return PA_FAULT;
    }

    if (strncmp(backupCrc, restoredCrc, TAF_PA_FILE_CRC_STR_SIZE - 1) != 0)
    {
        PA_ERROR("Restored %s does not match its backup %s (backup: %s, restored: %s)",
                 filePathPtr, backupPath, backupCrc, restoredCrc);
        return PA_FAULT;
    }

    // Store the checksum inherited from the backup, so the primary and its backup stay
    // consistent and a later RefreshBackupIfNeeded() does not consider the backup stale.
    if (SetFileCRC32CToExtendedAttrValue(filePathPtr, backupCrc) != PA_OK)
    {
        PA_ERROR("Failed to update checksum of %s after restore", filePathPtr);
        return PA_FAULT;
    }

    if (PersistFile(filePathPtr) != PA_OK)
    {
        PA_ERROR("Failed to persist %s after restore", filePathPtr);
        return PA_FAULT;
    }

    if (PersistParentDir(filePathPtr) != PA_OK)
    {
        PA_ERROR("Failed to persist restored file directory entry for %s", filePathPtr);
        return PA_FAULT;
    }

    return PA_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check whether a file already has a valid backup copy.
 *
 * A missing checksum extended attribute on the primary does not necessarily mean the file was
 * never tracked: the attribute lives on the inode, so any corruption that replaces the primary's
 * inode (cp, editor save, sed -i, etc.) silently drops it along with the good content while
 * leaving a perfectly valid backup behind. Callers must use this check to tell that case apart
 * from a genuinely new/untracked file before deciding to (re)create the backup from the primary's
 * current - possibly corrupted - content.
 */
//--------------------------------------------------------------------------------------------------
static bool HasValidBackup
(
    const char* filePathPtr
)
{
    char backupPath[TAF_PA_FILE_MAX_BACKUP_FILENAME] = {0};

    if (GetBackupPath(filePathPtr, backupPath, sizeof(backupPath)) != PA_OK)
    {
        return false;
    }

    return (ValidateFileCRC32CWithExtendedAttr(backupPath) == PA_OK);
}

//--------------------------------------------------------------------------------------------------
/**
 * Check that a file's backup is valid and matches the file's current content.
 *
 * A valid backup alone is insufficient after a failed refresh: it may still be the previous
 * backup for the same path. Compare both CRCs before treating a copy or rename as protected.
 */
//--------------------------------------------------------------------------------------------------
static bool HasMatchingBackup
(
    const char* filePathPtr
)
{
    char fileCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    char backupCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    char backupPath[TAF_PA_FILE_MAX_BACKUP_FILENAME] = {0};

    if ((CalculateFileCRC32C(filePathPtr, fileCrc, sizeof(fileCrc)) != PA_OK) ||
        (GetBackupPath(filePathPtr, backupPath, sizeof(backupPath)) != PA_OK) ||
        (GetFileCRC32CFromExtendedAttr(backupPath, backupCrc, sizeof(backupCrc)) != PA_OK) ||
        (ValidateFileCRC32CWithExtendedAttr(backupPath) != PA_OK))
    {
        return false;
    }

    return (strncmp(fileCrc, backupCrc, TAF_PA_FILE_CRC_STR_SIZE - 1) == 0);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove the backup copy of a file.
 */
//--------------------------------------------------------------------------------------------------
static pa_result_t DeleteBackup
(
    const char* filePathPtr
)
{
    char backupPath[TAF_PA_FILE_MAX_BACKUP_FILENAME] = {0};

    if (GetBackupPath(filePathPtr, backupPath, sizeof(backupPath)) != PA_OK)
    {
        PA_ERROR("Failed to build backup path for %s, cannot delete its backup", filePathPtr);
        return PA_FAULT;
    }

    if ((unlink(backupPath) != 0) && (errno != ENOENT))
    {
        PA_ERROR("Failed to delete backup file %s, errno: %d", backupPath, errno);
        return PA_FAULT;
    }

    // Persist the deletion so a stale backup cannot reappear after a crash and later be used for
    // restore under the same primary path.
    if (PersistParentDir(backupPath) != PA_OK)
    {
        PA_ERROR("Failed to persist deletion of backup file %s", backupPath);
        return PA_FAULT;
    }

    return PA_OK;
}

pa_result_t taf_pa_file_Init
(
    bool enableBackup,
    taf_pa_file_ErrorHandler_t handlerFunc
)
{
    PA_INFO("%s, enableBackup: %d", __FUNCTION__, enableBackup);

    BackupEnabled = enableBackup;
    ErrorHandlerFunc = handlerFunc;

    // Skip setup when backup is disabled, or when it was already fully initialized (storage
    // directory created and BackupStorage populated) by a previous call with enableBackup=true.
    // Do NOT short-circuit when Initialized=true but BackupEnabled was false on that first call:
    // BackupStorage would still be empty and every subsequent GetBackupPath() would produce a
    // relative path in the current working directory.
    if (!BackupEnabled || (Initialized && BackupStorage[0] != '\0'))
    {
        Initialized = true;
        return PA_OK;
    }

    // Generic default, see TAF_PA_FILE_BACKUP_STORAGE. Overridable through
    // taf_pa_file_SetBackupStorage().
    if (CreateDirTree(TAF_PA_FILE_BACKUP_STORAGE) != PA_OK)
    {
        PA_ERROR("Failed to create default backup storage directory %s",
                 TAF_PA_FILE_BACKUP_STORAGE);
        return PA_FAULT;
    }

    snprintf(BackupStorage, sizeof(BackupStorage), "%s", TAF_PA_FILE_BACKUP_STORAGE);
    Initialized = true;

    return PA_OK;
}

pa_result_t taf_pa_file_SetBackupStorage
(
    const char* pathPtr
)
{
    PA_DEBUG("%s", __FUNCTION__);

    if (!pathPtr || !pathPtr[0])
    {
        PA_ERROR("Invalid backup storage path parameter");
        return PA_BAD_PARAMETER;
    }

    struct stat statBuffer;
    if (stat(pathPtr, &statBuffer) != 0)
    {
        if (CreateDirTree(pathPtr) != PA_OK)
        {
            PA_ERROR("Failed to create backup storage directory %s", pathPtr);
            return PA_BAD_PARAMETER;
        }
    }
    else if (!S_ISDIR(statBuffer.st_mode))
    {
        PA_ERROR("Backup storage path %s exists and is not a directory", pathPtr);
        return PA_BUSY;
    }

    size_t pathLen = strlen(pathPtr);
    if (pathLen + 2 > sizeof(BackupStorage))
    {
        PA_ERROR("Backup storage path is too long");
        return PA_OVERFLOW;
    }

    snprintf(BackupStorage, sizeof(BackupStorage), "%s%s", pathPtr,
             (pathPtr[pathLen - 1] == '/') ? "" : "/");

    return PA_OK;
}

pa_result_t taf_pa_file_SetBackupCapacity
(
    uint32_t maxFileSizeBytes,
    uint16_t maxFileCount
)
{
    PA_DEBUG("%s", __FUNCTION__);

    if (!maxFileSizeBytes || !maxFileCount)
    {
        PA_ERROR("Invalid backup capacity parameter (maxFileSizeBytes: %u, maxFileCount: %u)",
                 maxFileSizeBytes, maxFileCount);
        return PA_BAD_PARAMETER;
    }

    MaxFileSize = maxFileSizeBytes;
    MaxFileCount = maxFileCount;

    return PA_OK;
}

pa_result_t taf_pa_file_Verify
(
    const char* filePathPtr
)
{
    PA_DEBUG("%s", __FUNCTION__);

    if (!filePathPtr)
    {
        PA_ERROR("Invalid file path parameter");
        return PA_BAD_PARAMETER;
    }

    pa_result_t result = ValidateFileCRC32CWithExtendedAttr(filePathPtr);
    if (result != PA_OK)
    {
        PA_WARN("File integrity check failed for %s", filePathPtr);
        ReportError(TAF_PA_FILE_ERR_CORRUPTED, filePathPtr);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Pseudo-filesystem prefixes for which the backup/restore mechanism must be bypassed.
 *
 * Paths such as "/proc/<pid>/attr/current" do not represent persistent regular files: they
 * cannot carry extended attributes, must not be checksummed or backed up, and should always
 * go through a plain POSIX open()/close().
 */
//--------------------------------------------------------------------------------------------------
static const char* const BackupExemptPrefixes[] =
{
    "/proc/",
    "/sys/",
    "/dev/",
};

//--------------------------------------------------------------------------------------------------
/**
 * Check whether a path belongs to a pseudo-filesystem that must bypass the backup/restore
 * mechanism (e.g. /proc, /sys, /dev entries).
 */
//--------------------------------------------------------------------------------------------------
static bool IsBackupExemptPath
(
    const char* filePathPtr
)
{
    if (!filePathPtr)
    {
        return false;
    }

    for (size_t i = 0; i < (sizeof(BackupExemptPrefixes) / sizeof(BackupExemptPrefixes[0])); i++)
    {
        size_t prefixLen = strlen(BackupExemptPrefixes[i]);
        if (strncmp(filePathPtr, BackupExemptPrefixes[i], prefixLen) == 0)
        {
            return true;
        }
    }

    return false;
}

int taf_pa_file_Open
(
    const char *filePathPtr,
    int flags,
    mode_t mode
)
{
    PA_DEBUG("%s, filePathPtr: %s", __FUNCTION__, filePathPtr);

    if (!filePathPtr)
    {
        PA_ERROR("Invalid file path parameter");
        errno = EINVAL;
        return -1;
    }

    if (!BackupEnabled || IsBackupExemptPath(filePathPtr))
    {
        return open(filePathPtr, flags, mode);
    }

    struct stat statBuffer;
    bool restore = false;
    // A restore is "best effort" when the caller allows the file to be created: if no usable
    // backup exists, the open must still succeed and create the file.
    bool restoreOptional = false;

    if (stat(filePathPtr, &statBuffer) == 0)
    {
        pa_result_t checkResult = ValidateFileCRC32CWithExtendedAttr(filePathPtr);

        if ((checkResult == PA_NOT_FOUND) && HasValidBackup(filePathPtr))
        {
            // The checksum attribute is missing, but a valid backup exists for this path: the
            // primary's inode was replaced (corruption, editor save, cp, etc.) and the attribute
            // was lost along with the good content. Treat this exactly like a checksum mismatch
            // and restore from the backup instead of overwriting it with the corrupted content.
            PA_ERROR("File %s lost its checksum attribute but a valid backup exists, "
                     "it will be restored from backup", filePathPtr);
            restore = true;
        }
        else if (checkResult == PA_NOT_FOUND)
        {
            // The file exists but was never tracked before (no checksum/backup set yet).
            // This isn't corruption, so create its initial backup instead of attempting -
            // and failing - a restore.
            PA_DEBUG("No checksum tracked yet for %s, creating its initial backup",
                     filePathPtr);
            if (BackupFileToStorage(filePathPtr, NULL) != PA_OK)
            {
                PA_ERROR("Failed to create initial backup for %s", filePathPtr);
                ReportError(TAF_PA_FILE_ERR_BACKUP, filePathPtr);
                return -1;
            }
        }
        else if (checkResult != PA_OK)
        {
            PA_ERROR("File %s failed CRC validation, it will be restored from backup",
                     filePathPtr);
            restore = true;
        }
    }
    else
    {
        if (errno != ENOENT)
        {
            PA_ERROR("Failed to stat %s, errno: %d", filePathPtr, errno);
            return -1;
        }

        // The primary file is gone. It must be restored from its backup even when the caller
        // passes O_CREAT: otherwise open() silently creates an empty file and the following
        // close() would refresh - and therefore destroy - the still valid backup.
        //
        // Two flag combinations legitimately mean "do not restore":
        //  - O_TRUNC: the caller explicitly discards any previous content,
        //  - O_EXCL:  the caller requires an exclusive creation, a restored file would make
        //             open() fail with EEXIST and break that contract.
        if ((flags & O_TRUNC) || ((flags & O_CREAT) && (flags & O_EXCL)))
        {
            PA_DEBUG("File %s is missing but the flags request a fresh file, skipping restore",
                     filePathPtr);
        }
        else
        {
            PA_WARN("File %s is missing, it will be restored from backup", filePathPtr);
            restore = true;
            // With O_CREAT the caller accepts a brand new file, so a failed restore (typically
            // no backup at all for a genuinely new file) must not fail the open.
            restoreOptional = ((flags & O_CREAT) != 0);
        }
    }

    if (restore)
    {
        if ((ReplaceFileWithBackup(filePathPtr) != PA_OK) ||
            (ValidateFileCRC32CWithExtendedAttr(filePathPtr) != PA_OK))
        {
            if (restoreOptional)
            {
                // No usable backup, but the caller allows the file to be created: proceed with
                // a plain open() so a genuinely new file is not rejected.
                PA_DEBUG("No usable backup for %s, creating it as a new file", filePathPtr);
                return open(filePathPtr, flags, mode);
            }

            PA_ERROR("Failed to restore file %s from backup", filePathPtr);
            ReportError(TAF_PA_FILE_ERR_RESTORE, filePathPtr);
            return -1;
        }
        PA_WARN("File %s successfully restored from backup", filePathPtr);

        // The primary was just restored from a validated backup and now carries the backup's
        // checksum: they are identical by construction. Refreshing the backup here would only
        // risk overwriting the last known-good copy, so open the file directly.
        //
        // O_CREAT is harmless now that the file exists again; O_EXCL/O_TRUNC never reach this
        // point (they skip the restore entirely).
        return open(filePathPtr, flags, mode);
    }

    char primaryCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    if (GetFileCRC32CFromExtendedAttr(filePathPtr, primaryCrc, sizeof(primaryCrc)) == PA_OK)
    {
        RefreshBackupIfNeeded(filePathPtr, primaryCrc);
    }

    return open(filePathPtr, flags, mode);
}

int taf_pa_file_Close
(
    int fd
)
{
    PA_DEBUG("%s", __FUNCTION__);

    if (fd < 0)
    {
        PA_ERROR("Invalid file descriptor");
        errno = EBADF;
        return -1;
    }

    if (!BackupEnabled)
    {
        return close(fd);
    }

    char procPath[64] = {0};
    char actualPath[PATH_MAX] = {0};

    snprintf(procPath, sizeof(procPath), "/proc/self/fd/%d", fd);

    ssize_t length = readlink(procPath, actualPath, sizeof(actualPath) - 1);
    if (length < 0)
    {
        PA_ERROR("Failed to resolve path of fd %d, errno: %d, closing without backup update",
                fd, errno);
        return close(fd);
    }
    actualPath[length] = '\0';

    if (IsBackupExemptPath(actualPath))
    {
        return close(fd);
    }

    int flags = fcntl(fd, F_GETFL);
    bool writable = (flags != -1) &&
                    (((flags & O_ACCMODE) == O_WRONLY) || ((flags & O_ACCMODE) == O_RDWR));
    // pread() (used by CalculateFdCRC32C()) needs read access on the descriptor itself: an
    // O_WRONLY fd fails it with EBADF even though the file it points to is perfectly readable.
    bool readable = (flags != -1) &&
                     (((flags & O_ACCMODE) == O_RDONLY) || ((flags & O_ACCMODE) == O_RDWR));

    // Checksum the content through the still-open descriptor before closing it, instead of
    // closing first and reopening actualPath: this avoids an extra open() and the small window
    // between close() and reopen during which actualPath could be replaced by something else.
    // Not possible for an O_WRONLY descriptor (no read access on the fd itself), so that case
    // falls back to a path-based checksum right after close() below. The window this opens
    // between close() and the path-based re-read is not a concurrency hazard here: this PA is
    // only ever driven by the single-threaded KeyStore service (see the note above
    // taf_pa_file_Init() in tafFilePa.h), so no other thread/caller can touch actualPath in
    // between.
    char crcStr[TAF_PA_FILE_CRC_STR_SIZE] = {0};
    pa_result_t crcResult = PA_FAULT;
    if (writable && readable)
    {
        crcResult = CalculateFdCRC32C(fd, crcStr, sizeof(crcStr));
    }

    if (close(fd) != 0)
    {
        PA_ERROR("Failed to close fd %d (%s), errno: %d", fd, actualPath, errno);
        return -1;
    }

    if (!writable)
    {
        // Report the invalid state, but defer restoration
        // until the next open and keep close() successful.
        if (ValidateFileCRC32CWithExtendedAttr(actualPath) != PA_OK)
        {
            PA_WARN("File integrity check failed on close for %s", actualPath);
            ReportError(TAF_PA_FILE_ERR_CORRUPTED, actualPath);
        }
        return 0;
    }

    if (!readable)
    {
        // O_WRONLY: the descriptor is gone now, so recompute the checksum from the path instead.
        crcResult = CalculateFileCRC32C(actualPath, crcStr, sizeof(crcStr));
    }

    if ((crcResult != PA_OK) ||
        (SetFileCRC32CToExtendedAttrValue(actualPath, crcStr) != PA_OK) ||
        (PersistFile(actualPath) != PA_OK))
    {
        PA_ERROR("Failed to update file checksum on close for %s", actualPath);
        ReportError(TAF_PA_FILE_ERR_SET_HASH, actualPath);
        // The fd is already closed; return 0 to avoid misleading callers into a double-close.
        return 0;
    }

    if (RefreshBackupIfNeeded(actualPath, crcStr) != PA_OK)
    {
        PA_ERROR("Failed to refresh file backup on close for %s", actualPath);
        ReportError(TAF_PA_FILE_ERR_BACKUP, actualPath);
        return 0;
    }

    return 0;
}

ssize_t taf_pa_file_Read
(
    int fd,
    uint8_t* bufPtr,
    size_t sizePtr
)
{
    PA_DEBUG("%s", __FUNCTION__);
    return read(fd, bufPtr, sizePtr);
}

ssize_t taf_pa_file_Write
(
    int fd,
    const uint8_t* bufPtr,
    size_t size
)
{
    PA_DEBUG("%s", __FUNCTION__);

    return write(fd, bufPtr, size);
}

void taf_pa_file_Delete
(
    const char* filePathPtr
)
{
    PA_DEBUG("%s", __FUNCTION__);
    PA_DEBUG("filePathPtr: %s", filePathPtr);

    if (!filePathPtr)
    {
        PA_ERROR("Invalid file path parameter");
        return;
    }

    if ((unlink(filePathPtr) != 0) && (errno != ENOENT))
    {
        PA_ERROR("Failed to delete file %s, errno: %d", filePathPtr, errno);
        return;
    }

    // Keep the recovery copy until the primary deletion is durable. If the directory fsync fails,
    // retain the backup so a subsequent restore still has a chance to recover the file.
    if (PersistParentDir(filePathPtr) != PA_OK)
    {
        PA_ERROR("Failed to persist deletion of file %s", filePathPtr);
        ReportError(TAF_PA_FILE_ERR_BACKUP, filePathPtr);
        return;
    }

    if (BackupEnabled)
    {
        if (DeleteBackup(filePathPtr) != PA_OK)
        {
            ReportError(TAF_PA_FILE_ERR_BACKUP, filePathPtr);
        }
    }
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
        PA_ERROR("Failed to open source file %s, errno: %d", sourcePath, errno);
        return errno;
    }

    // open destination file, if it doesn't exist, create it
    destFd = taf_pa_file_Open(destPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destFd < 0)
    {
        PA_ERROR("Failed to open destination file %s, errno: %d", destPath, errno);
        taf_pa_file_Close(srcFd);
        return errno;
    }

    struct stat sourceStat;
    if ((fstat(srcFd, &sourceStat) != 0) ||
        (fchmod(destFd, sourceStat.st_mode & 07777) != 0))
    {
        int savedErrno = errno;
        PA_ERROR("Failed to preserve metadata while copying %s to %s, errno: %d",
                 sourcePath, destPath, savedErrno);
        taf_pa_file_Close(srcFd);
        taf_pa_file_Close(destFd);
        taf_pa_file_Delete(destPath);
        return savedErrno ? savedErrno : EIO;
    }

    // read source file and write to destination file
    while ((bytesRead = read(srcFd, buffer, RFS_COPY_BUFFER_SIZE)) > 0)
    {
        bytesWritten = taf_pa_file_Write(destFd, buffer, bytesRead);
        if (bytesWritten != bytesRead)
        {
            int savedErrno = errno;
            PA_ERROR("Failed to write to destination file %s, errno: %d", destPath, savedErrno);
            taf_pa_file_Close(srcFd);
            taf_pa_file_Close(destFd);
            taf_pa_file_Delete(destPath);
            return savedErrno;
        }
    }

    // error check for taf_pa_file_Read returned value
    if (bytesRead < 0)
    {
        int savedErrno = errno;
        PA_ERROR("Failed to read from source file %s, errno: %d", sourcePath, savedErrno);
        taf_pa_file_Close(srcFd);
        taf_pa_file_Close(destFd);
        taf_pa_file_Delete(destPath);
        return savedErrno;
    }

    if (CopySELinuxContext(srcFd, destFd, sourcePath, destPath) != PA_OK)
    {
        taf_pa_file_Close(srcFd);
        taf_pa_file_Close(destFd);
        taf_pa_file_Delete(destPath);
        return EACCES;
    }

    if (taf_pa_file_Close(srcFd) != 0)
    {
        int savedErrno = errno;
        taf_pa_file_Close(destFd);
        taf_pa_file_Delete(destPath);
        return savedErrno ? savedErrno : EIO;
    }

    // Closing the destination updates its checksum and refreshes its backup copy.
    if (taf_pa_file_Close(destFd) != 0)
    {
        int savedErrno = errno;
        PA_ERROR("Failed to close destination file %s, errno: %d", destPath, savedErrno);
        return savedErrno ? savedErrno : EIO;
    }

    if (BackupEnabled && !HasMatchingBackup(destPath))
    {
        PA_ERROR("Destination %s was copied but has no valid backup", destPath);
        errno = EIO;
        return EIO;
    }

    return 0;
}

int taf_pa_file_Rename
(
    const char *sourcePath,
    const char *destPath
)
{
    PA_DEBUG("%s", __FUNCTION__);

    if (!sourcePath || !destPath)
    {
        PA_ERROR("Invalid source/destination path parameter");
        errno = EINVAL;
        return -1;
    }

    if (rename(sourcePath, destPath) == 0)
    {
        if (BackupEnabled)
        {
            pa_result_t backupResult = PA_OK;

            if (PersistParentDir(destPath) != PA_OK)
            {
                backupResult = PA_FAULT;
            }

            // The source primary is gone after rename(), so its backup must be removed. Report a
            // failure if the cleanup cannot be made durable; otherwise an old source-path backup
            // remains available for a future file created at that path.
            if (DeleteBackup(sourcePath) != PA_OK)
            {
                backupResult = PA_FAULT;
            }

            char destCrc[TAF_PA_FILE_CRC_STR_SIZE] = {0};
            if ((CalculateFileCRC32C(destPath, destCrc, sizeof(destCrc)) != PA_OK) ||
                (SetFileCRC32CToExtendedAttrValue(destPath, destCrc) != PA_OK) ||
                (BackupFileToStorage(destPath, destCrc) != PA_OK))
            {
                // destPath's backup slot (GetBackupPath(destPath)) may still be holding whatever
                // was backed up under that path before the rename, i.e. content that no longer
                // matches destPath now that sourcePath's content lives there. Leaving it in place
                // would let a later restore reinstate that stale, unrelated content onto destPath,
                // so drop it instead - taf_pa_file_Open()'s "no checksum tracked yet, create the
                // initial backup" path will recreate a correct one from the next open.
                PA_ERROR("Failed to refresh backup of %s after rename, discarding its stale "
                         "backup", destPath);
                if (DeleteBackup(destPath) != PA_OK)
                {
                    backupResult = PA_FAULT;
                }
                backupResult = PA_FAULT;
            }

            if (backupResult != PA_OK)
            {
                errno = EIO;
                return -1;
            }
        }
        return 0;
    }

    if (errno == EXDEV)
    {
        PA_DEBUG("Rename of %s to %s crosses file systems, falling back to copy + delete",
                 sourcePath, destPath);
        int result = taf_pa_file_Copy(sourcePath, destPath);
        if (result != 0)
        {
            PA_ERROR("Failed to copy %s to %s as a fallback for rename, errno: %d",
                     sourcePath, destPath, result);
            errno = result;
            return -1;
        }

        // Only delete the source (and its backup) after verifying the destination
        // has a valid backup, so we never lose both copies simultaneously.
        if (BackupEnabled)
        {
            if (!HasMatchingBackup(destPath))
            {
                PA_ERROR("Destination %s has no valid backup after cross-fs copy, "
                         "keeping source %s", destPath, sourcePath);
                errno = EIO;
                return -1;
            }
        }

        if ((unlink(sourcePath) != 0) && (errno != ENOENT))
        {
            PA_ERROR("Failed to delete source %s after cross-filesystem copy, errno: %d",
                     sourcePath, errno);
            return -1;
        }

        if (PersistParentDir(sourcePath) != PA_OK)
        {
            PA_ERROR("Failed to persist deletion of source %s", sourcePath);
            errno = EIO;
            return -1;
        }

        if (BackupEnabled && (DeleteBackup(sourcePath) != PA_OK))
        {
            errno = EIO;
            return -1;
        }

        return 0;
    }

    PA_ERROR("Failed to rename %s to %s, errno: %d", sourcePath, destPath, errno);

    return -1;
}
