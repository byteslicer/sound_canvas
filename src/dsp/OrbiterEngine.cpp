#include "dsp/OrbiterEngine.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

void OrbiterEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void OrbiterEngine::reset() noexcept
{
    stepCounter_ = 0;
    trailCounter_ = 0;
    nextVoiceIndex_ = 0;

    constexpr float initX[kNumOrbiters]  = { 0.24f, 0.72f, 0.34f, 0.66f };
    constexpr float initY[kNumOrbiters]  = { 0.28f, 0.32f, 0.68f, 0.64f };
    constexpr float initVx[kNumOrbiters] = {  0.19f, -0.16f,  0.14f, -0.21f };
    constexpr float initVy[kNumOrbiters] = {  0.14f,  0.18f, -0.17f, -0.13f };

    for (int i = 0; i < kNumOrbiters; ++i)
    {
        auto& a = agents_[static_cast<size_t>(i)];
        a.x = initX[i];
        a.y = initY[i];
        a.anchorX = initX[i];
        a.anchorY = initY[i];
        a.vx = initVx[i];
        a.vy = initVy[i];
        a.orbitPhase = static_cast<float>(i) * 1.57f;
        a.cooldownSec = 0.05f * static_cast<float>(i + 1);
        a.timeSinceLastNoteSec = 0.0f;

        auto& vs = visualStates_[static_cast<size_t>(i)];
        vs.x = a.x;
        vs.y = a.y;
        vs.anchorX = a.anchorX;
        vs.anchorY = a.anchorY;
        vs.triggerPulse = 0.0f;
        vs.trailWriteIndex = 0;
        for (auto& pt : vs.trail)
            pt = { a.x, a.y };
    }

    for (auto& v : voices_)
        v = {};
}

void OrbiterEngine::setOrbiterPosition(int index, float normX, float normY) noexcept
{
    if (index < 0 || index >= kNumOrbiters)
        return;

    const float cx = std::clamp(normX, 0.04f, 0.96f);
    const float cy = std::clamp(normY, 0.04f, 0.96f);

    auto& a = agents_[static_cast<size_t>(index)];
    a.x = cx;
    a.y = cy;
    a.anchorX = cx;
    a.anchorY = cy;

    auto& vs = visualStates_[static_cast<size_t>(index)];
    vs.x = cx;
    vs.y = cy;
    vs.anchorX = cx;
    vs.anchorY = cy;
}

void OrbiterEngine::triggerVoice(int orbiterIndex,
                                 int midiNote,
                                 const PixelSample& px,
                                 float panX) noexcept
{
    auto& v = voices_[static_cast<size_t>(nextVoiceIndex_)];
    nextVoiceIndex_ = (nextVoiceIndex_ + 1) % kNumVoices;

    v.active = true;
    v.midiNote = midiNote;
    v.freqHz = ScaleQuantizer::midiNoteToFrequency(midiNote);
    v.phase = 0.0f;
    v.modPhase = 0.0f;
    v.env = 1.0f;
    v.attackEnv = 0.0f;

    // Sharp brushstroke edges produce plucky transients; smooth areas produce softer bell swells
    const float attackTimeSec = std::clamp(0.035f - 0.031f * px.edge, 0.002f, 0.04f);
    v.attackCoeff = 1.0f - std::exp(-1.0f / (attackTimeSec * static_cast<float>(sampleRate_)));

    const float decayTimeSec = 0.35f + 0.95f * (1.0f - 0.4f * px.edge) + 0.4f * px.saturation;
    v.decayCoeff = std::exp(-1.0f / (decayTimeSec * static_cast<float>(sampleRate_)));

    v.velocity = std::clamp(0.28f + 0.65f * px.luminance + 0.25f * px.edge, 0.2f, 1.0f);
    v.pan = std::clamp(0.15f + 0.70f * panX, 0.1f, 0.9f);
    v.warmth = std::clamp(px.r * 0.8f + px.g * 0.3f, 0.1f, 1.0f);
    v.shimmer = std::clamp(px.b * 0.8f + px.saturation * 0.4f, 0.05f, 1.0f);

    if (orbiterIndex >= 0 && orbiterIndex < kNumOrbiters)
    {
        auto& vs = visualStates_[static_cast<size_t>(orbiterIndex)];
        vs.triggerPulse = 1.0f;
        vs.lastMidiNote = midiNote;
    }
}

void OrbiterEngine::renderBlock(const SynestheticGrid* grid,
                                float* outLeft,
                                float* outRight,
                                int numSamples,
                                float speed,
                                float edgeSensitivity,
                                int motionMode,
                                int rootNote,
                                int scaleIndex,
                                float level) noexcept
{
    if (grid == nullptr || level <= 1.0e-5f || numSamples <= 0)
        return;

    const float invSr = 1.0f / static_cast<float>(sampleRate_);
    constexpr int kControlInterval = 64;
    const float dt = static_cast<float>(kControlInterval) * invSr;
    const float speedScale = std::clamp(speed, 0.05f, 3.0f);

    for (int s = 0; s < numSamples; ++s)
    {
        if (stepCounter_ <= 0)
        {
            stepCounter_ = kControlInterval;
            const bool recordTrail = (++trailCounter_ >= 6);
            if (recordTrail)
                trailCounter_ = 0;

            for (int i = 0; i < kNumOrbiters; ++i)
            {
                auto& a = agents_[static_cast<size_t>(i)];
                auto& vs = visualStates_[static_cast<size_t>(i)];

                a.orbitPhase += dt * speedScale * (1.2f + 0.25f * static_cast<float>(i));
                if (a.orbitPhase > juce::MathConstants<float>::twoPi * 12.0f)
                    a.orbitPhase -= juce::MathConstants<float>::twoPi * 12.0f;

                if (motionMode == LissajousOrbit)
                {
                    const float rx = 0.16f + 0.03f * static_cast<float>(i % 2);
                    const float ry = 0.16f + 0.03f * static_cast<float>((i + 1) % 2);
                    const float freqX = 1.0f + 0.5f * static_cast<float>(i % 3);
                    const float freqY = 1.5f + 0.25f * static_cast<float>(i);
                    a.x = std::clamp(a.anchorX + rx * std::cos(a.orbitPhase * freqX), 0.04f, 0.96f);
                    a.y = std::clamp(a.anchorY + ry * std::sin(a.orbitPhase * freqY), 0.04f, 0.96f);
                }
                else if (motionMode == BrushstrokeDrift)
                {
                    // Sample nearby pixels to drift along vivid brushstrokes
                    const PixelSample rightPx = grid->sampleAt(std::min(1.0f, a.x + 0.02f), 1.0f - a.y);
                    const PixelSample leftPx  = grid->sampleAt(std::max(0.0f, a.x - 0.02f), 1.0f - a.y);
                    const PixelSample downPx  = grid->sampleAt(a.x, 1.0f - std::min(1.0f, a.y + 0.02f));
                    const PixelSample upPx    = grid->sampleAt(a.x, 1.0f - std::max(0.0f, a.y - 0.02f));

                    const float gx = (rightPx.luminance + rightPx.saturation) - (leftPx.luminance + leftPx.saturation);
                    const float gy = (downPx.luminance + downPx.saturation) - (upPx.luminance + upPx.saturation);

                    // Swirl tangent to gradient + gentle orbital curl
                    a.vx = 0.85f * a.vx + 0.15f * (-gy * 1.4f + 0.18f * std::cos(a.orbitPhase));
                    a.vy = 0.85f * a.vy + 0.15f * ( gx * 1.4f + 0.18f * std::sin(a.orbitPhase));

                    a.x += a.vx * dt * speedScale * 0.9f;
                    a.y += a.vy * dt * speedScale * 0.9f;
                    if (a.x < 0.05f || a.x > 0.95f) { a.vx = -a.vx; a.x = std::clamp(a.x, 0.05f, 0.95f); }
                    if (a.y < 0.05f || a.y > 0.95f) { a.vy = -a.vy; a.y = std::clamp(a.y, 0.05f, 0.95f); }
                }
                else // Bounce
                {
                    a.x += a.vx * dt * speedScale * 0.65f;
                    a.y += a.vy * dt * speedScale * 0.65f;
                    if (a.x < 0.04f || a.x > 0.96f)
                    {
                        a.vx = -a.vx;
                        a.x = std::clamp(a.x, 0.04f, 0.96f);
                    }
                    if (a.y < 0.04f || a.y > 0.96f)
                    {
                        a.vy = -a.vy;
                        a.y = std::clamp(a.y, 0.04f, 0.96f);
                    }
                }

                const PixelSample px = grid->sampleAt(a.x, 1.0f - a.y);

                vs.x = a.x;
                vs.y = a.y;
                vs.r = px.r;
                vs.g = px.g;
                vs.b = px.b;
                vs.triggerPulse *= 0.92f;

                if (recordTrail)
                {
                    vs.trail[static_cast<size_t>(vs.trailWriteIndex)] = { a.x, a.y };
                    vs.trailWriteIndex = (vs.trailWriteIndex + 1) % kTrailLength;
                }

                a.cooldownSec = std::max(0.0f, a.cooldownSec - dt);
                a.timeSinceLastNoteSec += dt;

                const float lumDelta = std::abs(px.luminance - a.prevSample.luminance);
                const float hueDiff = std::abs(px.hue - a.prevSample.hue);
                const float hueDelta = std::min(hueDiff, 1.0f - hueDiff);
                const float changeScore = lumDelta * 1.6f + hueDelta * 1.4f + px.edge * 0.55f;

                const float threshold = std::clamp(0.48f - 0.40f * edgeSensitivity, 0.06f, 0.55f);
                const float maxSilenceSec = std::clamp(1.1f / (0.35f + speedScale), 0.25f, 1.6f);

                if (a.cooldownSec <= 0.0f && (changeScore >= threshold || a.timeSinceLastNoteSec >= maxSilenceSec))
                {
                    // Base register shifts slightly per orbiter so the 4 walkers form rich chords/arpeggios
                    const int baseMidi = 48 + (i % 2) * 12;
                    const int midiNote = ScaleQuantizer::quantizeNormYToMidiNote(1.0f - a.y, rootNote, scaleIndex, baseMidi, 3);
                    triggerVoice(i, midiNote, px, a.x);

                    a.cooldownSec = std::clamp(0.24f / (0.4f + speedScale * 0.7f), 0.07f, 0.35f);
                    a.timeSinceLastNoteSec = 0.0f;
                    a.prevSample = px;
                }
            }
        }
        --stepCounter_;

        float sumL = 0.0f;
        float sumR = 0.0f;

        for (auto& v : voices_)
        {
            if (!v.active)
                continue;

            v.attackEnv += v.attackCoeff * (1.0f - v.attackEnv);
            v.env *= v.decayCoeff;
            if (v.env < 1.0e-4f)
            {
                v.active = false;
                continue;
            }

            const float inc = v.freqHz * invSr;
            v.phase += inc;
            if (v.phase >= 1.0f)
                v.phase -= 1.0f;

            v.modPhase += inc * 2.002f;
            if (v.modPhase >= 1.0f)
                v.modPhase -= 1.0f;

            const float rad = v.phase * juce::MathConstants<float>::twoPi;
            const float modRad = v.modPhase * juce::MathConstants<float>::twoPi;

            // Pluck transient + warm harmonic body + crystalline shimmer
            const float fmIndex = v.env * v.env * (0.6f + 1.4f * v.shimmer);
            const float sig = (std::sin(rad + fmIndex * std::sin(modRad))
                            + 0.35f * v.warmth * std::sin(rad * 2.0f)
                            + 0.15f * v.shimmer * std::sin(rad * 3.0f))
                            * (v.env * v.attackEnv * v.velocity * 0.26f);

            sumL += sig * (1.0f - v.pan);
            sumR += sig * v.pan;
        }

        outLeft[s] += sumL * level;
        outRight[s] += sumR * level;
    }
}

} // namespace soundcanvas
