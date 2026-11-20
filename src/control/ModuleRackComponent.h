#pragma once

#include <JuceHeader.h>
#include "control/ControlPluginProcessor.h"
#include "control/ControlLookAndFeel.h"
#include <functional>
#include <memory>
#include <vector>

namespace soundcanvas
{

class ModuleRackComponent : public juce::Component,
                            private juce::Timer
{
public:
    explicit ModuleRackComponent(SoundCanvasControlAudioProcessor& processor);
    ~ModuleRackComponent() override;

    void setSelectedPath(int pathIndex);
    int getSelectedPath() const noexcept { return selectedPath_; }

    void showAddModuleMenu(juce::Component* anchor = nullptr);
    void rebuildIfNeeded(bool force = false);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(const juce::String& statusText)> onStatusMessage;

private:
    class PickupCard;
    class ModuleCard;
    class AddCard;
    class ChainContentComponent;

    void timerCallback() override;
    void addModuleOfType(ModuleType type);
    void moveModule(int fromIndex, int delta);
    void removeModule(int index);
    void toggleModuleEnabled(int index);

    SoundCanvasControlAudioProcessor& processor_;
    int selectedPath_ = 0;
    uint32_t lastBuiltRevision_ = 0;
    int lastBuiltPath_ = -1;

    juce::Viewport viewport_;
    std::unique_ptr<ChainContentComponent> content_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleRackComponent)
};

} // namespace soundcanvas
