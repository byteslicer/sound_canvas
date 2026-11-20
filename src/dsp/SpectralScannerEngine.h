#pragma once

#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include <array>
#include <atomic>

namespace soundcanvas
{

class SpectralScannerEngine
{
public:
    static constexpr int kNumBands = 48;

    struct BandVisualState
    {
        float normY = 0.0f;
        float amplitude = 0.0f;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setPlayheadX(float normX) noexcept { playheadX_.store(std::clamp(normX, 0.0f, 1.0f)); }
    float getPlayheadX() const noexcept { return playheadX_.load(); }

    void renderBlock(const SynestheticGrid* grid,
                     float* outLeft,
                     float* outRight,
                     int numSamples,
                     float scanSpeedHz,
                     int rootNote,
                     int scaleIndex,
                     const std::array<bool, 128>& heldMidiNotes,
                     bool anyMidiHeld,
                     float level) noexcept;

    std::array<BandVisualState, kNumBands> getVisualBands() const noexcept { return visualBands_; }

private:
    struct BandState
    {
        float phase = 0.0f;
        float detunePhase = 0.0f;
        float freqHz = 220.0f;
        float currentAmp = 0.0f;
        float targetAmp = 0.0f;
        float hue = 0.0f;
        float saturation = 0.0f;
        float pan = 0.5f;
    };

    double sampleRate_ = 48000.0;
    std::atomic<float> playheadX_ { 0.15f };
    int controlCounter_ = 0;
    std::array<BandState, kNumBands> bands_ {};
    std::array<BandVisualState, kNumBands> visualBands_ {};
};

} // namespace soundcanvas
