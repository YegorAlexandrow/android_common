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
            input_sample_rate_(24000),
//            render_sample_rate_(16000),
            sample_rate_(16000),
            // StreamConfig used only for sample rate and channels (webrtc interfaces)
            // assume implicit resample 24->16 in render_audio_
            input_config_(input_sample_rate_, 1),
            config_(sample_rate_, 1)
    {
        webrtc::EchoCanceller3Config aec_config;
        webrtc::EchoCanceller3Factory aec_factory(aec_config);

        echo_control_ = aec_factory.Create(sample_rate_, 1, 1);
        hp_filter_ = std::make_unique<webrtc::HighPassFilter>(sample_rate_, 1);

        render_audio_ = std::make_unique<webrtc::AudioBuffer>(
                input_config_.sample_rate_hz(), 1, // assume implicit resample 24->16
                config_.sample_rate_hz(), 1,
                config_.sample_rate_hz(), 1);

        capture_audio_ = std::make_unique<webrtc::AudioBuffer>(
                input_config_.sample_rate_hz(), 1, // assume implicit resample 24->16
                config_.sample_rate_hz(), 1,
                config_.sample_rate_hz(), 1);
    }

    void processFrame(const int16_t *render_frame,
                      const int16_t *capture_frame,
                      int16_t *output_frame) {
//        namespace ch = std::chrono;
//        using clock = ch::high_resolution_clock;
//        const auto cast = [](auto val) { return ch::duration_cast<ch::microseconds>(val).count(); };
//        auto start = clock::now();
        // assume implicit resample 24->16
        render_audio_->CopyFrom(render_frame, input_config_);
//        auto duration_rC = cast(clock::now() - start);
//        start = clock::now();
        // assume implicit resample 24->16
        capture_audio_->CopyFrom(capture_frame, input_config_);
//        auto duration_cC = cast(clock::now() - start);
//        start = clock::now();
        hp_filter_->Process(capture_audio_.get(), true);
//        auto duration_hp = cast(clock::now() - start);
//        start = clock::now();
        echo_control_->AnalyzeCapture(capture_audio_.get());
//        auto duration_AC = cast(clock::now() - start);
//        start = clock::now();
        echo_control_->AnalyzeRender(render_audio_.get());
//        auto duration_AR = cast(clock::now() - start);
//        start = clock::now();
        echo_control_->ProcessCapture(capture_audio_.get(), false);
//        auto duration_PC = cast(clock::now() - start);
//        start = clock::now();
        capture_audio_->CopyTo(config_, output_frame);
//        auto duration_oC = cast(clock::now() - start);
//        myLog(
//                "rC: %10lld | cC: %10lld | hp: %10lld |  AC: %10lld |  "
//                "AR: %10lld |  PC: %10lld |  oC: %10lld",
//                duration_rC, duration_cC, duration_hp, duration_AC,
//                duration_AR, duration_PC, duration_oC
//        );
    }

private:
    const int input_sample_rate_;
    const int sample_rate_;
    const webrtc::StreamConfig input_config_;
    const webrtc::StreamConfig config_;
    std::unique_ptr<webrtc::EchoControl> echo_control_;
    std::unique_ptr<webrtc::HighPassFilter> hp_filter_;
    std::unique_ptr<webrtc::AudioBuffer> render_audio_;
    std::unique_ptr<webrtc::AudioBuffer> capture_audio_;
};