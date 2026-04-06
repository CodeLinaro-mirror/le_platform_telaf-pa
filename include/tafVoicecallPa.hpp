/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_VOICECALL_PA_HPP
#define TAF_VOICECALL_PA_HPP

#include <chrono>
#include <future>
#include <unistd.h>
#include <any>
#include "tafCommonPa.h"

namespace tafpa::voicecall {

#define PA_MAX_DESTINATION_LEN_BYTE 51

typedef enum
{
    TAF_PA_VOICECALL_EVENT_ALERTING = 1,             ///< Alerting call; MO only.
    TAF_PA_VOICECALL_EVENT_ACTIVE = 2 ,              ///< Active call; map to legacy connected.
    TAF_PA_VOICECALL_EVENT_ENDED = 3 ,               ///< Call end.
    TAF_PA_VOICECALL_EVENT_OFFLINE = 4 ,             ///< Not supported.
    TAF_PA_VOICECALL_EVENT_WAITING = 5,              ///< Waiting call; map to legacy busy.
    TAF_PA_VOICECALL_EVENT_RESOURCE_BUSY = 6,        ///< Not supported.
    TAF_PA_VOICECALL_EVENT_CALL_END_FAILED = 7,      ///< Call end failed.
    TAF_PA_VOICECALL_EVENT_CALL_ANSWER_FAILED = 8,   ///< Call answer failed.
    TAF_PA_VOICECALL_EVENT_INCOMING = 9,             ///< Incoming call; not answered yet.
    TAF_PA_VOICECALL_EVENT_ONHOLD = 10,              ///< On hold call.
    TAF_PA_VOICECALL_EVENT_DIALING = 11,             ///< MO call; dialing.
    TAF_PA_VOICECALL_EVENT_CALL_HOLD_FAILED = 12,    ///< Call hold failed.
    TAF_PA_VOICECALL_EVENT_CALL_RESUME_FAILED = 13,  ///< Call resume failed.
    TAF_PA_VOICECALL_EVENT_CALL_SWAP_FAILED = 14,    ///< Call swap failed.
} taf_pa_voicecall_event_t;

typedef enum
{
    TAF_PA_VOICECALL_TERM_NORMAL = 1,
    TAF_PA_VOICECALL_TERM_NETWORK_FAIL = 2 ,
    TAF_PA_VOICECALL_TERM_UNOBTAINABLE_NUMBER = 3 ,
    TAF_PA_VOICECALL_TERM_BUSY = 4 ,
    TAF_PA_VOICECALL_TERM_LOCAL = 5,
    TAF_PA_VOICECALL_TERM_REMOTE = 6,
    TAF_PA_VOICECALL_TERM_UNDEFINED = 7,
    TAF_PA_VOICECALL_TERM_REJECTED = 8,
    TAF_PA_VOICECALL_TERM_NORESPONSE = 9,
} taf_pa_voicecall_termination_t;

typedef enum
{
    TAF_PA_VOICECALL_DIR_NONE = 0,
    TAF_PA_VOICECALL_DIR_INCOMING = 1,
    TAF_PA_VOICECALL_DIR_OUTGOING = 2
} taf_pa_voicecall_dir_t;


struct taf_pa_voicecall_CallInfo_t
{
    int8_t phoneId;
    char destId[PA_MAX_DESTINATION_LEN_BYTE];
    taf_pa_voicecall_dir_t direction;
    taf_pa_voicecall_termination_t termination;
};


using taf_pa_voicecall_CallCb = std::function<void(
    pa_result_t errorCode,
    const taf_pa_voicecall_CallInfo_t& callInfo,
    std::any context
)>;

typedef void (*taf_pa_voicecall_EventListener)(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_event_t event,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Init();

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Make(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Stop(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Hold(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Resume(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Answer(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_Swap(
    const taf_pa_voicecall_CallInfo_t& callInfo,
    taf_pa_voicecall_CallCb callback,
    std::any context
);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_RegisterEventListener(taf_pa_voicecall_EventListener listener, std::any context);

PA_SHARED PA_WEAK pa_result_t taf_pa_voicecall_GetCallTermination(const taf_pa_voicecall_CallInfo_t& callInfo, taf_pa_voicecall_termination_t *termination);

}

#endif /* TAF_VOICECALL_PA_HPP */

