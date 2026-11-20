#include "gui/PaintingCanvasComponent.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

namespace
{
juce::String midiNoteToString(int midiNote)
{
    static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = std::clamp(midiNote, 0, 127);
    const int octave = (n / 12) - 1;
    return juce::String(kNames[n % 12]) + juce::String(octave);
}
} // namespace

PaintingCanvasComponent::PaintingCanvasComponent(SoundCanvasAudioProcessor& processor)
    : processor_(processor)
{
    startTimerHz(30);
}

PaintingCanvasComponent::~PaintingCanvasComponent()
{
    stopTimer();
}

void PaintingCanvasComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<float> PaintingCanvasComponent::getCanvasPlotBounds() const noexcept
{
    return getLocalBounds().toFloat().reduced(8.0f);
}

juce::Point<float> PaintingCanvasComponent::screenToNorm(juce::Point<float> pt) const noexcept
{
    const auto b = getCanvasPlotBounds();
    if (b.getWidth() <= 1.0f || b.getHeight() <= 1.0f)
        return { 0.5f, 0.5f };

    const float nx = std::clamp((pt.x - b.getX()) / b.getWidth(), 0.0f, 1.0f);
    const float ny = std::clamp((pt.y - b.getY()) / b.getHeight(), 0.0f, 1.0f);
    return { nx, ny };
}

juce::Point<float> PaintingCanvasComponent::normToScreen(juce::Point<float> normPt) const noexcept
{
    const auto b = getCanvasPlotBounds();
    return {
        b.getX() + normPt.x * b.getWidth(),
        b.getY() + normPt.y * b.getHeight()
    };
}

void PaintingCanvasComponent::paint(juce::Graphics& g)
{
    const auto plot = getCanvasPlotBounds();

    // Outer gallery frame
    g.setColour(juce::Colour(0xff13151c));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0f);

    g.setColour(juce::Colour(0xff2b3040));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.5f);

    auto grid = processor_.getCurrentGrid();
    if (grid && grid->getDisplayImage().isValid())
    {
        g.saveState();
        juce::Path clipPath;
        clipPath.addRoundedRectangle(plot, 6.0f);
        g.reduceClipRegion(clipPath);

        g.drawImage(grid->getDisplayImage(), plot, juce::RectanglePlacement::stretchToFit);

        // Subtle dark vignette so glowing overlays are crisp over very bright paintings
        g.setColour(juce::Colours::black.withAlpha(0.14f));
        g.fillRect(plot);

        // 1. Draw Wavetable Brushstroke Slice Ribbon (A <-> B)
        const float wtMix = processor_.getApvts().getRawParameterValue("wavetableMix")->load();
        if (wtMix > 0.01f)
        {
            const auto normA = processor_.getWavetableEngine().getSlicePointA();
            const auto normB = processor_.getWavetableEngine().getSlicePointB();
            const auto ptA = normToScreen(normA);
            const auto ptB = normToScreen(normB);

            const float dx = ptB.x - ptA.x;
            const float dy = ptB.y - ptA.y;
            const float len = std::max(1.0f, std::hypot(dx, dy));
            const float nx = -dy / len;
            const float ny =  dx / len;

            // Ribbon boundary
            juce::Path ribbonPath;
            const float ribPx = 24.0f;
            ribbonPath.startNewSubPath(ptA.x - nx * ribPx, ptA.y - ny * ribPx);
            ribbonPath.lineTo(ptB.x - nx * ribPx, ptB.y - ny * ribPx);
            ribbonPath.lineTo(ptB.x + nx * ribPx, ptB.y + ny * ribPx);
            ribbonPath.lineTo(ptA.x + nx * ribPx, ptA.y + ny * ribPx);
            ribbonPath.closeSubPath();

            g.setColour(juce::Colour(0x3800e5ff));
            g.fillPath(ribbonPath);
            g.setColour(juce::Colour(0x9000e5ff));
            g.strokePath(ribbonPath, juce::PathStrokeType(1.2f));

            // Draw live wavetable waveform along the brushstroke line
            auto wtData = processor_.getWavetableEngine().getWavetable();
            if (wtData)
            {
                const float morphPos = processor_.getWavetableEngine().getActiveMorphPosition();
                const int frameIdx = std::clamp(
                    static_cast<int>(morphPos * static_cast<float>(WavetableData::kNumFrames - 1)),
                    0, WavetableData::kNumFrames - 1);
                const float morphOffset = (morphPos - 0.5f) * 2.0f * ribPx;

                juce::Path wavePath;
                constexpr int kStep = 2;
                for (int s = 0; s < WavetableData::kSamplesPerFrame; s += kStep)
                {
                    const float t = static_cast<float>(s) / static_cast<float>(WavetableData::kSamplesPerFrame - 1);
                    const float amp = wtData->frames[static_cast<size_t>(frameIdx)][static_cast<size_t>(s)] * 18.0f;
                    const float wx = ptA.x + t * dx + nx * (morphOffset + amp);
                    const float wy = ptA.y + t * dy + ny * (morphOffset + amp);
                    if (s == 0)
                        wavePath.startNewSubPath(wx, wy);
                    else
                        wavePath.lineTo(wx, wy);
                }

                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.strokePath(wavePath, juce::PathStrokeType(2.2f));
            }

            // Endpoints A and B handles
            auto drawHandle = [&](juce::Point<float> p, const char* label)
            {
                g.setColour(juce::Colour(0xff00e5ff));
                g.fillEllipse(p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f);
                g.setColour(juce::Colours::black);
                g.drawEllipse(p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f, 1.5f);
                g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
                g.drawText(label, juce::Rectangle<float>(p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f),
                           juce::Justification::centred, false);
            };
            drawHandle(ptA, "A");
            drawHandle(ptB, "B");
        }

        // 2. Draw Spectral Scanner Playhead & Active Harmonic Partials
        const float scanMix = processor_.getApvts().getRawParameterValue("scannerMix")->load();
        if (scanMix > 0.01f)
        {
            const float playheadNormX = processor_.getScannerEngine().getPlayheadX();
            const float px = plot.getX() + playheadNormX * plot.getWidth();

            // Glowing vertical beam
            g.setColour(juce::Colour(0x44ffd54f));
            g.drawLine(px, plot.getY(), px, plot.getBottom(), 6.0f);
            g.setColour(juce::Colour(0xdfffecb3));
            g.drawLine(px, plot.getY(), px, plot.getBottom(), 1.8f);

            const auto bands = processor_.getScannerEngine().getVisualBands();
            for (const auto& b : bands)
            {
                if (b.amplitude < 0.05f)
                    continue;

                const float py = plot.getBottom() - b.normY * plot.getHeight();
                const float radius = std::clamp(2.5f + b.amplitude * 6.5f, 2.5f, 10.0f);
                const juce::Colour orbCol = juce::Colour::fromFloatRGBA(
                    std::clamp(b.r + 0.2f, 0.0f, 1.0f),
                    std::clamp(b.g + 0.2f, 0.0f, 1.0f),
                    std::clamp(b.b + 0.2f, 0.0f, 1.0f),
                    std::clamp(0.45f + b.amplitude * 0.5f, 0.0f, 1.0f));

                g.setColour(orbCol);
                g.fillEllipse(px - radius, py - radius, radius * 2.0f, radius * 2.0f);
            }
        }

        // 3. Draw Generative Paint Walker Orbiters & Trails
        const float orbMix = processor_.getApvts().getRawParameterValue("orbiterMix")->load();
        if (orbMix > 0.01f)
        {
            const auto orbiters = processor_.getOrbiterEngine().getVisualStates();
            for (size_t i = 0; i < orbiters.size(); ++i)
            {
                const auto& orb = orbiters[i];
                const juce::Colour baseCol = juce::Colour::fromFloatRGBA(
                    std::clamp(orb.r + 0.15f, 0.2f, 1.0f),
                    std::clamp(orb.g + 0.15f, 0.2f, 1.0f),
                    std::clamp(orb.b + 0.15f, 0.2f, 1.0f),
                    1.0f);

                // Comet trail
                juce::Path trailPath;
                for (int t = 0; t < OrbiterEngine::kTrailLength; ++t)
                {
                    const int idx = (orb.trailWriteIndex + t) % OrbiterEngine::kTrailLength;
                    const auto tp = normToScreen(orb.trail[static_cast<size_t>(idx)]);
                    if (t == 0)
                        trailPath.startNewSubPath(tp);
                    else
                        trailPath.lineTo(tp);
                }
                g.setColour(baseCol.withAlpha(0.45f));
                g.strokePath(trailPath, juce::PathStrokeType(2.2f));

                const auto pos = normToScreen({ orb.x, orb.y });

                // Note trigger ripple ring
                if (orb.triggerPulse > 0.05f)
                {
                    const float rippleR = 9.0f + (1.0f - orb.triggerPulse) * 26.0f;
                    g.setColour(juce::Colours::white.withAlpha(orb.triggerPulse * 0.85f));
                    g.drawEllipse(pos.x - rippleR, pos.y - rippleR, rippleR * 2.0f, rippleR * 2.0f, 2.0f);
                }

                // Core orbiter disc
                g.setColour(baseCol);
                g.fillEllipse(pos.x - 7.5f, pos.y - 7.5f, 15.0f, 15.0f);
                g.setColour(juce::Colours::white);
                g.drawEllipse(pos.x - 7.5f, pos.y - 7.5f, 15.0f, 15.0f, 1.6f);

                // Note label pill next to orbiter
                const juce::String noteText = midiNoteToString(orb.lastMidiNote);
                const juce::Rectangle<float> badge(pos.x + 9.0f, pos.y - 9.0f, 30.0f, 15.0f);
                g.setColour(juce::Colours::black.withAlpha(0.72f));
                g.fillRoundedRectangle(badge, 4.0f);
                g.setColour(juce::Colours::white.withAlpha(0.92f));
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(noteText, badge, juce::Justification::centred, false);
            }
        }

        // Painting Title & Interaction Hint Badge
        g.setColour(juce::Colours::black.withAlpha(0.62f));
        g.fillRoundedRectangle(plot.getX() + 10.0f, plot.getBottom() - 28.0f, 380.0f, 22.0f, 5.0f);
        g.setColour(juce::Colour(0xffd6deeb));
        g.setFont(juce::FontOptions(11.5f));
        g.drawText("Canvas: " + grid->getTitle() + "  |  Drag A/B slice, Orbiters, or Playhead",
                   juce::Rectangle<float>(plot.getX() + 16.0f, plot.getBottom() - 28.0f, 368.0f, 22.0f),
                   juce::Justification::centredLeft, false);

        g.restoreState();
    }

    if (isFileDragHover_)
    {
        g.setColour(juce::Colour(0xaa101824));
        g.fillRoundedRectangle(plot, 6.0f);
        g.setColour(juce::Colour(0xff00e5ff));
        g.drawRoundedRectangle(plot.reduced(4.0f), 6.0f, 2.5f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText("Drop Painting (.PNG / .JPG) to Sonify", plot, juce::Justification::centred, false);
    }
}

void PaintingCanvasComponent::handlePointerInteraction(juce::Point<float> pos, bool isInitialClick)
{
    const auto normPt = screenToNorm(pos);

    if (isInitialClick)
    {
        activeDrag_ = DragTarget::Playhead;
        float bestDist = 20.0f;

        const auto ptA = normToScreen(processor_.getWavetableEngine().getSlicePointA());
        const auto ptB = normToScreen(processor_.getWavetableEngine().getSlicePointB());

        if (pos.getDistanceFrom(ptA) < bestDist)
        {
            bestDist = pos.getDistanceFrom(ptA);
            activeDrag_ = DragTarget::SlicePointA;
        }
        if (pos.getDistanceFrom(ptB) < bestDist)
        {
            bestDist = pos.getDistanceFrom(ptB);
            activeDrag_ = DragTarget::SlicePointB;
        }

        const auto orbiters = processor_.getOrbiterEngine().getVisualStates();
        for (int i = 0; i < OrbiterEngine::kNumOrbiters; ++i)
        {
            const auto orbScreen = normToScreen({ orbiters[static_cast<size_t>(i)].x,
                                                  orbiters[static_cast<size_t>(i)].y });
            const float d = pos.getDistanceFrom(orbScreen);
            if (d < bestDist)
            {
                bestDist = d;
                activeDrag_ = static_cast<DragTarget>(static_cast<int>(DragTarget::Orbiter0) + i);
            }
        }
    }

    switch (activeDrag_)
    {
        case DragTarget::SlicePointA:
            processor_.updateWavetableSlice(normPt, processor_.getWavetableEngine().getSlicePointB());
            break;
        case DragTarget::SlicePointB:
            processor_.updateWavetableSlice(processor_.getWavetableEngine().getSlicePointA(), normPt);
            break;
        case DragTarget::Orbiter0:
        case DragTarget::Orbiter1:
        case DragTarget::Orbiter2:
        case DragTarget::Orbiter3:
        {
            const int idx = static_cast<int>(activeDrag_) - static_cast<int>(DragTarget::Orbiter0);
            processor_.getOrbiterEngine().setOrbiterPosition(idx, normPt.x, normPt.y);
            break;
        }
        case DragTarget::Playhead:
            processor_.getScannerEngine().setPlayheadX(normPt.x);
            break;
        case DragTarget::None:
            break;
    }

    repaint();
}

void PaintingCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    handlePointerInteraction(event.position, true);
}

void PaintingCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    handlePointerInteraction(event.position, false);
}

void PaintingCanvasComponent::mouseUp(const juce::MouseEvent&)
{
    activeDrag_ = DragTarget::None;
}

bool PaintingCanvasComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const juce::String ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            return true;
    }
    return false;
}

void PaintingCanvasComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isFileDragHover_ = true;
    repaint();
}

void PaintingCanvasComponent::fileDragExit(const juce::StringArray&)
{
    isFileDragHover_ = false;
    repaint();
}

void PaintingCanvasComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isFileDragHover_ = false;
    if (files.isEmpty())
        return;

    juce::String errorMsg;
    const juce::File droppedFile(files[0]);
    if (processor_.loadPaintingFromFile(droppedFile, errorMsg))
    {
        if (onStatusMessage)
            onStatusMessage("Loaded painting: " + droppedFile.getFileName(), false);
    }
    else
    {
        if (onStatusMessage)
            onStatusMessage(errorMsg, true);
    }
    repaint();
}

} // namespace soundcanvas
