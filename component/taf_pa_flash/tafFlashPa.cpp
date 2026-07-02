/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <errno.h>

#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "tafFlashPa.hpp"

#include "taf_prop_flash.h"
#include "tafInternalCommonPa.h"

using namespace std;

class PlatformAdaptor
{
    public:
        unordered_map<taf_pa_flash_MtdRef_t, taf_prop_flash_MtdRef_t> mtdRefMap;
        unordered_map<taf_pa_flash_UbiVolumeRef_t, taf_prop_flash_UbiVolumeRef_t> ubiVolumeRefMap;
        std::atomic<bool> isInitialized_{false};

        static PlatformAdaptor& GetInstance
        (
            void
        );
};

PlatformAdaptor& PlatformAdaptor::GetInstance
(
    void
)
{
    static PlatformAdaptor instance;
    return instance;
}

static taf_prop_flash_OpenModeBitMask_t ConvertOpenMode
(
    taf_pa_flash_OpenModeBitMask_t bitmask
)
{
    taf_prop_flash_OpenModeBitMask_t result = 0x0;

    if (bitmask & TAF_PA_FLASH_BITMASK_OPEN_MODE_READ_ONLY)
        result |= TAF_PROP_FLASH_BITMASK_OPEN_MODE_READ_ONLY;

    if (bitmask & TAF_PA_FLASH_BITMASK_OPEN_MODE_WRITE_ONLY)
        result |= TAF_PROP_FLASH_BITMASK_OPEN_MODE_WRITE_ONLY;

    if (bitmask & TAF_PA_FLASH_BITMASK_OPEN_MODE_READ_WRITE)
        result |= TAF_PROP_FLASH_BITMASK_OPEN_MODE_READ_WRITE;

    return result;
}

taf_pa_result_t taf_pa_flash_Init()
{
    TAF_PA_INFO("Flash platform adaptor initialization is done.");

    taf_prop_result_t result = taf_prop_flash_Init();
    if (result == TAF_PROP_NOT_IMPLEMENTED)
        TAF_PA_INFO("Flash proprietary platform adaptor is not implemented.");

    if (result == TAF_PROP_OK)
    {
        auto& pa = PlatformAdaptor::GetInstance();
        pa.isInitialized_.store(true, std::memory_order_release);
    }

    return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_Deinit()
{
    TAF_PA_INFO("Flash platform adaptor deinitialization starting.");

    auto& pa = PlatformAdaptor::GetInstance();

    // Check if initialization was successful before proceeding with deinitialization
    if (!pa.isInitialized_.load(std::memory_order_acquire))
    {
        TAF_PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return TAF_PA_FAULT;
    }

    // Close all open MTD references and free their malloc'd keys.
    // Collect keys and close handles first, then clear the map, then free the
    // keys. Freeing a key inside the iteration loop is undefined behaviour
    // because the unordered_map still holds the freed pointer in its internal
    // structure until clear() is called.
    {
        std::vector<taf_pa_flash_MtdRef_t> keysToFree;
        keysToFree.reserve(pa.mtdRefMap.size());
        for (auto& entry : pa.mtdRefMap)
        {
            taf_prop_result_t result = taf_prop_flash_CloseMtd(entry.second);
            if (result != TAF_PROP_OK)
            {
                TAF_PA_ERROR("Failed to close MTD during deinit. Error: %d", (int)result);
            }
            keysToFree.push_back(entry.first);
        }
        pa.mtdRefMap.clear();
        for (taf_pa_flash_MtdRef_t key : keysToFree)
        {
            free(key);
        }
    }

    // Close all open UBI volume references and free their malloc'd keys.
    // Same pattern: collect, clear map, then free.
    {
        std::vector<taf_pa_flash_UbiVolumeRef_t> keysToFree;
        keysToFree.reserve(pa.ubiVolumeRefMap.size());
        for (auto& entry : pa.ubiVolumeRefMap)
        {
            taf_prop_result_t result = taf_prop_flash_CloseUbiVolume(entry.second);
            if (result != TAF_PROP_OK)
            {
                TAF_PA_ERROR("Failed to close UBI volume during deinit. Error: %d", (int)result);
            }
            keysToFree.push_back(entry.first);
        }
        pa.ubiVolumeRefMap.clear();
        for (taf_pa_flash_UbiVolumeRef_t key : keysToFree)
        {
            free(key);
        }
    }

    TAF_PA_INFO("Flash platform adaptor deinitialization complete.");
    pa.isInitialized_.store(false, std::memory_order_release);
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_flash_OpenMtd
(
    const char* namePtr,
    taf_pa_flash_OpenModeBitMask_t mode,
    taf_pa_flash_MtdRef_t* mtdRefPtr
)
{
    taf_prop_flash_MtdRef_t nsMtdRef = nullptr;
    taf_prop_flash_OpenModeBitMask_t nsMode = ConvertOpenMode(mode);
    taf_prop_result_t result = taf_prop_flash_OpenMtd(namePtr, nsMode, &nsMtdRef);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to open MTD %s.", namePtr);
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    taf_pa_flash_MtdRef_t paMtdRef = (taf_pa_flash_MtdRef_t)malloc(sizeof(taf_pa_flash_MtdRef_t));
    if (paMtdRef == nullptr)
    {
        TAF_PA_ERROR("Failed to allocate memory for MTD reference.");
        result = taf_prop_flash_CloseMtd(nsMtdRef);
        if (result != TAF_PROP_OK)
        {
            TAF_PA_ERROR("Failed to close MTD %s.", namePtr);
            return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
        }
        return TAF_PA_NO_MEMORY;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.insert(make_pair(paMtdRef, nsMtdRef));
    if (!it.second)
    {
        TAF_PA_ERROR("MTD reference already exists, insert failed.");
        result = taf_prop_flash_CloseMtd(nsMtdRef);
        if (result != TAF_PROP_OK)
        {
            TAF_PA_ERROR("Failed to close MTD %s.", namePtr);
            return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
        }
        return TAF_PA_DUPLICATE;
    }

    *mtdRefPtr = paMtdRef;

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_flash_CloseMtd
(
    taf_pa_flash_MtdRef_t mtdRef
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t result = taf_prop_flash_CloseMtd(it->second);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to close MTD.");
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    pa.mtdRefMap.erase(it);
    free(mtdRef);
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_flash_GetMtdInfo
(
    taf_pa_flash_MtdRef_t mtdRef,
    taf_pa_flash_MtdInfo_t* infoPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_flash_MtdInfo_t info;
    taf_prop_result_t result = taf_prop_flash_GetMtdInfo(it->second, &info);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to get MTD information.");
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    infoPtr->size = info.size;
    infoPtr->eraseSize = info.eraseSize;
    infoPtr->writeSize = info.writeSize;

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_flash_EraseMtdBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_EraseMtdBlock(it->second, blockIndex);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_CheckMtdGoodBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex,
	bool* isGoodBlockPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_CheckMtdGoodBlock(
        it->second, blockIndex, isGoodBlockPtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_MarkMtdBadBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_MarkMtdBadBlock(it->second, blockIndex);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_ReadMtdPage
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    unsigned char* dataPtr,
    size_t* dataSizePtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_ReadMtdPage(it->second, pageIndex, dataPtr, dataSizePtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_WriteMtdPage
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t pageIndex,
    const unsigned char* dataPtr,
    size_t dataSize
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        TAF_PA_ERROR("MTD reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_WriteMtdPage(it->second, pageIndex, dataPtr, dataSize);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
)
{
    taf_prop_result_t rc = taf_prop_flash_CopyMtd(srcNamePtr, dstNamePtr, dataSize);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_pa_flash_OpenModeBitMask_t mode,
    taf_pa_flash_UbiVolumeRef_t* ubiVolumeRefPtr
)
{
    taf_prop_flash_UbiVolumeRef_t nsUbiVolumeRef = nullptr;
    taf_prop_flash_OpenModeBitMask_t nsMode = ConvertOpenMode(mode);
    taf_prop_result_t result = taf_prop_flash_OpenUbiVolume(namePtr, nsMode, &nsUbiVolumeRef);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to open UBI volume %s.", namePtr);
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    taf_pa_flash_UbiVolumeRef_t paUbiVolumeRef = (taf_pa_flash_UbiVolumeRef_t)malloc(sizeof(
        taf_pa_flash_UbiVolumeRef_t));
    if (paUbiVolumeRef == nullptr)
    {
        TAF_PA_ERROR("Failed to allocate memory for UBI volume reference.");
        result = taf_prop_flash_CloseUbiVolume(nsUbiVolumeRef);
        if (result != TAF_PROP_OK)
        {
            TAF_PA_ERROR("Failed to close UBI volume %s.", namePtr);
            return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
        }
        return TAF_PA_NO_MEMORY;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.insert(make_pair(paUbiVolumeRef, nsUbiVolumeRef));
    if (!it.second)
    {
        TAF_PA_ERROR("UBI volume reference already exists, insert failed.");
        result = taf_prop_flash_CloseUbiVolume(nsUbiVolumeRef);
        if (result != TAF_PROP_OK)
        {
            TAF_PA_ERROR("Failed to close UBI volume %s.", namePtr);
            return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
        }
        return TAF_PA_DUPLICATE;
    }

    *ubiVolumeRefPtr = paUbiVolumeRef;

    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_flash_CloseUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        TAF_PA_ERROR("UBI volume reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t result = taf_prop_flash_CloseUbiVolume(it->second);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to close UBI volume.");
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    pa.ubiVolumeRefMap.erase(it);
    free(ubiVolumeRef);
    return TAF_PA_OK;
}

taf_pa_result_t taf_pa_flash_SetUbiVolumeUpdateSize
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        TAF_PA_ERROR("UBI volume reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_SetUbiVolumeUpdateSize(it->second, size);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_ReadUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    off_t offset,
    unsigned char* dataPtr,
    size_t* dataSizePtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        TAF_PA_ERROR("UBI volume reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_ReadUbiVolume(it->second, offset, dataPtr, dataSizePtr);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_UpdateUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    const unsigned char* dataPtr,
    size_t dataSize
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        TAF_PA_ERROR("UBI volume reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_result_t rc = taf_prop_flash_UpdateUbiVolume(it->second, dataPtr, dataSize);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
)
{
    taf_prop_result_t rc = taf_prop_flash_CopyUbiVolume(srcNamePtr, dstNamePtr, bufferSize,
                                                         dataSize);
    return PropResultToPaResult(rc, TAF_PROP_UNDERLYING_ERR_NONE);
}

taf_pa_result_t taf_pa_flash_EraseUbiVolume
(
    const char* namePtr
)
{
    uint32_t underlyingErr = TAF_PROP_UNDERLYING_ERR_NONE;
    taf_prop_result_t rc = taf_prop_flash_EraseUbiVolume(namePtr, &underlyingErr);
    TAF_PROP_LOG_UNDERLYING_ERROR(rc, underlyingErr);
    return PropResultToPaResult(rc, underlyingErr);
}

taf_pa_result_t taf_pa_flash_GetUbiVolumeInfo
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_pa_flash_UbiVolumeInfo_t* infoPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        TAF_PA_ERROR("UBI volume reference not found.");
        return TAF_PA_NOT_FOUND;
    }

    taf_prop_flash_UbiVolumeInfo_t info;
    taf_prop_result_t result = taf_prop_flash_GetUbiVolumeInfo(it->second, &info);
    if (result != TAF_PROP_OK)
    {
        TAF_PA_ERROR("Failed to get UBI volume information.");
        return PropResultToPaResult(result, TAF_PROP_UNDERLYING_ERR_NONE);
    }

    infoPtr->size = info.size;
    infoPtr->lebSize = info.lebSize;
    infoPtr->reservedLebs = info.reservedLebs;
    infoPtr->availLebs = info.availLebs;

    return TAF_PA_OK;
}