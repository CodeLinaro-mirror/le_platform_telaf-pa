/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_FLASH_PA_HPP
#define TAF_FLASH_PA_HPP

#include "tafCommonPa.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAF_PA_FLASH_BITMASK_OPEN_MODE_READ_ONLY 0x1
#define TAF_PA_FLASH_BITMASK_OPEN_MODE_WRITE_ONLY 0x2
#define TAF_PA_FLASH_BITMASK_OPEN_MODE_READ_WRITE 0x4
typedef uint64_t taf_pa_flash_OpenModeBitMask_t;

typedef struct
{
    uint64_t size;
    uint32_t eraseSize;
    uint32_t writeSize;
} taf_pa_flash_MtdInfo_t;

typedef struct
{
    uint64_t size;
    uint32_t lebSize;
    uint32_t reservedLebs;
    uint32_t availLebs;
} taf_pa_flash_UbiVolumeInfo_t;

typedef struct taf_pa_flash_MtdRef* taf_pa_flash_MtdRef_t;

typedef struct taf_pa_flash_UbiVolumeRef* taf_pa_flash_UbiVolumeRef_t;

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_Init
(
    void
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_Deinit
(
    void
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_OpenMtd
(
    const char* namePtr,
    taf_pa_flash_OpenModeBitMask_t mode,
    taf_pa_flash_MtdRef_t* mtdRefPtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_CloseMtd
(
    taf_pa_flash_MtdRef_t mtdRef
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_GetMtdInfo
(
    taf_pa_flash_MtdRef_t mtdRef,
    taf_pa_flash_MtdInfo_t* infoPtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_EraseMtdBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_CheckMtdGoodBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex,
	bool* isGoodBlockPtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_MarkMtdBadBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_ReadMtdPage
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    unsigned char* dataPtr,
    size_t* dataSizePtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_WriteMtdPage
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    const unsigned char* dataPtr,
    size_t dataSize
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_pa_flash_OpenModeBitMask_t mode,
    taf_pa_flash_UbiVolumeRef_t* ubiVolumeRefPtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_CloseUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_SetUbiVolumeUpdateSize
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_ReadUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    off_t offset,
    unsigned char* dataPtr,
    size_t* dataSizePtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_UpdateUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    const unsigned char* dataPtr,
    size_t dataSize
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_EraseUbiVolume
(
    const char* namePtr
);

TAF_PA_SHARED taf_pa_result_t taf_pa_flash_GetUbiVolumeInfo
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_pa_flash_UbiVolumeInfo_t* infoPtr
);

#ifdef __cplusplus
}
#endif

#endif /* TAF_FLASH_PA_HPP */
