#include "synth/SynthPluginEditor.h"
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

SoundCanvasSynthAudioProcessorEditor::SoundCanvasSynthAudioProcessorEditor(SoundCanvasSynthAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      canvasComponent_(p),
      midiKeyboard_(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel_);

    addAndMakeVisible(canvasComponent_);
    canvasComponent_.onStatusMessage = [this](const juce::String& text, bool isError)
    {
        statusLabel_.setColour(juce::Label::textColourId,
                               isError ? juce::Colour(0xffff6b6b) : juce::Colour(0xff7ee787));
        statusLabel_.setText(text, juce::dontSendNotification);
    };

    // Top bar
    addAndMakeVisible(loadPaintingButton_);
    loadPaintingButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff234b6e));
    loadPaintingButton_.onClick = [this]() { openPaintingFileChooser(); };

    addAndMakeVisible(presetSelector_);
    presetSelector_.addItem("Canvas 1: Starry Nocturne", 1);
    presetSelector_.addItem("Canvas 2: Sunset Impasto", 2);
    presetSelector_.addItem("Canvas 3: Watercolor Botanicals", 3);
    presetSelector_.setSelectedId(std::max(1, processor_.getCurrentPresetIndex() + 1), juce::dontSendNotification);
    presetSelector_.onChange = [this]()
    {
        const int idx = presetSelector_.getSelectedId() - 1;
        if (idx >= 0)
        {
            processor_.loadStarterPreset(idx);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9bb0d1));
            statusLabel_.setText("Loaded starter painting: " + presetSelector_.getText(), juce::dontSendNotification);
            canvasComponent_.repaint();
        }
    };

    addAndMakeVisible(rootNoteBox_);
    rootNoteBox_.addItemList(ScaleQuantizer::getRootNoteNames(), 1);
    comboAttachments_.push_back(std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "rootNote", rootNoteBox_));

    addAndMakeVisible(scaleTypeBox_);
    scaleTypeBox_.addItemList(ScaleQuantizer::getScaleNames(), 1);
    comboAttachments_.push_back(std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "scaleType", scaleTypeBox_));

    addAndMakeVisible(quantizeMidiButton_);
    buttonAttachments_.push_back(std::make_unique<ButtonAttachment>(processor_.getApvts(), "quantizeMidi", quantizeMidiButton_));

    addAndMakeVisible(droneHoldButton_);
    buttonAttachments_.push_back(std::make_unique<ButtonAttachment>(processor_.getApvts(), "droneHold", droneHoldButton_));

    addAndMakeVisible(statusLabel_);
    statusLabel_.setFont(juce::FontOptions(12.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9bb0d1));
    statusLabel_.setText("Drag A1/B1 (Osc A), A2/B2 (Osc B), or C/Ring (Texture Cloud). Drop .PNG/.JPG to load artwork.",
                         juce::dontSendNotification);

    // Tab buttons
    for (size_t i = 0; i < tabButtons_.size(); ++i)
    {
        addAndMakeVisible(tabButtons_[i]);
        tabButtons_[i].onClick = [this, tabIdx = static_cast<int>(i)]() { selectTab(tabIdx); };
    }

    const juce::Colour cyanCol(0xff00e5ff);
    const juce::Colour amberCol(0xffffb300);
    const juce::Colour violetCol(0xffc084fc);
    const juce::Colour greenCol(0xff4ade80);
    const juce::Colour coralCol(0xffff7a7a);

    auto attachSlider = [this](const char* paramId, juce::Slider& slider)
    {
        sliderAttachments_.push_back(std::make_unique<SliderAttachment>(processor_.getApvts(), paramId, slider));
    };

    // ---------------- Tab 0: OSC & TEXTURE ----------------
    configureCompactRotary(oscALevelSlider_,    oscALevelLabel_,    "Level",     cyanCol, 0);
    configureCompactRotary(oscAMorphSlider_,    oscAMorphLabel_,    "Morph",     cyanCol, 0);
    configureCompactRotary(oscALfoRateSlider_,  oscALfoRateLabel_,  "LFO Rate",  cyanCol, 0);
    configureCompactRotary(oscALfoDepthSlider_, oscALfoDepthLabel_, "LFO Depth", cyanCol, 0);
    configureCompactRotary(oscAWavefoldSlider_, oscAWavefoldLabel_, "Wavefold",  cyanCol, 0);
    configureCompactRotary(oscAOctaveSlider_,   oscAOctaveLabel_,   "Octave",    cyanCol, 0);
    configureCompactRotary(oscASemiSlider_,     oscASemiLabel_,     "Semi",      cyanCol, 0);
    configureCompactRotary(oscAUnisonSlider_,   oscAUnisonLabel_,   "Unison",    cyanCol, 0);
    configureCompactRotary(oscADetuneSlider_,   oscADetuneLabel_,   "Detune",    cyanCol, 0);
    configureCompactRotary(oscASpreadSlider_,   oscASpreadLabel_,   "Spread",    cyanCol, 0);

    attachSlider("oscALevel",    oscALevelSlider_);
    attachSlider("oscAMorph",    oscAMorphSlider_);
    attachSlider("oscALfoRate",  oscALfoRateSlider_);
    attachSlider("oscALfoDepth", oscALfoDepthSlider_);
    attachSlider("oscAWavefold", oscAWavefoldSlider_);
    attachSlider("oscAOctave",   oscAOctaveSlider_);
    attachSlider("oscASemitone", oscASemiSlider_);
    attachSlider("oscAUnison",   oscAUnisonSlider_);
    attachSlider("oscADetune",   oscADetuneSlider_);
    attachSlider("oscASpread",   oscASpreadSlider_);

    configureCompactRotary(oscBLevelSlider_,    oscBLevelLabel_,    "Level",     amberCol, 0);
    configureCompactRotary(oscBMorphSlider_,    oscBMorphLabel_,    "Morph",     amberCol, 0);
    configureCompactRotary(oscBLfoDepthSlider_, oscBLfoDepthLabel_, "LFO Depth", amberCol, 0);
    configureCompactRotary(oscBFmToASlider_,    oscBFmToALabel_,    "FM -> A",   amberCol, 0);
    configureCompactRotary(oscBOctaveSlider_,   oscBOctaveLabel_,   "Octave",    amberCol, 0);
    configureCompactRotary(oscBSemiSlider_,     oscBSemiLabel_,     "Semi",      amberCol, 0);
    configureCompactRotary(oscBUnisonSlider_,   oscBUnisonLabel_,   "Unison",    amberCol, 0);
    configureCompactRotary(oscBDetuneSlider_,   oscBDetuneLabel_,   "Detune",    amberCol, 0);

    attachSlider("oscBLevel",    oscBLevelSlider_);
    attachSlider("oscBMorph",    oscBMorphSlider_);
    attachSlider("oscBLfoDepth", oscBLfoDepthSlider_);
    attachSlider("oscBFmToA",    oscBFmToASlider_);
    attachSlider("oscBOctave",   oscBOctaveSlider_);
    attachSlider("oscBSemitone", oscBSemiSlider_);
    attachSlider("oscBUnison",   oscBUnisonSlider_);
    attachSlider("oscBDetune",   oscBDetuneSlider_);

    configureCompactRotary(subLevelSlider_,      subLevelLabel_,      "Sub Lvl",   violetCol, 0);
    configureCompactRotary(edgeTransientSlider_, edgeTransientLabel_, "Edge Tr",   violetCol, 0);
    configureCompactRotary(texLevelSlider_,      texLevelLabel_,      "Tex Lvl",   violetCol, 0);
    configureCompactRotary(texDensitySlider_,    texDensityLabel_,    "Density",   violetCol, 0);
    configureCompactRotary(texRadiusSlider_,     texRadiusLabel_,     "Radius",    violetCol, 0);
    configureCompactRotary(texShimmerSlider_,    texShimmerLabel_,    "Shimmer",   violetCol, 0);
    configureCompactRotary(texTiltSlider_,       texTiltLabel_,       "Tilt",      violetCol, 0);

    addChildComponent(subOctaveLabel_);
    subOctaveLabel_.setText("Sub Oct", juce::dontSendNotification);
    subOctaveLabel_.setJustificationType(juce::Justification::centred);
    subOctaveLabel_.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    subOctaveLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffc9d4e8));
    tabComponents_[0].push_back(&subOctaveLabel_);

    addChildComponent(subOctaveBox_);
    subOctaveBox_.addItemList({ "-1 Oct", "-2 Oct" }, 1);
    tabComponents_[0].push_back(&subOctaveBox_);
    comboAttachments_.push_back(std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "subOctave", subOctaveBox_));

    attachSlider("subLevel",       subLevelSlider_);
    attachSlider("edgeTransient",  edgeTransientSlider_);
    attachSlider("textureLevel",   texLevelSlider_);
    attachSlider("textureDensity", texDensitySlider_);
    attachSlider("textureRadius",  texRadiusSlider_);
    attachSlider("textureShimmer", texShimmerSlider_);
    attachSlider("textureTilt",    texTiltSlider_);

    // ---------------- Tab 1: FILTER & ENV ----------------
    addChildComponent(filterModeLabel_);
    filterModeLabel_.setText("Filter Mode", juce::dontSendNotification);
    filterModeLabel_.setJustificationType(juce::Justification::centredLeft);
    filterModeLabel_.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    filterModeLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffc9d4e8));
    tabComponents_[1].push_back(&filterModeLabel_);

    addChildComponent(filterModeBox_);
    filterModeBox_.addItemList(PolySynthEngine::getFilterModeNames(), 1);
    tabComponents_[1].push_back(&filterModeBox_);
    comboAttachments_.push_back(std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "filterMode", filterModeBox_));

    configureCompactRotary(filterCutoffSlider_,   filterCutoffLabel_,   "Cutoff",   cyanCol, 1);
    configureCompactRotary(filterResoSlider_,     filterResoLabel_,     "Reso",     cyanCol, 1);
    configureCompactRotary(filterDriveSlider_,    filterDriveLabel_,    "Drive",    cyanCol, 1);
    configureCompactRotary(filterKeyTrackSlider_, filterKeyTrackLabel_, "KeyTrack", cyanCol, 1);
    configureCompactRotary(filterEnvAmtSlider_,   filterEnvAmtLabel_,   "Env Amt",  cyanCol, 1);
    configureCompactRotary(glideTimeSlider_,      glideTimeLabel_,      "Glide",    cyanCol, 1);

    attachSlider("filterCutoff",    filterCutoffSlider_);
    attachSlider("filterReso",      filterResoSlider_);
    attachSlider("filterDrive",     filterDriveSlider_);
    attachSlider("filterKeyTrack",  filterKeyTrackSlider_);
    attachSlider("filterEnvAmount", filterEnvAmtSlider_);
    attachSlider("glideTime",       glideTimeSlider_);

    configureCompactRotary(ampAttackSlider_,  ampAttackLabel_,  "Attack",   greenCol, 1);
    configureCompactRotary(ampDecaySlider_,   ampDecayLabel_,   "Decay",    greenCol, 1);
    configureCompactRotary(ampSustainSlider_, ampSustainLabel_, "Sustain",  greenCol, 1);
    configureCompactRotary(ampReleaseSlider_, ampReleaseLabel_, "Release",  greenCol, 1);
    configureCompactRotary(velSensSlider_,    velSensLabel_,    "Vel Sens", greenCol, 1);

    attachSlider("ampAttack",      ampAttackSlider_);
    attachSlider("ampDecay",       ampDecaySlider_);
    attachSlider("ampSustain",     ampSustainSlider_);
    attachSlider("ampRelease",     ampReleaseSlider_);
    attachSlider("velSensitivity", velSensSlider_);

    configureCompactRotary(filAttackSlider_,  filAttackLabel_,  "Attack",  amberCol, 1);
    configureCompactRotary(filDecaySlider_,   filDecayLabel_,   "Decay",   amberCol, 1);
    configureCompactRotary(filSustainSlider_, filSustainLabel_, "Sustain", amberCol, 1);
    configureCompactRotary(filReleaseSlider_, filReleaseLabel_, "Release", amberCol, 1);

    attachSlider("filAttack",  filAttackSlider_);
    attachSlider("filDecay",   filDecaySlider_);
    attachSlider("filSustain", filSustainSlider_);
    attachSlider("filRelease", filReleaseSlider_);

    // ---------------- Tab 2: COLOR MATRIX ----------------
    const auto modSourceNames = PolySynthEngine::getModSourceNames();
    const auto modDestNames = PolySynthEngine::getModDestinationNames();
    for (int i = 0; i < 4; ++i)
    {
        const size_t idx = static_cast<size_t>(i);
        const juce::String idxStr(i + 1);

        addChildComponent(modSourceBoxes_[idx]);
        modSourceBoxes_[idx].addItemList(modSourceNames, 1);
        tabComponents_[2].push_back(&modSourceBoxes_[idx]);
        comboAttachments_.push_back(std::make_unique<ComboBoxAttachment>(
            processor_.getApvts(), "modSource" + idxStr, modSourceBoxes_[idx]));

        addChildComponent(modDestBoxes_[idx]);
        modDestBoxes_[idx].addItemList(modDestNames, 1);
        tabComponents_[2].push_back(&modDestBoxes_[idx]);
        comboAttachments_.push_back(std::make_unique<ComboBoxAttachment>(
            processor_.getApvts(), "modDest" + idxStr, modDestBoxes_[idx]));

        configureBipolarSlider(modAmountSliders_[idx], 2);
        attachSlider(("modAmount" + idxStr).toRawUTF8(), modAmountSliders_[idx]);
    }

    // ---------------- Tab 3: MASTER FX ----------------
    configureCompactRotary(chorusRateSlider_,  chorusRateLabel_,  "Rate",    cyanCol, 3);
    configureCompactRotary(chorusDepthSlider_, chorusDepthLabel_, "Depth",   cyanCol, 3);
    configureCompactRotary(chorusMixSlider_,   chorusMixLabel_,   "Mix",     cyanCol, 3);

    attachSlider("chorusRate",  chorusRateSlider_);
    attachSlider("chorusDepth", chorusDepthSlider_);
    attachSlider("chorusMix",   chorusMixSlider_);

    configureCompactRotary(delayTimeSlider_,     delayTimeLabel_,     "Time",     amberCol, 3);
    configureCompactRotary(delayFeedbackSlider_, delayFeedbackLabel_, "Feedback", amberCol, 3);
    configureCompactRotary(delayMixSlider_,      delayMixLabel_,      "Mix",      amberCol, 3);

    attachSlider("delayTime",     delayTimeSlider_);
    attachSlider("delayFeedback", delayFeedbackSlider_);
    attachSlider("delayMix",      delayMixSlider_);

    configureCompactRotary(reverbSizeSlider_, reverbSizeLabel_, "Size",    violetCol, 3);
    configureCompactRotary(reverbDampSlider_, reverbDampLabel_, "Damping", violetCol, 3);
    configureCompactRotary(reverbMixSlider_,  reverbMixLabel_,  "Mix",     violetCol, 3);

    attachSlider("reverbSize",    reverbSizeSlider_);
    attachSlider("reverbDamping", reverbDampSlider_);
    attachSlider("reverbMix",     reverbMixSlider_);

    configureCompactRotary(masterDriveSlider_, masterDriveLabel_, "Drive", coralCol, 3);
    configureCompactRotary(masterGainSlider_,  masterGainLabel_,  "Gain",  coralCol, 3);

    attachSlider("masterDrive", masterDriveSlider_);
    attachSlider("masterGain",  masterGainSlider_);

    // On-screen MIDI keyboard (36..84)
    addAndMakeVisible(midiKeyboard_);
    midiKeyboard_.setAvailableRange(36, 84);
    midiKeyboard_.setKeyWidth(36.5f);

    setSize(1120, 700);
    selectTab(0);
    startTimerHz(30);
}

SoundCanvasSynthAudioProcessorEditor::~SoundCanvasSynthAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SoundCanvasSynthAudioProcessorEditor::configureCompactRotary(juce::Slider& slider,
                                                                  juce::Label& label,
                                                                  const juce::String& text,
                                                                  juce::Colour accentColour,
                                                                  int tabIndex)
{
    addChildComponent(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 13);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffdce4f2));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    addChildComponent(label);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffc9d4e8));

    const size_t idx = static_cast<size_t>(std::clamp(tabIndex, 0, 3));
    tabComponents_[idx].push_back(&slider);
    tabComponents_[idx].push_back(&label);
}

void SoundCanvasSynthAudioProcessorEditor::configureBipolarSlider(juce::Slider& slider, int tabIndex)
{
    addChildComponent(slider);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00e5ff));
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2a3042));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffdce4f2));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    const size_t idx = static_cast<size_t>(std::clamp(tabIndex, 0, 3));
    tabComponents_[idx].push_back(&slider);
}

void SoundCanvasSynthAudioProcessorEditor::selectTab(int tabIndex)
{
    activeTab_ = std::clamp(tabIndex, 0, 3);
    for (size_t i = 0; i < tabButtons_.size(); ++i)
    {
        const bool active = (static_cast<int>(i) == activeTab_);
        tabButtons_[i].setToggleState(active, juce::dontSendNotification);
        for (auto* comp : tabComponents_[i])
        {
            if (comp != nullptr)
                comp->setVisible(active);
        }
    }
    repaint(708, 64, 400, 540);
}

void SoundCanvasSynthAudioProcessorEditor::openPaintingFileChooser()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Select a Painting (.png, .jpg, .jpeg)",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg");

    const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser_->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        const auto result = fc.getResult();
        if (result == juce::File {})
            return;

        juce::String errorMsg;
        if (processor_.loadPaintingFromFile(result, errorMsg))
        {
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff7ee787));
            statusLabel_.setText("Loaded painting: " + result.getFileName(), juce::dontSendNotification);
            canvasComponent_.repaint();
        }
        else
        {
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
            statusLabel_.setText(errorMsg, juce::dontSendNotification);
        }
    });
}

void SoundCanvasSynthAudioProcessorEditor::timerCallback()
{
    if (!scopeBounds_.isEmpty())
        repaint(scopeBounds_);

    if (activeTab_ == 2)
    {
        for (const auto& meterRect : modMeterBounds_)
        {
            if (!meterRect.isEmpty())
                repaint(meterRect);
        }
    }
}

void SoundCanvasSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d0f14));

    // Top bar
    g.setColour(juce::Colour(0xff141821));
    g.fillRect(0, 0, getWidth(), 56);
    g.setColour(juce::Colour(0xff262c3d));
    g.drawLine(0.0f, 56.0f, static_cast<float>(getWidth()), 56.0f, 1.0f);

    // Title + subtitle
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("SOUND CANVAS SYNTH", 12, 7, 186, 22, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0xff8fa3c4));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Polyphonic Image Synthesizer", 12, 29, 186, 18, juce::Justification::centredLeft, false);

    // Stereo Oscilloscope
    if (!scopeBounds_.isEmpty())
    {
        const auto sb = scopeBounds_.toFloat();
        g.setColour(juce::Colour(0xff0b0d12));
        g.fillRoundedRectangle(sb, 5.0f);
        g.setColour(juce::Colour(0xff2b3348));
        g.drawRoundedRectangle(sb, 5.0f, 1.0f);

        const auto scopeL = processor_.getScopeSnapshot();
        const auto scopeR = processor_.getScopeSnapshotRight();

        auto buildWavePath = [&](const std::array<float, SoundCanvasSynthAudioProcessor::kScopeSize>& data)
        {
            juce::Path p;
            for (int i = 0; i < SoundCanvasSynthAudioProcessor::kScopeSize; ++i)
            {
                const float x = sb.getX() + (static_cast<float>(i) / static_cast<float>(SoundCanvasSynthAudioProcessor::kScopeSize - 1)) * sb.getWidth();
                const float y = sb.getCentreY() - std::clamp(data[static_cast<size_t>(i)], -1.0f, 1.0f) * (sb.getHeight() * 0.42f);
                if (i == 0)
                    p.startNewSubPath(x, y);
                else
                    p.lineTo(x, y);
            }
            return p;
        };

        g.setColour(juce::Colour(0xffc084fc).withAlpha(0.75f));
        g.strokePath(buildWavePath(scopeR), juce::PathStrokeType(1.3f));

        g.setColour(juce::Colour(0xff00e5ff));
        g.strokePath(buildWavePath(scopeL), juce::PathStrokeType(1.5f));
    }

    // Right Sound-Shaping Rack Card (400 x 540 at x = 708, y = 64)
    const juce::Rectangle<float> rackRect(708.0f, 64.0f, 400.0f, 540.0f);
    g.setColour(juce::Colour(0xff141821));
    g.fillRoundedRectangle(rackRect, 10.0f);
    g.setColour(juce::Colour(0xff272e40));
    g.drawRoundedRectangle(rackRect, 10.0f, 1.2f);

    auto drawSectionHeader = [&](const juce::String& title, int yPos, juce::Colour col)
    {
        g.setColour(col);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, 718, yPos, 380, 16, juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xff262d3f));
        g.drawLine(718.0f, static_cast<float>(yPos + 16), 1098.0f, static_cast<float>(yPos + 16), 1.0f);
    };

    if (activeTab_ == 0)
    {
        drawSectionHeader("OSC A (CYAN BRUSH SLICE)", 106, juce::Colour(0xff00e5ff));
        drawSectionHeader("OSC B (AMBER BRUSH SLICE)", 264, juce::Colour(0xffffb300));
        drawSectionHeader("SUB, EDGE & TEXTURE CLOUD (VIOLET)", 422, juce::Colour(0xffc084fc));
    }
    else if (activeTab_ == 1)
    {
        drawSectionHeader("MULTIMODE FILTER & GLIDE", 106, juce::Colour(0xff00e5ff));
        drawSectionHeader("AMP ENVELOPE (ADSR + VEL)", 312, juce::Colour(0xff4ade80));
        drawSectionHeader("FILTER ENVELOPE (ADSR)", 418, juce::Colour(0xffffb300));
    }
    else if (activeTab_ == 2)
    {
        drawSectionHeader("PAINTING COLOR -> SYNTH MOD MATRIX", 106, juce::Colour(0xffc084fc));
        const auto modOutputs = processor_.getSynthEngine().getLastSlotModOutputs();

        for (int i = 0; i < 4; ++i)
        {
            const int cardY = 132 + i * 114;
            const juce::Rectangle<float> card(716.0f, static_cast<float>(cardY), 384.0f, 104.0f);
            g.setColour(juce::Colour(0xff191e2b));
            g.fillRoundedRectangle(card, 7.0f);
            g.setColour(juce::Colour(0xff2b3348));
            g.drawRoundedRectangle(card, 7.0f, 1.0f);

            g.setColour(juce::Colour(0xff9bb0d1));
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText("SLOT " + juce::String(i + 1), 724, cardY + 6, 52, 16, juce::Justification::centredLeft, false);
            g.drawText("->", 899, cardY + 26, 18, 24, juce::Justification::centred, false);
            g.drawText("Amt", 724, cardY + 56, 30, 20, juce::Justification::centredLeft, false);

            // Live Bipolar Mod Indicator Bar
            const auto meter = modMeterBounds_[static_cast<size_t>(i)].toFloat();
            if (!meter.isEmpty())
            {
                g.setColour(juce::Colour(0xff0b0d12));
                g.fillRoundedRectangle(meter, 3.0f);
                g.setColour(juce::Colour(0xff2b3348));
                g.drawRoundedRectangle(meter, 3.0f, 1.0f);

                const float val = std::clamp(modOutputs[static_cast<size_t>(i)], -1.0f, 1.0f);
                const float midX = meter.getCentreX();
                const float halfW = meter.getWidth() * 0.5f - 2.0f;
                const float barW = std::abs(val) * halfW;

                if (barW > 0.5f)
                {
                    const juce::Colour barCol = (val >= 0.0f) ? juce::Colour(0xff00e5ff) : juce::Colour(0xffffb300);
                    g.setColour(barCol);
                    const float barX = (val >= 0.0f) ? midX : (midX - barW);
                    g.fillRoundedRectangle(barX, meter.getY() + 2.0f, barW, meter.getHeight() - 4.0f, 2.0f);
                }

                // Center zero marker
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.drawLine(midX, meter.getY() + 1.0f, midX, meter.getBottom() - 1.0f, 1.0f);
            }
        }
    }
    else if (activeTab_ == 3)
    {
        drawSectionHeader("STEREO CHORUS", 106, juce::Colour(0xff00e5ff));
        drawSectionHeader("PING-PONG DELAY", 216, juce::Colour(0xffffb300));
        drawSectionHeader("GALLERY REVERB", 326, juce::Colour(0xffc084fc));
        drawSectionHeader("MASTER DRIVE & OUTPUT", 436, juce::Colour(0xffff7a7a));
    }
}

void SoundCanvasSynthAudioProcessorEditor::resized()
{
    // Top bar (56px high, 1120px wide)
    loadPaintingButton_.setBounds(200, 13, 112, 30);
    presetSelector_.setBounds(318, 13, 172, 30);
    rootNoteBox_.setBounds(496, 13, 56, 30);
    scaleTypeBox_.setBounds(558, 13, 156, 30);
    quantizeMidiButton_.setBounds(720, 13, 110, 30);
    droneHoldButton_.setBounds(836, 13, 94, 30);
    scopeBounds_ = { 936, 11, 172, 34 };

    // Left/Center Painting Canvas + Status Label
    canvasComponent_.setBounds(12, 64, 684, 516);
    statusLabel_.setBounds(12, 584, 684, 20);

    // Bottom MIDI Keyboard
    midiKeyboard_.setBounds(12, 612, getWidth() - 24, 76);

    // Right Sound-Shaping Rack (708, 64, 400, 540)
    const int rackX = 716;
    const int rackW = 384;

    // 4 Tab Buttons
    const int tabW = 93;
    const int tabGap = 4;
    for (int i = 0; i < 4; ++i)
        tabButtons_[static_cast<size_t>(i)].setBounds(rackX + i * (tabW + tabGap), 72, tabW, 26);

    auto placeKnob = [](juce::Slider& s, juce::Label& l, int cellX, int cellY, int cellW, int knobH)
    {
        l.setBounds(cellX, cellY, cellW, 14);
        s.setBounds(cellX + 2, cellY + 14, cellW - 4, knobH);
    };

    // ---------------- Tab 0: OSC & TEXTURE ----------------
    {
        const int col5W = rackW / 5; // 76px
        placeKnob(oscALevelSlider_,    oscALevelLabel_,    rackX + 0 * col5W, 126, col5W, 52);
        placeKnob(oscAMorphSlider_,    oscAMorphLabel_,    rackX + 1 * col5W, 126, col5W, 52);
        placeKnob(oscALfoRateSlider_,  oscALfoRateLabel_,  rackX + 2 * col5W, 126, col5W, 52);
        placeKnob(oscALfoDepthSlider_, oscALfoDepthLabel_, rackX + 3 * col5W, 126, col5W, 52);
        placeKnob(oscAWavefoldSlider_, oscAWavefoldLabel_, rackX + 4 * col5W, 126, col5W, 52);

        placeKnob(oscAOctaveSlider_,   oscAOctaveLabel_,   rackX + 0 * col5W, 194, col5W, 52);
        placeKnob(oscASemiSlider_,     oscASemiLabel_,     rackX + 1 * col5W, 194, col5W, 52);
        placeKnob(oscAUnisonSlider_,   oscAUnisonLabel_,   rackX + 2 * col5W, 194, col5W, 52);
        placeKnob(oscADetuneSlider_,   oscADetuneLabel_,   rackX + 3 * col5W, 194, col5W, 52);
        placeKnob(oscASpreadSlider_,   oscASpreadLabel_,   rackX + 4 * col5W, 194, col5W, 52);

        const int col4W = rackW / 4; // 96px
        placeKnob(oscBLevelSlider_,    oscBLevelLabel_,    rackX + 0 * col4W, 284, col4W, 52);
        placeKnob(oscBMorphSlider_,    oscBMorphLabel_,    rackX + 1 * col4W, 284, col4W, 52);
        placeKnob(oscBLfoDepthSlider_, oscBLfoDepthLabel_, rackX + 2 * col4W, 284, col4W, 52);
        placeKnob(oscBFmToASlider_,    oscBFmToALabel_,    rackX + 3 * col4W, 284, col4W, 52);

        placeKnob(oscBOctaveSlider_,   oscBOctaveLabel_,   rackX + 0 * col4W, 352, col4W, 52);
        placeKnob(oscBSemiSlider_,     oscBSemiLabel_,     rackX + 1 * col4W, 352, col4W, 52);
        placeKnob(oscBUnisonSlider_,   oscBUnisonLabel_,   rackX + 2 * col4W, 352, col4W, 52);
        placeKnob(oscBDetuneSlider_,   oscBDetuneLabel_,   rackX + 3 * col4W, 352, col4W, 52);

        placeKnob(subLevelSlider_,      subLevelLabel_,      rackX + 0 * col4W, 442, col4W, 54);
        subOctaveLabel_.setBounds(rackX + 1 * col4W, 442, col4W, 14);
        subOctaveBox_.setBounds(rackX + 1 * col4W + 8, 466, col4W - 16, 26);
        placeKnob(edgeTransientSlider_, edgeTransientLabel_, rackX + 2 * col4W, 442, col4W, 54);
        placeKnob(texLevelSlider_,      texLevelLabel_,      rackX + 3 * col4W, 442, col4W, 54);

        placeKnob(texDensitySlider_, texDensityLabel_, rackX + 0 * col4W, 516, col4W, 54);
        placeKnob(texRadiusSlider_,  texRadiusLabel_,  rackX + 1 * col4W, 516, col4W, 54);
        placeKnob(texShimmerSlider_, texShimmerLabel_, rackX + 2 * col4W, 516, col4W, 54);
        placeKnob(texTiltSlider_,    texTiltLabel_,    rackX + 3 * col4W, 516, col4W, 54);
    }

    // ---------------- Tab 1: FILTER & ENV ----------------
    {
        filterModeLabel_.setBounds(rackX, 128, 90, 26);
        filterModeBox_.setBounds(rackX + 94, 128, 190, 26);

        const int col3W = rackW / 3; // 128px
        placeKnob(filterCutoffSlider_,   filterCutoffLabel_,   rackX + 0 * col3W, 162, col3W, 56);
        placeKnob(filterResoSlider_,     filterResoLabel_,     rackX + 1 * col3W, 162, col3W, 56);
        placeKnob(filterDriveSlider_,    filterDriveLabel_,    rackX + 2 * col3W, 162, col3W, 56);
        placeKnob(filterKeyTrackSlider_, filterKeyTrackLabel_, rackX + 0 * col3W, 236, col3W, 56);
        placeKnob(filterEnvAmtSlider_,   filterEnvAmtLabel_,   rackX + 1 * col3W, 236, col3W, 56);
        placeKnob(glideTimeSlider_,      glideTimeLabel_,      rackX + 2 * col3W, 236, col3W, 56);

        const int col5W = rackW / 5; // 76px
        placeKnob(ampAttackSlider_,  ampAttackLabel_,  rackX + 0 * col5W, 336, col5W, 58);
        placeKnob(ampDecaySlider_,   ampDecayLabel_,   rackX + 1 * col5W, 336, col5W, 58);
        placeKnob(ampSustainSlider_, ampSustainLabel_, rackX + 2 * col5W, 336, col5W, 58);
        placeKnob(ampReleaseSlider_, ampReleaseLabel_, rackX + 3 * col5W, 336, col5W, 58);
        placeKnob(velSensSlider_,    velSensLabel_,    rackX + 4 * col5W, 336, col5W, 58);

        const int col4W = rackW / 4; // 96px
        placeKnob(filAttackSlider_,  filAttackLabel_,  rackX + 0 * col4W, 442, col4W, 60);
        placeKnob(filDecaySlider_,   filDecayLabel_,   rackX + 1 * col4W, 442, col4W, 60);
        placeKnob(filSustainSlider_, filSustainLabel_, rackX + 2 * col4W, 442, col4W, 60);
        placeKnob(filReleaseSlider_, filReleaseLabel_, rackX + 3 * col4W, 442, col4W, 60);
    }

    // ---------------- Tab 2: COLOR MATRIX ----------------
    {
        for (int i = 0; i < 4; ++i)
        {
            const size_t idx = static_cast<size_t>(i);
            const int cardY = 132 + i * 114;
            modSourceBoxes_[idx].setBounds(724, cardY + 26, 172, 24);
            modDestBoxes_[idx].setBounds(920, cardY + 26, 172, 24);
            modAmountSliders_[idx].setBounds(758, cardY + 54, 334, 24);
            modMeterBounds_[idx] = { 724, cardY + 84, 368, 12 };
        }
    }

    // ---------------- Tab 3: MASTER FX ----------------
    {
        const int col3W = rackW / 3; // 128px
        placeKnob(chorusRateSlider_,    chorusRateLabel_,    rackX + 0 * col3W, 130, col3W, 64);
        placeKnob(chorusDepthSlider_,   chorusDepthLabel_,   rackX + 1 * col3W, 130, col3W, 64);
        placeKnob(chorusMixSlider_,     chorusMixLabel_,     rackX + 2 * col3W, 130, col3W, 64);

        placeKnob(delayTimeSlider_,     delayTimeLabel_,     rackX + 0 * col3W, 240, col3W, 64);
        placeKnob(delayFeedbackSlider_, delayFeedbackLabel_, rackX + 1 * col3W, 240, col3W, 64);
        placeKnob(delayMixSlider_,      delayMixLabel_,      rackX + 2 * col3W, 240, col3W, 64);

        placeKnob(reverbSizeSlider_,    reverbSizeLabel_,    rackX + 0 * col3W, 350, col3W, 64);
        placeKnob(reverbDampSlider_,    reverbDampLabel_,    rackX + 1 * col3W, 350, col3W, 64);
        placeKnob(reverbMixSlider_,     reverbMixLabel_,     rackX + 2 * col3W, 350, col3W, 64);

        placeKnob(masterDriveSlider_,   masterDriveLabel_,   rackX + 0 * col3W, 460, col3W, 64);
        placeKnob(masterGainSlider_,    masterGainLabel_,    rackX + 1 * col3W, 460, col3W, 64);
    }
}

} // namespace soundcanvas
