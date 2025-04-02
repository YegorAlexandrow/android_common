#pragma once

#include <vector>
#include <memory>
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "api/audio/echo_canceller3_factory.h"

class AECProcessor {
public:
    explicit AECProcessor() :
            render_sample_rate_(24000),
//            render_sample_rate_(16000),
            sample_rate_(16000),
            // StreamConfig used only for sample rate and channels (webrtc interfaces)
            // assume implicit resample 24->16 in render_audio_
            render_config_(render_sample_rate_, 1),
            capture_config_(sample_rate_, 1)
    {
        webrtc::EchoCanceller3Config aec_config;
        webrtc::EchoCanceller3Factory aec_factory(aec_config);

        echo_control_ = aec_factory.Create(sample_rate_, 1, 1);
        hp_filter_ = std::make_unique<webrtc::HighPassFilter>(sample_rate_, 1);

        render_audio_ = std::make_unique<webrtc::AudioBuffer>(
                render_config_.sample_rate_hz(), 1, // assume implicit resample 24->16
                capture_config_.sample_rate_hz(), 1,
                capture_config_.sample_rate_hz(), 1);

        capture_audio_ = std::make_unique<webrtc::AudioBuffer>(
                capture_config_.sample_rate_hz(), 1,
                capture_config_.sample_rate_hz(), 1,
                capture_config_.sample_rate_hz(), 1);
    }

    void processFrame(const int16_t *render_frame,
                      const int16_t *capture_frame,
                      int16_t *output_frame) {
        // assume implicit resample 24->16
        render_audio_->CopyFrom(render_frame, render_config_);
        // assume capturing in 16khz
        capture_audio_->CopyFrom(capture_frame, capture_config_);
        hp_filter_->Process(capture_audio_.get(), true);
        echo_control_->AnalyzeCapture(capture_audio_.get());
        echo_control_->AnalyzeRender(render_audio_.get());
        echo_control_->ProcessCapture(capture_audio_.get(), false);
        capture_audio_->CopyTo(capture_config_, output_frame);
    }

private:
    const int render_sample_rate_;
    const int sample_rate_;
    const webrtc::StreamConfig render_config_;
    const webrtc::StreamConfig capture_config_;
    std::unique_ptr<webrtc::EchoControl> echo_control_;
    std::unique_ptr<webrtc::HighPassFilter> hp_filter_;
    std::unique_ptr<webrtc::AudioBuffer> render_audio_;
    std::unique_ptr<webrtc::AudioBuffer> capture_audio_;
};