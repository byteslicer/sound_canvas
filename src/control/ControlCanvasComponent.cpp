#include "control/ControlCanvasComponent.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace soundcanvas
{

namespace
{
juce::String midiNoteToName(int midiNote)
{
    static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = std::clamp(midiNote, 0, 127);
    const int octave = (n / 12) - 1;
    return juce::String(kNames[n % 12]) + juce::String(octave);
}

float pointToSegmentDistSq(juce::Point<float> p, juce::Point<float> a, juce::Point<float> b) noexcept
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float lenSq = dx * dx + dy * dy;
    if (lenSq <= 1.0e-7f)
        return p.getDistanceSquaredFrom(a);
    const float t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq, 0.0f, 1.0f);
    const juce::Point<float> proj { a.x + t * dx, a.y + t * dy };
    return p.getDistanceSquaredFrom(proj);
}
} // namespace

ControlCanvasComponent::ControlCanvasComponent(SoundCanvasControlAudioProcessor& processor)
    : processor_(processor)
{
    auto setupBtn = [this](juce::TextButton& b, std::function<void()> fn)
    {
        addAndMakeVisible(b);
        b.onClick = std::move(fn);
    };

    setupBtn(lensPaintingBtn_, [this]() { setLensMode(LensMode::Painting); });
    setupBtn(lensEdgesBtn_,    [this]() { setLensMode(LensMode::Edges); });
    setupBtn(scaleGridBtn_, [this]()
    {
        showScaleGrid_ = !showScaleGrid_;
        updateHudButtonStyles();
        repaint();
    });

    updateHudButtonStyles();
    startTimerHz(30);
}

ControlCanvasComponent::~ControlCanvasComponent()
{
    stopTimer();
}

void ControlCanvasComponent::setSelectedPath(int pathIndex) noexcept
{
    const int clamped = std::clamp(pathIndex, 0, RackDocument::kMaxPaths - 1);
    if (selectedPath_ != clamped)
    {
        selectedPath_ = clamped;
        updateHudButtonStyles();
        repaint();
    }
}

void ControlCanvasComponent::setLensMode(LensMode mode)
{
    lensMode_ = mode;
    updateHudButtonStyles();
    repaint();
}

void ControlCanvasComponent::updateHudButtonStyles()
{
    const juce::Colour accent = ControlPalette::orbiterAccent(selectedPath_);
    const juce::Colour inactiveBg(0xd410141f);
    const juce::Colour activeBg = accent.withAlpha(0.28f);

    auto styleBtn = [&](juce::TextButton& b, bool active)
    {
        b.setToggleState(active, juce::dontSendNotification);
        b.setColour(juce::TextButton::buttonColourId, active ? activeBg : inactiveBg);
        b.setColour(juce::TextButton::buttonOnColourId, activeBg);
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcbd5e1));
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };

    styleBtn(lensPaintingBtn_, lensMode_ == LensMode::Painting);
    styleBtn(lensEdgesBtn_,    lensMode_ == LensMode::Edges);
    styleBtn(scaleGridBtn_,    showScaleGrid_);
}

void ControlCanvasComponent::timerCallback()
{
    repaint();
}

juce::Rectangle<float> ControlCanvasComponent::getCanvasFrameBounds() const noexcept
{
    return getLocalBounds().toFloat().reduced(5.0f);
}

juce::Rectangle<float> ControlCanvasComponent::getCanvasPlotBounds() const noexcept
{
    const auto frame = getCanvasFrameBounds().reduced(6.0f, 6.0f);
    if (frame.getWidth() <= 2.0f || frame.getHeight() <= 2.0f)
        return frame;

    auto grid = processor_.getCurrentGrid();
    if (!grid || !grid->getDisplayImage().isValid())
        return frame;

    const float imgW = static_cast<float>(std::max(1, grid->getDisplayImage().getWidth()));
    const float imgH = static_cast<float>(std::max(1, grid->getDisplayImage().getHeight()));
    const float imgAspect = imgW / imgH;
    const float frameAspect = frame.getWidth() / frame.getHeight();

    float targetW = frame.getWidth();
    float targetH = frame.getHeight();
    if (frameAspect > imgAspect)
        targetW = targetH * imgAspect;
    else
        targetH = targetW / imgAspect;

    return juce::Rectangle<float>(
        frame.getCentreX() - targetW * 0.5f,
        frame.getCentreY() - targetH * 0.5f,
        targetW,
        targetH);
}

juce::Point<float> ControlCanvasComponent::screenToNorm(juce::Point<float> pt) const noexcept
{
    const auto b = getCanvasPlotBounds();
    if (b.getWidth() <= 1.0f || b.getHeight() <= 1.0f)
        return { 0.5f, 0.5f };

    return {
        std::clamp((pt.x - b.getX()) / b.getWidth(), 0.015f, 0.985f),
        std::clamp((pt.y - b.getY()) / b.getHeight(), 0.015f, 0.985f)
    };
}

juce::Point<float> ControlCanvasComponent::normToScreen(juce::Point<float> normPt) const noexcept
{
    const auto b = getCanvasPlotBounds();
    return {
        b.getX() + normPt.x * b.getWidth(),
        b.getY() + normPt.y * b.getHeight()
    };
}

void ControlCanvasComponent::resized()
{
    const auto frame = getCanvasFrameBounds().toNearestInt();
    const int topY = frame.getY() + 8;
    const int leftX = frame.getX() + 10;

    lensPaintingBtn_.setBounds(leftX, topY, 66, 22);
    lensEdgesBtn_.setBounds(leftX + 70, topY, 54, 22);
    scaleGridBtn_.setBounds(frame.getRight() - 82, topY, 72, 22);
}

void ControlCanvasComponent::ensureEdgesImageCached(const SynestheticGrid& grid)
{
    if (lensMode_ != LensMode::Edges)
        return;

    if (cachedEdgesImage_.isValid() && cachedGridPtr_ == &grid)
        return;

    const int gw = grid.getWidth();
    const int gh = grid.getHeight();
    const auto& cells = grid.getCells();
    if (gw <= 0 || gh <= 0 || static_cast<int>(cells.size()) < gw * gh)
        return;

    cachedEdgesImage_ = juce::Image(juce::Image::RGB, gw, gh, false);
    juce::Image::BitmapData bmp(cachedEdgesImage_, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < gh; ++y)
    {
        for (int x = 0; x < gw; ++x)
        {
            const auto& px = cells[static_cast<size_t>(y * gw + x)];
            const float e = std::pow(std::clamp(px.edge, 0.0f, 1.0f), 0.78f);
            const float r = std::clamp(px.r * 0.22f + e * (0.18f + 0.82f * e), 0.0f, 1.0f);
            const float g = std::clamp(px.g * 0.22f + e * 0.92f, 0.0f, 1.0f);
            const float b = std::clamp(px.b * 0.24f + e * (1.0f - 0.32f * e), 0.0f, 1.0f);
            bmp.setPixelColour(x, y, juce::Colour::fromFloatRGBA(r, g, b, 1.0f));
        }
    }

    cachedGridPtr_ = &grid;
}

ControlCanvasComponent::WaypointHit ControlCanvasComponent::hitTestWaypoint(
    juce::Point<float> screenPos, const RackDocument& doc) const noexcept
{
    WaypointHit best {};
    float bestDist = 17.0f;

    // Check selected path first so its waypoints win ties
    int order[RackDocument::kMaxPaths] = { 0, 1, 2, 3 };
    std::stable_sort(std::begin(order), std::end(order), [this](int a, int b)
    {
        return (a == selectedPath_) > (b == selectedPath_);
    });

    for (int p : order)
    {
        const auto& path = doc.paths[static_cast<size_t>(p)];
        if (!path.active)
            continue;

        const int n = std::clamp(path.numPoints, 0, PickupPathConfig::kMaxWaypoints);
        for (int w = 0; w < n; ++w)
        {
            const auto pt = normToScreen(path.points[static_cast<size_t>(w)]);
            const float d = screenPos.getDistanceFrom(pt);
            if (d < bestDist)
            {
                bestDist = d;
                best = { p, w };
            }
        }
    }

    return best;
}

int ControlCanvasComponent::findBestInsertIndex(const PickupPathConfig& path, juce::Point<float> normPt) const noexcept
{
    const int n = std::clamp(path.numPoints, 0, PickupPathConfig::kMaxWaypoints);
    if (n < 2)
        return n;

    const bool closed = path.closedLoop && (n >= 3);
    const int numSegs = closed ? n : (n - 1);

    int bestSeg = n - 1;
    float bestDistSq = std::numeric_limits<float>::max();

    for (int s = 0; s < numSegs; ++s)
    {
        const auto a = path.points[static_cast<size_t>(s)];
        const auto b = path.points[static_cast<size_t>((s + 1) % n)];
        const float d2 = pointToSegmentDistSq(normPt, a, b);
        if (d2 < bestDistSq)
        {
            bestDistSq = d2;
            bestSeg = s;
        }
    }

    // If clicked reasonably close to an existing segment, splice into that segment; otherwise append to end
    if (bestDistSq < 0.014f || closed)
        return (bestSeg + 1) % (n + 1);
    return n;
}

void ControlCanvasComponent::paint(juce::Graphics& g)
{
    const auto fullBounds = getLocalBounds().toFloat();
    const auto frame = getCanvasFrameBounds();
    const auto plot = getCanvasPlotBounds();

    // Gallery backdrop & subtle recessed mat around the aspect-preserved artwork
    g.setColour(juce::Colour(0xff0b0d14));
    g.fillRoundedRectangle(fullBounds, 10.0f);
    g.setColour(juce::Colour(ControlPalette::kBorderSubtle));
    g.drawRoundedRectangle(fullBounds.reduced(0.6f), 10.0f, 1.1f);

    auto grid = processor_.getCurrentGrid();
    if (!grid || !grid->getDisplayImage().isValid())
        return;

    const auto doc = processor_.getDocumentCopy();
    const auto snap = processor_.getEngine().getVisualSnapshot();

    ensureEdgesImageCached(*grid);

    // Subtle elevation shadow behind the aspect-preserved painting
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(plot.expanded(2.0f), 8.0f);

    g.saveState();
    juce::Path clipPath;
    clipPath.addRoundedRectangle(plot, 6.0f);
    g.reduceClipRegion(clipPath);

    // 1. Painting / Edges backdrop (drawn strictly at original aspect ratio!)
    if (lensMode_ == LensMode::Painting || !cachedEdgesImage_.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(grid->getDisplayImage(), plot, juce::RectanglePlacement::centred);
        g.setColour(juce::Colours::black.withAlpha(0.14f));
        g.fillRect(plot);
    }
    else
    {
        g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        g.drawImage(cachedEdgesImage_, plot, juce::RectanglePlacement::centred);
    }

    // 2. Subtle horizontal Scale Guide Lines
    if (showScaleGrid_)
    {
        auto& apvts = processor_.getApvts();
        const int rootNote = static_cast<int>(apvts.getRawParameterValue("rootNote")->load());
        const auto rootNames = ScaleQuantizer::getRootNoteNames();
        const juce::String rootStr = rootNames[std::clamp(rootNote, 0, 11)];

        // Use selected path's first NoteGenerator octave/span if present, else Oct 3 / Span 2
        int baseOct = 3;
        int span = 2;
        const auto& selPath = doc.paths[static_cast<size_t>(selectedPath_)];
        for (int m = 0; m < std::clamp(selPath.numModules, 0, PickupPathConfig::kMaxModules); ++m)
        {
            if (selPath.modules[static_cast<size_t>(m)].type == ModuleType::NoteGenerator)
            {
                baseOct = std::clamp(selPath.modules[static_cast<size_t>(m)].intC, 1, 6);
                span = std::clamp(selPath.modules[static_cast<size_t>(m)].intD, 1, 4);
                break;
            }
        }

        for (int o = 0; o <= span; ++o)
        {
            const float normFromBottom = static_cast<float>(o) / static_cast<float>(span);
            const float lineY = plot.getBottom() - normFromBottom * plot.getHeight();
            if (o > 0 && o < span)
            {
                g.setColour(juce::Colours::white.withAlpha(0.14f));
                g.drawLine(plot.getX(), lineY, plot.getRight(), lineY, 1.0f);
            }
            if (o < span)
            {
                const float labelY = std::clamp(lineY - 15.0f, plot.getY() + 36.0f, plot.getBottom() - 36.0f);
                g.setColour(juce::Colours::white.withAlpha(0.45f));
                g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
                g.drawText(rootStr + juce::String(baseOct + o),
                           juce::Rectangle<float>(plot.getX() + 10.0f, labelY, 34.0f, 13.0f),
                           juce::Justification::centredLeft, false);
            }
        }
    }

    // 3. Draw active Pickup Paths (unselected first, selected on top)
    int drawOrder[RackDocument::kMaxPaths] = { 0, 1, 2, 3 };
    std::stable_sort(std::begin(drawOrder), std::end(drawOrder), [this](int a, int b)
    {
        return (a == selectedPath_) < (b == selectedPath_);
    });

    for (int p : drawOrder)
    {
        const size_t pu = static_cast<size_t>(p);
        const auto& path = doc.paths[pu];
        if (!path.active)
            continue;

        const auto& vis = snap.paths[pu];
        const bool isSelected = (p == selectedPath_);
        const juce::Colour accent = ControlPalette::orbiterAccent(p);
        const float alpha = path.muted ? 0.30f : (isSelected ? 1.0f : 0.52f);

        // Smooth deterministic ribbon curve
        constexpr int kCurveSamples = 96;
        juce::Path curvePath;
        for (int s = 0; s <= kCurveSamples; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(kCurveSamples);
            const auto sp = normToScreen(ImageMidiControlEngine::evaluatePathPoint(path, t));
            if (s == 0)
                curvePath.startNewSubPath(sp);
            else
                curvePath.lineTo(sp);
        }

        // Soft outer glow + crisp vector line
        g.setColour(accent.withAlpha(0.20f * alpha));
        g.strokePath(curvePath, juce::PathStrokeType(isSelected ? 7.5f : 4.5f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        g.setColour(accent.withAlpha(0.92f * alpha));
        g.strokePath(curvePath, juce::PathStrokeType(isSelected ? 2.4f : 1.6f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

        // Subtle direction tick marks at 25%, 50%, 75% along curve
        if (isSelected)
        {
            constexpr float kTicks[3] = { 0.25f, 0.50f, 0.75f };
            for (float tk : kTicks)
            {
                const auto pA = normToScreen(ImageMidiControlEngine::evaluatePathPoint(path, std::max(0.0f, tk - 0.012f)));
                const auto pB = normToScreen(ImageMidiControlEngine::evaluatePathPoint(path, std::min(1.0f, tk + 0.012f)));
                const float ang = std::atan2(pB.y - pA.y, pB.x - pA.x) + (path.playMode == 2 ? juce::MathConstants<float>::pi : 0.0f);
                const auto mid = (pA + pB) * 0.5f;
                const float cs = std::cos(ang), sn = std::sin(ang);

                juce::Path chev;
                chev.startNewSubPath(mid.x - cs * 4.0f - sn * 3.5f, mid.y - sn * 4.0f + cs * 3.5f);
                chev.lineTo(mid.x + cs * 4.0f, mid.y + sn * 4.0f);
                chev.lineTo(mid.x - cs * 4.0f + sn * 3.5f, mid.y - sn * 4.0f - cs * 3.5f);
                g.setColour(juce::Colours::white.withAlpha(0.65f * alpha));
                g.strokePath(chev, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // Vector Waypoints P1..Pn
        const int n = std::clamp(path.numPoints, 0, PickupPathConfig::kMaxWaypoints);
        for (int w = 0; w < n; ++w)
        {
            const auto wp = normToScreen(path.points[static_cast<size_t>(w)]);
            const bool isHov = (hoveredWp_.pathIdx == p && hoveredWp_.wpIdx == w);
            const bool isDrag = (draggingWp_.pathIdx == p && draggingWp_.wpIdx == w);
            const float r = isSelected ? (isHov || isDrag ? 10.0f : 8.5f) : 5.5f;

            if (isSelected)
            {
                g.setColour(juce::Colour(0xff0b0e16).withAlpha(0.92f * alpha));
                g.fillEllipse(wp.x - r, wp.y - r, r * 2.0f, r * 2.0f);
                g.setColour((isHov || isDrag) ? juce::Colours::white : accent.withAlpha(0.95f * alpha));
                g.drawEllipse(wp.x - r, wp.y - r, r * 2.0f, r * 2.0f, (isHov || isDrag) ? 2.0f : 1.5f);

                g.setColour(juce::Colours::white.withAlpha(0.95f * alpha));
                g.setFont(juce::FontOptions(9.2f, juce::Font::bold));
                g.drawText(juce::String(w + 1),
                           juce::Rectangle<float>(wp.x - r, wp.y - r, r * 2.0f, r * 2.0f),
                           juce::Justification::centred, false);
            }
            else
            {
                g.setColour(accent.withAlpha(0.75f * alpha));
                g.fillEllipse(wp.x - r, wp.y - r, r * 2.0f, r * 2.0f);
            }
        }

        // Glowing Deterministic Playhead Puck
        const auto ph = normToScreen({ vis.playheadX, vis.playheadY });

        if (!path.muted && vis.triggerPulse > 0.04f)
        {
            const float ripR = 12.0f + (1.0f - vis.triggerPulse) * 28.0f;
            g.setColour(accent.withAlpha(vis.triggerPulse * 0.80f));
            g.drawEllipse(ph.x - ripR, ph.y - ripR, ripR * 2.0f, ripR * 2.0f, 2.0f);
        }

        // Fine precision crosshair around active pickup
        if (isSelected)
        {
            g.setColour(juce::Colours::white.withAlpha(0.45f * alpha));
            g.drawLine(ph.x - 18.0f, ph.y, ph.x - 11.0f, ph.y, 1.2f);
            g.drawLine(ph.x + 11.0f, ph.y, ph.x + 18.0f, ph.y, 1.2f);
            g.drawLine(ph.x, ph.y - 18.0f, ph.x, ph.y - 11.0f, 1.2f);
            g.drawLine(ph.x, ph.y + 11.0f, ph.x, ph.y + 18.0f, 1.2f);
        }

        const float phR = isSelected ? 9.5f : 7.5f;
        const juce::Colour sampled = juce::Colour::fromFloatRGBA(
            std::clamp(vis.r, 0.0f, 1.0f),
            std::clamp(vis.g, 0.0f, 1.0f),
            std::clamp(vis.b, 0.0f, 1.0f),
            1.0f);

        g.setColour(accent.withAlpha(0.32f * alpha));
        g.fillEllipse(ph.x - phR - 4.0f, ph.y - phR - 4.0f, (phR + 4.0f) * 2.0f, (phR + 4.0f) * 2.0f);
        g.setColour(sampled.withAlpha(alpha));
        g.fillEllipse(ph.x - phR, ph.y - phR, phR * 2.0f, phR * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.95f * alpha));
        g.drawEllipse(ph.x - phR, ph.y - phR, phR * 2.0f, phR * 2.0f, 2.0f);

        // Compact pill: Path# : Note
        const juce::String pillTxt = "P" + juce::String(p + 1) + ":" + midiNoteToName(vis.lastMidiNote);
        const float px = (ph.x + 62.0f < plot.getRight()) ? (ph.x + 14.0f) : (ph.x - 58.0f);
        const juce::Rectangle<float> badge(px, ph.y - 9.0f, 46.0f, 18.0f);
        g.setColour(juce::Colour(0xff0b0e16).withAlpha(0.84f * alpha));
        g.fillRoundedRectangle(badge, 9.0f);
        g.setColour(accent.withAlpha(0.90f * alpha));
        g.drawRoundedRectangle(badge, 9.0f, 1.1f);
        g.setColour(juce::Colours::white.withAlpha(0.96f * alpha));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(pillTxt, badge, juce::Justification::centred, false);
    }

    g.restoreState();

    // Crisp gallery border around the aspect-preserved painting itself
    g.setColour(juce::Colour(ControlPalette::kBorderBright).withAlpha(0.65f));
    g.drawRoundedRectangle(plot, 6.0f, 1.1f);

    // 4. Crisp bottom-left caption pill anchored to the outer frame so portrait/narrow art stays clean
    {
        const juce::String caption = grid->getTitle()
            + "   |   Click painting to add point  -  Drag to sculpt  -  Right-click to remove";
        const juce::Font capFont(juce::FontOptions(10.0f));
        const float textW = juce::GlyphArrangement::getStringWidth(capFont, caption);
        const float pillW = std::min(frame.getWidth() - 20.0f, textW + 22.0f);
        const juce::Rectangle<float> capRect(frame.getX() + 10.0f, frame.getBottom() - 24.0f, pillW, 18.0f);

        g.setColour(juce::Colour(0xff0b0d14).withAlpha(0.80f));
        g.fillRoundedRectangle(capRect, 9.0f);
        g.setColour(juce::Colour(ControlPalette::kBorderSubtle).withAlpha(0.8f));
        g.drawRoundedRectangle(capRect, 9.0f, 0.9f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.setFont(capFont);
        g.drawFittedText(caption, capRect.reduced(9.0f, 0.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
    }

    if (isFileDragHover_)
    {
        g.setColour(juce::Colour(0xcc090b10));
        g.fillRoundedRectangle(frame, 7.0f);
        g.setColour(ControlPalette::orbiterAccent(selectedPath_));
        g.drawRoundedRectangle(frame.reduced(6.0f), 7.0f, 2.2f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        g.drawText("Drop Painting (.PNG / .JPG) to Scan", frame, juce::Justification::centred, false);
    }
}

void ControlCanvasComponent::mouseMove(const juce::MouseEvent& event)
{
    const auto doc = processor_.getDocumentCopy();
    const auto hit = hitTestWaypoint(event.position, doc);
    if (hit.pathIdx != hoveredWp_.pathIdx || hit.wpIdx != hoveredWp_.wpIdx)
    {
        hoveredWp_ = hit;
        setMouseCursor(hit.wpIdx >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::CrosshairCursor);
        repaint();
    }
}

void ControlCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    if (!getCanvasPlotBounds().contains(event.position))
        return;

    const auto doc = processor_.getDocumentCopy();
    const auto hit = hitTestWaypoint(event.position, doc);

    if (event.mods.isRightButtonDown())
    {
        if (hit.pathIdx >= 0 && hit.wpIdx >= 0)
        {
            processor_.updateDocument([&](RackDocument& d)
            {
                auto& path = d.paths[static_cast<size_t>(hit.pathIdx)];
                if (path.numPoints > 2 && hit.wpIdx < path.numPoints)
                {
                    for (int i = hit.wpIdx; i < path.numPoints - 1; ++i)
                        path.points[static_cast<size_t>(i)] = path.points[static_cast<size_t>(i + 1)];
                    path.numPoints--;
                }
            });
            if (onStatusMessage)
                onStatusMessage("Removed waypoint from Path " + juce::String(hit.pathIdx + 1) + ".", false);
            repaint();
        }
        return;
    }

    if (hit.pathIdx >= 0 && hit.wpIdx >= 0)
    {
        if (hit.pathIdx != selectedPath_)
        {
            selectedPath_ = hit.pathIdx;
            updateHudButtonStyles();
            if (onPathSelected)
                onPathSelected(hit.pathIdx);
        }
        draggingWp_ = hit;
        return;
    }

    // Left-click on empty canvas: add a new waypoint to the currently selected path!
    const auto normPt = screenToNorm(event.position);
    int insertedIdx = -1;
    processor_.updateDocument([&](RackDocument& d)
    {
        auto& path = d.paths[static_cast<size_t>(selectedPath_)];
        if (!path.active)
            path.active = true;
        if (path.numPoints >= PickupPathConfig::kMaxWaypoints)
            return;

        insertedIdx = findBestInsertIndex(path, normPt);
        insertedIdx = std::clamp(insertedIdx, 0, path.numPoints);
        for (int i = path.numPoints; i > insertedIdx; --i)
            path.points[static_cast<size_t>(i)] = path.points[static_cast<size_t>(i - 1)];
        path.points[static_cast<size_t>(insertedIdx)] = normPt;
        path.numPoints++;
    });

    if (insertedIdx >= 0)
    {
        draggingWp_ = { selectedPath_, insertedIdx };
        if (onStatusMessage)
            onStatusMessage("Added waypoint P" + juce::String(insertedIdx + 1) + " to Path " + juce::String(selectedPath_ + 1) + ".", false);
    }
    repaint();
}

void ControlCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggingWp_.pathIdx < 0 || draggingWp_.wpIdx < 0)
        return;

    const auto normPt = screenToNorm(event.position);
    const int pIdx = draggingWp_.pathIdx;
    const int wIdx = draggingWp_.wpIdx;

    processor_.updateDocument([&](RackDocument& d)
    {
        auto& path = d.paths[static_cast<size_t>(pIdx)];
        if (wIdx >= 0 && wIdx < path.numPoints)
            path.points[static_cast<size_t>(wIdx)] = normPt;
    });
    repaint();
}

void ControlCanvasComponent::mouseUp(const juce::MouseEvent&)
{
    draggingWp_ = {};
}

bool ControlCanvasComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const juce::String ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            return true;
    }
    return false;
}

void ControlCanvasComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isFileDragHover_ = true;
    repaint();
}

void ControlCanvasComponent::fileDragExit(const juce::StringArray&)
{
    isFileDragHover_ = false;
    repaint();
}

void ControlCanvasComponent::filesDropped(const juce::StringArray& files, int, int)
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
