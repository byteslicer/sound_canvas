#pragma once

#include <JuceHeader.h>
#include "synth/SynthPluginProcessor.h"
#include <functional>

namespace soundcanvas
{

class SynthCanvasComponent : public juce::Component,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    explicit SynthCanvasComponent(SoundCanvasSynthAudioProcessor& processor);
    ~SynthCanvasComponent() override;

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
        SliceA1,
        SliceB1,
        SliceA2,
        SliceB2,
        CloudCenter,
        CloudRadius
    };

    void timerCallback() override;
    juce::Rectangle<float> getCanvasPlotBounds() const noexcept;
    juce::Point<float> screenToNorm(juce::Point<float> pt) const noexcept;
    juce::Point<float> normToScreen(juce::Point<float> normPt) const noexcept;

    void drawSliceRibbon(juce::Graphics& g,
                         juce::Point<float> normA,
                         juce::Point<float> normB,
                         const std::shared_ptr<const WavetableData>& wt,
                         float morphPos,
                         juce::Colour accent,
                         const char* labelA,
                         const char* labelB) const;

    void handlePointerInteraction(juce::Point<float> pos, bool isInitialClick);

    SoundCanvasSynthAudioProcessor& processor_;
    DragTarget activeDrag_ = DragTarget::None;
    bool isFileDragHover_ = false;
    bool radiusGestureActive_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthCanvasComponent)
};

} // namespace soundcanvas
