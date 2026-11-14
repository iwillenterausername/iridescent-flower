#include "FilterModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

namespace
{
float processSvf (FilterModule::SvfState& state,
                  float input,
                  float g,
                  float k,
                  int type,
                  float& low,
                  float& band,
                  float& high) noexcept
{
    const auto a1 = 1.0f / (1.0f + g * (g + k));
    const auto a2 = g * a1;
    const auto a3 = g * a2;

    const auto v3 = input - state.ic2;
    const auto v1 = a1 * state.ic1 + a2 * v3;
    const auto v2 = state.ic2 + a2 * state.ic1 + a3 * v3;

    state.ic1 = 2.0f * v1 - state.ic1;
    state.ic2 = 2.0f * v2 - state.ic2;

    low = v2;
    band = v1;
    high = input - k * v1 - v2;

    switch (type)
    {
        case 1:  return high;
        case 2:  return band;
        case 3:  return high + low;
        default: return low;
    }
}
} // namespace

FilterModule::FilterModule (SlotState& state)
{
    cutoff.bind (state, ids::filterCutoff);
    resonance.bind (state, ids::filterReso);
    drive.bind (state, ids::filterDrive);
    mix.bind (state, ids::filterMix);
    type.bind (state, ids::filterType);
}

void FilterModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    cutoffSmooth.prepare (sampleRate, 0.025f);
    resonanceSmooth.prepare (sampleRate, 0.025f);
    driveSmooth.prepare (sampleRate, 0.025f);
    mixSmooth.prepare (sampleRate, 0.025f);

    cutoffSmooth.snap (juce::jlimit (20.0f, 20000.0f, cutoff.get()));
    resonanceSmooth.snap (juce::jlimit (0.0f, 1.0f, resonance.get()));
    driveSmooth.snap (dbToGain (juce::jlimit (0.0f, 24.0f, drive.get())));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));

    reset();
}

void FilterModule::reset()
{
    for (auto& stage : leftStages)
        stage.reset();

    for (auto& stage : rightStages)
        stage.reset();
}

void FilterModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    cutoffSmooth.setTarget (juce::jlimit (20.0f, 20000.0f, cutoff.get()));
    resonanceSmooth.setTarget (juce::jlimit (0.0f, 1.0f, resonance.get()));
    driveSmooth.setTarget (dbToGain (juce::jlimit (0.0f, 24.0f, drive.get())));
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));

    const auto filterType = juce::jlimit (0, 3, type.getIndex());
    auto* left = context.buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? context.buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;

        const auto frequency = juce::jlimit (20.0f, static_cast<float> (sampleRate) * 0.45f,
                                             cutoffSmooth.next());
        const auto resonanceValue = juce::jlimit (0.0f, 1.0f, resonanceSmooth.next());
        const auto driveGain = driveSmooth.next();
        const auto wetMix = mixSmooth.next();

        const auto g = std::tan (juce::MathConstants<float>::pi * frequency
                                 / static_cast<float> (sampleRate));
        const auto k = 2.0f - 1.9f * resonanceValue;

        auto inLeft = fastTanh (dryLeft * driveGain);
        auto inRight = fastTanh (dryRight * driveGain);
        float low = 0.0f, band = 0.0f, high = 0.0f;

        for (auto& stage : leftStages)
            inLeft = processSvf (stage, inLeft, g, k, filterType, low, band, high);

        for (auto& stage : rightStages)
            inRight = processSvf (stage, inRight, g, k, filterType, low, band, high);

        left[i] = dryLeft + wetMix * (inLeft - dryLeft);

        if (right != nullptr)
            right[i] = dryRight + wetMix * (inRight - dryRight);
    }
}

} // namespace iridescent
