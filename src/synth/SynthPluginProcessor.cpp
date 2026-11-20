#include "synth/SynthPluginProcessor.h"
#include "synth/SynthPluginEditor.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace soundcanvas
{

SoundCanvasSynthAudioProcessor::SoundCanvasSynthAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SoundCanvasSynthParameters", createParameterLayout())
{
    noteSnapMap_.fill(-1);
    for (size_t i = 0; i < kScopeSize; ++i)
    {
        scopeBuffer_[i].store(0.0f);
        scopeBufferRight_[i].store(0.0f);
    }
    loadStarterPreset(0);
}

SoundCanvasSynthAudioProcessor::~SoundCanvasSynthAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SoundCanvasSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Osc A (Slice A)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscALevel", 1 }, "Osc A Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.72f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscAMorph", 1 }, "Osc A Morph",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscALfoRate", 1 }, "Osc A LFO Rate",
        juce::NormalisableRange<float>(0.05f, 8.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscALfoDepth", 1 }, "Osc A LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.28f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "oscAOctave", 1 }, "Osc A Octave", -2, 2, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "oscASemitone", 1 }, "Osc A Semitone", -12, 12, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscAFine", 1 }, "Osc A Fine",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "oscAUnison", 1 }, "Osc A Unison", 1, 7, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscADetune", 1 }, "Osc A Detune",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.22f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscASpread", 1 }, "Osc A Spread",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscAWavefold", 1 }, "Osc A Wavefold",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));

    // Osc B (Slice B)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBLevel", 1 }, "Osc B Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBMorph", 1 }, "Osc B Morph",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBLfoRate", 1 }, "Osc B LFO Rate",
        juce::NormalisableRange<float>(0.05f, 8.0f, 0.01f), 0.22f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBLfoDepth", 1 }, "Osc B LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "oscBOctave", 1 }, "Osc B Octave", -2, 2, 0));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "oscBSemitone", 1 }, "Osc B Semitone", -12, 12, 7));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBFine", 1 }, "Osc B Fine",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "oscBUnison", 1 }, "Osc B Unison", 1, 7, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBDetune", 1 }, "Osc B Detune",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.18f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "oscBFmToA", 1 }, "Osc B FM -> A",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.12f));

    // Sub & Edge Transient
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "subLevel", 1 }, "Sub Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "subOctave", 1 }, "Sub Octave",
        juce::StringArray { "-1 Oct", "-2 Oct" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "edgeTransient", 1 }, "Edge Transient",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.30f));

    // 2D Texture Cloud
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "textureLevel", 1 }, "Texture Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "textureDensity", 1 }, "Texture Density",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "textureRadius", 1 }, "Texture Radius",
        juce::NormalisableRange<float>(0.04f, 0.45f, 0.005f), 0.18f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "textureShimmer", 1 }, "Texture Shimmer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "textureTilt", 1 }, "Texture Tilt",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.10f));

    // Multimode Filter
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "filterMode", 1 }, "Filter Mode",
        PolySynthEngine::getFilterModeNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterCutoff", 1 }, "Filter Cutoff",
        juce::NormalisableRange<float>(40.0f, 18000.0f, 1.0f, 0.3f), 5400.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterReso", 1 }, "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 0.94f, 0.01f), 0.28f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterDrive", 1 }, "Filter Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterKeyTrack", 1 }, "Filter KeyTrack",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filterEnvAmount", 1 }, "Filter Env Amount",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.42f));

    // Amp ADSR
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampAttack", 1 }, "Amp Attack",
        juce::NormalisableRange<float>(0.002f, 3.0f, 0.001f, 0.4f), 0.025f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampDecay", 1 }, "Amp Decay",
        juce::NormalisableRange<float>(0.01f, 4.0f, 0.001f, 0.4f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampSustain", 1 }, "Amp Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.78f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "ampRelease", 1 }, "Amp Release",
        juce::NormalisableRange<float>(0.01f, 5.0f, 0.001f, 0.4f), 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "velSensitivity", 1 }, "Velocity Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.70f));

    // Filter ADSR
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filAttack", 1 }, "Filter Attack",
        juce::NormalisableRange<float>(0.002f, 3.0f, 0.001f, 0.4f), 0.03f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filDecay", 1 }, "Filter Decay",
        juce::NormalisableRange<float>(0.01f, 4.0f, 0.001f, 0.4f), 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filSustain", 1 }, "Filter Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "filRelease", 1 }, "Filter Release",
        juce::NormalisableRange<float>(0.01f, 5.0f, 0.001f, 0.4f), 0.60f));

    // Performance
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "glideTime", 1 }, "Glide Time",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.02f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "rootNote", 1 }, "Root Note",
        ScaleQuantizer::getRootNoteNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "scaleType", 1 }, "Musical Scale",
        ScaleQuantizer::getScaleNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "quantizeMidi", 1 }, "Quantize MIDI", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "droneHold", 1 }, "Drone Hold", true));

    // 4-Slot Color Mod Matrix
    static constexpr int kDefaultModSources[4] = { 1, 3, 9, 12 };
    static constexpr int kDefaultModDests[4] = { 7, 2, 6, 0 };
    static constexpr float kDefaultModAmounts[4] = { 0.35f, 0.25f, 0.30f, 0.40f };

    const auto modSourceNames = PolySynthEngine::getModSourceNames();
    const auto modDestNames = PolySynthEngine::getModDestinationNames();

    for (int i = 1; i <= 4; ++i)
    {
        const juce::String idxStr(i);
        const size_t arrIdx = static_cast<size_t>(i - 1);

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "modSource" + idxStr, 1 },
            "Mod Source " + idxStr,
            modSourceNames,
            kDefaultModSources[arrIdx]));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { "modDest" + idxStr, 1 },
            "Mod Destination " + idxStr,
            modDestNames,
            kDefaultModDests[arrIdx]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "modAmount" + idxStr, 1 },
            "Mod Amount " + idxStr,
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
            kDefaultModAmounts[arrIdx]));
    }

    // Master FX
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "chorusRate", 1 }, "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 4.0f, 0.01f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "chorusDepth", 1 }, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "chorusMix", 1 }, "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "delayTime", 1 }, "Delay Time",
        juce::NormalisableRange<float>(0.05f, 1.2f, 0.005f), 0.36f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "delayFeedback", 1 }, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.85f, 0.01f), 0.38f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "delayMix", 1 }, "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverbSize", 1 }, "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverbDamping", 1 }, "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "reverbMix", 1 }, "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.38f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "masterDrive", 1 }, "Master Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.10f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "masterGain", 1 }, "Master Gain",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 0.85f));

    return { params.begin(), params.end() };
}

const juce::String SoundCanvasSynthAudioProcessor::getName() const
{
#ifdef JucePlugin_Name
    return JucePlugin_Name;
#else
    return "Sound Canvas Synth";
#endif
}

bool SoundCanvasSynthAudioProcessor::acceptsMidi() const { return true; }
bool SoundCanvasSynthAudioProcessor::producesMidi() const { return false; }
bool SoundCanvasSynthAudioProcessor::isMidiEffect() const { return false; }
double SoundCanvasSynthAudioProcessor::getTailLengthSeconds() const { return 2.5; }

int SoundCanvasSynthAudioProcessor::getNumPrograms() { return 3; }
int SoundCanvasSynthAudioProcessor::getCurrentProgram() { return currentPresetIndex_.load(); }

void SoundCanvasSynthAudioProcessor::setCurrentProgram(int index)
{
    loadStarterPreset(index);
}

const juce::String SoundCanvasSynthAudioProcessor::getProgramName(int index)
{
    switch (index)
    {
        case 0: return "Starry Nocturne";
        case 1: return "Sunset Impasto";
        case 2: return "Watercolor Botanicals";
        default: return "Painting Preset";
    }
}

void SoundCanvasSynthAudioProcessor::changeProgramName(int, const juce::String&) {}

void SoundCanvasSynthAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    synthEngine_.prepare(sampleRate);
    masterEffects_.prepare(sampleRate);
    noteSnapMap_.fill(-1);
    keyboardState_.reset();
}

void SoundCanvasSynthAudioProcessor::releaseResources()
{
    synthEngine_.allNotesOff();
}

bool SoundCanvasSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool SoundCanvasSynthAudioProcessor::loadPaintingFromFile(const juce::File& file, juce::String& errorOut)
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

    updateWavetableSliceA(synthEngine_.getSlicePointA1(), synthEngine_.getSlicePointB1());
    updateWavetableSliceB(synthEngine_.getSlicePointA2(), synthEngine_.getSlicePointB2());
    return true;
}

void SoundCanvasSynthAudioProcessor::loadStarterPreset(int presetIndex)
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
        updateWavetableSliceA(synthEngine_.getSlicePointA1(), synthEngine_.getSlicePointB1());
        updateWavetableSliceB(synthEngine_.getSlicePointA2(), synthEngine_.getSlicePointB2());
    }
}

std::shared_ptr<const SynestheticGrid> SoundCanvasSynthAudioProcessor::getCurrentGrid() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(gridLock_);
    return currentGrid_;
}

void SoundCanvasSynthAudioProcessor::updateWavetableSliceA(juce::Point<float> a, juce::Point<float> b)
{
    synthEngine_.setSlicePointsA(a, b);
    auto grid = getCurrentGrid();
    if (grid)
    {
        auto wt = std::make_shared<const WavetableData>(grid->extractWavetable(a, b));
        synthEngine_.setWavetableA(std::move(wt));
    }
}

void SoundCanvasSynthAudioProcessor::updateWavetableSliceB(juce::Point<float> a, juce::Point<float> b)
{
    synthEngine_.setSlicePointsB(a, b);
    auto grid = getCurrentGrid();
    if (grid)
    {
        auto wt = std::make_shared<const WavetableData>(grid->extractWavetable(a, b));
        synthEngine_.setWavetableB(std::move(wt));
    }
}

void SoundCanvasSynthAudioProcessor::updateTextureCloudCenter(juce::Point<float> center)
{
    synthEngine_.setTextureCloudCenter(center);
}

std::array<float, SoundCanvasSynthAudioProcessor::kScopeSize> SoundCanvasSynthAudioProcessor::getScopeSnapshot() const noexcept
{
    std::array<float, kScopeSize> out {};
    for (size_t i = 0; i < kScopeSize; ++i)
        out[i] = scopeBuffer_[i].load(std::memory_order_relaxed);
    return out;
}

std::array<float, SoundCanvasSynthAudioProcessor::kScopeSize> SoundCanvasSynthAudioProcessor::getScopeSnapshotRight() const noexcept
{
    std::array<float, kScopeSize> out {};
    for (size_t i = 0; i < kScopeSize; ++i)
        out[i] = scopeBufferRight_[i].load(std::memory_order_relaxed);
    return out;
}

void SoundCanvasSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (numSamples <= 0 || totalNumOutputChannels < 2)
        return;

    keyboardState_.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    const int rootNote = static_cast<int>(apvts_.getRawParameterValue("rootNote")->load());
    const int scaleType = static_cast<int>(apvts_.getRawParameterValue("scaleType")->load());
    const bool quantizeMidi = apvts_.getRawParameterValue("quantizeMidi")->load() > 0.5f;

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const int rawNote = std::clamp(msg.getNoteNumber(), 0, 127);
            const int targetNote = quantizeMidi
                ? ScaleQuantizer::snapMidiNoteToScale(rawNote, rootNote, scaleType)
                : rawNote;
            noteSnapMap_[static_cast<size_t>(rawNote)] = targetNote;
            synthEngine_.noteOn(targetNote, msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            const int rawNote = std::clamp(msg.getNoteNumber(), 0, 127);
            const int mappedNote = noteSnapMap_[static_cast<size_t>(rawNote)];
            noteSnapMap_[static_cast<size_t>(rawNote)] = -1;
            if (mappedNote >= 0)
                synthEngine_.noteOff(mappedNote);

            const int snappedNote = ScaleQuantizer::snapMidiNoteToScale(rawNote, rootNote, scaleType);
            if (snappedNote != mappedNote)
                synthEngine_.noteOff(snappedNote);
            if (rawNote != mappedNote && rawNote != snappedNote)
                synthEngine_.noteOff(rawNote);
        }
        else if (msg.isPitchWheel())
        {
            const float semi = (static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f * 2.0f;
            synthEngine_.setPitchBendSemitones(semi);
        }
        else if (msg.isChannelPressure())
        {
            synthEngine_.setAftertouch(static_cast<float>(msg.getChannelPressureValue()) / 127.0f);
        }
        else if (msg.isAftertouch())
        {
            synthEngine_.setAftertouch(static_cast<float>(msg.getAfterTouchValue()) / 127.0f);
        }
        else if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            const float normVal = static_cast<float>(msg.getControllerValue()) / 127.0f;
            if (cc == 1)
                synthEngine_.setModWheel(normVal);
            else if (cc == 11)
                synthEngine_.setExpression(normVal);
            else if (cc == 64)
                synthEngine_.setSustainPedal(msg.getControllerValue() >= 64);
            else if (cc == 74)
                cc74Offset_.store(std::clamp(normVal - 0.5f, -0.5f, 0.5f));
            else if (cc == 71)
                cc71Offset_.store(std::clamp(normVal - 0.5f, -0.5f, 0.5f));
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            noteSnapMap_.fill(-1);
            synthEngine_.allNotesOff();
        }
    }

    SynthParams synthParams;
    synthParams.oscALevel = apvts_.getRawParameterValue("oscALevel")->load();
    synthParams.oscAMorph = apvts_.getRawParameterValue("oscAMorph")->load();
    synthParams.oscALfoRate = apvts_.getRawParameterValue("oscALfoRate")->load();
    synthParams.oscALfoDepth = apvts_.getRawParameterValue("oscALfoDepth")->load();
    synthParams.oscAOctave = static_cast<int>(std::round(apvts_.getRawParameterValue("oscAOctave")->load()));
    synthParams.oscASemitone = static_cast<int>(std::round(apvts_.getRawParameterValue("oscASemitone")->load()));
    synthParams.oscAFineCents = apvts_.getRawParameterValue("oscAFine")->load();
    synthParams.oscAUnison = static_cast<int>(std::round(apvts_.getRawParameterValue("oscAUnison")->load()));
    synthParams.oscADetune = apvts_.getRawParameterValue("oscADetune")->load();
    synthParams.oscASpread = apvts_.getRawParameterValue("oscASpread")->load();
    synthParams.oscAWavefold = apvts_.getRawParameterValue("oscAWavefold")->load();

    synthParams.oscBLevel = apvts_.getRawParameterValue("oscBLevel")->load();
    synthParams.oscBMorph = apvts_.getRawParameterValue("oscBMorph")->load();
    synthParams.oscBLfoRate = apvts_.getRawParameterValue("oscBLfoRate")->load();
    synthParams.oscBLfoDepth = apvts_.getRawParameterValue("oscBLfoDepth")->load();
    synthParams.oscBOctave = static_cast<int>(std::round(apvts_.getRawParameterValue("oscBOctave")->load()));
    synthParams.oscBSemitone = static_cast<int>(std::round(apvts_.getRawParameterValue("oscBSemitone")->load()));
    synthParams.oscBFineCents = apvts_.getRawParameterValue("oscBFine")->load();
    synthParams.oscBUnison = static_cast<int>(std::round(apvts_.getRawParameterValue("oscBUnison")->load()));
    synthParams.oscBDetune = apvts_.getRawParameterValue("oscBDetune")->load();
    synthParams.oscBFmToA = apvts_.getRawParameterValue("oscBFmToA")->load();

    synthParams.subLevel = apvts_.getRawParameterValue("subLevel")->load();
    synthParams.subOctave = static_cast<int>(apvts_.getRawParameterValue("subOctave")->load());
    synthParams.edgeTransient = apvts_.getRawParameterValue("edgeTransient")->load();

    synthParams.textureLevel = apvts_.getRawParameterValue("textureLevel")->load();
    synthParams.textureDensity = apvts_.getRawParameterValue("textureDensity")->load();
    synthParams.textureRadius = apvts_.getRawParameterValue("textureRadius")->load();
    synthParams.textureShimmer = apvts_.getRawParameterValue("textureShimmer")->load();
    synthParams.textureTilt = apvts_.getRawParameterValue("textureTilt")->load();
    synthEngine_.setTextureCloudRadius(synthParams.textureRadius);

    synthParams.filterMode = static_cast<int>(apvts_.getRawParameterValue("filterMode")->load());
    const float baseCutoff = apvts_.getRawParameterValue("filterCutoff")->load();
    synthParams.filterCutoff = std::clamp(baseCutoff * std::pow(2.0f, cc74Offset_.load() * 3.0f), 40.0f, 18000.0f);
    const float baseReso = apvts_.getRawParameterValue("filterReso")->load();
    synthParams.filterReso = std::clamp(baseReso + cc71Offset_.load(), 0.0f, 0.94f);
    synthParams.filterDrive = apvts_.getRawParameterValue("filterDrive")->load();
    synthParams.filterKeyTrack = apvts_.getRawParameterValue("filterKeyTrack")->load();
    synthParams.filterEnvAmount = apvts_.getRawParameterValue("filterEnvAmount")->load();

    synthParams.ampAttack = apvts_.getRawParameterValue("ampAttack")->load();
    synthParams.ampDecay = apvts_.getRawParameterValue("ampDecay")->load();
    synthParams.ampSustain = apvts_.getRawParameterValue("ampSustain")->load();
    synthParams.ampRelease = apvts_.getRawParameterValue("ampRelease")->load();
    synthParams.velSensitivity = apvts_.getRawParameterValue("velSensitivity")->load();

    synthParams.filAttack = apvts_.getRawParameterValue("filAttack")->load();
    synthParams.filDecay = apvts_.getRawParameterValue("filDecay")->load();
    synthParams.filSustain = apvts_.getRawParameterValue("filSustain")->load();
    synthParams.filRelease = apvts_.getRawParameterValue("filRelease")->load();

    synthParams.glideTime = apvts_.getRawParameterValue("glideTime")->load();
    synthParams.rootNote = rootNote;
    synthParams.scaleType = scaleType;
    synthParams.droneHold = apvts_.getRawParameterValue("droneHold")->load() > 0.5f;

    for (int i = 0; i < 4; ++i)
    {
        const juce::String idxStr(i + 1);
        auto& slot = synthParams.modSlots[static_cast<size_t>(i)];
        slot.source = static_cast<int>(apvts_.getRawParameterValue("modSource" + idxStr)->load());
        slot.destination = static_cast<int>(apvts_.getRawParameterValue("modDest" + idxStr)->load());
        slot.amount = apvts_.getRawParameterValue("modAmount" + idxStr)->load();
    }

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    auto grid = getCurrentGrid();
    synthEngine_.renderBlock(grid.get(), left, right, numSamples, synthParams);

    SynthFxParams fxParams;
    fxParams.chorusRate = apvts_.getRawParameterValue("chorusRate")->load();
    fxParams.chorusDepth = apvts_.getRawParameterValue("chorusDepth")->load();
    fxParams.chorusMix = apvts_.getRawParameterValue("chorusMix")->load();
    fxParams.delayTimeSec = apvts_.getRawParameterValue("delayTime")->load();
    fxParams.delayFeedback = apvts_.getRawParameterValue("delayFeedback")->load();
    fxParams.delayMix = apvts_.getRawParameterValue("delayMix")->load();
    fxParams.reverbSize = apvts_.getRawParameterValue("reverbSize")->load();
    fxParams.reverbDamping = apvts_.getRawParameterValue("reverbDamping")->load();
    fxParams.reverbMix = apvts_.getRawParameterValue("reverbMix")->load();
    fxParams.masterDrive = apvts_.getRawParameterValue("masterDrive")->load();
    fxParams.masterGain = apvts_.getRawParameterValue("masterGain")->load();

    masterEffects_.processBlock(left, right, numSamples, fxParams);

    float sumSq = 0.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        const float l = left[s];
        const float r = right[s];
        const float mono = 0.5f * (l + r);
        sumSq += mono * mono;
        if ((s & 3) == 0)
        {
            const size_t idx = static_cast<size_t>(scopeWritePos_);
            scopeBuffer_[idx].store(l, std::memory_order_relaxed);
            scopeBufferRight_[idx].store(r, std::memory_order_relaxed);
            scopeWritePos_ = (scopeWritePos_ + 1) % kScopeSize;
        }
    }
    outputRms_.store(std::sqrt(sumSq / static_cast<float>(numSamples)));
}

bool SoundCanvasSynthAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SoundCanvasSynthAudioProcessor::createEditor()
{
    return new SoundCanvasSynthAudioProcessorEditor(*this);
}

void SoundCanvasSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml)
    {
        xml->setAttribute("presetIndex", currentPresetIndex_.load());
        xml->setAttribute("customImagePath", customImagePath_);

        const auto a1 = synthEngine_.getSlicePointA1();
        const auto b1 = synthEngine_.getSlicePointB1();
        const auto a2 = synthEngine_.getSlicePointA2();
        const auto b2 = synthEngine_.getSlicePointB2();
        const auto cloud = synthEngine_.getTextureCloudCenter();

        xml->setAttribute("sliceA1X", static_cast<double>(a1.x));
        xml->setAttribute("sliceA1Y", static_cast<double>(a1.y));
        xml->setAttribute("sliceB1X", static_cast<double>(b1.x));
        xml->setAttribute("sliceB1Y", static_cast<double>(b1.y));

        xml->setAttribute("sliceA2X", static_cast<double>(a2.x));
        xml->setAttribute("sliceA2Y", static_cast<double>(a2.y));
        xml->setAttribute("sliceB2X", static_cast<double>(b2.x));
        xml->setAttribute("sliceB2Y", static_cast<double>(b2.y));

        xml->setAttribute("cloudX", static_cast<double>(cloud.x));
        xml->setAttribute("cloudY", static_cast<double>(cloud.y));

        copyXmlToBinary(*xml, destData);
    }
}

void SoundCanvasSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts_.state.getType()))
    {
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));

        const juce::Point<float> a1(
            static_cast<float>(xmlState->getDoubleAttribute("sliceA1X", 0.16)),
            static_cast<float>(xmlState->getDoubleAttribute("sliceA1Y", 0.68)));
        const juce::Point<float> b1(
            static_cast<float>(xmlState->getDoubleAttribute("sliceB1X", 0.82)),
            static_cast<float>(xmlState->getDoubleAttribute("sliceB1Y", 0.32)));

        const juce::Point<float> a2(
            static_cast<float>(xmlState->getDoubleAttribute("sliceA2X", 0.20)),
            static_cast<float>(xmlState->getDoubleAttribute("sliceA2Y", 0.28)));
        const juce::Point<float> b2(
            static_cast<float>(xmlState->getDoubleAttribute("sliceB2X", 0.78)),
            static_cast<float>(xmlState->getDoubleAttribute("sliceB2Y", 0.74)));

        const juce::Point<float> cloud(
            static_cast<float>(xmlState->getDoubleAttribute("cloudX", 0.50)),
            static_cast<float>(xmlState->getDoubleAttribute("cloudY", 0.46)));

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

        updateWavetableSliceA(a1, b1);
        updateWavetableSliceB(a2, b2);
        updateTextureCloudCenter(cloud);
        synthEngine_.setTextureCloudRadius(apvts_.getRawParameterValue("textureRadius")->load());
    }
}

} // namespace soundcanvas
