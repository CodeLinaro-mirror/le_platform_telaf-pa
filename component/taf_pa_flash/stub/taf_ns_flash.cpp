/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include "taf_ns_flash.h"

int32_t taf_ns_flash_Init
(
    void
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_OpenMtd
(
    const char* namePtr,
    taf_ns_flash_OpenModeBitMask_t mode,
    taf_ns_flash_MtdRef_t* mtdRefPtr
)
{
    NS_INFO("Function is not implemented in stub PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_CloseMtd
(
    taf_ns_flash_MtdRef_t mtdRef
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_GetMtdInfo
(
    taf_ns_flash_MtdRef_t mtdRef,
    taf_ns_flash_MtdInfo_t* infoPtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_EraseMtdBlock
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_CheckMtdGoodBlock
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t blockIndex,
	bool* isGoodBlockPtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_MarkMtdBadBlock
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_ReadMtdPage
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    unsigned char* dataPtr,
    size_t* dataSizePtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_WriteMtdPage
(
    taf_ns_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    const unsigned char* dataPtr,
    size_t dataSize
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_ns_flash_OpenModeBitMask_t mode,
    taf_ns_flash_UbiVolumeRef_t* ubiVolumeRefPtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_CloseUbiVolume
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_SetUbiVolumeUpdateSize
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_ReadUbiVolume
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    off_t offset,
    unsigned char* dataPtr,
    size_t* dataSizePtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_UpdateUbiVolume
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    const unsigned char* dataPtr,
    size_t dataSize
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_EraseUbiVolume
(
    const char* namePtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}

int32_t taf_ns_flash_GetUbiVolumeInfo
(
    taf_ns_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_ns_flash_UbiVolumeInfo_t* infoPtr
)
{
    NS_INFO("Function is not implemented in default PA.");

    return -ENOSYS;
}