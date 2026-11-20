#pragma once

#include <JuceHeader.h>
#include "core/ImageCanvas.h"
#include "dsp/ScaleQuantizer.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace soundcanvas
{

enum class SignalId : int
{
    Lum = 0,
    Hue,
    Sat,
    Edge,
    Red,
    Green,
    Blue,
    PosX,
    PosY,
    DeltaColor,
    Phase,
    ModA,
    ModB,
    Fixed,
    Count
};

enum class ModuleType : int
{
    SignalShaper = 0,
    TriggerGate,
    NoteGenerator,
    Chord,
    Chance,
    MidiEcho,
    CcMapper,
    Count
};

struct ModuleConfig
{
    uint32_t uid = 1;
    ModuleType type = ModuleType::NoteGenerator;
    bool enabled = true;

    // Generic signal routing
    int inputSignal = static_cast<int>(SignalId::Lum);
    int outputSignal = static_cast<int>(SignalId::ModA); // For SignalShaper (can also overwrite input)
    int secondarySignal = static_cast<int>(SignalId::Lum); // Vel source for NoteGen, Mod source for Chord/Chance

    // Shared numeric/mode parameters (interpreted cleanly per ModuleType):
    // SignalShaper: mode=curve (0..4), paramA=minOut, paramB=maxOut, paramC=slewMs/1000, flagA=invert
    // TriggerGate:  mode=0(Clock+Thresh),1(Rising Edge),2(Euclidean); intA=rateDiv, intB=steps, intC=pulses, paramA=threshold, paramB=hysteresis
    // NoteGenerator: mode=0(Auto Trig),1(Internal Clock),2(Upstream Trig); intA=channel(1..16), intB=rateDiv, intC=baseOct(1..6), intD=octSpan(1..4), paramA=gate(0.1..0.95)
    // Chord:        mode=0(Dyad +5th),1(Triad),2(Seventh),3(Sus2),4(Octave),5(Power); paramA=strumMs/100, paramB=modThreshold, flagA=useModSignal
    // Chance:       paramA=probability(0..1), paramB=velMin(0..1), paramC=velMax(0..1), flagA=modProbBySignal
    // MidiEcho:     intA=rateDiv, intB=repeats(1..6), intC=pitchStep(-12..+12), paramA=decay(0.2..0.95)
    // CcMapper:     mode=0(MIDI CC),1(Pitch Bend),2(Aftertouch),3(Macro Only); intA=channel(1..16), intB=ccNum(0..127), intC=curve(0..4), intD=macroSlot(0..7), paramA=minOut, paramB=maxOut, paramC=slewMs/1000, flagA=invert
    int mode = 0;
    int intA = 1;
    int intB = 5;
    int intC = 3;
    int intD = 2;
    float paramA = 0.5f;
    float paramB = 1.0f;
    float paramC = 0.03f;
    bool flagA = false;
};

struct PickupPathConfig
{
    static constexpr int kMaxWaypoints = 16;
    static constexpr int kMaxModules = 8;

    bool active = false;    // Whether this path track exists in the rack
    bool muted = false;     // Quick mute toggle
    bool closedLoop = false;
    bool smoothCurve = true;
    int loopLengthIdx = 3;  // 0=1/4 Bar, 1=1/2 Bar, 2=1 Bar, 3=2 Bars, 4=4 Bars, 5=8 Bars
    int playMode = 0;       // 0=Forward, 1=Ping-Pong, 2=Reverse
    float phaseOffset = 0.0f; // 0.0..1.0

    int numPoints = 4;
    std::array<juce::Point<float>, kMaxWaypoints> points {};

    int numModules = 0;
    std::array<ModuleConfig, kMaxModules> modules {};
};

struct RackDocument
{
    static constexpr int kMaxPaths = 4;
    std::array<PickupPathConfig, kMaxPaths> paths {};
    uint32_t nextModuleUid = 100;
};

struct ControlEngineParams
{
    double bpm = 120.0;
    int rootNote = 0;
    int scaleType = 0;
    bool running = true;
    bool hasHostPpq = false;
    double hostPpqPosition = 0.0;
};

struct ModuleVisualState
{
    uint32_t uid = 0;
    float liveValue = 0.0f;   // 0..1 meter value (shaped signal, CC output, etc.)
    float activityPulse = 0.0f; // 0..1 flashing LED (trigger / note / echo)
    int lastNote = -1;
    int lastValueInt = 0;
};

struct PickupVisualState
{
    bool active = false;
    bool muted = false;
    float playheadX = 0.5f;
    float playheadY = 0.5f;
    float normalizedPhase = 0.0f;
    float r = 0.5f;
    float g = 0.7f;
    float b = 0.8f;
    float triggerPulse = 0.0f;
    int lastMidiNote = 60;
    int lastChannel = 1;
    std::array<float, static_cast<size_t>(SignalId::Count)> signals {};
    std::array<ModuleVisualState, PickupPathConfig::kMaxModules> modules {};
};

struct MidiLogEntry
{
    enum class Kind : uint8_t
    {
        Empty = 0,
        Note,
        ChordOrEchoNote,
        CC
    };

    Kind kind = Kind::Empty;
    int pathIndex = 0; // 0..3
    int channel = 0;   // 1..16
    int data1 = 0;     // note or CC#
    int data2 = 0;     // velocity or CC value
};

struct RackVisualSnapshot
{
    std::array<PickupVisualState, RackDocument::kMaxPaths> paths {};
    std::array<MidiLogEntry, 16> midiLog {};
    std::array<float, 8> macroOutputs {};
};

class ImageMidiControlEngine
{
public:
    static constexpr int kMaxPaths = RackDocument::kMaxPaths;
    static constexpr int kMaxWaypoints = PickupPathConfig::kMaxWaypoints;
    static constexpr int kMaxModules = PickupPathConfig::kMaxModules;
    static constexpr int kNumMacroOutputs = 8;
    static constexpr int kMaxActiveNotes = 96;
    static constexpr int kMaxPendingEchoes = 64;
    static constexpr int kMidiLogCapacity = 16;

    ImageMidiControlEngine();

    static juce::StringArray getSignalNames();
    static juce::StringArray getSignalShortNames();
    static juce::StringArray getModuleTypeNames();
    static juce::StringArray getLoopLengthNames();
    static double loopLengthIndexToBeats(int idx) noexcept;
    static juce::StringArray getPlayModeNames();
    static juce::StringArray getCurveNames();
    static juce::StringArray getTriggerModeNames();
    static juce::StringArray getNoteTrigSourceNames();
    static juce::StringArray getChordTypeNames();
    static juce::StringArray getCcTargetNames();
    static juce::StringArray getShapePresetNames();

    static ModuleConfig createDefaultModule(ModuleType type, uint32_t uid, int pathIndex) noexcept;
    static PickupPathConfig createDefaultPath(int pathIndex, uint32_t& nextUid) noexcept;
    static void applyShapePreset(PickupPathConfig& path, int shapeIndex) noexcept;
    static RackDocument createDefaultDocument() noexcept;

    // Deterministic arc-length-parameterized curve evaluator (used by both Engine & Canvas UI!)
    static juce::Point<float> evaluatePathPoint(const PickupPathConfig& path, float normT) noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Thread-safe document update from UI/Processor
    void setDocument(const RackDocument& doc) noexcept;

    void processBlock(const SynestheticGrid* grid,
                      juce::MidiBuffer& midiOut,
                      int numSamples,
                      const ControlEngineParams& params) noexcept;

    void allNotesOff(juce::MidiBuffer& midiOut, bool sendAllNotesOffCC = true) noexcept;

    RackVisualSnapshot getVisualSnapshot() const noexcept;
    std::array<float, kNumMacroOutputs> getMacroOutputs() const noexcept;

private:
    struct ModuleRuntime
    {
        uint32_t uid = 0;
        float slewedValue = 0.0f;
        bool gateHigh = false;
        double beatAccum = 0.0;
        int stepCounter = 0;
        int lastSentMidiVal = -1;
        int lastSentCcNum = -1;
        int lastSentTarget = -1;
        int lastSentChannel = -1;
        int lastLoggedCcVal = -10;
        float activityPulse = 0.0f;
        float liveValue = 0.0f;
        int lastNote = -1;
        int lastValueInt = 0;
    };

    struct PathRuntime
    {
        double transportBeats = 0.0;
        float playheadT = 0.0f;
        PixelSample prevSample {};
        bool hasPrevSample = false;
        float smoothedDelta = 0.0f;
        float triggerPulse = 0.0f;
        int lastMidiNote = 60;
        int lastChannel = 1;
        std::array<ModuleRuntime, kMaxModules> modRuntime {};
    };

    struct ChainNoteEvent
    {
        int channel = 1;
        int note = 60;
        float velocity = 0.8f;
        int gateSamples = 12000;
        int delaySamples = 0;
        bool isFxNote = false;
    };

    struct ActiveNote
    {
        bool active = false;
        int channel = 1;
        int note = 60;
        int remainingSamples = 0;
    };

    struct ScheduledNote
    {
        bool active = false;
        int pathIndex = 0;
        int channel = 1;
        int note = 60;
        float velocity = 0.8f;
        int gateSamples = 12000;
        int delayRemainingSamples = 0;
        bool isFxNote = true;
    };

    static float applyCurve(float x, int curveType) noexcept;
    static juce::Point<float> evalCatmullRom(juce::Point<float> p0,
                                             juce::Point<float> p1,
                                             juce::Point<float> p2,
                                             juce::Point<float> p3,
                                             float t) noexcept;

    ModuleRuntime& findOrResetModuleRuntime(PathRuntime& pr, int slotIdx, uint32_t uid) noexcept;

    void pullPendingDocument() noexcept;
    void expireActiveNotes(juce::MidiBuffer& midiOut, int startSample, int chunkSamples) noexcept;
    void processScheduledNotes(juce::MidiBuffer& midiOut, int startSample, int chunkSamples) noexcept;
    void triggerMidiNote(juce::MidiBuffer& midiOut,
                         int sampleOffset,
                         int chunkSamples,
                         int pathIndex,
                         int channel,
                         int midiNote,
                         float velocityNorm,
                         int gateSamples,
                         bool isFxNote) noexcept;
    void scheduleEchoNote(int pathIndex,
                          int channel,
                          int midiNote,
                          float velocityNorm,
                          int gateSamples,
                          int delaySamples) noexcept;
    void pushMidiLog(MidiLogEntry::Kind kind, int pathIndex, int channel, int data1, int data2) noexcept;
    float nextRandomFloat() noexcept;

    void stepPathChain(int pathIndex,
                       const PickupPathConfig& cfg,
                       PathRuntime& rt,
                       const SynestheticGrid* grid,
                       juce::MidiBuffer& midiOut,
                       int sampleOffset,
                       int chunkSamples,
                       double dtSec,
                       const ControlEngineParams& params) noexcept;

    void publishSnapshot() noexcept;

    double sampleRate_ = 48000.0;
    uint32_t rngState_ = 0x5A17C0DEu;
    bool wasRunning_ = true;

    mutable juce::SpinLock docLock_;
    RackDocument stagedDoc_ {};
    std::atomic<bool> docDirty_ { false };
    RackDocument audioDoc_ {};

    std::array<PathRuntime, kMaxPaths> pathRuntime_ {};
    std::array<PickupVisualState, kMaxPaths> pathVisuals_ {};
    std::array<float, kNumMacroOutputs> macroOutputs_ {};

    std::array<ActiveNote, kMaxActiveNotes> activeNotes_ {};
    std::array<ScheduledNote, kMaxPendingEchoes> scheduledNotes_ {};
    std::array<MidiLogEntry, kMidiLogCapacity> midiLog_ {};
    int midiLogWriteIdx_ = 0;

    mutable juce::SpinLock snapshotLock_;
    RackVisualSnapshot publishedSnapshot_ {};
};

} // namespace soundcanvas
