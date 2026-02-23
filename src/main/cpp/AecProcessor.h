#pragma once

#include <vector>
#include <memory>
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "api/audio/echo_canceller3_factory.h"
#include "modules/audio_processing/gain_controller2.h"
#include "utils/temp.h"

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
//        myLog("processCapture: s: %d, e: %d, e-s: %d",
//              inCaptureStart, inCaptureEnd, inCaptureEnd - inCaptureStart);
//        myLog("OUT: s: %d, e: %d, e-s: %d", outStart, outEnd, outEnd - outStart);

        std::copy(capture_frame, capture_frame + CAPTURE_FRAME_SAMPLES_HW,
                  inCaptureBuffer.data() + inCaptureEnd);
        inCaptureEnd += CAPTURE_FRAME_SAMPLES_HW;
        if (inCaptureEnd - inCaptureStart >= CAPTURE_FRAME_SAMPLES_AEC) {
//            myLog("AnalyzeCapture");
            capture_audio_->CopyFrom(inCaptureBuffer.data() + inCaptureStart, input_config_);
            capture_audio_->SplitIntoFrequencyBands();
            hp_filter_->Process(capture_audio_.get(), true);
            echo_control_->AnalyzeCapture(capture_audio_.get());
            echo_control_->ProcessCapture(capture_audio_.get(), false);
            capture_audio_->MergeFrequencyBands();
            inCaptureStart += CAPTURE_FRAME_SAMPLES_AEC;
            capture_audio_->CopyTo(output_config_, outBuffer.data() + outEnd);
            outEnd += OUT_FRAME_SAMPLES_AEC;
        }
        std::copy(outBuffer.data() + outStart, outBuffer.data() + outStart + OUT_FRAME_SAMPLES_HW,
                  output_frame);
        outStart += OUT_FRAME_SAMPLES_HW;
        if (inCaptureEnd == inCaptureStart) {
//            myLog("capture loop");
            inCaptureEnd = 0;
            inCaptureStart = 0;
        }
        if (outEnd == outStart) {
//            myLog("OUT loop");
            outEnd = 0;
            outStart = 0;
        }
    }

    void processRender(const int16_t *render_frame) {
//        myLog("processRender: s: %d, e: %d, e-s: %d",
//              inRenderStart, inRenderEnd, inRenderEnd - inRenderStart);

        std::copy(render_frame, render_frame + RENDER_FRAME_SAMPLES_HW,
                  inRenderBuffer.data() + inRenderEnd);
        inRenderEnd += RENDER_FRAME_SAMPLES_HW;
        if (inRenderEnd - inRenderStart >= RENDER_FRAME_SAMPLES_AEC) {
//            myLog("AnalyzeRender");
            render_audio_->CopyFrom(inRenderBuffer.data() + inRenderStart,
                                    reference_config_);
            render_audio_->SplitIntoFrequencyBands();
            echo_control_->AnalyzeRender(render_audio_.get());

            inRenderStart += RENDER_FRAME_SAMPLES_AEC;
        }
        if (inRenderEnd == inRenderStart) {
//            myLog("render loop");
            inRenderEnd = 0;
            inRenderStart = 0;
        }
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
    std::unique_ptr<webrtc::HighPassFilter> hp_filter_;
    std::unique_ptr<webrtc::AudioBuffer> render_audio_;
    std::unique_ptr<webrtc::AudioBuffer> capture_audio_;

    std::array<int16_t, 4 * RENDER_FRAME_SAMPLES_AEC> inRenderBuffer{};
    std::array<int16_t, 4 * CAPTURE_FRAME_SAMPLES_AEC> inCaptureBuffer{};
    std::array<int16_t, 4 * OUT_FRAME_SAMPLES_AEC> outBuffer{};
    size_t inRenderStart{};
    size_t inRenderEnd{RENDER_FRAME_SAMPLES_HW};
    size_t inCaptureStart{};
    size_t inCaptureEnd{CAPTURE_FRAME_SAMPLES_HW};
    size_t outStart{};
    size_t outEnd{};

};