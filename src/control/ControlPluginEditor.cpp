#include "control/ControlPluginEditor.h"
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

SoundCanvasControlAudioProcessorEditor::SoundCanvasControlAudioProcessorEditor(SoundCanvasControlAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      canvasComponent_(p),
      moduleRack_(p)
{
    setLookAndFeel(&lookAndFeel_);

    auto& apvts = processor_.getApvts();

    // --- Top Studio Header ---
    addAndMakeVisible(loadPaintingButton_);
    loadPaintingButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2d44));
    loadPaintingButton_.onClick = [this]() { openPaintingFileChooser(); };

    addAndMakeVisible(presetSelector_);
    presetSelector_.addItem("Starry Nocturne", 1);
    presetSelector_.addItem("Sunset Impasto", 2);
    presetSelector_.addItem("Watercolor Botanicals", 3);
    presetSelector_.setSelectedId(std::max(1, processor_.getCurrentPresetIndex() + 1), juce::dontSendNotification);
    presetSelector_.onChange = [this]()
    {
        const int idx = presetSelector_.getSelectedId() - 1;
        if (idx >= 0)
        {
            processor_.loadStarterPreset(idx);
            statusIsError_ = false;
            statusText_ = "Loaded canvas: " + presetSelector_.getText();
            canvasComponent_.repaint();
            repaint(chainHeaderBounds_);
        }
    };

    addAndMakeVisible(rootNoteBox_);
    rootNoteBox_.addItemList(ScaleQuantizer::getRootNoteNames(), 1);
    rootNoteAtt_ = std::make_unique<ComboBoxAttachment>(apvts, "rootNote", rootNoteBox_);

    addAndMakeVisible(scaleTypeBox_);
    scaleTypeBox_.addItemList(ScaleQuantizer::getScaleNames(), 1);
    scaleTypeAtt_ = std::make_unique<ComboBoxAttachment>(apvts, "scaleType", scaleTypeBox_);

    addAndMakeVisible(runningToggle_);
    runningToggle_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff34d399));
    runningAtt_ = std::make_unique<ButtonAttachment>(apvts, "running", runningToggle_);

    addAndMakeVisible(hostSyncToggle_);
    hostSyncToggle_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff38bdf8));
    hostSyncAtt_ = std::make_unique<ButtonAttachment>(apvts, "hostSync", hostSyncToggle_);

    addAndMakeVisible(internalBpmSlider_);
    configureBarSlider(internalBpmSlider_, " BPM");
    internalBpmSlider_.setColour(juce::Slider::trackColourId, juce::Colour(0xff38bdf8));
    internalBpmAtt_ = std::make_unique<SliderAttachment>(apvts, "internalBpm", internalBpmSlider_);

    addAndMakeVisible(previewSoundBox_);
    previewSoundBox_.addItemList(PreviewSynthEngine::getModeNames(), 1);
    previewSoundAtt_ = std::make_unique<ComboBoxAttachment>(apvts, "previewSoundMode", previewSoundBox_);
    previewSoundBox_.onChange = [this]()
    {
        const bool soundOn = previewSoundBox_.getSelectedId() > 1;
        previewSoundBox_.setColour(juce::ComboBox::outlineColourId,
                                   soundOn ? juce::Colour(0xff34d399).withAlpha(0.85f)
                                           : juce::Colour(ControlPalette::kBorderSubtle));
        previewSoundBox_.setColour(juce::ComboBox::backgroundColourId,
                                   soundOn ? juce::Colour(0xff12241f)
                                           : juce::Colour(ControlPalette::kSurfaceInput));
        previewSoundBox_.repaint();
    };
    previewSoundBox_.onChange();

    addAndMakeVisible(panicButton_);
    panicButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a1d2a));
    panicButton_.onClick = [this]()
    {
        processor_.triggerPanic();
        statusIsError_ = false;
        statusText_ = "MIDI Panic: All Notes Off sent across channels 1-16.";
        repaint(chainHeaderBounds_);
    };

    // --- Path & Shape Toolbar ---
    for (int i = 0; i < RackDocument::kMaxPaths; ++i)
    {
        auto& btn = pathTabButtons_[static_cast<size_t>(i)];
        addChildComponent(btn);
        btn.setButtonText("Path " + juce::String(i + 1));
        btn.onClick = [this, i]() { selectPath(i); };
    }

    addAndMakeVisible(addPathButton_);
    addPathButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18263a));
    addPathButton_.onClick = [this]() { addNewPath(); };

    addAndMakeVisible(removePathButton_);
    removePathButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c1922));
    removePathButton_.onClick = [this]() { removeSelectedPath(); };

    const auto shapes = ImageMidiControlEngine::getShapePresetNames();
    for (int s = 0; s < 6; ++s)
    {
        auto& sb = shapeButtons_[static_cast<size_t>(s)];
        addAndMakeVisible(sb);
        sb.setButtonText(shapes[s]);
        sb.onClick = [this, s, shapes]()
        {
            processor_.updateDocument([this, s](RackDocument& d)
            {
                ImageMidiControlEngine::applyShapePreset(d.paths[static_cast<size_t>(selectedPath_)], s);
            });
            statusIsError_ = false;
            statusText_ = "Applied " + shapes[s] + " shape to Path " + juce::String(selectedPath_ + 1) + ".";
            syncPathToolbar();
            canvasComponent_.repaint();
            repaint(chainHeaderBounds_);
        };
    }

    addAndMakeVisible(closedLoopToggle_);
    closedLoopToggle_.onClick = [this]()
    {
        const bool closed = closedLoopToggle_.getToggleState();
        processor_.updateDocument([this, closed](RackDocument& d)
        {
            d.paths[static_cast<size_t>(selectedPath_)].closedLoop = closed;
        });
        canvasComponent_.repaint();
    };

    addAndMakeVisible(smoothCurveToggle_);
    smoothCurveToggle_.onClick = [this]()
    {
        const bool smooth = smoothCurveToggle_.getToggleState();
        processor_.updateDocument([this, smooth](RackDocument& d)
        {
            d.paths[static_cast<size_t>(selectedPath_)].smoothCurve = smooth;
        });
        canvasComponent_.repaint();
    };

    // --- Hero Painting Canvas ---
    addAndMakeVisible(canvasComponent_);
    canvasComponent_.onPathSelected = [this](int idx) { selectPath(idx); };
    canvasComponent_.onStatusMessage = [this](const juce::String& txt, bool isErr)
    {
        statusText_ = txt;
        statusIsError_ = isErr;
        repaint(chainHeaderBounds_);
    };

    // --- Chain Header + Ableton-Style Horizontal Device Rack ---
    addAndMakeVisible(addModuleButton_);
    addModuleButton_.onClick = [this]() { moduleRack_.showAddModuleMenu(&addModuleButton_); };

    addAndMakeVisible(moduleRack_);
    moduleRack_.onStatusMessage = [this](const juce::String& txt)
    {
        statusText_ = txt;
        statusIsError_ = false;
        repaint(chainHeaderBounds_);
    };

    setSize(1200, 760);
    selectPath(0);
    syncPathToolbar();
    startTimerHz(30);
}

SoundCanvasControlAudioProcessorEditor::~SoundCanvasControlAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SoundCanvasControlAudioProcessorEditor::configureBarSlider(juce::Slider& s, const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::LinearBar);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ControlPalette::kTextPrimary));
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void SoundCanvasControlAudioProcessorEditor::selectPath(int pathIndex)
{
    const auto doc = processor_.getDocumentCopy();
    int clamped = std::clamp(pathIndex, 0, RackDocument::kMaxPaths - 1);
    if (!doc.paths[static_cast<size_t>(clamped)].active)
    {
        for (int i = 0; i < RackDocument::kMaxPaths; ++i)
        {
            if (doc.paths[static_cast<size_t>(i)].active)
            {
                clamped = i;
                break;
            }
        }
    }

    selectedPath_ = clamped;
    canvasComponent_.setSelectedPath(selectedPath_);
    moduleRack_.setSelectedPath(selectedPath_);
    syncPathToolbar();
    repaint();
}

void SoundCanvasControlAudioProcessorEditor::addNewPath()
{
    int addedIdx = -1;
    processor_.updateDocument([&](RackDocument& d)
    {
        for (int i = 0; i < RackDocument::kMaxPaths; ++i)
        {
            if (!d.paths[static_cast<size_t>(i)].active)
            {
                d.paths[static_cast<size_t>(i)] = ImageMidiControlEngine::createDefaultPath(i, d.nextModuleUid);
                d.paths[static_cast<size_t>(i)].active = true;
                addedIdx = i;
                break;
            }
        }
    });

    if (addedIdx >= 0)
    {
        statusIsError_ = false;
        statusText_ = "Added Pickup Path " + juce::String(addedIdx + 1) + " with its own modular chain.";
        selectPath(addedIdx);
    }
}

void SoundCanvasControlAudioProcessorEditor::removeSelectedPath()
{
    int nextSel = -1;
    processor_.updateDocument([&](RackDocument& d)
    {
        int activeCount = 0;
        for (const auto& p : d.paths)
            if (p.active)
                ++activeCount;
        if (activeCount <= 1)
            return;

        d.paths[static_cast<size_t>(selectedPath_)].active = false;
        for (int i = 0; i < RackDocument::kMaxPaths; ++i)
        {
            if (d.paths[static_cast<size_t>(i)].active)
            {
                nextSel = i;
                break;
            }
        }
    });

    if (nextSel >= 0)
    {
        statusIsError_ = false;
        statusText_ = "Removed Pickup Path.";
        selectPath(nextSel);
    }
}

void SoundCanvasControlAudioProcessorEditor::syncPathToolbar()
{
    const auto doc = processor_.getDocumentCopy();
    lastSyncedDocRev_ = processor_.getDocumentRevision();

    int activeCount = 0;
    int x = 12;
    const int y = 58;
    const int h = 26;

    for (int i = 0; i < RackDocument::kMaxPaths; ++i)
    {
        const bool isActive = doc.paths[static_cast<size_t>(i)].active;
        auto& btn = pathTabButtons_[static_cast<size_t>(i)];
        btn.setVisible(isActive);
        if (isActive)
        {
            ++activeCount;
            const juce::Colour c = ControlPalette::orbiterAccent(i);
            const bool isSel = (i == selectedPath_);
            btn.setToggleState(isSel, juce::dontSendNotification);
            btn.setColour(juce::TextButton::buttonColourId, isSel ? c.withAlpha(0.26f) : juce::Colour(0xff141824));
            btn.setColour(juce::TextButton::buttonOnColourId, c.withAlpha(0.26f));
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(ControlPalette::kTextMuted));
            btn.setBounds(x, y, 72, h);
            x += 78;
        }
    }

    addPathButton_.setVisible(activeCount < RackDocument::kMaxPaths);
    if (addPathButton_.isVisible())
    {
        addPathButton_.setBounds(x, y, 64, h);
        x += 70;
    }

    removePathButton_.setVisible(activeCount > 1);
    if (removePathButton_.isVisible())
    {
        removePathButton_.setBounds(x, y, 62, h);
    }

    const auto& selPath = doc.paths[static_cast<size_t>(selectedPath_)];
    const juce::Colour accent = ControlPalette::orbiterAccent(selectedPath_);

    closedLoopToggle_.setToggleState(selPath.closedLoop, juce::dontSendNotification);
    closedLoopToggle_.setColour(juce::ToggleButton::tickColourId, accent);

    smoothCurveToggle_.setToggleState(selPath.smoothCurve, juce::dontSendNotification);
    smoothCurveToggle_.setColour(juce::ToggleButton::tickColourId, accent);

    addModuleButton_.setColour(juce::TextButton::buttonColourId, accent.withAlpha(0.24f));
    addModuleButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

void SoundCanvasControlAudioProcessorEditor::openPaintingFileChooser()
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
            statusIsError_ = false;
            statusText_ = "Loaded painting: " + result.getFileName();
            canvasComponent_.repaint();
        }
        else
        {
            statusIsError_ = true;
            statusText_ = errorMsg;
        }
        repaint(chainHeaderBounds_);
    });
}

void SoundCanvasControlAudioProcessorEditor::timerCallback()
{
    const int activePreset = processor_.getCurrentPresetIndex();
    if (activePreset >= 0 && presetSelector_.getSelectedId() != activePreset + 1)
        presetSelector_.setSelectedId(activePreset + 1, juce::dontSendNotification);

    if (processor_.getDocumentRevision() != lastSyncedDocRev_)
        syncPathToolbar();
}

void SoundCanvasControlAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ControlPalette::kBgDeep));

    const juce::Colour accent = ControlPalette::orbiterAccent(selectedPath_);

    // --- Top Studio Header (50px high) ---
    {
        g.setColour(juce::Colour(ControlPalette::kBgHeader));
        g.fillRect(0, 0, getWidth(), 50);
        g.setColour(juce::Colour(ControlPalette::kBorderSubtle));
        g.drawLine(0.0f, 50.0f, static_cast<float>(getWidth()), 50.0f, 1.0f);

        g.setColour(accent.withAlpha(0.28f));
        g.fillEllipse(12.0f, 13.0f, 24.0f, 24.0f);
        g.setColour(accent);
        g.fillEllipse(18.0f, 19.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillEllipse(21.5f, 22.5f, 5.0f, 5.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(14.5f, juce::Font::bold));
        g.drawText("SOUND CANVAS", 42, 8, 152, 18, juce::Justification::centredLeft, false);

        g.setColour(accent.withAlpha(0.90f));
        g.setFont(juce::FontOptions(9.2f, juce::Font::bold));
        g.drawText("MODULAR PICKUP CONTROL", 42, 27, 162, 14, juce::Justification::centredLeft, false);
    }

    // --- Chain Header Strip (calm, zero-flicker header between Canvas and Ableton Rack) ---
    if (!chainHeaderBounds_.isEmpty())
    {
        const int x = chainHeaderBounds_.getX();
        const int y = chainHeaderBounds_.getY();
        const int h = chainHeaderBounds_.getHeight();

        // Active Path Chain pill label
        const juce::Rectangle<float> tagRect(static_cast<float>(x), static_cast<float>(y + 3), 138.0f, 22.0f);
        g.setColour(accent.withAlpha(0.16f));
        g.fillRoundedRectangle(tagRect, 6.0f);
        g.setColour(accent.withAlpha(0.70f));
        g.drawRoundedRectangle(tagRect, 6.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("PATH " + juce::String(selectedPath_ + 1) + "  DEVICE CHAIN",
                   tagRect, juce::Justification::centred, false);

        // Clean status / hint text (no flickering debug MIDI pill stream)
        const int statusX = static_cast<int>(tagRect.getRight()) + 14;
        const int statusW = addModuleButton_.getX() - statusX - 12;
        if (statusW > 80)
        {
            g.setColour(statusIsError_ ? juce::Colour(0xfff87171) : juce::Colour(ControlPalette::kTextDim));
            g.setFont(juce::FontOptions(10.2f));
            g.drawFittedText(statusText_, statusX, y, statusW, h, juce::Justification::centredLeft, 1);
        }
    }
}

void SoundCanvasControlAudioProcessorEditor::resized()
{
    // Top studio header (50px)
    loadPaintingButton_.setBounds(212, 11, 70, 28);
    presetSelector_.setBounds(288, 11, 162, 28);
    rootNoteBox_.setBounds(458, 11, 56, 28);
    scaleTypeBox_.setBounds(520, 11, 154, 28);
    runningToggle_.setBounds(686, 11, 62, 28);
    hostSyncToggle_.setBounds(754, 11, 64, 28);
    internalBpmSlider_.setBounds(826, 12, 146, 26);
    previewSoundBox_.setBounds(980, 11, 124, 28);
    panicButton_.setBounds(1112, 11, 76, 28);

    // Path & Shape Toolbar (y = 58..84)
    syncPathToolbar();
    int sx = getWidth() - 12;
    smoothCurveToggle_.setBounds(sx - 78, 58, 78, 26); sx -= 84;
    closedLoopToggle_.setBounds(sx - 66, 58, 66, 26);  sx -= 74;
    for (int s = 5; s >= 0; --s)
    {
        shapeButtons_[static_cast<size_t>(s)].setBounds(sx - 56, 58, 56, 26);
        sx -= 60;
    }

    // Hero Painting Canvas (y = 92..446)
    const int contentW = getWidth() - 24;
    canvasComponent_.setBounds(12, 92, contentW, 354);

    // Chain Header Strip (y = 452..480)
    chainHeaderBounds_ = { 12, 452, contentW, 28 };
    addModuleButton_.setBounds(getWidth() - 12 - 118, 454, 118, 24);

    // Bottom Ableton-Style Horizontal Device Rack (y = 484..750)
    moduleRack_.setBounds(12, 484, contentW, getHeight() - 484 - 10);
}

} // namespace soundcanvas
