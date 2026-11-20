#pragma once

#include <JuceHeader.h>
#include "control/ControlLookAndFeel.h"
#include "control/ControlPluginProcessor.h"
#include "control/ControlCanvasComponent.h"
#include "control/ModuleRackComponent.h"
#include <array>
#include <memory>

namespace soundcanvas
{

class SoundCanvasControlAudioProcessorEditor : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit SoundCanvasControlAudioProcessorEditor(SoundCanvasControlAudioProcessor&);
    ~SoundCanvasControlAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void openPaintingFileChooser();
    void selectPath(int pathIndex);
    void addNewPath();
    void removeSelectedPath();
    void syncPathToolbar();
    void configureBarSlider(juce::Slider& s, const juce::String& suffix);

    SoundCanvasControlAudioProcessor& processor_;
    ControlLookAndFeel lookAndFeel_;
    int selectedPath_ = 0;
    uint32_t lastSyncedDocRev_ = 0;

    // Top studio bar
    juce::TextButton loadPaintingButton_ { "+ Load" };
    juce::ComboBox presetSelector_;
    juce::ComboBox rootNoteBox_;
    juce::ComboBox scaleTypeBox_;
    juce::ToggleButton runningToggle_ { "Run" };
    juce::ToggleButton hostSyncToggle_ { "Sync" };
    juce::Slider internalBpmSlider_;
    juce::ComboBox previewSoundBox_;
    juce::TextButton panicButton_ { "Panic" };

    std::unique_ptr<ComboBoxAttachment> rootNoteAtt_, scaleTypeAtt_, previewSoundAtt_;
    std::unique_ptr<ButtonAttachment> runningAtt_, hostSyncAtt_;
    std::unique_ptr<SliderAttachment> internalBpmAtt_;

    // Path & Shape Toolbar
    std::array<juce::TextButton, RackDocument::kMaxPaths> pathTabButtons_;
    juce::TextButton addPathButton_ { "+ Path" };
    juce::TextButton removePathButton_ { "- Path" };
    std::array<juce::TextButton, 6> shapeButtons_;
    juce::ToggleButton closedLoopToggle_ { "Loop" };
    juce::ToggleButton smoothCurveToggle_ { "Smooth" };

    // Hero Painting Canvas
    ControlCanvasComponent canvasComponent_;

    // Chain Header Strip + Ableton-Style Horizontal Rack
    juce::Rectangle<int> chainHeaderBounds_;
    juce::TextButton addModuleButton_ { "+ Add Module" };
    ModuleRackComponent moduleRack_;

    juce::String statusText_ { "Click painting to add waypoints. Stack & reorder modules left-to-right below." };
    bool statusIsError_ = false;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundCanvasControlAudioProcessorEditor)
};

} // namespace soundcanvas
