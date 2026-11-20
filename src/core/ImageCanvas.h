#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <memory>

namespace soundcanvas
{

struct PixelSample
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    float hue = 0.0f;        // [0.0, 1.0]
    float saturation = 0.0f; // [0.0, 1.0]
    float luminance = 0.0f;  // [0.0, 1.0] Rec.709 perceptual brightness
    float edge = 0.0f;       // [0.0, 1.0] Sobel brushstroke edge gradient
};

struct WavetableData
{
    static constexpr int kNumFrames = 64;
    static constexpr int kSamplesPerFrame = 256;

    std::array<std::array<float, kSamplesPerFrame>, kNumFrames> frames {};
    std::array<PixelSample, kNumFrames> frameColors {};
};

struct RegionStats
{
    PixelSample mean {};
    float colorVariance = 0.0f;
    std::array<float, 16> harmonicProfile {}; // 16 radial/angular harmonic weights in [0.0, 1.0]
};

class SynestheticGrid
{
public:
    SynestheticGrid() = default;
    SynestheticGrid(int gridWidth, int gridHeight, juce::Image displayImg, juce::String title);

    int getWidth() const noexcept { return width_; }
    int getHeight() const noexcept { return height_; }
    const juce::Image& getDisplayImage() const noexcept { return displayImage_; }
    const juce::String& getTitle() const noexcept { return title_; }

    // Bilinear interpolated lookup for normalized coordinates [0.0, 1.0]
    // normY = 0.0 corresponds to bottom (low pitch), normY = 1.0 corresponds to top (high pitch)
    PixelSample sampleAt(float normX, float normY) const noexcept;

    // Sample radial/angular statistics around a canvas point (top-down norm coordinates [0, 1])
    RegionStats sampleRegionStats(juce::Point<float> center, float radius) const noexcept;

    // Extract 64-frame band-smoothed wavetable along a 2D brushstroke ribbon A -> B
    WavetableData extractWavetable(juce::Point<float> pointA,
                                   juce::Point<float> pointB,
                                   float ribbonWidth = 0.12f) const noexcept;

    std::vector<PixelSample>& getMutableCells() noexcept { return cells_; }
    const std::vector<PixelSample>& getCells() const noexcept { return cells_; }
    void computeSobelEdges() noexcept;

private:
    const PixelSample& cellAt(int x, int y) const noexcept;

    int width_ = 0;
    int height_ = 0;
    std::vector<PixelSample> cells_;
    juce::Image displayImage_;
    juce::String title_;
};

class ImageCanvas
{
public:
    static constexpr int64_t kMaxFileSizeBytes = 25LL * 1024LL * 1024LL; // 25 MB
    static constexpr int kMaxImageDimension = 4096;
    static constexpr int kGridResolution = 256;

    // Security validation for untrusted file paths and image headers
    static bool validateFilePath(const juce::String& rawPath, juce::String& errorMessage);
    static bool validateImageFile(const juce::File& file, juce::String& errorMessage);

    // Load a PNG/JPG painting from disk after security checks
    static std::shared_ptr<const SynestheticGrid> loadFromFile(const juce::File& file,
                                                               juce::String& errorMessage);

    // Build a SynestheticGrid from an in-memory juce::Image
    static std::shared_ptr<const SynestheticGrid> createFromImage(const juce::Image& srcImage,
                                                                  const juce::String& title);

    // Generate one of the 3 built-in procedural starter paintings:
    // 0 = Starry Nocturne, 1 = Sunset Impasto, 2 = Watercolor Botanicals
    static std::shared_ptr<const SynestheticGrid> createPresetCanvas(int presetIndex);

    static void rgbToHsv(float r, float g, float b, float& h, float& s, float& v) noexcept;
};

} // namespace soundcanvas
