#include "control/ImageMidiControlEngine.h"
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

ImageMidiControlEngine::ImageMidiControlEngine()
{
    stagedDoc_ = createDefaultDocument();
    audioDoc_ = stagedDoc_;
    reset();
}

juce::StringArray ImageMidiControlEngine::getSignalNames()
{
    return {
        "Luminance",
        "Hue",
        "Saturation",
        "Sobel Edge",
        "Red",
        "Green",
        "Blue",
        "Pos X",
        "Pos Y",
        "Color Delta",
        "Path Phase",
        "Mod Bus A",
        "Mod Bus B",
        "Fixed (100)"
    };
}

juce::StringArray ImageMidiControlEngine::getSignalShortNames()
{
    return {
        "LUM", "HUE", "SAT", "EDG", "R", "G", "B",
        "X", "Y", "dCL", "PHS", "MdA", "MdB", "FIX"
    };
}

juce::StringArray ImageMidiControlEngine::getModuleTypeNames()
{
    return {
        "Signal Shaper",
        "Trigger Gate",
        "Note Generator",
        "Chord",
        "Chance",
        "MIDI Echo",
        "CC / Macro Map"
    };
}

juce::StringArray ImageMidiControlEngine::getLoopLengthNames()
{
    return {
        "1/4 Bar",
        "1/2 Bar",
        "1 Bar",
        "2 Bars",
        "4 Bars",
        "8 Bars"
    };
}

double ImageMidiControlEngine::loopLengthIndexToBeats(int idx) noexcept
{
    static constexpr double kBeats[6] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    return kBeats[std::clamp(idx, 0, 5)];
}

juce::StringArray ImageMidiControlEngine::getPlayModeNames()
{
    return {
        "Forward",
        "Ping-Pong",
        "Reverse"
    };
}

juce::StringArray ImageMidiControlEngine::getCurveNames()
{
    return {
        "Linear",
        "Exp",
        "Log",
        "S-Curve",
        "Step 8"
    };
}

juce::StringArray ImageMidiControlEngine::getTriggerModeNames()
{
    return {
        "Clock + Thresh",
        "Rising Edge",
        "Euclidean"
    };
}

juce::StringArray ImageMidiControlEngine::getNoteTrigSourceNames()
{
    return {
        "Auto (Gate/Clk)",
        "Internal Clock",
        "Upstream Gate"
    };
}

juce::StringArray ImageMidiControlEngine::getChordTypeNames()
{
    return {
        "Dyad (+5th)",
        "Triad (1-3-5)",
        "Seventh (1-3-5-7)",
        "Sus2 (1-2-5)",
        "Octave (+12)",
        "Power (1-5-8)"
    };
}

juce::StringArray ImageMidiControlEngine::getCcTargetNames()
{
    return {
        "MIDI CC",
        "Pitch Bend",
        "Aftertouch",
        "Macro Only"
    };
}

juce::StringArray ImageMidiControlEngine::getShapePresetNames()
{
    return {
        "Line",
        "Circle",
        "Fig-8",
        "Wave",
        "ZigZag",
        "Spiral"
    };
}

ModuleConfig ImageMidiControlEngine::createDefaultModule(ModuleType type, uint32_t uid, int pathIndex) noexcept
{
    ModuleConfig m;
    m.uid = uid;
    m.type = type;
    m.enabled = true;
    const int ch = std::clamp(pathIndex + 1, 1, 16);

    switch (type)
    {
        case ModuleType::SignalShaper:
            m.inputSignal = static_cast<int>(SignalId::Edge);
            m.outputSignal = static_cast<int>(SignalId::ModA);
            m.mode = 0;       // Linear
            m.paramA = 0.0f;  // minOut
            m.paramB = 1.0f;  // maxOut
            m.paramC = 0.08f; // 80ms slew
            m.flagA = false;  // invert
            break;

        case ModuleType::TriggerGate:
            m.inputSignal = static_cast<int>(SignalId::Lum);
            m.mode = 0;       // Clock + Thresh
            m.intA = 5;       // 1/8
            m.intB = 16;      // Euclidean steps
            m.intC = 7;       // Euclidean pulses
            m.paramA = 0.24f; // threshold
            m.paramB = 0.05f; // hysteresis
            break;

        case ModuleType::NoteGenerator:
            m.inputSignal = static_cast<int>(SignalId::PosY);     // Pitch
            m.secondarySignal = static_cast<int>(SignalId::Lum);  // Velocity
            m.mode = 0;       // Auto (Gate/Clk)
            m.intA = ch;      // MIDI channel
            m.intB = 5;       // 1/8
            m.intC = (pathIndex == 2) ? 2 : (pathIndex == 1 ? 4 : 3); // Base octave
            m.intD = 2;       // Octave span
            m.paramA = 0.65f; // Gate %
            break;

        case ModuleType::Chord:
            m.mode = 1;       // Triad
            m.secondarySignal = static_cast<int>(SignalId::Edge);
            m.paramA = 0.18f; // 18ms strum
            m.paramB = 0.40f; // mod threshold
            m.flagA = false;
            break;

        case ModuleType::Chance:
            m.secondarySignal = static_cast<int>(SignalId::Sat);
            m.paramA = 0.80f; // probability
            m.paramB = 0.35f; // min vel
            m.paramC = 1.0f;  // max vel
            m.flagA = false;  // mod probability by signal
            break;

        case ModuleType::MidiEcho:
            m.intA = 6;       // 1/8D
            m.intB = 3;       // 3 repeats
            m.intC = 0;       // 0 semitone shift per repeat
            m.paramA = 0.62f; // velocity decay
            break;

        case ModuleType::CcMapper:
            m.inputSignal = static_cast<int>(SignalId::Lum);
            m.mode = 0;       // MIDI CC
            m.intA = ch;      // MIDI channel
            m.intB = (pathIndex == 0) ? 74 : ((pathIndex == 1) ? 71 : ((pathIndex == 2) ? 11 : 1));
            m.intC = 0;       // Linear curve
            m.intD = std::clamp(pathIndex * 2, 0, 7); // Macro slot
            m.paramA = 0.0f;  // Min
            m.paramB = 1.0f;  // Max
            m.paramC = 0.035f;// 35ms slew
            m.flagA = false;  // Invert
            break;

        case ModuleType::Count:
            break;
    }

    return m;
}

void ImageMidiControlEngine::applyShapePreset(PickupPathConfig& path, int shapeIndex) noexcept
{
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

    switch (shapeIndex)
    {
        case 1: // Circle (closed loop)
        {
            path.numPoints = 8;
            path.closedLoop = true;
            path.smoothCurve = true;
            for (int i = 0; i < 8; ++i)
            {
                const float a = static_cast<float>(i) / 8.0f * kTwoPi;
                path.points[static_cast<size_t>(i)] = {
                    0.50f + 0.28f * std::cos(a),
                    0.46f + 0.28f * std::sin(a)
                };
            }
            break;
        }
        case 2: // Figure-8 (closed Lissajous loop)
        {
            path.numPoints = 12;
            path.closedLoop = true;
            path.smoothCurve = true;
            for (int i = 0; i < 12; ++i)
            {
                const float a = static_cast<float>(i) / 12.0f * kTwoPi;
                path.points[static_cast<size_t>(i)] = {
                    0.50f + 0.34f * std::sin(a),
                    0.48f + 0.27f * std::sin(a * 2.0f)
                };
            }
            break;
        }
        case 3: // Wave (smooth ribbon)
        {
            path.numPoints = 7;
            path.closedLoop = false;
            path.smoothCurve = true;
            for (int i = 0; i < 7; ++i)
            {
                const float t = static_cast<float>(i) / 6.0f;
                path.points[static_cast<size_t>(i)] = {
                    0.10f + 0.80f * t,
                    0.48f - 0.24f * std::sin(t * kTwoPi * 1.25f)
                };
            }
            break;
        }
        case 4: // ZigZag (sharp straight corners)
        {
            path.numPoints = 6;
            path.closedLoop = false;
            path.smoothCurve = false;
            path.points[0] = { 0.12f, 0.76f };
            path.points[1] = { 0.28f, 0.22f };
            path.points[2] = { 0.44f, 0.72f };
            path.points[3] = { 0.60f, 0.24f };
            path.points[4] = { 0.76f, 0.70f };
            path.points[5] = { 0.90f, 0.26f };
            break;
        }
        case 5: // Spiral
        {
            path.numPoints = 12;
            path.closedLoop = false;
            path.smoothCurve = true;
            for (int i = 0; i < 12; ++i)
            {
                const float t = static_cast<float>(i) / 11.0f;
                const float a = t * kTwoPi * 1.75f;
                const float r = 0.06f + 0.32f * t;
                path.points[static_cast<size_t>(i)] = {
                    std::clamp(0.50f + r * std::cos(a), 0.06f, 0.94f),
                    std::clamp(0.48f + r * std::sin(a) * 0.85f, 0.06f, 0.94f)
                };
            }
            break;
        }
        default: // 0 = Line (sweeping diagonal)
        {
            path.numPoints = 4;
            path.closedLoop = false;
            path.smoothCurve = true;
            path.points[0] = { 0.12f, 0.68f };
            path.points[1] = { 0.37f, 0.34f };
            path.points[2] = { 0.64f, 0.56f };
            path.points[3] = { 0.88f, 0.24f };
            break;
        }
    }
}

PickupPathConfig ImageMidiControlEngine::createDefaultPath(int pathIndex, uint32_t& nextUid) noexcept
{
    PickupPathConfig p;
    p.active = true;
    p.muted = false;
    p.loopLengthIdx = (pathIndex == 0) ? 3 : ((pathIndex == 1) ? 2 : 4); // 2 Bars, 1 Bar, 4 Bars
    p.playMode = (pathIndex == 1) ? 0 : 1; // Ping-Pong or Forward
    p.phaseOffset = 0.0f;

    if (pathIndex == 0)
    {
        // Graceful 5-point path sweeping across Starry Nocturne's stars
        p.numPoints = 5;
        p.closedLoop = false;
        p.smoothCurve = true;
        p.points[0] = { 0.12f, 0.62f };
        p.points[1] = { 0.28f, 0.24f };
        p.points[2] = { 0.52f, 0.44f };
        p.points[3] = { 0.74f, 0.22f };
        p.points[4] = { 0.88f, 0.52f };
    }
    else if (pathIndex == 1)
    {
        applyShapePreset(p, 1); // Circle
    }
    else if (pathIndex == 2)
    {
        applyShapePreset(p, 3); // Wave
    }
    else
    {
        applyShapePreset(p, 2); // Fig-8
    }

    // Starter Ableton-style chain: Trigger Gate -> Note Generator -> CC Mapper
    p.numModules = 3;
    p.modules[0] = createDefaultModule(ModuleType::TriggerGate, nextUid++, pathIndex);
    p.modules[1] = createDefaultModule(ModuleType::NoteGenerator, nextUid++, pathIndex);
    p.modules[2] = createDefaultModule(ModuleType::CcMapper, nextUid++, pathIndex);

    return p;
}

RackDocument ImageMidiControlEngine::createDefaultDocument() noexcept
{
    RackDocument doc;
    doc.nextModuleUid = 100;
    // Start with 1 active Pickup Path; user can dynamically add up to 4!
    doc.paths[0] = createDefaultPath(0, doc.nextModuleUid);
    for (int i = 1; i < kMaxPaths; ++i)
    {
        doc.paths[static_cast<size_t>(i)] = createDefaultPath(i, doc.nextModuleUid);
        doc.paths[static_cast<size_t>(i)].active = false;
    }
    return doc;
}

juce::Point<float> ImageMidiControlEngine::evalCatmullRom(juce::Point<float> p0,
                                                          juce::Point<float> p1,
                                                          juce::Point<float> p2,
                                                          juce::Point<float> p3,
                                                          float t) noexcept
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float x = 0.5f * ((2.0f * p1.x)
                          + (-p0.x + p2.x) * t
                          + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2
                          + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    const float y = 0.5f * ((2.0f * p1.y)
                          + (-p0.y + p2.y) * t
                          + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2
                          + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    return { std::clamp(x, 0.005f, 0.995f), std::clamp(y, 0.005f, 0.995f) };
}

juce::Point<float> ImageMidiControlEngine::evaluatePathPoint(const PickupPathConfig& path, float normT) noexcept
{
    const int n = std::clamp(path.numPoints, 0, kMaxWaypoints);
    if (n <= 0)
        return { 0.5f, 0.5f };
    if (n == 1)
        return path.points[0];

    const bool closed = path.closedLoop && (n >= 3);
    const int numSegs = closed ? n : (n - 1);

    auto evalRaw = [&](float u) noexcept -> juce::Point<float>
    {
        const float cu = std::clamp(u, 0.0f, 1.0f);
        const float scaled = cu * static_cast<float>(numSegs);
        int seg = static_cast<int>(std::floor(scaled));
        float lt = scaled - static_cast<float>(seg);
        if (seg >= numSegs)
        {
            seg = numSegs - 1;
            lt = 1.0f;
        }

        auto getPt = [&](int idx) noexcept -> juce::Point<float>
        {
            if (closed)
                return path.points[static_cast<size_t>(((idx % n) + n) % n)];
            return path.points[static_cast<size_t>(std::clamp(idx, 0, n - 1))];
        };

        const auto p1 = getPt(seg);
        const auto p2 = getPt(seg + 1);
        if (!path.smoothCurve || n < 3)
            return { p1.x + (p2.x - p1.x) * lt, p1.y + (p2.y - p1.y) * lt };

        return evalCatmullRom(getPt(seg - 1), p1, p2, getPt(seg + 2), lt);
    };

    // Arc-length reparameterization over 64 samples for strictly constant playhead velocity
    constexpr int kLutSteps = 64;
    std::array<float, kLutSteps + 1> cumLen {};
    juce::Point<float> prev = evalRaw(0.0f);
    cumLen[0] = 0.0f;
    for (int i = 1; i <= kLutSteps; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(kLutSteps);
        const auto cur = evalRaw(u);
        cumLen[static_cast<size_t>(i)] = cumLen[static_cast<size_t>(i - 1)]
                                       + std::hypot(cur.x - prev.x, cur.y - prev.y);
        prev = cur;
    }

    const float totalLen = cumLen[kLutSteps];
    if (totalLen <= 1.0e-5f)
        return path.points[0];

    const float targetDist = std::clamp(normT, 0.0f, 1.0f) * totalLen;
    int hi = 1;
    while (hi < kLutSteps && cumLen[static_cast<size_t>(hi)] < targetDist)
        ++hi;

    const float d0 = cumLen[static_cast<size_t>(hi - 1)];
    const float d1 = cumLen[static_cast<size_t>(hi)];
    const float frac = (d1 > d0 + 1.0e-6f) ? ((targetDist - d0) / (d1 - d0)) : 0.0f;
    const float u = (static_cast<float>(hi - 1) + frac) / static_cast<float>(kLutSteps);
    return evalRaw(u);
}

void ImageMidiControlEngine::prepare(double sampleRate) noexcept
{
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void ImageMidiControlEngine::reset() noexcept
{
    rngState_ = 0x5A17C0DEu;
    wasRunning_ = true;

    for (int i = 0; i < kMaxPaths; ++i)
    {
        const size_t u = static_cast<size_t>(i);
        pathRuntime_[u] = {};
        pathVisuals_[u] = {};
        pathVisuals_[u].active = audioDoc_.paths[u].active;
        pathVisuals_[u].muted = audioDoc_.paths[u].muted;
        const auto pt = evaluatePathPoint(audioDoc_.paths[u], 0.0f);
        pathVisuals_[u].playheadX = pt.x;
        pathVisuals_[u].playheadY = pt.y;
        pathVisuals_[u].lastChannel = i + 1;
    }

    macroOutputs_.fill(0.0f);
    for (auto& n : activeNotes_)
        n = {};
    for (auto& s : scheduledNotes_)
        s = {};
    for (auto& e : midiLog_)
        e = {};
    midiLogWriteIdx_ = 0;

    publishSnapshot();
}

void ImageMidiControlEngine::setDocument(const RackDocument& doc) noexcept
{
    {
        const juce::SpinLock::ScopedLockType lock(docLock_);
        stagedDoc_ = doc;
        docDirty_.store(true, std::memory_order_release);
    }

    // Immediately mirror structural active/position state into snapshot so UI feels zero-latency
    const juce::SpinLock::ScopedLockType lock(snapshotLock_);
    for (int i = 0; i < kMaxPaths; ++i)
    {
        const size_t u = static_cast<size_t>(i);
        publishedSnapshot_.paths[u].active = doc.paths[u].active;
        publishedSnapshot_.paths[u].muted = doc.paths[u].muted;
    }
}

void ImageMidiControlEngine::pullPendingDocument() noexcept
{
    if (!docDirty_.load(std::memory_order_acquire))
        return;

    const juce::SpinLock::ScopedTryLockType tryLock(docLock_);
    if (!tryLock.isLocked())
        return;

    audioDoc_ = stagedDoc_;
    docDirty_.store(false, std::memory_order_relaxed);
}

float ImageMidiControlEngine::nextRandomFloat() noexcept
{
    rngState_ ^= (rngState_ << 13);
    rngState_ ^= (rngState_ >> 17);
    rngState_ ^= (rngState_ << 5);
    return static_cast<float>(rngState_ & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float ImageMidiControlEngine::applyCurve(float x, int curveType) noexcept
{
    const float v = std::clamp(x, 0.0f, 1.0f);
    switch (curveType)
    {
        case 1: return std::pow(v, 2.2f);                       // Exp
        case 2: return std::sqrt(v);                            // Log
        case 3: return v * v * (3.0f - 2.0f * v);               // S-Curve
        case 4: return std::floor(v * 7.999f) / 7.0f;           // Step 8
        default: return v;                                      // Linear
    }
}

ImageMidiControlEngine::ModuleRuntime& ImageMidiControlEngine::findOrResetModuleRuntime(
    PathRuntime& pr, int slotIdx, uint32_t uid) noexcept
{
    const size_t su = static_cast<size_t>(std::clamp(slotIdx, 0, kMaxModules - 1));
    if (pr.modRuntime[su].uid == uid)
        return pr.modRuntime[su];

    for (int j = 0; j < kMaxModules; ++j)
    {
        if (pr.modRuntime[static_cast<size_t>(j)].uid == uid)
        {
            std::swap(pr.modRuntime[su], pr.modRuntime[static_cast<size_t>(j)]);
            return pr.modRuntime[su];
        }
    }

    pr.modRuntime[su] = {};
    pr.modRuntime[su].uid = uid;
    return pr.modRuntime[su];
}

void ImageMidiControlEngine::pushMidiLog(MidiLogEntry::Kind kind, int pathIndex, int channel, int data1, int data2) noexcept
{
    midiLog_[static_cast<size_t>(midiLogWriteIdx_)] = {
        kind,
        std::clamp(pathIndex, 0, kMaxPaths - 1),
        std::clamp(channel, 1, 16),
        data1,
        data2
    };
    midiLogWriteIdx_ = (midiLogWriteIdx_ + 1) % kMidiLogCapacity;
}

void ImageMidiControlEngine::expireActiveNotes(juce::MidiBuffer& midiOut, int startSample, int chunkSamples) noexcept
{
    for (auto& n : activeNotes_)
    {
        if (!n.active)
            continue;

        if (n.remainingSamples < chunkSamples)
        {
            const int offSample = startSample + std::max(0, n.remainingSamples);
            midiOut.addEvent(juce::MidiMessage::noteOff(n.channel, n.note), offSample);
            n.active = false;
            n.remainingSamples = 0;
        }
        else
        {
            n.remainingSamples -= chunkSamples;
        }
    }
}

void ImageMidiControlEngine::processScheduledNotes(juce::MidiBuffer& midiOut, int startSample, int chunkSamples) noexcept
{
    for (auto& s : scheduledNotes_)
    {
        if (!s.active)
            continue;

        if (s.delayRemainingSamples < chunkSamples)
        {
            const int fireOffset = startSample + std::max(0, s.delayRemainingSamples);
            const int remChunk = std::max(1, chunkSamples - std::max(0, s.delayRemainingSamples));
            triggerMidiNote(midiOut, fireOffset, remChunk, s.pathIndex, s.channel, s.note,
                            s.velocity, s.gateSamples, s.isFxNote);
            s.active = false;
            s.delayRemainingSamples = 0;
        }
        else
        {
            s.delayRemainingSamples -= chunkSamples;
        }
    }
}

void ImageMidiControlEngine::triggerMidiNote(juce::MidiBuffer& midiOut,
                                             int sampleOffset,
                                             int chunkSamples,
                                             int pathIndex,
                                             int channel,
                                             int midiNote,
                                             float velocityNorm,
                                             int gateSamples,
                                             bool isFxNote) noexcept
{
    const int ch = std::clamp(channel, 1, 16);
    const int note = std::clamp(midiNote, 0, 127);
    const uint8_t vel = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(velocityNorm * 127.0f)), 1, 127));
    const int safeGate = std::max(1, gateSamples);

    for (auto& existing : activeNotes_)
    {
        if (existing.active && existing.channel == ch && existing.note == note)
        {
            midiOut.addEvent(juce::MidiMessage::noteOff(ch, note), sampleOffset);
            existing.active = false;
            existing.remainingSamples = 0;
        }
    }

    midiOut.addEvent(juce::MidiMessage::noteOn(ch, note, vel), sampleOffset);
    pushMidiLog(isFxNote ? MidiLogEntry::Kind::ChordOrEchoNote : MidiLogEntry::Kind::Note,
                pathIndex, ch, note, static_cast<int>(vel));

    if (safeGate < chunkSamples)
    {
        midiOut.addEvent(juce::MidiMessage::noteOff(ch, note), sampleOffset + safeGate);
        return;
    }

    int freeSlot = -1;
    int shortestIdx = 0;
    int shortestRem = activeNotes_[0].remainingSamples;

    for (int i = 0; i < kMaxActiveNotes; ++i)
    {
        const auto& n = activeNotes_[static_cast<size_t>(i)];
        if (!n.active)
        {
            freeSlot = i;
            break;
        }
        if (n.remainingSamples < shortestRem)
        {
            shortestRem = n.remainingSamples;
            shortestIdx = i;
        }
    }

    if (freeSlot < 0)
    {
        auto& stolen = activeNotes_[static_cast<size_t>(shortestIdx)];
        if (stolen.active)
            midiOut.addEvent(juce::MidiMessage::noteOff(stolen.channel, stolen.note), sampleOffset);
        freeSlot = shortestIdx;
    }

    activeNotes_[static_cast<size_t>(freeSlot)] = {
        true,
        ch,
        note,
        safeGate - chunkSamples
    };
}

void ImageMidiControlEngine::scheduleEchoNote(int pathIndex,
                                              int channel,
                                              int midiNote,
                                              float velocityNorm,
                                              int gateSamples,
                                              int delaySamples) noexcept
{
    int freeSlot = -1;
    int longestIdx = 0;
    int longestDelay = -1;

    for (int i = 0; i < kMaxPendingEchoes; ++i)
    {
        const auto& s = scheduledNotes_[static_cast<size_t>(i)];
        if (!s.active)
        {
            freeSlot = i;
            break;
        }
        if (s.delayRemainingSamples > longestDelay)
        {
            longestDelay = s.delayRemainingSamples;
            longestIdx = i;
        }
    }

    if (freeSlot < 0)
        freeSlot = longestIdx;

    scheduledNotes_[static_cast<size_t>(freeSlot)] = {
        true,
        pathIndex,
        std::clamp(channel, 1, 16),
        std::clamp(midiNote, 0, 127),
        std::clamp(velocityNorm, 0.05f, 1.0f),
        std::max(1, gateSamples),
        std::max(1, delaySamples),
        true
    };
}

void ImageMidiControlEngine::allNotesOff(juce::MidiBuffer& midiOut, bool sendAllNotesOffCC) noexcept
{
    for (auto& n : activeNotes_)
    {
        if (n.active)
        {
            midiOut.addEvent(juce::MidiMessage::noteOff(n.channel, n.note), 0);
            n.active = false;
            n.remainingSamples = 0;
        }
    }

    for (auto& s : scheduledNotes_)
        s.active = false;

    if (sendAllNotesOffCC)
    {
        for (int ch = 1; ch <= 16; ++ch)
            midiOut.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
    }
}

void ImageMidiControlEngine::stepPathChain(int pathIndex,
                                           const PickupPathConfig& cfg,
                                           PathRuntime& rt,
                                           const SynestheticGrid* grid,
                                           juce::MidiBuffer& midiOut,
                                           int sampleOffset,
                                           int chunkSamples,
                                           double dtSec,
                                           const ControlEngineParams& params) noexcept
{
    const size_t pu = static_cast<size_t>(pathIndex);
    auto& vis = pathVisuals_[pu];
    vis.active = cfg.active;
    vis.muted = cfg.muted;

    const float fdt = static_cast<float>(dtSec);
    rt.triggerPulse = std::max(0.0f, rt.triggerPulse * (1.0f - fdt * 4.5f));

    if (!cfg.active)
        return;

    const double bpm = std::clamp(params.bpm, 20.0, 300.0);
    const double beatsPerSec = bpm / 60.0;
    const double loopBeats = loopLengthIndexToBeats(cfg.loopLengthIdx);

    // 1. Advance deterministic playhead along the path
    if (params.running)
    {
        if (params.hasHostPpq)
            rt.transportBeats = std::max(0.0, params.hostPpqPosition);
        else
            rt.transportBeats += dtSec * beatsPerSec;
    }

    double cycleNorm = std::fmod(rt.transportBeats / loopBeats + static_cast<double>(cfg.phaseOffset), 1.0);
    if (cycleNorm < 0.0)
        cycleNorm += 1.0;

    float normT = static_cast<float>(cycleNorm);
    if (cfg.playMode == 1) // Ping-Pong (deterministic triangle wave over 1 loop length)
    {
        const float tri = normT * 2.0f;
        normT = (tri <= 1.0f) ? tri : (2.0f - tri);
    }
    else if (cfg.playMode == 2) // Reverse
    {
        normT = 1.0f - normT;
    }

    rt.playheadT = std::clamp(normT, 0.0f, 1.0f);
    const auto pt = evaluatePathPoint(cfg, rt.playheadT);

    // 2. Sample raw pixel values from the painting & build the 14-channel Signal Bus
    const PixelSample px = grid->sampleAt(pt.x, 1.0f - pt.y);

    float rawDelta = 0.0f;
    if (rt.hasPrevSample)
    {
        const float dLum = std::abs(px.luminance - rt.prevSample.luminance);
        const float dHueRaw = std::abs(px.hue - rt.prevSample.hue);
        const float dHue = std::min(dHueRaw, 1.0f - dHueRaw);
        const float dSat = std::abs(px.saturation - rt.prevSample.saturation);
        rawDelta = std::clamp((dLum * 12.0f + dHue * 10.0f + dSat * 6.0f), 0.0f, 1.0f);
    }
    rt.prevSample = px;
    rt.hasPrevSample = true;

    const float deltaCoeff = 1.0f - std::exp(-fdt / 0.045f);
    rt.smoothedDelta += deltaCoeff * (rawDelta - rt.smoothedDelta);

    std::array<float, static_cast<size_t>(SignalId::Count)> bus {};
    bus[static_cast<size_t>(SignalId::Lum)]        = px.luminance;
    bus[static_cast<size_t>(SignalId::Hue)]        = px.hue;
    bus[static_cast<size_t>(SignalId::Sat)]        = px.saturation;
    bus[static_cast<size_t>(SignalId::Edge)]       = px.edge;
    bus[static_cast<size_t>(SignalId::Red)]        = px.r;
    bus[static_cast<size_t>(SignalId::Green)]      = px.g;
    bus[static_cast<size_t>(SignalId::Blue)]       = px.b;
    bus[static_cast<size_t>(SignalId::PosX)]       = pt.x;
    bus[static_cast<size_t>(SignalId::PosY)]       = 1.0f - pt.y; // bottom=0, top=1 (natural pitch)
    bus[static_cast<size_t>(SignalId::DeltaColor)] = std::clamp(rt.smoothedDelta, 0.0f, 1.0f);
    bus[static_cast<size_t>(SignalId::Phase)]      = rt.playheadT;
    bus[static_cast<size_t>(SignalId::ModA)]       = 0.0f;
    bus[static_cast<size_t>(SignalId::ModB)]       = 0.0f;
    bus[static_cast<size_t>(SignalId::Fixed)]      = 100.0f / 127.0f;

    auto readSig = [&bus](int sigIdx) noexcept -> float
    {
        return bus[static_cast<size_t>(std::clamp(sigIdx, 0, static_cast<int>(SignalId::Count) - 1))];
    };

    // 3. Process the Ableton-style Left -> Right Modular Device Chain!
    bool hasUpstreamGate = false;
    bool upstreamTriggerFired = false;
    double upstreamStepSec = 0.25;

    std::array<ChainNoteEvent, 32> notePipe {};
    int notePipeCount = 0;

    const int modCount = std::clamp(cfg.numModules, 0, kMaxModules);

    for (int mIdx = 0; mIdx < modCount; ++mIdx)
    {
        const auto& m = cfg.modules[static_cast<size_t>(mIdx)];
        auto& mrt = findOrResetModuleRuntime(rt, mIdx, m.uid);
        mrt.activityPulse = std::max(0.0f, mrt.activityPulse * (1.0f - fdt * 4.5f));

        if (!m.enabled)
        {
            vis.modules[static_cast<size_t>(mIdx)] = { m.uid, mrt.liveValue, mrt.activityPulse, mrt.lastNote, mrt.lastValueInt };
            continue;
        }

        switch (m.type)
        {
            case ModuleType::SignalShaper:
            {
                float v = readSig(m.inputSignal);
                if (m.flagA) // invert
                    v = 1.0f - v;
                v = applyCurve(v, m.mode);
                const float minO = std::clamp(m.paramA, 0.0f, 1.0f);
                const float maxO = std::clamp(m.paramB, 0.0f, 1.0f);
                const float target = std::clamp(minO + v * (maxO - minO), 0.0f, 1.0f);

                const float tauSec = std::clamp(m.paramC, 0.0f, 1.0f);
                if (tauSec <= 0.001f)
                {
                    mrt.slewedValue = target;
                }
                else
                {
                    const float coeff = 1.0f - std::exp(-fdt / tauSec);
                    mrt.slewedValue += coeff * (target - mrt.slewedValue);
                }

                const float outVal = std::clamp(mrt.slewedValue, 0.0f, 1.0f);
                const int dstIdx = std::clamp(m.outputSignal, 0, static_cast<int>(SignalId::Fixed) - 1);
                bus[static_cast<size_t>(dstIdx)] = outVal;

                mrt.liveValue = outVal;
                mrt.lastValueInt = static_cast<int>(std::round(outVal * 100.0f));
                break;
            }

            case ModuleType::TriggerGate:
            {
                hasUpstreamGate = true;
                const float sig = readSig(m.inputSignal);
                const float thresh = std::clamp(m.paramA, 0.02f, 0.98f);
                const float hyst = std::clamp(m.paramB, 0.01f, 0.25f);
                mrt.liveValue = sig;

                if (!params.running || cfg.muted)
                    break;

                if (m.mode == 1) // Rising Edge crossing (sample-accurate unclocked!)
                {
                    if (!mrt.gateHigh && sig >= thresh)
                    {
                        mrt.gateHigh = true;
                        upstreamTriggerFired = true;
                        upstreamStepSec = 0.25;
                        mrt.activityPulse = 1.0f;
                    }
                    else if (mrt.gateHigh && sig < std::max(0.0f, thresh - hyst))
                    {
                        mrt.gateHigh = false;
                    }
                }
                else // 0 = Clock + Thresh, 2 = Euclidean
                {
                    const double stepBeats = static_cast<double>(ScaleQuantizer::divisionToBeats(m.intA));
                    mrt.beatAccum += dtSec * beatsPerSec;
                    while (mrt.beatAccum >= stepBeats)
                    {
                        mrt.beatAccum -= stepBeats;
                        const int stepIdx = mrt.stepCounter++;
                        bool stepActive = true;
                        if (m.mode == 2)
                        {
                            const int steps = std::clamp(m.intB, 2, 32);
                            const int pulses = std::clamp(m.intC, 1, steps);
                            stepActive = ScaleQuantizer::isEuclideanStepActive(stepIdx % steps, steps, pulses, 0);
                        }

                        if (stepActive && sig >= thresh)
                        {
                            upstreamTriggerFired = true;
                            upstreamStepSec = stepBeats / beatsPerSec;
                            mrt.activityPulse = 1.0f;
                        }
                    }
                }
                break;
            }

            case ModuleType::NoteGenerator:
            {
                if (!params.running || cfg.muted)
                    break;

                bool fire = false;
                double stepSec = 0.25;

                const bool useUpstream = (m.mode == 2) || (m.mode == 0 && hasUpstreamGate);
                if (useUpstream)
                {
                    fire = upstreamTriggerFired;
                    stepSec = upstreamStepSec;
                }
                else
                {
                    const double stepBeats = static_cast<double>(ScaleQuantizer::divisionToBeats(m.intB));
                    mrt.beatAccum += dtSec * beatsPerSec;
                    if (mrt.beatAccum >= stepBeats)
                    {
                        mrt.beatAccum = std::fmod(mrt.beatAccum, stepBeats);
                        fire = true;
                        stepSec = stepBeats / beatsPerSec;
                    }
                }

                if (fire && notePipeCount < static_cast<int>(notePipe.size()))
                {
                    const float pitchNorm = readSig(m.inputSignal);
                    const float velNorm = std::clamp(0.18f + 0.82f * readSig(m.secondarySignal), 0.10f, 1.0f);
                    const int baseMidi = (std::clamp(m.intC, 1, 6) + 1) * 12;
                    const int span = std::clamp(m.intD, 1, 4);
                    const int midiNote = ScaleQuantizer::quantizeNormYToMidiNote(
                        pitchNorm, params.rootNote, params.scaleType, baseMidi, span);
                    const int gateSamples = static_cast<int>(
                        stepSec * static_cast<double>(std::clamp(m.paramA, 0.1f, 0.95f)) * sampleRate_);
                    const int ch = std::clamp(m.intA, 1, 16);

                    notePipe[static_cast<size_t>(notePipeCount++)] = {
                        ch, midiNote, velNorm, std::max(64, gateSamples), 0, false
                    };

                    mrt.lastNote = midiNote;
                    mrt.liveValue = pitchNorm;
                    mrt.activityPulse = 1.0f;
                    rt.lastMidiNote = midiNote;
                    rt.lastChannel = ch;
                    rt.triggerPulse = 1.0f;
                }
                break;
            }

            case ModuleType::Chord:
            {
                if (notePipeCount <= 0)
                    break;

                bool addChord = true;
                if (m.flagA) // Modulate chord activation by signal threshold
                {
                    const float modVal = readSig(m.secondarySignal);
                    mrt.liveValue = modVal;
                    addChord = (modVal >= std::clamp(m.paramB, 0.0f, 1.0f));
                }

                if (!addChord)
                    break;

                std::array<int, 3> offsets {};
                int numExtras = 0;
                switch (m.mode)
                {
                    case 0: offsets = { 7, 0, 0 };  numExtras = 1; break; // Dyad +5th
                    case 1: offsets = { 4, 7, 0 };  numExtras = 2; break; // Triad
                    case 2: offsets = { 4, 7, 11 }; numExtras = 3; break; // Seventh
                    case 3: offsets = { 2, 7, 0 };  numExtras = 2; break; // Sus2
                    case 4: offsets = { 12, 0, 0 }; numExtras = 1; break; // Octave
                    default: offsets = { 7, 12, 0 }; numExtras = 2; break; // Power
                }

                const int strumSamples = static_cast<int>(std::clamp(m.paramA, 0.0f, 0.5f) * 0.2f * sampleRate_);
                const int initialCount = notePipeCount;
                for (int n = 0; n < initialCount; ++n)
                {
                    const auto baseEv = notePipe[static_cast<size_t>(n)];
                    for (int k = 0; k < numExtras && notePipeCount < static_cast<int>(notePipe.size()); ++k)
                    {
                        const int hNote = ScaleQuantizer::transposeInScale(
                            baseEv.note, offsets[static_cast<size_t>(k)], params.rootNote, params.scaleType);
                        notePipe[static_cast<size_t>(notePipeCount++)] = {
                            baseEv.channel,
                            hNote,
                            std::clamp(baseEv.velocity * 0.90f, 0.12f, 1.0f),
                            baseEv.gateSamples,
                            baseEv.delaySamples + strumSamples * (k + 1),
                            true
                        };
                        mrt.lastNote = hNote;
                    }
                }
                mrt.activityPulse = 1.0f;
                break;
            }

            case ModuleType::Chance:
            {
                if (notePipeCount <= 0)
                    break;

                float prob = std::clamp(m.paramA, 0.0f, 1.0f);
                if (m.flagA)
                    prob = std::clamp(prob * readSig(m.secondarySignal), 0.0f, 1.0f);
                mrt.liveValue = prob;

                const float vMin = std::clamp(m.paramB, 0.05f, 1.0f);
                const float vMax = std::clamp(std::max(vMin, m.paramC), 0.05f, 1.0f);

                int writeIdx = 0;
                for (int n = 0; n < notePipeCount; ++n)
                {
                    if (nextRandomFloat() <= prob)
                    {
                        auto ev = notePipe[static_cast<size_t>(n)];
                        ev.velocity = std::clamp(vMin + ev.velocity * (vMax - vMin), 0.05f, 1.0f);
                        notePipe[static_cast<size_t>(writeIdx++)] = ev;
                        mrt.activityPulse = 1.0f;
                    }
                }
                notePipeCount = writeIdx;
                break;
            }

            case ModuleType::MidiEcho:
            {
                if (notePipeCount <= 0)
                    break;

                const double delayBeats = static_cast<double>(ScaleQuantizer::divisionToBeats(m.intA));
                const int delayStepSamples = std::max(64, static_cast<int>((delayBeats / beatsPerSec) * sampleRate_));
                const int repeats = std::clamp(m.intB, 1, 6);
                const int pitchStep = std::clamp(m.intC, -12, 12);
                const float decay = std::clamp(m.paramA, 0.2f, 0.95f);

                for (int n = 0; n < notePipeCount; ++n)
                {
                    const auto& ev = notePipe[static_cast<size_t>(n)];
                    int curNote = ev.note;
                    float curVel = ev.velocity;
                    for (int rep = 1; rep <= repeats; ++rep)
                    {
                        curVel *= decay;
                        if (curVel < 0.06f)
                            break;
                        if (pitchStep != 0)
                            curNote = ScaleQuantizer::transposeInScale(curNote, pitchStep, params.rootNote, params.scaleType);

                        scheduleEchoNote(pathIndex, ev.channel, curNote, curVel,
                                         ev.gateSamples, ev.delaySamples + delayStepSamples * rep);
                        mrt.lastNote = curNote;
                    }
                }
                mrt.activityPulse = 1.0f;
                break;
            }

            case ModuleType::CcMapper:
            {
                float v = readSig(m.inputSignal);
                if (m.flagA)
                    v = 1.0f - v;
                v = applyCurve(v, m.intC);
                const float minO = std::clamp(m.paramA, 0.0f, 1.0f);
                const float maxO = std::clamp(m.paramB, 0.0f, 1.0f);
                const float target = std::clamp(minO + v * (maxO - minO), 0.0f, 1.0f);

                const float tauSec = std::clamp(m.paramC, 0.0f, 1.0f);
                if (tauSec <= 0.001f)
                {
                    mrt.slewedValue = target;
                }
                else
                {
                    const float coeff = 1.0f - std::exp(-fdt / tauSec);
                    mrt.slewedValue += coeff * (target - mrt.slewedValue);
                }

                const float outVal = std::clamp(mrt.slewedValue, 0.0f, 1.0f);
                mrt.liveValue = outVal;

                const int macroSlot = std::clamp(m.intD, 0, kNumMacroOutputs - 1);
                macroOutputs_[static_cast<size_t>(macroSlot)] = outVal;

                if (!params.running || cfg.muted)
                    break;

                const int ch = std::clamp(m.intA, 1, 16);
                const int ccNum = std::clamp(m.intB, 0, 127);

                if (m.mode == 3) // Macro Only
                {
                    mrt.lastValueInt = static_cast<int>(std::round(outVal * 100.0f));
                }
                else if (m.mode == 1) // Pitch Bend
                {
                    const int pbVal = std::clamp(static_cast<int>(std::round(outVal * 16383.0f)), 0, 16383);
                    mrt.lastValueInt = pbVal;
                    if (pbVal != mrt.lastSentMidiVal || mrt.lastSentTarget != m.mode || mrt.lastSentChannel != ch)
                    {
                        midiOut.addEvent(juce::MidiMessage::pitchWheel(ch, pbVal), sampleOffset);
                        mrt.lastSentMidiVal = pbVal;
                        mrt.lastSentTarget = m.mode;
                        mrt.lastSentChannel = ch;
                        mrt.activityPulse = 0.8f;
                    }
                }
                else if (m.mode == 2) // Channel Aftertouch
                {
                    const int atVal = std::clamp(static_cast<int>(std::round(outVal * 127.0f)), 0, 127);
                    mrt.lastValueInt = atVal;
                    if (atVal != mrt.lastSentMidiVal || mrt.lastSentTarget != m.mode || mrt.lastSentChannel != ch)
                    {
                        midiOut.addEvent(juce::MidiMessage::channelPressureChange(ch, atVal), sampleOffset);
                        mrt.lastSentMidiVal = atVal;
                        mrt.lastSentTarget = m.mode;
                        mrt.lastSentChannel = ch;
                        mrt.activityPulse = 0.8f;
                    }
                }
                else // 0 = MIDI CC
                {
                    const int ccVal = std::clamp(static_cast<int>(std::round(outVal * 127.0f)), 0, 127);
                    mrt.lastValueInt = ccVal;
                    if (ccVal != mrt.lastSentMidiVal || mrt.lastSentTarget != m.mode
                        || mrt.lastSentCcNum != ccNum || mrt.lastSentChannel != ch)
                    {
                        midiOut.addEvent(juce::MidiMessage::controllerEvent(ch, ccNum, ccVal), sampleOffset);
                        if (std::abs(ccVal - mrt.lastLoggedCcVal) >= 6 || mrt.lastSentCcNum != ccNum || mrt.lastSentChannel != ch)
                        {
                            pushMidiLog(MidiLogEntry::Kind::CC, pathIndex, ch, ccNum, ccVal);
                            mrt.lastLoggedCcVal = ccVal;
                        }
                        mrt.lastSentMidiVal = ccVal;
                        mrt.lastSentTarget = m.mode;
                        mrt.lastSentCcNum = ccNum;
                        mrt.lastSentChannel = ch;
                        mrt.activityPulse = 0.7f;
                    }
                }
                break;
            }

            case ModuleType::Count:
                break;
        }

        vis.modules[static_cast<size_t>(mIdx)] = {
            m.uid,
            mrt.liveValue,
            mrt.activityPulse,
            mrt.lastNote,
            mrt.lastValueInt
        };
    }

    // 4. Emit surviving MIDI note events from the end of the chain
    for (int n = 0; n < notePipeCount; ++n)
    {
        const auto& ev = notePipe[static_cast<size_t>(n)];
        if (ev.delaySamples > 0)
        {
            scheduleEchoNote(pathIndex, ev.channel, ev.note, ev.velocity, ev.gateSamples, ev.delaySamples);
        }
        else
        {
            triggerMidiNote(midiOut, sampleOffset, chunkSamples, pathIndex,
                            ev.channel, ev.note, ev.velocity, ev.gateSamples, ev.isFxNote);
        }
    }

    // 5. Store pickup visual telemetry
    vis.playheadX = pt.x;
    vis.playheadY = pt.y;
    vis.normalizedPhase = rt.playheadT;
    vis.r = px.r;
    vis.g = px.g;
    vis.b = px.b;
    vis.triggerPulse = rt.triggerPulse;
    vis.lastMidiNote = rt.lastMidiNote;
    vis.lastChannel = rt.lastChannel;
    vis.signals = bus;
}

void ImageMidiControlEngine::publishSnapshot() noexcept
{
    const juce::SpinLock::ScopedTryLockType tryLock(snapshotLock_);
    if (!tryLock.isLocked())
        return;

    publishedSnapshot_.paths = pathVisuals_;
    publishedSnapshot_.macroOutputs = macroOutputs_;

    for (int i = 0; i < kMidiLogCapacity; ++i)
    {
        const int idx = (midiLogWriteIdx_ - 1 - i + kMidiLogCapacity) % kMidiLogCapacity;
        publishedSnapshot_.midiLog[static_cast<size_t>(i)] = midiLog_[static_cast<size_t>(idx)];
    }
}

void ImageMidiControlEngine::processBlock(const SynestheticGrid* grid,
                                          juce::MidiBuffer& midiOut,
                                          int numSamples,
                                          const ControlEngineParams& params) noexcept
{
    if (numSamples <= 0)
        return;

    pullPendingDocument();

    if (wasRunning_ && !params.running)
        allNotesOff(midiOut, false);
    wasRunning_ = params.running;

    constexpr int kSubBlockSamples = 32;
    int offset = 0;

    while (offset < numSamples)
    {
        const int chunk = std::min(kSubBlockSamples, numSamples - offset);
        const double dtSec = static_cast<double>(chunk) / sampleRate_;

        expireActiveNotes(midiOut, offset, chunk);
        processScheduledNotes(midiOut, offset, chunk);

        if (grid != nullptr)
        {
            for (int p = 0; p < kMaxPaths; ++p)
            {
                const size_t pu = static_cast<size_t>(p);
                stepPathChain(p, audioDoc_.paths[pu], pathRuntime_[pu],
                              grid, midiOut, offset, chunk, dtSec, params);
            }
        }

        offset += chunk;
    }

    publishSnapshot();
}

RackVisualSnapshot ImageMidiControlEngine::getVisualSnapshot() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(snapshotLock_);
    return publishedSnapshot_;
}

std::array<float, ImageMidiControlEngine::kNumMacroOutputs> ImageMidiControlEngine::getMacroOutputs() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(snapshotLock_);
    return publishedSnapshot_.macroOutputs;
}

} // namespace soundcanvas
