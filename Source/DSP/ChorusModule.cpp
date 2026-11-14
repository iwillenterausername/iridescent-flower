#include "ChorusModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

ChorusModule::ChorusModule (SlotState& state)
{
    rate.bind (state, ids::chorusRate);
    depth.bind (state, ids::chorusDepth);
    feedback.bind (state, ids::chorusFeedback);
    mix.bind (state, ids::chorusMix);
    mode.bind (state, ids::chorusMode);
}

void ChorusModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    rateSmooth.prepare (sampleRate, 0.05f);
    depthSmooth.prepare (sampleRate, 0.05f);
    feedbackSmooth.prepare (sampleRate, 0.05f);
    mixSmooth.prepare (sampleRate, 0.05f);
    delayL.prepare (static_cast<int> (sampleRate * 0.1));
    delayR.prepare (static_cast<int> (sampleRate * 0.1));
    toneL.prepare (sampleRate);
    toneR.prepare (sampleRate);
    toneL.setCutoff (12500.0f);
    toneR.setCutoff (12500.0f);

    rateSmooth.snap (juce::jlimit (0.05f, 5.0f, rate.get()));
    depthSmooth.snap (juce::jlimit (0.0f, 1.0f, depth.get()));
    feedbackSmooth.snap (juce::jlimit (0.0f, 0.9f, feedback.get()));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));
    reset();
}

void ChorusModule::reset()
{
    delayL.reset();
    delayR.reset();
    toneL.reset();
    toneR.reset();
    phase = 0.0f;
}

void ChorusModule::process (ProcessContext& context)
{
    if (context.numSamples <= 0 || context.buffer.getNumChannels() <= 0)
        return;

    const auto isFlanger = mode.getIndex() == 1;
    const auto baseMs = isFlanger ? 2.0f : 17.0f;
    const auto modMs = isFlanger ? 2.0f : 7.0f;
    const auto rateValue = juce::jlimit (0.05f, 5.0f, rate.get());
    const auto depthValue = juce::jlimit (0.0f, 1.0f, depth.get());
    const auto feedbackValue = juce::jlimit (0.0f, 0.9f, feedback.get());
    const auto mixValue = juce::jlimit (0.0f, 1.0f, mix.get());

    rateSmooth.setTarget (rateValue);
    depthSmooth.setTarget (depthValue);
    feedbackSmooth.setTarget (feedbackValue);
    mixSmooth.setTarget (mixValue);

    auto* left = context.buffer.getWritePointer (0);
    auto* right = context.buffer.getNumChannels() > 1 ? context.buffer.getWritePointer (1) : nullptr;
    const auto phaseInc = rateSmooth.next() / static_cast<float> (sampleRate);

    for (int i = 0; i < context.numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;
        const auto depthNow = depthSmooth.next();
        const auto feedbackNow = feedbackSmooth.next();
        const auto lfoLeft = std::sin (phase * juce::MathConstants<float>::twoPi);
        const auto lfoRight = std::sin ((phase + 0.25f) * juce::MathConstants<float>::twoPi);

        const auto leftSamples = (baseMs + depthNow * modMs * (1.0f + lfoLeft) * 0.5f)
                               * 0.001f * static_cast<float> (sampleRate);
        const auto rightSamples = (baseMs + depthNow * modMs * (1.0f + lfoRight) * 0.5f)
                                * 0.001f * static_cast<float> (sampleRate);

        const auto rawLeft = delayL.readLinear (leftSamples);
        const auto rawRight = delayR.readLinear (rightSamples);
        const auto wetLeft = toneL.process (rawLeft);
        const auto wetRight = toneR.process (rawRight);

        delayL.write (fastTanh (dryLeft + wetLeft * feedbackNow));
        delayR.write (fastTanh (dryRight + wetRight * feedbackNow));

        const auto wet = mixSmooth.next();
        left[i] = dryLeft + wet * (wetLeft - dryLeft);

        if (right != nullptr)
            right[i] = dryRight + wet * (wetRight - dryRight);

        phase += phaseInc;

        if (phase >= 1.0f)
            phase -= 1.0f;
    }
}

} // namespace iridescent
