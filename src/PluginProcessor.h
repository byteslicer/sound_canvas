#pragma once

#include <JuceHeader.h>
#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/SpectralScannerEngine.h"
#include "dsp/OrbiterEngine.h"
#include "dsp/WavetableSculptorEngine.h"
#include "dsp/MasterEffects.h"
#include <array>
#include <atomic>
#include <memory>

namespace soundcanvas
{

class SoundCanvasAudioProcessor : public juce::AudioProcessor
{
public:
    SoundCanvasAudioProcessor();
    ~SoundCanvasAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Artwork & Synesthetic Engine Controls
    bool loadPaintingFromFile(const juce::File& file, juce::String& errorOut);
    void loadStarterPreset(int presetIndex);
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex_.load(); }

    std::shared_ptr<const SynestheticGrid> getCurrentGrid() const noexcept;
    void updateWavetableSlice(juce::Point<float> pointA, juce::Point<float> pointB);

    SpectralScannerEngine& getScannerEngine() noexcept { return scannerEngine_; }
    const SpectralScannerEngine& getScannerEngine() const noexcept { return scannerEngine_; }

    OrbiterEngine& getOrbiterEngine() noexcept { return orbiterEngine_; }
    const OrbiterEngine& getOrbiterEngine() const noexcept { return orbiterEngine_; }

    WavetableSculptorEngine& getWavetableEngine() noexcept { return wavetableEngine_; }
    const WavetableSculptorEngine& getWavetableEngine() const noexcept { return wavetableEngine_; }

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts_; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState_; }

    static constexpr int kScopeSize = 256;
    std::array<float, kScopeSize> getScopeSnapshot() const noexcept { return scopeBuffer_; }
    float getOutputRms() const noexcept { return outputRms_.load(); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    juce::MidiKeyboardState keyboardState_;

    mutable juce::SpinLock gridLock_;
    std::shared_ptr<const SynestheticGrid> currentGrid_;
    juce::String customImagePath_;
    std::atomic<int> currentPresetIndex_ { 0 };

    SpectralScannerEngine scannerEngine_;
    OrbiterEngine orbiterEngine_;
    WavetableSculptorEngine wavetableEngine_;
    MasterEffects masterEffects_;

    std::array<bool, 128> heldMidiNotes_ {};
    std::array<float, kScopeSize> scopeBuffer_ {};
    int scopeWritePos_ = 0;
    std::atomic<float> outputRms_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundCanvasAudioProcessor)
};

} // namespace soundcanvas
