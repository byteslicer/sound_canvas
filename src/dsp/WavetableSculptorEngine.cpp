#include "dsp/WavetableSculptorEngine.h"
#include <cmath>
#include <algorithm>
#include <atomic>

namespace soundcanvas
{

void WavetableSculptorEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void WavetableSculptorEngine::reset() noexcept
{
    lfoPhase_ = 0.0f;
    droneActive_ = false;
    lastDroneRoot_ = -1;
    lastDroneScale_ = -1;
    for (auto& v : voices_)
        v = {};
}

void WavetableSculptorEngine::setWavetable(std::shared_ptr<const WavetableData> wt) noexcept
{
    const juce::SpinLock::ScopedLockType lock(wtLock_);
    wavetable_ = std::move(wt);
}

std::shared_ptr<const WavetableData> WavetableSculptorEngine::getWavetable() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(wtLock_);
    return wavetable_;
}

void WavetableSculptorEngine::noteOn(int midiNote, float velocity) noexcept
{
    const int clampedNote = std::clamp(midiNote, 12, 108);

    // Reuse voice if already playing this note
    int targetIdx = -1;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices_[static_cast<size_t>(i)].active && voices_[static_cast<size_t>(i)].midiNote == clampedNote)
        {
            targetIdx = i;
            break;
        }
    }

    // Find free voice
    if (targetIdx < 0)
    {
        for (int i = 0; i < kMaxVoices; ++i)
        {
            if (!voices_[static_cast<size_t>(i)].active)
            {
                targetIdx = i;
                break;
            }
        }
    }

    // Steal quietest voice if all busy
    if (targetIdx < 0)
    {
        float minEnv = 999.0f;
        targetIdx = 0;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            if (voices_[static_cast<size_t>(i)].env < minEnv)
            {
                minEnv = voices_[static_cast<size_t>(i)].env;
                targetIdx = i;
            }
        }
    }

    auto& v = voices_[static_cast<size_t>(targetIdx)];
    v.active = true;
    v.releasing = false;
    v.isDroneVoice = false;
    v.midiNote = clampedNote;
    v.freqHz = ScaleQuantizer::midiNoteToFrequency(clampedNote);
    v.velocity = std::clamp(velocity, 0.1f, 1.0f);
}

void WavetableSculptorEngine::noteOff(int midiNote) noexcept
{
    for (auto& v : voices_)
    {
        if (v.active && !v.isDroneVoice && v.midiNote == midiNote)
            v.releasing = true;
    }
}

void WavetableSculptorEngine::allNotesOff() noexcept
{
    for (auto& v : voices_)
        v.releasing = true;
}

float WavetableSculptorEngine::sampleTable(const WavetableData& wt, float framePos, float phase) const noexcept
{
    const float fScaled = std::clamp(framePos, 0.0f, 1.0f) * static_cast<float>(WavetableData::kNumFrames - 1);
    const int f0 = static_cast<int>(std::floor(fScaled));
    const int f1 = std::min(f0 + 1, WavetableData::kNumFrames - 1);
    const float ft = fScaled - static_cast<float>(f0);

    const float sScaled = (phase - std::floor(phase)) * static_cast<float>(WavetableData::kSamplesPerFrame);
    const int s0 = static_cast<int>(std::floor(sScaled)) % WavetableData::kSamplesPerFrame;
    const int s1 = (s0 + 1) % WavetableData::kSamplesPerFrame;
    const float st = sScaled - std::floor(sScaled);

    const float v00 = wt.frames[static_cast<size_t>(f0)][static_cast<size_t>(s0)];
    const float v01 = wt.frames[static_cast<size_t>(f0)][static_cast<size_t>(s1)];
    const float v10 = wt.frames[static_cast<size_t>(f1)][static_cast<size_t>(s0)];
    const float v11 = wt.frames[static_cast<size_t>(f1)][static_cast<size_t>(s1)];

    const float frameA = v00 + st * (v01 - v00);
    const float frameB = v10 + st * (v11 - v10);
    return frameA + ft * (frameB - frameA);
}

void WavetableSculptorEngine::updateDroneChord(bool droneHold, int rootNote, int scaleIndex) noexcept
{
    if (!droneHold)
    {
        if (droneActive_)
        {
            for (auto& v : voices_)
            {
                if (v.isDroneVoice)
                    v.releasing = true;
            }
            droneActive_ = false;
        }
        return;
    }

    if (droneActive_ && rootNote == lastDroneRoot_ && scaleIndex == lastDroneScale_)
        return;

    // Release previous drone notes
    for (auto& v : voices_)
    {
        if (v.isDroneVoice)
            v.releasing = true;
    }

    const auto& intervals = ScaleQuantizer::getScaleIntervals(scaleIndex);
    const int nDeg = static_cast<int>(intervals.size());
    const int rootMidi = 48 + std::clamp(rootNote, 0, 11);

    const int chordNotes[3] = {
        rootMidi,
        rootMidi + intervals[static_cast<size_t>(2 % nDeg)],
        rootMidi + 12 + intervals[static_cast<size_t>(4 % nDeg)]
    };

    for (int c = 0; c < 3; ++c)
    {
        auto& v = voices_[static_cast<size_t>(c)];
        v.active = true;
        v.releasing = false;
        v.isDroneVoice = true;
        v.midiNote = chordNotes[c];
        v.freqHz = ScaleQuantizer::midiNoteToFrequency(chordNotes[c]);
        v.velocity = 0.58f;
    }

    droneActive_ = true;
    lastDroneRoot_ = rootNote;
    lastDroneScale_ = scaleIndex;
}

void WavetableSculptorEngine::renderBlock(float* outLeft,
                                          float* outRight,
                                          int numSamples,
                                          float morphPosition,
                                          float morphLfoDepth,
                                          float morphRateHz,
                                          bool droneHold,
                                          int rootNote,
                                          int scaleIndex,
                                          float level) noexcept
{
    updateDroneChord(droneHold, rootNote, scaleIndex);

    auto wt = getWavetable();
    if (!wt || level <= 1.0e-5f || numSamples <= 0)
        return;

    const float invSr = 1.0f / static_cast<float>(sampleRate_);
    const float attackStep = 1.0f / (0.065f * static_cast<float>(sampleRate_));
    const float releaseMult = std::exp(-1.0f / (0.38f * static_cast<float>(sampleRate_)));
    const float lfoInc = std::clamp(morphRateHz, 0.02f, 4.0f) * invSr;

    float curMorph = morphPosition;

    for (int s = 0; s < numSamples; ++s)
    {
        lfoPhase_ += lfoInc;
        if (lfoPhase_ >= 1.0f)
            lfoPhase_ -= 1.0f;

        const float lfoVal = std::sin(lfoPhase_ * juce::MathConstants<float>::twoPi);
        curMorph = std::clamp(morphPosition + lfoVal * morphLfoDepth * 0.45f, 0.0f, 1.0f);

        const int frameIdx = std::clamp(static_cast<int>(curMorph * static_cast<float>(WavetableData::kNumFrames - 1)),
                                        0, WavetableData::kNumFrames - 1);
        const PixelSample& frameCol = wt->frameColors[static_cast<size_t>(frameIdx)];
        const float detuneSpread = 0.0025f + 0.0055f * frameCol.saturation;

        float sumL = 0.0f;
        float sumR = 0.0f;

        for (auto& v : voices_)
        {
            if (!v.active)
                continue;

            if (v.releasing)
            {
                v.env *= releaseMult;
                if (v.env < 1.0e-4f)
                {
                    v.active = false;
                    v.releasing = false;
                    v.env = 0.0f;
                    continue;
                }
            }
            else
            {
                v.env = std::min(1.0f, v.env + attackStep);
            }

            const float baseInc = v.freqHz * invSr;
            v.phaseCenter += baseInc;
            if (v.phaseCenter >= 1.0f) v.phaseCenter -= 1.0f;

            v.phaseLeft += baseInc * (1.0f - detuneSpread);
            if (v.phaseLeft >= 1.0f) v.phaseLeft -= 1.0f;

            v.phaseRight += baseInc * (1.0f + detuneSpread);
            if (v.phaseRight >= 1.0f) v.phaseRight -= 1.0f;

            const float cSig = sampleTable(*wt, curMorph, v.phaseCenter);
            const float lSig = sampleTable(*wt, curMorph, v.phaseLeft);
            const float rSig = sampleTable(*wt, curMorph, v.phaseRight);

            const float gain = v.env * v.velocity * 0.22f;
            sumL += (0.55f * cSig + 0.45f * lSig) * gain;
            sumR += (0.55f * cSig + 0.45f * rSig) * gain;
        }

        outLeft[s] += sumL * level;
        outRight[s] += sumR * level;
    }

    activeMorphPos_.store(curMorph);
    const int finalIdx = std::clamp(static_cast<int>(curMorph * static_cast<float>(WavetableData::kNumFrames - 1)),
                                    0, WavetableData::kNumFrames - 1);
    activeFrameColor_ = wt->frameColors[static_cast<size_t>(finalIdx)];
}

} // namespace soundcanvas
