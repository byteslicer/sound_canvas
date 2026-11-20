#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <array>

namespace soundcanvas
{

class MasterEffects
{
public:
    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = std::max(8000.0, sampleRate);
        reverb_.setSampleRate(sampleRate_);
        reset();
    }

    void reset() noexcept
    {
        ic1eqL_ = ic2eqL_ = 0.0f;
        ic1eqR_ = ic2eqR_ = 0.0f;
        chorusPhase_ = 0.0f;
        chorusWriteIdx_ = 0;
        chorusBufL_.fill(0.0f);
        chorusBufR_.fill(0.0f);
        reverb_.reset();
    }

    void processBlock(float* left,
                      float* right,
                      int numSamples,
                      float cutoffHz,
                      float resonance,
                      float chorusDepth,
                      float reverbMix,
                      float masterGain = 0.85f) noexcept
    {
        if (numSamples <= 0)
            return;

        // Cytomic SVF lowpass coefficients
        const float clampedCut = std::clamp(cutoffHz, 80.0f, static_cast<float>(sampleRate_) * 0.45f);
        const float g = std::tan(juce::MathConstants<float>::pi * clampedCut / static_cast<float>(sampleRate_));
        const float k = 2.0f - 1.75f * std::clamp(resonance, 0.0f, 0.95f);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float chorusInc = 0.42f / static_cast<float>(sampleRate_);
        const float cDepth = std::clamp(chorusDepth, 0.0f, 1.0f);

        for (int s = 0; s < numSamples; ++s)
        {
            // 1. State-Variable Filter (Lowpass)
            const float v0L = left[s];
            const float v3L = v0L - ic2eqL_;
            const float v1L = a1 * ic1eqL_ + a2 * v3L;
            const float v2L = ic2eqL_ + a2 * ic1eqL_ + a3 * v3L;
            ic1eqL_ = 2.0f * v1L - ic1eqL_;
            ic2eqL_ = 2.0f * v2L - ic2eqL_;

            const float v0R = right[s];
            const float v3R = v0R - ic2eqR_;
            const float v1R = a1 * ic1eqR_ + a2 * v3R;
            const float v2R = ic2eqR_ + a2 * ic1eqR_ + a3 * v3R;
            ic1eqR_ = 2.0f * v1R - ic1eqR_;
            ic2eqR_ = 2.0f * v2R - ic2eqR_;

            float sigL = v2L;
            float sigR = v2R;

            // 2. Stereo Modulated Chorus
            chorusBufL_[static_cast<size_t>(chorusWriteIdx_)] = sigL;
            chorusBufR_[static_cast<size_t>(chorusWriteIdx_)] = sigR;

            chorusPhase_ += chorusInc;
            if (chorusPhase_ >= 1.0f)
                chorusPhase_ -= 1.0f;

            const float lfoL = 0.5f + 0.5f * std::sin(chorusPhase_ * juce::MathConstants<float>::twoPi);
            const float lfoR = 0.5f + 0.5f * std::cos(chorusPhase_ * juce::MathConstants<float>::twoPi);

            const float delaySamplesL = (0.008f + 0.007f * lfoL) * static_cast<float>(sampleRate_);
            const float delaySamplesR = (0.009f + 0.007f * lfoR) * static_cast<float>(sampleRate_);

            const int readL = (chorusWriteIdx_ - static_cast<int>(delaySamplesL) + kChorusBufSize) % kChorusBufSize;
            const int readR = (chorusWriteIdx_ - static_cast<int>(delaySamplesR) + kChorusBufSize) % kChorusBufSize;

            sigL = sigL + 0.35f * cDepth * chorusBufL_[static_cast<size_t>(readL)];
            sigR = sigR + 0.35f * cDepth * chorusBufR_[static_cast<size_t>(readR)];

            chorusWriteIdx_ = (chorusWriteIdx_ + 1) % kChorusBufSize;

            left[s] = sigL;
            right[s] = sigR;
        }

        // 3. Lush Stereo Hall Reverb
        juce::Reverb::Parameters revParams;
        revParams.roomSize = std::clamp(0.55f + 0.38f * reverbMix, 0.1f, 0.96f);
        revParams.damping = 0.32f;
        revParams.wetLevel = std::clamp(reverbMix * 0.55f, 0.0f, 0.65f);
        revParams.dryLevel = 1.0f - revParams.wetLevel * 0.35f;
        revParams.width = 0.95f;
        reverb_.setParameters(revParams);
        reverb_.processStereo(left, right, numSamples);

        // 4. Master Gain + Smooth Tanh Saturation Limiter
        for (int s = 0; s < numSamples; ++s)
        {
            left[s] = std::tanh(left[s] * masterGain);
            right[s] = std::tanh(right[s] * masterGain);
        }
    }

private:
    static constexpr int kChorusBufSize = 4096;
    double sampleRate_ = 48000.0;
    float ic1eqL_ = 0.0f, ic2eqL_ = 0.0f;
    float ic1eqR_ = 0.0f, ic2eqR_ = 0.0f;
    float chorusPhase_ = 0.0f;
    int chorusWriteIdx_ = 0;
    std::array<float, kChorusBufSize> chorusBufL_ {};
    std::array<float, kChorusBufSize> chorusBufR_ {};
    juce::Reverb reverb_;
};

} // namespace soundcanvas
