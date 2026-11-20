#pragma once

#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include <array>
#include <atomic>
#include <memory>

namespace soundcanvas
{

struct SynthModSlot
{
    int source = 0;      // 0..14
    int destination = 0; // 0..9
    float amount = 0.0f; // -1.0 .. +1.0
};

struct SynthParams
{
    // Oscillator A (Brushstroke Slice A)
    float oscALevel = 0.72f;
    float oscAMorph = 0.35f;
    float oscALfoRate = 0.35f;
    float oscALfoDepth = 0.28f;
    int oscAOctave = 0;
    int oscASemitone = 0;
    float oscAFineCents = 0.0f;
    int oscAUnison = 3;
    float oscADetune = 0.22f;
    float oscASpread = 0.55f;
    float oscAWavefold = 0.15f;

    // Oscillator B (Brushstroke Slice B)
    float oscBLevel = 0.55f;
    float oscBMorph = 0.50f;
    float oscBLfoRate = 0.22f;
    float oscBLfoDepth = 0.25f;
    int oscBOctave = 0;
    int oscBSemitone = 7;
    float oscBFineCents = 0.0f;
    int oscBUnison = 2;
    float oscBDetune = 0.18f;
    float oscBFmToA = 0.12f;

    // Sub & Edge Transient
    float subLevel = 0.35f;
    int subOctave = 0; // 0 = -1 oct, 1 = -2 oct
    float edgeTransient = 0.30f;

    // 2D Granular / Spectral Image Texture Cloud
    float textureLevel = 0.45f;
    float textureDensity = 0.65f;
    float textureRadius = 0.18f;
    float textureShimmer = 0.40f;
    float textureTilt = 0.10f;

    // Multimode Filter
    int filterMode = 0; // 0=LP24, 1=LP12, 2=BP, 3=HP, 4=Notch
    float filterCutoff = 5400.0f;
    float filterReso = 0.28f;
    float filterDrive = 0.15f;
    float filterKeyTrack = 0.35f;
    float filterEnvAmount = 0.42f;

    // Amp ADSR
    float ampAttack = 0.025f;
    float ampDecay = 0.45f;
    float ampSustain = 0.78f;
    float ampRelease = 0.55f;
    float velSensitivity = 0.70f;

    // Filter ADSR
    float filAttack = 0.03f;
    float filDecay = 0.55f;
    float filSustain = 0.35f;
    float filRelease = 0.60f;

    // Performance & Scale
    float glideTime = 0.02f;
    int rootNote = 0;
    int scaleType = 0;
    bool droneHold = false;

    // 4-Slot Color -> Synth Mod Matrix
    std::array<SynthModSlot, 4> modSlots {};
};

class PolySynthEngine
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxUnison = 7;
    static constexpr int kNumTexturePartials = 8;

    static juce::StringArray getModSourceNames()
    {
        return {
            "None",
            "Slice A Luminance",
            "Slice A Hue",
            "Slice A Saturation",
            "Slice A Edge",
            "Slice B Luminance",
            "Slice B Hue",
            "Slice B Saturation",
            "Slice B Edge",
            "Cloud Luminance",
            "Cloud Saturation",
            "Cloud Edge",
            "Mod Wheel (CC1)",
            "Aftertouch",
            "Filter Env"
        };
    }

    static juce::StringArray getModDestinationNames()
    {
        return {
            "Osc A Morph",
            "Osc B Morph",
            "FM (B -> A)",
            "Osc A Wavefold",
            "Unison Detune",
            "Texture Level",
            "Texture Shimmer",
            "Filter Cutoff",
            "Filter Resonance",
            "Stereo Pan"
        };
    }

    static juce::StringArray getFilterModeNames()
    {
        return { "LP 24dB", "LP 12dB", "Bandpass", "Highpass", "Notch" };
    }

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setWavetableA(std::shared_ptr<const WavetableData> wt) noexcept;
    std::shared_ptr<const WavetableData> getWavetableA() const noexcept;

    void setWavetableB(std::shared_ptr<const WavetableData> wt) noexcept;
    std::shared_ptr<const WavetableData> getWavetableB() const noexcept;

    void setSlicePointsA(juce::Point<float> a, juce::Point<float> b) noexcept;
    juce::Point<float> getSlicePointA1() const noexcept { return { sliceA1X_.load(), sliceA1Y_.load() }; }
    juce::Point<float> getSlicePointB1() const noexcept { return { sliceB1X_.load(), sliceB1Y_.load() }; }

    void setSlicePointsB(juce::Point<float> a, juce::Point<float> b) noexcept;
    juce::Point<float> getSlicePointA2() const noexcept { return { sliceA2X_.load(), sliceA2Y_.load() }; }
    juce::Point<float> getSlicePointB2() const noexcept { return { sliceB2X_.load(), sliceB2Y_.load() }; }

    void setTextureCloudCenter(juce::Point<float> center) noexcept;
    juce::Point<float> getTextureCloudCenter() const noexcept { return { cloudX_.load(), cloudY_.load() }; }

    void setTextureCloudRadius(float r) noexcept { cloudRadius_.store(std::clamp(r, 0.04f, 0.45f)); }
    float getTextureCloudRadius() const noexcept { return cloudRadius_.load(); }

    float getActiveMorphA() const noexcept { return activeMorphA_.load(); }
    float getActiveMorphB() const noexcept { return activeMorphB_.load(); }
    RegionStats getLastCloudStats() const noexcept { return lastCloudStats_; }
    std::array<float, 4> getLastSlotModOutputs() const noexcept { return lastSlotModOutputs_; }

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;

    void setModWheel(float norm) noexcept { modWheel_.store(std::clamp(norm, 0.0f, 1.0f)); }
    void setExpression(float norm) noexcept { expression_.store(std::clamp(norm, 0.0f, 1.0f)); }
    void setAftertouch(float norm) noexcept { aftertouch_.store(std::clamp(norm, 0.0f, 1.0f)); }
    void setPitchBendSemitones(float semi) noexcept { pitchBendSemitones_.store(std::clamp(semi, -12.0f, 12.0f)); }
    void setSustainPedal(bool down) noexcept;

    int getActiveVoiceCount() const noexcept { return activeVoiceCount_.load(); }

    void renderBlock(const SynestheticGrid* grid,
                     float* outLeft,
                     float* outRight,
                     int numSamples,
                     const SynthParams& params) noexcept;

private:
    enum class EnvStage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    struct AdsrState
    {
        EnvStage stage = EnvStage::Idle;
        float level = 0.0f;

        void trigger() noexcept { stage = EnvStage::Attack; }
        void release() noexcept
        {
            if (stage != EnvStage::Idle)
                stage = EnvStage::Release;
        }
        void reset() noexcept
        {
            stage = EnvStage::Idle;
            level = 0.0f;
        }
        float step(float sr, float aSec, float dSec, float sLevel, float rSec) noexcept;
    };

    struct SvfStage
    {
        float ic1L = 0.0f, ic2L = 0.0f;
        float ic1R = 0.0f, ic2R = 0.0f;

        void reset() noexcept { ic1L = ic2L = ic1R = ic2R = 0.0f; }
        void processSample(float& l, float& r, float g, float k, int mode) noexcept;
    };

    struct Voice
    {
        bool active = false;
        bool isDroneVoice = false;
        bool sustained = false;
        int midiNote = 60;
        float targetFreqHz = 261.63f;
        float currentFreqHz = 261.63f;
        float velocity = 0.8f;
        float transientEnv = 0.0f;

        std::array<float, kMaxUnison> phaseA {};
        std::array<float, kMaxUnison> phaseB {};
        float subPhase = 0.0f;
        std::array<float, kNumTexturePartials> texPhase {};

        AdsrState ampEnv;
        AdsrState filEnv;
        SvfStage svf1;
        SvfStage svf2;
    };

    float sampleTable(const WavetableData& wt, float framePos, float phase) const noexcept;
    void updateDroneChord(bool droneHold, int rootNote, int scaleIndex) noexcept;

    double sampleRate_ = 48000.0;
    float lfoPhaseA_ = 0.0f;
    float lfoPhaseB_ = 0.0f;
    bool droneActive_ = false;
    int lastDroneRoot_ = -1;
    int lastDroneScale_ = -1;
    float lastTargetFreqHz_ = 261.63f;

    std::atomic<float> sliceA1X_ { 0.16f }, sliceA1Y_ { 0.68f };
    std::atomic<float> sliceB1X_ { 0.82f }, sliceB1Y_ { 0.32f };
    std::atomic<float> sliceA2X_ { 0.20f }, sliceA2Y_ { 0.28f };
    std::atomic<float> sliceB2X_ { 0.78f }, sliceB2Y_ { 0.74f };
    std::atomic<float> cloudX_ { 0.50f }, cloudY_ { 0.46f };
    std::atomic<float> cloudRadius_ { 0.18f };

    std::atomic<float> activeMorphA_ { 0.35f };
    std::atomic<float> activeMorphB_ { 0.50f };
    std::atomic<float> modWheel_ { 0.0f };
    std::atomic<float> expression_ { 1.0f };
    std::atomic<float> aftertouch_ { 0.0f };
    std::atomic<float> pitchBendSemitones_ { 0.0f };
    std::atomic<bool> sustainPedal_ { false };
    std::atomic<int> activeVoiceCount_ { 0 };

    RegionStats lastCloudStats_ {};
    std::array<float, 4> lastSlotModOutputs_ {};

    mutable juce::SpinLock wtLock_;
    std::shared_ptr<const WavetableData> wavetableA_;
    std::shared_ptr<const WavetableData> wavetableB_;
    std::array<Voice, kMaxVoices> voices_ {};
};

} // namespace soundcanvas
