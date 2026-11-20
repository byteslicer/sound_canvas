#pragma once

#include <JuceHeader.h>
#include "gui/SoundCanvasLookAndFeel.h"
#include "synth/SynthPluginProcessor.h"
#include "synth/SynthCanvasComponent.h"
#include <array>
#include <memory>
#include <vector>

namespace soundcanvas
{

class SoundCanvasSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit SoundCanvasSynthAudioProcessorEditor(SoundCanvasSynthAudioProcessor&);
    ~SoundCanvasSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openPaintingFileChooser();
    void selectTab(int tabIndex);

    void configureCompactRotary(juce::Slider& slider,
                                juce::Label& label,
                                const juce::String& text,
                                juce::Colour accentColour,
                                int tabIndex);

    void configureBipolarSlider(juce::Slider& slider, int tabIndex);

    SoundCanvasSynthAudioProcessor& processor_;
    SoundCanvasLookAndFeel lookAndFeel_;

    SynthCanvasComponent canvasComponent_;
    juce::MidiKeyboardComponent midiKeyboard_;

    // Top bar controls
    juce::TextButton loadPaintingButton_ { "Load Painting..." };
    juce::ComboBox presetSelector_;
    juce::ComboBox rootNoteBox_;
    juce::ComboBox scaleTypeBox_;
    juce::ToggleButton quantizeMidiButton_ { "Quantize MIDI" };
    juce::ToggleButton droneHoldButton_ { "Drone Hold" };
    juce::Label statusLabel_;

    // Right rack tab buttons
    std::array<juce::TextButton, 4> tabButtons_ {
        juce::TextButton("OSC & TEXTURE"),
        juce::TextButton("FILTER & ENV"),
        juce::TextButton("COLOR MATRIX"),
        juce::TextButton("MASTER FX")
    };
    int activeTab_ = 0;
    std::array<std::vector<juce::Component*>, 4> tabComponents_;

    // Tab 0: OSC & TEXTURE
    juce::Slider oscALevelSlider_, oscAMorphSlider_, oscALfoRateSlider_, oscALfoDepthSlider_, oscAWavefoldSlider_;
    juce::Slider oscAOctaveSlider_, oscASemiSlider_, oscAUnisonSlider_, oscADetuneSlider_, oscASpreadSlider_;
    juce::Label oscALevelLabel_, oscAMorphLabel_, oscALfoRateLabel_, oscALfoDepthLabel_, oscAWavefoldLabel_;
    juce::Label oscAOctaveLabel_, oscASemiLabel_, oscAUnisonLabel_, oscADetuneLabel_, oscASpreadLabel_;

    juce::Slider oscBLevelSlider_, oscBMorphSlider_, oscBLfoDepthSlider_, oscBFmToASlider_;
    juce::Slider oscBOctaveSlider_, oscBSemiSlider_, oscBUnisonSlider_, oscBDetuneSlider_;
    juce::Label oscBLevelLabel_, oscBMorphLabel_, oscBLfoDepthLabel_, oscBFmToALabel_;
    juce::Label oscBOctaveLabel_, oscBSemiLabel_, oscBUnisonLabel_, oscBDetuneLabel_;

    juce::Slider subLevelSlider_, edgeTransientSlider_;
    juce::ComboBox subOctaveBox_;
    juce::Label subLevelLabel_, subOctaveLabel_, edgeTransientLabel_;

    juce::Slider texLevelSlider_, texDensitySlider_, texRadiusSlider_, texShimmerSlider_, texTiltSlider_;
    juce::Label texLevelLabel_, texDensityLabel_, texRadiusLabel_, texShimmerLabel_, texTiltLabel_;

    // Tab 1: FILTER & ENV
    juce::ComboBox filterModeBox_;
    juce::Label filterModeLabel_;
    juce::Slider filterCutoffSlider_, filterResoSlider_, filterDriveSlider_;
    juce::Slider filterKeyTrackSlider_, filterEnvAmtSlider_, glideTimeSlider_;
    juce::Label filterCutoffLabel_, filterResoLabel_, filterDriveLabel_;
    juce::Label filterKeyTrackLabel_, filterEnvAmtLabel_, glideTimeLabel_;

    juce::Slider ampAttackSlider_, ampDecaySlider_, ampSustainSlider_, ampReleaseSlider_, velSensSlider_;
    juce::Label ampAttackLabel_, ampDecayLabel_, ampSustainLabel_, ampReleaseLabel_, velSensLabel_;

    juce::Slider filAttackSlider_, filDecaySlider_, filSustainSlider_, filReleaseSlider_;
    juce::Label filAttackLabel_, filDecayLabel_, filSustainLabel_, filReleaseLabel_;

    // Tab 2: COLOR MATRIX
    std::array<juce::ComboBox, 4> modSourceBoxes_;
    std::array<juce::ComboBox, 4> modDestBoxes_;
    std::array<juce::Slider, 4> modAmountSliders_;
    std::array<juce::Rectangle<int>, 4> modMeterBounds_;

    // Tab 3: MASTER FX
    juce::Slider chorusRateSlider_, chorusDepthSlider_, chorusMixSlider_;
    juce::Label chorusRateLabel_, chorusDepthLabel_, chorusMixLabel_;
    juce::Slider delayTimeSlider_, delayFeedbackSlider_, delayMixSlider_;
    juce::Label delayTimeLabel_, delayFeedbackLabel_, delayMixLabel_;
    juce::Slider reverbSizeSlider_, reverbDampSlider_, reverbMixSlider_;
    juce::Label reverbSizeLabel_, reverbDampLabel_, reverbMixLabel_;
    juce::Slider masterDriveSlider_, masterGainSlider_;
    juce::Label masterDriveLabel_, masterGainLabel_;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments_;
    std::vector<std::unique_ptr<ComboBoxAttachment>> comboAttachments_;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments_;

    std::unique_ptr<juce::FileChooser> fileChooser_;
    juce::Rectangle<int> scopeBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundCanvasSynthAudioProcessorEditor)
};

} // namespace soundcanvas
