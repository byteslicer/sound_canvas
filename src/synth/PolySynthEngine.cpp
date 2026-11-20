#include "synth/PolySynthEngine.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

float PolySynthEngine::AdsrState::step(float sr, float aSec, float dSec, float sLevel, float rSec) noexcept
{
    const float minTime = 0.0015f;
    switch (stage)
    {
        case EnvStage::Idle:
            level = 0.0f;
            break;

        case EnvStage::Attack:
        {
            const float stepAmt = 1.0f / (std::max(minTime, aSec) * sr);
            level += stepAmt;
            if (level >= 1.0f)
            {
                level = 1.0f;
                stage = EnvStage::Decay;
            }
            break;
        }

        case EnvStage::Decay:
        {
            const float coeff = std::exp(-1.0f / (std::max(minTime, dSec) * sr * 0.35f));
            const float target = std::clamp(sLevel, 0.0f, 1.0f);
            level = target + coeff * (level - target);
            if (std::abs(level - target) < 1.0e-4f)
            {
                level = target;
                stage = EnvStage::Sustain;
            }
            break;
        }

        case EnvStage::Sustain:
            level = std::clamp(sLevel, 0.0f, 1.0f);
            break;

        case EnvStage::Release:
        {
            const float coeff = std::exp(-1.0f / (std::max(minTime, rSec) * sr * 0.35f));
            level *= coeff;
            if (level < 1.0e-4f)
            {
                level = 0.0f;
                stage = EnvStage::Idle;
            }
            break;
        }
    }
    return level;
}

void PolySynthEngine::SvfStage::processSample(float& l, float& r, float g, float k, int mode) noexcept
{
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;

    const float v3L = l - ic2L;
    const float v1L = a1 * ic1L + a2 * v3L;
    const float v2L = ic2L + a2 * ic1L + a3 * v3L;
    ic1L = 2.0f * v1L - ic1L;
    ic2L = 2.0f * v2L - ic2L;

    const float v3R = r - ic2R;
    const float v1R = a1 * ic1R + a2 * v3R;
    const float v2R = ic2R + a2 * ic1R + a3 * v3R;
    ic1R = 2.0f * v1R - ic1R;
    ic2R = 2.0f * v2R - ic2R;

    switch (mode)
    {
        case 2: // Bandpass
            l = v1L;
            r = v1R;
            break;
        case 3: // Highpass
            l = l - k * v1L - v2L;
            r = r - k * v1R - v2R;
            break;
        case 4: // Notch
            l = l - k * v1L;
            r = r - k * v1R;
            break;
        default: // Lowpass (12 or 24dB)
            l = v2L;
            r = v2R;
            break;
    }
}

void PolySynthEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void PolySynthEngine::reset() noexcept
{
    lfoPhaseA_ = 0.0f;
    lfoPhaseB_ = 0.25f;
    droneActive_ = false;
    lastDroneRoot_ = -1;
    lastDroneScale_ = -1;
    activeVoiceCount_.store(0);
    for (auto& v : voices_)
        v = {};
}

void PolySynthEngine::setWavetableA(std::shared_ptr<const WavetableData> wt) noexcept
{
    const juce::SpinLock::ScopedLockType lock(wtLock_);
    wavetableA_ = std::move(wt);
}

std::shared_ptr<const WavetableData> PolySynthEngine::getWavetableA() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(wtLock_);
    return wavetableA_;
}

void PolySynthEngine::setWavetableB(std::shared_ptr<const WavetableData> wt) noexcept
{
    const juce::SpinLock::ScopedLockType lock(wtLock_);
    wavetableB_ = std::move(wt);
}

std::shared_ptr<const WavetableData> PolySynthEngine::getWavetableB() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(wtLock_);
    return wavetableB_;
}

void PolySynthEngine::setSlicePointsA(juce::Point<float> a, juce::Point<float> b) noexcept
{
    sliceA1X_.store(std::clamp(a.x, 0.02f, 0.98f));
    sliceA1Y_.store(std::clamp(a.y, 0.02f, 0.98f));
    sliceB1X_.store(std::clamp(b.x, 0.02f, 0.98f));
    sliceB1Y_.store(std::clamp(b.y, 0.02f, 0.98f));
}

void PolySynthEngine::setSlicePointsB(juce::Point<float> a, juce::Point<float> b) noexcept
{
    sliceA2X_.store(std::clamp(a.x, 0.02f, 0.98f));
    sliceA2Y_.store(std::clamp(a.y, 0.02f, 0.98f));
    sliceB2X_.store(std::clamp(b.x, 0.02f, 0.98f));
    sliceB2Y_.store(std::clamp(b.y, 0.02f, 0.98f));
}

void PolySynthEngine::setTextureCloudCenter(juce::Point<float> center) noexcept
{
    cloudX_.store(std::clamp(center.x, 0.05f, 0.95f));
    cloudY_.store(std::clamp(center.y, 0.05f, 0.95f));
}

void PolySynthEngine::setSustainPedal(bool down) noexcept
{
    sustainPedal_.store(down);
    if (!down)
    {
        for (auto& v : voices_)
        {
            if (v.active && !v.isDroneVoice && v.sustained)
            {
                v.sustained = false;
                v.ampEnv.release();
                v.filEnv.release();
            }
        }
    }
}

void PolySynthEngine::noteOn(int midiNote, float velocity) noexcept
{
    const int clampedNote = std::clamp(midiNote, 12, 115);

    int targetIdx = -1;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& v = voices_[static_cast<size_t>(i)];
        if (v.active && !v.isDroneVoice && v.midiNote == clampedNote)
        {
            targetIdx = i;
            break;
        }
    }

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

    if (targetIdx < 0)
    {
        float minEnv = 999.0f;
        targetIdx = 0;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            const auto& v = voices_[static_cast<size_t>(i)];
            const float score = v.ampEnv.level + (v.isDroneVoice ? 2.0f : 0.0f);
            if (score < minEnv)
            {
                minEnv = score;
                targetIdx = i;
            }
        }
    }

    auto& v = voices_[static_cast<size_t>(targetIdx)];
    const bool wasInactive = !v.active;
    v.active = true;
    v.isDroneVoice = false;
    v.sustained = false;
    v.midiNote = clampedNote;
    v.targetFreqHz = ScaleQuantizer::midiNoteToFrequency(clampedNote);
    if (wasInactive)
    {
        v.currentFreqHz = lastTargetFreqHz_;
        if (v.currentFreqHz <= 20.0f)
            v.currentFreqHz = v.targetFreqHz;
        v.svf1.reset();
        v.svf2.reset();
        for (int u = 0; u < kMaxUnison; ++u)
        {
            v.phaseA[static_cast<size_t>(u)] = static_cast<float>(u) * 0.137f;
            v.phaseB[static_cast<size_t>(u)] = static_cast<float>(u) * 0.193f;
        }
        v.subPhase = 0.0f;
        for (int t = 0; t < kNumTexturePartials; ++t)
            v.texPhase[static_cast<size_t>(t)] = static_cast<float>(t) * 0.11f;
    }
    lastTargetFreqHz_ = v.targetFreqHz;
    v.velocity = std::clamp(velocity, 0.05f, 1.0f);
    v.transientEnv = 1.0f;
    v.ampEnv.trigger();
    v.filEnv.trigger();
}

void PolySynthEngine::noteOff(int midiNote) noexcept
{
    const bool sustain = sustainPedal_.load();
    for (auto& v : voices_)
    {
        if (v.active && !v.isDroneVoice && v.midiNote == midiNote)
        {
            if (sustain)
            {
                v.sustained = true;
            }
            else
            {
                v.ampEnv.release();
                v.filEnv.release();
            }
        }
    }
}

void PolySynthEngine::allNotesOff() noexcept
{
    sustainPedal_.store(false);
    for (auto& v : voices_)
    {
        if (v.active)
        {
            v.sustained = false;
            v.ampEnv.release();
            v.filEnv.release();
        }
    }
}

float PolySynthEngine::sampleTable(const WavetableData& wt, float framePos, float phase) const noexcept
{
    const float fScaled = std::clamp(framePos, 0.0f, 1.0f) * static_cast<float>(WavetableData::kNumFrames - 1);
    const int f0 = static_cast<int>(std::floor(fScaled));
    const int f1 = std::min(f0 + 1, WavetableData::kNumFrames - 1);
    const float ft = fScaled - static_cast<float>(f0);

    const float wrappedPhase = phase - std::floor(phase);
    const float sScaled = wrappedPhase * static_cast<float>(WavetableData::kSamplesPerFrame);
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

void PolySynthEngine::updateDroneChord(bool droneHold, int rootNote, int scaleIndex) noexcept
{
    if (!droneHold)
    {
        if (droneActive_)
        {
            for (auto& v : voices_)
            {
                if (v.isDroneVoice)
                {
                    v.ampEnv.release();
                    v.filEnv.release();
                }
            }
            droneActive_ = false;
        }
        return;
    }

    if (droneActive_ && rootNote == lastDroneRoot_ && scaleIndex == lastDroneScale_)
        return;

    for (auto& v : voices_)
    {
        if (v.isDroneVoice)
        {
            v.ampEnv.release();
            v.filEnv.release();
        }
    }

    const auto& intervals = ScaleQuantizer::getScaleIntervals(scaleIndex);
    const int nDeg = static_cast<int>(intervals.size());
    const int rootMidi = 48 + std::clamp(rootNote, 0, 11);

    const int chordNotes[4] = {
        rootMidi - 12,
        rootMidi,
        rootMidi + intervals[static_cast<size_t>(2 % nDeg)],
        rootMidi + 12 + intervals[static_cast<size_t>(4 % nDeg)]
    };

    for (int c = 0; c < 4; ++c)
    {
        auto& v = voices_[static_cast<size_t>(kMaxVoices - 1 - c)];
        v.active = true;
        v.isDroneVoice = true;
        v.sustained = false;
        v.midiNote = chordNotes[c];
        v.targetFreqHz = ScaleQuantizer::midiNoteToFrequency(chordNotes[c]);
        v.currentFreqHz = v.targetFreqHz;
        v.velocity = (c == 0) ? 0.55f : 0.48f;
        v.transientEnv = 0.4f;
        v.ampEnv.trigger();
        v.filEnv.trigger();
    }

    droneActive_ = true;
    lastDroneRoot_ = rootNote;
    lastDroneScale_ = scaleIndex;
}

void PolySynthEngine::renderBlock(const SynestheticGrid* grid,
                                  float* outLeft,
                                  float* outRight,
                                  int numSamples,
                                  const SynthParams& params) noexcept
{
    if (numSamples <= 0)
        return;

    updateDroneChord(params.droneHold, params.rootNote, params.scaleType);

    auto wtA = getWavetableA();
    auto wtB = getWavetableB();
    if (!wtA && !wtB)
        return;
    if (!wtA) wtA = wtB;
    if (!wtB) wtB = wtA;

    const float sr = static_cast<float>(sampleRate_);
    const float invSr = 1.0f / sr;

    if (grid != nullptr)
    {
        lastCloudStats_ = grid->sampleRegionStats({ cloudX_.load(), cloudY_.load() }, params.textureRadius);
    }

    const float pitchBendMult = std::pow(2.0f, pitchBendSemitones_.load() / 12.0f);
    const float pitchMultA = std::pow(2.0f,
        static_cast<float>(params.oscAOctave)
        + static_cast<float>(params.oscASemitone) / 12.0f
        + params.oscAFineCents / 1200.0f) * pitchBendMult;
    const float pitchMultB = std::pow(2.0f,
        static_cast<float>(params.oscBOctave)
        + static_cast<float>(params.oscBSemitone) / 12.0f
        + params.oscBFineCents / 1200.0f) * pitchBendMult;
    const float subMult = (params.subOctave == 1) ? 0.25f : 0.5f;

    const float glideCoeff = (params.glideTime > 0.002f)
                           ? (1.0f - std::exp(-1.0f / (params.glideTime * sr)))
                           : 1.0f;
    const float transientDecay = std::exp(-1.0f / (0.018f * sr));
    const float lfoIncA = std::clamp(params.oscALfoRate, 0.02f, 12.0f) * invSr;
    const float lfoIncB = std::clamp(params.oscBLfoRate, 0.02f, 12.0f) * invSr;

    const int uniCountA = std::clamp(params.oscAUnison, 1, kMaxUnison);
    const int uniCountB = std::clamp(params.oscBUnison, 1, kMaxUnison);
    const float uniGainA = 1.0f / std::sqrt(static_cast<float>(uniCountA));
    const float uniGainB = 1.0f / std::sqrt(static_cast<float>(uniCountB));

    const float exprGain = std::clamp(0.25f + 0.75f * expression_.load(), 0.2f, 1.0f);
    const float modWheelVal = modWheel_.load();
    const float aftertouchVal = aftertouch_.load();

    // Sample average voice Filter Env for Color Mod Matrix
    float avgFilEnv = 0.0f;
    int runningCount = 0;
    for (const auto& v : voices_)
    {
        if (v.active)
        {
            avgFilEnv += v.filEnv.level;
            ++runningCount;
        }
    }
    if (runningCount > 0)
        avgFilEnv /= static_cast<float>(runningCount);

    const float baseMorphA = std::clamp(params.oscAMorph, 0.0f, 1.0f);
    const float baseMorphB = std::clamp(params.oscBMorph, 0.0f, 1.0f);
    const int frameIdxA = std::clamp(static_cast<int>(baseMorphA * (WavetableData::kNumFrames - 1)), 0, WavetableData::kNumFrames - 1);
    const int frameIdxB = std::clamp(static_cast<int>(baseMorphB * (WavetableData::kNumFrames - 1)), 0, WavetableData::kNumFrames - 1);
    const PixelSample& colA = wtA->frameColors[static_cast<size_t>(frameIdxA)];
    const PixelSample& colB = wtB->frameColors[static_cast<size_t>(frameIdxB)];

    const float modSources[15] = {
        0.0f,
        colA.luminance, colA.hue, colA.saturation, colA.edge,
        colB.luminance, colB.hue, colB.saturation, colB.edge,
        lastCloudStats_.mean.luminance, lastCloudStats_.mean.saturation, lastCloudStats_.mean.edge,
        modWheelVal, aftertouchVal, avgFilEnv
    };

    float modDest[10] = {};
    for (int m = 0; m < 4; ++m)
    {
        const auto& slot = params.modSlots[static_cast<size_t>(m)];
        if (slot.source > 0 && slot.source < 15 && slot.destination >= 0 && slot.destination < 10)
        {
            const float val = modSources[slot.source] * slot.amount;
            modDest[slot.destination] += val;
            lastSlotModOutputs_[static_cast<size_t>(m)] = std::clamp(val, -1.0f, 1.0f);
        }
        else
        {
            lastSlotModOutputs_[static_cast<size_t>(m)] = 0.0f;
        }
    }

    const float effDetuneA = std::clamp(params.oscADetune + modDest[4] * 0.35f, 0.0f, 1.0f);
    const float effDetuneB = std::clamp(params.oscBDetune + modDest[4] * 0.35f, 0.0f, 1.0f);
    const float effFmBtoA = std::clamp(params.oscBFmToA + modDest[2] * 0.65f, 0.0f, 1.0f);
    const float effFoldA = std::clamp(params.oscAWavefold + modDest[3] * 0.75f, 0.0f, 1.0f);
    const float effTexLevel = std::clamp(params.textureLevel + modDest[5] * 0.65f, 0.0f, 1.0f);
    const float effTexShimmer = std::clamp(params.textureShimmer + modDest[6] * 0.75f, 0.0f, 1.0f);
    const float effReso = std::clamp(params.filterReso + modDest[8] * 0.55f, 0.0f, 0.94f);
    const float panOffset = std::clamp(modDest[9] * 0.45f, -0.45f, 0.45f);
    const float cutoffModMult = std::pow(2.0f, modDest[7] * 3.0f);

    static constexpr float kHarmonicRatios[kNumTexturePartials] = {
        1.0f, 2.0f, 3.0f, 4.0f, 1.5f, 2.5f, 5.0f, 6.0f
    };

    float curMorphA = baseMorphA;
    float curMorphB = baseMorphB;

    for (int s = 0; s < numSamples; ++s)
    {
        lfoPhaseA_ += lfoIncA;
        if (lfoPhaseA_ >= 1.0f) lfoPhaseA_ -= 1.0f;
        lfoPhaseB_ += lfoIncB;
        if (lfoPhaseB_ >= 1.0f) lfoPhaseB_ -= 1.0f;

        const float lfoA = std::sin(lfoPhaseA_ * juce::MathConstants<float>::twoPi);
        const float lfoB = std::sin(lfoPhaseB_ * juce::MathConstants<float>::twoPi);

        curMorphA = std::clamp(baseMorphA + lfoA * params.oscALfoDepth * 0.45f + modDest[0] * 0.5f, 0.0f, 1.0f);
        curMorphB = std::clamp(baseMorphB + lfoB * params.oscBLfoDepth * 0.45f + modDest[1] * 0.5f, 0.0f, 1.0f);

        float sumL = 0.0f;
        float sumR = 0.0f;
        int activeNow = 0;

        for (auto& v : voices_)
        {
            if (!v.active)
                continue;

            const float amp = v.ampEnv.step(sr, params.ampAttack, params.ampDecay, params.ampSustain, params.ampRelease);
            const float fEnv = v.filEnv.step(sr, params.filAttack, params.filDecay, params.filSustain, params.filRelease);

            if (v.ampEnv.stage == EnvStage::Idle)
            {
                v.active = false;
                continue;
            }
            ++activeNow;

            v.currentFreqHz += glideCoeff * (v.targetFreqHz - v.currentFreqHz);
            v.transientEnv *= transientDecay;

            const float baseIncA = (v.currentFreqHz * pitchMultA) * invSr;
            const float baseIncB = (v.currentFreqHz * pitchMultB) * invSr;

            // 1. Oscillator B (can modulate Osc A via FM)
            float oscBL = 0.0f, oscBR = 0.0f, oscBMono = 0.0f;
            for (int u = 0; u < uniCountB; ++u)
            {
                const float normU = (uniCountB == 1) ? 0.0f
                    : (static_cast<float>(u) / static_cast<float>(uniCountB - 1)) * 2.0f - 1.0f;
                const float detMult = 1.0f + normU * effDetuneB * 0.028f;
                float& ph = v.phaseB[static_cast<size_t>(u)];
                ph += baseIncB * detMult;
                if (ph >= 1.0f) ph -= 1.0f;

                const float sig = sampleTable(*wtB, curMorphB, ph);
                oscBMono += sig;
                const float pan = std::clamp(0.5f + normU * params.oscASpread * 0.42f + panOffset, 0.05f, 0.95f);
                oscBL += sig * (1.0f - pan);
                oscBR += sig * pan;
            }
            oscBL *= uniGainB * params.oscBLevel;
            oscBR *= uniGainB * params.oscBLevel;
            oscBMono *= uniGainB;

            // 2. Oscillator A (with Phase Modulation from Osc B & Harmonic Wavefolder)
            float oscAL = 0.0f, oscAR = 0.0f;
            const float fmOffset = oscBMono * effFmBtoA * 0.35f;
            for (int u = 0; u < uniCountA; ++u)
            {
                const float normU = (uniCountA == 1) ? 0.0f
                    : (static_cast<float>(u) / static_cast<float>(uniCountA - 1)) * 2.0f - 1.0f;
                const float detMult = 1.0f + normU * effDetuneA * 0.032f;
                float& ph = v.phaseA[static_cast<size_t>(u)];
                ph += baseIncA * detMult;
                if (ph >= 1.0f) ph -= 1.0f;

                float sig = sampleTable(*wtA, curMorphA, ph + fmOffset);
                if (effFoldA > 0.005f)
                {
                    const float folded = std::sin(sig * (1.0f + effFoldA * 3.8f) * juce::MathConstants<float>::halfPi);
                    sig = sig * (1.0f - effFoldA * 0.65f) + folded * (effFoldA * 0.75f);
                }

                const float pan = std::clamp(0.5f + normU * params.oscASpread * 0.46f + panOffset, 0.05f, 0.95f);
                oscAL += sig * (1.0f - pan);
                oscAR += sig * pan;
            }
            oscAL *= uniGainA * params.oscALevel;
            oscAR *= uniGainA * params.oscALevel;

            // 3. Sub-oscillator & Sobel Edge Transient
            v.subPhase += (v.currentFreqHz * subMult * pitchBendMult) * invSr;
            if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
            const float subSig = std::sin(v.subPhase * juce::MathConstants<float>::twoPi) * params.subLevel * 0.45f;

            const float edgeAmt = std::clamp(0.25f + colA.edge * 1.1f + lastCloudStats_.mean.edge * 0.5f, 0.1f, 1.4f);
            const float transientSig = v.transientEnv * params.edgeTransient * edgeAmt
                                     * std::sin(v.subPhase * 8.0f * juce::MathConstants<float>::twoPi) * 0.35f;

            // 4. 2D Granular / Spectral Image Texture Cloud
            float texL = 0.0f, texR = 0.0f;
            if (effTexLevel > 0.005f)
            {
                const int activePartials = std::clamp(2 + static_cast<int>(params.textureDensity * 6.0f), 2, kNumTexturePartials);
                const float cloudDetune = 1.0f + 0.006f * lastCloudStats_.colorVariance;
                for (int t = 0; t < activePartials; ++t)
                {
                    const float ratio = (t >= 4 && effTexShimmer > 0.05f)
                                      ? kHarmonicRatios[t] * (1.0f + effTexShimmer * 0.5f)
                                      : kHarmonicRatios[t];
                    const float tiltWeight = (params.textureTilt >= 0.0f)
                        ? (1.0f - params.textureTilt * 0.4f + params.textureTilt * (static_cast<float>(t) / 7.0f))
                        : (1.0f + params.textureTilt * 0.55f * (static_cast<float>(t) / 7.0f));
                    const float harmAmp = (0.25f + 0.75f * lastCloudStats_.harmonicProfile[static_cast<size_t>(t)])
                                        * std::clamp(tiltWeight, 0.15f, 1.4f);

                    float& tp = v.texPhase[static_cast<size_t>(t)];
                    tp += (v.currentFreqHz * pitchBendMult * ratio * (t % 2 == 0 ? cloudDetune : 2.0f - cloudDetune)) * invSr;
                    if (tp >= 1.0f) tp -= 1.0f;

                    const float pSig = std::sin(tp * juce::MathConstants<float>::twoPi) * harmAmp;
                    const float pPan = (t % 2 == 0) ? 0.30f : 0.70f;
                    texL += pSig * (1.0f - pPan);
                    texR += pSig * pPan;
                }
                const float texScale = (effTexLevel * 0.42f) / std::sqrt(static_cast<float>(activePartials));
                texL *= texScale;
                texR *= texScale;
            }

            float voiceL = oscAL + oscBL + subSig + transientSig + texL;
            float voiceR = oscAR + oscBR + subSig + transientSig + texR;

            // Pre-Filter Drive
            if (params.filterDrive > 0.01f)
            {
                const float drv = 1.0f + params.filterDrive * 3.5f;
                voiceL = std::tanh(voiceL * drv);
                voiceR = std::tanh(voiceR * drv);
            }

            // Per-voice Multimode Cytomic SVF (with Filter ADSR & Key Tracking)
            const float keyTrackOct = ((static_cast<float>(v.midiNote) - 60.0f) / 12.0f) * params.filterKeyTrack;
            const float envOct = fEnv * params.filterEnvAmount * 4.5f;
            const float cutHz = std::clamp(
                params.filterCutoff * cutoffModMult * std::pow(2.0f, keyTrackOct + envOct),
                35.0f, sr * 0.44f);
            const float g = std::tan(juce::MathConstants<float>::pi * cutHz * invSr);
            const float k = 2.0f - 1.85f * effReso;

            v.svf1.processSample(voiceL, voiceR, g, k, params.filterMode);
            if (params.filterMode == 0) // LP 24dB uses 2nd stage
                v.svf2.processSample(voiceL, voiceR, g, k * 0.85f, 0);

            const float velGain = (1.0f - params.velSensitivity) + params.velSensitivity * v.velocity;
            const float finalGain = amp * velGain * exprGain * 0.28f;

            sumL += voiceL * finalGain;
            sumR += voiceR * finalGain;
        }

        outLeft[s] += sumL;
        outRight[s] += sumR;
        activeVoiceCount_.store(activeNow);
    }

    activeMorphA_.store(curMorphA);
    activeMorphB_.store(curMorphB);
}

} // namespace soundcanvas
