#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

SoundCanvasAudioProcessor::SoundCanvasAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SoundCanvasParameters", createParameterLayout())
{
    loadStarterPreset(0);
}

SoundCanvasAudioProcessor::~SoundCanvasAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SoundCanvasAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "scannerMix", 1 }, "Scanner Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "orbiterMix", 1 }, "Orbiters Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wavetableMix", 1 }, "Brush Synth Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.70f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "scanSpeed", 1 }, "Scan & Walk Speed",
        juce::NormalisableRange<float>(0.05f, 2.5f, 0.01f), 0.75f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "edgeSensitivity", 1 }, "Brush Edge Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.62f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "orbiterMode", 1 }, "Orbiter Motion",
        juce::StringArray { "Billiard Bounce", "Lissajous Orbit", "Brushstroke Drift" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wtMorph", 1 }, "Brush Slice Morph",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wtLfoDepth", 1 }, "Morph LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterCutoff", 1 }, "Color Filter Cutoff",
        juce::NormalisableRange<float>(120.0f, 18000.0f, 1.0f, 0.35f), 6800.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterReso", 1 }, "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 0.90f, 0.01f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverbMix", 1 }, "Gallery Reverb",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.48f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "rootNote", 1 }, "Root Note",
        ScaleQuantizer::getRootNoteNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "scaleType", 1 }, "Musical Scale",
        ScaleQuantizer::getScaleNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "droneHold", 1 }, "Autonomous Drone Hold", true));

    return { params.begin(), params.end() };
}

const juce::String SoundCanvasAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SoundCanvasAudioProcessor::acceptsMidi() const { return true; }
bool SoundCanvasAudioProcessor::producesMidi() const { return false; }
bool SoundCanvasAudioProcessor::isMidiEffect() const { return false; }
double SoundCanvasAudioProcessor::getTailLengthSeconds() const { return 2.5; }

int SoundCanvasAudioProcessor::getNumPrograms() { return 3; }
int SoundCanvasAudioProcessor::getCurrentProgram() { return currentPresetIndex_.load(); }

void SoundCanvasAudioProcessor::setCurrentProgram(int index)
{
    loadStarterPreset(index);
}

const juce::String SoundCanvasAudioProcessor::getProgramName(int index)
{
    switch (index)
    {
        case 0: return "Starry Nocturne";
        case 1: return "Sunset Impasto";
        case 2: return "Watercolor Botanicals";
        default: return "Painting Preset";
    }
}

void SoundCanvasAudioProcessor::changeProgramName(int, const juce::String&) {}

void SoundCanvasAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    scannerEngine_.prepare(sampleRate);
    orbiterEngine_.prepare(sampleRate);
    wavetableEngine_.prepare(sampleRate);
    masterEffects_.prepare(sampleRate);
    heldMidiNotes_.fill(false);
    keyboardState_.reset();
}

void SoundCanvasAudioProcessor::releaseResources()
{
    wavetableEngine_.allNotesOff();
}

bool SoundCanvasAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool SoundCanvasAudioProcessor::loadPaintingFromFile(const juce::File& file, juce::String& errorOut)
{
    auto grid = ImageCanvas::loadFromFile(file, errorOut);
    if (!grid)
        return false;

    {
        const juce::SpinLock::ScopedLockType lock(gridLock_);
        currentGrid_ = grid;
    }
    customImagePath_ = file.getFullPathName();
    currentPresetIndex_.store(-1);

    updateWavetableSlice(wavetableEngine_.getSlicePointA(), wavetableEngine_.getSlicePointB());
    return true;
}

void SoundCanvasAudioProcessor::loadStarterPreset(int presetIndex)
{
    const int clampedIdx = std::clamp(presetIndex, 0, 2);
    auto grid = ImageCanvas::createPresetCanvas(clampedIdx);
    if (grid)
    {
        {
            const juce::SpinLock::ScopedLockType lock(gridLock_);
            currentGrid_ = grid;
        }
        customImagePath_.clear();
        currentPresetIndex_.store(clampedIdx);
        updateWavetableSlice(wavetableEngine_.getSlicePointA(), wavetableEngine_.getSlicePointB());
    }
}

std::shared_ptr<const SynestheticGrid> SoundCanvasAudioProcessor::getCurrentGrid() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(gridLock_);
    return currentGrid_;
}

void SoundCanvasAudioProcessor::updateWavetableSlice(juce::Point<float> pointA, juce::Point<float> pointB)
{
    wavetableEngine_.setSlicePoints(pointA, pointB);
    auto grid = getCurrentGrid();
    if (grid)
    {
        auto wt = std::make_shared<const WavetableData>(grid->extractWavetable(pointA, pointB));
        wavetableEngine_.setWavetable(std::move(wt));
    }
}

void SoundCanvasAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (numSamples <= 0 || totalNumOutputChannels < 2)
        return;

    // Synchronize GUI on-screen keyboard events with incoming MIDI buffer
    keyboardState_.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    const int rootNote = static_cast<int>(apvts_.getRawParameterValue("rootNote")->load());
    const int scaleType = static_cast<int>(apvts_.getRawParameterValue("scaleType")->load());

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const int rawNote = msg.getNoteNumber();
            const int snappedNote = ScaleQuantizer::snapMidiNoteToScale(rawNote, rootNote, scaleType);
            heldMidiNotes_[static_cast<size_t>(std::clamp(rawNote, 0, 127))] = true;
            heldMidiNotes_[static_cast<size_t>(std::clamp(snappedNote, 0, 127))] = true;
            wavetableEngine_.noteOn(snappedNote, msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            const int rawNote = msg.getNoteNumber();
            const int snappedNote = ScaleQuantizer::snapMidiNoteToScale(rawNote, rootNote, scaleType);
            heldMidiNotes_[static_cast<size_t>(std::clamp(rawNote, 0, 127))] = false;
            heldMidiNotes_[static_cast<size_t>(std::clamp(snappedNote, 0, 127))] = false;
            wavetableEngine_.noteOff(snappedNote);
            wavetableEngine_.noteOff(rawNote);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldMidiNotes_.fill(false);
            wavetableEngine_.allNotesOff();
        }
    }

    bool anyMidiHeld = false;
    for (bool held : heldMidiNotes_)
    {
        if (held)
        {
            anyMidiHeld = true;
            break;
        }
    }

    const float scannerMix = apvts_.getRawParameterValue("scannerMix")->load();
    const float orbiterMix = apvts_.getRawParameterValue("orbiterMix")->load();
    const float wavetableMix = apvts_.getRawParameterValue("wavetableMix")->load();
    const float scanSpeed = apvts_.getRawParameterValue("scanSpeed")->load();
    const float edgeSensitivity = apvts_.getRawParameterValue("edgeSensitivity")->load();
    const int orbiterMode = static_cast<int>(apvts_.getRawParameterValue("orbiterMode")->load());
    const float wtMorph = apvts_.getRawParameterValue("wtMorph")->load();
    const float wtLfoDepth = apvts_.getRawParameterValue("wtLfoDepth")->load();
    const float baseCutoff = apvts_.getRawParameterValue("filterCutoff")->load();
    const float filterReso = apvts_.getRawParameterValue("filterReso")->load();
    const float reverbMix = apvts_.getRawParameterValue("reverbMix")->load();
    const bool droneHold = apvts_.getRawParameterValue("droneHold")->load() > 0.5f;

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    auto grid = getCurrentGrid();

    // 1. Render Spectral Scanner Engine
    scannerEngine_.renderBlock(grid.get(),
                               left,
                               right,
                               numSamples,
                               scanSpeed,
                               rootNote,
                               scaleType,
                               heldMidiNotes_,
                               anyMidiHeld,
                               scannerMix);

    // 2. Render Generative Orbiter Engine
    orbiterEngine_.renderBlock(grid.get(),
                               left,
                               right,
                               numSamples,
                               scanSpeed,
                               edgeSensitivity,
                               orbiterMode,
                               rootNote,
                               scaleType,
                               orbiterMix);

    // 3. Render Wavetable & Pad Sculptor Engine
    wavetableEngine_.renderBlock(left,
                                 right,
                                 numSamples,
                                 wtMorph,
                                 wtLfoDepth,
                                 scanSpeed * 0.35f,
                                 droneHold,
                                 rootNote,
                                 scaleType,
                                 wavetableMix);

    // Modulate Master Filter Cutoff & Chorus by active painting color
    const PixelSample activeColor = wavetableEngine_.getActiveFrameColor();
    const float colorCutoffMod = 0.65f + 0.70f * activeColor.luminance + 0.35f * activeColor.saturation;
    const float effectiveCutoff = std::clamp(baseCutoff * colorCutoffMod, 120.0f, 18000.0f);
    const float chorusDepth = std::clamp(0.25f + 0.65f * activeColor.saturation, 0.1f, 1.0f);

    masterEffects_.processBlock(left,
                                right,
                                numSamples,
                                effectiveCutoff,
                                filterReso,
                                chorusDepth,
                                reverbMix);

    // Capture oscilloscope and RMS for GUI visual feedback
    float sumSq = 0.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        const float mono = 0.5f * (left[s] + right[s]);
        sumSq += mono * mono;
        if ((s & 3) == 0)
        {
            scopeBuffer_[static_cast<size_t>(scopeWritePos_)] = mono;
            scopeWritePos_ = (scopeWritePos_ + 1) % kScopeSize;
        }
    }
    outputRms_.store(std::sqrt(sumSq / static_cast<float>(numSamples)));
}

bool SoundCanvasAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SoundCanvasAudioProcessor::createEditor()
{
    return new SoundCanvasAudioProcessorEditor(*this);
}

void SoundCanvasAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml)
    {
        xml->setAttribute("presetIndex", currentPresetIndex_.load());
        xml->setAttribute("customImagePath", customImagePath_);

        const auto sliceA = wavetableEngine_.getSlicePointA();
        const auto sliceB = wavetableEngine_.getSlicePointB();
        xml->setAttribute("sliceAX", static_cast<double>(sliceA.x));
        xml->setAttribute("sliceAY", static_cast<double>(sliceA.y));
        xml->setAttribute("sliceBX", static_cast<double>(sliceB.x));
        xml->setAttribute("sliceBY", static_cast<double>(sliceB.y));

        copyXmlToBinary(*xml, destData);
    }
}

void SoundCanvasAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts_.state.getType()))
    {
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));

        const juce::Point<float> sliceA(
            static_cast<float>(xmlState->getDoubleAttribute("sliceAX", 0.18)),
            static_cast<float>(xmlState->getDoubleAttribute("sliceAY", 0.65)));
        const juce::Point<float> sliceB(
            static_cast<float>(xmlState->getDoubleAttribute("sliceBX", 0.82)),
            static_cast<float>(xmlState->getDoubleAttribute("sliceBY", 0.35)));

        const juce::String savedPath = xmlState->getStringAttribute("customImagePath");
        juce::String errMsg;
        if (savedPath.isNotEmpty() && ImageCanvas::validateFilePath(savedPath, errMsg))
        {
            const juce::File imgFile(savedPath);
            if (!loadPaintingFromFile(imgFile, errMsg))
                loadStarterPreset(xmlState->getIntAttribute("presetIndex", 0));
        }
        else
        {
            loadStarterPreset(xmlState->getIntAttribute("presetIndex", 0));
        }

        updateWavetableSlice(sliceA, sliceB);
    }
}

} // namespace soundcanvas

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new soundcanvas::SoundCanvasAudioProcessor();
}
