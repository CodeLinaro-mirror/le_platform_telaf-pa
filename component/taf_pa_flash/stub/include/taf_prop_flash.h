/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PROP_FLASH_H
#define TAF_PROP_FLASH_H

#include <stddef.h>      // size_t
#include <sys/types.h>   // off_t
#include <cstdint>
#include "taf_prop_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PROP_FLASH_BITMASK_OPEN_MODE_READ_ONLY 0x1
#define TAF_PROP_FLASH_BITMASK_OPEN_MODE_WRITE_ONLY 0x2
#define TAF_PROP_FLASH_BITMASK_OPEN_MODE_READ_WRITE 0x4
typedef uint64_t taf_prop_flash_OpenModeBitMask_t;

typedef struct
{
    uint64_t size;
    uint32_t eraseSize;
    uint32_t writeSize;
} taf_prop_flash_MtdInfo_t;

typedef struct
{
    uint64_t size;
    uint32_t lebSize;
    uint32_t reservedLebs;
    uint32_t availLebs;
} taf_prop_flash_UbiVolumeInfo_t;

typedef struct taf_prop_flash_MtdRef* taf_prop_flash_MtdRef_t;

typedef struct taf_prop_flash_UbiVolumeRef* taf_prop_flash_UbiVolumeRef_t;

PROP_SHARED int32_t taf_prop_flash_Init
(
    void
);

PROP_SHARED int32_t taf_prop_flash_OpenMtd
(
    const char* namePtr,
    taf_prop_flash_OpenModeBitMask_t mode,
    taf_prop_flash_MtdRef_t* mtdRefPtr
);

PROP_SHARED int32_t taf_prop_flash_CloseMtd
(
    taf_prop_flash_MtdRef_t mtdRef
);

PROP_SHARED int32_t taf_prop_flash_GetMtdInfo
(
    taf_prop_flash_MtdRef_t mtdRef,
    taf_prop_flash_MtdInfo_t* infoPtr
);

PROP_SHARED int32_t taf_prop_flash_EraseMtdBlock
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
);

PROP_SHARED int32_t taf_prop_flash_CheckMtdGoodBlock
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t blockIndex,
	bool* isGoodBlockPtr
);

PROP_SHARED int32_t taf_prop_flash_MarkMtdBadBlock
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
);

PROP_SHARED int32_t taf_prop_flash_ReadMtdPage
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    unsigned char* dataPtr,
    size_t* dataSizePtr
);

PROP_SHARED int32_t taf_prop_flash_WriteMtdPage
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    const unsigned char* dataPtr,
    size_t dataSize
);

PROP_SHARED int32_t taf_prop_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
);

PROP_SHARED int32_t taf_prop_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_prop_flash_OpenModeBitMask_t mode,
    taf_prop_flash_UbiVolumeRef_t* ubiVolumeRefPtr
);

PROP_SHARED int32_t taf_prop_flash_CloseUbiVolume
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef
);

PROP_SHARED int32_t taf_prop_flash_SetUbiVolumeUpdateSize
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
);

PROP_SHARED int32_t taf_prop_flash_ReadUbiVolume
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    off_t offset,
    unsigned char* dataPtr,
    size_t* dataSizePtr
);

PROP_SHARED int32_t taf_prop_flash_UpdateUbiVolume
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    const unsigned char* dataPtr,
    size_t dataSize
);

PROP_SHARED int32_t taf_prop_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
);

PROP_SHARED int32_t taf_prop_flash_EraseUbiVolume
(
    const char* namePtr
);

PROP_SHARED int32_t taf_prop_flash_GetUbiVolumeInfo
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_prop_flash_UbiVolumeInfo_t* infoPtr
);


#ifdef __cplusplus
}
#endif

#endif /* TAF_PROP_MRC_H */