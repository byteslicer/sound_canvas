#pragma once

#include <JuceHeader.h>
#include "control/ControlPluginProcessor.h"
#include "control/ControlLookAndFeel.h"
#include <functional>

namespace soundcanvas
{

class ControlCanvasComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               private juce::Timer
{
public:
    enum class LensMode
    {
        Painting = 0,
        Edges = 1
    };

    explicit ControlCanvasComponent(SoundCanvasControlAudioProcessor& processor);
    ~ControlCanvasComponent() override;

    void setSelectedPath(int pathIndex) noexcept;
    int getSelectedPath() const noexcept { return selectedPath_; }

    void setLensMode(LensMode mode);
    LensMode getLensMode() const noexcept { return lensMode_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    std::function<void(int pathIndex)> onPathSelected;
    std::function<void(const juce::String& statusText, bool isError)> onStatusMessage;

private:
    void timerCallback() override;
    void updateHudButtonStyles();
    void ensureEdgesImageCached(const SynestheticGrid& grid);

    juce::Rectangle<float> getCanvasFrameBounds() const noexcept;
    juce::Rectangle<float> getCanvasPlotBounds() const noexcept;
    juce::Point<float> screenToNorm(juce::Point<float> pt) const noexcept;
    juce::Point<float> normToScreen(juce::Point<float> normPt) const noexcept;

    struct WaypointHit
    {
        int pathIdx = -1;
        int wpIdx = -1;
    };

    WaypointHit hitTestWaypoint(juce::Point<float> screenPos, const RackDocument& doc) const noexcept;
    int findBestInsertIndex(const PickupPathConfig& path, juce::Point<float> normPt) const noexcept;

    SoundCanvasControlAudioProcessor& processor_;
    int selectedPath_ = 0;
    WaypointHit hoveredWp_ {};
    WaypointHit draggingWp_ {};

    LensMode lensMode_ = LensMode::Painting;
    bool showScaleGrid_ = true;
    bool isFileDragHover_ = false;

    juce::TextButton lensPaintingBtn_ { "Painting" };
    juce::TextButton lensEdgesBtn_    { "Edges" };
    juce::TextButton scaleGridBtn_    { "Scale Grid" };

    juce::Image cachedEdgesImage_;
    const SynestheticGrid* cachedGridPtr_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlCanvasComponent)
};

} // namespace soundcanvas
