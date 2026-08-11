#include "ceilingfang/CeilingFangEngine.h"

#include <algorithm>
#include <cmath>

namespace ceilingfang
{

CeilingFangEngine::CeilingFangEngine() { prepare (44100.0); reset(); }

void CeilingFangEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    setParameters (params);
    reset();
}

void CeilingFangEngine::reset() noexcept
{
    delay.fill ({});
    writeIndex = 0;
    previousLeft = 0.0f;
    previousRight = 0.0f;
    gain = 1.0f;
    releaseMemory = 0.0f;
}

void CeilingFangEngine::setParameters (const CeilingFangParameters& p) noexcept
{
    params.ceiling = clampFinite (p.ceiling, -24.0f, 0.0f, CeilingFangParameters {}.ceiling);
    params.lookahead = clampFinite (p.lookahead, 0.0f, 20.0f, CeilingFangParameters {}.lookahead);
    params.release = clampFinite (p.release, 5.0f, 800.0f, CeilingFangParameters {}.release);
    params.detect = clampFinite (p.detect, 0.0f, 1.0f, CeilingFangParameters {}.detect);
    params.zeroBias = clampFinite (p.zeroBias, 0.0f, 1.0f, CeilingFangParameters {}.zeroBias);
    params.adapt = clampFinite (p.adapt, 0.0f, 1.0f, CeilingFangParameters {}.adapt);
    params.clip = clampFinite (p.clip, 0.0f, 1.0f, CeilingFangParameters {}.clip);
    lookaheadSamples = std::clamp (static_cast<int> (std::round (params.lookahead * sampleRate * 0.001)), 0, maxLookaheadSamples - 1);
}

float CeilingFangEngine::coefficientForMs (float milliseconds) const noexcept
{
    return std::exp (-1.0f / (std::max (0.001f, milliseconds) * 0.001f * static_cast<float> (sampleRate)));
}

float CeilingFangEngine::detectTruePeakish (float left, float right) noexcept
{
    const auto midLeft = 0.5f * (previousLeft + left);
    const auto midRight = 0.5f * (previousRight + right);
    const auto quarterLeft = 0.75f * previousLeft + 0.25f * left;
    const auto threeQuarterLeft = 0.25f * previousLeft + 0.75f * left;
    const auto quarterRight = 0.75f * previousRight + 0.25f * right;
    const auto threeQuarterRight = 0.25f * previousRight + 0.75f * right;
    previousLeft = left;
    previousRight = right;
    const auto peak = std::max ({ std::fabs (left), std::fabs (right), std::fabs (midLeft), std::fabs (midRight), std::fabs (quarterLeft), std::fabs (threeQuarterLeft), std::fabs (quarterRight), std::fabs (threeQuarterRight) });
    const auto channelSum = std::fabs (left) + std::fabs (right);
    const auto detector = params.detect * peak + (1.0f - params.detect) * (channelSum * 0.5f);
    return std::max (detector, 1.0e-9f);
}

StereoFrame CeilingFangEngine::delayedFrame (float left, float right) noexcept
{
    delay[static_cast<std::size_t> (writeIndex)] = { left, right };
    auto readIndex = writeIndex - lookaheadSamples;
    if (readIndex < 0) readIndex += maxLookaheadSamples;
    ++writeIndex;
    if (writeIndex >= maxLookaheadSamples) writeIndex = 0;
    return delay[static_cast<std::size_t> (readIndex)];
}

StereoFrame CeilingFangEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto left = sanitizeAudio (inputLeft);
    const auto right = sanitizeAudio (inputRight);
    const auto peak = detectTruePeakish (left, right);
    const auto ceilingLinear = std::pow (10.0f, params.ceiling / 20.0f);
    const auto wantedGain = std::min (1.0f, ceilingLinear / peak);
    releaseMemory = 0.999f * releaseMemory + 0.001f * std::max (0.0f, peak - ceilingLinear);
    const auto adaptiveRelease = params.release * (1.0f - 0.65f * params.adapt * std::clamp (releaseMemory * 4.0f, 0.0f, 1.0f));
    const auto coefficient = wantedGain < gain ? coefficientForMs (0.08f + params.zeroBias * 1.5f) : coefficientForMs (adaptiveRelease);
    gain = std::min (1.0f, coefficient * gain + (1.0f - coefficient) * wantedGain);
    const auto delayed = delayedFrame (left, right);
    return sanitizeFrame (delayed.left * gain, delayed.right * gain);
}

void CeilingFangEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0) return;
    for (int i = 0; i < numSamples; ++i) { const auto f = processSample (left[i], right[i]); left[i] = f.left; right[i] = f.right; }
}

StereoFrame CeilingFangEngine::sanitizeFrame (float left, float right) const noexcept
{
    const auto ceilingLinear = std::pow (10.0f, params.ceiling / 20.0f);
    const auto drive = 1.0f + params.clip * 2.5f;
    const auto hard = std::min (0.999f, ceilingLinear * (1.0f + params.clip * 0.02f));
    auto l = params.clip > 0.0001f ? softClip (left / std::max (0.1f, hard), drive) * hard : left;
    auto r = params.clip > 0.0001f ? softClip (right / std::max (0.1f, hard), drive) * hard : right;
    l = std::clamp (l, -hard, hard);
    r = std::clamp (r, -hard, hard);
    if (std::fabs (l) < 1.0e-20f) l = 0.0f;
    if (std::fabs (r) < 1.0e-20f) r = 0.0f;
    return { l, r };
}

} // namespace ceilingfang
