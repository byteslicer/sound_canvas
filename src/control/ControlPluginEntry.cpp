#include "control/ControlPluginProcessor.h"

#if !defined(SOUNDCANVAS_NO_CREATE_PLUGIN_FILTER)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new soundcanvas::SoundCanvasControlAudioProcessor();
}
#endif
