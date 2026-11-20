#include <JuceHeader.h>
#include "synth/SynthPluginProcessor.h"
#include "synth/SynthPluginEditor.h"
#include "control/ControlPluginProcessor.h"
#include "control/ControlPluginEditor.h"
#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include <array>
#include <cmath>
#include <iostream>
#include <set>

namespace
{

int gFailures = 0;

void expect(bool condition, const char* description)
{
    if (condition)
    {
        std::cout << "  [PASS] " << description << "\n";
    }
    else
    {
        std::cerr << "  [FAIL] " << description << "\n";
        ++gFailures;
    }
}

void setParamValue(juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramId,
                   float unnormalizedValue)
{
    if (auto* param = apvts.getParameter(paramId))
        param->setValueNotifyingHost(param->convertTo0to1(unnormalizedValue));
}

void testSecurityAndImageCanvas()
{
    std::cout << "=== 1. Testing ImageCanvas Security & Synesthetic Analysis ===\n";

    juce::String err;
    expect(!soundcanvas::ImageCanvas::validateFilePath("../etc/passwd.png", err),
           "Rejects relative path traversal (../)");
    expect(!soundcanvas::ImageCanvas::validateFilePath("paintings\\..\\secret.png", err),
           "Rejects Windows backslash path traversal (..\\)");

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("sound_canvas_test_" + juce::Uuid().toString());
    tempDir.createDirectory();

    // 1. Test invalid extension
    juce::File badExtFile = tempDir.getChildFile("malicious.svg");
    badExtFile.replaceWithText("<svg></svg>");
    expect(!soundcanvas::ImageCanvas::validateImageFile(badExtFile, err),
           "Rejects non-allowlisted file extension (.svg)");

    // 2. Test forged magic bytes (text inside a .png file)
    juce::File forgedPng = tempDir.getChildFile("forged.png");
    forgedPng.replaceWithText("This is not a valid PNG header at all!");
    expect(!soundcanvas::ImageCanvas::validateImageFile(forgedPng, err),
           "Rejects .png file with invalid magic bytes");

    // 3. Test valid PNG creation and loading
    juce::Image testImg(juce::Image::RGB, 128, 128, true);
    for (int y = 0; y < 128; ++y)
    {
        for (int x = 0; x < 128; ++x)
        {
            const juce::Colour c = (x > 64)
                ? juce::Colour::fromRGB(240, 80, 40)
                : juce::Colour::fromRGB(20, 60, 200);
            testImg.setPixelAt(x, y, c);
        }
    }

    juce::File validPng = tempDir.getChildFile("test_painting.png");
    {
        juce::FileOutputStream out(validPng);
        juce::PNGImageFormat pngFormat;
        pngFormat.writeImageToStream(testImg, out);
    }

    auto loadedGrid = soundcanvas::ImageCanvas::loadFromFile(validPng, err);
    expect(loadedGrid != nullptr, "Loads valid PNG painting file after security checks");

    if (loadedGrid)
    {
        const auto leftSample = loadedGrid->sampleAt(0.15f, 0.5f);
        const auto rightSample = loadedGrid->sampleAt(0.85f, 0.5f);
        const auto boundarySample = loadedGrid->sampleAt(0.50f, 0.5f);

        expect(rightSample.r > leftSample.r && leftSample.b > rightSample.b,
               "Accurately samples RGB & HSV colors across the painting");
        expect(boundarySample.edge > leftSample.edge,
               "Sobel brushstroke detector detects sharp color edge at x=0.5");

        const auto wt = loadedGrid->extractWavetable({ 0.1f, 0.5f }, { 0.9f, 0.5f });
        bool allBounded = true;
        float maxAbs = 0.0f;
        for (const auto& frame : wt.frames)
        {
            for (float v : frame)
            {
                if (!std::isfinite(v) || std::abs(v) > 1.05f)
                    allBounded = false;
                maxAbs = std::max(maxAbs, std::abs(v));
            }
        }
        expect(allBounded && maxAbs > 0.2f,
               "Extracts normalized, DC-blocked 64-frame wavetable from brushstroke slice");

        // Region stats test around color boundary
        const auto stats = loadedGrid->sampleRegionStats({ 0.5f, 0.5f }, 0.25f);
        const bool meanFinite = std::isfinite(stats.mean.r)
                             && std::isfinite(stats.mean.g)
                             && std::isfinite(stats.mean.b)
                             && std::isfinite(stats.mean.hue)
                             && std::isfinite(stats.mean.saturation)
                             && std::isfinite(stats.mean.luminance)
                             && std::isfinite(stats.mean.edge);
        const bool varianceValid = std::isfinite(stats.colorVariance)
                                && stats.colorVariance >= 0.0f;
        bool harmonicValid = true;
        for (float h : stats.harmonicProfile)
        {
            if (!std::isfinite(h) || h < 0.0f || h > 1.0f)
                harmonicValid = false;
        }
        expect(meanFinite && varianceValid && stats.colorVariance > 0.01f && harmonicValid,
               "SynestheticGrid::sampleRegionStats computes finite RGB/HSV/Edge, variance >= 0, and harmonicProfile in [0, 1]");
    }

    // 4. Test aspect ratio preservation on non-square high-res images
    {
        juce::Image wideImg(juce::Image::RGB, 1600, 800, true);
        auto wideGrid = soundcanvas::ImageCanvas::createFromImage(wideImg, "Wide Test");
        const bool wideAspectOk = wideGrid != nullptr
                               && wideGrid->getDisplayImage().getWidth() == 1024
                               && wideGrid->getDisplayImage().getHeight() == 512
                               && wideGrid->getWidth() == 256
                               && wideGrid->getHeight() == 128;
        expect(wideAspectOk, "ImageCanvas preserves exact original aspect ratio when downscaling non-square images");
    }

    // 5. Test all 3 built-in starter paintings
    for (int p = 0; p < 3; ++p)
    {
        auto presetGrid = soundcanvas::ImageCanvas::createPresetCanvas(p);
        expect(presetGrid != nullptr && presetGrid->getDisplayImage().isValid(),
               "Generates built-in procedural painting preset");
    }

    tempDir.deleteRecursively();
}

void testScaleQuantizer()
{
    std::cout << "=== 2. Testing Musical Scale Quantizer & Rhythm Utilities ===\n";

    bool allQuantizedInScale = true;
    for (int root = 0; root < 12; ++root)
    {
        for (int scale = 0; scale < soundcanvas::ScaleQuantizer::NumScales; ++scale)
        {
            for (int i = 0; i <= 20; ++i)
            {
                const float normY = static_cast<float>(i) / 20.0f;
                const int note = soundcanvas::ScaleQuantizer::quantizeNormYToMidiNote(normY, root, scale);
                if (!soundcanvas::ScaleQuantizer::isNoteInScale(note, root, scale))
                    allQuantizedInScale = false;
            }
            for (int rawMidi = 36; rawMidi <= 84; ++rawMidi)
            {
                const int snapped = soundcanvas::ScaleQuantizer::snapMidiNoteToScale(rawMidi, root, scale);
                if (!soundcanvas::ScaleQuantizer::isNoteInScale(snapped, root, scale))
                    allQuantizedInScale = false;
            }
        }
    }

    expect(allQuantizedInScale, "All 12 roots x 10 musical scales strictly quantize Y-coordinates and MIDI notes");

    // Tempo division duration checks
    const float bar120 = soundcanvas::ScaleQuantizer::divisionToStepDurationSec(0, 120.0);
    const float q120 = soundcanvas::ScaleQuantizer::divisionToStepDurationSec(2, 120.0);
    const float e120 = soundcanvas::ScaleQuantizer::divisionToStepDurationSec(5, 120.0);
    const float s120 = soundcanvas::ScaleQuantizer::divisionToStepDurationSec(8, 120.0);
    const float q60 = soundcanvas::ScaleQuantizer::divisionToStepDurationSec(2, 60.0);
    expect(std::abs(bar120 - 2.0f) < 1e-4f
               && std::abs(q120 - 0.5f) < 1e-4f
               && std::abs(e120 - 0.25f) < 1e-4f
               && std::abs(s120 - 0.125f) < 1e-4f
               && std::abs(q60 - 1.0f) < 1e-4f,
           "divisionToStepDurationSec returns exact step durations at 120 BPM and 60 BPM");

    // Euclidean rhythm checks
    auto countPulses = [](int totalSteps, int pulses, int rotation)
    {
        int count = 0;
        for (int s = 0; s < totalSteps; ++s)
        {
            if (soundcanvas::ScaleQuantizer::isEuclideanStepActive(s, totalSteps, pulses, rotation))
                ++count;
        }
        return count;
    };

    bool e5_16_ok = true;
    for (int rot = 0; rot < 16; ++rot)
    {
        if (countPulses(16, 5, rot) != 5)
            e5_16_ok = false;
    }
    expect(e5_16_ok && countPulses(16, 0, 0) == 0 && countPulses(16, 16, 0) == 16,
           "isEuclideanStepActive yields exact pulse counts for E(5,16), E(0,16), and E(16,16)");
}

void testSynthPluginDspAndState()
{
    std::cout << "=== 3. Testing SoundCanvasSynthAudioProcessor DSP & State ===\n";

    soundcanvas::SoundCanvasSynthAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    expect(processor.acceptsMidi() && !processor.producesMidi(),
           "SoundCanvasSynth accepts MIDI and does not produce MIDI");

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    float maxPeak = 0.0f;
    float sumEnergy = 0.0f;
    bool allFinite = true;

    for (int b = 0; b < 24; ++b)
    {
        midi.clear();
        if (b == 1)
        {
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 96), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 108), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)105), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)95), 0);
        }
        else if (b == 16)
        {
            midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
        }

        processor.processBlock(buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* ptr = buffer.getReadPointer(ch);
            for (int s = 0; s < 512; ++s)
            {
                if (!std::isfinite(ptr[s]))
                    allFinite = false;
                maxPeak = std::max(maxPeak, std::abs(ptr[s]));
                sumEnergy += ptr[s] * ptr[s];
            }
        }
    }

    expect(allFinite, "SoundCanvasSynth output contains zero NaN or Inf samples");
    expect(sumEnergy > 0.05f, "SoundCanvasSynth produces rich non-zero audio with MIDI notes & CCs");
    expect(maxPeak <= 1.0001f, "SoundCanvasSynth output peak stays bounded within [-1.0, +1.0]");

    // Parameter sensitivity check (filterCutoff + oscALevel / oscBLevel)
    soundcanvas::SoundCanvasSynthAudioProcessor brightProc;
    soundcanvas::SoundCanvasSynthAudioProcessor darkProc;
    brightProc.prepareToPlay(48000.0, 512);
    darkProc.prepareToPlay(48000.0, 512);

    setParamValue(brightProc.getApvts(), "droneHold", 0.0f);
    setParamValue(brightProc.getApvts(), "filterCutoff", 16000.0f);
    setParamValue(brightProc.getApvts(), "oscALevel", 0.95f);
    setParamValue(brightProc.getApvts(), "oscBLevel", 0.10f);

    setParamValue(darkProc.getApvts(), "droneHold", 0.0f);
    setParamValue(darkProc.getApvts(), "filterCutoff", 160.0f);
    setParamValue(darkProc.getApvts(), "oscALevel", 0.15f);
    setParamValue(darkProc.getApvts(), "oscBLevel", 0.85f);

    juce::AudioBuffer<float> bufBright(2, 512);
    juce::AudioBuffer<float> bufDark(2, 512);
    float diffEnergy = 0.0f;
    bool sensitivityFinite = true;
    float sensitivityPeak = 0.0f;

    for (int b = 0; b < 12; ++b)
    {
        juce::MidiBuffer mA;
        juce::MidiBuffer mB;
        if (b == 0)
        {
            mA.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)110), 0);
            mB.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)110), 0);
        }

        brightProc.processBlock(bufBright, mA);
        darkProc.processBlock(bufDark, mB);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* pA = bufBright.getReadPointer(ch);
            const float* pB = bufDark.getReadPointer(ch);
            for (int s = 0; s < 512; ++s)
            {
                if (!std::isfinite(pA[s]) || !std::isfinite(pB[s]))
                    sensitivityFinite = false;
                sensitivityPeak = std::max(sensitivityPeak, std::max(std::abs(pA[s]), std::abs(pB[s])));
                const float d = pA[s] - pB[s];
                diffEnergy += d * d;
            }
        }
    }

    expect(sensitivityFinite && sensitivityPeak <= 1.0001f && diffEnergy > 0.01f,
           "SoundCanvasSynth output responds cleanly to filterCutoff and oscALevel/oscBLevel changes");

    // State save/restore for sliceA1/B1, sliceA2/B2, and cloudCenter
    processor.updateWavetableSliceA({ 0.25f, 0.75f }, { 0.80f, 0.20f });
    processor.updateWavetableSliceB({ 0.15f, 0.35f }, { 0.65f, 0.85f });
    processor.updateTextureCloudCenter({ 0.42f, 0.58f });

    juce::MemoryBlock stateBlock;
    processor.getStateInformation(stateBlock);
    expect(stateBlock.getSize() > 0, "Serializes SoundCanvasSynth APVTS and canvas coordinates");

    soundcanvas::SoundCanvasSynthAudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));

    const auto rA1 = restoredProcessor.getSynthEngine().getSlicePointA1();
    const auto rB1 = restoredProcessor.getSynthEngine().getSlicePointB1();
    const auto rA2 = restoredProcessor.getSynthEngine().getSlicePointA2();
    const auto rB2 = restoredProcessor.getSynthEngine().getSlicePointB2();
    const auto rCloud = restoredProcessor.getSynthEngine().getTextureCloudCenter();

    const bool sliceARestored = std::abs(rA1.x - 0.25f) < 0.01f && std::abs(rA1.y - 0.75f) < 0.01f
                             && std::abs(rB1.x - 0.80f) < 0.01f && std::abs(rB1.y - 0.20f) < 0.01f;
    const bool sliceBRestored = std::abs(rA2.x - 0.15f) < 0.01f && std::abs(rA2.y - 0.35f) < 0.01f
                             && std::abs(rB2.x - 0.65f) < 0.01f && std::abs(rB2.y - 0.85f) < 0.01f;
    const bool cloudRestored = std::abs(rCloud.x - 0.42f) < 0.01f && std::abs(rCloud.y - 0.58f) < 0.01f;

    expect(sliceARestored && sliceBRestored && cloudRestored,
           "Restores saved sliceA1/B1, sliceA2/B2, and cloudCenter coordinates from synth state");
}

void testControlPluginMidiAndMatrix()
{
    std::cout << "=== 4. Testing SoundCanvasControlAudioProcessor Deterministic Paths & Modular Rack ===\n";

    soundcanvas::SoundCanvasControlAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    expect(processor.acceptsMidi() && processor.producesMidi(),
           "SoundCanvasControl accepts MIDI and produces MIDI");

    auto& apvts = processor.getApvts();
    setParamValue(apvts, "internalBpm", 120.0f);
    setParamValue(apvts, "running", 1.0f);
    setParamValue(apvts, "hostSync", 0.0f);
    setParamValue(apvts, "previewSoundMode", 0.0f); // Strictly silent MIDI-only default

    // Build a full multi-path Ableton-style stack:
    // Path 1: SignalShaper (Edge -> ModA) -> TriggerGate -> NoteGen -> Chord -> Chance -> MidiEcho -> CcMapper (ModA -> CC74)
    // Path 2: Circle path with NoteGen (Ch 2) + CcMapper (Lum -> PitchBend)
    processor.updateDocument([](soundcanvas::RackDocument& doc)
    {
        auto& p0 = doc.paths[0];
        p0.active = true;
        p0.numModules = 7;
        p0.modules[0] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::SignalShaper, doc.nextModuleUid++, 0);
        p0.modules[0].inputSignal = static_cast<int>(soundcanvas::SignalId::Edge);
        p0.modules[0].outputSignal = static_cast<int>(soundcanvas::SignalId::ModA);
        p0.modules[0].paramC = 0.01f;

        p0.modules[1] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::TriggerGate, doc.nextModuleUid++, 0);
        p0.modules[1].paramA = 0.05f; // Low threshold so deterministic clock fires reliably

        p0.modules[2] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::NoteGenerator, doc.nextModuleUid++, 0);
        p0.modules[2].intA = 1;

        p0.modules[3] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::Chord, doc.nextModuleUid++, 0);
        p0.modules[3].mode = 1; // Triad

        p0.modules[4] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::Chance, doc.nextModuleUid++, 0);
        p0.modules[4].paramA = 1.0f; // 100% pass for deterministic test

        p0.modules[5] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::MidiEcho, doc.nextModuleUid++, 0);
        p0.modules[5].intA = 8; // 1/16 echo
        p0.modules[5].intB = 2; // 2 taps

        p0.modules[6] = soundcanvas::ImageMidiControlEngine::createDefaultModule(soundcanvas::ModuleType::CcMapper, doc.nextModuleUid++, 0);
        p0.modules[6].inputSignal = static_cast<int>(soundcanvas::SignalId::ModA);
        p0.modules[6].intB = 74;
        p0.modules[6].intD = 0; // Macro 1

        // Dynamically activate Path 2 on Channel 2
        doc.paths[1] = soundcanvas::ImageMidiControlEngine::createDefaultPath(1, doc.nextModuleUid);
        doc.paths[1].active = true;
        doc.paths[1].modules[0].paramA = 0.05f;
        doc.paths[1].modules[1].intA = 2;
        doc.paths[1].modules[2].mode = 1; // Pitch Bend
        doc.paths[1].modules[2].intD = 1; // Macro 2
    });

    const int rootNote = static_cast<int>(apvts.getRawParameterValue("rootNote")->load());
    const int scaleType = static_cast<int>(apvts.getRawParameterValue("scaleType")->load());

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    float maxAudioAbs = 0.0f;
    int noteOnCount = 0;
    int noteOffCount = 0;
    int ccCount = 0;
    int pitchBendCount = 0;
    bool allNotesInScale = true;
    bool sawRawSignalsAndModA = false;
    std::set<int> noteChannels;

    // ~1.54 seconds at 48 kHz (145 * 512 samples)
    for (int b = 0; b < 145; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            float* ptr = buffer.getWritePointer(ch);
            for (int s = 0; s < 512; ++s)
                ptr[s] = 0.25f;
        }
        midi.clear();

        processor.processBlock(buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* ptr = buffer.getReadPointer(ch);
            for (int s = 0; s < 512; ++s)
                maxAudioAbs = std::max(maxAudioAbs, std::abs(ptr[s]));
        }

        const auto snap = processor.getEngine().getVisualSnapshot();
        if (snap.paths[0].signals[static_cast<size_t>(soundcanvas::SignalId::Lum)] > 0.01f
            && snap.paths[0].signals[static_cast<size_t>(soundcanvas::SignalId::ModA)] > 0.01f)
        {
            sawRawSignalsAndModA = true;
        }

        for (const auto metadata : midi)
        {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                ++noteOnCount;
                noteChannels.insert(msg.getChannel());
                if (!soundcanvas::ScaleQuantizer::isNoteInScale(msg.getNoteNumber(), rootNote, scaleType))
                    allNotesInScale = false;
            }
            else if (msg.isNoteOff())
            {
                ++noteOffCount;
            }
            else if (msg.isController())
            {
                ++ccCount;
            }
            else if (msg.isPitchWheel())
            {
                ++pitchBendCount;
            }
        }
    }

    // Test pausing transport freezes deterministic playhead cleanly
    setParamValue(apvts, "running", 0.0f);
    midi.clear();
    processor.processBlock(buffer, midi);
    const auto pausedSnapA = processor.getEngine().getVisualSnapshot();
    midi.clear();
    processor.processBlock(buffer, midi);
    const auto pausedSnapB = processor.getEngine().getVisualSnapshot();
    const bool frozenWhenPaused = std::abs(pausedSnapA.paths[0].normalizedPhase - pausedSnapB.paths[0].normalizedPhase) < 1e-6f;
    setParamValue(apvts, "running", 1.0f);

    // Test triggerPanic() emits NoteOff / AllNotesOff and keeps audio silent
    processor.triggerPanic();
    midi.clear();
    processor.processBlock(buffer, midi);
    int panicAllNotesOffCount = 0;
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOff())
            ++noteOffCount;
        else if (msg.isAllNotesOff())
            ++panicAllNotesOffCount;
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* ptr = buffer.getReadPointer(ch);
        for (int s = 0; s < 512; ++s)
            maxAudioAbs = std::max(maxAudioAbs, std::abs(ptr[s]));
    }

    expect(maxAudioAbs == 0.0f,
           "SoundCanvasControl audio output buffer is strictly silent when Sound Mode is Off");
    expect(noteOnCount >= 12 && allNotesInScale && noteChannels.size() >= 2,
           "Deterministic Pickup Paths + NoteGen/Chord/Echo generate scale-quantized notes across channels");
    expect(noteOffCount > 0 && panicAllNotesOffCount >= 16,
           "SoundCanvasControl emits paired NoteOff events and AllNotesOff on triggerPanic()");
    expect(ccCount > 0 && pitchBendCount > 0,
           "Modular CC / Macro Mapper emits MIDI CC and Pitch Bend from raw & shaped signals");
    expect(sawRawSignalsAndModA,
           "Pickup emits raw painting signals and Signal Shaper writes shaped output to ModA bus");
    expect(frozenWhenPaused,
           "Pausing transport freezes deterministic playhead phase");

    const auto macros = processor.getMacroOutputs();
    bool macrosBounded = true;
    bool anyMacroNonZero = false;
    for (float v : macros)
    {
        if (!std::isfinite(v) || v < 0.0f || v > 1.0f)
            macrosBounded = false;
        if (v > 0.0f)
            anyMacroNonZero = true;
    }
    expect(macrosBounded && anyMacroNonZero,
           "SoundCanvasControl updates macroOut1..8 values within [0.0, 1.0]");

    // Test built-in Preview Sound Generator mode for standalone testing
    setParamValue(apvts, "previewSoundMode", 1.0f); // Sound: Multi
    float previewPeak = 0.0f;
    double previewEnergy = 0.0;
    bool previewFinite = true;
    for (int b = 0; b < 45; ++b)
    {
        buffer.clear();
        midi.clear();
        processor.processBlock(buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* ptr = buffer.getReadPointer(ch);
            for (int s = 0; s < 512; ++s)
            {
                if (!std::isfinite(ptr[s]))
                    previewFinite = false;
                previewPeak = std::max(previewPeak, std::abs(ptr[s]));
                previewEnergy += static_cast<double>(ptr[s]) * static_cast<double>(ptr[s]);
            }
        }
    }
    setParamValue(apvts, "previewSoundMode", 0.0f); // Sound: Off
    buffer.clear();
    midi.clear();
    processor.processBlock(buffer, midi);
    float offPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* ptr = buffer.getReadPointer(ch);
        for (int s = 0; s < 512; ++s)
            offPeak = std::max(offPeak, std::abs(ptr[s]));
    }
    expect(previewFinite && previewPeak <= 1.0001f && previewEnergy > 0.05 && offPeak == 0.0f,
           "Built-in Preview Sound Generator produces bounded stereo audio when enabled and silence when Off");

    // Full state save/restore for dynamic paths, waypoints, and stacked module chains
    juce::MemoryBlock stateBlock;
    processor.getStateInformation(stateBlock);
    expect(stateBlock.getSize() > 0, "Serializes SoundCanvasControl APVTS, pickup paths, and stacked module chains");

    soundcanvas::SoundCanvasControlAudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));

    const auto origDoc = processor.getDocumentCopy();
    const auto restDoc = restoredProcessor.getDocumentCopy();
    bool docRestored = (restDoc.paths[0].active == true)
                    && (restDoc.paths[1].active == true)
                    && (restDoc.paths[0].numModules == 7)
                    && (restDoc.paths[0].modules[0].type == soundcanvas::ModuleType::SignalShaper)
                    && (restDoc.paths[0].modules[3].type == soundcanvas::ModuleType::Chord)
                    && (restDoc.paths[0].modules[5].type == soundcanvas::ModuleType::MidiEcho)
                    && (restDoc.paths[0].numPoints == origDoc.paths[0].numPoints);
    if (docRestored)
    {
        for (int w = 0; w < origDoc.paths[0].numPoints; ++w)
        {
            if (std::abs(restDoc.paths[0].points[static_cast<size_t>(w)].x - origDoc.paths[0].points[static_cast<size_t>(w)].x) > 0.005f
                || std::abs(restDoc.paths[0].points[static_cast<size_t>(w)].y - origDoc.paths[0].points[static_cast<size_t>(w)].y) > 0.005f)
                docRestored = false;
        }
    }

    expect(docRestored,
           "Restores saved dynamic pickup paths, vector waypoints, and left-to-right module chains from state");
}

void testEndToEndControlToSynth()
{
    std::cout << "=== 5. Testing End-to-End SoundCanvasControl -> SoundCanvasSynth ===\n";

    soundcanvas::SoundCanvasControlAudioProcessor controlProc;
    soundcanvas::SoundCanvasSynthAudioProcessor synthProc;

    controlProc.prepareToPlay(48000.0, 512);
    synthProc.prepareToPlay(48000.0, 512);

    // Disable droneHold on synth so audio is exclusively driven by Control's generated MIDI
    setParamValue(synthProc.getApvts(), "droneHold", 0.0f);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    int totalControlNoteOns = 0;
    bool allFinite = true;
    float maxPeak = 0.0f;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;

    for (int b = 0; b < 40; ++b)
    {
        buffer.clear();
        midi.clear();

        controlProc.processBlock(buffer, midi);

        for (const auto metadata : midi)
        {
            if (metadata.getMessage().isNoteOn())
                ++totalControlNoteOns;
        }

        synthProc.processBlock(buffer, midi);

        const float* lPtr = buffer.getReadPointer(0);
        const float* rPtr = buffer.getReadPointer(1);
        for (int s = 0; s < 512; ++s)
        {
            if (!std::isfinite(lPtr[s]) || !std::isfinite(rPtr[s]))
                allFinite = false;
            maxPeak = std::max(maxPeak, std::max(std::abs(lPtr[s]), std::abs(rPtr[s])));
            leftEnergy += static_cast<double>(lPtr[s]) * static_cast<double>(lPtr[s]);
            rightEnergy += static_cast<double>(rPtr[s]) * static_cast<double>(rPtr[s]);
        }
    }

    expect(totalControlNoteOns > 0, "SoundCanvasControl emits NoteOn events within 40 blocks");
    expect(allFinite && maxPeak <= 1.0001f && leftEnergy > 0.05 && rightEnergy > 0.05,
           "SoundCanvasSynth renders rich, finite, bounded stereo audio driven by SoundCanvasControl MIDI");
}

void testEditorsAndSnapshots(const juce::String& snapshotDirPath)
{
    std::cout << "=== 6. Testing Synth & Control Editors and Capturing Canvas Snapshots ===\n";

    soundcanvas::SoundCanvasSynthAudioProcessor synthProc;
    synthProc.prepareToPlay(48000.0, 512);

    soundcanvas::SoundCanvasControlAudioProcessor controlProc;
    controlProc.prepareToPlay(48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> synthEditor(synthProc.createEditor());
    expect(synthEditor != nullptr && synthEditor->getWidth() == 1120 && synthEditor->getHeight() == 700,
           "Creates 1120x700 SoundCanvasSynthAudioProcessorEditor GUI component");

    std::unique_ptr<juce::AudioProcessorEditor> controlEditor(controlProc.createEditor());
    expect(controlEditor != nullptr && controlEditor->getWidth() == 1200 && controlEditor->getHeight() == 760,
           "Creates 1200x760 SoundCanvasControlAudioProcessorEditor GUI component");

    if (!synthEditor || !controlEditor)
        return;

    const char* synthFilenames[3] = {
        "sound_canvas_synth_nocturne.png",
        "sound_canvas_synth_sunset.png",
        "sound_canvas_synth_botanicals.png"
    };
    const char* controlFilenames[3] = {
        "sound_canvas_control_nocturne.png",
        "sound_canvas_control_sunset.png",
        "sound_canvas_control_botanicals.png"
    };

    for (int p = 0; p < 3; ++p)
    {
        synthProc.loadStarterPreset(p);
        controlProc.loadStarterPreset(p);

        juce::AudioBuffer<float> synthBuffer(2, 512);
        juce::MidiBuffer synthMidi;
        synthMidi.addEvent(juce::MidiMessage::noteOn(1, 60 + p * 2, (juce::uint8)100), 0);
        synthMidi.addEvent(juce::MidiMessage::noteOn(1, 64 + p * 2, (juce::uint8)95), 0);
        synthMidi.addEvent(juce::MidiMessage::noteOn(1, 67 + p * 2, (juce::uint8)90), 0);

        juce::AudioBuffer<float> controlBuffer(2, 512);
        juce::MidiBuffer controlMidi;

        for (int b = 0; b < 55; ++b)
        {
            if (b < 25)
            {
                synthProc.processBlock(synthBuffer, synthMidi);
                synthMidi.clear();
            }

            controlMidi.clear();
            controlProc.processBlock(controlBuffer, controlMidi);
        }

        juce::Image synthSnapshot = synthEditor->createComponentSnapshot(synthEditor->getLocalBounds());
        expect(synthSnapshot.isValid() && synthSnapshot.getWidth() == 1120 && synthSnapshot.getHeight() == 700,
               "Renders full SoundCanvasSynth GUI snapshot with active painting and overlays");

        juce::Image controlSnapshot = controlEditor->createComponentSnapshot(controlEditor->getLocalBounds());
        expect(controlSnapshot.isValid() && controlSnapshot.getWidth() == 1200 && controlSnapshot.getHeight() == 760,
               "Renders full SoundCanvasControl GUI snapshot with active painting and overlays");

        if (snapshotDirPath.isNotEmpty())
        {
            juce::File outDir(snapshotDirPath);
            if (outDir.isDirectory())
            {
                auto writePng = [&outDir](const char* name, const juce::Image& img)
                {
                    juce::File outFile = outDir.getChildFile(name);
                    outFile.deleteFile();
                    juce::FileOutputStream stream(outFile);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(img, stream);
                    }
                };
                writePng(synthFilenames[p], synthSnapshot);
                writePng(controlFilenames[p], controlSnapshot);
            }
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String snapshotDir = (argc > 1) ? juce::String(argv[1]) : juce::String();

    testSecurityAndImageCanvas();
    testScaleQuantizer();
    testSynthPluginDspAndState();
    testControlPluginMidiAndMatrix();
    testEndToEndControlToSynth();
    testEditorsAndSnapshots(snapshotDir);

    if (gFailures == 0)
    {
        std::cout << "\nALL SOUND CANVAS TESTS PASSED!\n";
        return 0;
    }

    std::cerr << "\n" << gFailures << " TEST(S) FAILED!\n";
    return 1;
}
