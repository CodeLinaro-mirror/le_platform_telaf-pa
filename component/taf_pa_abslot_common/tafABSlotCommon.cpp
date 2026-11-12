/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <array>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>

#include "taf_pa_abslot.h"

namespace
{
constexpr const char* kGetActiveSlotCommand = "/usr/bin/nad-abctl --boot_slot";
constexpr size_t kSlotSuffixBufferSize = 8;

taf_pa_result_t ParseSlotSuffix
(
    const char* slotSuffixPtr,
    taf_pa_ABSlot_t* slotPtr
)
{
    if (slotSuffixPtr == nullptr || slotPtr == nullptr)
    {
        TAF_PA_ERROR("Invalid parameter while parsing active slot.");
        return TAF_PA_BAD_PARAMETER;
    }

    if (strncmp(slotSuffixPtr, "_a", strlen("_a")) == 0)
    {
        *slotPtr = TAF_PA_ABSLOT_A;
        return TAF_PA_OK;
    }

    if (strncmp(slotSuffixPtr, "_b", strlen("_b")) == 0)
    {
        *slotPtr = TAF_PA_ABSLOT_B;
        return TAF_PA_OK;
    }

    *slotPtr = TAF_PA_ABSLOT_UNKNOWN;
    TAF_PA_ERROR("Unknown active slot suffix: %s", slotSuffixPtr);
    return TAF_PA_FAULT;
}
}

taf_pa_result_t taf_pa_abslot_GetActiveSlot
(
    taf_pa_ABSlot_t* slotPtr
)
{
    if (slotPtr == nullptr)
    {
        TAF_PA_ERROR("slotPtr is null.");
        return TAF_PA_BAD_PARAMETER;
    }

    *slotPtr = TAF_PA_ABSLOT_UNKNOWN;

    FILE* pipePtr = popen(kGetActiveSlotCommand, "r");
    if (pipePtr == nullptr)
    {
        TAF_PA_ERROR("Failed to run active slot command: %s", kGetActiveSlotCommand);
        return TAF_PA_FAULT;
    }

    std::array<char, kSlotSuffixBufferSize> slotSuffix = { 0 };
    if (fgets(slotSuffix.data(), slotSuffix.size(), pipePtr) == nullptr)
    {
        TAF_PA_ERROR("Failed to read active slot command output.");
        pclose(pipePtr);
        return TAF_PA_FAULT;
    }

    int closeStatus = pclose(pipePtr);
    if (closeStatus == -1)
    {
        TAF_PA_ERROR("Failed to close active slot command pipe.");
        return TAF_PA_FAULT;
    }

    // nad-abctl uses the slot index as its exit status: slot A returns 0 and
    // slot B returns 1. Accept both expected statuses, but reject abnormal
    // termination and other command-error statuses.
    if (!WIFEXITED(closeStatus))
    {
        TAF_PA_ERROR("Active slot command terminated abnormally.");
        return TAF_PA_FAULT;
    }

    const int exitStatus = WEXITSTATUS(closeStatus);
    if (exitStatus != 0 && exitStatus != 1)
    {
        TAF_PA_ERROR("Active slot command exited with status %d.", exitStatus);
        return TAF_PA_FAULT;
    }

    return ParseSlotSuffix(slotSuffix.data(), slotPtr);
}
