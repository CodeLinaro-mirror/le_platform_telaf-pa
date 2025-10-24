/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef _PM_LAYER_HPP_
#define _PM_LAYER_HPP_

#include "taf_pa_common.h"

#include <stdint.h>
#include <vector>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: Must define the 'PaName' in your pa header file !! */

#define CAT_(a,b)      CAT_I(a,b)
#define CAT_I(a,b)     a ## b
#define STRIMPL(x)     #x
#define STR(x)         STRIMPL(x)

#define IF(c)          IF_I(c)
#define IF_I(c)        CAT_(IF_, c)
#define IF_1(t, f)     t
#define IF_0(t, f)     f

#define PROBE()        ~, 1
#define SECOND(a,b,...) b
#define IS_PROBE(...)  SECOND(__VA_ARGS__, 0)

/* Ignore <PaName> collection */
#define _OMIT_TAG_Result PROBE()
#define _OMIT_TAG_Too    PROBE()
#define _OMIT_TAG_Many   PROBE()
#define IS_OMIT_TAG(x)   IS_PROBE(CAT_(_OMIT_TAG_, x))

#define _JoinTypeImpl(name, type)    taf_pa_ ## name ## _ ## type ## _t
#define _JoinType(name, type)        _JoinTypeImpl(name, type)
#define _JoinTypeImplNoName(type)    taf_pa_ ## type ## _t

#define _JoinFuncImpl(name, fn)      taf_pa_ ## name ## _ ## fn
#define _JoinFunc(name, fn)          _JoinFuncImpl(name, fn)
#define PaFn(fn)                     _JoinFunc(PaName, fn)

/* If type in {Result, ... } -> taf_pa_<type>_t，else taf_pa_<PaName>_<type>_t */
#define PaType(type) \
  IF(IS_OMIT_TAG(type))(_JoinTypeImplNoName(type), _JoinType(PaName, type))

/* taf_pa_Result_<Tag> */
#define PaResult(tag) taf_pa_Result_ ## tag

typedef enum {
    PaResult(OK),
    PaResult(FAULT),
    PaResult(TIMEOUT),
    PaResult(BUSY),
    PaResult(NOT_IMPLEMENTED),

    PaResult(MAX),
} PaType(Result);

#define PaName pms

struct PaType(RefStruct);
typedef struct PaType(RefStruct) * PaType(Reference);

typedef enum
{
    EV_WAKEUP_INFO,
    EV_SVC_STATUS,
    EV_POWER_STATE_UPDATE,
    EV_MACHINE_UPDATE,
    EV_CONSOLIDATED_INFO,
}
PaEventType_t;

typedef struct
{
    PaEventType_t evType;
    void * evPayload;
    size_t evPsize;
} PaEvent_t;

typedef enum {
    SVC_AVAILABLE = 0,
    SVC_UNAVAILABLE = 1,
} ServiceStatus_t;

#define MAX_MACHINE_NAME 31
#define PA_MAX_NAME_LEN MAX_MACHINE_NAME

typedef enum {
#define PaAck(x) taf_pa_pms_ ## x
    PaAck(ACK),
    PaAck(NACK),
} PaType(Ack);

typedef enum {
#define PaPwrState(x) taf_pa_pms_ ## x
    PaPwrState(SUSPEND),
    PaPwrState(RESUME),
    PaPwrState(SHUTDOWN),
    PaPwrState(UNKNOWN),
}
PaType(PowerState);

typedef struct
{
    PaType(PowerState) state;
    char               machineName[MAX_MACHINE_NAME];
}
PowerUpdateEvent_t;

typedef enum
{
    MACHINE_AVAILABLE,
    MACHINE_UNAVAILABLE,
}
MachineStatusEvent_t;

typedef struct
{
    MachineStatusEvent_t machineEvent;
    char                 machineName[MAX_MACHINE_NAME];
}
MachineUpdateEvent_t;

typedef struct
{
    char clientName[PA_MAX_NAME_LEN];
    char machineName[PA_MAX_NAME_LEN];
}
ClientInfo_t;

typedef struct
{
#define TAF_CONSOLIDATED_CLNT_SIZE 64
    ClientInfo_t nackResponseClntData[TAF_CONSOLIDATED_CLNT_SIZE];
    ClientInfo_t unresponsiveClntData[TAF_CONSOLIDATED_CLNT_SIZE];
    uint32_t nackResponseClntSize;
    uint32_t unresponsiveClntSize;
}
PaType(ConsolidatedInfo);

/* ------------------------------------ PA - APIs (S) ------------------------------------ */
typedef void (* SendEventFunc_t)
(
    PaEvent_t * ev
);

PA_SHARED PA_WEAK PaType(Result) PaFn(Init)
(
    PaType(Reference)  *paRefPtr,
    SendEventFunc_t     fnSendEvent,
    uint32_t            timeoutMs ///< Note: if we have 3 managers, the total is (3 * timeoutMs)
);

PA_SHARED PA_WEAK void PaFn(Deinit)
(
    PaType(Reference) *paRefPtr
);

PA_SHARED PA_WEAK PaType(Result) PaFn(SetPowerStateAsMaster)
(
    PaType(Reference)         paRef,
    PaType(PowerState)        state,
    const char               *name
);

PA_SHARED PA_WEAK PaType(Result) PaFn(SendAckForStateUpdate)
(
    PaType(Reference)  paRef,
    PaType(PowerState) state,
    PaType(Ack)        ack
);

PA_SHARED PA_WEAK PaType(Result) PaFn(GetAllMachineNames)
(
    PaType(Reference)         paRef,
    std::vector<std::string> &machineNames
);

PA_SHARED PA_WEAK PaType(Result) PaFn(SetModemWakeupFilter)
(
    PaType(Reference)         paRef,
    uint32_t                  wsBitmask
);

PA_SHARED PA_WEAK PaType(Result) PaFn(GetModemWakeupFilter)
(
    PaType(Reference)         paRef,
    uint32_t                 *wsBitmaskPtr
);

/* ------------------------------------ PA - APIs (E) ------------------------------------ */

#ifdef __cplusplus
}
#endif

#endif
