#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/PaintingCanvasComponent.h"
#include <memory>

namespace soundcanvas
{

class SoundCanvasLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SoundCanvasLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;
};

class SoundCanvasAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit SoundCanvasAudioProcessorEditor(SoundCanvasAudioProcessor&);
    ~SoundCanvasAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openPaintingFileChooser();
    void configureRotarySlider(juce::Slider& slider,
                               juce::Label& label,
                               const juce::String& text,
                               juce::Colour accentColour);

    SoundCanvasAudioProcessor& processor_;
    SoundCanvasLookAndFeel lookAndFeel_;

    PaintingCanvasComponent canvasComponent_;
    juce::MidiKeyboardComponent midiKeyboard_;

    // Top bar controls
    juce::TextButton loadPaintingButton_ { "Load Painting..." };
    juce::ComboBox presetSelector_;
    juce::ComboBox rootNoteBox_;
    juce::ComboBox scaleTypeBox_;
    juce::ToggleButton droneHoldButton_ { "Drone Hold" };
    juce::Label statusLabel_;

    // Right panel rotary sliders
    juce::Slider scannerMixSlider_, orbiterMixSlider_, wavetableMixSlider_;
    juce::Slider scanSpeedSlider_, edgeSensSlider_;
    juce::Slider wtMorphSlider_, wtLfoSlider_;
    juce::Slider cutoffSlider_, resoSlider_, reverbSlider_;
    juce::ComboBox orbiterModeBox_;

    juce::Label scannerMixLabel_, orbiterMixLabel_, wavetableMixLabel_;
    juce::Label scanSpeedLabel_, edgeSensLabel_;
    juce::Label wtMorphLabel_, wtLfoLabel_;
    juce::Label cutoffLabel_, resoLabel_, reverbLabel_;
    juce::Label orbiterModeLabel_;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> scannerMixAtt_, orbiterMixAtt_, wavetableMixAtt_;
    std::unique_ptr<SliderAttachment> scanSpeedAtt_, edgeSensAtt_;
    std::unique_ptr<SliderAttachment> wtMorphAtt_, wtLfoAtt_;
    std::unique_ptr<SliderAttachment> cutoffAtt_, resoAtt_, reverbAtt_;
    std::unique_ptr<ComboBoxAttachment> rootNoteAtt_, scaleTypeAtt_, orbiterModeAtt_;
    std::unique_ptr<ButtonAttachment> droneHoldAtt_;

    std::unique_ptr<juce::FileChooser> fileChooser_;
    juce::Rectangle<int> scopeBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundCanvasAudioProcessorEditor)
};

} // namespace soundcanvas
