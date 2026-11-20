#pragma once

#include <JuceHeader.h>
#include <algorithm>

namespace soundcanvas
{

class SoundCanvasLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SoundCanvasLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0d0f14));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff191d28));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff31384c));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe8ecf4));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f2637));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2b4c7e));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe8ecf4));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd6deeb));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00e5ff));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff191d28));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff293652));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff2a3042));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff00e5ff));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
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
        g.strokePath(bgArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc
        juce::Path valArc;
        valArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(accent);
        g.strokePath(valArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Pointer indicator
        juce::Path pointer;
        const float ptrLen = radius * 0.66f;
        pointer.addRoundedRectangle(-1.4f, -radius + 2.0f, 2.8f, ptrLen, 1.4f);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colours::white);
        g.fillPath(pointer);
    }
};

} // namespace soundcanvas
