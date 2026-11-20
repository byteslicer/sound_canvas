#include "synth/SynthCanvasComponent.h"
#include <algorithm>
#include <cmath>

namespace soundcanvas
{

SynthCanvasComponent::SynthCanvasComponent(SoundCanvasSynthAudioProcessor& processor)
    : processor_(processor)
{
    startTimerHz(30);
}

SynthCanvasComponent::~SynthCanvasComponent()
{
    stopTimer();
}

void SynthCanvasComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<float> SynthCanvasComponent::getCanvasPlotBounds() const noexcept
{
    return getLocalBounds().toFloat().reduced(8.0f);
}

juce::Point<float> SynthCanvasComponent::screenToNorm(juce::Point<float> pt) const noexcept
{
    const auto b = getCanvasPlotBounds();
    if (b.getWidth() <= 1.0f || b.getHeight() <= 1.0f)
        return { 0.5f, 0.5f };

    const float nx = std::clamp((pt.x - b.getX()) / b.getWidth(), 0.0f, 1.0f);
    const float ny = std::clamp((pt.y - b.getY()) / b.getHeight(), 0.0f, 1.0f);
    return { nx, ny };
}

juce::Point<float> SynthCanvasComponent::normToScreen(juce::Point<float> normPt) const noexcept
{
    const auto b = getCanvasPlotBounds();
    return {
        b.getX() + normPt.x * b.getWidth(),
        b.getY() + normPt.y * b.getHeight()
    };
}

void SynthCanvasComponent::drawSliceRibbon(juce::Graphics& g,
                                           juce::Point<float> normA,
                                           juce::Point<float> normB,
                                           const std::shared_ptr<const WavetableData>& wt,
                                           float morphPos,
                                           juce::Colour accent,
                                           const char* labelA,
                                           const char* labelB) const
{
    const auto ptA = normToScreen(normA);
    const auto ptB = normToScreen(normB);

    const float dx = ptB.x - ptA.x;
    const float dy = ptB.y - ptA.y;
    const float len = std::max(1.0f, std::hypot(dx, dy));
    const float nx = -dy / len;
    const float ny = dx / len;

    constexpr float ribPx = 20.0f;
    juce::Path ribbonPath;
    ribbonPath.startNewSubPath(ptA.x - nx * ribPx, ptA.y - ny * ribPx);
    ribbonPath.lineTo(ptB.x - nx * ribPx, ptB.y - ny * ribPx);
    ribbonPath.lineTo(ptB.x + nx * ribPx, ptB.y + ny * ribPx);
    ribbonPath.lineTo(ptA.x + nx * ribPx, ptA.y + ny * ribPx);
    ribbonPath.closeSubPath();

    g.setColour(accent.withAlpha(0.18f));
    g.fillPath(ribbonPath);
    g.setColour(accent.withAlpha(0.58f));
    g.strokePath(ribbonPath, juce::PathStrokeType(1.2f));

    if (wt)
    {
        const int frameIdx = std::clamp(
            static_cast<int>(morphPos * static_cast<float>(WavetableData::kNumFrames - 1)),
            0, WavetableData::kNumFrames - 1);
        const float morphOffset = (morphPos - 0.5f) * 2.0f * ribPx;

        juce::Path wavePath;
        constexpr int kStep = 2;
        for (int s = 0; s < WavetableData::kSamplesPerFrame; s += kStep)
        {
            const float t = static_cast<float>(s) / static_cast<float>(WavetableData::kSamplesPerFrame - 1);
            const float amp = wt->frames[static_cast<size_t>(frameIdx)][static_cast<size_t>(s)] * 16.0f;
            const float wx = ptA.x + t * dx + nx * (morphOffset + amp);
            const float wy = ptA.y + t * dy + ny * (morphOffset + amp);
            if (s == 0)
                wavePath.startNewSubPath(wx, wy);
            else
                wavePath.lineTo(wx, wy);
        }

        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.strokePath(wavePath, juce::PathStrokeType(2.0f));
    }

    auto drawHandle = [&](juce::Point<float> p, const char* label)
    {
        constexpr float r = 9.0f;
        g.setColour(accent);
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.5f);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(label,
                   juce::Rectangle<float>(p.x - r, p.y - r, r * 2.0f, r * 2.0f),
                   juce::Justification::centred,
                   false);
    };

    drawHandle(ptA, labelA);
    drawHandle(ptB, labelB);
}

void SynthCanvasComponent::paint(juce::Graphics& g)
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

        // Subtle dark vignette for crisp overlay legibility
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillRect(plot);

        const auto& engine = processor_.getSynthEngine();

        // 1. 2D Texture Cloud (Violet)
        const auto cloudNorm = engine.getTextureCloudCenter();
        const auto cloudScreen = normToScreen(cloudNorm);
        const float cloudRadNorm = processor_.getApvts().getRawParameterValue("textureRadius")->load();
        const float rx = cloudRadNorm * plot.getWidth();
        const float ry = cloudRadNorm * plot.getHeight();
        const juce::Colour cloudCol(0xffc084fc);

        g.setColour(cloudCol.withAlpha(0.14f));
        g.fillEllipse(cloudScreen.x - rx, cloudScreen.y - ry, rx * 2.0f, ry * 2.0f);
        g.setColour(cloudCol.withAlpha(0.75f));
        g.drawEllipse(cloudScreen.x - rx, cloudScreen.y - ry, rx * 2.0f, ry * 2.0f, 1.6f);

        // Inner half-radius ring guide
        g.setColour(cloudCol.withAlpha(0.32f));
        g.drawEllipse(cloudScreen.x - rx * 0.45f, cloudScreen.y - ry * 0.45f, rx * 0.90f, ry * 0.90f, 1.0f);

        // 16 radial harmonic dots matching SynestheticGrid::sampleRegionStats ring geometry
        const auto stats = engine.getLastCloudStats();
        int profIdx = 0;
        for (int ring = 0; ring < 2; ++ring)
        {
            const float rScale = (ring == 0) ? (0.45f * cloudRadNorm) : (0.95f * cloudRadNorm);
            for (int a = 0; a < 8; ++a)
            {
                const float angle = (static_cast<float>(a) / 8.0f) * juce::MathConstants<float>::twoPi
                                  + (ring == 1 ? juce::MathConstants<float>::pi / 8.0f : 0.0f);
                const float sx = std::clamp(cloudNorm.x + rScale * std::cos(angle), 0.0f, 1.0f);
                const float sy = std::clamp(cloudNorm.y + rScale * std::sin(angle), 0.0f, 1.0f);
                const auto dotPos = normToScreen({ sx, sy });

                const float weight = std::clamp(stats.harmonicProfile[static_cast<size_t>(profIdx)], 0.0f, 1.0f);
                const float dotR = 2.2f + weight * 5.2f;
                g.setColour(cloudCol.withAlpha(0.35f + weight * 0.60f));
                g.fillEllipse(dotPos.x - dotR, dotPos.y - dotR, dotR * 2.0f, dotR * 2.0f);
                g.setColour(juce::Colours::white.withAlpha(0.65f));
                g.drawEllipse(dotPos.x - dotR, dotPos.y - dotR, dotR * 2.0f, dotR * 2.0f, 0.9f);
                ++profIdx;
            }
        }

        // Cloud center handle 'C'
        {
            constexpr float cr = 9.5f;
            g.setColour(cloudCol);
            g.fillEllipse(cloudScreen.x - cr, cloudScreen.y - cr, cr * 2.0f, cr * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.drawEllipse(cloudScreen.x - cr, cloudScreen.y - cr, cr * 2.0f, cr * 2.0f, 1.5f);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText("C",
                       juce::Rectangle<float>(cloudScreen.x - cr, cloudScreen.y - cr, cr * 2.0f, cr * 2.0f),
                       juce::Justification::centred,
                       false);
        }

        // 2. Slice B Ribbon (Amber 0xffffb300)
        drawSliceRibbon(g,
                        engine.getSlicePointA2(),
                        engine.getSlicePointB2(),
                        engine.getWavetableB(),
                        engine.getActiveMorphB(),
                        juce::Colour(0xffffb300),
                        "A2",
                        "B2");

        // 3. Slice A Ribbon (Cyan 0xff00e5ff)
        drawSliceRibbon(g,
                        engine.getSlicePointA1(),
                        engine.getSlicePointB1(),
                        engine.getWavetableA(),
                        engine.getActiveMorphA(),
                        juce::Colour(0xff00e5ff),
                        "A1",
                        "B1");

        // Bottom canvas badge
        const juce::String badgeText = "Canvas: " + grid->getTitle()
            + "  |  Voices: " + juce::String(engine.getActiveVoiceCount())
            + "  |  Drag A1/B1, A2/B2, C or Ring";
        g.setColour(juce::Colours::black.withAlpha(0.66f));
        g.fillRoundedRectangle(plot.getX() + 10.0f, plot.getBottom() - 28.0f, 450.0f, 22.0f, 5.0f);
        g.setColour(juce::Colour(0xffd6deeb));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(badgeText,
                   juce::Rectangle<float>(plot.getX() + 16.0f, plot.getBottom() - 28.0f, 438.0f, 22.0f),
                   juce::Justification::centredLeft,
                   false);

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

void SynthCanvasComponent::handlePointerInteraction(juce::Point<float> pos, bool isInitialClick)
{
    const auto plot = getCanvasPlotBounds();
    const auto normPt = screenToNorm(pos);
    const auto& engine = processor_.getSynthEngine();

    if (isInitialClick)
    {
        activeDrag_ = DragTarget::None;
        float bestDist = 20.0f;

        auto testPoint = [&](juce::Point<float> normHandle, DragTarget target)
        {
            const float d = pos.getDistanceFrom(normToScreen(normHandle));
            if (d < bestDist)
            {
                bestDist = d;
                activeDrag_ = target;
            }
        };

        testPoint(engine.getSlicePointA1(), DragTarget::SliceA1);
        testPoint(engine.getSlicePointB1(), DragTarget::SliceB1);
        testPoint(engine.getSlicePointA2(), DragTarget::SliceA2);
        testPoint(engine.getSlicePointB2(), DragTarget::SliceB2);
        testPoint(engine.getTextureCloudCenter(), DragTarget::CloudCenter);

        if (activeDrag_ == DragTarget::None)
        {
            const auto cloudNorm = engine.getTextureCloudCenter();
            const auto cloudScreen = normToScreen(cloudNorm);
            const float cloudRadNorm = processor_.getApvts().getRawParameterValue("textureRadius")->load();
            const float rNorm = std::hypot((pos.x - cloudScreen.x) / std::max(1.0f, plot.getWidth()),
                                           (pos.y - cloudScreen.y) / std::max(1.0f, plot.getHeight()));
            const float edgeDistPx = std::abs(rNorm - cloudRadNorm) * std::min(plot.getWidth(), plot.getHeight());
            if (edgeDistPx < 14.0f)
            {
                activeDrag_ = DragTarget::CloudRadius;
                if (auto* param = processor_.getApvts().getParameter("textureRadius"))
                {
                    param->beginChangeGesture();
                    radiusGestureActive_ = true;
                }
            }
            else if (rNorm < cloudRadNorm)
            {
                activeDrag_ = DragTarget::CloudCenter;
            }
        }
    }

    switch (activeDrag_)
    {
        case DragTarget::SliceA1:
            processor_.updateWavetableSliceA(normPt, engine.getSlicePointB1());
            break;
        case DragTarget::SliceB1:
            processor_.updateWavetableSliceA(engine.getSlicePointA1(), normPt);
            break;
        case DragTarget::SliceA2:
            processor_.updateWavetableSliceB(normPt, engine.getSlicePointB2());
            break;
        case DragTarget::SliceB2:
            processor_.updateWavetableSliceB(engine.getSlicePointA2(), normPt);
            break;
        case DragTarget::CloudCenter:
            processor_.updateTextureCloudCenter(normPt);
            break;
        case DragTarget::CloudRadius:
        {
            const auto cloudNorm = engine.getTextureCloudCenter();
            const float newRadius = std::clamp(std::hypot(normPt.x - cloudNorm.x, normPt.y - cloudNorm.y), 0.04f, 0.45f);
            if (auto* param = processor_.getApvts().getParameter("textureRadius"))
                param->setValueNotifyingHost(param->convertTo0to1(newRadius));
            processor_.getSynthEngine().setTextureCloudRadius(newRadius);
            break;
        }
        case DragTarget::None:
            break;
    }

    repaint();
}

void SynthCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    handlePointerInteraction(event.position, true);
}

void SynthCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    handlePointerInteraction(event.position, false);
}

void SynthCanvasComponent::mouseUp(const juce::MouseEvent&)
{
    if (radiusGestureActive_)
    {
        if (auto* param = processor_.getApvts().getParameter("textureRadius"))
            param->endChangeGesture();
        radiusGestureActive_ = false;
    }
    activeDrag_ = DragTarget::None;
}

bool SynthCanvasComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const juce::String ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            return true;
    }
    return false;
}

void SynthCanvasComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isFileDragHover_ = true;
    repaint();
}

void SynthCanvasComponent::fileDragExit(const juce::StringArray&)
{
    isFileDragHover_ = false;
    repaint();
}

void SynthCanvasComponent::filesDropped(const juce::StringArray& files, int, int)
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
