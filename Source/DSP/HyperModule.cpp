#include "HyperModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

HyperModule::HyperModule (SlotState& state)
{
    detune.bind (state, ids::hyperDetune);
    width.bind (state, ids::hyperWidth);
    rate.bind (state, ids::hyperRate);
    mix.bind (state, ids::hyperMix);
}

void HyperModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    detuneSmooth.prepare (sampleRate, 0.08f);
    widthSmooth.prepare (sampleRate, 0.08f);
    rateSmooth.prepare (sampleRate, 0.08f);
    mixSmooth.prepare (sampleRate, 0.08f);
    delayL.prepare (static_cast<int> (sampleRate * 0.08));
    delayR.prepare (static_cast<int> (sampleRate * 0.08));

    detuneSmooth.snap (juce::jlimit (0.0f, 50.0f, detune.get()));
    widthSmooth.snap (juce::jlimit (0.0f, 1.0f, width.get()));
    rateSmooth.snap (juce::jlimit (0.02f, 2.0f, rate.get()));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));
    reset();
}

void HyperModule::reset()
{
    delayL.reset();
    delayR.reset();
    phase = 0.0f;
}

void HyperModule::process (ProcessContext& context)
{
    if (context.numSamples <= 0 || context.buffer.getNumChannels() <= 0)
        return;

    detuneSmooth.setTarget (juce::jlimit (0.0f, 50.0f, detune.get()));
    widthSmooth.setTarget (juce::jlimit (0.0f, 1.0f, width.get()));
    rateSmooth.setTarget (juce::jlimit (0.02f, 2.0f, rate.get()));
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));

    auto* left = context.buffer.getWritePointer (0);
    auto* right = context.buffer.getNumChannels() > 1 ? context.buffer.getWritePointer (1) : nullptr;
    const auto inc = rateSmooth.next() / static_cast<float> (sampleRate);

    for (int i = 0; i < context.numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;
        const auto detuneValue = detuneSmooth.next();
        const auto widthValue = widthSmooth.next();
        const auto lfoL = std::sin (phase * juce::MathConstants<float>::twoPi);
        const auto lfoR = std::sin ((phase + 0.5f) * juce::MathConstants<float>::twoPi);

        const auto baseMs = 7.0f + detuneValue * 0.22f;
        const auto modMs = 0.25f + detuneValue * 0.05f;
        const auto delayLSamples = (baseMs + modMs * lfoL) * 0.001f * static_cast<float> (sampleRate);
        const auto delayRSamples = (baseMs + modMs * lfoR) * 0.001f * static_cast<float> (sampleRate);

        const auto wetLeft = delayL.readCubic (delayLSamples);
        const auto wetRight = delayR.readCubic (delayRSamples);
        delayL.write (dryLeft);
        delayR.write (dryRight);

        auto outLeft = dryLeft * (1.0f - widthValue * 0.25f) + wetRight * widthValue * 0.55f;
        auto outRight = dryRight * (1.0f - widthValue * 0.25f) + wetLeft * widthValue * 0.55f;
        const auto wet = mixSmooth.next();
        left[i] = dryLeft + wet * (outLeft - dryLeft);

        if (right != nullptr)
            right[i] = dryRight + wet * (outRight - dryRight);

        phase += inc;

        if (phase >= 1.0f)
            phase -= 1.0f;
    }
}

} // namespace iridescent
