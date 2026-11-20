#include "dsp/SpectralScannerEngine.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

void SpectralScannerEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void SpectralScannerEngine::reset() noexcept
{
    controlCounter_ = 0;
    for (int i = 0; i < kNumBands; ++i)
    {
        auto& b = bands_[static_cast<size_t>(i)];
        b.phase = static_cast<float>(i) * 0.137f;
        b.detunePhase = static_cast<float>(i) * 0.293f;
        b.currentAmp = 0.0f;
        b.targetAmp = 0.0f;
        visualBands_[static_cast<size_t>(i)] = {};
    }
}

void SpectralScannerEngine::renderBlock(const SynestheticGrid* grid,
                                        float* outLeft,
                                        float* outRight,
                                        int numSamples,
                                        float scanSpeedHz,
                                        int rootNote,
                                        int scaleIndex,
                                        const std::array<bool, 128>& heldMidiNotes,
                                        bool anyMidiHeld,
                                        float level) noexcept
{
    if (grid == nullptr || level <= 1.0e-5f || numSamples <= 0)
        return;

    const float invSr = 1.0f / static_cast<float>(sampleRate_);
    // scanSpeedHz is cycles across canvas per minute-scale (0.02 .. 0.5 canvas widths per second)
    const float playheadDelta = (scanSpeedHz * 0.08f) * invSr;
    const float ampSmoothCoeff = 1.0f - std::exp(-1.0f / (0.025f * static_cast<float>(sampleRate_)));
    const float bandNormScale = (1.15f / std::sqrt(static_cast<float>(kNumBands))) * level;

    float curX = playheadX_.load();

    for (int s = 0; s < numSamples; ++s)
    {
        curX += playheadDelta;
        if (curX >= 1.0f)
            curX -= 1.0f;
        else if (curX < 0.0f)
            curX += 1.0f;

        if (controlCounter_ <= 0)
        {
            controlCounter_ = 32;
            for (int i = 0; i < kNumBands; ++i)
            {
                const float normY = static_cast<float>(i) / static_cast<float>(kNumBands - 1);
                const PixelSample px = grid->sampleAt(curX, normY);

                const int midiNote = ScaleQuantizer::quantizeNormYToMidiNote(normY, rootNote, scaleIndex, 36, 4);
                auto& b = bands_[static_cast<size_t>(i)];
                b.freqHz = ScaleQuantizer::midiNoteToFrequency(midiNote);
                b.hue = px.hue;
                b.saturation = px.saturation;
                // Stereo spread based on hue and band index
                b.pan = std::clamp(0.5f + 0.38f * std::sin(px.hue * juce::MathConstants<float>::twoPi + static_cast<float>(i) * 0.3f),
                                   0.1f, 0.9f);

                // Gate out very dark background pixels so bright brushstrokes stand out musically
                const float gatedLum = std::max(0.0f, (px.luminance - 0.14f) / 0.86f);
                float amp = gatedLum * gatedLum * (0.85f + 0.55f * px.edge);

                // High-frequency pink-noise tilt compensation so top octaves are silky, not piercing
                amp *= (1.15f - 0.55f * normY);

                if (anyMidiHeld)
                {
                    const int pitchClass = midiNote % 12;
                    bool matchesHeldPitchClass = false;
                    for (int n = pitchClass; n < 128; n += 12)
                    {
                        if (heldMidiNotes[static_cast<size_t>(n)])
                        {
                            matchesHeldPitchClass = true;
                            break;
                        }
                    }
                    amp *= matchesHeldPitchClass ? 1.85f : 0.22f;
                }

                b.targetAmp = std::clamp(amp, 0.0f, 1.5f);

                auto& vb = visualBands_[static_cast<size_t>(i)];
                vb.normY = normY;
                vb.amplitude = b.targetAmp;
                vb.r = px.r;
                vb.g = px.g;
                vb.b = px.b;
            }
        }
        --controlCounter_;

        float sumL = 0.0f;
        float sumR = 0.0f;

        for (int i = 0; i < kNumBands; ++i)
        {
            auto& b = bands_[static_cast<size_t>(i)];
            b.currentAmp += ampSmoothCoeff * (b.targetAmp - b.currentAmp);
            if (b.currentAmp < 1.0e-5f)
                continue;

            const float phaseInc = b.freqHz * invSr;
            b.phase += phaseInc;
            if (b.phase >= 1.0f)
                b.phase -= 1.0f;

            b.detunePhase += phaseInc * (1.003f + 0.004f * b.saturation);
            if (b.detunePhase >= 1.0f)
                b.detunePhase -= 1.0f;

            const float rad1 = b.phase * juce::MathConstants<float>::twoPi;
            const float rad2 = b.detunePhase * juce::MathConstants<float>::twoPi;

            // Morph waveform continuously by Hue:
            // Warm (Red/Orange: hue near 0 or 1) -> rich soft-saw + fundamental
            // Green/Teal (hue ~ 0.33)            -> lush detuned chorus pad
            // Blue/Violet (hue ~ 0.66)           -> glassy 2-op FM bell
            const float s1 = std::sin(rad1);
            const float warmWave = 0.65f * s1 + 0.25f * std::sin(rad1 * 2.0f) + 0.10f * std::sin(rad1 * 3.0f);
            const float padWave = 0.55f * (s1 + std::sin(rad2));
            const float fmWave = std::sin(rad1 + (0.4f + 0.8f * b.saturation) * std::sin(rad1 * 2.0f));

            const float warmWeight = 0.5f + 0.5f * std::cos(b.hue * juce::MathConstants<float>::twoPi);
            const float coolWeight = 1.0f - warmWeight;
            const float sig = (warmWeight * (0.6f * warmWave + 0.4f * padWave)
                             + coolWeight * (0.5f * padWave + 0.5f * fmWave)) * b.currentAmp;

            sumL += sig * (1.0f - b.pan);
            sumR += sig * b.pan;
        }

        outLeft[s] += sumL * bandNormScale;
        outRight[s] += sumR * bandNormScale;
    }

    playheadX_.store(curX);
}

} // namespace soundcanvas
