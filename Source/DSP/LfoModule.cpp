#include "LfoModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

namespace
{
float evaluateShape (float p, int shape, float randomCurrent, float randomTarget)
{
    switch (shape)
    {
        case 1:  return 1.0f - 4.0f * std::abs (p - 0.5f);       // triangle
        case 2:  return 2.0f * p - 1.0f;                         // saw up
        case 3:  return 1.0f - 2.0f * p;                         // saw down
        case 4:
        {
            const auto smooth = p * p * (3.0f - 2.0f * p);
            return randomCurrent + (randomTarget - randomCurrent) * smooth;
        }
        default: return std::sin (twoPi * p);                    // sine
    }
}
} // namespace

LfoModule::LfoModule (juce::AudioProcessorValueTreeState& state)
{
    rate.bind (state, ids::lfoRate);
    depth.bind (state, ids::lfoDepth);
    shape.bind (state, ids::lfoShape);
    phase.bind (state, ids::lfoPhase);
    pan.bind (state, ids::lfoPan);
    stereo.bind (state, ids::lfoStereo);
    on.bind (state, ids::lfoOn);
}

void LfoModule::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    values.assign (static_cast<size_t> (std::max (1, maximumBlockSize)), 0.0f);
    quadValues.assign (static_cast<size_t> (std::max (1, maximumBlockSize)), 0.0f);

    rateSmooth.prepare (sampleRate, 0.2f);
    rateSmooth.snap (juce::jlimit (0.01f, 8.0f, rate.get()));
    panSmooth.prepare (sampleRate, 0.06f);
    stereoSmooth.prepare (sampleRate, 0.06f);
    depthSmooth.prepare (sampleRate, 0.06f);

    reset();
}

void LfoModule::reset()
{
    freePhase = 0.0f;
    phaseOffset = 0.0f;
    randomCurrent = 0.0f;
    randomTarget = 0.0f;
    lastValue = 0.0f;
    depthValue = 0.0f;
    enabledValue = 1.0f;
    lastShape = 0;
    std::fill (values.begin(), values.end(), 0.0f);
    std::fill (quadValues.begin(), quadValues.end(), 0.0f);
}

void LfoModule::generate (int numSamples)
{
    if (numSamples <= 0)
        return;

    const auto count = static_cast<size_t> (numSamples);

    if (values.size() < count)
    {
        values.resize (count, 0.0f);
        quadValues.resize (count, 0.0f);
    }

    rateSmooth.setTarget (juce::jlimit (0.01f, 8.0f, rate.get()));
    const auto shapeValue = shape.getIndex();
    const auto phaseValue = wrapPhase (phase.get());
    depthValue = juce::jlimit (0.0f, 1.0f, depth.get());
    enabledValue = on.get();
    phaseOffset = phaseValue;

    if (shapeValue != lastShape)
    {
        randomCurrent = 0.0f;
        randomTarget = 0.0f;
        lastShape = shapeValue;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto increment = rateSmooth.next() / static_cast<float> (sampleRate);

        if (freePhase < increment && lastShape == 4)
        {
            randomCurrent = randomTarget;
            randomTarget = noise.nextBipolar();
        }

        const auto p = wrapPhase (phaseOffset + freePhase);
        values[static_cast<size_t> (i)] = evaluateShape (p, shapeValue, randomCurrent, randomTarget);
        quadValues[static_cast<size_t> (i)] = evaluateShape (wrapPhase (p + 0.25f), shapeValue,
                                                              randomCurrent, randomTarget);

        freePhase = wrapPhase (freePhase + increment);
    }

    lastValue = values[count - 1];
}

void LfoModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels < 2 || context.lfo == nullptr)
        return;

    panSmooth.setTarget (pan.get());
    stereoSmooth.setTarget (stereo.get());

    auto* left = context.buffer.getWritePointer (0);
    auto* right = context.buffer.getWritePointer (1);
    const auto enabledNow = on.get() > 0.5f ? 1.0f : 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right[i];
        const auto panAmount = panSmooth.next() * enabledNow;
        const auto stereoAmount = stereoSmooth.next();

        const auto position = juce::jlimit (0.0f, 1.0f,
                                           0.5f + 0.5f * (context.lfo[i] * (1.0f - stereoAmount)
                                                          + context.lfoQuad[i] * stereoAmount));
        const auto angle = position * halfPi;
        const auto gainL = std::cos (angle) * 1.41421356f;
        const auto gainR = std::sin (angle) * 1.41421356f;

        left[i] = dryLeft + panAmount * (dryLeft * gainL - dryLeft);
        right[i] = dryRight + panAmount * (dryRight * gainR - dryRight);
    }
}

} // namespace iridescent
