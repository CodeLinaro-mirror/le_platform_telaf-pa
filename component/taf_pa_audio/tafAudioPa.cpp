/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <queue>
#include <atomic>
#include <telux/audio/AudioFactory.hpp>
#include "telux/audio/AudioManager.hpp"
#include "telux/audio/AudioPlayer.hpp"
#include "telux/tel/CallManager.hpp"
#include "telux/tel/CallListener.hpp"
#include "telux/common/CommonDefines.hpp"
#include "telux/common/Utils.hpp"
#include "telux/tel/PhoneDefines.hpp"
#include "telux/tel/PhoneFactory.hpp"
#include "tafAudioPa.hpp"
#include "tafInternalCommonPa.h"

// Thread-safe initialization flag
static std::atomic<bool> gAudioPaInitialized(false);

#define SUBSYSTEM_TIMEOUT 5

#define DEVICE_TYPE_SINK_0   1
#define DEVICE_TYPE_SINK_1   2
#define DEVICE_TYPE_SINK_2   3
#define DEVICE_TYPE_SINK_3   4
#define DEVICE_TYPE_SINK_4   5
#define DEVICE_TYPE_SOURCE_0 257
#define DEVICE_TYPE_SOURCE_1 258
#define DEVICE_TYPE_SOURCE_2 259
#define DEVICE_TYPE_SOURCE_3 260
#define DEVICE_TYPE_SOURCE_4 261

#define TOTAL_BUFFERS              2
#define DEFAULT_SAMPLERATE         48000
#define DEFAULT_BITSPERSAMPLE      16
#define DEFAULT_NUM_CHANNELS       2
#define BITS_PER_BYTE              8

#define CALLBACK_TO_SET_RESULT                                                               \
    auto cb = [callback, context](ErrorCode error) {                                         \
        PA_INFO("callback received");                                                        \
            if (ErrorCode::SUCCESS == error) {                                               \
                PA_DEBUG("Request is successful!!");                                         \
                callback(PA_OK, context);                                                    \
            }                                                                                \
            else{                                                                            \
                PA_ERROR("Request failed, err %s", Utils::getErrorCodeAsString(error).c_str());      \
                callback(PA_FAULT, context);                                                 \
            }                                                                                \
    };                                                                                       \

/* Implementation */
using namespace tafpa::audio;
using namespace telux::audio;
using namespace telux::common;

class AudioPAController {

struct SubsystemEventsCallbackEntry_t
{
    uint16_t id;
    taf_pa_audio_SubsystemStateChangeCb callBack;
    std::shared_ptr<void> context;
};

public:
    static std::shared_ptr<AudioPAController> getInstance()
    {
        static std::shared_ptr<AudioPAController> instance(new AudioPAController());
        return instance;
    }

    pa_result_t initialize();
    pa_result_t deinitialize();

    pa_result_t createStream(
        PaStreamConfig config,
        taf_pa_audio_cb callback,
        std::any context);
    pa_result_t deleteStream(
        PaStreamConfig config,
        taf_pa_audio_cb callback,
        std::any context);
    pa_result_t startAudio(
        PaStreamConfig config,
        taf_pa_audio_cb callback,
        std::any context);
    pa_result_t stopAudio(
        PaStreamConfig config,
        taf_pa_audio_cb callback,
        std::any context);
    pa_result_t setVolume(
        PaStreamConfig config,
        double volLevel,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t getVolume(
        PaStreamConfig config,
        double *volLevel,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t setMute(
        PaStreamConfig config,
        bool isMute,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t getMute(
        PaStreamConfig config,
        bool *isMute,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t startPlayback(
        std::vector<taf_pa_audio_PlayFileInfo_t> &playFileInfos,
        int listSiz,
        std::weak_ptr<IPaPlayListListener> pbStatusListener
    );
    pa_result_t stopPlayback(
        PaStreamConfig streamConfig
    );
    std::shared_ptr<PaAudioCaptureStream> GetCaptureStream
    (
        PaStreamDirection streamDir
    );
    pa_result_t playSignallingDtmfOnTx(
        uint32_t slotId,
        const char dtmf,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t stopSignallingDtmfOnTx(
        uint32_t slotId,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t playDtmf(
        PaDtmfTone dtmfTone, uint16_t duration,
        uint16_t gain,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t registerDtmfListener(
        std::weak_ptr<IPaDtmfListener> dtmfListener
    );
    pa_result_t deregisterDtmfListener(
        std::weak_ptr<IPaDtmfListener> dtmfListener
    );
    pa_result_t stopDtmf(
        PaStreamDirection direction,
        taf_pa_audio_cb callback,
        std::any context
    );
    pa_result_t registerAudioSubsystemChangeListener(
        taf_pa_audio_SubsystemStateChangeCb callBack,
        std::shared_ptr<void> context,
        uint16_t &id
    );
    pa_result_t deregisterAudioSubsystemChangeListener(
        uint16_t id
    );
    void SendSubsystemEventToClients(SubsystemState_e state);

    class tafPaPromptsStatusListener : public telux::audio::IPlayListListener {
    public:
        void onPlaybackStarted()
        {
            auto pbListener = pbStatusListener.lock();
            pbListener->onPlaybackStarted();
        };
        void onPlaybackStopped()
        {
            auto pbListener = pbStatusListener.lock();
            pbListener->onPlaybackStopped();
        };
        void onError(telux::common::ErrorCode error, std::string file)
        {
            auto pbListener = pbStatusListener.lock();
            pbListener->onError((int)error, file);
        };
        void onFilePlayed(std::string file)
        {
            auto pbListener = pbStatusListener.lock();
            pbListener->onFilePlayed(file);
        };
        void onPlaybackFinished()
        {
            auto pbListener = pbStatusListener.lock();
            pbListener->onPlaybackFinished();
        };
        std::weak_ptr<IPaPlayListListener> pbStatusListener;
    };

    class tafPaStreamBuffer : public tafpa::audio::IPaStreamBuffer {
    public:
        tafPaStreamBuffer(std::shared_ptr<IStreamBuffer> buffer) : streamBuffer_(buffer)
        {
            return;
        }
        size_t getMinSize() override
        {
            return streamBuffer_->getMinSize();
        };
        size_t getMaxSize()
        {
            return streamBuffer_->getMaxSize();
        };
        uint8_t *getRawBuffer()
        {
            return streamBuffer_->getRawBuffer();
        };
        uint32_t getDataSize()
        {
            return streamBuffer_->getDataSize();
        };
        void setDataSize(uint32_t size)
        {
            streamBuffer_->setDataSize(size);
        };
        std::shared_ptr<IStreamBuffer> getStreamBuffer()
        {
            return streamBuffer_;
        }
        pa_result_t reset()
        {
            Status result = streamBuffer_->reset();
            if(Status::SUCCESS == result)
            {
                return PA_OK;
            }
            return PA_FAULT;
        };
        protected:
        std::shared_ptr<IStreamBuffer> streamBuffer_;
    };

    class tafPaAudioCaptureStream : public tafpa::audio::PaAudioCaptureStream {
    public:
        tafPaAudioCaptureStream(
            std::shared_ptr<IAudioCaptureStream> stream)
            : captureStream_(stream)
        {
            return;
        }
        std::shared_ptr<IPaStreamBuffer> getStreamBuffer()
        {
            return std::make_shared<tafPaStreamBuffer>(this->captureStream_->getStreamBuffer());
        };
        pa_result_t read(std::shared_ptr<IPaStreamBuffer> paBuffer,
            uint32_t bytesToRead, taf_pa_audio_Readcb callback) {
            auto readCb = [callback, paBuffer](std::shared_ptr<telux::audio::IStreamBuffer> buffer,
                telux::common::ErrorCode error){
                    if (error != ErrorCode::SUCCESS)
                    {
                        callback(paBuffer, PA_FAULT);
                    }
                    else
                    {
                        callback(paBuffer, PA_OK);
                    }
            };
            std::shared_ptr<tafPaStreamBuffer> paStreamBufferImpl =
                    std::dynamic_pointer_cast<tafPaStreamBuffer>(paBuffer);
            Status readRes = captureStream_->read((paStreamBufferImpl->getStreamBuffer()),
                    bytesToRead, readCb);
            if (readRes == Status::SUCCESS)
                return PA_OK;
            else
                return PA_FAULT;
        };
        protected:
            std::shared_ptr<IAudioCaptureStream> captureStream_;
    };

    class tafPaDtmfListener : public telux::audio::IVoiceListener {
        public:
        void onDtmfToneDetection(DtmfTone dtmfTone)
        {
            PaDtmfTone paDtmfTone = {};
            paDtmfTone.lowFreq = (int)dtmfTone.lowFreq;
            paDtmfTone.highFreq = (int)dtmfTone.highFreq;
            paDtmfTone.direction = static_cast<PaStreamDirection>(dtmfTone.direction);
            auto listener = paDtmfListener.lock();
            listener->onDtmfToneDetection(paDtmfTone);

        };
        std::weak_ptr<IPaDtmfListener> paDtmfListener;
    };

    class tafPaServiceStatusListener : public telux::audio::IAudioListener {
        public:
            void onServiceStatusChange(telux::common::ServiceStatus status)
            {
                auto pACtrl = AudioPAController::getInstance();
                if (status == telux::common::ServiceStatus::SERVICE_UNAVAILABLE) {
                    PA_ERROR("Audio Service UNAVAILABLE");
                    pACtrl->SendSubsystemEventToClients(SubsystemState_e::UNAVAILABLE);
                }
                else if (status == telux::common::ServiceStatus::SERVICE_AVAILABLE) {
                    PA_INFO("Audio Service AVAILABLE");
                    pACtrl->SendSubsystemEventToClients(SubsystemState_e::AVAILABLE);
                }
            }
    };

    class CommandCallback : public telux::common::ICommandResponseCallback
    {
        public:
            CommandCallback() = default;
            ~CommandCallback() = default;

            CommandCallback(
                taf_pa_audio_cb callback,
                std::any context)
                : callback_(callback), context_(context)
            {
                return;
            }

            void commandResponse(telux::common::ErrorCode error)
            {
                PA_DEBUG("Cmd resp: %d", static_cast<int>(error));
                if (callback_) {
                    if(error == ErrorCode::SUCCESS)
                        callback_(PA_OK, context_);
                    else
                        callback_(PA_FAULT, context_);
                }
            }
        protected:
            taf_pa_audio_cb callback_;
            std::any context_;
    };

    AudioPAController() = default;
    ~AudioPAController() = default;
private:
    AudioPAController(const AudioPAController&) = delete;
    AudioPAController& operator=(const AudioPAController&) = delete;

    std::shared_ptr<telux::audio::IAudioManager> audioManager_;
    std::shared_ptr<telux::audio::IAudioVoiceStream> mAudioVoiceStream;
    std::shared_ptr<telux::audio::IAudioPlayStream> mAudioPlaybackStream;
    std::shared_ptr<telux::audio::IAudioCaptureStream> mAudioCaptureStream, mAudioRxCaptureStream;
    std::shared_ptr<telux::audio::IAudioLoopbackStream> mAudioLoopbackStream;
    // mAudioPlayer - local/incall downlink playback, mTxAudioPlayer - incall uplink playback
    std::shared_ptr<telux::audio::IAudioPlayer> mAudioPlayer, mTxAudioPlayer;
    std::shared_ptr<telux::tel::ICallManager> callManager = nullptr;
    // local/incall downlink playback listener
    std::shared_ptr<tafPaPromptsStatusListener> repeatedPlayerStatusListener;
    std::shared_ptr<tafPaAudioCaptureStream> paCaptureStream, paRxCaptureStream;
    // incall uplink playback listener
    std::shared_ptr<tafPaPromptsStatusListener> repeatedTxPlayerStatusListener;
    std::shared_ptr<tafPaDtmfListener> dtmfListener;
    std::shared_ptr<CommandCallback> dtmfCb = nullptr;
    std::shared_ptr<tafPaServiceStatusListener> paServiceStatusChangeListener;
    std::queue<std::shared_ptr<telux::audio::IStreamBuffer>> mRecFreeBuffers, mRxRecFreeBuffers;
    std::mutex subsystemEventsCbksMtx_;
    // The callback entry vector for subsystem events
    std::vector<SubsystemEventsCallbackEntry_t> subsystemEventsCallbacks_;
    uint16_t subsystemEventsCallbackId_ = 1;
    SubsystemState_e audioMngrInitState_ = SubsystemState_e::UNAVAILABLE;

};

pa_result_t AudioPAController::initialize()
{
    auto &audioFactory = telux::audio::AudioFactory::getInstance();
    bool isReady = false;

    auto p = std::make_shared<std::promise<telux::common::ServiceStatus>>();
    audioManager_ = audioFactory.getAudioManager(
            [&p](telux::common::ServiceStatus status) {
        PA_INFO("Getting status: %d from audio manager", static_cast<int>(status));
        try
        {
            // If the status is SERVICE_UNAVAILABLE,
            // the audio manager will also update the status through initCB
            if (status != telux::common::ServiceStatus::SERVICE_UNAVAILABLE)
            {
                p->set_value(status);
            }
        }
        catch (const std::future_error& e)
        {
            PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in callback.");
        }
    });

    if (!audioManager_) {
        PA_CRIT("Can't get IAudioManager");
        return PA_FAULT;
    }

    std::future<telux::common::ServiceStatus> initFuture = p->get_future();
    std::future_status waitStatus = initFuture.wait_for(std::chrono::seconds(5));
    telux::common::ServiceStatus serviceStatus;
    if (std::future_status::timeout == waitStatus)
    {
        PA_CRIT("Timeout waiting for audio subsystem");
        return PA_TIMEOUT;
    } else {
        serviceStatus = initFuture.get();
        if (serviceStatus != telux::common::ServiceStatus::SERVICE_AVAILABLE) {
            PA_CRIT("Unable to initialize audio subsystem");
            return PA_UNAVAILABLE;
        }
        isReady = true;
    }

    if (isReady) {
        PA_INFO("Audio subsystem is ready");
        audioMngrInitState_ = SubsystemState_e::AVAILABLE;
        paServiceStatusChangeListener = std::make_shared<tafPaServiceStatusListener>();
        auto status = audioManager_->registerListener(paServiceStatusChangeListener);
        if (status != telux::common::Status::SUCCESS)
        {
           PA_ERROR("Failed to register with audio subsystem service status change listener");
        }
        return PA_OK;
    } else {
        PA_CRIT("Unable to initialize audio subsystem");
        return PA_UNAVAILABLE;
    }
}

pa_result_t AudioPAController::deinitialize()
{
    PA_INFO("Starting audio PA deinitialization...");

    // Step 1: Deregister the service status change listener from the audio manager
    // before tearing down any state, so no further SDK callbacks arrive.
    if (audioManager_ && paServiceStatusChangeListener)
    {
        PA_INFO("Deregistering paServiceStatusChangeListener from audioManager_");
        telux::common::Status status =
        audioManager_->deRegisterListener(paServiceStatusChangeListener);
        if (status != telux::common::Status::SUCCESS)
        {
            PA_ERROR("Failed to deregister service status listener. Status: %d",
                     static_cast<int>(status));
        }
        paServiceStatusChangeListener.reset();
    }

    // Step 2: Reset all audio stream shared pointers
    PA_INFO("Resetting audio stream shared pointers");
    mAudioVoiceStream.reset();
    mAudioPlaybackStream.reset();
    mAudioCaptureStream.reset();
    mAudioRxCaptureStream.reset();
    mAudioLoopbackStream.reset();

    // Step 3: Reset audio player shared pointers
    PA_INFO("Resetting audio player shared pointers");
    mAudioPlayer.reset();
    mTxAudioPlayer.reset();

    // Step 4: Reset call manager shared pointer
    PA_INFO("Resetting callManager");
    callManager.reset();

    // Step 5: Reset playback listener, capture stream wrapper, and DTMF shared pointers
    PA_INFO("Resetting listener and callback shared pointers");
    repeatedPlayerStatusListener.reset();
    repeatedTxPlayerStatusListener.reset();
    paCaptureStream.reset();
    paRxCaptureStream.reset();
    dtmfListener.reset();
    dtmfCb.reset();

    // Step 6: Drain the IStreamBuffer queues
    PA_INFO("Draining mRecFreeBuffers and mRxRecFreeBuffers queues");
    while (!mRecFreeBuffers.empty())
    {
        mRecFreeBuffers.pop();
    }
    while (!mRxRecFreeBuffers.empty())
    {
        mRxRecFreeBuffers.pop();
    }

    // Step 7: Clear subsystem event callbacks under mutex
    PA_INFO("Clearing subsystemEventsCallbacks_");
    {
        std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
        subsystemEventsCallbacks_.clear();
    }

    // Step 8: Reset audio manager shared pointer (last, after all streams are gone)
    PA_INFO("Resetting audioManager_");
    audioManager_.reset();

    // Step 9: Reset init state so post-deinit guard checks correctly see UNAVAILABLE
    audioMngrInitState_ = SubsystemState_e::UNAVAILABLE;

    PA_INFO("Audio PA deinitialization complete");
    return PA_OK;
}

void AudioPAController::SendSubsystemEventToClients
(
    SubsystemState_e state
)
{
    PA_DEBUG("Calling registered callbacks...");
    std::vector<SubsystemEventsCallbackEntry_t> localCbksCopy;
    {
        // Use exclusive lock to serialize event delivery and prevent race conditions
        // This ensures that all registered callbacks receive events in the correct order
        // even when called from multiple threads simultaneously
        std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
        localCbksCopy = subsystemEventsCallbacks_;
    }
    for (auto &cbk : localCbksCopy)
    {
        try
        {
            PA_DEBUG("Calling callback: %d", cbk.id);
            cbk.callBack(state, cbk.context);
        }
        catch (const std::exception &e)
        {
            PA_ERROR("Exception in callback %d: %s", cbk.id, e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown exception in callback %d", cbk.id);
        }
    }
}

pa_result_t AudioPAController::createStream
(
    PaStreamConfig config,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto status = Status::FAILED;
    auto p = std::make_shared<std::promise<bool>>();
    telux::common::ErrorCode ec;
    std::shared_ptr<telux::audio::IAudioStream> paAudioStream;
    StreamConfig streamConfig = {};
    streamConfig.channelTypeMask = config.channelTypeMask;
    streamConfig.formatParams = nullptr;
    streamConfig.sampleRate = config.sampleRate;
    streamConfig.ecnrMode = config.ecnrMode ? EcnrMode::ENABLE : EcnrMode::DISABLE;
    for(PaAudioIf audioIf : config.deviceTypes)
    {
        if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SINK_1)
            streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SINK_0);
        else if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SINK_2)
            streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SINK_1);
        else if (audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SINK_3)
            streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SINK_2);
        else if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SOURCE_1)
            streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SOURCE_0);
        else if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SOURCE_2)
            streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SOURCE_1);
        else if (audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SOURCE_3)
            streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SOURCE_2);
    }
    for(PaStreamDirection dir : config.streamDir)
    {
        if (dir == PaStreamDirection::RX)
            streamConfig.voicePaths.emplace_back(Direction::RX);
        else if (dir == PaStreamDirection::TX)
            streamConfig.voicePaths.emplace_back(Direction::TX);
    }
    if (config.format == PaFileFormat::WAVE)
    {
        streamConfig.format = AudioFormat::PCM_16BIT_SIGNED;
    }
    else
    {
        PA_DEBUG("Format is not set");
    }
    if (config.type == PaStreamType::VOICE_CALL)
    {
            streamConfig.type = StreamType::VOICE_CALL;
            streamConfig.slotId = static_cast<SlotId>(config.slotId);
    }
    else if (config.type == PaStreamType::CAPTURE)
    {
        streamConfig.type = StreamType::CAPTURE;
    }
    else if (config.type == PaStreamType::LOOPBACK)
    {
        streamConfig.type = StreamType::LOOPBACK;
    }
    Status audioStatus = audioManager_->createStream(streamConfig,
            [&p, &paAudioStream, &callback, &context, this]
            (std::shared_ptr<telux::audio::IAudioStream> &stream, telux::common::ErrorCode error)
    {
        try
        {
            if (error == telux::common::ErrorCode::SUCCESS) {
                paAudioStream = stream;
                callback(PA_OK, context);
                p->set_value(true);
            } else {
                callback(PA_FAULT, context);
                p->set_value(false);
                PA_ERROR("Failed to Create a stream err : %d", error);
            }
        }
        catch (const std::future_error& e)
        {
            PA_ERROR("Future error in callback: %s", e.what());
        }
        catch (const std::exception& e)
        {
            PA_ERROR("Exception in callback: %s", e.what());
        }
        catch (...)
        {
            PA_ERROR("Unknown error in callback.");
        }
    });
    if(audioStatus == Status::SUCCESS) {
        PA_DEBUG("Request to create stream sent" );
    } else {
        PA_ERROR("Request to create stream failed: %d", int(audioStatus));
        return PA_FAULT;
    }

    if (p->get_future().get()) {
        if(paAudioStream->getType() == StreamType::VOICE_CALL) {
            if (streamConfig.slotId == SlotId::SLOT_ID_1) {
                mAudioVoiceStream = std::dynamic_pointer_cast<
                    telux::audio::IAudioVoiceStream>(paAudioStream);
            }
            PA_INFO("Voice Stream is Created on slot id %d ",config.slotId );
        } else if(paAudioStream->getType() == StreamType::CAPTURE) {
            for(Direction dir : streamConfig.voicePaths) {
                if(dir == telux::audio::Direction::RX) {
                    PA_DEBUG("Create stream for incall downlink recording");
                    mAudioRxCaptureStream = std::dynamic_pointer_cast<
                        telux::audio::IAudioCaptureStream>(paAudioStream);
                    return PA_OK;
                }
            }
            mAudioCaptureStream = std::dynamic_pointer_cast<
                    telux::audio::IAudioCaptureStream>(paAudioStream);
            PA_INFO("Audio Capture Stream is Created" );
        } else if(paAudioStream->getType() == StreamType::LOOPBACK) {
            mAudioLoopbackStream = std::dynamic_pointer_cast<
                    telux::audio::IAudioLoopbackStream>(paAudioStream);
            PA_INFO("Audio Loopback Stream is Created" );
        } else {
            PA_ERROR("Unknown Stream Created" );
        }
    }
    else
    {
        PA_ERROR("Failed to create stream");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::deleteStream(PaStreamConfig streamConfig, taf_pa_audio_cb callback,
    std::any context)
{
        telux::common::Status status;
        telux::common::ErrorCode ec;
        CALLBACK_TO_SET_RESULT;
    if((streamConfig.type == PaStreamType::VOICE_CALL) && mAudioVoiceStream)
    {
        status = audioManager_->deleteStream(mAudioVoiceStream, cb);
        if (status == Status::SUCCESS)
        {
            mAudioVoiceStream.reset();
            mAudioVoiceStream = nullptr;
        }
        else{
            PA_ERROR("Failed to delete voice stream");
        }
    }
    else if (streamConfig.type == PaStreamType::CAPTURE)
    {
        for(PaStreamDirection dir : streamConfig.streamDir) {
            if(dir == PaStreamDirection::RX && mAudioRxCaptureStream) {
                PA_DEBUG("Delete stream for incall downlink recording");
                status = audioManager_->deleteStream(mAudioRxCaptureStream, cb);
                if (status == Status::SUCCESS)
                {
                    mAudioRxCaptureStream.reset();
                    mAudioRxCaptureStream = nullptr;
                }
                else{
                    PA_ERROR("Failed to delete remote record stream");
                }
            }
        }
        if ((streamConfig.streamDir.size() == 0) && mAudioCaptureStream)
        {
            status = audioManager_->deleteStream(mAudioCaptureStream, cb);
            if (status == Status::SUCCESS)
            {
                mAudioCaptureStream.reset();
                mAudioCaptureStream = nullptr;
            }
            else{
                PA_ERROR("Failed to delete record stream");
            }
        }
    }
    else if ((streamConfig.type == PaStreamType::LOOPBACK) && mAudioLoopbackStream)
    {
        status = audioManager_->deleteStream(mAudioLoopbackStream, cb);
        if (status == Status::SUCCESS)
        {
            mAudioLoopbackStream.reset();
            mAudioLoopbackStream = nullptr;
        }
        else{
            PA_ERROR("Failed to delete loopback stream");
        }
    }
    if(status != Status::SUCCESS)
    {
        return PA_FAULT;
    }
    PA_DEBUG("Successfully deleted the stream");
    return PA_OK;
}

pa_result_t AudioPAController::startAudio(PaStreamConfig streamconfig, taf_pa_audio_cb callback,
    std::any context)
{
    auto status = Status::FAILED;
    telux::common::ErrorCode ec;

    auto pACtrl = AudioPAController::getInstance();
    CALLBACK_TO_SET_RESULT;

    if(streamconfig.type == PaStreamType::VOICE_CALL) {
        if(mAudioVoiceStream) {
            status = mAudioVoiceStream->startAudio(cb);
            if (status != telux::common::Status::SUCCESS) {
                PA_ERROR("Request to start voice call audio failed.\n");
                return PA_FAULT;
            }
        }
    }
    else if(streamconfig.type == PaStreamType::LOOPBACK) {
        status = mAudioLoopbackStream->startLoopback(cb);
        if (status != telux::common::Status::SUCCESS) {
            PA_ERROR("Request to start loopback failed.\n");
            return PA_FAULT;
        }
    }
    PA_DEBUG("Successfully started audio in PA");
    return PA_OK;
}

pa_result_t AudioPAController::stopAudio(PaStreamConfig streamconfig, taf_pa_audio_cb callback,
    std::any context)
{
    auto pACtrl = AudioPAController::getInstance();
    auto status = Status::FAILED;
    CALLBACK_TO_SET_RESULT;
   PA_DEBUG("stopAudio");
    if(streamconfig.type == PaStreamType::VOICE_CALL) {
        if(mAudioVoiceStream) {
            status = mAudioVoiceStream->stopAudio(cb);
            if (status != telux::common::Status::SUCCESS) {
                PA_ERROR("Request to stop voice call audio failed.");
                return PA_FAULT;
            }
        }
    }
    else if(streamconfig.type == PaStreamType::LOOPBACK) {
        status = mAudioLoopbackStream->stopLoopback(cb);
        if (status != telux::common::Status::SUCCESS) {
            PA_ERROR("Request to stop loopback failed.");
            return PA_FAULT;
        }
    }
    PA_DEBUG("Successfully stopped audio in PA");
    return PA_OK;
}

pa_result_t AudioPAController::setVolume(
    PaStreamConfig streamConfig,
    double volLevel,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto status = Status::FAILED;
    CALLBACK_TO_SET_RESULT;
    telux::audio::StreamVolume streamVol;
    ChannelVolume leftChannelVol, rightChannelVol;
    leftChannelVol.vol = volLevel;
    rightChannelVol.vol = volLevel;

    leftChannelVol.channelType = ChannelType::LEFT;
    rightChannelVol.channelType = ChannelType::RIGHT;
    streamVol.volume.emplace_back(leftChannelVol);
    streamVol.volume.emplace_back(rightChannelVol);

    if (streamConfig.type == PaStreamType::PLAY)
    {
         ErrorCode err = mAudioPlayer->setVolume(volLevel);
        if (ErrorCode::SUCCESS != err) {
            PA_ERROR("Request to set volume failed err: %d", int (err));
            callback(PA_FAULT, context);
            return PA_FAULT;
        }
        callback(PA_OK, context);
        return PA_OK;
    }
    else if (streamConfig.type == PaStreamType::CAPTURE)
    {
        streamVol.dir = StreamDirection::TX;
        status = mAudioCaptureStream->setVolume(streamVol, cb);
    }
    else if (streamConfig.type == PaStreamType::VOICE_CALL)
    {
        streamVol.dir = StreamDirection::RX;
        status = mAudioVoiceStream->setVolume(streamVol, cb);
    }
    else
    {
        PA_ERROR("Invalid audio interface");
        return PA_BAD_PARAMETER;
    }
    if(status == telux::common::Status::SUCCESS) {
        PA_INFO("Request to set volume sent");
    } else {
        PA_ERROR("Request to set volume failed");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::getVolume(
    PaStreamConfig streamConfig,
    double *volLevel,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto status = Status::FAILED;
    auto cb = [callback, context, volLevel](StreamVolume streamVol, ErrorCode error) {
            if (ErrorCode::SUCCESS == error) {
                PA_DEBUG("Request is successful !!");
                for (auto channelVolume : streamVol.volume) {
                    PA_INFO("vol is %f", channelVolume.vol);
                    *volLevel  = channelVolume.vol;
                }
                callback(PA_OK, context);
            }
            else{
                PA_ERROR("Failed to start dtmf tone, err %d", (int)error);
                callback(PA_FAULT, context);
            }
    };
    if (streamConfig.type == PaStreamType::PLAY)
    {
        float volume;
        ErrorCode err = mAudioPlayer->getVolume(volume);
        if (ErrorCode::SUCCESS != err) {
            PA_ERROR("Request to get volume failed err: %d", int (err));
            callback(PA_FAULT, context);
            return PA_FAULT;
        } else {
            PA_INFO("vol is %f", volume);
            *volLevel  = volume;
            callback(PA_OK, context);
            return PA_OK;
        }
    }
    else if (streamConfig.type == PaStreamType::CAPTURE)
    {
        StreamDirection dir = StreamDirection::TX;;
        status = mAudioCaptureStream->getVolume(dir, cb);
    }
    else if (streamConfig.type == PaStreamType::VOICE_CALL)
    {
        status = mAudioVoiceStream->getVolume(StreamDirection::RX, cb);
    }
    else
    {
        PA_ERROR("Invalid audio interface");
        return PA_BAD_PARAMETER;
    }
    if(status == telux::common::Status::SUCCESS) {
        PA_INFO("Request to get volume sent");
    } else {
        PA_ERROR("Request to get volume failed");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::setMute(
    PaStreamConfig streamConfig,
    bool isMute,
    taf_pa_audio_cb callback,
    std::any context
) {
    auto status = Status::FAILED;
    StreamMute muteObj = {};
    muteObj.enable = isMute;
    CALLBACK_TO_SET_RESULT;
    if (streamConfig.type == PaStreamType::PLAY)
    {
        ErrorCode err = mAudioPlayer->setMute(isMute);
        if (ErrorCode::SUCCESS != err) {
            PA_ERROR("Request to set mute failed err: %d", int (err));
            return PA_FAULT;
        }
        callback(PA_OK, context);
        return PA_OK;
    }
    else if (streamConfig.type == PaStreamType::CAPTURE)
    {
        muteObj.dir = StreamDirection::TX;
        status = mAudioCaptureStream->setMute(muteObj, cb);
    }
    else if (streamConfig.type == PaStreamType::VOICE_CALL)
    {
        for(PaStreamDirection dir : streamConfig.streamDir){
            if (dir == PaStreamDirection::RX)
            {
        muteObj.dir = StreamDirection::RX;
        status = mAudioVoiceStream->setMute(muteObj, cb);
            }
            else if (dir == PaStreamDirection::TX)
            {
        muteObj.dir = StreamDirection::TX;
        status = mAudioVoiceStream->setMute(muteObj, cb);
            }
        }
    }
    else
    {
        PA_ERROR("Invalid audio interface");
        return PA_BAD_PARAMETER;
    }
    if(status == telux::common::Status::SUCCESS) {
        PA_INFO("Request to set mute sent");
    } else {
        PA_ERROR("Request to set mute failed");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::getMute(
    PaStreamConfig streamConfig,
    bool *isMute,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto status = Status::FAILED;
    auto p = std::make_shared<std::promise<bool>>();
    StreamMute muteObj = {};
    auto cb = [callback, context, isMute, this](StreamMute mute, ErrorCode error)
    {
            if (error == ErrorCode::SUCCESS) {
                *isMute = mute.enable;
                callback(PA_OK, context);
            } else {
                PA_ERROR("Failed to get mute status, error : %d", (int)error);
                callback(PA_FAULT, context);
            }

    };

    if (streamConfig.type == PaStreamType::PLAY)
    {
        ErrorCode err = mAudioPlayer->getMute(*isMute);
        if (ErrorCode::SUCCESS != err) {
            PA_ERROR("Request to get Mute status failed err: %d", int (err));
            callback(PA_FAULT, context);
            return PA_FAULT;
        } else {
            PA_INFO("Mute status for playback is %s", isMute ? "true" : "false");
            callback(PA_OK, context);
            return PA_OK;
        }
    }
    else if (streamConfig.type == PaStreamType::CAPTURE)
    {
        status = mAudioCaptureStream->getMute(StreamDirection::TX, cb);
    }
    else if (streamConfig.type == PaStreamType::VOICE_CALL)
    {
        for(PaStreamDirection dir : streamConfig.streamDir){
            if (dir == PaStreamDirection::RX)
            {
                muteObj.dir = StreamDirection::RX;
                status = mAudioVoiceStream->getMute(StreamDirection::RX, cb);
            }
            else if (dir == PaStreamDirection::TX)
            {
                muteObj.dir = StreamDirection::TX;
                status = mAudioVoiceStream->getMute(StreamDirection::TX, cb);
            }
        }
    }
    else
    {
        PA_ERROR("Invalid audio interface");
        return PA_BAD_PARAMETER;
    }
    if(status == telux::common::Status::SUCCESS) {
        PA_INFO("Request to get mute sent");
    } else {
        PA_ERROR("Request to get mute failed");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::startPlayback(std::vector<taf_pa_audio_PlayFileInfo_t> &playFileInfos, int listSize, std::weak_ptr<IPaPlayListListener> pbListener)
{
    PA_DEBUG("startPlayback");
    telux::audio::PlaybackConfig pbConfig = {};
    telux::common::ErrorCode ec = telux::common::ErrorCode::INTERNAL_ERROR;
    std::vector<telux::audio::PlaybackConfig> filesToPlay;
    for(taf_pa_audio_PlayFileInfo_t fileInfo : playFileInfos)
    {
        pbConfig = {};
        pbConfig.absoluteFilePath = fileInfo.absoluteFilePath.c_str();
        PA_INFO("absoluteFilePath path is %s", pbConfig.absoluteFilePath.c_str());
        pbConfig.streamConfig.type = StreamType::PLAY;
        pbConfig.streamConfig.sampleRate = fileInfo.streamConfig.sampleRate;
        pbConfig.streamConfig.channelTypeMask = fileInfo.streamConfig.channelTypeMask;
        if(fileInfo.repeat == -1){
            pbConfig.repeatInfo.type = telux::audio::RepeatType::INDEFINITELY;
        } else{
            pbConfig.repeatInfo.type = telux::audio::RepeatType::COUNT;
            pbConfig.repeatInfo.count = fileInfo.repeat + 1;
        }
        if (fileInfo.streamConfig.format == PaFileFormat::WAVE)
            pbConfig.streamConfig.format = AudioFormat::PCM_16BIT_SIGNED;
        else if (fileInfo.streamConfig.format == PaFileFormat::AMR_NB)
            pbConfig.streamConfig.format = AudioFormat::AMRNB;
        else if (fileInfo.streamConfig.format == PaFileFormat::AMR_WB)
            pbConfig.streamConfig.format = AudioFormat::AMRWB;
        PA_DEBUG("samplerate is %d channelmask is %d count is %d format is %d",
            pbConfig.streamConfig.sampleRate, pbConfig.streamConfig.channelTypeMask,
            pbConfig.repeatInfo.count, pbConfig.streamConfig.format);
        for (PaAudioIf audioIf : fileInfo.streamConfig.deviceTypes)
        {
            if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SINK_1)
                pbConfig.streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SINK_0);
            else if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SINK_2)
                pbConfig.streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SINK_1);
            else if(audioIf == PaAudioIf::TAF_PA_AUDIO_IF_CODEC_SINK_3)
                pbConfig.streamConfig.deviceTypes.emplace_back((DeviceType)DEVICE_TYPE_SINK_2);
        }
        for(PaStreamDirection dir : fileInfo.streamConfig.streamDir)
        {
            if(dir == PaStreamDirection::TX)
                pbConfig.streamConfig.voicePaths.emplace_back(telux::audio::Direction::TX);
        }

        PA_DEBUG("devices sizes %d, voicePaths sizes %d", pbConfig.streamConfig.deviceTypes.size(),
                pbConfig.streamConfig.voicePaths.size());
        filesToPlay.push_back(pbConfig);
    }
    PA_DEBUG("playfile sizes %d", filesToPlay.size());
    ec = telux::common::ErrorCode::INTERNAL_ERROR;
    if(pbConfig.streamConfig.voicePaths.size() > 0)
    {
        PA_DEBUG("Create stream for incall uplink playback and start playback");
        ec = AudioFactory::getInstance().getAudioPlayer(mTxAudioPlayer);
        if (ec != telux::common::ErrorCode::SUCCESS) {
            PA_ERROR("can't get IAudioPlayer");
            return PA_FAULT;
        }
        repeatedTxPlayerStatusListener = std::make_shared<tafPaPromptsStatusListener>();
        repeatedTxPlayerStatusListener->pbStatusListener = pbListener;
        ec = mTxAudioPlayer->startPlayback(filesToPlay, repeatedTxPlayerStatusListener);
        if (ec != telux::common::ErrorCode::SUCCESS) {
            PA_ERROR("failed start, err %d", static_cast<int>(ec));
            return PA_FAULT;
        }
    }
    else
    {
        PA_DEBUG("Create stream for local playback and start playback");
        ec = AudioFactory::getInstance().getAudioPlayer(mAudioPlayer);
        if (ec != telux::common::ErrorCode::SUCCESS) {
            PA_ERROR("can't get IAudioPlayer");
            return PA_FAULT;
        }
        repeatedPlayerStatusListener = std::make_shared<tafPaPromptsStatusListener>();
        repeatedPlayerStatusListener->pbStatusListener = pbListener;
        ec = mAudioPlayer->startPlayback(filesToPlay, repeatedPlayerStatusListener);
        if (ec != telux::common::ErrorCode::SUCCESS) {
            PA_ERROR("failed start, err %d", static_cast<int>(ec));
            return PA_FAULT;
        }
    }
    return PA_OK;
}

pa_result_t AudioPAController::stopPlayback(PaStreamConfig streamConfig)
{
    telux::common::ErrorCode ec = ErrorCode::INTERNAL_ERROR;
    if(streamConfig.streamDir.size() > 0)
    {
        PA_INFO("Stop remote playback");
        if (mTxAudioPlayer)
            ec = mTxAudioPlayer->stopPlayback();
    }
    else
    {
        PA_INFO("Stop local playback");
        if (mAudioPlayer)
            ec = mAudioPlayer->stopPlayback();
    }
    if (ec != telux::common::ErrorCode::SUCCESS) {
        if (ec == telux::common::ErrorCode::INVALID_STATE) {
            PA_ERROR("no playback in progress");
            return PA_FAULT;
        }
        PA_ERROR("failed stoping playback, err %d",static_cast<int>(ec));
        return PA_FAULT;
    }
    if(streamConfig.streamDir.size() > 0)
    {
        mTxAudioPlayer.reset();
        mTxAudioPlayer = nullptr;
    }
    else
    {
        mAudioPlayer.reset();
        mAudioPlayer = nullptr;
    }
    return PA_OK;
}

std::shared_ptr<PaAudioCaptureStream> AudioPAController::GetCaptureStream(
    tafpa::audio::PaStreamDirection streamDir
)
{
    PA_DEBUG("GetCaptureStream");
    if (streamDir == PaStreamDirection::TX)
    {
        if(mAudioCaptureStream)
        {
            paCaptureStream = std::make_shared<tafPaAudioCaptureStream>(mAudioCaptureStream);
            return paCaptureStream;
        }
    }
    else if (streamDir == PaStreamDirection::RX)
    {
       if(mAudioRxCaptureStream)
        {
            paRxCaptureStream = std::make_shared<tafPaAudioCaptureStream>(mAudioRxCaptureStream);
            return paRxCaptureStream;
        }
    }
    return NULL;
}

pa_result_t AudioPAController::playSignallingDtmfOnTx( uint32_t slotId, const char dtmf,
        taf_pa_audio_cb callback, std::any context )
{
    PA_INFO("Playing signaling DTMF for slot %d: '%c'", slotId, dtmf);
    if (!dtmf) {
        PA_ERROR("Invalid DTMF string");
        return PA_BAD_PARAMETER;
    }
    std::promise<ServiceStatus> callMgrprom;
    std::shared_ptr<telux::tel::ICall> spCall = nullptr;
    std::vector<std::shared_ptr<telux::tel::ICall>> inProgressCalls;

    if(!callManager) {
        auto &phoneFactory = telux::tel::PhoneFactory::getInstance();

        //  Get the PhoneFactory and CallManager instances.
        callManager = phoneFactory.getCallManager([&](ServiceStatus status) {
            callMgrprom.set_value(status);
        });
        if(!callManager) {
            PA_ERROR(" Failed to get CallManager instance");
            callback(PA_FAULT, context);
            return PA_FAULT;
        }
        PA_DEBUG("CallManager subsystem is not ready, Please wait ");

        ServiceStatus callMgrsubSystemStatus = callMgrprom.get_future().get();
        if(callMgrsubSystemStatus == ServiceStatus::SERVICE_AVAILABLE) {
            PA_DEBUG("CallManager subsystem is ready ");
        } else {
            PA_ERROR("Unable to initialise CallManager subsystem ");
            callback(PA_FAULT, context);
            return PA_FAULT;
        }
    }

    inProgressCalls = callManager->getInProgressCalls();

    // Fetch the list of in progress calls from CallManager and if there is atleast one in
    // progress calls on user provided slot, send DTMF request
    for(auto callIterator = std::begin(inProgressCalls);
        callIterator != std::end(inProgressCalls); ++callIterator) {
        if ((*callIterator)->getPhoneId() == slotId) {
            spCall = *callIterator;
            break;
        }
    }

    if(!spCall)
    {
        PA_ERROR("No progressing calls");
        return PA_UNSUPPORTED;
    }

    dtmfCb = std::make_shared<AudioPAController::CommandCallback>(
            callback, context);
    auto ret = spCall->startDtmfTone(dtmf, dtmfCb);
    if (ret != telux::common::Status::SUCCESS) {
        PA_ERROR("Play tone request failed, err %d", (int)ret);
        return PA_FAULT;
    }
    PA_INFO("Signaling DTMF request successfully");
    return PA_OK;
}

pa_result_t AudioPAController::stopSignallingDtmfOnTx( uint32_t slotId, taf_pa_audio_cb callback, std::any context )
{
    PA_INFO("Stopping signaling DTMF for slot %d", slotId);

    auto status = Status::FAILED;
    std::shared_ptr<telux::tel::ICall> spCall = nullptr;
    std::vector<std::shared_ptr<telux::tel::ICall>> inProgressCalls;

    inProgressCalls = callManager->getInProgressCalls();
    // Fetch the list of in progress calls from CallManager and if there is atleast one
    // in progress calls on user provided slot, send DTMF request
    for(auto callIterator = std::begin(inProgressCalls);
        callIterator != std::end(inProgressCalls); ++callIterator) {
        if ((*callIterator)->getPhoneId() == (int)slotId) {
            spCall = *callIterator;
            break;
        }
    }
    if(!spCall) {
        PA_ERROR("No call found on slot Id %d", slotId);
        return PA_UNSUPPORTED;
    }
    dtmfCb = std::make_shared<AudioPAController::CommandCallback>(
            callback, context);
    status = spCall->stopDtmfTone(dtmfCb);
    if(status != Status::SUCCESS) {
        PA_ERROR("Request to stop Dtmf Tone failed");
        return PA_FAULT;
    }

    PA_INFO("Signaling DTMF stopped request successfully");
    return PA_OK;
}

pa_result_t AudioPAController::playDtmf( PaDtmfTone paDtmfTone, uint16_t duration,
        uint16_t gain,
        taf_pa_audio_cb callback,
        std::any context )
{
    auto status = Status::FAILED;
    DtmfTone dtmfTone = {};
    dtmfTone.direction = static_cast<telux::audio::StreamDirection>(paDtmfTone.direction);
    dtmfTone.lowFreq = static_cast<telux::audio::DtmfLowFreq>(paDtmfTone.lowFreq);
    dtmfTone.highFreq = static_cast<telux::audio::DtmfHighFreq>(paDtmfTone.highFreq);
    PA_DEBUG("Playing frequencies low: %d, high: %d\n", dtmfTone.lowFreq,
    dtmfTone.highFreq);
    CALLBACK_TO_SET_RESULT;
    status = mAudioVoiceStream->playDtmfTone( dtmfTone, duration, gain, cb);
    if (status != Status::SUCCESS)
    {
        PA_ERROR("Failed to send request to start dtmf tone, err : %d", (int)status);
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::stopDtmf(PaStreamDirection direction, taf_pa_audio_cb callback,
    std::any context)
{
    CALLBACK_TO_SET_RESULT;
    auto status = Status::FAILED;
    status = mAudioVoiceStream->stopDtmfTone(static_cast<StreamDirection>(direction), cb);
    if(status == Status::SUCCESS) {
        PA_DEBUG("Stop Dtmf Tone requedt sent successfully");
    }else {
        PA_ERROR("Request to stop Dtmf Tone failed");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::registerDtmfListener(
    std::weak_ptr<IPaDtmfListener> listener
)
{
    if(mAudioVoiceStream) {
        dtmfListener = std::make_shared<tafPaDtmfListener>();
        dtmfListener->paDtmfListener = listener;
        telux::common::Status st = mAudioVoiceStream->registerListener(dtmfListener);
        if(st!=telux::common::Status::SUCCESS) {
            PA_ERROR("Request to register for DTMF detection failed error : %d", (int)st);
            return PA_FAULT;
        }
        PA_DEBUG("Request to register dtmf listener successful" );
    }
    else{
        PA_ERROR("Voice stream is not active!");
        return PA_FAULT;
    }
    return PA_OK;
}

pa_result_t AudioPAController::deregisterDtmfListener(
    std::weak_ptr<IPaDtmfListener> listener
)
{
    if(mAudioVoiceStream) {
        telux::common::Status st = mAudioVoiceStream->deRegisterListener(dtmfListener);
        if(st!=telux::common::Status::SUCCESS) {
            PA_ERROR("Request to register for DTMF detection failed error : %d", (int)st);
            return PA_FAULT;
        }
        PA_DEBUG("Request to deregister dtmf listener successful" );
    }
    else{
        PA_ERROR("Voice stream is not active!");
        return  PA_FAULT;
    }
    dtmfListener.reset();
    dtmfListener = nullptr;
    return PA_OK;
}

pa_result_t AudioPAController::registerAudioSubsystemChangeListener
(
    taf_pa_audio_SubsystemStateChangeCb callBack,
    std::shared_ptr<void> context,
    uint16_t &id
)
{
    TAF_PA_ERROR_IF_RET_VAL(nullptr == callBack, PA_BAD_PARAMETER, "callBack is NULL!");

    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != audioMngrInitState_, PA_FAULT,
                                                              "PA audio manager not initialized.");
    // Lock
    std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
    // Add the callback
    SubsystemEventsCallbackEntry_t entry = {
        subsystemEventsCallbackId_,
        callBack,
        context,
    };
    subsystemEventsCallbacks_.push_back(entry);
    // Give ID back to app
    id = subsystemEventsCallbackId_;
    // Increment the ID.
    subsystemEventsCallbackId_++;

    PA_INFO("Id: %d, Cbk: %p, Ctx: %p", entry.id, entry.callBack, entry.context.get());
    PA_INFO("Number of registered callbacks: %zu", subsystemEventsCallbacks_.size());

    return PA_OK;
}

pa_result_t AudioPAController::deregisterAudioSubsystemChangeListener
(
    uint16_t id
)
{
    TAF_PA_ERROR_IF_RET_VAL(SubsystemState_e::AVAILABLE != audioMngrInitState_, PA_FAULT,
                                                              "PA audio manager not initialized.");
    // Lock
    std::lock_guard<std::mutex> lock(subsystemEventsCbksMtx_);
    // Iterate over the vector and remove the one with the provided id.
    for (auto cbk = subsystemEventsCallbacks_.begin();cbk != subsystemEventsCallbacks_.end(); ++cbk)
    {
        if (cbk->id == id)
        {
            PA_INFO("Id: %d, Cbk: %p", id, cbk);
            subsystemEventsCallbacks_.erase(cbk);
            return PA_OK;
        }
    }
    PA_WARN("Callback not found. Id: %d", id);
    return PA_NOT_FOUND;
}

pa_result_t tafpa::audio::taf_pa_audio_Init()
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->initialize();
    if (result == PA_OK)
    {
        PA_INFO("Audio platform adapter initialization is done");
        // Mark initialization as complete
        gAudioPaInitialized.store(true, std::memory_order_release);
        PA_INFO("Audio PA initialization flag set to true.");
    }
    else
    {
        PA_CRIT("Failed to initialize audio platform adapter, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_Deinit()
{
    // Check if Init() was called before Deinit()
    if (!gAudioPaInitialized.load(std::memory_order_acquire))
    {
        PA_WARN("Deinit() called before Init() - ignoring deinit request.");
        return PA_FAULT;
    }

    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->deinitialize();
    if (result == PA_OK)
    {
        PA_INFO("Audio platform adapter deinitialization is done");
        // Reset initialization flag
        gAudioPaInitialized.store(false, std::memory_order_release);
        PA_INFO("Audio PA initialization flag reset to false.");
    }
    else
    {
        PA_ERROR("Failed to deinitialize audio platform adapter, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_CreateStream(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->createStream(streamConfig, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA create stream request is successful");
    }
    else
    {
        PA_ERROR("Failed to create stream in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_DeleteStream(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->deleteStream(streamConfig, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA delete stream request is successful");
    }
    else
    {
        PA_ERROR("Failed to delete stream in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_StartAudio(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->startAudio(streamConfig, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA start stream request is successful");
    }
    else
    {
        PA_ERROR("Failed to start stream in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_StopAudio(
    PaStreamConfig streamConfig,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->stopAudio(streamConfig, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA stop stream request is successful");
    }
    else
    {
        PA_ERROR("Failed to stop stream in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_StartPlayback(
    std::vector<taf_pa_audio_PlayFileInfo_t> &playFileInfos,
    int listSiz,
    std::weak_ptr<IPaPlayListListener> pbStatusListener)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->startPlayback(playFileInfos, listSiz, pbStatusListener);
    if (result == PA_OK)
    {
        PA_INFO("Successfully sent the request to start the playback");
    }
    else
    {
        PA_ERROR("Failed to start the playback, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_StopPlayback(
    PaStreamConfig streamConfig
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->stopPlayback(streamConfig);
    if (result == PA_OK)
    {
        PA_INFO("Successfully sent the request to stop the playback");
    }
    else
    {
        PA_ERROR("Failed to stop the playback, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_SetVolume(
    PaStreamConfig streamConfig,
    double volLevel,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->setVolume(streamConfig, volLevel, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA setVolume request is successful");
    }
    else
    {
        PA_ERROR("Failed to setVolume in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_GetVolume(
    PaStreamConfig streamConfig,
    double *volLevel,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->getVolume(streamConfig, volLevel, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA getVolume request is successful");
    }
    else
    {
        PA_ERROR("Failed to getVolume in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_SetMute(
    PaStreamConfig streamConfig,
    bool isMute,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->setMute(streamConfig, isMute, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA setMute request is successful");
    }
    else
    {
        PA_ERROR("Failed to setMute in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_GetMute(
    PaStreamConfig streamConfig,
    bool *isMute,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->getMute(streamConfig, isMute, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Audio PA getMute request is successful");
    }
    else
    {
        PA_ERROR("Failed to getMute in audio PA, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_PlayDtmf(PaDtmfTone dtmfTone, uint16_t duration,
        uint16_t gain,
        taf_pa_audio_cb callback,
        std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->playDtmf(dtmfTone, duration, gain, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Successfully sent the request to start dtmf tone");
    }
    else
    {
        PA_ERROR("Failed to send request to start the dtmf tone, ret: %d", result);
    }
    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_StopDtmf(
    PaStreamDirection direction, taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    pa_result_t result = pACtrl->stopDtmf(direction, callback, context);
    if (result == PA_OK)
    {
        PA_INFO("Successfully sent the request to stop the dtmf tone");
    }
    else
    {
        PA_ERROR("Failed to stop the dtmf tone, ret: %d", result);
    }

    return result;
}

pa_result_t tafpa::audio::taf_pa_audio_GetCaptureStream(
    tafpa::audio::PaStreamDirection streamDir,
    std::shared_ptr<PaAudioCaptureStream>& captureStream
)
{
    auto pACtrl = AudioPAController::getInstance();

    captureStream = pACtrl->GetCaptureStream(streamDir);
    return PA_OK;
}

pa_result_t tafpa::audio::taf_pa_audio_registerDtmfListener(
    std::weak_ptr<IPaDtmfListener> dtmfListener
)
{
    auto pACtrl = AudioPAController::getInstance();

    return pACtrl->registerDtmfListener(dtmfListener);
}

pa_result_t tafpa::audio::taf_pa_audio_deregisterDtmfListener(
    std::weak_ptr<IPaDtmfListener> dtmfListener
)
{
    auto pACtrl = AudioPAController::getInstance();

    return pACtrl->deregisterDtmfListener(dtmfListener);
}

pa_result_t tafpa::audio::taf_pa_audio_PlaySignallingDtmfOnTx(
    uint32_t slotId,
    const char dtmf,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    return pACtrl->playSignallingDtmfOnTx(slotId, dtmf, callback, context);
}

pa_result_t tafpa::audio::taf_pa_audio_StopSignallingDtmfOnTx(
    uint32_t slotId,
    taf_pa_audio_cb callback,
    std::any context
)
{
    auto pACtrl = AudioPAController::getInstance();

    return pACtrl->stopSignallingDtmfOnTx(slotId, callback, context);
}

pa_result_t tafpa::audio::AddSubsystemStateChangeListener
(
    taf_pa_audio_SubsystemStateChangeCb callBack,
    std::shared_ptr<void> context,
    uint16_t &id
)
{
    auto pACtrl = AudioPAController::getInstance();

    return pACtrl->registerAudioSubsystemChangeListener(callBack, context, id);
}

pa_result_t tafpa::audio::RemoveSubsystemStateChangeListener
(
    uint16_t id
)
{
    auto pACtrl = AudioPAController::getInstance();

    return pACtrl->deregisterAudioSubsystemChangeListener(id);
}
