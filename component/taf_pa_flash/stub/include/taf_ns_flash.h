/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_NS_FLASH_H
#define TAF_NS_FLASH_H

#include "taf_ns_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_NS_FLASH_BITMASK_OPEN_MODE_READ_ONLY 0x1
#define TAF_NS_FLASH_BITMASK_OPEN_MODE_WRITE_ONLY 0x2
#define TAF_NS_FLASH_BITMASK_OPEN_MODE_READ_WRITE 0x4
typedef uint64_t taf_ns_flash_OpenModeBitMask_t;

typedef struct
{
    uint64_t size;
    uint32_t eraseSize;
    uint32_t writeSize;
} taf_ns_flash_MtdInfo_t;

typedef struct
{
    uint64_t size;
    uint32_t lebSize;
    uint32_t reservedLebs;
    uint32_t availLebs;
} taf_ns_flash_UbiVolumeInfo_t;

typedef struct taf_ns_flash_MtdRef* taf_ns_flash_MtdRef_t;

typedef struct taf_ns_flash_UbiVolumeRef* taf_ns_flash_UbiVolumeRef_t;

NS_SHARED int32_t taf_ns_flash_Init
(
    void
);

NS_SHARED int32_t taf_ns_flash_OpenMtd
(
    const char* namePtr,
    taf_ns_flash_OpenModeBitMask_t mode,
    taf_ns_flash_MtdRef_t* mtdRefPtr
);

NS_SHARED int32_t taf_ns_flash_CloseMtd
(
    taf_ns_flash_MtdRef_t mtdRef
);

NS_SHARED int32_t taf_ns_flash_GetMtdInfo
(
    taf_ns_flash_MtdRef_t mtdRef,
    taf_ns_flash_MtdInfo_t* infoPtr
);

NS_SHARED int32_t taf_ns_flash_EraseMtdBlock
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
);

NS_SHARED int32_t taf_ns_flash_CheckMtdGoodBlock
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t blockIndex,
	bool* isGoodBlockPtr
);

NS_SHARED int32_t taf_ns_flash_MarkMtdBadBlock
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
);

NS_SHARED int32_t taf_ns_flash_ReadMtdPage
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    unsigned char* dataPtr,
    size_t* dataSizePtr
);

NS_SHARED int32_t taf_ns_flash_WriteMtdPage
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    const unsigned char* dataPtr,
    size_t dataSize
);

NS_SHARED int32_t taf_ns_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
);

NS_SHARED int32_t taf_ns_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_ns_flash_OpenModeBitMask_t mode,
    taf_ns_flash_UbiVolumeRef_t* ubiVolumeRefPtr
);

NS_SHARED int32_t taf_ns_flash_CloseUbiVolume
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef
);

NS_SHARED int32_t taf_ns_flash_SetUbiVolumeUpdateSize
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
);

NS_SHARED int32_t taf_ns_flash_ReadUbiVolume
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    off_t offset,
    unsigned char* dataPtr,
    size_t* dataSizePtr
);

NS_SHARED int32_t taf_ns_flash_UpdateUbiVolume
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    const unsigned char* dataPtr,
    size_t dataSize
);

NS_SHARED int32_t taf_ns_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
);

NS_SHARED int32_t taf_ns_flash_EraseUbiVolume
(
    const char* namePtr
);

NS_SHARED int32_t taf_ns_flash_GetUbiVolumeInfo
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_ns_flash_UbiVolumeInfo_t* infoPtr
);


#ifdef __cplusplus
}
#endif

#endif /* TAF_NS_MRC_H */