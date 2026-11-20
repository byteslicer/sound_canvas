#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>

namespace soundcanvas
{

class PaintingCanvasComponent : public juce::Component,
                                public juce::FileDragAndDropTarget,
                                private juce::Timer
{
public:
    explicit PaintingCanvasComponent(SoundCanvasAudioProcessor& processor);
    ~PaintingCanvasComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    std::function<void(const juce::String& statusText, bool isError)> onStatusMessage;

private:
    enum class DragTarget
    {
        None,
        Playhead,
        SlicePointA,
        SlicePointB,
        Orbiter0,
        Orbiter1,
        Orbiter2,
        Orbiter3
    };

    void timerCallback() override;
    juce::Rectangle<float> getCanvasPlotBounds() const noexcept;
    juce::Point<float> screenToNorm(juce::Point<float> pt) const noexcept;
    juce::Point<float> normToScreen(juce::Point<float> normPt) const noexcept;

    void handlePointerInteraction(juce::Point<float> pos, bool isInitialClick);

    SoundCanvasAudioProcessor& processor_;
    DragTarget activeDrag_ = DragTarget::None;
    bool isFileDragHover_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaintingCanvasComponent)
};

} // namespace soundcanvas
