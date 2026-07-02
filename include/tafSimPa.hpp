/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_SIM_PA_HPP
#define TAF_SIM_PA_HPP

#include "tafCommonPa.h"
#include <string>
#include <vector>
#include <functional> // For std::function
#include <any>        // For std::any context
#include <memory>     // For std::shared_ptr

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_SIM_PATH 10
#define MAX_SIM_REFRESH_FILES 100

#define TAF_PA_DEFAULT_SLOT_ID 1
#define TAF_PA_SIM_ICCID_BYTES 21
#define TAF_PA_SIM_IMSI_BYTES 16
#define TAF_PA_SIM_PHONE_NUM_MAX_BYTES 18
#define TAF_PA_DEFAULT_TIMEOUT_IN_SECONDS 5
#define TAF_PA_SIM_MAX_APP_TYPE 5
#define TAF_PA_SIM_RESPONSE_MAX_BYTES  (256+2)

typedef enum
{
    TAF_PA_SIM_SESSION_TYPE_UNKNOWN = -1,
    TAF_PA_SIM_SESSION_TYPE_PRI_GW_PROV = 0,
    TAF_PA_SIM_SESSION_TYPE_SEC_GW_PROV = 1,
    TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_1 = 2,
    TAF_PA_SIM_SESSION_TYPE_CARD_ON_SLOT_2 = 3
}taf_pa_sim_SessionType_t;

typedef enum
{
    TAF_PA_SIM_REFRESH_MODE_UNKNOWN = -1,
    TAF_PA_SIM_REFRESH_MODE_RESET = 0,
    TAF_PA_SIM_REFRESH_MODE_INIT = 1,
    TAF_PA_SIM_REFRESH_MODE_INIT_FCN = 2,
    TAF_PA_SIM_REFRESH_MODE_FCN = 3,
    TAF_PA_SIM_REFRESH_MODE_INIT_FULL_FCN =4,
    TAF_PA_SIM_REFRESH_MODE_APP_RESET = 5,
    TAF_PA_SIM_REFRESH_MODE_3G_RESET = 6
}taf_pa_sim_RefreshMode_t;

typedef enum
{
    TAF_PA_SIM_REFRESH_STAGE_UNKNOWN = -1,
    TAF_PA_SIM_REFRESH_STAGE_WAIT_FOR_OK = 0,
    TAF_PA_SIM_REFRESH_STAGE_START = 1,
    TAF_PA_SIM_REFRESH_STAGE_END_WITH_SUCCESS = 2,
    TAF_PA_SIM_REFRESH_STAGE_END_WITH_FAILURE = 3
}taf_pa_sim_RefreshStage_t;

typedef struct {
    uint16_t file_id;
    uint32_t path_len;
    uint8_t path[MAX_SIM_PATH];
}taf_pa_sim_RefreshFile_t;

typedef struct {
    taf_pa_sim_SessionType_t sessionType;
    taf_pa_sim_RefreshMode_t refreshMode;
    taf_pa_sim_RefreshStage_t refreshStage;
    uint32_t filesLen;
    taf_pa_sim_RefreshFile_t files[MAX_SIM_REFRESH_FILES];
}taf_pa_sim_RefreshChangeInd_t;

typedef enum
{
    TAF_PA_SIM_PRESENT,
    TAF_PA_SIM_ABSENT,
    TAF_PA_SIM_READY,
    TAF_PA_SIM_BLOCKED,
    TAF_PA_SIM_BUSY,
    TAF_PA_SIM_POWER_DOWN,
    TAF_PA_SIM_STATE_UNKNOWN,
    TAF_PA_SIM_RESTRICTED,
    TAF_PA_SIM_ERROR
}taf_pa_sim_States_t;

typedef enum
{
    TAF_PA_SIM_EMBEDDED,
    TAF_PA_SIM_SLOT_ID_1,
    TAF_PA_SIM_SLOT_ID_2,
    TAF_PA_SIM_REMOTE,
    TAF_PA_SIM_UNSPECIFIED,
    TAF_PA_SIM_ID_MAX
}taf_pa_sim_Id_t;

typedef enum
{
    TAF_PA_SIM_READ_BINARY = 0xb0,
    TAF_PA_SIM_READ_RECORD = 0xb2,
    TAF_PA_SIM_GET_RESPONSE = 0xc0,
    TAF_PA_SIM_UPDATE_BINARY = 0xd6,
    TAF_PA_SIM_UPDATE_RECORD = 0xdc,
    TAF_PA_SIM_COMMAND_MAX
}taf_pa_sim_Command_t;

typedef struct
{
    taf_pa_sim_Id_t   simId;
    taf_pa_sim_States_t  state;
}taf_pa_sim_pa_event_t;

typedef struct taf_pa_sim_Obj
{
    taf_pa_sim_Id_t     simId;
    char  ICCID[TAF_PA_SIM_ICCID_BYTES];
    char  IMSI[TAF_PA_SIM_IMSI_BYTES];
    char  phoneNumber[TAF_PA_SIM_PHONE_NUM_MAX_BYTES];
    int32_t pinTryCount;
    uint32_t pukTryCount;
}taf_pa_sim_info_t ;

typedef enum
{
    PA_SIM_POWER_OFF = 0,
    PA_SIM_POWER_ON  = 1,
}
taf_pa_sim_power_state_t;

typedef enum
{
   TAF_PA_SIM_PIN1 = 0,
   TAF_PA_SIM_PIN2 = 1,
   TAF_PA_SIM_PUK1 = 2,
   TAF_PA_SIM_PUK2 = 3,
   TAF_PA_SIM_FDN  = 4
}taf_pa_sim_LockType_t;

typedef enum
{
   TAF_PA_APPTYPE_UNKNOWN = 0,
   TAF_PA_APPTYPE_SIM = 1,
   TAF_PA_APPTYPE_USIM = 2,
   TAF_PA_APPTYPE_RUIM = 3,
   TAF_PA_APPTYPE_CSIM = 4,
   TAF_PA_APPTYPE_ISIM = 5
}taf_pa_sim_AppType_t;

typedef enum
{
    TAF_PA_CHANGE_PIN,
    TAF_PA_UNLOCK_BY_PIN,
    TAF_PA_UNLOCK_BY_PUK,
    TAF_PA_SET_LOCK
}taf_pa_sim_LockResponse_t;

typedef struct
{
    taf_pa_sim_Id_t simId;
    taf_pa_sim_LockResponse_t responseType;
    taf_pa_result_t    result;
}telaf_pa_sim_pa_response_event_t;

struct taf_pa_sim_Iccid_t
{
    taf_pa_sim_Id_t simId;
    std::string     ICCID;
};
struct taf_pa_sim_CardInfo_t
{
    taf_pa_sim_Id_t slotId;
    taf_pa_sim_States_t state;
};
struct taf_pa_sim_ResponseInfo_t
{
    taf_pa_sim_Id_t simId;
    taf_pa_sim_LockResponse_t responseType;
    taf_pa_result_t    result;
};
struct taf_pa_sim_UnlockCardResponseInfo_t
{
    taf_pa_sim_Id_t simId;
    taf_pa_sim_LockResponse_t responseType;
    taf_pa_result_t    result;
};
struct taf_pa_sim_UnlockCardPukResponseInfo_t
{
    taf_pa_sim_Id_t simId;
    taf_pa_sim_LockResponse_t responseType;
    taf_pa_result_t    result;
};
struct taf_pa_sim_CardLockResponseInfo_t
{
    taf_pa_sim_Id_t simId;
    taf_pa_sim_LockResponse_t responseType;
    taf_pa_result_t    result;
};

using taf_pa_sim_GeneralCb = std::function<void(taf_pa_result_t result, std::any context)>;

using taf_pa_sim_onSubscriptionInfoChanged =
      std::function<void(const std::shared_ptr<taf_pa_sim_Iccid_t>& iccidDataInfo)>;

using taf_pa_sim_onCardInfoChanged =
      std::function<void(const std::shared_ptr<taf_pa_sim_CardInfo_t>& cardInfo)>;

using taf_pa_sim_ChangeCardPinResponseCb =
      std::function<void(const std::shared_ptr<taf_pa_sim_ResponseInfo_t>& responseInfo)>;

using taf_pa_sim_unlockCardByPinResponseCb =
      std::function<void(const std::shared_ptr<taf_pa_sim_UnlockCardResponseInfo_t>& responseInfo)>;

using taf_pa_sim_unlockCardByPukResponseCb =
      std::function<void(const std::shared_ptr<taf_pa_sim_UnlockCardPukResponseInfo_t>& responseInfo)>;

using taf_pa_sim_setCardLockResponseCb =
      std::function<void(const std::shared_ptr<taf_pa_sim_CardLockResponseInfo_t>& responseInfo)>;

struct taf_pa_sim_EventListener
{
    taf_pa_sim_onSubscriptionInfoChanged onSubscriptionInfoChanged;
    taf_pa_sim_onCardInfoChanged onCardInfoChanged;
    taf_pa_sim_ChangeCardPinResponseCb ChangeCardPinResponseCb;
    taf_pa_sim_unlockCardByPinResponseCb unlockCardByPinResponseCb;
    taf_pa_sim_unlockCardByPukResponseCb unlockCardByPukResponseCb;
    taf_pa_sim_setCardLockResponseCb setCardLockResponseCb;
};

typedef void (*taf_pa_sim_RefreshChangeHandlerFunc_t)
(
    taf_pa_sim_RefreshChangeInd_t refreshChangeInd, void* contextPtr
);

typedef struct taf_pa_sim_RefreshChangeHandler* taf_pa_sim_RefreshChangeHandlerRef_t;

typedef enum
{
    TAF_PA_SIM_PROFILE_TYPE_UNKNOWN   = -1,
    TAF_PA_SIM_PROFILE_TYPE_REGULAR   = 0,
    TAF_PA_SIM_PROFILE_TYPE_EMERGENCY = 1
} taf_pa_sim_ProfileType_t;

typedef enum
{
    TAF_PA_SIM_PROFILE_STATE_UNKNOWN  = -1,
    TAF_PA_SIM_PROFILE_STATE_INACTIVE = 0,
    TAF_PA_SIM_PROFILE_STATE_ACTIVE   = 1
} taf_pa_sim_ProfileState_t;

typedef enum
{
    TAF_PA_SIM_SLOT_UNKNOWN = -1,
    TAF_PA_SIM_SLOT_1       = 0,
    TAF_PA_SIM_SLOT_2       = 1
} taf_pa_sim_SlotId_t;

typedef enum
{
    TAF_PA_SIM_PROFILE_ID_UNKNOWN = -1,
    TAF_PA_SIM_PROFILE_ID_1       = 0,
    TAF_PA_SIM_PROFILE_ID_2       = 1
} taf_pa_sim_ProfileId_t;

typedef struct
{
    taf_pa_sim_ProfileId_t   profileId;
    taf_pa_sim_ProfileType_t type;
    taf_pa_sim_ProfileState_t state;
} taf_pa_sim_ProfileInfo_t;

// Number of profiles for a given slot
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetProfileNum
(
    taf_pa_sim_SlotId_t slot,
    uint8_t* profileNumPtr
);

// Get profile info by slot and index
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetProfile
(
    taf_pa_sim_SlotId_t slot,
    uint8_t index,
    taf_pa_sim_ProfileInfo_t* profileInfoPtr
);

// Set active profile (also enables profile by default)
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_SetActiveProfile
(
    taf_pa_sim_SlotId_t slot,
    taf_pa_sim_ProfileId_t profileId
);

//--------------------------------------------------------------------------------------------------
/**
 *  PA refresh register.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RefreshRegister
(
    taf_pa_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_pa_sim_RefreshFile_t* files
);

//--------------------------------------------------------------------------------------------------
/**
 *  PA refresh unregister.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RefreshUnregister
(
    taf_pa_sim_SessionType_t sessionType,
    uint32_t filesLen,
    taf_pa_sim_RefreshFile_t* files
);

//--------------------------------------------------------------------------------------------------
/**
 *  PA refresh ok.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RefreshOk
(
    taf_pa_sim_SessionType_t sessionType,
    bool* refreshAllow
);

//--------------------------------------------------------------------------------------------------
/**
 *  PA refresh complete.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RefreshComplete
(
   taf_pa_sim_SessionType_t sessionType
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler for SIM refresh
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_AddRefreshChangeHandler
(
    taf_pa_sim_RefreshChangeHandlerFunc_t handlerFuncPtr,
    void* contextPtr,
    taf_pa_sim_RefreshChangeHandlerRef_t* handlerRefPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler for SIM refresh
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RemoveRefreshChangeHandler
(
   taf_pa_sim_RefreshChangeHandlerRef_t handlerRef ///< [IN] Handler reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize PA SIM
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_Init
(

);

TAF_PA_SHARED taf_pa_result_t taf_pa_sim_Deinit
(

);


//--------------------------------------------------------------------------------------------------
/**
 * Register subscription, card and multi sim listeners.
 *
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RegisterListeners
(

);

//--------------------------------------------------------------------------------------------------
/**
 * Deregister subscription, card and multi sim listeners.
 *
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_DeregisterListeners
(

);

//--------------------------------------------------------------------------------------------------
/**
 * Get Iccid.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetIccid
(
    taf_pa_sim_Id_t simId,
    std::string& iccIdStr
);

//--------------------------------------------------------------------------------------------------
/**
 * Get subscriber phone number.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetSubscriberPhoneNumber
(
    taf_pa_sim_Id_t simId,
    std::string& phoneNumber
);

//--------------------------------------------------------------------------------------------------
/**
 * Get IMSI.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetImsi
(
    taf_pa_sim_Id_t simId,
    std::string& imsi
);

//--------------------------------------------------------------------------------------------------
/**
 * Get carrier name.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetCarrierName
(
    taf_pa_sim_Id_t simId,
    std::string& nameString
);

//--------------------------------------------------------------------------------------------------
/**
 * Get home network MCC/MNC.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 * @deprecated This function is deprecated. Use taf_pa_sim_GetHomeNetworkMccMncStr instead.
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetHomeNetworkMccMnc
(
    taf_pa_sim_Id_t simId,
    int* mcc,
    int* mnc
);

//--------------------------------------------------------------------------------------------------
/**
 * Get home network MCC/MNC.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetHomeNetworkMccMncStr
(
    taf_pa_sim_Id_t simId,
    std::string& mcc,
    std::string& mnc
);

//--------------------------------------------------------------------------------------------------
/**
 * Get slot count.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_getSlotCount
(
    int* count
);

//--------------------------------------------------------------------------------------------------
/**
 * Get sim state.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetState
(
    taf_pa_sim_Id_t simId,
    taf_pa_sim_States_t* state
);


//--------------------------------------------------------------------------------------------------
/**
 * Set power state.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_SetPower
(
    taf_pa_sim_Id_t simId,
    taf_pa_sim_power_state_t powerState
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if multisim sub system is ready or not.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_IsSubsystemReady
(
    bool* isReady
);

//--------------------------------------------------------------------------------------------------
/**
 * Select sim slot.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_selectSimSlot
(
    taf_pa_sim_Id_t simId
);

//--------------------------------------------------------------------------------------------------
/**
 * Change sim card pin.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_ChangeCardPin
(
    taf_pa_sim_LockType_t lockType,
    const char* oldpinPtr,
    const char* newpinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Unlock sim card by pin.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_UnlockCardByPin
(
    taf_pa_sim_LockType_t lockType,
    const char* pinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Unlock sim card by Puk.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_UnlockCardByPuk
(
    taf_pa_sim_LockType_t lockType,
    const char* pukPtr,
    const char* newpinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Set SIM card lock.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_SetCardLock
(
    taf_pa_sim_LockType_t lockType,
    const char* pinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Unlocks the SIM card and disables the lock request of PIN1/PIN2.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_SetCardUnLock
(
    taf_pa_sim_LockType_t lockType,
    const char* pinPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Get SIM app types.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetAppTypes
(
    taf_pa_sim_AppType_t* appTypePtr,
    size_t* appTypeNumElementsPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Open logical channel.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_OpenLogicalChannel
(
    taf_pa_sim_AppType_t appType,
    uint8_t* channelPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Open logical channel by Aid.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_OpenLogicalChannelByAid
(
    const char* aid,
    uint8_t* channelIdPtr,
    taf_pa_sim_GeneralCb callback,
    std::any context
);


//--------------------------------------------------------------------------------------------------
/**
 * Close logical channel.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_CloseLogicalChannel
(
    uint8_t channelId,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send APDU on channel.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_SendApduOnLogicalChannel
(
    uint8_t channel,
    uint8_t* responseApduPtr,
    size_t* responseApduNumElementsPtr,
    uint8_t p1, uint8_t p2, uint8_t p3,
    uint8_t cla, uint8_t instruction,
    std::vector<uint8_t> data,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send APDU.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_SendApdu
(
    uint8_t* responseApduPtr,
    size_t* responseApduNumElementsPtr,
    uint8_t p1, uint8_t p2, uint8_t p3,
    uint8_t cla, uint8_t instruction,
    std::vector<uint8_t> data,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//--------------------------------------------------------------------------------------------------
/**
 * Send Command.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//--------------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_ExchangeSimIO
(
    taf_pa_sim_Command_t command,
    uint8_t *p1, uint8_t *p2,
    uint8_t *p3, const uint8_t* dataPtr,
    size_t dataNumElements,const char* pathPtr,
    uint8_t *sw1,uint8_t *sw2,
    uint8_t* responsePtr, size_t* responseNumElementsPtr,
    uint16_t field,
    taf_pa_sim_GeneralCb callback,
    std::any context
);

//----------------------------------------------------------------------------------------------
/**
 * Register event listeners.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//----------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_RegisterEventListener
(
    taf_pa_sim_EventListener* eventListener,
    std::any context
);

//----------------------------------------------------------------------------------------------
/**
 * Get remaining PIN retries.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//----------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetRemainingPINTries
(
    taf_pa_sim_Id_t simId,
    int32_t* retryCount
);

//----------------------------------------------------------------------------------------------
/**
 * Get remaining PUK tries.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//----------------------------------------------------------------------------------------------
TAF_PA_SHARED taf_pa_result_t taf_pa_sim_GetRemainingPukTries
(
    taf_pa_sim_Id_t simId,
    uint32_t*  remainingPukTries
);

//----------------------------------------------------------------------------------------------
/**
 * Get EID string.
 * @return
 *  - TAF_PA_OK on success
 *  - TAF_PA_FAULT on failure
 *
 */
//----------------------------------------------------------------------------------------------
TAF_PA_SHARED TAF_PA_WEAK taf_pa_result_t taf_pa_sim_GetEID
(
    taf_pa_sim_Id_t simId,
    std::string&  eidStr
);
#ifdef __cplusplus
}
#endif

#endif /* TAF_SIM_PA_HPP */
