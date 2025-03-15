#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "api/audio/echo_canceller3_factory.h"

class AECProcessor {
public:
    explicit AECProcessor(int sample_rate)
            : sample_rate_(sample_rate),
              samples_per_frame_(sample_rate / 100),  // 10ms frames
              config_(sample_rate_, 1)  // StreamConfig with sample rate and channels
    {
        webrtc::EchoCanceller3Config aec_config;
        webrtc::EchoCanceller3Factory aec_factory(aec_config);

        echo_control_ = aec_factory.Create(sample_rate_, 1, 1);
        hp_filter_ = std::make_unique<webrtc::HighPassFilter>(sample_rate_, 1);

        // Initialize audio buffers with proper configuration
        render_audio_ = std::make_unique<webrtc::AudioBuffer>(
                config_.sample_rate_hz(), config_.num_channels(),
                config_.sample_rate_hz(), config_.num_channels(),
                config_.sample_rate_hz(), config_.num_channels());

        capture_audio_ = std::make_unique<webrtc::AudioBuffer>(
                config_.sample_rate_hz(), config_.num_channels(),
                config_.sample_rate_hz(), config_.num_channels(),
                config_.sample_rate_hz(), config_.num_channels());
    }


    void ProcessAudioFrames(const std::vector<int16_t> &render_data,
                            const std::vector<int16_t> &capture_data,
                            std::vector<int16_t> &output_data,
                            int stream_delay_ms = 0) {
        const size_t total_frames = capture_data.size() / samples_per_frame_;
        output_data.resize(capture_data.size());

        for (size_t frame_idx = 0; frame_idx < total_frames; ++frame_idx) {
            const size_t offset = frame_idx * samples_per_frame_;
            myLog("frame_idx: %i", frame_idx);
            render_audio_->CopyFrom(&render_data[offset], config_);
            capture_audio_->CopyFrom(&capture_data[offset], config_);
            hp_filter_->Process(capture_audio_.get(), true);
            echo_control_->SetAudioBufferDelay(stream_delay_ms);
            echo_control_->AnalyzeCapture(capture_audio_.get());
            echo_control_->AnalyzeRender(render_audio_.get());
            echo_control_->ProcessCapture(capture_audio_.get(), false);
            capture_audio_->CopyTo(config_, &output_data[offset]);
        }
    }

private:

    const int sample_rate_;
    const size_t samples_per_frame_;
    const webrtc::StreamConfig config_;
    std::unique_ptr<webrtc::EchoControl> echo_control_;
    std::unique_ptr<webrtc::HighPassFilter> hp_filter_;
    std::unique_ptr<webrtc::AudioBuffer> render_audio_;
    std::unique_ptr<webrtc::AudioBuffer> capture_audio_;
};