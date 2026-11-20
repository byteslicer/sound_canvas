#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

class ScaleQuantizer
{
public:
    enum ScaleType
    {
        MajorPentatonic = 0,
        MinorPentatonic,
        NaturalMinor,
        Dorian,
        Lydian,
        MajorIonian,
        HarmonicMinor,
        Hirajoshi,
        WholeTone,
        Chromatic,
        NumScales
    };

    static juce::StringArray getScaleNames()
    {
        return {
            "Major Pentatonic",
            "Minor Pentatonic",
            "Natural Minor (Aeolian)",
            "Dorian",
            "Lydian (Dreamy)",
            "Major (Ionian)",
            "Harmonic Minor",
            "Hirajoshi (Japanese)",
            "Whole Tone (Impressionist)",
            "Chromatic"
        };
    }

    static juce::StringArray getRootNoteNames()
    {
        return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    }

    static const std::vector<int>& getScaleIntervals(int scaleIndex) noexcept
    {
        static const std::array<std::vector<int>, NumScales> kScales = {{
            { 0, 2, 4, 7, 9 },                      // Major Pentatonic
            { 0, 3, 5, 7, 10 },                     // Minor Pentatonic
            { 0, 2, 3, 5, 7, 8, 10 },               // Natural Minor
            { 0, 2, 3, 5, 7, 9, 10 },               // Dorian
            { 0, 2, 4, 6, 7, 9, 11 },               // Lydian
            { 0, 2, 4, 5, 7, 9, 11 },               // Major Ionian
            { 0, 2, 3, 5, 7, 8, 11 },               // Harmonic Minor
            { 0, 2, 3, 7, 8 },                      // Hirajoshi
            { 0, 2, 4, 6, 8, 10 },                  // Whole Tone
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } // Chromatic
        }};

        const int idx = std::clamp(scaleIndex, 0, static_cast<int>(NumScales) - 1);
        return kScales[static_cast<size_t>(idx)];
    }

    static int quantizeNormYToMidiNote(float normY,
                                       int rootNote,
                                       int scaleIndex,
                                       int baseMidiNote = 36,
                                       int octaveSpan = 4) noexcept
    {
        const auto& intervals = getScaleIntervals(scaleIndex);
        const int numDegreesPerOctave = static_cast<int>(intervals.size());
        const int totalSteps = std::max(1, numDegreesPerOctave * octaveSpan);

        const float clampedY = std::clamp(normY, 0.0f, 1.0f);
        const int step = std::clamp(static_cast<int>(std::round(clampedY * static_cast<float>(totalSteps - 1))),
                                    0, totalSteps - 1);

        const int octave = step / numDegreesPerOctave;
        const int degree = step % numDegreesPerOctave;
        const int rootOffset = std::clamp(rootNote, 0, 11);

        const int midiNote = baseMidiNote + rootOffset + octave * 12 + intervals[static_cast<size_t>(degree)];
        return std::clamp(midiNote, 12, 108);
    }

    static bool isNoteInScale(int midiNote, int rootNote, int scaleIndex) noexcept
    {
        const auto& intervals = getScaleIntervals(scaleIndex);
        const int rootOffset = std::clamp(rootNote, 0, 11);
        const int rel = ((midiNote - rootOffset) % 12 + 12) % 12;
        return std::find(intervals.begin(), intervals.end(), rel) != intervals.end();
    }

    static int snapMidiNoteToScale(int midiNote, int rootNote, int scaleIndex) noexcept
    {
        const auto& intervals = getScaleIntervals(scaleIndex);
        const int rootOffset = std::clamp(rootNote, 0, 11);
        const int shifted = midiNote - rootOffset;
        const int octave = static_cast<int>(std::floor(static_cast<float>(shifted) / 12.0f));
        const int rel = ((shifted % 12) + 12) % 12;

        int bestInterval = intervals[0];
        int bestDist = 99;
        for (int iv : intervals)
        {
            const int dist = std::abs(iv - rel);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestInterval = iv;
            }
        }

        return std::clamp(rootOffset + octave * 12 + bestInterval, 12, 108);
    }

    static int transposeInScale(int midiNote, int semitoneOffset, int rootNote, int scaleIndex) noexcept
    {
        return snapMidiNoteToScale(std::clamp(midiNote + semitoneOffset, 12, 108), rootNote, scaleIndex);
    }

    static float midiNoteToFrequency(int midiNote) noexcept
    {
        return 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
    }

    static juce::StringArray getTempoDivisionNames()
    {
        return {
            "1/1 Bar",
            "1/2",
            "1/4",
            "1/4D",
            "1/4T",
            "1/8",
            "1/8D",
            "1/8T",
            "1/16",
            "1/16D",
            "1/16T",
            "1/32"
        };
    }

    static float divisionToBeats(int divisionIndex) noexcept
    {
        static constexpr float kBeats[12] = {
            4.0f,         // 1/1 Bar
            2.0f,         // 1/2
            1.0f,         // 1/4
            1.5f,         // 1/4D
            2.0f / 3.0f,  // 1/4T
            0.5f,         // 1/8
            0.75f,        // 1/8D
            1.0f / 3.0f,  // 1/8T
            0.25f,        // 1/16
            0.375f,       // 1/16D
            1.0f / 6.0f,  // 1/16T
            0.125f        // 1/32
        };
        const int idx = std::clamp(divisionIndex, 0, 11);
        return kBeats[idx];
    }

    static float divisionToStepDurationSec(int divisionIndex, double bpm) noexcept
    {
        const double safeBpm = std::clamp(bpm, 20.0, 300.0);
        const double quarterSec = 60.0 / safeBpm;
        return static_cast<float>(quarterSec * static_cast<double>(divisionToBeats(divisionIndex)));
    }

    static bool isEuclideanStepActive(int stepIndex, int totalSteps, int pulses, int rotation) noexcept
    {
        const int n = std::clamp(totalSteps, 1, 32);
        const int k = std::clamp(pulses, 0, n);
        if (k == 0)
            return false;
        if (k >= n)
            return true;

        const int rot = ((rotation % n) + n) % n;
        const int s = (((stepIndex - rot) % n) + n) % n;
        return ((s * k) % n) < k;
    }
};

} // namespace soundcanvas
