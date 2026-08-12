#include "CeilingFangPlugin.h"

#include "ProductState.h"

#if ! CEILINGFANG_HEADLESS_TEST
#include "ParameterGridEditor.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace ceilingfang::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'C', 'F', 'N', '1' }};
constexpr int stateVersion = 1;
constexpr std::size_t presetParameterCount = 7;
constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ -1.0f, 4.0f, 90.0f, 0.65f, 0.35f, 0.55f, 0.2f }},
    {{ -3.0f, 8.0f, 160.0f, 0.9f, 0.2f, 0.25f, 0.1f }},
    {{ -0.2f, 2.0f, 45.0f, 1.0f, 0.8f, 0.75f, 0.35f }},
    {{ -6.0f, 10.0f, 300.0f, 0.45f, 0.55f, 1.0f, 0.6f }}
}};

yup::AudioParameter::Ptr makeParameter (const char* id, const char* name, int hostID, float minValue, float maxValue, float defaultValue, yup::AudioParameter::ParameterUnit unit, float smoothingMs)
{
    return yup::AudioParameterBuilder().withID (id).withName (name).withHostID (static_cast<yup::uint32> (hostID)).withRange (minValue, maxValue).withDefault (defaultValue).withSmoothing (smoothingMs).withModulatable (true).withUnit (unit).build();
}
}

CeilingFangPlugin::CeilingFangPlugin()
    : yup::AudioProcessor ("CeilingFang", yup::AudioBusLayout ({ yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2) }, { yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2) }))
{
    parameters[ceiling] = makeParameter ("ceiling", "Ceiling", ceiling, -24.0f, 0.0f, presetValues[0][ceiling], yup::AudioParameter::ParameterUnit::Decibels, 8.0f);
    parameters[lookahead] = makeParameter ("lookahead", "Lookahead", lookahead, 0.0f, 20.0f, presetValues[0][lookahead], yup::AudioParameter::ParameterUnit::Milliseconds, 18.0f);
    parameters[release] = makeParameter ("release", "Release", release, 5.0f, 800.0f, presetValues[0][release], yup::AudioParameter::ParameterUnit::Milliseconds, 24.0f);
    parameters[detect] = makeParameter ("detect", "Detect", detect, 0.0f, 1.0f, presetValues[0][detect], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[zeroBias] = makeParameter ("zeroBias", "ZeroBias", zeroBias, 0.0f, 1.0f, presetValues[0][zeroBias], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[adapt] = makeParameter ("adapt", "Adapt", adapt, 0.0f, 1.0f, presetValues[0][adapt], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[clip] = makeParameter ("clip", "Clip", clip, 0.0f, 1.0f, presetValues[0][clip], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    for (const auto& parameter : parameters)
        addParameter (parameter);
    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void CeilingFangPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    engine.reset();
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);
    syncParameterValuesFromParameters();
    updateEngineParameters();
    controlUpdateCountdown = 0;
    inputPeakMilli.store (0, std::memory_order_relaxed);
    outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionSampleRate = std::isfinite (spec.sampleRate) && spec.sampleRate > 1.0 ? spec.sampleRate : 44100.0;
    auditionPhase = 0.0f;
    auditionNoise = 0x6d2b79f5u;
#endif
}

void CeilingFangPlugin::releaseResources() {}

void CeilingFangPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());
    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;
    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        advanceParameterHandles (sample);
        if (controlUpdateCountdown <= 0) { updateEngineParameters(); controlUpdateCountdown = parameterUpdateCadenceSamples; }
        --controlUpdateCountdown;
        auto inputLeft = left != nullptr ? left[sample] : 0.0f;
        auto inputRight = right != nullptr ? right[sample] : inputLeft;
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
        const auto audition = renderAuditionFrame();
        inputLeft += audition.left;
        inputRight += audition.right;
#endif
        blockInputPeak = std::max (blockInputPeak, std::max (std::fabs (inputLeft), std::fabs (inputRight)));
        const auto frame = engine.processSample (inputLeft, inputRight);
        if (left != nullptr) left[sample] = frame.left;
        if (right != nullptr) right[sample] = frame.right;
        blockOutputPeak = std::max (blockOutputPeak, std::max (std::fabs (frame.left), std::fabs (frame.right)));
        for (int channel = 2; channel < numChannels; ++channel) audio.getWritePointer (channel)[sample] = 0.0f;
    }
    inputPeakMilli.store (static_cast<int> (std::clamp (blockInputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f), std::memory_order_relaxed);
    outputPeakMilli.store (static_cast<int> (std::clamp (blockOutputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f), std::memory_order_relaxed);
    context.midi.clear();
}

void CeilingFangPlugin::flush()
{
    engine.reset(); controlUpdateCountdown = 0; inputPeakMilli.store (0, std::memory_order_relaxed); outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionPhase = 0.0f; auditionNoise = 0x6d2b79f5u;
#endif
}

bool CeilingFangPlugin::acceptsMidi() const noexcept { return false; }
bool CeilingFangPlugin::producesMidi() const noexcept { return false; }
int CeilingFangPlugin::getCurrentPreset() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
void CeilingFangPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size()))) return;
    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i) parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}
int CeilingFangPlugin::getNumPresets() const { return static_cast<int> (presetNames.size()); }
yup::String CeilingFangPlugin::getPresetName (int index) const { return yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())) ? presetNames[static_cast<std::size_t> (index)] : "Invalid Preset"; }
void CeilingFangPlugin::setPresetName (int index, yup::StringRef newName) { if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size()))) presetNames[static_cast<std::size_t> (index)] = newName; }
yup::Result CeilingFangPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0; const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
    if (result.failed()) return result; currentPreset.store (loadedPreset, std::memory_order_relaxed); return yup::Result::ok();
}
yup::Result CeilingFangPlugin::saveStateIntoMemory (yup::MemoryBlock& data) { return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed)); }
bool CeilingFangPlugin::hasEditor() const
{
#if CEILINGFANG_HEADLESS_TEST
    return false;
#else
    return true;
#endif
}
yup::AudioProcessorEditor* CeilingFangPlugin::createEditor()
{
#if CEILINGFANG_HEADLESS_TEST
    return nullptr;
#else
    return new ParameterGridEditor (*this, "CeilingFang", "Lookahead inter-sample peak limiter with standalone-only audition.", 0xfff2f2f0u);
#endif
}
float CeilingFangPlugin::getInputPeakLevel() const noexcept { return static_cast<float> (inputPeakMilli.load (std::memory_order_relaxed)) * 0.001f; }
float CeilingFangPlugin::getOutputPeakLevel() const noexcept { return static_cast<float> (outputPeakMilli.load (std::memory_order_relaxed)) * 0.001f; }
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
void CeilingFangPlugin::setAuditionEnabled (bool shouldBeEnabled) noexcept { auditionEnabled.store (shouldBeEnabled ? 1 : 0, std::memory_order_relaxed); }
bool CeilingFangPlugin::isAuditionEnabled() const noexcept { return auditionEnabled.load (std::memory_order_relaxed) != 0; }
void CeilingFangPlugin::setAuditionType (int type) noexcept { auditionType.store (std::clamp (type, 0, 1), std::memory_order_relaxed); }
int CeilingFangPlugin::getAuditionType() const noexcept { return auditionType.load (std::memory_order_relaxed); }
#endif
void CeilingFangPlugin::advanceParameterHandles (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < parameterHandles.size(); ++i) { parameterHandles[i].advanceToSample (samplePosition); currentParameterValues[i] = parameterHandles[i].getNextValue(); }
}
void CeilingFangPlugin::syncParameterValuesFromParameters() noexcept { for (std::size_t i = 0; i < parameters.size(); ++i) currentParameterValues[i] = parameters[i]->getValue(); }
void CeilingFangPlugin::updateEngineParameters() noexcept
{
    ceilingfang::CeilingFangParameters engineParameters;
    engineParameters.ceiling = currentParameterValues[ceiling];
    engineParameters.lookahead = currentParameterValues[lookahead];
    engineParameters.release = currentParameterValues[release];
    engineParameters.detect = currentParameterValues[detect];
    engineParameters.zeroBias = currentParameterValues[zeroBias];
    engineParameters.adapt = currentParameterValues[adapt];
    engineParameters.clip = currentParameterValues[clip];
    engine.setParameters (engineParameters);
}
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
StereoFrame CeilingFangPlugin::renderAuditionFrame() noexcept
{
    if (auditionEnabled.load (std::memory_order_relaxed) == 0) return {};
    auditionPhase += 96.0f / static_cast<float> (auditionSampleRate);
    if (auditionPhase >= 1.0f) auditionPhase -= 1.0f;
    auditionNoise ^= auditionNoise << 13u; auditionNoise ^= auditionNoise >> 17u; auditionNoise ^= auditionNoise << 5u;
    if (auditionNoise == 0u) auditionNoise = 0x6d2b79f5u;
    const auto type = auditionType.load (std::memory_order_relaxed);
    const auto noise = static_cast<float> (static_cast<double> (auditionNoise) / 2147483648.0 - 1.0);
    const auto pulse = auditionPhase < 0.18f ? 1.0f : -0.55f;
    const auto saw = auditionPhase * 2.0f - 1.0f;
    const auto source = type == 0 ? saw * 0.22f + noise * 0.035f : pulse * 0.18f + noise * 0.055f;
    return { source, source * 0.93f };
}
#endif

} // namespace ceilingfang::plugin

extern "C" yup::AudioProcessor* createPluginProcessor() { return new ceilingfang::plugin::CeilingFangPlugin(); }
