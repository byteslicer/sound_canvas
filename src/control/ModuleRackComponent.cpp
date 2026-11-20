#include "control/ModuleRackComponent.h"
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

namespace
{
juce::String formatMidiNote(int note)
{
    static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (note < 0 || note > 127)
        return "--";
    return juce::String(kNames[note % 12]) + juce::String((note / 12) - 1);
}

juce::Colour moduleCategoryAccent(ModuleType type) noexcept
{
    switch (type)
    {
        case ModuleType::SignalShaper:  return juce::Colour(0xff38bdf8); // Electric Sky
        case ModuleType::TriggerGate:   return juce::Colour(0xfffbbf24); // Solar Amber
        case ModuleType::NoteGenerator: return juce::Colour(0xff34d399); // Bioluminescent Emerald
        case ModuleType::Chord:
        case ModuleType::Chance:
        case ModuleType::MidiEcho:      return juce::Colour(0xffc084fc); // Orchid Violet (MIDI FX)
        case ModuleType::CcMapper:      return juce::Colour(0xfffb7185); // Coral Rose
        case ModuleType::Count:
        default:                        return juce::Colour(0xff38bdf8);
    }
}

juce::String moduleShortTitle(ModuleType type)
{
    switch (type)
    {
        case ModuleType::SignalShaper:  return "SHAPER";
        case ModuleType::TriggerGate:   return "TRIG GATE";
        case ModuleType::NoteGenerator: return "NOTE GEN";
        case ModuleType::Chord:         return "CHORD FX";
        case ModuleType::Chance:        return "CHANCE FX";
        case ModuleType::MidiEcho:      return "MIDI ECHO";
        case ModuleType::CcMapper:      return "CC MAPPER";
        case ModuleType::Count:
        default:                        return "MODULE";
    }
}

void setupCompactBar(juce::Slider& s, double minV, double maxV, double step, double val,
                     const juce::String& suffix, juce::Colour accent)
{
    s.setSliderStyle(juce::Slider::LinearBar);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 64, 18);
    s.setRange(minV, maxV, step);
    s.setValue(val, juce::dontSendNotification);
    s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::trackColourId, accent);
    s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(ControlPalette::kTextPrimary));
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void setupMicroLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setFont(juce::FontOptions(9.2f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, juce::Colour(ControlPalette::kTextMuted));
    l.setJustificationType(juce::Justification::centredLeft);
}

uint64_t computeStructuralHash(int selectedPath, const PickupPathConfig& p) noexcept
{
    uint64_t h = 1469598103934665603ULL;
    auto mix = [&h](uint64_t v) noexcept
    {
        h ^= v;
        h *= 1099511628211ULL;
    };

    mix(static_cast<uint64_t>(selectedPath));
    mix(static_cast<uint64_t>(p.active ? 1 : 0));
    mix(static_cast<uint64_t>(p.muted ? 1 : 0));
    mix(static_cast<uint64_t>(p.loopLengthIdx));
    mix(static_cast<uint64_t>(p.playMode));
    mix(static_cast<uint64_t>(p.numModules));
    for (int i = 0; i < std::clamp(p.numModules, 0, PickupPathConfig::kMaxModules); ++i)
    {
        const auto& m = p.modules[static_cast<size_t>(i)];
        mix(m.uid);
        mix(static_cast<uint64_t>(m.type));
        mix(static_cast<uint64_t>(m.enabled ? 1 : 0));
    }
    return h;
}
} // namespace

// ============================================================================
// Fixed Far-Left PICKUP Device Card (Path Timing + Live Raw Signal Bus)
// ============================================================================
class ModuleRackComponent::PickupCard : public juce::Component
{
public:
    PickupCard(SoundCanvasControlAudioProcessor& proc, int pathIdx)
        : processor_(proc), pathIndex_(pathIdx)
    {
        const auto doc = processor_.getDocumentCopy();
        const auto& p = doc.paths[static_cast<size_t>(pathIndex_)];
        const juce::Colour accent = ControlPalette::orbiterAccent(pathIndex_);

        addAndMakeVisible(muteBtn_);
        muteBtn_.setButtonText(p.muted ? "Muted" : "On");
        muteBtn_.setToggleState(!p.muted, juce::dontSendNotification);
        muteBtn_.setColour(juce::ToggleButton::tickColourId, accent);
        muteBtn_.onClick = [this]()
        {
            const bool muted = !muteBtn_.getToggleState();
            muteBtn_.setButtonText(muted ? "Muted" : "On");
            processor_.updateDocument([this, muted](RackDocument& d)
            {
                d.paths[static_cast<size_t>(pathIndex_)].muted = muted;
            });
        };

        addAndMakeVisible(loopLbl_);
        setupMicroLabel(loopLbl_, "LOOP");
        addAndMakeVisible(loopBox_);
        loopBox_.addItemList(ImageMidiControlEngine::getLoopLengthNames(), 1);
        loopBox_.setSelectedId(std::clamp(p.loopLengthIdx, 0, 5) + 1, juce::dontSendNotification);
        loopBox_.onChange = [this]()
        {
            const int idx = std::max(0, loopBox_.getSelectedId() - 1);
            processor_.updateDocument([this, idx](RackDocument& d)
            {
                d.paths[static_cast<size_t>(pathIndex_)].loopLengthIdx = idx;
            });
        };

        addAndMakeVisible(modeLbl_);
        setupMicroLabel(modeLbl_, "DIR");
        addAndMakeVisible(modeBox_);
        modeBox_.addItemList(ImageMidiControlEngine::getPlayModeNames(), 1);
        modeBox_.setSelectedId(std::clamp(p.playMode, 0, 2) + 1, juce::dontSendNotification);
        modeBox_.onChange = [this]()
        {
            const int idx = std::max(0, modeBox_.getSelectedId() - 1);
            processor_.updateDocument([this, idx](RackDocument& d)
            {
                d.paths[static_cast<size_t>(pathIndex_)].playMode = idx;
            });
        };

        addAndMakeVisible(phaseLbl_);
        setupMicroLabel(phaseLbl_, "PHS");
        addAndMakeVisible(phaseSlider_);
        setupCompactBar(phaseSlider_, 0.0, 1.0, 0.01, p.phaseOffset, "", accent);
        phaseSlider_.onValueChange = [this]()
        {
            const float v = static_cast<float>(phaseSlider_.getValue());
            processor_.updateDocument([this, v](RackDocument& d)
            {
                d.paths[static_cast<size_t>(pathIndex_)].phaseOffset = v;
            });
        };
    }

    void paint(juce::Graphics& g) override
    {
        const auto bf = getLocalBounds().toFloat();
        const juce::Colour accent = ControlPalette::orbiterAccent(pathIndex_);
        const auto snap = processor_.getEngine().getVisualSnapshot();
        const auto& vis = snap.paths[static_cast<size_t>(pathIndex_)];

        g.setColour(juce::Colour(0xff131722));
        g.fillRoundedRectangle(bf, 9.0f);
        g.setColour(accent.withAlpha(0.55f));
        g.drawRoundedRectangle(bf.reduced(0.5f), 9.0f, 1.1f);

        // Header bar
        const auto header = bf.withHeight(26.0f);
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(bf, 9.0f);
        g.reduceClipRegion(clip);
        g.setColour(accent.withAlpha(0.18f));
        g.fillRect(header);
        g.setColour(accent.withAlpha(0.85f));
        g.fillRect(bf.getX(), bf.getY(), bf.getWidth(), 2.2f);
        g.restoreState();

        // Live sampled painting color swatch dot in header
        const juce::Colour pxCol = juce::Colour::fromFloatRGBA(
            std::clamp(vis.r, 0.0f, 1.0f),
            std::clamp(vis.g, 0.0f, 1.0f),
            std::clamp(vis.b, 0.0f, 1.0f), 1.0f);
        g.setColour(pxCol);
        g.fillEllipse(10.0f, 8.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawEllipse(10.0f, 8.0f, 10.0f, 10.0f, 1.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(10.2f, juce::Font::bold));
        g.drawText("PICKUP  /  P" + juce::String(pathIndex_ + 1), 26, 4, 114, 18, juce::Justification::centredLeft, false);

        // Raw Signal Telemetry Grid (2 columns x 6 rows)
        g.setColour(juce::Colour(ControlPalette::kTextDim));
        g.setFont(juce::FontOptions(8.8f, juce::Font::bold));
        g.drawText("RAW SIGNAL BUS", 10, 112, 190, 12, juce::Justification::centredLeft, false);

        static constexpr int kShowSignals[12] = {
            static_cast<int>(SignalId::Lum),
            static_cast<int>(SignalId::Hue),
            static_cast<int>(SignalId::Sat),
            static_cast<int>(SignalId::Edge),
            static_cast<int>(SignalId::PosX),
            static_cast<int>(SignalId::PosY),
            static_cast<int>(SignalId::Red),
            static_cast<int>(SignalId::Green),
            static_cast<int>(SignalId::Blue),
            static_cast<int>(SignalId::DeltaColor),
            static_cast<int>(SignalId::ModA),
            static_cast<int>(SignalId::ModB)
        };
        const auto shortNames = ImageMidiControlEngine::getSignalShortNames();

        const float colW = (bf.getWidth() - 26.0f) * 0.5f;
        const float startY = 128.0f;
        const float rowH = 18.5f;

        for (int i = 0; i < 12; ++i)
        {
            const int col = i / 6;
            const int row = i % 6;
            const float rx = 10.0f + static_cast<float>(col) * (colW + 6.0f);
            const float ry = startY + static_cast<float>(row) * rowH;

            const int sigIdx = kShowSignals[i];
            const float val = std::clamp(vis.signals[static_cast<size_t>(sigIdx)], 0.0f, 1.0f);

            g.setColour(juce::Colour(ControlPalette::kTextMuted));
            g.setFont(juce::FontOptions(8.6f, juce::Font::bold));
            g.drawText(shortNames[sigIdx], juce::Rectangle<float>(rx, ry, 25.0f, 14.0f),
                       juce::Justification::centredLeft, false);

            const juce::Rectangle<float> bar(rx + 26.0f, ry + 2.5f, colW - 28.0f, 9.0f);
            g.setColour(juce::Colour(0xff0b0e16));
            g.fillRoundedRectangle(bar, 3.0f);

            juce::Colour barCol = accent;
            if (sigIdx == static_cast<int>(SignalId::Red)) barCol = juce::Colour(0xfff87171);
            else if (sigIdx == static_cast<int>(SignalId::Green)) barCol = juce::Colour(0xff4ade80);
            else if (sigIdx == static_cast<int>(SignalId::Blue)) barCol = juce::Colour(0xff60a5fa);
            else if (sigIdx == static_cast<int>(SignalId::ModA) || sigIdx == static_cast<int>(SignalId::ModB))
                barCol = juce::Colour(0xffc084fc);

            if (val > 0.008f)
            {
                g.setColour(barCol.withAlpha(0.85f));
                g.fillRoundedRectangle(bar.getX() + 1.0f, bar.getY() + 1.0f,
                                       (bar.getWidth() - 2.0f) * val, bar.getHeight() - 2.0f, 2.0f);
            }
            g.setColour(juce::Colour(ControlPalette::kBorderSubtle));
            g.drawRoundedRectangle(bar, 3.0f, 0.7f);
        }
    }

    void resized() override
    {
        muteBtn_.setBounds(getWidth() - 62, 4, 56, 18);

        const int x = 10;
        const int w = getWidth() - 20;
        loopLbl_.setBounds(x, 33, 34, 21);
        loopBox_.setBounds(x + 36, 33, w - 36, 21);

        modeLbl_.setBounds(x, 59, 34, 21);
        modeBox_.setBounds(x + 36, 59, w - 36, 21);

        phaseLbl_.setBounds(x, 85, 34, 20);
        phaseSlider_.setBounds(x + 36, 85, w - 36, 20);
    }

private:
    SoundCanvasControlAudioProcessor& processor_;
    int pathIndex_ = 0;

    juce::ToggleButton muteBtn_ { "On" };
    juce::Label loopLbl_, modeLbl_, phaseLbl_;
    juce::ComboBox loopBox_, modeBox_;
    juce::Slider phaseSlider_;
};

// ============================================================================
// Stackable Ableton-Style Module Card
// ============================================================================
class ModuleRackComponent::ModuleCard : public juce::Component
{
public:
    ModuleCard(SoundCanvasControlAudioProcessor& proc,
               int pathIdx,
               int slotIdx,
               const ModuleConfig& cfg,
               int totalModules,
               std::function<void(int)> onToggleEnable,
               std::function<void(int, int)> onMove,
               std::function<void(int)> onDelete)
        : processor_(proc),
          pathIndex_(pathIdx),
          slotIndex_(slotIdx),
          type_(cfg.type),
          enabled_(cfg.enabled),
          accent_(moduleCategoryAccent(cfg.type))
    {
        // Header controls: Ableton-style Power LED, Move < >, Delete x
        addAndMakeVisible(powerBtn_);
        powerBtn_.setButtonText("");
        powerBtn_.setToggleState(cfg.enabled, juce::dontSendNotification);
        powerBtn_.setColour(juce::ToggleButton::tickColourId, accent_);
        powerBtn_.onClick = [slotIdx, onToggleEnable = std::move(onToggleEnable)]()
        {
            if (onToggleEnable)
                onToggleEnable(slotIdx);
        };

        auto styleMiniBtn = [this](juce::TextButton& b, const juce::String& txt, bool enabled)
        {
            addAndMakeVisible(b);
            b.setButtonText(txt);
            b.setEnabled(enabled);
            b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff181d2b));
            b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
        };

        styleMiniBtn(leftBtn_, "<", slotIdx > 0);
        leftBtn_.onClick = [slotIdx, onMove]() { if (onMove) onMove(slotIdx, -1); };

        styleMiniBtn(rightBtn_, ">", slotIdx + 1 < totalModules);
        rightBtn_.onClick = [slotIdx, onMove]() { if (onMove) onMove(slotIdx, +1); };

        styleMiniBtn(delBtn_, "x", true);
        delBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff351923));
        delBtn_.onClick = [slotIdx, onDelete = std::move(onDelete)]()
        {
            if (onDelete)
                onDelete(slotIdx);
        };

        buildBodyControls(cfg);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bf = getLocalBounds().toFloat();
        const float alpha = enabled_ ? 1.0f : 0.45f;

        g.setColour(juce::Colour(0xff141824));
        g.fillRoundedRectangle(bf, 9.0f);
        g.setColour(enabled_ ? accent_.withAlpha(0.45f) : juce::Colour(ControlPalette::kBorderSubtle));
        g.drawRoundedRectangle(bf.reduced(0.5f), 9.0f, 1.0f);

        // Ableton-style tinted header strip
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(bf, 9.0f);
        g.reduceClipRegion(clip);
        g.setColour(accent_.withAlpha(enabled_ ? 0.16f : 0.05f));
        g.fillRect(bf.withHeight(26.0f));
        g.setColour(accent_.withAlpha(enabled_ ? 0.90f : 0.25f));
        g.fillRect(bf.getX(), bf.getY(), bf.getWidth(), 2.2f);
        g.restoreState();

        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(moduleShortTitle(type_), 28, 4, 102, 18, juce::Justification::centredLeft, false);

        // Live telemetry footer inside the module card!
        const auto snap = processor_.getEngine().getVisualSnapshot();
        const auto& mvis = snap.paths[static_cast<size_t>(pathIndex_)].modules[static_cast<size_t>(slotIndex_)];
        const juce::Rectangle<float> foot(10.0f, bf.getBottom() - 26.0f, bf.getWidth() - 20.0f, 18.0f);

        g.setColour(juce::Colour(0xff0b0e16));
        g.fillRoundedRectangle(foot, 5.0f);

        const float val = std::clamp(mvis.liveValue, 0.0f, 1.0f);
        const float pulse = std::clamp(mvis.activityPulse, 0.0f, 1.0f);

        if (type_ == ModuleType::SignalShaper || type_ == ModuleType::CcMapper || type_ == ModuleType::TriggerGate)
        {
            if (val > 0.005f)
            {
                g.setColour(accent_.withAlpha(0.55f * alpha));
                g.fillRoundedRectangle(foot.getX() + 1.0f, foot.getY() + 1.0f,
                                       (foot.getWidth() - 2.0f) * val, foot.getHeight() - 2.0f, 4.0f);
            }
            if (type_ == ModuleType::TriggerGate)
            {
                g.setColour(accent_.withAlpha((0.18f + 0.72f * pulse) * alpha));
                g.fillEllipse(foot.getX() + 5.0f, foot.getCentreY() - 3.5f, 7.0f, 7.0f);
            }
            const juce::String txt = (type_ == ModuleType::TriggerGate)
                ? juce::String(val, 2)
                : juce::String(mvis.lastValueInt);
            g.setColour(juce::Colours::white.withAlpha(0.92f * alpha));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(txt, foot.reduced(6.0f, 0.0f), juce::Justification::centredRight, false);
        }
        else // NoteGen, Chord, Chance, MidiEcho -> Live activity pulse + Note badge
        {
            if (pulse > 0.04f)
            {
                g.setColour(accent_.withAlpha(0.32f * pulse));
                g.fillRoundedRectangle(foot.reduced(1.0f), 4.0f);
            }
            g.setColour(pulse > 0.08f ? accent_ : juce::Colour(ControlPalette::kBorderBright));
            g.fillEllipse(foot.getX() + 6.0f, foot.getCentreY() - 3.5f, 7.0f, 7.0f);

            const juce::String noteTxt = (type_ == ModuleType::Chance)
                ? ("P: " + juce::String(static_cast<int>(std::round(val * 100.0f))) + "%")
                : ("Note: " + formatMidiNote(mvis.lastNote));
            g.setColour(juce::Colours::white.withAlpha(0.92f * alpha));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(noteTxt, foot.reduced(8.0f, 0.0f), juce::Justification::centredRight, false);
        }

        g.setColour(juce::Colour(ControlPalette::kBorderSubtle));
        g.drawRoundedRectangle(foot, 5.0f, 0.8f);
    }

    void resized() override
    {
        powerBtn_.setBounds(6, 4, 20, 18);
        delBtn_.setBounds(getWidth() - 24, 4, 18, 18);
        rightBtn_.setBounds(getWidth() - 44, 4, 18, 18);
        leftBtn_.setBounds(getWidth() - 64, 4, 18, 18);

        const int x = 10;
        const int w = getWidth() - 20;
        const int rowH = 22;
        const int gap = 6;
        int y = 34;

        for (auto& row : rows_)
        {
            if (row.label != nullptr && row.primary != nullptr && row.secondary != nullptr)
            {
                row.label->setBounds(x, y, 36, rowH);
                const int halfW = (w - 40) / 2;
                row.primary->setBounds(x + 38, y, halfW, rowH);
                row.secondary->setBounds(x + 42 + halfW, y, w - 42 - halfW, rowH);
            }
            else if (row.label != nullptr && row.primary != nullptr)
            {
                row.label->setBounds(x, y, 42, rowH);
                row.primary->setBounds(x + 44, y, w - 44, rowH);
            }
            else if (row.primary != nullptr && row.secondary != nullptr)
            {
                const int halfW = (w - 4) / 2;
                row.primary->setBounds(x, y, halfW, rowH);
                row.secondary->setBounds(x + halfW + 4, y, w - halfW - 4, rowH);
            }
            else if (row.primary != nullptr)
            {
                row.primary->setBounds(x, y, w, rowH);
            }
            y += rowH + gap;
        }
    }

private:
    struct LayoutRow
    {
        juce::Component* label = nullptr;
        juce::Component* primary = nullptr;
        juce::Component* secondary = nullptr;
    };

    void mutateModule(std::function<void(ModuleConfig&)> fn)
    {
        const int pIdx = pathIndex_;
        const int sIdx = slotIndex_;
        processor_.updateDocument([pIdx, sIdx, fn = std::move(fn)](RackDocument& d)
        {
            auto& path = d.paths[static_cast<size_t>(pIdx)];
            if (sIdx >= 0 && sIdx < path.numModules)
                fn(path.modules[static_cast<size_t>(sIdx)]);
        });
    }

    juce::Label* makeLabel(const juce::String& txt)
    {
        auto l = std::make_unique<juce::Label>();
        setupMicroLabel(*l, txt);
        addAndMakeVisible(*l);
        auto* ptr = l.get();
        ownedWidgets_.push_back(std::move(l));
        return ptr;
    }

    juce::ComboBox* makeCombo(const juce::StringArray& items, int selectedZeroBased, std::function<void(int)> onChange)
    {
        auto cb = std::make_unique<juce::ComboBox>();
        cb->addItemList(items, 1);
        cb->setSelectedId(std::clamp(selectedZeroBased, 0, std::max(0, items.size() - 1)) + 1, juce::dontSendNotification);
        auto* ptr = cb.get();
        cb->onChange = [ptr, onChange = std::move(onChange)]()
        {
            if (onChange)
                onChange(std::max(0, ptr->getSelectedId() - 1));
        };
        addAndMakeVisible(*cb);
        ownedWidgets_.push_back(std::move(cb));
        return ptr;
    }

    juce::Slider* makeBar(double minV, double maxV, double step, double val,
                          const juce::String& suffix, std::function<void(double)> onChange)
    {
        auto sl = std::make_unique<juce::Slider>();
        setupCompactBar(*sl, minV, maxV, step, val, suffix, accent_);
        auto* ptr = sl.get();
        sl->onValueChange = [ptr, onChange = std::move(onChange)]()
        {
            if (onChange)
                onChange(ptr->getValue());
        };
        addAndMakeVisible(*sl);
        ownedWidgets_.push_back(std::move(sl));
        return ptr;
    }

    juce::ToggleButton* makeToggle(const juce::String& txt, bool on, std::function<void(bool)> onChange)
    {
        auto tb = std::make_unique<juce::ToggleButton>(txt);
        tb->setToggleState(on, juce::dontSendNotification);
        tb->setColour(juce::ToggleButton::tickColourId, accent_);
        auto* ptr = tb.get();
        tb->onClick = [ptr, onChange = std::move(onChange)]()
        {
            if (onChange)
                onChange(ptr->getToggleState());
        };
        addAndMakeVisible(*tb);
        ownedWidgets_.push_back(std::move(tb));
        return ptr;
    }

    void buildBodyControls(const ModuleConfig& cfg)
    {
        const auto sigNames = ImageMidiControlEngine::getSignalNames();
        const auto divNames = ScaleQuantizer::getTempoDivisionNames();

        switch (cfg.type)
        {
            case ModuleType::SignalShaper:
            {
                rows_.push_back({ makeLabel("IN"),
                                  makeCombo(sigNames, cfg.inputSignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.inputSignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("OUT"),
                                  makeCombo(sigNames, cfg.outputSignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.outputSignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("CRV"),
                                  makeCombo(ImageMidiControlEngine::getCurveNames(), cfg.mode, [this](int v) { mutateModule([v](ModuleConfig& m) { m.mode = v; }); }),
                                  makeToggle("Inv", cfg.flagA, [this](bool v) { mutateModule([v](ModuleConfig& m) { m.flagA = v; }); }) });
                rows_.push_back({ makeLabel("MIN"),
                                  makeBar(0.0, 1.0, 0.01, cfg.paramA, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("MAX"),
                                  makeBar(0.0, 1.0, 0.01, cfg.paramB, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramB = static_cast<float>(v); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("SLEW"),
                                  makeBar(0.0, 1000.0, 1.0, cfg.paramC * 1000.0, "ms", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramC = static_cast<float>(v * 0.001); }); }),
                                  nullptr });
                break;
            }

            case ModuleType::TriggerGate:
            {
                rows_.push_back({ makeLabel("MODE"),
                                  makeCombo(ImageMidiControlEngine::getTriggerModeNames(), cfg.mode, [this](int v) { mutateModule([v](ModuleConfig& m) { m.mode = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("SRC"),
                                  makeCombo(sigNames, cfg.inputSignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.inputSignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("DIV"),
                                  makeCombo(divNames, cfg.intA, [this](int v) { mutateModule([v](ModuleConfig& m) { m.intA = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("THR"),
                                  makeBar(0.02, 0.98, 0.01, cfg.paramA, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("STEPS"),
                                  makeBar(2.0, 32.0, 1.0, cfg.intB, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intB = static_cast<int>(v); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("PULSE"),
                                  makeBar(1.0, 32.0, 1.0, cfg.intC, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intC = static_cast<int>(v); }); }),
                                  nullptr });
                break;
            }

            case ModuleType::NoteGenerator:
            {
                rows_.push_back({ makeLabel("TRIG"),
                                  makeCombo(ImageMidiControlEngine::getNoteTrigSourceNames(), cfg.mode, [this](int v) { mutateModule([v](ModuleConfig& m) { m.mode = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("PITCH"),
                                  makeCombo(sigNames, cfg.inputSignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.inputSignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("VEL"),
                                  makeCombo(sigNames, cfg.secondarySignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.secondarySignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("DIV"),
                                  makeCombo(divNames, cfg.intB, [this](int v) { mutateModule([v](ModuleConfig& m) { m.intB = v; }); }),
                                  makeBar(1.0, 16.0, 1.0, cfg.intA, "ch", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intA = static_cast<int>(v); }); }) });
                rows_.push_back({ makeLabel("OCT"),
                                  makeBar(1.0, 6.0, 1.0, cfg.intC, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intC = static_cast<int>(v); }); }),
                                  makeBar(1.0, 4.0, 1.0, cfg.intD, "sp", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intD = static_cast<int>(v); }); }) });
                rows_.push_back({ makeLabel("GATE"),
                                  makeBar(10.0, 95.0, 1.0, cfg.paramA * 100.0, "%", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v * 0.01); }); }),
                                  nullptr });
                break;
            }

            case ModuleType::Chord:
            {
                rows_.push_back({ makeLabel("TYPE"),
                                  makeCombo(ImageMidiControlEngine::getChordTypeNames(), cfg.mode, [this](int v) { mutateModule([v](ModuleConfig& m) { m.mode = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("STRUM"),
                                  makeBar(0.0, 100.0, 1.0, cfg.paramA * 200.0, "ms", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v / 200.0); }); }),
                                  nullptr });
                rows_.push_back({ nullptr,
                                  makeToggle("Gate by Signal", cfg.flagA, [this](bool v) { mutateModule([v](ModuleConfig& m) { m.flagA = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("MOD"),
                                  makeCombo(sigNames, cfg.secondarySignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.secondarySignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("THR"),
                                  makeBar(0.0, 1.0, 0.01, cfg.paramB, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramB = static_cast<float>(v); }); }),
                                  nullptr });
                break;
            }

            case ModuleType::Chance:
            {
                rows_.push_back({ makeLabel("PROB"),
                                  makeBar(0.0, 100.0, 1.0, cfg.paramA * 100.0, "%", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v * 0.01); }); }),
                                  nullptr });
                rows_.push_back({ nullptr,
                                  makeToggle("Mod Prob by Sig", cfg.flagA, [this](bool v) { mutateModule([v](ModuleConfig& m) { m.flagA = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("MOD"),
                                  makeCombo(sigNames, cfg.secondarySignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.secondarySignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("VMIN"),
                                  makeBar(5.0, 100.0, 1.0, cfg.paramB * 100.0, "%", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramB = static_cast<float>(v * 0.01); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("VMAX"),
                                  makeBar(5.0, 100.0, 1.0, cfg.paramC * 100.0, "%", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramC = static_cast<float>(v * 0.01); }); }),
                                  nullptr });
                break;
            }

            case ModuleType::MidiEcho:
            {
                rows_.push_back({ makeLabel("DELAY"),
                                  makeCombo(divNames, cfg.intA, [this](int v) { mutateModule([v](ModuleConfig& m) { m.intA = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("TAPS"),
                                  makeBar(1.0, 6.0, 1.0, cfg.intB, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intB = static_cast<int>(v); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("DECAY"),
                                  makeBar(20.0, 95.0, 1.0, cfg.paramA * 100.0, "%", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v * 0.01); }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("SHIFT"),
                                  makeBar(-12.0, 12.0, 1.0, cfg.intC, "st", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intC = static_cast<int>(v); }); }),
                                  nullptr });
                break;
            }

            case ModuleType::CcMapper:
            {
                rows_.push_back({ makeLabel("SRC"),
                                  makeCombo(sigNames, cfg.inputSignal, [this](int v) { mutateModule([v](ModuleConfig& m) { m.inputSignal = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("DST"),
                                  makeCombo(ImageMidiControlEngine::getCcTargetNames(), cfg.mode, [this](int v) { mutateModule([v](ModuleConfig& m) { m.mode = v; }); }),
                                  nullptr });
                rows_.push_back({ makeLabel("CC/CH"),
                                  makeBar(0.0, 127.0, 1.0, cfg.intB, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intB = static_cast<int>(v); }); }),
                                  makeBar(1.0, 16.0, 1.0, cfg.intA, "ch", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intA = static_cast<int>(v); }); }) });
                rows_.push_back({ makeLabel("CRV"),
                                  makeCombo(ImageMidiControlEngine::getCurveNames(), cfg.intC, [this](int v) { mutateModule([v](ModuleConfig& m) { m.intC = v; }); }),
                                  makeToggle("Inv", cfg.flagA, [this](bool v) { mutateModule([v](ModuleConfig& m) { m.flagA = v; }); }) });
                rows_.push_back({ makeLabel("RNG"),
                                  makeBar(0.0, 1.0, 0.01, cfg.paramA, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramA = static_cast<float>(v); }); }),
                                  makeBar(0.0, 1.0, 0.01, cfg.paramB, "", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramB = static_cast<float>(v); }); }) });
                rows_.push_back({ makeLabel("SLW/M"),
                                  makeBar(0.0, 1000.0, 1.0, cfg.paramC * 1000.0, "ms", [this](double v) { mutateModule([v](ModuleConfig& m) { m.paramC = static_cast<float>(v * 0.001); }); }),
                                  makeBar(1.0, 8.0, 1.0, cfg.intD + 1, "M", [this](double v) { mutateModule([v](ModuleConfig& m) { m.intD = static_cast<int>(v) - 1; }); }) });
                break;
            }

            case ModuleType::Count:
                break;
        }
    }

    SoundCanvasControlAudioProcessor& processor_;
    int pathIndex_ = 0;
    int slotIndex_ = 0;
    ModuleType type_ = ModuleType::NoteGenerator;
    bool enabled_ = true;
    juce::Colour accent_;

    juce::ToggleButton powerBtn_;
    juce::TextButton leftBtn_, rightBtn_, delBtn_;

    std::vector<std::unique_ptr<juce::Component>> ownedWidgets_;
    std::vector<LayoutRow> rows_;
};

// ============================================================================
// Far-Right "+ Add Module" Ghost Slot
// ============================================================================
class ModuleRackComponent::AddCard : public juce::Component
{
public:
    explicit AddCard(std::function<void(juce::Component*)> onClick)
        : onClick_(std::move(onClick))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bf = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(hovered_ ? juce::Colour(0xff192031) : juce::Colour(0xff10131d));
        g.fillRoundedRectangle(bf, 9.0f);

        juce::Path borderPath;
        borderPath.addRoundedRectangle(bf, 9.0f);
        const float dashes[2] = { 5.0f, 4.0f };
        juce::PathStrokeType stroke(1.3f);
        juce::Path dashed;
        stroke.createDashedStroke(dashed, borderPath, dashes, 2);
        g.setColour(hovered_ ? juce::Colour(0xff38bdf8) : juce::Colour(ControlPalette::kBorderBright));
        g.fillPath(dashed);

        const float cx = bf.getCentreX();
        const float cy = bf.getCentreY() - 14.0f;
        g.setColour(hovered_ ? juce::Colours::white : juce::Colour(ControlPalette::kTextMuted));
        g.fillRoundedRectangle(cx - 10.0f, cy - 1.5f, 20.0f, 3.0f, 1.5f);
        g.fillRoundedRectangle(cx - 1.5f, cy - 10.0f, 3.0f, 20.0f, 1.5f);

        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("ADD\nMODULE", bf.withTop(cy + 14.0f), juce::Justification::centredTop, false);
    }

    void mouseEnter(const juce::MouseEvent&) override { hovered_ = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hovered_ = false; repaint(); }
    void mouseUp(const juce::MouseEvent&) override    { if (onClick_) onClick_(this); }

private:
    std::function<void(juce::Component*)> onClick_;
    bool hovered_ = false;
};

// ============================================================================
// Horizontal Chain Container (renders glowing Left -> Right signal arrows!)
// ============================================================================
class ModuleRackComponent::ChainContentComponent : public juce::Component
{
public:
    ChainContentComponent() = default;

    void setAccent(juce::Colour c) noexcept { accent_ = c; }

    void setCards(std::unique_ptr<PickupCard> pickup,
                  std::vector<std::unique_ptr<ModuleCard>> mods,
                  std::unique_ptr<AddCard> addCard)
    {
        removeAllChildren();
        pickup_ = std::move(pickup);
        modules_ = std::move(mods);
        addCard_ = std::move(addCard);

        if (pickup_) addAndMakeVisible(*pickup_);
        for (auto& m : modules_) addAndMakeVisible(*m);
        if (addCard_) addAndMakeVisible(*addCard_);

        layoutCards();
    }

    void layoutCards()
    {
        constexpr int kCardH = 248;
        constexpr int kPickupW = 216;
        constexpr int kModW = 204;
        constexpr int kAddW = 90;
        constexpr int kGap = 18;
        constexpr int kPad = 8;

        int x = kPad;
        if (pickup_)
        {
            pickup_->setBounds(x, kPad, kPickupW, kCardH);
            x += kPickupW + kGap;
        }
        for (auto& m : modules_)
        {
            m->setBounds(x, kPad, kModW, kCardH);
            x += kModW + kGap;
        }
        if (addCard_)
        {
            addCard_->setBounds(x, kPad, kAddW, kCardH);
            x += kAddW + kPad;
        }

        setSize(std::max(100, x), kCardH + kPad * 2);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0b0d14));

        // Draw crisp glowing Left -> Right signal flow arrows between adjacent cards
        std::vector<juce::Rectangle<int>> rects;
        if (pickup_) rects.push_back(pickup_->getBounds());
        for (const auto& m : modules_) rects.push_back(m->getBounds());
        if (addCard_) rects.push_back(addCard_->getBounds());

        for (size_t i = 0; i + 1 < rects.size(); ++i)
        {
            const float x0 = static_cast<float>(rects[i].getRight()) + 2.0f;
            const float x1 = static_cast<float>(rects[i + 1].getX()) - 2.0f;
            const float cy = static_cast<float>(rects[i].getCentreY());
            const bool toGhost = (addCard_ != nullptr && i + 2 == rects.size());

            const juce::Colour arrowCol = toGhost
                ? juce::Colour(ControlPalette::kBorderBright)
                : accent_.withAlpha(0.75f);

            g.setColour(arrowCol);
            g.drawLine(x0, cy, x1 - 4.0f, cy, 1.8f);

            juce::Path head;
            head.addTriangle(x1, cy, x1 - 6.0f, cy - 4.0f, x1 - 6.0f, cy + 4.0f);
            g.fillPath(head);
        }
    }

    void repaintTelemetry()
    {
        if (pickup_) pickup_->repaint();
        for (auto& m : modules_) m->repaint();
    }

private:
    juce::Colour accent_ { 0xff34d399 };
    std::unique_ptr<PickupCard> pickup_;
    std::vector<std::unique_ptr<ModuleCard>> modules_;
    std::unique_ptr<AddCard> addCard_;
};

// ============================================================================
// ModuleRackComponent Implementation
// ============================================================================
ModuleRackComponent::ModuleRackComponent(SoundCanvasControlAudioProcessor& processor)
    : processor_(processor),
      content_(std::make_unique<ChainContentComponent>())
{
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(content_.get(), false);
    viewport_.setScrollBarsShown(false, true, false, true);
    viewport_.setScrollBarThickness(8);

    rebuildIfNeeded(true);
    startTimerHz(30);
}

ModuleRackComponent::~ModuleRackComponent()
{
    stopTimer();
    viewport_.setViewedComponent(nullptr, false);
}

void ModuleRackComponent::setSelectedPath(int pathIndex)
{
    const int clamped = std::clamp(pathIndex, 0, RackDocument::kMaxPaths - 1);
    if (selectedPath_ != clamped)
    {
        selectedPath_ = clamped;
        rebuildIfNeeded(true);
    }
}

void ModuleRackComponent::showAddModuleMenu(juce::Component* anchor)
{
    const auto doc = processor_.getDocumentCopy();
    if (doc.paths[static_cast<size_t>(selectedPath_)].numModules >= PickupPathConfig::kMaxModules)
    {
        if (onStatusMessage)
            onStatusMessage("Maximum 8 modules per path reached.");
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader("SIGNAL & TRIGGER");
    menu.addItem(1, "Signal Shaper  (Curve / Range / Slew -> Mod)");
    menu.addItem(2, "Trigger Gate   (Clock / Rising Edge / Euclidean)");
    menu.addSeparator();
    menu.addSectionHeader("MIDI GENERATORS & MAPPING");
    menu.addItem(3, "Note Generator (Signal -> Quantized Scale Note)");
    menu.addItem(7, "CC / Macro Map (Signal -> MIDI CC / PB / Macro)");
    menu.addSeparator();
    menu.addSectionHeader("MIDI FX (STACK AFTER NOTE GEN)");
    menu.addItem(4, "Chord FX       (Scale-Locked Harmony Voicing)");
    menu.addItem(5, "Chance FX      (Probability & Velocity Gate)");
    menu.addItem(6, "MIDI Echo      (Tempo-Synced Pitch/Vel Repeats)");

    auto opts = juce::PopupMenu::Options();
    if (anchor != nullptr)
        opts = opts.withTargetComponent(anchor);

    menu.showMenuAsync(opts, [this](int result)
    {
        if (result >= 1 && result <= static_cast<int>(ModuleType::Count))
            addModuleOfType(static_cast<ModuleType>(result - 1));
    });
}

void ModuleRackComponent::addModuleOfType(ModuleType type)
{
    processor_.updateDocument([this, type](RackDocument& d)
    {
        auto& path = d.paths[static_cast<size_t>(selectedPath_)];
        if (path.numModules < PickupPathConfig::kMaxModules)
        {
            const uint32_t uid = d.nextModuleUid++;
            path.modules[static_cast<size_t>(path.numModules++)] =
                ImageMidiControlEngine::createDefaultModule(type, uid, selectedPath_);
        }
    });

    if (onStatusMessage)
        onStatusMessage("Added " + moduleShortTitle(type) + " to Path " + juce::String(selectedPath_ + 1) + ".");
    rebuildIfNeeded(true);
}

void ModuleRackComponent::moveModule(int fromIndex, int delta)
{
    processor_.updateDocument([this, fromIndex, delta](RackDocument& d)
    {
        auto& path = d.paths[static_cast<size_t>(selectedPath_)];
        const int toIndex = fromIndex + delta;
        if (fromIndex >= 0 && fromIndex < path.numModules && toIndex >= 0 && toIndex < path.numModules)
            std::swap(path.modules[static_cast<size_t>(fromIndex)], path.modules[static_cast<size_t>(toIndex)]);
    });
    rebuildIfNeeded(true);
}

void ModuleRackComponent::removeModule(int index)
{
    processor_.updateDocument([this, index](RackDocument& d)
    {
        auto& path = d.paths[static_cast<size_t>(selectedPath_)];
        if (index >= 0 && index < path.numModules)
        {
            for (int i = index; i < path.numModules - 1; ++i)
                path.modules[static_cast<size_t>(i)] = path.modules[static_cast<size_t>(i + 1)];
            path.numModules--;
        }
    });
    rebuildIfNeeded(true);
}

void ModuleRackComponent::toggleModuleEnabled(int index)
{
    processor_.updateDocument([this, index](RackDocument& d)
    {
        auto& path = d.paths[static_cast<size_t>(selectedPath_)];
        if (index >= 0 && index < path.numModules)
            path.modules[static_cast<size_t>(index)].enabled = !path.modules[static_cast<size_t>(index)].enabled;
    });
    rebuildIfNeeded(true);
}

void ModuleRackComponent::rebuildIfNeeded(bool force)
{
    const auto doc = processor_.getDocumentCopy();
    const auto& path = doc.paths[static_cast<size_t>(selectedPath_)];
    const uint64_t sig = computeStructuralHash(selectedPath_, path);
    const uint32_t hash32 = static_cast<uint32_t>(sig ^ (sig >> 32));

    if (!force && hash32 == lastBuiltRevision_ && lastBuiltPath_ == selectedPath_)
        return;

    lastBuiltRevision_ = hash32;
    lastBuiltPath_ = selectedPath_;

    auto pickup = std::make_unique<PickupCard>(processor_, selectedPath_);

    std::vector<std::unique_ptr<ModuleCard>> cards;
    const int n = std::clamp(path.numModules, 0, PickupPathConfig::kMaxModules);
    cards.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        cards.push_back(std::make_unique<ModuleCard>(
            processor_,
            selectedPath_,
            i,
            path.modules[static_cast<size_t>(i)],
            n,
            [this](int idx) { toggleModuleEnabled(idx); },
            [this](int idx, int delta) { moveModule(idx, delta); },
            [this](int idx) { removeModule(idx); }));
    }

    std::unique_ptr<AddCard> addCard;
    if (n < PickupPathConfig::kMaxModules)
        addCard = std::make_unique<AddCard>([this](juce::Component* c) { showAddModuleMenu(c); });

    content_->setAccent(ControlPalette::orbiterAccent(selectedPath_));
    content_->setCards(std::move(pickup), std::move(cards), std::move(addCard));
}

void ModuleRackComponent::timerCallback()
{
    rebuildIfNeeded(false);
    content_->repaintTelemetry();
}

void ModuleRackComponent::paint(juce::Graphics& g)
{
    const auto bf = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0b0d14));
    g.fillRoundedRectangle(bf, 10.0f);
    g.setColour(juce::Colour(ControlPalette::kBorderSubtle));
    g.drawRoundedRectangle(bf.reduced(0.5f), 10.0f, 1.0f);
}

void ModuleRackComponent::resized()
{
    viewport_.setBounds(getLocalBounds().reduced(2));
}

} // namespace soundcanvas
