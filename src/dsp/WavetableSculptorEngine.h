#pragma once

#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include <array>
#include <atomic>
#include <memory>

namespace soundcanvas
{

class WavetableSculptorEngine
{
public:
    static constexpr int kMaxVoices = 8;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setWavetable(std::shared_ptr<const WavetableData> wt) noexcept;
    std::shared_ptr<const WavetableData> getWavetable() const noexcept;

    void setSlicePoints(juce::Point<float> a, juce::Point<float> b) noexcept
    {
        sliceAX_.store(std::clamp(a.x, 0.02f, 0.98f));
        sliceAY_.store(std::clamp(a.y, 0.02f, 0.98f));
        sliceBX_.store(std::clamp(b.x, 0.02f, 0.98f));
        sliceBY_.store(std::clamp(b.y, 0.02f, 0.98f));
    }

    juce::Point<float> getSlicePointA() const noexcept { return { sliceAX_.load(), sliceAY_.load() }; }
    juce::Point<float> getSlicePointB() const noexcept { return { sliceBX_.load(), sliceBY_.load() }; }
    float getActiveMorphPosition() const noexcept { return activeMorphPos_.load(); }
    PixelSample getActiveFrameColor() const noexcept { return activeFrameColor_; }

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;

    void renderBlock(float* outLeft,
                     float* outRight,
                     int numSamples,
                     float morphPosition,
                     float morphLfoDepth,
                     float morphRateHz,
                     bool droneHold,
                     int rootNote,
                     int scaleIndex,
                     float level) noexcept;

private:
    struct SynthVoice
    {
        bool active = false;
        bool releasing = false;
        bool isDroneVoice = false;
        int midiNote = 60;
        float freqHz = 261.63f;
        float velocity = 0.8f;
        float phaseCenter = 0.0f;
        float phaseLeft = 0.0f;
        float phaseRight = 0.0f;
        float env = 0.0f;
    };

    float sampleTable(const WavetableData& wt, float framePos, float phase) const noexcept;
    void updateDroneChord(bool droneHold, int rootNote, int scaleIndex) noexcept;

    double sampleRate_ = 48000.0;
    float lfoPhase_ = 0.0f;
    bool droneActive_ = false;
    int lastDroneRoot_ = -1;
    int lastDroneScale_ = -1;

    std::atomic<float> sliceAX_ { 0.18f };
    std::atomic<float> sliceAY_ { 0.65f };
    std::atomic<float> sliceBX_ { 0.82f };
    std::atomic<float> sliceBY_ { 0.35f };
    std::atomic<float> activeMorphPos_ { 0.5f };

    PixelSample activeFrameColor_ {};
    mutable juce::SpinLock wtLock_;
    std::shared_ptr<const WavetableData> wavetable_;
    std::array<SynthVoice, kMaxVoices> voices_ {};
};

} // namespace soundcanvas
