#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace soundcanvas
{

struct SynthFxParams
{
    float chorusRate = 0.45f;
    float chorusDepth = 0.45f;
    float chorusMix = 0.35f;

    float delayTimeSec = 0.36f;
    float delayFeedback = 0.38f;
    float delayMix = 0.25f;

    float reverbSize = 0.65f;
    float reverbDamping = 0.35f;
    float reverbMix = 0.38f;

    float masterDrive = 0.10f;
    float masterGain = 0.85f;
};

class SynthMasterEffects
{
public:
    void prepare(double sampleRate)
    {
        sampleRate_ = std::max(8000.0, sampleRate);
        chorusBufL_.assign(static_cast<size_t>(kChorusBufSize), 0.0f);
        chorusBufR_.assign(static_cast<size_t>(kChorusBufSize), 0.0f);
        delayBufL_.assign(static_cast<size_t>(kDelayBufSize), 0.0f);
        delayBufR_.assign(static_cast<size_t>(kDelayBufSize), 0.0f);
        reverb_.setSampleRate(sampleRate_);
        reset();
    }

    void reset() noexcept
    {
        chorusPhase_ = 0.0f;
        chorusWriteIdx_ = 0;
        std::fill(chorusBufL_.begin(), chorusBufL_.end(), 0.0f);
        std::fill(chorusBufR_.begin(), chorusBufR_.end(), 0.0f);

        delayWriteIdx_ = 0;
        delayDampL_ = 0.0f;
        delayDampR_ = 0.0f;
        std::fill(delayBufL_.begin(), delayBufL_.end(), 0.0f);
        std::fill(delayBufR_.begin(), delayBufR_.end(), 0.0f);

        reverb_.reset();
    }

    void processBlock(float* left,
                      float* right,
                      int numSamples,
                      const SynthFxParams& fx) noexcept
    {
        if (numSamples <= 0 || chorusBufL_.empty() || delayBufL_.empty())
            return;

        const float sr = static_cast<float>(sampleRate_);
        const float chorusInc = std::clamp(fx.chorusRate, 0.05f, 5.0f) / sr;
        const float cDepth = std::clamp(fx.chorusDepth, 0.0f, 1.0f);
        const float cMix = std::clamp(fx.chorusMix, 0.0f, 1.0f);

        const int maxDelaySamples = kDelayBufSize - 4;
        const int delaySampsL = std::clamp(static_cast<int>(fx.delayTimeSec * sr), 64, maxDelaySamples);
        const int delaySampsR = std::clamp(static_cast<int>(fx.delayTimeSec * 0.75f * sr), 64, maxDelaySamples);
        const float dFb = std::clamp(fx.delayFeedback, 0.0f, 0.88f);
        const float dMix = std::clamp(fx.delayMix, 0.0f, 1.0f);

        for (int s = 0; s < numSamples; ++s)
        {
            float sigL = left[s];
            float sigR = right[s];

            // 1. Stereo Modulated Chorus
            chorusBufL_[static_cast<size_t>(chorusWriteIdx_)] = sigL;
            chorusBufR_[static_cast<size_t>(chorusWriteIdx_)] = sigR;

            chorusPhase_ += chorusInc;
            if (chorusPhase_ >= 1.0f) chorusPhase_ -= 1.0f;

            const float lfoL = 0.5f + 0.5f * std::sin(chorusPhase_ * juce::MathConstants<float>::twoPi);
            const float lfoR = 0.5f + 0.5f * std::cos(chorusPhase_ * juce::MathConstants<float>::twoPi);

            const float dSamplesL = (0.006f + 0.009f * cDepth * lfoL) * sr;
            const float dSamplesR = (0.007f + 0.009f * cDepth * lfoR) * sr;

            const int readCL = (chorusWriteIdx_ - static_cast<int>(dSamplesL) + kChorusBufSize) % kChorusBufSize;
            const int readCR = (chorusWriteIdx_ - static_cast<int>(dSamplesR) + kChorusBufSize) % kChorusBufSize;

            sigL = sigL * (1.0f - cMix * 0.25f) + chorusBufL_[static_cast<size_t>(readCL)] * (cMix * 0.55f);
            sigR = sigR * (1.0f - cMix * 0.25f) + chorusBufR_[static_cast<size_t>(readCR)] * (cMix * 0.55f);
            chorusWriteIdx_ = (chorusWriteIdx_ + 1) % kChorusBufSize;

            // 2. Ping-Pong Stereo Delay with Damping
            if (dMix > 0.002f)
            {
                const int readDL = (delayWriteIdx_ - delaySampsL + kDelayBufSize) % kDelayBufSize;
                const int readDR = (delayWriteIdx_ - delaySampsR + kDelayBufSize) % kDelayBufSize;
                const float tapL = delayBufL_[static_cast<size_t>(readDL)];
                const float tapR = delayBufR_[static_cast<size_t>(readDR)];

                delayDampL_ += 0.35f * (tapR - delayDampL_);
                delayDampR_ += 0.35f * (tapL - delayDampR_);

                delayBufL_[static_cast<size_t>(delayWriteIdx_)] = sigL + delayDampL_ * dFb;
                delayBufR_[static_cast<size_t>(delayWriteIdx_)] = sigR + delayDampR_ * dFb;

                sigL += tapL * dMix * 0.65f;
                sigR += tapR * dMix * 0.65f;
            }
            else
            {
                delayBufL_[static_cast<size_t>(delayWriteIdx_)] = 0.0f;
                delayBufR_[static_cast<size_t>(delayWriteIdx_)] = 0.0f;
            }
            delayWriteIdx_ = (delayWriteIdx_ + 1) % kDelayBufSize;

            left[s] = sigL;
            right[s] = sigR;
        }

        // 3. Stereo Gallery Reverb
        if (fx.reverbMix > 0.002f)
        {
            juce::Reverb::Parameters revParams;
            revParams.roomSize = std::clamp(fx.reverbSize, 0.05f, 0.98f);
            revParams.damping = std::clamp(fx.reverbDamping, 0.05f, 0.95f);
            revParams.wetLevel = std::clamp(fx.reverbMix * 0.62f, 0.0f, 0.72f);
            revParams.dryLevel = 1.0f - revParams.wetLevel * 0.30f;
            revParams.width = 0.96f;
            reverb_.setParameters(revParams);
            reverb_.processStereo(left, right, numSamples);
        }

        // 4. Master Warmth Drive & Soft Limiter
        const float driveGain = 1.0f + std::clamp(fx.masterDrive, 0.0f, 1.0f) * 2.5f;
        const float outGain = std::clamp(fx.masterGain, 0.0f, 1.5f);
        for (int s = 0; s < numSamples; ++s)
        {
            left[s] = std::tanh(left[s] * driveGain * outGain);
            right[s] = std::tanh(right[s] * driveGain * outGain);
        }
    }

private:
    static constexpr int kChorusBufSize = 4096;
    static constexpr int kDelayBufSize = 262144; // ~5.4s at 48kHz

    double sampleRate_ = 48000.0;
    float chorusPhase_ = 0.0f;
    int chorusWriteIdx_ = 0;
    std::vector<float> chorusBufL_;
    std::vector<float> chorusBufR_;

    int delayWriteIdx_ = 0;
    float delayDampL_ = 0.0f;
    float delayDampR_ = 0.0f;
    std::vector<float> delayBufL_;
    std::vector<float> delayBufR_;

    juce::Reverb reverb_;
};

} // namespace soundcanvas
