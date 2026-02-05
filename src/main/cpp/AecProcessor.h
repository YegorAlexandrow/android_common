#pragma once

#include <vector>
#include <memory>
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "api/audio/echo_canceller3_factory.h"
#include "modules/audio_processing/gain_controller2.h"

class AECProcessor {
public:
    AECProcessor(int reference_sample_rate,
                 int input_sample_rate,
                 int processing_sample_rate,
                 int output_sample_rate) :
            reference_sample_rate_(reference_sample_rate),
            input_sample_rate_(input_sample_rate),
            processing_sample_rate_(processing_sample_rate),
            output_sample_rate_(output_sample_rate),
            reference_frame_size_(getFrameSize(reference_sample_rate_)),
            input_frame_size_(getFrameSize(input_sample_rate_)),
            processing_frame_size_(getFrameSize(processing_sample_rate_)),
            output_frame_size_(getFrameSize(output_sample_rate_)),
            reference_config_(reference_sample_rate_, 1),
            input_config_(input_sample_rate_, 1),
            processing_config_(processing_sample_rate_, 1),
            output_config_(output_sample_rate_, 1) {
        webrtc::EchoCanceller3Config aec_config;
        webrtc::EchoCanceller3Factory aec_factory(aec_config);

        echo_control_ = aec_factory.Create(processing_sample_rate_, 1, 1);

        gain_controller2 = std::make_unique<webrtc::GainController2>();
        using gc_config = webrtc::AudioProcessing::Config::GainController2;
        gc_config config;
        config.enabled = true;
        // fixed gain
        config.fixed_digital.gain_db = 22.0f; // todo? pass here preferred mic gain?
        // dynamic gain
        config.adaptive_digital.enabled = true;
        config.adaptive_digital.noise_estimator = gc_config::kNoiseFloor;  // for variable noise
        config.adaptive_digital.vad_reset_period_ms = 1500;                // speech timeout
        config.adaptive_digital.adjacent_speech_frames_threshold = 12;     // speech detection
        config.adaptive_digital.max_gain_change_db_per_second = 30.0f;      // smooth transitions
        config.adaptive_digital.max_output_noise_level_dbfs = -40.0f;      // limits noise boosting
        auto res = webrtc::GainController2::Validate(config);
        myLog("agc validate res: %d", res);

        gain_controller2->ApplyConfig(config);
        gain_controller2->Initialize(processing_sample_rate_);

        hp_filter_ = std::make_unique<webrtc::HighPassFilter>(processing_sample_rate_, 1);

        render_audio_ = std::make_unique<webrtc::AudioBuffer>(
                reference_config_.sample_rate_hz(), 1,
                processing_config_.sample_rate_hz(), 1,
                processing_config_.sample_rate_hz(), 1);

        capture_audio_ = std::make_unique<webrtc::AudioBuffer>(
                input_config_.sample_rate_hz(), 1,
                processing_config_.sample_rate_hz(), 1,
                output_config_.sample_rate_hz(), 1);
    }


    void processCapture(const int16_t *capture_frame, int16_t *output_frame) {
        capture_audio_->CopyFrom(capture_frame, input_config_);
        capture_audio_->SplitIntoFrequencyBands();
        hp_filter_->Process(capture_audio_.get(), true);
        echo_control_->AnalyzeCapture(capture_audio_.get());
        echo_control_->ProcessCapture(capture_audio_.get(), false);
        capture_audio_->MergeFrequencyBands();
        capture_audio_->CopyTo(output_config_, output_frame);
    }

    void processRender(const int16_t *render_frame) {
        render_audio_->CopyFrom(render_frame, reference_config_);
        render_audio_->SplitIntoFrequencyBands();
        echo_control_->AnalyzeRender(render_audio_.get());
    }

    int getDelay() {
        return echo_control_->GetMetrics().delay_ms;
    }

public:
    const int
            reference_sample_rate_,
            input_sample_rate_,
            processing_sample_rate_,
            output_sample_rate_,
            reference_frame_size_,
            input_frame_size_,
            processing_frame_size_,
            output_frame_size_;

private:
    constexpr static int getFrameSize(int sample_rate) { return sample_rate / 100; } //10 ms

    const webrtc::StreamConfig
            reference_config_,
            input_config_,
            processing_config_,
            output_config_;

    std::unique_ptr<webrtc::EchoControl> echo_control_;
    std::unique_ptr<webrtc::GainController2> gain_controller2;
    std::unique_ptr<webrtc::HighPassFilter> hp_filter_;
    std::unique_ptr<webrtc::AudioBuffer> render_audio_;
    std::unique_ptr<webrtc::AudioBuffer> capture_audio_;
};