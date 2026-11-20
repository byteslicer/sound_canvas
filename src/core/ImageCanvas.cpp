#include "core/ImageCanvas.h"
#include <cmath>
#include <algorithm>

namespace soundcanvas
{

SynestheticGrid::SynestheticGrid(int gridWidth, int gridHeight, juce::Image displayImg, juce::String title)
    : width_(std::max(2, gridWidth)),
      height_(std::max(2, gridHeight)),
      cells_(static_cast<size_t>(width_ * height_)),
      displayImage_(std::move(displayImg)),
      title_(std::move(title))
{
}

const PixelSample& SynestheticGrid::cellAt(int x, int y) const noexcept
{
    const int cx = std::clamp(x, 0, width_ - 1);
    const int cy = std::clamp(y, 0, height_ - 1);
    return cells_[static_cast<size_t>(cy * width_ + cx)];
}

void SynestheticGrid::computeSobelEdges() noexcept
{
    if (width_ < 3 || height_ < 3)
        return;

    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < width_; ++x)
        {
            const float tl = cellAt(x - 1, y - 1).luminance;
            const float tc = cellAt(x,     y - 1).luminance;
            const float tr = cellAt(x + 1, y - 1).luminance;
            const float ml = cellAt(x - 1, y).luminance;
            const float mr = cellAt(x + 1, y).luminance;
            const float bl = cellAt(x - 1, y + 1).luminance;
            const float bc = cellAt(x,     y + 1).luminance;
            const float br = cellAt(x + 1, y + 1).luminance;

            const float gx = (-tl - 2.0f * ml - bl) + (tr + 2.0f * mr + br);
            const float gy = (-tl - 2.0f * tc - tr) + (bl + 2.0f * bc + br);

            // Also include chroma edge contribution so color-only brushstroke transitions trigger
            const float hDiffX = std::abs(cellAt(x + 1, y).hue - cellAt(x - 1, y).hue);
            const float hDiffY = std::abs(cellAt(x, y + 1).hue - cellAt(x, y - 1).hue);
            const float chromaGrad = std::min(hDiffX, 1.0f - hDiffX) + std::min(hDiffY, 1.0f - hDiffY);

            const float mag = std::sqrt(gx * gx + gy * gy) * 0.5f + chromaGrad * 0.6f;
            cells_[static_cast<size_t>(y * width_ + x)].edge = std::clamp(mag, 0.0f, 1.0f);
        }
    }
}

PixelSample SynestheticGrid::sampleAt(float normX, float normY) const noexcept
{
    if (width_ <= 0 || height_ <= 0 || cells_.empty())
        return {};

    const float clampedX = std::clamp(normX, 0.0f, 1.0f);
    // Invert normY so normY = 0 is bottom of painting (low pitch) and normY = 1 is top (high pitch)
    const float imgY = 1.0f - std::clamp(normY, 0.0f, 1.0f);

    const float fx = clampedX * static_cast<float>(width_ - 1);
    const float fy = imgY * static_cast<float>(height_ - 1);

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = std::min(x0 + 1, width_ - 1);
    const int y1 = std::min(y0 + 1, height_ - 1);

    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const auto& c00 = cellAt(x0, y0);
    const auto& c10 = cellAt(x1, y0);
    const auto& c01 = cellAt(x0, y1);
    const auto& c11 = cellAt(x1, y1);

    auto bilerp = [tx, ty](float v00, float v10, float v01, float v11) noexcept
    {
        const float top = v00 + tx * (v10 - v00);
        const float bot = v01 + tx * (v11 - v01);
        return top + ty * (bot - top);
    };

    PixelSample out;
    out.r = bilerp(c00.r, c10.r, c01.r, c11.r);
    out.g = bilerp(c00.g, c10.g, c01.g, c11.g);
    out.b = bilerp(c00.b, c10.b, c01.b, c11.b);
    out.a = bilerp(c00.a, c10.a, c01.a, c11.a);
    out.luminance = bilerp(c00.luminance, c10.luminance, c01.luminance, c11.luminance);
    out.saturation = bilerp(c00.saturation, c10.saturation, c01.saturation, c11.saturation);
    out.edge = bilerp(c00.edge, c10.edge, c01.edge, c11.edge);

    float h = 0.0f, s = 0.0f, v = 0.0f;
    ImageCanvas::rgbToHsv(out.r, out.g, out.b, h, s, v);
    out.hue = h;
    return out;
}

RegionStats SynestheticGrid::sampleRegionStats(juce::Point<float> center, float radius) const noexcept
{
    RegionStats stats;
    if (width_ <= 0 || height_ <= 0 || cells_.empty())
        return stats;

    const float cx = std::clamp(center.x, 0.0f, 1.0f);
    const float cy = std::clamp(center.y, 0.0f, 1.0f);
    const float rad = std::clamp(radius, 0.01f, 0.5f);

    std::array<PixelSample, 17> samples {};
    samples[0] = sampleAt(cx, 1.0f - cy);

    int idx = 1;
    for (int ring = 0; ring < 2; ++ring)
    {
        const float rScale = (ring == 0) ? (0.45f * rad) : (0.95f * rad);
        for (int a = 0; a < 8; ++a)
        {
            const float angle = (static_cast<float>(a) / 8.0f) * juce::MathConstants<float>::twoPi
                              + (ring == 1 ? juce::MathConstants<float>::pi / 8.0f : 0.0f);
            const float sx = std::clamp(cx + rScale * std::cos(angle), 0.0f, 1.0f);
            const float sy = std::clamp(cy + rScale * std::sin(angle), 0.0f, 1.0f);
            const PixelSample px = sampleAt(sx, 1.0f - sy);
            samples[static_cast<size_t>(idx)] = px;

            const float hWeight = px.luminance * (0.6f + 0.4f * px.saturation) * (0.7f + 0.3f * px.edge);
            stats.harmonicProfile[static_cast<size_t>(idx - 1)] = std::clamp(hWeight, 0.0f, 1.0f);
            ++idx;
        }
    }

    constexpr float invCount = 1.0f / 17.0f;
    for (const auto& px : samples)
    {
        stats.mean.r += px.r;
        stats.mean.g += px.g;
        stats.mean.b += px.b;
        stats.mean.a += px.a;
        stats.mean.saturation += px.saturation;
        stats.mean.luminance += px.luminance;
        stats.mean.edge += px.edge;
    }

    stats.mean.r *= invCount;
    stats.mean.g *= invCount;
    stats.mean.b *= invCount;
    stats.mean.a *= invCount;
    stats.mean.saturation *= invCount;
    stats.mean.luminance *= invCount;
    stats.mean.edge *= invCount;

    float h = 0.0f, s = 0.0f, v = 0.0f;
    ImageCanvas::rgbToHsv(stats.mean.r, stats.mean.g, stats.mean.b, h, s, v);
    stats.mean.hue = h;

    float varSum = 0.0f;
    for (const auto& px : samples)
    {
        const float dLum = px.luminance - stats.mean.luminance;
        const float dR = px.r - stats.mean.r;
        const float dG = px.g - stats.mean.g;
        const float dB = px.b - stats.mean.b;
        varSum += dLum * dLum + 0.33f * (dR * dR + dG * dG + dB * dB);
    }
    stats.colorVariance = std::clamp(std::sqrt(varSum * invCount) * 2.0f, 0.0f, 1.0f);

    return stats;
}

WavetableData SynestheticGrid::extractWavetable(juce::Point<float> pointA,
                                                juce::Point<float> pointB,
                                                float ribbonWidth) const noexcept
{
    WavetableData wt;

    const float dx = pointB.x - pointA.x;
    const float dy = pointB.y - pointA.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float nx = (len > 1.0e-4f) ? (-dy / len) : 0.0f;
    const float ny = (len > 1.0e-4f) ? (dx / len) : 1.0f;

    for (int f = 0; f < WavetableData::kNumFrames; ++f)
    {
        const float frameNorm = (static_cast<float>(f) / static_cast<float>(WavetableData::kNumFrames - 1)) - 0.5f;
        const float offsetX = nx * frameNorm * ribbonWidth;
        const float offsetY = ny * frameNorm * ribbonWidth;

        float mean = 0.0f;
        PixelSample avgColor {};

        std::array<float, WavetableData::kSamplesPerFrame> raw {};

        for (int s = 0; s < WavetableData::kSamplesPerFrame; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(WavetableData::kSamplesPerFrame);
            const float sx = std::clamp(pointA.x + t * dx + offsetX, 0.0f, 1.0f);
            const float sy = std::clamp(pointA.y + t * dy + offsetY, 0.0f, 1.0f);

            const PixelSample px = sampleAt(sx, 1.0f - sy);
            avgColor.r += px.r;
            avgColor.g += px.g;
            avgColor.b += px.b;
            avgColor.luminance += px.luminance;
            avgColor.saturation += px.saturation;
            avgColor.edge += px.edge;

            // Combine luminance relief with a warm harmonic fundamental + color harmonic richness
            const float phase = t * juce::MathConstants<float>::twoPi;
            const float colorRelief = (px.luminance * 2.0f - 1.0f) * 0.65f
                                    + (px.r - px.b) * 0.35f * std::sin(phase * 2.0f)
                                    + (px.g - 0.5f) * 0.25f * std::cos(phase * 3.0f);

            // Apply seamless periodic windowing near loop endpoints while preserving fundamental
            const float edgeWindow = std::sin(t * juce::MathConstants<float>::pi);
            const float val = 0.45f * std::sin(phase) + 0.55f * colorRelief * edgeWindow;
            raw[static_cast<size_t>(s)] = val;
            mean += val;
        }

        const float invN = 1.0f / static_cast<float>(WavetableData::kSamplesPerFrame);
        mean *= invN;
        avgColor.r *= invN;
        avgColor.g *= invN;
        avgColor.b *= invN;
        avgColor.luminance *= invN;
        avgColor.saturation *= invN;
        avgColor.edge *= invN;
        float h = 0.0f, sat = 0.0f, val = 0.0f;
        ImageCanvas::rgbToHsv(avgColor.r, avgColor.g, avgColor.b, h, sat, val);
        avgColor.hue = h;
        wt.frameColors[static_cast<size_t>(f)] = avgColor;

        // Remove DC offset and perform 5-point circular smoothing for warm, low-aliasing playback
        float peak = 1.0e-4f;
        for (int s = 0; s < WavetableData::kSamplesPerFrame; ++s)
        {
            const int sm2 = (s - 2 + WavetableData::kSamplesPerFrame) % WavetableData::kSamplesPerFrame;
            const int sm1 = (s - 1 + WavetableData::kSamplesPerFrame) % WavetableData::kSamplesPerFrame;
            const int sp1 = (s + 1) % WavetableData::kSamplesPerFrame;
            const int sp2 = (s + 2) % WavetableData::kSamplesPerFrame;

            const float smoothed = 0.1f * (raw[static_cast<size_t>(sm2)] - mean)
                                 + 0.2f * (raw[static_cast<size_t>(sm1)] - mean)
                                 + 0.4f * (raw[static_cast<size_t>(s)]   - mean)
                                 + 0.2f * (raw[static_cast<size_t>(sp1)] - mean)
                                 + 0.1f * (raw[static_cast<size_t>(sp2)] - mean);

            wt.frames[static_cast<size_t>(f)][static_cast<size_t>(s)] = smoothed;
            peak = std::max(peak, std::abs(smoothed));
        }

        const float normGain = 0.92f / peak;
        for (int s = 0; s < WavetableData::kSamplesPerFrame; ++s)
            wt.frames[static_cast<size_t>(f)][static_cast<size_t>(s)] *= normGain;
    }

    return wt;
}

void ImageCanvas::rgbToHsv(float r, float g, float b, float& h, float& s, float& v) noexcept
{
    const float maxC = std::max({ r, g, b });
    const float minC = std::min({ r, g, b });
    const float delta = maxC - minC;

    v = maxC;
    s = (maxC > 1.0e-5f) ? (delta / maxC) : 0.0f;

    if (delta <= 1.0e-5f)
    {
        h = 0.0f;
        return;
    }

    if (std::abs(maxC - r) <= 1.0e-6f)
        h = std::fmod((g - b) / delta, 6.0f);
    else if (std::abs(maxC - g) <= 1.0e-6f)
        h = ((b - r) / delta) + 2.0f;
    else
        h = ((r - g) / delta) + 4.0f;

    h /= 6.0f;
    if (h < 0.0f)
        h += 1.0f;
}

bool ImageCanvas::validateFilePath(const juce::String& rawPath, juce::String& errorMessage)
{
    if (rawPath.trim().isEmpty())
    {
        errorMessage = "File path is empty.";
        return false;
    }

    // Reject path traversal sequences in raw user-supplied paths
    if (rawPath.contains("../") || rawPath.contains("..\\")
        || rawPath.endsWith("/..") || rawPath.endsWith("\\..")
        || rawPath == "..")
    {
        errorMessage = "Path traversal sequences are not permitted.";
        return false;
    }

    if (rawPath.containsChar('\0'))
    {
        errorMessage = "Invalid null character in file path.";
        return false;
    }

    return true;
}

bool ImageCanvas::validateImageFile(const juce::File& file, juce::String& errorMessage)
{
    if (!validateFilePath(file.getFullPathName(), errorMessage))
        return false;

    if (!file.existsAsFile())
    {
        errorMessage = "Selected painting file does not exist.";
        return false;
    }

    const juce::String ext = file.getFileExtension().toLowerCase();
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg")
    {
        errorMessage = "Unsupported file extension. Only .png, .jpg, and .jpeg paintings are allowed.";
        return false;
    }

    const int64_t fileSize = file.getSize();
    if (fileSize < 12)
    {
        errorMessage = "File is too small to be a valid PNG or JPEG image.";
        return false;
    }
    if (fileSize > kMaxFileSizeBytes)
    {
        errorMessage = "File exceeds the 25 MB maximum size limit.";
        return false;
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        errorMessage = "Unable to open painting file for reading.";
        return false;
    }

    uint8_t header[8] = { 0 };
    if (stream.read(header, 8) != 8)
    {
        errorMessage = "Unable to read image file header.";
        return false;
    }

    const bool isPngMagic = (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47
                          && header[4] == 0x0D && header[5] == 0x0A && header[6] == 0x1A && header[7] == 0x0A);
    const bool isJpgMagic = (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF);

    if (ext == ".png" && !isPngMagic)
    {
        errorMessage = "File header does not match valid PNG magic bytes.";
        return false;
    }

    if ((ext == ".jpg" || ext == ".jpeg") && !isJpgMagic)
    {
        errorMessage = "File header does not match valid JPEG magic bytes.";
        return false;
    }

    return true;
}

std::shared_ptr<const SynestheticGrid> ImageCanvas::loadFromFile(const juce::File& file,
                                                                 juce::String& errorMessage)
{
    if (!validateImageFile(file, errorMessage))
        return nullptr;

    juce::Image decoded = juce::ImageFileFormat::loadFrom(file);
    if (!decoded.isValid())
    {
        errorMessage = "Failed to decode image data.";
        return nullptr;
    }

    if (decoded.getWidth() < 2 || decoded.getHeight() < 2
        || decoded.getWidth() > kMaxImageDimension
        || decoded.getHeight() > kMaxImageDimension)
    {
        errorMessage = "Image dimensions are outside the supported range (2x2 to 4096x4096).";
        return nullptr;
    }

    return createFromImage(decoded, file.getFileNameWithoutExtension());
}

std::shared_ptr<const SynestheticGrid> ImageCanvas::createFromImage(const juce::Image& srcImage,
                                                                    const juce::String& title)
{
    if (!srcImage.isValid() || srcImage.getWidth() < 2 || srcImage.getHeight() < 2)
        return nullptr;

    const int srcW = srcImage.getWidth();
    const int srcH = srcImage.getHeight();
    const int maxSrcDim = std::max(srcW, srcH);

    // Preserve exact original aspect ratio when downscaling analysis & display images
    const float gridScale = (maxSrcDim > kGridResolution)
        ? (static_cast<float>(kGridResolution) / static_cast<float>(maxSrcDim))
        : 1.0f;
    const int gridW = std::max(2, static_cast<int>(std::round(static_cast<float>(srcW) * gridScale)));
    const int gridH = std::max(2, static_cast<int>(std::round(static_cast<float>(srcH) * gridScale)));

    // Keep a reasonably sized display image for fast 60fps UI rendering
    const float dispScale = (maxSrcDim > 1024)
        ? (1024.0f / static_cast<float>(maxSrcDim))
        : 1.0f;
    const int dispW = std::max(2, static_cast<int>(std::round(static_cast<float>(srcW) * dispScale)));
    const int dispH = std::max(2, static_cast<int>(std::round(static_cast<float>(srcH) * dispScale)));
    juce::Image dispImg = (srcW == dispW && srcH == dispH)
                        ? srcImage
                        : srcImage.rescaled(dispW, dispH, juce::Graphics::mediumResamplingQuality);

    auto grid = std::make_shared<SynestheticGrid>(gridW, gridH, dispImg, title);
    auto& cells = grid->getMutableCells();

    juce::Image analysisImg = (srcW == gridW && srcH == gridH)
                            ? srcImage
                            : srcImage.rescaled(gridW, gridH, juce::Graphics::mediumResamplingQuality);

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            const juce::Colour col = analysisImg.getPixelAt(x, y);
            PixelSample px;
            px.r = col.getFloatRed();
            px.g = col.getFloatGreen();
            px.b = col.getFloatBlue();
            px.a = col.getFloatAlpha();
            px.luminance = std::clamp(0.2126f * px.r + 0.7152f * px.g + 0.0722f * px.b, 0.0f, 1.0f);

            float h = 0.0f, s = 0.0f, v = 0.0f;
            rgbToHsv(px.r, px.g, px.b, h, s, v);
            px.hue = h;
            px.saturation = s;

            cells[static_cast<size_t>(y * gridW + x)] = px;
        }
    }

    grid->computeSobelEdges();
    return grid;
}

std::shared_ptr<const SynestheticGrid> ImageCanvas::createPresetCanvas(int presetIndex)
{
    constexpr int w = 512;
    constexpr int h = 320;
    juce::Image img(juce::Image::RGB, w, h, true);
    juce::String title;

    if (presetIndex == 1)
    {
        title = "Sunset Impasto";
        for (int y = 0; y < h; ++y)
        {
            const float ny = static_cast<float>(y) / static_cast<float>(h - 1);
            for (int x = 0; x < w; ++x)
            {
                const float nx = static_cast<float>(x) / static_cast<float>(w - 1);
                const float wave1 = std::sin(nx * 14.0f + std::cos(ny * 8.0f) * 1.5f);
                const float wave2 = std::cos(ny * 22.0f + nx * 6.0f);
                const float knife = 0.5f + 0.5f * std::sin((nx * 35.0f + ny * 12.0f) + wave1);

                const float sunDist = std::hypot(nx - 0.52f, (ny - 0.42f) * 1.3f);
                const float sunGlow = std::exp(-sunDist * sunDist * 14.0f);

                float r = std::clamp(0.85f - ny * 0.35f + sunGlow * 0.45f + knife * 0.14f, 0.05f, 1.0f);
                float g = std::clamp(0.28f + (1.0f - std::abs(ny - 0.45f) * 1.8f) * 0.42f + sunGlow * 0.5f + wave2 * 0.08f, 0.04f, 1.0f);
                float b = std::clamp(0.32f + ny * 0.45f - sunGlow * 0.2f + wave1 * 0.12f, 0.08f, 1.0f);

                // Water reflection ripples in lower half
                if (ny > 0.58f)
                {
                    const float ripple = std::sin((ny - 0.58f) * 95.0f + nx * 18.0f);
                    if (ripple > 0.35f && std::abs(nx - 0.52f) < 0.28f)
                    {
                        r = std::min(1.0f, r + 0.35f);
                        g = std::min(1.0f, g + 0.28f);
                    }
                }

                img.setPixelAt(x, y, juce::Colour::fromFloatRGBA(r, g, b, 1.0f));
            }
        }
    }
    else if (presetIndex == 2)
    {
        title = "Watercolor Botanicals";
        for (int y = 0; y < h; ++y)
        {
            const float ny = static_cast<float>(y) / static_cast<float>(h - 1);
            for (int x = 0; x < w; ++x)
            {
                const float nx = static_cast<float>(x) / static_cast<float>(w - 1);

                // Soft parchment-teal wash background
                float r = 0.10f + 0.08f * std::sin(nx * 5.0f + ny * 3.0f);
                float g = 0.26f + 0.14f * std::cos(ny * 4.0f - nx * 4.0f);
                float b = 0.28f + 0.12f * std::sin(nx * 6.0f);

                // Repeating botanical fronds & blossoms
                for (int i = 0; i < 5; ++i)
                {
                    const float cx = 0.15f + 0.18f * static_cast<float>(i);
                    const float stemX = cx + 0.05f * std::sin(ny * 7.0f + static_cast<float>(i));
                    const float distStem = std::abs(nx - stemX);

                    // Leaf lobes along stem
                    const float leafWave = std::abs(std::sin(ny * 18.0f + static_cast<float>(i) * 1.3f)) * 0.07f;
                    if (distStem < leafWave && ny > 0.18f)
                    {
                        const float edgeFactor = 1.0f - (distStem / std::max(0.001f, leafWave));
                        r = std::clamp(r + 0.12f * edgeFactor, 0.0f, 1.0f);
                        g = std::clamp(g + 0.52f * edgeFactor, 0.0f, 1.0f);
                        b = std::clamp(b + 0.26f * edgeFactor, 0.0f, 1.0f);
                    }

                    // Sakura / lotus blossom near top of each stem
                    const float by = 0.24f + 0.08f * std::cos(static_cast<float>(i) * 2.1f);
                    const float bDist = std::hypot(nx - stemX, (ny - by) * 0.8f);
                    const float angle = std::atan2(ny - by, nx - stemX);
                    const float petalRadius = 0.065f + 0.025f * std::cos(angle * 5.0f);
                    if (bDist < petalRadius)
                    {
                        const float pGlow = 1.0f - (bDist / petalRadius);
                        r = std::clamp(0.92f * pGlow + r * (1.0f - pGlow), 0.0f, 1.0f);
                        g = std::clamp(0.45f * pGlow + g * (1.0f - pGlow), 0.0f, 1.0f);
                        b = std::clamp(0.68f * pGlow + b * (1.0f - pGlow), 0.0f, 1.0f);
                    }
                }

                img.setPixelAt(x, y, juce::Colour::fromFloatRGBA(r, g, b, 1.0f));
            }
        }
    }
    else
    {
        title = "Starry Nocturne";
        for (int y = 0; y < h; ++y)
        {
            const float ny = static_cast<float>(y) / static_cast<float>(h - 1);
            for (int x = 0; x < w; ++x)
            {
                const float nx = static_cast<float>(x) / static_cast<float>(w - 1);

                // Swirling Van Gogh-inspired night sky currents
                const float swirl = std::sin(nx * 10.0f + 2.2f * std::sin(ny * 7.5f + nx * 4.0f))
                                  * std::cos(ny * 9.0f - 1.8f * std::cos(nx * 6.0f));
                const float brush = 0.5f + 0.5f * std::sin((nx * 60.0f + swirl * 8.0f));

                float r = 0.06f + 0.10f * (0.5f + 0.5f * swirl) + 0.04f * brush;
                float g = 0.14f + 0.26f * (0.5f + 0.5f * swirl) + 0.08f * brush;
                float b = 0.36f + 0.42f * (1.0f - ny * 0.5f) + 0.14f * swirl;

                // Luminous golden stars & crescent moon
                constexpr float starX[6] = { 0.18f, 0.36f, 0.55f, 0.72f, 0.84f, 0.26f };
                constexpr float starY[6] = { 0.22f, 0.16f, 0.32f, 0.19f, 0.36f, 0.46f };
                constexpr float starR[6] = { 0.055f, 0.045f, 0.065f, 0.050f, 0.075f, 0.042f };

                for (int s = 0; s < 6; ++s)
                {
                    const float d = std::hypot(nx - starX[s], (ny - starY[s]) * 0.72f);
                    if (d < starR[s] * 1.8f)
                    {
                        const float ring = 0.5f + 0.5f * std::cos(d * 140.0f);
                        const float falloff = std::exp(-d * d / (starR[s] * starR[s] * 0.65f));
                        r += falloff * (0.88f + 0.12f * ring);
                        g += falloff * (0.74f + 0.15f * ring);
                        b += falloff * 0.22f;
                    }
                }

                // Rolling dark cypress & village silhouette at bottom
                const float hill = 0.78f + 0.06f * std::sin(nx * 9.0f) - (nx < 0.16f ? (0.16f - nx) * 3.2f : 0.0f);
                if (ny > hill)
                {
                    r *= 0.28f;
                    g *= 0.32f;
                    b *= 0.25f;
                }

                img.setPixelAt(x, y, juce::Colour::fromFloatRGBA(
                    std::clamp(r, 0.0f, 1.0f),
                    std::clamp(g, 0.0f, 1.0f),
                    std::clamp(b, 0.0f, 1.0f),
                    1.0f));
            }
        }
    }

    return createFromImage(img, title);
}

} // namespace soundcanvas
