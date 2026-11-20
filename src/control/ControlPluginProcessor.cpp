#include "control/ControlPluginProcessor.h"
#include "control/ControlPluginEditor.h"
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

SoundCanvasControlAudioProcessor::SoundCanvasControlAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SoundCanvasControlParameters", createParameterLayout())
{
    cacheParameterPointers();
    lastMacroSent_.fill(0.0f);
    for (auto& m : macroOutputs_)
        m.store(0.0f, std::memory_order_relaxed);

    document_ = ImageMidiControlEngine::createDefaultDocument();
    engine_.setDocument(document_);
    loadStarterPreset(0);

    // Automatically enable audition synth in Standalone app for instant testing
    if (wrapperType == wrapperType_Standalone)
    {
        if (auto* p = apvts_.getParameter("previewSoundMode"))
            p->setValueNotifyingHost(p->convertTo0to1(1.0f)); // Sound: Multi
    }
}

SoundCanvasControlAudioProcessor::~SoundCanvasControlAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SoundCanvasControlAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "rootNote", 1 }, "Root Note",
        ScaleQuantizer::getRootNoteNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "scaleType", 1 }, "Musical Scale",
        ScaleQuantizer::getScaleNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "internalBpm", 1 }, "Internal BPM",
        juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "running", 1 }, "Transport Running", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "hostSync", 1 }, "DAW Host Sync", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "previewSoundMode", 1 }, "Audition Sound Generator",
        PreviewSynthEngine::getModeNames(), 0));

    // 8 DAW Macro Outputs driven by CC / Macro Mapper modules
    for (int m = 1; m <= ImageMidiControlEngine::kNumMacroOutputs; ++m)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { "macroOut" + juce::String(m), 1 },
            "Macro Output " + juce::String(m),
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    }

    return { params.begin(), params.end() };
}

void SoundCanvasControlAudioProcessor::cacheParameterPointers()
{
    rootNotePtr_         = apvts_.getRawParameterValue("rootNote");
    scaleTypePtr_        = apvts_.getRawParameterValue("scaleType");
    internalBpmPtr_      = apvts_.getRawParameterValue("internalBpm");
    runningPtr_          = apvts_.getRawParameterValue("running");
    hostSyncPtr_         = apvts_.getRawParameterValue("hostSync");
    previewSoundModePtr_ = apvts_.getRawParameterValue("previewSoundMode");

    for (int m = 0; m < ImageMidiControlEngine::kNumMacroOutputs; ++m)
        macroParams_[static_cast<size_t>(m)] = apvts_.getParameter("macroOut" + juce::String(m + 1));
}

RackDocument SoundCanvasControlAudioProcessor::getDocumentCopy() const
{
    const juce::SpinLock::ScopedLockType lock(docEditLock_);
    return document_;
}

void SoundCanvasControlAudioProcessor::updateDocument(const std::function<void(RackDocument&)>& mutator)
{
    if (!mutator)
        return;

    RackDocument updated;
    {
        const juce::SpinLock::ScopedLockType lock(docEditLock_);
        mutator(document_);
        updated = document_;
    }

    engine_.setDocument(updated);
    documentRevision_.fetch_add(1, std::memory_order_release);
}

const juce::String SoundCanvasControlAudioProcessor::getName() const
{
#ifdef JucePlugin_Name
    return JucePlugin_Name;
#else
    return "Sound Canvas Control";
#endif
}

bool SoundCanvasControlAudioProcessor::acceptsMidi() const { return true; }
bool SoundCanvasControlAudioProcessor::producesMidi() const { return true; }
bool SoundCanvasControlAudioProcessor::isMidiEffect() const { return false; }
double SoundCanvasControlAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int SoundCanvasControlAudioProcessor::getNumPrograms() { return 3; }
int SoundCanvasControlAudioProcessor::getCurrentProgram() { return currentPresetIndex_.load(); }

void SoundCanvasControlAudioProcessor::setCurrentProgram(int index)
{
    loadStarterPreset(index);
}

const juce::String SoundCanvasControlAudioProcessor::getProgramName(int index)
{
    switch (index)
    {
        case 0: return "Starry Nocturne";
        case 1: return "Sunset Impasto";
        case 2: return "Watercolor Botanicals";
        default: return "Painting Preset";
    }
}

void SoundCanvasControlAudioProcessor::changeProgramName(int, const juce::String&) {}

void SoundCanvasControlAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    engine_.prepare(sampleRate);
    previewSynth_.prepare(sampleRate);
    panicRequested_.store(false, std::memory_order_relaxed);
}

void SoundCanvasControlAudioProcessor::releaseResources()
{
}

bool SoundCanvasControlAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool SoundCanvasControlAudioProcessor::loadPaintingFromFile(const juce::File& file, juce::String& errorOut)
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
    return true;
}

void SoundCanvasControlAudioProcessor::loadStarterPreset(int presetIndex)
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
    }
}

std::shared_ptr<const SynestheticGrid> SoundCanvasControlAudioProcessor::getCurrentGrid() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(gridLock_);
    return currentGrid_;
}

std::array<float, ImageMidiControlEngine::kNumMacroOutputs> SoundCanvasControlAudioProcessor::getMacroOutputs() const noexcept
{
    std::array<float, ImageMidiControlEngine::kNumMacroOutputs> out {};
    for (int i = 0; i < ImageMidiControlEngine::kNumMacroOutputs; ++i)
        out[static_cast<size_t>(i)] = macroOutputs_[static_cast<size_t>(i)].load(std::memory_order_relaxed);
    return out;
}

void SoundCanvasControlAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    if (panicRequested_.exchange(false, std::memory_order_acq_rel))
    {
        engine_.allNotesOff(midiMessages, true);
        previewSynth_.allNotesOff();
    }

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            engine_.allNotesOff(midiMessages, false);
            previewSynth_.allNotesOff();
        }
    }

    double bpm = static_cast<double>(internalBpmPtr_->load());
    bool running = runningPtr_->load() > 0.5f;
    const bool hostSync = hostSyncPtr_->load() > 0.5f;
    bool hasHostPpq = false;
    double hostPpq = 0.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            const bool hostPlaying = pos->getIsPlaying();
            if (hostSync)
            {
                running = running && hostPlaying;
                if (auto ppq = pos->getPpqPosition())
                {
                    hasHostPpq = true;
                    hostPpq = *ppq;
                }
            }

            if (hostPlaying)
            {
                if (auto hostBpm = pos->getBpm())
                {
                    if (*hostBpm >= 20.0 && *hostBpm <= 300.0)
                        bpm = *hostBpm;
                }
            }
        }
    }

    activeBpm_.store(bpm, std::memory_order_relaxed);

    ControlEngineParams params;
    params.bpm = bpm;
    params.rootNote = static_cast<int>(rootNotePtr_->load());
    params.scaleType = static_cast<int>(scaleTypePtr_->load());
    params.running = running;
    params.hasHostPpq = hasHostPpq;
    params.hostPpqPosition = hostPpq;

    auto grid = getCurrentGrid();
    engine_.processBlock(grid.get(), midiMessages, numSamples, params);

    const int soundMode = (previewSoundModePtr_ != nullptr)
        ? static_cast<int>(previewSoundModePtr_->load())
        : 0;
    previewSynth_.processBlock(buffer, midiMessages, soundMode);

    const auto macros = engine_.getMacroOutputs();
    for (int i = 0; i < ImageMidiControlEngine::kNumMacroOutputs; ++i)
    {
        const float val = macros[static_cast<size_t>(i)];
        macroOutputs_[static_cast<size_t>(i)].store(val, std::memory_order_relaxed);
        if (std::abs(val - lastMacroSent_[static_cast<size_t>(i)]) > 0.002f)
        {
            lastMacroSent_[static_cast<size_t>(i)] = val;
            if (macroParams_[static_cast<size_t>(i)] != nullptr)
                macroParams_[static_cast<size_t>(i)]->setValueNotifyingHost(val);
        }
    }
}

bool SoundCanvasControlAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SoundCanvasControlAudioProcessor::createEditor()
{
    return new SoundCanvasControlAudioProcessorEditor(*this);
}

void SoundCanvasControlAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (!xml)
        return;

    xml->setAttribute("presetIndex", currentPresetIndex_.load());
    xml->setAttribute("customImagePath", customImagePath_);

    const auto doc = getDocumentCopy();
    auto* rackXml = xml->createNewChildElement("ModularRack");
    rackXml->setAttribute("nextModuleUid", static_cast<int>(doc.nextModuleUid));

    for (int p = 0; p < RackDocument::kMaxPaths; ++p)
    {
        const auto& path = doc.paths[static_cast<size_t>(p)];
        auto* pXml = rackXml->createNewChildElement("Path");
        pXml->setAttribute("index", p);
        pXml->setAttribute("active", path.active);
        pXml->setAttribute("muted", path.muted);
        pXml->setAttribute("closedLoop", path.closedLoop);
        pXml->setAttribute("smoothCurve", path.smoothCurve);
        pXml->setAttribute("loopLengthIdx", path.loopLengthIdx);
        pXml->setAttribute("playMode", path.playMode);
        pXml->setAttribute("phaseOffset", static_cast<double>(path.phaseOffset));

        for (int wp = 0; wp < std::clamp(path.numPoints, 0, PickupPathConfig::kMaxWaypoints); ++wp)
        {
            auto* wXml = pXml->createNewChildElement("Waypoint");
            wXml->setAttribute("x", static_cast<double>(path.points[static_cast<size_t>(wp)].x));
            wXml->setAttribute("y", static_cast<double>(path.points[static_cast<size_t>(wp)].y));
        }

        for (int m = 0; m < std::clamp(path.numModules, 0, PickupPathConfig::kMaxModules); ++m)
        {
            const auto& mod = path.modules[static_cast<size_t>(m)];
            auto* mXml = pXml->createNewChildElement("Module");
            mXml->setAttribute("uid", static_cast<int>(mod.uid));
            mXml->setAttribute("type", static_cast<int>(mod.type));
            mXml->setAttribute("enabled", mod.enabled);
            mXml->setAttribute("inSig", mod.inputSignal);
            mXml->setAttribute("outSig", mod.outputSignal);
            mXml->setAttribute("secSig", mod.secondarySignal);
            mXml->setAttribute("mode", mod.mode);
            mXml->setAttribute("intA", mod.intA);
            mXml->setAttribute("intB", mod.intB);
            mXml->setAttribute("intC", mod.intC);
            mXml->setAttribute("intD", mod.intD);
            mXml->setAttribute("paramA", static_cast<double>(mod.paramA));
            mXml->setAttribute("paramB", static_cast<double>(mod.paramB));
            mXml->setAttribute("paramC", static_cast<double>(mod.paramC));
            mXml->setAttribute("flagA", mod.flagA);
        }
    }

    copyXmlToBinary(*xml, destData);
}

void SoundCanvasControlAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (!xmlState || !xmlState->hasTagName(apvts_.state.getType()))
        return;

    apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));

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

    if (auto* rackXml = xmlState->getChildByName("ModularRack"))
    {
        RackDocument restored = ImageMidiControlEngine::createDefaultDocument();
        restored.nextModuleUid = static_cast<uint32_t>(std::max(100, rackXml->getIntAttribute("nextModuleUid", 100)));

        for (auto* pXml : rackXml->getChildWithTagNameIterator("Path"))
        {
            const int idx = pXml->getIntAttribute("index", -1);
            if (idx < 0 || idx >= RackDocument::kMaxPaths)
                continue;

            auto& path = restored.paths[static_cast<size_t>(idx)];
            path.active        = pXml->getBoolAttribute("active", idx == 0);
            path.muted         = pXml->getBoolAttribute("muted", false);
            path.closedLoop    = pXml->getBoolAttribute("closedLoop", false);
            path.smoothCurve   = pXml->getBoolAttribute("smoothCurve", true);
            path.loopLengthIdx = std::clamp(pXml->getIntAttribute("loopLengthIdx", 3), 0, 5);
            path.playMode      = std::clamp(pXml->getIntAttribute("playMode", 0), 0, 2);
            path.phaseOffset   = std::clamp(static_cast<float>(pXml->getDoubleAttribute("phaseOffset", 0.0)), 0.0f, 1.0f);

            int wpCount = 0;
            for (auto* wXml : pXml->getChildWithTagNameIterator("Waypoint"))
            {
                if (wpCount >= PickupPathConfig::kMaxWaypoints)
                    break;
                path.points[static_cast<size_t>(wpCount++)] = {
                    std::clamp(static_cast<float>(wXml->getDoubleAttribute("x", 0.5)), 0.01f, 0.99f),
                    std::clamp(static_cast<float>(wXml->getDoubleAttribute("y", 0.5)), 0.01f, 0.99f)
                };
            }
            if (wpCount >= 2)
                path.numPoints = wpCount;

            int modCount = 0;
            for (auto* mXml : pXml->getChildWithTagNameIterator("Module"))
            {
                if (modCount >= PickupPathConfig::kMaxModules)
                    break;
                ModuleConfig mod;
                mod.uid             = static_cast<uint32_t>(std::max(1, mXml->getIntAttribute("uid", static_cast<int>(restored.nextModuleUid++))));
                mod.type            = static_cast<ModuleType>(std::clamp(mXml->getIntAttribute("type", 0), 0, static_cast<int>(ModuleType::Count) - 1));
                mod.enabled         = mXml->getBoolAttribute("enabled", true);
                mod.inputSignal     = std::clamp(mXml->getIntAttribute("inSig", 0), 0, static_cast<int>(SignalId::Count) - 1);
                mod.outputSignal    = std::clamp(mXml->getIntAttribute("outSig", 11), 0, static_cast<int>(SignalId::Count) - 1);
                mod.secondarySignal = std::clamp(mXml->getIntAttribute("secSig", 0), 0, static_cast<int>(SignalId::Count) - 1);
                mod.mode            = mXml->getIntAttribute("mode", 0);
                mod.intA            = mXml->getIntAttribute("intA", 1);
                mod.intB            = mXml->getIntAttribute("intB", 5);
                mod.intC            = mXml->getIntAttribute("intC", 3);
                mod.intD            = mXml->getIntAttribute("intD", 2);
                mod.paramA          = static_cast<float>(mXml->getDoubleAttribute("paramA", 0.5));
                mod.paramB          = static_cast<float>(mXml->getDoubleAttribute("paramB", 1.0));
                mod.paramC          = static_cast<float>(mXml->getDoubleAttribute("paramC", 0.03));
                mod.flagA           = mXml->getBoolAttribute("flagA", false);
                path.modules[static_cast<size_t>(modCount++)] = mod;
            }
            path.numModules = modCount;
        }

        // Guarantee at least one active path exists
        bool anyActive = false;
        for (const auto& p : restored.paths)
            if (p.active)
                anyActive = true;
        if (!anyActive)
            restored.paths[0].active = true;

        updateDocument([&](RackDocument& d) { d = restored; });
    }
}

} // namespace soundcanvas
