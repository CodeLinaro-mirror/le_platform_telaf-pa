/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_PMS_PA_HPP
#define TAF_PMS_PA_HPP

#include "tafCommonPa.h"

#include <stdint.h>
#include <vector>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

struct taf_pa_pms_RefStruct_t;
typedef struct taf_pa_pms_RefStruct_t * taf_pa_pms_Reference_t;

typedef enum
{
    EV_WAKEUP_INFO,
    EV_SVC_STATUS,
    EV_POWER_STATE_UPDATE,
    EV_MACHINE_UPDATE,
    EV_CONSOLIDATED_INFO,
}
taf_pa_pms_PaEventType_t;

typedef struct
{
    taf_pa_pms_PaEventType_t evType;
    void * evPayload;
    size_t evPsize;
}
taf_pa_pms_Event_t;

typedef enum
{
    SVC_AVAILABLE = 0,
    SVC_UNAVAILABLE = 1,
}
taf_pa_pms_ServiceStatus_t;

#define MAX_MACHINE_NAME 31
#define PA_MAX_NAME_LEN MAX_MACHINE_NAME

typedef enum
{
    taf_pa_pms_ACK,
    taf_pa_pms_NACK,
}
taf_pa_pms_Ack_t;

typedef enum
{
    taf_pa_pms_PwrSts_SUSPEND,
    taf_pa_pms_PwrSts_RESUME,
    taf_pa_pms_PwrSts_SHUTDOWN,
    taf_pa_pms_PwrSts_UNKNOWN,
}
taf_pa_pms_PowerState_t;

typedef struct
{
    taf_pa_pms_PowerState_t state;
    char                    machineName[MAX_MACHINE_NAME];
}
taf_pa_pms_PowerUpdateEvent_t;

typedef enum
{
    MACHINE_AVAILABLE,
    MACHINE_UNAVAILABLE,
}
taf_pa_pms_MachineStatusEvent_t;

typedef struct
{
    taf_pa_pms_MachineStatusEvent_t machineEvent;
    char                            machineName[MAX_MACHINE_NAME];
}
taf_pa_pms_MachineUpdateEvent_t;

typedef struct
{
    char clientName[PA_MAX_NAME_LEN];
    char machineName[PA_MAX_NAME_LEN];
}
taf_pa_pms_ClientInfo_t;

typedef struct
{
#define TAF_CONSOLIDATED_CLNT_SIZE 64
    taf_pa_pms_ClientInfo_t nackResponseClntData[TAF_CONSOLIDATED_CLNT_SIZE];
    taf_pa_pms_ClientInfo_t unresponsiveClntData[TAF_CONSOLIDATED_CLNT_SIZE];
    uint32_t nackResponseClntSize;
    uint32_t unresponsiveClntSize;
}
taf_pa_pms_ConsolidatedInfo_t;

/* ------------------------------------ PA - APIs (S) ------------------------------------ */
typedef void (* SendEventFunc_t)
(
    taf_pa_pms_Event_t * ev
);

PA_SHARED pa_result_t taf_pa_pms_Init
(
    taf_pa_pms_Reference_t   *paRefPtr,
    SendEventFunc_t           fnSendEvent,
    uint32_t                  timeoutMs ///< Note: if we have 3 managers, the total is (3 * timeoutMs)
);

PA_SHARED void taf_pa_pms_Deinit
(
    taf_pa_pms_Reference_t   *paRefPtr
);

PA_SHARED pa_result_t taf_pa_pms_SetPowerStateAsMaster
(
    taf_pa_pms_Reference_t    paRef,
    taf_pa_pms_PowerState_t   state,
    const char               *name
);

PA_SHARED pa_result_t taf_pa_pms_SendAckForStateUpdate
(
    taf_pa_pms_Reference_t    paRef,
    taf_pa_pms_PowerState_t   state,
    taf_pa_pms_Ack_t          ack
);

PA_SHARED pa_result_t taf_pa_pms_GetAllMachineNames
(
    taf_pa_pms_Reference_t    paRef,
    std::vector<std::string> &machineNames
);

PA_SHARED pa_result_t taf_pa_pms_SetModemWakeupFilter
(
    taf_pa_pms_Reference_t    paRef,
    uint32_t                  wsBitmask
);

PA_SHARED pa_result_t taf_pa_pms_GetModemWakeupFilter
(
    taf_pa_pms_Reference_t    paRef,
    uint32_t                 *wsBitmaskPtr
);

/* ------------------------------------ PA - APIs (E) ------------------------------------ */

#ifdef __cplusplus
}
#endif

#endif // TAF_PMS_PA_HPP
