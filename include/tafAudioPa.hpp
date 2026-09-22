/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef TAF_AUDIO_PA_HPP
#define TAF_AUDIO_PA_HPP

#include <any>
#include "tafCommonPa.h"
#include <chrono>
#include <future>
#include <unistd.h>
#include <vector>

using namespace std;
namespace tafpa::audio {

#define TAF_PA_AUDIO_MAX_NUM_OF_PLAYBACK_FILES 8

enum PaAudioIf
{
    TAF_PA_AUDIO_IF_CODEC_SOURCE_1,
    TAF_PA_AUDIO_IF_CODEC_SINK_1,
    TAF_PA_AUDIO_IF_CODEC_SOURCE_2,
    TAF_PA_AUDIO_IF_CODEC_SINK_2,
    TAF_PA_AUDIO_IF_CODEC_SOURCE_3,
    TAF_PA_AUDIO_IF_CODEC_SINK_3,
    TAF_PA_AUDIO_IF_CODEC_SOURCE_4,
    TAF_PA_AUDIO_IF_CODEC_SINK_4,
    TAF_PA_AUDIO_IF_CODEC_SOURCE_5,
    TAF_PA_AUDIO_IF_CODEC_SINK_5,
    TAF_PA_AUDIO_NUM_INTERFACES
};

/**
 * Audio PA format.
 */
enum class PaFileFormat
{
    WAVE,
    AMR_NB,
    AMR_WB,
    FILE_MAX
};

enum class PaStreamDirection {

    /** Default direction (invalid) */
    NONE = -1,

    /** Specifies that the audio data will flow towards a sink device */
    RX = 1,

    /** Specifies that the audio data originates from a source device */
    TX = 2,
};

/**
 *  Defines the type of the audio stream and the type's purpose.
 */
enum class PaStreamType {

    /** Default type (invalid) */
    NONE = -1,

    /** Used for audio over a cellular network */
    VOICE_CALL = 1,

    /** Used for playing audio, for example playing music and notifications */
    PLAY = 2,

    /** Used for capturing audio, for example recording sound using a mic */
    CAPTURE = 3,

    /** Used for audio loopback */
    LOOPBACK = 4,
};

enum PaSlotId{
   SLOT_ID_INVALID = -1,
   SLOT_ID_DEFAULT = 1,
   SLOT_ID_1 = SLOT_ID_DEFAULT,
   SLOT_ID_2 = 2,
   SLOT_ID_MAX = SLOT_ID_2,
};

enum PaChannelType{

    /** Specifies the left channel */
    LEFT = (1 << 0),

    /** Specifies the right channel */
    RIGHT = (1 << 1),
};

/**
 * The PA audio subsystem state.
 */
enum class SubsystemState_e
{
    AVAILABLE,   ///< Subsystem available.
    UNAVAILABLE, ///< Subsystem unavailable.
    FAILED       ///< Subsystem initialization failed.
};

struct PaStreamConfig {

    /** @ref PaStreamType - defines purpose of the stream */
    PaStreamType type;

    /** @ref PaSlotId - specifies the slot ID where the UICC card is inserted */
    PaSlotId slotId;

    /** Sample rate in Hz. Typical values are 8k, 16k, 32k and 48k.
     *  For Bluetooth use-cases, supported values are 8k and 16k */
    uint32_t sampleRate;

    /** @ref ChannelTypeMask - defines audio channels to use */
    int channelTypeMask;

    /** @ref PaAudioFormat - defines audio format */
    PaFileFormat format;

    /** Defines the list of audio devices @ref PaAudioIf to use for this stream.
     *  For PaStreamType::PLAY, a single sink device should be specified.
     *  For PaStreamType::CAPTURE, a single source device should
     *  be specified. For PaStreamType::VOICE_CALL and PaStreamType::LOOPBACK, both
     *  sink and source should be specified with sink as the first device and
     *  source as the second. */
    std::vector<PaAudioIf> deviceTypes;

    /** For an in-call audio usecase, this represents the voice path direction
     *  @ref PaStreamDirection */
    std::vector<PaStreamDirection> streamDir;

    /** True to enable ECNR on an ecall */
    bool ecnrMode = false;
};

typedef struct
{
    std::string absoluteFilePath; //Absolute path of the file
    int32_t  repeat; // Defines how a file should be played. -1 = infinite loop, 0 = play once,
                     // x = repeat X times.
    PaStreamConfig streamConfig;
}taf_pa_audio_PlayFileInfo_t;

/**
 *  Defines the characteristics of the DTMF tone.
 */
struct PaDtmfTone {

    /** Lower frequency associated with the DTMF tone */
    int lowFreq;

    /** Higher frequency associated with the DTMF tone */
    int highFreq;

    /** @ref PaStreamDirection associated with the stream */
    PaStreamDirection direction;
};

using taf_pa_audio_cb = std::function<void(
    taf_pa_result_t errorCode,
    std::any context
)>;

/**
 * PaStreamBuffer used for
 */
class IPaStreamBuffer {
 public:

   virtual size_t getMinSize() = 0;

   virtual size_t getMaxSize() = 0;

   virtual uint8_t *getRawBuffer() = 0;

   virtual uint32_t getDataSize() = 0;

   virtual void setDataSize(uint32_t size) = 0;

   virtual taf_pa_result_t reset() = 0;

    virtual ~IPaStreamBuffer() {};
};

using taf_pa_audio_Readcb = std::function<void(
    std::shared_ptr<IPaStreamBuffer> buffer,
    taf_pa_result_t paRes
)>;

/**
 *  Receives status of the playback.
 */
class IPaPlayListListener {
 public:

    virtual void onPlaybackStarted() { }

    virtual void onPlaybackStopped() { }

    virtual void onError(int error, std::string file) { }

    virtual void onFilePlayed(std::string file) { }

    virtual void onPlaybackFinished() { }

    virtual ~IPaPlayListListener() { }
};

class PaAudioCaptureStream{
 public:

    virtual std::shared_ptr<IPaStreamBuffer> getStreamBuffer() = 0;

    virtual taf_pa_result_t read(std::shared_ptr<IPaStreamBuffer> buffer,
            uint32_t bytesToRead, taf_pa_audio_Readcb callback = nullptr) = 0;

    virtual ~PaAudioCaptureStream() {};
};

class IPaDtmfListener {
 public:
    /**
     * Called when a DTMF tone is detected.
     *
     * @param [in] dtmfTone Contains details of the tone detected
     */
    virtual void onDtmfToneDetection(PaDtmfTone dtmfTone) {}

    virtual ~IPaDtmfListener() {}
};

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_Init();

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_Deinit();

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_CreateStream(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_DeleteStream(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_StartAudio(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_StopAudio(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_StartPlayback(
    std::vector<taf_pa_audio_PlayFileInfo_t> &playFileInfos,
    int listSiz,
    std::weak_ptr<IPaPlayListListener> pbStatusListener
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_StopPlayback(
    PaStreamConfig streamConfig
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_GetCaptureStream(
    PaStreamDirection streamDir,
    std::shared_ptr<PaAudioCaptureStream>& captureStream
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_SetVolume(
    PaStreamConfig streamConfig,
    double volLevel,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_GetVolume(
    PaStreamConfig streamConfig,
    double *volLevel,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_SetMute(
    PaStreamConfig streamConfig,
    bool isMute,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_GetMute(
    PaStreamConfig streamConfig,
    bool *isMute,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_PlaySignallingDtmfOnTx(
    uint32_t slotId,
    const char dtmf,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_StopSignallingDtmfOnTx(
    uint32_t slotId,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_PlayDtmf(
    PaDtmfTone dtmfTone,
    uint16_t duration,
    uint16_t gain,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_StopDtmf(
    PaStreamDirection direction,
    taf_pa_audio_cb callback,
    std::any context
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_registerDtmfListener(
    std::weak_ptr<IPaDtmfListener> dtmfListener
);

TAF_PA_SHARED taf_pa_result_t taf_pa_audio_deregisterDtmfListener(
    std::weak_ptr<IPaDtmfListener> dtmfListener
);

/**
 * The audio subsystem state callback.
 * @param [in] subsystemState     The subsystem state.
 * @param [in] context            The app provided context.
 */
using taf_pa_audio_SubsystemStateChangeCb =
    std::function<void
                    (
                        SubsystemState_e      subsystemState,
                        std::shared_ptr<void> context
                    )>;

/**
 * Register audio subsystem state events callback
 */
TAF_PA_SHARED taf_pa_result_t AddSubsystemStateChangeListener
(
    taf_pa_audio_SubsystemStateChangeCb callBack,
    ///< [IN] The callback function.
    std::shared_ptr<void> context,
    ///< [IN] The context pointer.
    uint16_t &id
    ///< [OUT] The ID of the registered callback.
);

/**
 * Deregister a previously registered subsystem state change callback
 */
TAF_PA_SHARED taf_pa_result_t RemoveSubsystemStateChangeListener
(
    uint16_t id
        ///< [IN] The ID of the registered callback.
);

}
#endif /* TAF_AUDIO_PA_HPP */
