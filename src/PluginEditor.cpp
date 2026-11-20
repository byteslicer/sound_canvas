#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

SoundCanvasLookAndFeel::SoundCanvasLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0d0f14));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff191d28));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff31384c));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe8ecf4));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2637));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe8ecf4));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff191d28));
}

void SoundCanvasLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                              int x, int y, int width, int height,
                                              float sliderPosProportional,
                                              float rotaryStartAngle,
                                              float rotaryEndAngle,
                                              juce::Slider& slider)
{
    const float radius = static_cast<float>(std::min(width, height)) * 0.40f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const juce::Colour accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // Dial body
    g.setColour(juce::Colour(0xff161922));
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

    // Background arc
    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff2a3042));
    g.strokePath(bgArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc
    juce::Path valArc;
    valArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(accent);
    g.strokePath(valArc, juce::PathStrokeType(3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer indicator
    juce::Path pointer;
    const float ptrLen = radius * 0.68f;
    pointer.addRoundedRectangle(-1.6f, -radius + 2.0f, 3.2f, ptrLen, 1.5f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colours::white);
    g.fillPath(pointer);
}

SoundCanvasAudioProcessorEditor::SoundCanvasAudioProcessorEditor(SoundCanvasAudioProcessor& p)
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

    // Load Painting Button
    addAndMakeVisible(loadPaintingButton_);
    loadPaintingButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff234b6e));
    loadPaintingButton_.onClick = [this]() { openPaintingFileChooser(); };

    // Built-in Starter Canvases
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

    // Root Note & Musical Scale selectors
    addAndMakeVisible(rootNoteBox_);
    rootNoteBox_.addItemList(ScaleQuantizer::getRootNoteNames(), 1);
    rootNoteAtt_ = std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "rootNote", rootNoteBox_);

    addAndMakeVisible(scaleTypeBox_);
    scaleTypeBox_.addItemList(ScaleQuantizer::getScaleNames(), 1);
    scaleTypeAtt_ = std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "scaleType", scaleTypeBox_);

    // Drone Hold Toggle
    addAndMakeVisible(droneHoldButton_);
    droneHoldAtt_ = std::make_unique<ButtonAttachment>(processor_.getApvts(), "droneHold", droneHoldButton_);

    // Status label
    addAndMakeVisible(statusLabel_);
    statusLabel_.setFont(juce::FontOptions(12.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9bb0d1));
    statusLabel_.setText("Drag & drop any .PNG/.JPG painting onto the canvas, or play MIDI notes below.",
                         juce::dontSendNotification);

    // Configure right-rack rotary knobs
    const juce::Colour scannerCol(0xffffca28);
    const juce::Colour orbiterCol(0xff4ade80);
    const juce::Colour waveCol(0xff00e5ff);
    const juce::Colour fxCol(0xffc084fc);

    configureRotarySlider(scannerMixSlider_, scannerMixLabel_, "Scanner", scannerCol);
    configureRotarySlider(orbiterMixSlider_, orbiterMixLabel_, "Orbiters", orbiterCol);
    configureRotarySlider(wavetableMixSlider_, wavetableMixLabel_, "Brush Synth", waveCol);

    configureRotarySlider(scanSpeedSlider_, scanSpeedLabel_, "Motion Speed", scannerCol);
    configureRotarySlider(edgeSensSlider_, edgeSensLabel_, "Edge Pluck", orbiterCol);

    configureRotarySlider(wtMorphSlider_, wtMorphLabel_, "Slice Morph", waveCol);
    configureRotarySlider(wtLfoSlider_, wtLfoLabel_, "Morph LFO", waveCol);

    configureRotarySlider(cutoffSlider_, cutoffLabel_, "Color Filter", fxCol);
    configureRotarySlider(resoSlider_, resoLabel_, "Resonance", fxCol);
    configureRotarySlider(reverbSlider_, reverbLabel_, "Gallery Space", fxCol);

    addAndMakeVisible(orbiterModeBox_);
    orbiterModeBox_.addItemList({ "Billiard Bounce", "Lissajous Orbit", "Brushstroke Drift" }, 1);
    orbiterModeAtt_ = std::make_unique<ComboBoxAttachment>(processor_.getApvts(), "orbiterMode", orbiterModeBox_);

    addAndMakeVisible(orbiterModeLabel_);
    orbiterModeLabel_.setText("Orbiter Path Mode", juce::dontSendNotification);
    orbiterModeLabel_.setFont(juce::FontOptions(11.5f));
    orbiterModeLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffb4c0d4));

    scannerMixAtt_   = std::make_unique<SliderAttachment>(processor_.getApvts(), "scannerMix", scannerMixSlider_);
    orbiterMixAtt_   = std::make_unique<SliderAttachment>(processor_.getApvts(), "orbiterMix", orbiterMixSlider_);
    wavetableMixAtt_ = std::make_unique<SliderAttachment>(processor_.getApvts(), "wavetableMix", wavetableMixSlider_);
    scanSpeedAtt_    = std::make_unique<SliderAttachment>(processor_.getApvts(), "scanSpeed", scanSpeedSlider_);
    edgeSensAtt_     = std::make_unique<SliderAttachment>(processor_.getApvts(), "edgeSensitivity", edgeSensSlider_);
    wtMorphAtt_      = std::make_unique<SliderAttachment>(processor_.getApvts(), "wtMorph", wtMorphSlider_);
    wtLfoAtt_        = std::make_unique<SliderAttachment>(processor_.getApvts(), "wtLfoDepth", wtLfoSlider_);
    cutoffAtt_       = std::make_unique<SliderAttachment>(processor_.getApvts(), "filterCutoff", cutoffSlider_);
    resoAtt_         = std::make_unique<SliderAttachment>(processor_.getApvts(), "filterReso", resoSlider_);
    reverbAtt_       = std::make_unique<SliderAttachment>(processor_.getApvts(), "reverbMix", reverbSlider_);

    // On-screen MIDI keyboard
    addAndMakeVisible(midiKeyboard_);
    midiKeyboard_.setAvailableRange(36, 84);
    midiKeyboard_.setKeyWidth(24.0f);

    setSize(980, 650);
    startTimerHz(30);
}

SoundCanvasAudioProcessorEditor::~SoundCanvasAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SoundCanvasAudioProcessorEditor::configureRotarySlider(juce::Slider& slider,
                                                            juce::Label& label,
                                                            const juce::String& text,
                                                            juce::Colour accentColour)
{
    addAndMakeVisible(slider);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 16);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffdce4f2));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(label);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffc9d4e8));
}

void SoundCanvasAudioProcessorEditor::openPaintingFileChooser()
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

void SoundCanvasAudioProcessorEditor::timerCallback()
{
    if (!scopeBounds_.isEmpty())
        repaint(scopeBounds_);
}

void SoundCanvasAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d0f14));

    // Top bar background
    g.setColour(juce::Colour(0xff141821));
    g.fillRect(0, 0, getWidth(), 54);
    g.setColour(juce::Colour(0xff262c3d));
    g.drawLine(0.0f, 54.0f, static_cast<float>(getWidth()), 54.0f, 1.0f);

    // Brand title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.5f, juce::Font::bold));
    g.drawText("SOUND CANVAS", 14, 6, 170, 24, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(0xff8fa3c4));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Painting-to-Music Synth", 14, 28, 170, 18, juce::Justification::centredLeft, false);

    // Right control panel card
    const juce::Rectangle<float> rightPanel(static_cast<float>(getWidth() - 304), 64.0f, 292.0f, 486.0f);
    g.setColour(juce::Colour(0xff141821));
    g.fillRoundedRectangle(rightPanel, 10.0f);
    g.setColour(juce::Colour(0xff272e40));
    g.drawRoundedRectangle(rightPanel, 10.0f, 1.2f);

    // Section headers in right rack
    g.setColour(juce::Colour(0xff8fa3c4));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("SYNESTHETIC LAYER MIX", getWidth() - 294, 72, 272, 16, juce::Justification::centredLeft, false);
    g.drawText("SCANNER, ORBITERS & BRUSH SLICE", getWidth() - 294, 194, 272, 16, juce::Justification::centredLeft, false);
    g.drawText("COLOR FILTER & GALLERY SPACE", getWidth() - 294, 420, 272, 16, juce::Justification::centredLeft, false);

    // Real-time Output Waveform Oscilloscope in top-right
    if (!scopeBounds_.isEmpty())
    {
        const auto sb = scopeBounds_.toFloat();
        g.setColour(juce::Colour(0xff0b0d12));
        g.fillRoundedRectangle(sb, 5.0f);
        g.setColour(juce::Colour(0xff2b3348));
        g.drawRoundedRectangle(sb, 5.0f, 1.0f);

        const auto scope = processor_.getScopeSnapshot();
        juce::Path wave;
        for (int i = 0; i < SoundCanvasAudioProcessor::kScopeSize; ++i)
        {
            const float x = sb.getX() + (static_cast<float>(i) / static_cast<float>(SoundCanvasAudioProcessor::kScopeSize - 1)) * sb.getWidth();
            const float y = sb.getCentreY() - std::clamp(scope[static_cast<size_t>(i)], -1.0f, 1.0f) * (sb.getHeight() * 0.42f);
            if (i == 0)
                wave.startNewSubPath(x, y);
            else
                wave.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xff00e5ff));
        g.strokePath(wave, juce::PathStrokeType(1.6f));
    }
}

void SoundCanvasAudioProcessorEditor::resized()
{
    // Top bar layout
    loadPaintingButton_.setBounds(178, 12, 124, 30);
    presetSelector_.setBounds(310, 12, 195, 30);
    rootNoteBox_.setBounds(513, 12, 62, 30);
    scaleTypeBox_.setBounds(582, 12, 168, 30);
    droneHoldButton_.setBounds(758, 12, 95, 30);
    scopeBounds_ = { 858, 10, 110, 34 };

    // Center Painting Canvas
    canvasComponent_.setBounds(12, 64, getWidth() - 326, 486);

    // Right Control Rack (3 columns x rows)
    const int rackX = getWidth() - 296;
    const int colW = 92;
    const int knobH = 74;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int col, int yTop)
    {
        const int x = rackX + col * colW;
        l.setBounds(x, yTop, colW, 16);
        s.setBounds(x + 6, yTop + 16, colW - 12, knobH);
    };

    // Row 1: Layer Mix
    placeKnob(scannerMixSlider_, scannerMixLabel_, 0, 92);
    placeKnob(orbiterMixSlider_, orbiterMixLabel_, 1, 92);
    placeKnob(wavetableMixSlider_, wavetableMixLabel_, 2, 92);

    // Row 2: Motion & Morph
    placeKnob(scanSpeedSlider_, scanSpeedLabel_, 0, 214);
    placeKnob(edgeSensSlider_, edgeSensLabel_, 1, 214);
    placeKnob(wtMorphSlider_, wtMorphLabel_, 2, 214);

    // Row 3: Morph LFO + Orbiter Motion Mode selector
    placeKnob(wtLfoSlider_, wtLfoLabel_, 0, 314);
    orbiterModeLabel_.setBounds(rackX + colW + 6, 326, colW * 2 - 12, 18);
    orbiterModeBox_.setBounds(rackX + colW + 6, 348, colW * 2 - 16, 28);

    // Row 4: Color Filter & Space
    placeKnob(cutoffSlider_, cutoffLabel_, 0, 440);
    placeKnob(resoSlider_, resoLabel_, 1, 440);
    placeKnob(reverbSlider_, reverbLabel_, 2, 440);

    // Bottom status + MIDI keyboard
    statusLabel_.setBounds(14, 554, getWidth() - 28, 18);
    midiKeyboard_.setBounds(12, 576, getWidth() - 24, 64);
}

} // namespace soundcanvas
