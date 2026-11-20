#pragma once

#include <JuceHeader.h>
#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include "synth/PolySynthEngine.h"
#include "synth/SynthMasterEffects.h"
#include <array>
#include <atomic>
#include <memory>

namespace soundcanvas
{

class SoundCanvasSynthAudioProcessor : public juce::AudioProcessor
{
public:
    SoundCanvasSynthAudioProcessor();
    ~SoundCanvasSynthAudioProcessor() override;

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

    // Artwork & Synesthetic Synth Controls
    bool loadPaintingFromFile(const juce::File& file, juce::String& errorOut);
    void loadStarterPreset(int presetIndex);
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex_.load(); }

    std::shared_ptr<const SynestheticGrid> getCurrentGrid() const noexcept;

    void updateWavetableSliceA(juce::Point<float> a, juce::Point<float> b);
    void updateWavetableSliceB(juce::Point<float> a, juce::Point<float> b);
    void updateTextureCloudCenter(juce::Point<float> center);

    PolySynthEngine& getSynthEngine() noexcept { return synthEngine_; }
    const PolySynthEngine& getSynthEngine() const noexcept { return synthEngine_; }

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts_; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState_; }

    static constexpr int kScopeSize = 256;
    std::array<float, kScopeSize> getScopeSnapshot() const noexcept;
    std::array<float, kScopeSize> getScopeSnapshotRight() const noexcept;
    float getOutputRms() const noexcept { return outputRms_.load(); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    juce::MidiKeyboardState keyboardState_;

    mutable juce::SpinLock gridLock_;
    std::shared_ptr<const SynestheticGrid> currentGrid_;
    juce::String customImagePath_;
    std::atomic<int> currentPresetIndex_ { 0 };

    PolySynthEngine synthEngine_;
    SynthMasterEffects masterEffects_;

    std::array<int, 128> noteSnapMap_ {};
    std::atomic<float> cc74Offset_ { 0.0f };
    std::atomic<float> cc71Offset_ { 0.0f };

    std::array<std::atomic<float>, kScopeSize> scopeBuffer_ {};
    std::array<std::atomic<float>, kScopeSize> scopeBufferRight_ {};
    int scopeWritePos_ = 0;
    std::atomic<float> outputRms_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundCanvasSynthAudioProcessor)
};

} // namespace soundcanvas
