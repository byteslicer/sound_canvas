#pragma once

#include <JuceHeader.h>
#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include "control/ImageMidiControlEngine.h"
#include "control/PreviewSynthEngine.h"
#include <array>
#include <atomic>
#include <functional>
#include <memory>

namespace soundcanvas
{

class SoundCanvasControlAudioProcessor : public juce::AudioProcessor
{
public:
    SoundCanvasControlAudioProcessor();
    ~SoundCanvasControlAudioProcessor() override;

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

    bool loadPaintingFromFile(const juce::File& file, juce::String& errorOut);
    void loadStarterPreset(int presetIndex);
    int getCurrentPresetIndex() const noexcept { return currentPresetIndex_.load(); }

    std::shared_ptr<const SynestheticGrid> getCurrentGrid() const noexcept;

    // Thread-safe Rack Document API (paths + left-to-right module chains)
    RackDocument getDocumentCopy() const;
    void updateDocument(const std::function<void(RackDocument&)>& mutator);
    uint32_t getDocumentRevision() const noexcept { return documentRevision_.load(std::memory_order_acquire); }

    ImageMidiControlEngine& getEngine() noexcept { return engine_; }
    const ImageMidiControlEngine& getEngine() const noexcept { return engine_; }

    void triggerPanic() noexcept { panicRequested_.store(true, std::memory_order_release); }

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts_; }

    std::array<float, ImageMidiControlEngine::kNumMacroOutputs> getMacroOutputs() const noexcept;
    double getActiveBpm() const noexcept { return activeBpm_.load(std::memory_order_relaxed); }

private:
    void cacheParameterPointers();

    juce::AudioProcessorValueTreeState apvts_;

    mutable juce::SpinLock gridLock_;
    std::shared_ptr<const SynestheticGrid> currentGrid_;
    juce::String customImagePath_;
    std::atomic<int> currentPresetIndex_ { 0 };

    mutable juce::SpinLock docEditLock_;
    RackDocument document_ {};
    std::atomic<uint32_t> documentRevision_ { 1 };

    ImageMidiControlEngine engine_;
    PreviewSynthEngine previewSynth_;
    std::atomic<bool> panicRequested_ { false };
    std::atomic<double> activeBpm_ { 120.0 };

    std::atomic<float>* rootNotePtr_ = nullptr;
    std::atomic<float>* scaleTypePtr_ = nullptr;
    std::atomic<float>* internalBpmPtr_ = nullptr;
    std::atomic<float>* runningPtr_ = nullptr;
    std::atomic<float>* hostSyncPtr_ = nullptr;
    std::atomic<float>* previewSoundModePtr_ = nullptr;

    std::array<juce::RangedAudioParameter*, ImageMidiControlEngine::kNumMacroOutputs> macroParams_ {};
    std::array<float, ImageMidiControlEngine::kNumMacroOutputs> lastMacroSent_ {};
    std::array<std::atomic<float>, ImageMidiControlEngine::kNumMacroOutputs> macroOutputs_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundCanvasControlAudioProcessor)
};

} // namespace soundcanvas
