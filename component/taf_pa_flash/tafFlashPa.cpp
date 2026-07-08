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

pa_result_t taf_pa_flash_Init()
{
    PA_INFO("Flash platform adaptor initialization is done.");

    int32_t result = taf_prop_flash_Init();
    if (result == -ENOSYS)
        PA_INFO("Flash proprietary platform adaptor is not implemented.");

    if (result == 0)
    {
        auto& pa = PlatformAdaptor::GetInstance();
        pa.isInitialized_.store(true, std::memory_order_release);
    }

    return result;
}

pa_result_t taf_pa_flash_Deinit()
{
    PA_INFO("Flash platform adaptor deinitialization starting.");

    auto& pa = PlatformAdaptor::GetInstance();

    // Check if initialization was successful before proceeding with deinitialization
    if (!pa.isInitialized_.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before successful Init(). Ignoring deinit request.");
        return PA_FAULT;
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
            int32_t result = taf_prop_flash_CloseMtd(entry.second);
            if (result != 0)
            {
                PA_ERROR("Failed to close MTD during deinit. Error: %d", result);
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
            int32_t result = taf_prop_flash_CloseUbiVolume(entry.second);
            if (result != 0)
            {
                PA_ERROR("Failed to close UBI volume during deinit. Error: %d", result);
            }
            keysToFree.push_back(entry.first);
        }
        pa.ubiVolumeRefMap.clear();
        for (taf_pa_flash_UbiVolumeRef_t key : keysToFree)
        {
            free(key);
        }
    }

    PA_INFO("Flash platform adaptor deinitialization complete.");
    pa.isInitialized_.store(false, std::memory_order_release);
    return 0;
}

pa_result_t taf_pa_flash_OpenMtd
(
    const char* namePtr,
    taf_pa_flash_OpenModeBitMask_t mode,
    taf_pa_flash_MtdRef_t* mtdRefPtr
)
{
    taf_prop_flash_MtdRef_t nsMtdRef = nullptr;
    taf_prop_flash_OpenModeBitMask_t nsMode = ConvertOpenMode(mode);
    int32_t result = taf_prop_flash_OpenMtd(namePtr, nsMode, &nsMtdRef);
    if (result != 0)
    {
        PA_ERROR("Failed to open MTD %s.", namePtr);
        return result;
    }

    taf_pa_flash_MtdRef_t paMtdRef = (taf_pa_flash_MtdRef_t)malloc(sizeof(taf_pa_flash_MtdRef_t));
    if (paMtdRef == nullptr)
    {
        PA_ERROR("Failed to allocate memory for MTD reference.");
        result = taf_prop_flash_CloseMtd(nsMtdRef);
        if (result != 0)
        {
            PA_ERROR("Failed to close MTD %s.", namePtr);
            return result;
        }
        return -ENOMEM;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.insert(make_pair(paMtdRef, nsMtdRef));
    if (!it.second)
    {
        PA_ERROR("MTD reference already exists, insert failed.");
        result = taf_prop_flash_CloseMtd(nsMtdRef);
        if (result != 0)
        {
            PA_ERROR("Failed to close MTD %s.", namePtr);
            return result;
        }
        return -EEXIST;
    }

    *mtdRefPtr = paMtdRef;

    return 0;
}

pa_result_t taf_pa_flash_CloseMtd
(
    taf_pa_flash_MtdRef_t mtdRef
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    int32_t result = taf_prop_flash_CloseMtd(it->second);
    if (result != 0)
    {
        PA_ERROR("Failed to close MTD.");
        return result;
    }

    pa.mtdRefMap.erase(it);
    free(mtdRef);
    return 0;
}

pa_result_t taf_pa_flash_GetMtdInfo
(
    taf_pa_flash_MtdRef_t mtdRef,
    taf_pa_flash_MtdInfo_t* infoPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    taf_prop_flash_MtdInfo_t info;
    int32_t result = taf_prop_flash_GetMtdInfo(it->second, &info);
    if (result != 0)
    {
        PA_ERROR("Failed to get MTD information.");
        return result;
    }

    infoPtr->size = info.size;
    infoPtr->eraseSize = info.eraseSize;
    infoPtr->writeSize = info.writeSize;

    return 0;
}

pa_result_t taf_pa_flash_EraseMtdBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_EraseMtdBlock(it->second, blockIndex);
}

pa_result_t taf_pa_flash_CheckMtdGoodBlock
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
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_CheckMtdGoodBlock(it->second, blockIndex, isGoodBlockPtr);
}

pa_result_t taf_pa_flash_MarkMtdBadBlock
(
    taf_pa_flash_MtdRef_t mtdRef,
    uint32_t blockIndex
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.mtdRefMap.find(mtdRef);
    if (it == pa.mtdRefMap.end())
    {
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_MarkMtdBadBlock(it->second, blockIndex);
}

pa_result_t taf_pa_flash_ReadMtdPage
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
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_ReadMtdPage(it->second, pageIndex, dataPtr, dataSizePtr);
}

pa_result_t taf_pa_flash_WriteMtdPage
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
        PA_ERROR("MTD reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_WriteMtdPage(it->second, pageIndex, dataPtr, dataSize);
}

pa_result_t taf_pa_flash_CopyMtd
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t dataSize
)
{
    return taf_prop_flash_CopyMtd(srcNamePtr, dstNamePtr, dataSize);
}

pa_result_t taf_pa_flash_OpenUbiVolume
(
    const char* namePtr,
    taf_pa_flash_OpenModeBitMask_t mode,
    taf_pa_flash_UbiVolumeRef_t* ubiVolumeRefPtr
)
{
    taf_prop_flash_UbiVolumeRef_t nsUbiVolumeRef = nullptr;
    taf_prop_flash_OpenModeBitMask_t nsMode = ConvertOpenMode(mode);
    int32_t result = taf_prop_flash_OpenUbiVolume(namePtr, nsMode, &nsUbiVolumeRef);
    if (result != 0)
    {
        PA_ERROR("Failed to open UBI volume %s.", namePtr);
        return result;
    }

    taf_pa_flash_UbiVolumeRef_t paUbiVolumeRef = (taf_pa_flash_UbiVolumeRef_t)malloc(sizeof(
        taf_pa_flash_UbiVolumeRef_t));
    if (paUbiVolumeRef == nullptr)
    {
        PA_ERROR("Failed to allocate memory for UBI volume reference.");
        result = taf_prop_flash_CloseUbiVolume(nsUbiVolumeRef);
        if (result != 0)
        {
            PA_ERROR("Failed to close UBI volume %s.", namePtr);
            return result;
        }
        return -ENOMEM;
    }

    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.insert(make_pair(paUbiVolumeRef, nsUbiVolumeRef));
    if (!it.second)
    {
        PA_ERROR("UBI volume reference already exists, insert failed.");
        result = taf_prop_flash_CloseUbiVolume(nsUbiVolumeRef);
        if (result != 0)
        {
            PA_ERROR("Failed to close UBI volume %s.", namePtr);
            return result;
        }
        return -EEXIST;
    }

    *ubiVolumeRefPtr = paUbiVolumeRef;

    return 0;
}

pa_result_t taf_pa_flash_CloseUbiVolume
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        PA_ERROR("UBI volume reference not found.");
        return -ENOENT;
    }

    int32_t result = taf_prop_flash_CloseUbiVolume(it->second);
    if (result != 0)
    {
        PA_ERROR("Failed to close UBI volume.");
        return result;
    }

    pa.ubiVolumeRefMap.erase(it);
    free(ubiVolumeRef);
    return 0;
}

pa_result_t taf_pa_flash_SetUbiVolumeUpdateSize
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    int64_t size
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        PA_ERROR("UBI volume reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_SetUbiVolumeUpdateSize(it->second, size);
}

pa_result_t taf_pa_flash_ReadUbiVolume
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
        PA_ERROR("UBI volume reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_ReadUbiVolume(it->second, offset, dataPtr, dataSizePtr);
}

pa_result_t taf_pa_flash_UpdateUbiVolume
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
        PA_ERROR("UBI volume reference not found.");
        return -ENOENT;
    }

    return taf_prop_flash_UpdateUbiVolume(it->second, dataPtr, dataSize);
}

pa_result_t taf_pa_flash_CopyUbiVolume
(
    const char* srcNamePtr,
    const char* dstNamePtr,
    size_t bufferSize,
    size_t dataSize
)
{
    return taf_prop_flash_CopyUbiVolume(srcNamePtr, dstNamePtr, bufferSize, dataSize);
}

pa_result_t taf_pa_flash_EraseUbiVolume
(
    const char* namePtr
)
{
    return taf_prop_flash_EraseUbiVolume(namePtr);
}

pa_result_t taf_pa_flash_GetUbiVolumeInfo
(
    taf_pa_flash_UbiVolumeRef_t ubiVolumeRef,
    taf_pa_flash_UbiVolumeInfo_t* infoPtr
)
{
    auto& pa = PlatformAdaptor::GetInstance();
    auto it = pa.ubiVolumeRefMap.find(ubiVolumeRef);
    if (it == pa.ubiVolumeRefMap.end())
    {
        PA_ERROR("UBI volume reference not found.");
        return -ENOENT;
    }

    taf_prop_flash_UbiVolumeInfo_t info;
    int32_t result = taf_prop_flash_GetUbiVolumeInfo(it->second, &info);
    if (result != 0)
    {
        PA_ERROR("Failed to get UBI volume information.");
        return result;
    }

    infoPtr->size = info.size;
    infoPtr->lebSize = info.lebSize;
    infoPtr->reservedLebs = info.reservedLebs;
    infoPtr->availLebs = info.availLebs;

    return 0;
}

