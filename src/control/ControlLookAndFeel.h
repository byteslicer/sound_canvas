#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

struct ControlPalette
{
    static constexpr uint32_t kBgDeep       = 0xff090b10;
    static constexpr uint32_t kBgHeader     = 0xff10131c;
    static constexpr uint32_t kSurface      = 0xff121620;
    static constexpr uint32_t kSurfaceCard  = 0xff171c28;
    static constexpr uint32_t kSurfaceInput = 0xff11151f;
    static constexpr uint32_t kBorderSubtle = 0xff242b3d;
    static constexpr uint32_t kBorderBright = 0xff364059;
    static constexpr uint32_t kTextPrimary  = 0xfff1f5f9;
    static constexpr uint32_t kTextMuted    = 0xff8b98b2;
    static constexpr uint32_t kTextDim      = 0xff5a667f;

    static juce::Colour orbiterAccent(int index) noexcept
    {
        switch (std::clamp(index, 0, 3))
        {
            case 0: return juce::Colour(0xff34d399); // Bioluminescent Emerald
            case 1: return juce::Colour(0xff38bdf8); // Electric Sky
            case 2: return juce::Colour(0xfffbbf24); // Solar Amber
            case 3: return juce::Colour(0xfff472b6); // Orchid Rose
            default: return juce::Colour(0xff34d399);
        }
    }
};

class ControlLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ControlLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(ControlPalette::kBgDeep));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(ControlPalette::kSurfaceInput));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(ControlPalette::kBorderSubtle));
        setColour(juce::ComboBox::textColourId, juce::Colour(ControlPalette::kTextPrimary));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(ControlPalette::kTextMuted));

        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff141824));
        setColour(juce::PopupMenu::textColourId, juce::Colour(ControlPalette::kTextPrimary));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff253149));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff191f2e));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff253752));
        setColour(juce::TextButton::textColourOffId, juce::Colour(ControlPalette::kTextPrimary));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        setColour(juce::ToggleButton::textColourId, juce::Colour(ControlPalette::kTextPrimary));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff34d399));

        setColour(juce::Slider::backgroundColourId, juce::Colour(ControlPalette::kSurfaceInput));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff34d399));
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(ControlPalette::kTextPrimary));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isHighlighted,
                              bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float radius = std::min(7.0f, bounds.getHeight() * 0.45f);

        juce::Colour base = backgroundColour;
        if (isButtonDown)
            base = base.brighter(0.16f);
        else if (isHighlighted)
            base = base.brighter(0.08f);

        g.setColour(base);
        g.fillRoundedRectangle(bounds, radius);

        const juce::Colour borderCol = button.getToggleState()
            ? base.brighter(0.45f).withAlpha(0.85f)
            : juce::Colour(ControlPalette::kBorderSubtle).brighter(isHighlighted ? 0.25f : 0.0f);
        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool /*isHighlighted*/,
                        bool /*isButtonDown*/) override
    {
        const float fontSize = std::clamp(static_cast<float>(button.getHeight()) * 0.44f, 10.0f, 12.2f);
        g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
        g.setColour(button.findColour(button.getToggleState()
                                          ? juce::TextButton::textColourOnId
                                          : juce::TextButton::textColourOffId)
                          .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));

        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(5, 1),
                         juce::Justification::centred, 1);
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool isHighlighted,
                          bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float radius = std::min(7.0f, bounds.getHeight() * 0.45f);
        const bool on = button.getToggleState();
        const juce::Colour accent = button.findColour(juce::ToggleButton::tickColourId);

        juce::Colour bg = on ? accent.withAlpha(0.18f) : juce::Colour(0xff151924);
        if (isDown)
            bg = bg.brighter(0.12f);
        else if (isHighlighted)
            bg = bg.brighter(0.06f);

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(on ? accent.withAlpha(0.8f) : juce::Colour(ControlPalette::kBorderSubtle));
        g.drawRoundedRectangle(bounds, radius, 1.0f);

        // Status LED dot on left
        const float dotR = 3.2f;
        const float dotX = bounds.getX() + 9.0f;
        const float dotY = bounds.getCentreY();
        if (on)
        {
            g.setColour(accent.withAlpha(0.32f));
            g.fillEllipse(dotX - dotR * 1.9f, dotY - dotR * 1.9f, dotR * 3.8f, dotR * 3.8f);
            g.setColour(accent);
            g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
        else
        {
            g.setColour(juce::Colour(ControlPalette::kTextDim));
            g.drawEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f, 1.2f);
        }

        const float fontSize = std::clamp(bounds.getHeight() * 0.44f, 10.0f, 11.8f);
        g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
        g.setColour(on ? juce::Colours::white : juce::Colour(ControlPalette::kTextMuted));
        g.drawFittedText(button.getButtonText(),
                         bounds.withTrimmedLeft(16.0f).withTrimmedRight(5.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
    }

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
        const float radius = 6.0f;

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId)
                       .brighter(isButtonDown ? 0.08f : 0.0f));
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, radius, 1.0f);

        // Minimalist chevron
        const float cx = static_cast<float>(width) - 12.0f;
        const float cy = static_cast<float>(height) * 0.5f;
        juce::Path chevron;
        chevron.startNewSubPath(cx - 3.5f, cy - 1.5f);
        chevron.lineTo(cx, cy + 2.2f);
        chevron.lineTo(cx + 3.5f, cy - 1.5f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(8, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont(juce::FontOptions(11.3f, juce::Font::plain));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const float radius = static_cast<float>(std::min(width, height)) * 0.41f;
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const juce::Colour accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

        // Outer track groove
        const float arcR = radius;
        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff232a3b));
        g.strokePath(bgArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Soft glowing halo + crisp active arc
        if (sliderPosProportional > 0.004f)
        {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(accent.withAlpha(0.24f));
            g.strokePath(valArc, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(accent);
            g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Recessed anodized metallic knob cap
        const float capR = radius * 0.76f;
        juce::ColourGradient capGrad(juce::Colour(0xff222838), cx, cy - capR,
                                     juce::Colour(0xff10131b), cx, cy + capR, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

        g.setColour(juce::Colour(0xff343d54).withAlpha(0.75f));
        g.drawEllipse(cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        // Crisp glowing indicator pip
        const float pipDist = capR * 0.62f;
        const float pipX = cx + std::sin(angle) * pipDist;
        const float pipY = cy - std::cos(angle) * pipDist;
        g.setColour(accent.withAlpha(0.45f));
        g.fillEllipse(pipX - 3.6f, pipY - 3.6f, 7.2f, 7.2f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(pipX - 2.0f, pipY - 2.0f, 4.0f, 4.0f);
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        const float radius = 6.0f;
        const juce::Colour accent = slider.findColour(juce::Slider::trackColourId);

        g.setColour(juce::Colour(ControlPalette::kSurfaceInput));
        g.fillRoundedRectangle(bounds, radius);

        const float prop = std::clamp((sliderPos - bounds.getX()) / std::max(1.0f, bounds.getWidth()), 0.0f, 1.0f);
        if (prop > 0.005f)
        {
            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle(bounds.reduced(1.0f), radius - 1.0f);
            g.reduceClipRegion(clip);

            const float fillW = bounds.getWidth() * prop;
            juce::ColourGradient grad(accent.withAlpha(0.28f), bounds.getX(), bounds.getY(),
                                      accent.withAlpha(0.52f), bounds.getX() + fillW, bounds.getY(), false);
            g.setGradientFill(grad);
            g.fillRect(bounds.getX(), bounds.getY(), fillW, bounds.getHeight());

            // Crisp right leading edge highlight
            g.setColour(accent.withAlpha(0.95f));
            g.fillRect(bounds.getX() + fillW - 2.0f, bounds.getY(), 2.0f, bounds.getHeight());
            g.restoreState();
        }

        g.setColour(juce::Colour(ControlPalette::kBorderSubtle));
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }
};

} // namespace soundcanvas
