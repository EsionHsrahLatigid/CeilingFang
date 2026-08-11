#pragma once

#include "ceilingfang/CeilingFangDspPrimitives.h"

#include <array>

namespace ceilingfang
{

struct CeilingFangParameters
{
    float ceiling = -1.0f;
    float lookahead = 4.0f;
    float release = 90.0f;
    float detect = 0.65f;
    float zeroBias = 0.35f;
    float adapt = 0.55f;
    float clip = 0.20f;
};

class CeilingFangEngine
{
public:
    CeilingFangEngine();
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters (const CeilingFangParameters& parameters) noexcept;
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int maxLookaheadSamples = 2048;
    [[nodiscard]] float coefficientForMs (float milliseconds) const noexcept;
    [[nodiscard]] float detectTruePeakish (float left, float right) noexcept;
    [[nodiscard]] StereoFrame delayedFrame (float left, float right) noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    CeilingFangParameters params;
    double sampleRate = 44100.0;
    int lookaheadSamples = 0;
    int writeIndex = 0;
    std::array<StereoFrame, maxLookaheadSamples> delay {};
    float previousLeft = 0.0f;
    float previousRight = 0.0f;
    float gain = 1.0f;
    float releaseMemory = 0.0f;
};

} // namespace ceilingfang
