#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

// Lightweight, real-time-safe polyphonic audition synth for standalone & quick DAW monitoring.
class PreviewSynthEngine
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxDelaySamples = 32768; // ~680ms at 48kHz

    static juce::StringArray getModeNames()
    {
        return {
            "Sound: Off",
            "Sound: Multi",
            "Sound: Pluck",
            "Sound: Pad",
            "Sound: Bells"
        };
    }

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = std::max(8000.0, sampleRate);
        delayLenL_ = std::clamp(static_cast<int>(sampleRate_ * 0.23), 64, kMaxDelaySamples - 1);
        delayLenR_ = std::clamp(static_cast<int>(sampleRate_ * 0.31), 64, kMaxDelaySamples - 1);
        reset();
    }

    void reset() noexcept
    {
        for (auto& v : voices_)
            v = {};
        channelCutoffNorm_.fill(0.65f);
        channelModNorm_.fill(0.25f);
        channelPan_.fill(0.5f);
        channelPan_[0] = 0.35f;
        channelPan_[1] = 0.65f;
        channelPan_[2] = 0.45f;
        channelPan_[3] = 0.55f;

        delayBufL_.fill(0.0f);
        delayBufR_.fill(0.0f);
        delayWriteIdx_ = 0;
        delayLpL_ = 0.0f;
        delayLpR_ = 0.0f;
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices_)
        {
            if (v.active)
                v.releasing = true;
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer,
                      const juce::MidiBuffer& midiMessages,
                      int soundMode) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        if (soundMode <= 0)
        {
            if (wasActive_)
            {
                reset();
                wasActive_ = false;
            }
            return;
        }
        wasActive_ = true;

        float* outL = buffer.getWritePointer(0);
        float* outR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

        auto midiIt = midiMessages.cbegin();
        const auto midiEnd = midiMessages.cend();

        const double invSr = 1.0 / sampleRate_;
        constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

        for (int s = 0; s < numSamples; ++s)
        {
            while (midiIt != midiEnd && (*midiIt).samplePosition <= s)
            {
                handleMidiMessage((*midiIt).getMessage(), soundMode);
                ++midiIt;
            }

            float mixL = 0.0f;
            float mixR = 0.0f;

            for (auto& v : voices_)
            {
                if (!v.active)
                    continue;

                // Envelope progression
                if (!v.releasing)
                {
                    if (v.attackStage)
                    {
                        v.env += v.attackInc;
                        if (v.env >= 1.0f)
                        {
                            v.env = 1.0f;
                            v.attackStage = false;
                        }
                    }
                    else
                    {
                        v.env = v.sustainLevel + (v.env - v.sustainLevel) * v.decayCoeff;
                    }
                }
                else
                {
                    v.env *= v.releaseCoeff;
                    if (v.env < 1.0e-4f)
                    {
                        v.active = false;
                        continue;
                    }
                }

                const size_t chIdx = static_cast<size_t>(std::clamp(v.channel - 1, 0, 15));
                const float ccBright = channelCutoffNorm_[chIdx];
                const float ccMod = channelModNorm_[chIdx];

                v.phase += v.phaseInc;
                if (v.phase >= 1.0f)
                    v.phase -= 1.0f;

                v.modPhase += v.modPhaseInc;
                if (v.modPhase >= 1.0f)
                    v.modPhase -= 1.0f;

                float sig = 0.0f;
                const float pRad = v.phase * kTwoPi;

                if (v.timbre == 0) // Warm Pluck (glassy harmonic pluck)
                {
                    const float bright = v.env * (0.35f + 0.65f * ccBright);
                    const float h1 = std::sin(pRad);
                    const float h2 = std::sin(pRad * 2.0f) * (0.45f * bright);
                    const float h3 = std::sin(pRad * 3.0f) * (0.22f * bright * bright);
                    const float fm = std::sin(pRad + 0.55f * bright * std::sin(pRad * 2.0f));
                    sig = 0.58f * (h1 + h2 + h3) + 0.30f * fm;
                }
                else if (v.timbre == 1) // Ambient Pad (detuned dual warm voices)
                {
                    v.phase2 += v.phaseInc * (1.0045f + 0.004f * ccMod);
                    if (v.phase2 >= 1.0f)
                        v.phase2 -= 1.0f;

                    const float p2Rad = v.phase2 * kTwoPi;
                    const float tri1 = 2.0f * std::abs(2.0f * v.phase - 1.0f) - 1.0f;
                    const float tri2 = 2.0f * std::abs(2.0f * v.phase2 - 1.0f) - 1.0f;
                    const float sine = 0.5f * (std::sin(pRad) + std::sin(p2Rad));
                    sig = 0.55f * sine + 0.45f * (0.5f * (tri1 + tri2));
                }
                else // 2 = Crystal Bells (FM metallic chime)
                {
                    const float fmIndex = (1.15f + 1.25f * ccBright) * (0.25f + 0.75f * v.env * v.env);
                    const float modSig = std::sin(v.modPhase * kTwoPi);
                    sig = 0.78f * std::sin(pRad + fmIndex * modSig)
                        + 0.22f * std::sin(pRad * 0.5f);
                }

                // One-pole musical lowpass filter modulated by CC74 + envelope
                const float cutoffHz = std::clamp(
                    380.0f + (v.timbre == 2 ? 7500.0f : 5200.0f) * (0.25f + 0.75f * ccBright) * (0.35f + 0.65f * v.env),
                    120.0f, 14000.0f);
                const float lpCoeff = std::clamp(static_cast<float>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * cutoffHz * invSr)), 0.005f, 0.92f);
                v.lpState += lpCoeff * (sig - v.lpState);

                const float sample = v.lpState * v.env * v.velocity * 0.24f;
                const float pan = std::clamp(channelPan_[chIdx], 0.08f, 0.92f);
                const float gainL = std::cos(pan * juce::MathConstants<float>::halfPi);
                const float gainR = std::sin(pan * juce::MathConstants<float>::halfPi);

                mixL += sample * gainL;
                mixR += sample * gainR;
            }

            // Gentle damped ping-pong stereo ambience delay
            const int readL = (delayWriteIdx_ - delayLenL_ + kMaxDelaySamples) % kMaxDelaySamples;
            const int readR = (delayWriteIdx_ - delayLenR_ + kMaxDelaySamples) % kMaxDelaySamples;
            const float delL = delayBufL_[static_cast<size_t>(readL)];
            const float delR = delayBufR_[static_cast<size_t>(readR)];

            delayLpL_ += 0.22f * (delL - delayLpL_);
            delayLpR_ += 0.22f * (delR - delayLpR_);

            delayBufL_[static_cast<size_t>(delayWriteIdx_)] = mixL + delayLpR_ * 0.30f;
            delayBufR_[static_cast<size_t>(delayWriteIdx_)] = mixR + delayLpL_ * 0.28f;
            delayWriteIdx_ = (delayWriteIdx_ + 1) % kMaxDelaySamples;

            const float wetL = std::tanh((mixL + delayLpL_ * 0.26f) * 1.15f) * 0.85f;
            const float wetR = std::tanh((mixR + delayLpR_ * 0.26f) * 1.15f) * 0.85f;

            outL[s] = wetL;
            if (outR != nullptr)
                outR[s] = wetR;
        }
    }

private:
    struct Voice
    {
        bool active = false;
        bool attackStage = false;
        bool releasing = false;
        int channel = 1;
        int note = 60;
        int timbre = 0; // 0=Pluck, 1=Pad, 2=Bell
        float velocity = 0.8f;
        float phase = 0.0f;
        float phase2 = 0.0f;
        float phaseInc = 0.01f;
        float modPhase = 0.0f;
        float modPhaseInc = 0.02f;
        float env = 0.0f;
        float attackInc = 0.01f;
        float decayCoeff = 0.999f;
        float sustainLevel = 0.35f;
        float releaseCoeff = 0.999f;
        float lpState = 0.0f;
        uint64_t age = 0;
    };

    int resolveTimbre(int channel, int soundMode) const noexcept
    {
        switch (soundMode)
        {
            case 2: return 0; // Pluck
            case 3: return 1; // Pad
            case 4: return 2; // Bells
            default: // 1 = Multi-Voice per channel
                switch ((std::max(1, channel) - 1) % 4)
                {
                    case 0: return 0; // Orb 1: Warm Pluck
                    case 1: return 2; // Orb 2: Crystal Bell
                    case 2: return 1; // Orb 3: Warm Bass Pad
                    default: return 2; // Orb 4: Shimmer Bell
                }
        }
    }

    void handleMidiMessage(const juce::MidiMessage& msg, int soundMode) noexcept
    {
        const int ch = std::clamp(msg.getChannel(), 1, 16);
        const size_t chIdx = static_cast<size_t>(ch - 1);

        if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            allNotesOff();
            return;
        }

        if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            const float val = static_cast<float>(msg.getControllerValue()) / 127.0f;
            if (cc == 74 || cc == 71 || cc == 11)
                channelCutoffNorm_[chIdx] = val;
            else if (cc == 1 || cc == 91 || cc == 73)
                channelModNorm_[chIdx] = val;
            else if (cc == 10)
                channelPan_[chIdx] = val;
            return;
        }

        if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            for (auto& v : voices_)
            {
                if (v.active && v.channel == ch && v.note == note)
                    v.releasing = true;
            }
            return;
        }

        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const float vel = std::clamp(msg.getFloatVelocity(), 0.08f, 1.0f);

            int slot = -1;
            uint64_t oldestAge = UINT64_MAX;
            int oldestIdx = 0;

            for (int i = 0; i < kMaxVoices; ++i)
            {
                const auto& v = voices_[static_cast<size_t>(i)];
                if (v.active && v.channel == ch && v.note == note)
                {
                    slot = i;
                    break;
                }
                if (!v.active && slot < 0)
                {
                    slot = i;
                }
                if (v.age < oldestAge)
                {
                    oldestAge = v.age;
                    oldestIdx = i;
                }
            }

            if (slot < 0)
                slot = oldestIdx;

            auto& v = voices_[static_cast<size_t>(slot)];
            const int timbre = resolveTimbre(ch, soundMode);
            const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);

            v.active = true;
            v.attackStage = true;
            v.releasing = false;
            v.channel = ch;
            v.note = note;
            v.timbre = timbre;
            v.velocity = vel;
            v.phase = 0.0f;
            v.phase2 = 0.0f;
            v.modPhase = 0.0f;
            v.phaseInc = freq / static_cast<float>(sampleRate_);
            v.modPhaseInc = (freq * (timbre == 2 ? 2.756f : 2.0f)) / static_cast<float>(sampleRate_);
            v.env = 0.0f;
            v.lpState = 0.0f;
            v.age = ++voiceAgeCounter_;

            const float sr = static_cast<float>(sampleRate_);
            if (timbre == 1) // Pad
            {
                v.attackInc = 1.0f / std::max(1.0f, 0.095f * sr);
                v.decayCoeff = std::exp(-1.0f / (0.65f * sr));
                v.sustainLevel = 0.72f;
                v.releaseCoeff = std::exp(-1.0f / (0.55f * sr));
            }
            else if (timbre == 2) // Crystal Bells
            {
                v.attackInc = 1.0f / std::max(1.0f, 0.004f * sr);
                v.decayCoeff = std::exp(-1.0f / (0.42f * sr));
                v.sustainLevel = 0.16f;
                v.releaseCoeff = std::exp(-1.0f / (0.45f * sr));
            }
            else // Warm Pluck
            {
                v.attackInc = 1.0f / std::max(1.0f, 0.005f * sr);
                v.decayCoeff = std::exp(-1.0f / (0.28f * sr));
                v.sustainLevel = 0.24f;
                v.releaseCoeff = std::exp(-1.0f / (0.22f * sr));
            }
        }
    }

    double sampleRate_ = 48000.0;
    bool wasActive_ = false;
    uint64_t voiceAgeCounter_ = 0;

    std::array<Voice, kMaxVoices> voices_ {};
    std::array<float, 16> channelCutoffNorm_ {};
    std::array<float, 16> channelModNorm_ {};
    std::array<float, 16> channelPan_ {};

    std::array<float, kMaxDelaySamples> delayBufL_ {};
    std::array<float, kMaxDelaySamples> delayBufR_ {};
    int delayWriteIdx_ = 0;
    int delayLenL_ = 11040;
    int delayLenR_ = 14880;
    float delayLpL_ = 0.0f;
    float delayLpR_ = 0.0f;
};

} // namespace soundcanvas
