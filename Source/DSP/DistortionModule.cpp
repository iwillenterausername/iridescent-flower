#include "DistortionModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

DistortionModule::DistortionModule (SlotState& state)
{
    drive.bind (state, ids::distDrive);
    tone.bind (state, ids::distTone);
    bits.bind (state, ids::distBits);
    downsample.bind (state, ids::distDown);
    mix.bind (state, ids::distMix);
    mode.bind (state, ids::distMode);
}

void DistortionModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    driveSmooth.prepare (sampleRate, 0.02f);
    mixSmooth.prepare (sampleRate, 0.02f);
    toneL.prepare (sampleRate);
    toneR.prepare (sampleRate);
    driveSmooth.snap (1.0f);
    mixSmooth.snap (1.0f);
    reset();
}

void DistortionModule::reset()
{
    toneL.reset();
    toneR.reset();
    heldL = heldR = 0.0f;
    holdCounter = 0;
}

void DistortionModule::process (ProcessContext& context)
{
    if (context.numSamples <= 0 || context.buffer.getNumChannels() <= 0)
        return;

    const auto driveValue = dbToGain (juce::jlimit (0.0f, 36.0f, drive.get()));
    const auto toneValue = juce::jlimit (0.0f, 1.0f, tone.get());
    const auto bitDepth = juce::jlimit (2.0f, 16.0f, bits.get());
    const auto levels = std::pow (2.0f, bitDepth - 1.0f);
    const auto down = juce::jlimit (1, 64, juce::roundToInt (downsample.get()));
    const auto shaping = juce::jlimit (0, 2, mode.getIndex());
    const auto cutoff = 800.0f + toneValue * 15000.0f;

    driveSmooth.setTarget (driveValue);
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));
    toneL.setCutoff (cutoff);
    toneR.setCutoff (cutoff);

    auto* left = context.buffer.getWritePointer (0);
    auto* right = context.buffer.getNumChannels() > 1 ? context.buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < context.numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;
        const auto gain = driveSmooth.next();

        if (holdCounter <= 0)
        {
            auto l = dryLeft * gain;
            auto r = dryRight * gain;

            if (shaping == 0)
            {
                l = fastTanh (l);
                r = fastTanh (r);
            }
            else if (shaping == 1)
            {
                l = juce::jlimit (-1.0f, 1.0f, l);
                r = juce::jlimit (-1.0f, 1.0f, r);
            }
            else
            {
                l = std::sin (l * 1.7f);
                r = std::sin (r * 1.7f);
            }

            l = std::round (l * levels) / levels;
            r = std::round (r * levels) / levels;
            heldL = toneL.process (l);
            heldR = toneR.process (r);
            holdCounter = down;
        }

        --holdCounter;
        const auto wet = mixSmooth.next();
        left[i] = dryLeft + wet * (heldL - dryLeft);

        if (right != nullptr)
            right[i] = dryRight + wet * (heldR - dryRight);
    }
}

} // namespace iridescent
