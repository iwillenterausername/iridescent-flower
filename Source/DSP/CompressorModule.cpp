#include "CompressorModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

CompressorModule::CompressorModule (SlotState& state)
{
    threshold.bind (state, ids::compThreshold);
    ratio.bind (state, ids::compRatio);
    attack.bind (state, ids::compAttack);
    release.bind (state, ids::compRelease);
    makeup.bind (state, ids::compMakeup);
    mix.bind (state, ids::compMix);
}

void CompressorModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    makeupSmooth.prepare (sampleRate, 0.04f);
    mixSmooth.prepare (sampleRate, 0.04f);
    makeupSmooth.snap (dbToGain (juce::jlimit (0.0f, 24.0f, makeup.get())));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));
    reset();
}

void CompressorModule::reset()
{
    envelope = 0.0f;
}

void CompressorModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto thresholdDb = juce::jlimit (-60.0f, 0.0f, threshold.get());
    const auto ratioValue = juce::jlimit (1.0f, 20.0f, ratio.get());
    const auto attackMs = juce::jlimit (0.5f, 100.0f, attack.get());
    const auto releaseMs = juce::jlimit (10.0f, 1000.0f, release.get());

    attackCoefficient = 1.0f - std::exp (-1.0f / (attackMs * 0.001f * static_cast<float> (sampleRate)));
    releaseCoefficient = 1.0f - std::exp (-1.0f / (releaseMs * 0.001f * static_cast<float> (sampleRate)));

    makeupSmooth.setTarget (dbToGain (juce::jlimit (0.0f, 24.0f, makeup.get())));
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));

    auto* left = context.buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? context.buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;

        const auto detector = std::max (std::abs (dryLeft), std::abs (dryRight));
        const auto coefficient = detector > envelope ? attackCoefficient : releaseCoefficient;
        envelope += (detector - envelope) * coefficient;

        const auto levelDb = 20.0f * std::log10 (std::max (1.0e-6f, envelope));
        float reductionDb = 0.0f;

        // Soft knee over 6 dB, then a normal ratio above the knee.
        if (levelDb > thresholdDb)
        {
            const auto over = levelDb - thresholdDb;
            reductionDb = over * (1.0f - 1.0f / ratioValue);
        }

        const auto gain = dbToGain (-reductionDb);
        const auto makeupValue = makeupSmooth.next();
        const auto wetMix = mixSmooth.next();

        const auto compressedLeft = dryLeft * gain * makeupValue;
        const auto compressedRight = dryRight * gain * makeupValue;
        left[i] = dryLeft + wetMix * (compressedLeft - dryLeft);

        if (right != nullptr)
            right[i] = dryRight + wetMix * (compressedRight - dryRight);
    }
}

} // namespace iridescent
