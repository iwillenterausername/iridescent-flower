#include "EchoModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

EchoModule::EchoModule (SlotState& state)
{
    time.bind (state, ids::echoTime);
    feedback.bind (state, ids::echoFeedback);
    diffusion.bind (state, ids::echoDiffusion);
    tone.bind (state, ids::echoTone);
    mix.bind (state, ids::echoMix);
    pingpong.bind (state, ids::echoPingpong);
    sync.bind (state, ids::echoSync);
    division.bind (state, ids::echoDivision);
}

void EchoModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto& delay : delays)
        delay.prepare (static_cast<int> (sampleRate * 2.2));

    constexpr float baseTimes[] = { 5.1f, 8.7f, 13.3f, 19.1f };

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < 4; ++i)
        {
            const auto samples = static_cast<int> ((baseTimes[i] + channel * 1.37f)
                                                   * 0.001f * static_cast<float> (sampleRate));
            diffusers[static_cast<size_t> (channel)][static_cast<size_t> (i)].prepare (std::max (2, samples), 0.5f);
        }

        feedbackTone[static_cast<size_t> (channel)].prepare (sampleRate);
        outputTone[static_cast<size_t> (channel)].prepare (sampleRate);
        dcBlockers[static_cast<size_t> (channel)].prepare (sampleRate, 9.0f);
    }

    timeSmooth.prepare (sampleRate, 0.07f);
    feedbackSmooth.prepare (sampleRate, 0.04f);
    mixSmooth.prepare (sampleRate, 0.04f);

    timeSmooth.snap (juce::jlimit (20.0f, 2000.0f, time.get()) * 0.001f
                     * static_cast<float> (sampleRate));
    feedbackSmooth.snap (juce::jlimit (0.0f, 0.95f, feedback.get()));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));

    reset();
}

void EchoModule::reset()
{
    for (auto& delay : delays)
        delay.reset();

    for (auto& channel : diffusers)
        for (auto& ap : channel)
            ap.reset();

    for (auto& filter : feedbackTone)
        filter.reset();

    for (auto& filter : outputTone)
        filter.reset();

    for (auto& dc : dcBlockers)
        dc.reset();
}

void EchoModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    auto timeMs = juce::jlimit (20.0f, 2000.0f, time.get());

    if (sync.get() > 0.5f)
        timeMs = juce::jlimit (20.0f, 2000.0f,
                               divisionBeats (juce::roundToInt (division.get())) * 60000.0f
                                   / static_cast<float> (juce::jmax (20.0, context.bpm)));

    timeSmooth.setTarget (timeMs * 0.001f * static_cast<float> (sampleRate));
    feedbackSmooth.setTarget (juce::jlimit (0.0f, 0.95f, feedback.get()));
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));

    const auto toneValue = juce::jlimit (0.0f, 1.0f, tone.get());
    const auto diffusionValue = juce::jlimit (0.0f, 1.0f, diffusion.get());
    const auto cutoff = 750.0f + 12500.0f * toneValue;

    for (int channel = 0; channel < 2; ++channel)
    {
        feedbackTone[static_cast<size_t> (channel)].setCutoff (cutoff);
        outputTone[static_cast<size_t> (channel)].setCutoff (std::min (cutoff * 1.25f, 18000.0f));

        for (auto& ap : diffusers[static_cast<size_t> (channel)])
            ap.setGain (0.18f + diffusionValue * 0.62f);
    }

    auto* left = context.buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? context.buffer.getWritePointer (1) : nullptr;
    const auto usePingpong = pingpong.get() > 0.5f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;

        const auto delaySamples = timeSmooth.next();
        const auto fb = feedbackSmooth.next();
        const auto wetMix = mixSmooth.next();

        auto delayedLeft = delays[0].readCubic (delaySamples);
        auto delayedRight = delays[1].readCubic (delaySamples);

        auto outLeft = outputTone[0].process (delayedLeft);
        auto outRight = outputTone[1].process (delayedRight);

        auto feedLeft = delayedLeft * fb;
        auto feedRight = delayedRight * fb;

        if (usePingpong)
            std::swap (feedLeft, feedRight);

        for (auto& ap : diffusers[0])
            feedLeft = ap.process (feedLeft);

        for (auto& ap : diffusers[1])
            feedRight = ap.process (feedRight);

        feedLeft = dcBlockers[0].process (fastTanh (feedbackTone[0].process (feedLeft) * 1.25f) / 1.25f);
        feedRight = dcBlockers[1].process (fastTanh (feedbackTone[1].process (feedRight) * 1.25f) / 1.25f);

        delays[0].write (dryLeft + feedLeft);
        delays[1].write (dryRight + feedRight);

        const auto mixedLeft = dryLeft * (1.0f - wetMix) + outLeft * wetMix;
        const auto mixedRight = dryRight * (1.0f - wetMix) + outRight * wetMix;

        left[i] = mixedLeft;

        if (right != nullptr)
            right[i] = mixedRight;
    }
}

} // namespace iridescent
