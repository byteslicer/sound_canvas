#pragma once

#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include <array>

namespace soundcanvas
{

class OrbiterEngine
{
public:
    static constexpr int kNumOrbiters = 4;
    static constexpr int kNumVoices = 12;
    static constexpr int kTrailLength = 20;

    enum MotionMode
    {
        Bounce = 0,
        LissajousOrbit = 1,
        BrushstrokeDrift = 2
    };

    struct OrbiterVisualState
    {
        float x = 0.5f;
        float y = 0.5f;
        float anchorX = 0.5f;
        float anchorY = 0.5f;
        float r = 0.9f;
        float g = 0.7f;
        float b = 0.3f;
        float triggerPulse = 0.0f; // [0.0, 1.0] decays after note trigger for ripple animation
        int lastMidiNote = 60;
        std::array<juce::Point<float>, kTrailLength> trail {};
        int trailWriteIndex = 0;
    };

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setOrbiterPosition(int index, float normX, float normY) noexcept;
    std::array<OrbiterVisualState, kNumOrbiters> getVisualStates() const noexcept { return visualStates_; }

    void renderBlock(const SynestheticGrid* grid,
                     float* outLeft,
                     float* outRight,
                     int numSamples,
                     float speed,
                     float edgeSensitivity,
                     int motionMode,
                     int rootNote,
                     int scaleIndex,
                     float level) noexcept;

private:
    struct OrbiterAgent
    {
        float x = 0.5f;
        float y = 0.5f;
        float vx = 0.15f;
        float vy = 0.11f;
        float anchorX = 0.5f;
        float anchorY = 0.5f;
        float orbitPhase = 0.0f;
        float cooldownSec = 0.0f;
        float timeSinceLastNoteSec = 0.0f;
        PixelSample prevSample {};
    };

    struct PolyVoice
    {
        bool active = false;
        int midiNote = 60;
        float freqHz = 261.63f;
        float phase = 0.0f;
        float modPhase = 0.0f;
        float env = 0.0f;
        float attackEnv = 0.0f;
        float decayCoeff = 0.9995f;
        float attackCoeff = 0.01f;
        float velocity = 0.5f;
        float pan = 0.5f;
        float warmth = 0.5f;
        float shimmer = 0.3f;
    };

    void triggerVoice(int orbiterIndex,
                      int midiNote,
                      const PixelSample& px,
                      float panX) noexcept;

    double sampleRate_ = 48000.0;
    int stepCounter_ = 0;
    int trailCounter_ = 0;
    int nextVoiceIndex_ = 0;
    std::array<OrbiterAgent, kNumOrbiters> agents_ {};
    std::array<OrbiterVisualState, kNumOrbiters> visualStates_ {};
    std::array<PolyVoice, kNumVoices> voices_ {};
};

} // namespace soundcanvas
