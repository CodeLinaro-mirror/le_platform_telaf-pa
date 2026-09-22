/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include "taf_prop_flash.h"

taf_prop_result_t taf_prop_flash_Init ( void )
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_OpenMtd
(
    const char* namePtr,
    taf_prop_flash_OpenModeBitMask_t mode,
    taf_prop_flash_MtdRef_t* mtdRefPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_CloseMtd
(
    taf_prop_flash_MtdRef_t mtdRef
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_GetMtdInfo
(
    taf_prop_flash_MtdRef_t mtdRef,
    taf_prop_flash_MtdInfo_t* infoPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_EraseMtdBlock
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_CheckMtdGoodBlock
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t blockIndex,
	bool* isGoodBlockPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_MarkMtdBadBlock
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_ReadMtdPage
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    unsigned char* dataPtr,
    size_t* dataSizePtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_WriteMtdPage
(
    taf_prop_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    const unsigned char* dataPtr,
    size_t dataSize
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_prop_flash_OpenModeBitMask_t mode,
    taf_prop_flash_UbiVolumeRef_t* ubiVolumeRefPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_CloseUbiVolume
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_SetUbiVolumeUpdateSize
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_ReadUbiVolume
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    off_t offset,
    unsigned char* dataPtr,
    size_t* dataSizePtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_UpdateUbiVolume
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    const unsigned char* dataPtr,
    size_t dataSize
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_EraseUbiVolume
(
    const char* namePtr,
    uint32_t* underlyingErrPtr
)
{
    if (underlyingErrPtr != NULL)
    {
        *underlyingErrPtr = TAF_PROP_UNDERLYING_ERR_NONE;
    }

    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}

taf_prop_result_t taf_prop_flash_GetUbiVolumeInfo
(
    taf_prop_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_prop_flash_UbiVolumeInfo_t* infoPtr
)
{
    TAF_PROP_INFO("Function is not implemented in stub PA.");

    return TAF_PROP_NOT_IMPLEMENTED;
}
