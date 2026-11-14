#include "MorphModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

MorphModule::MorphModule (juce::AudioProcessorValueTreeState& state)
    : lfo (state)
{
    amount.bind (state, ids::morphAmount);
    bTrim.bind (state, ids::morphBTrim);
    curve.bind (state, ids::morphCurve);
    bWidth.bind (state, ids::morphBWidth);
    source.bind (state, ids::morphSource);
}

void MorphModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto& delay : internalDelay)
        delay.prepare (static_cast<int> (sampleRate * 0.08));

    for (auto& hp : internalHighpass)
    {
        hp.prepare (sampleRate);
        hp.setCutoff (65.0f);
    }

    amountSmooth.prepare (sampleRate, 0.04f);
    trimSmooth.prepare (sampleRate, 0.04f);
    curveSmooth.prepare (sampleRate, 0.04f);
    widthSmooth.prepare (sampleRate, 0.04f);
    lfo.prepare (sampleRate, 512);

    reset();
}

void MorphModule::reset()
{
    for (auto& delay : internalDelay)
        delay.reset();

    for (auto& hp : internalHighpass)
        hp.reset();

    chorusPhase = 0.0f;
    lastMorph = 0.0f;
    lfo.reset();
}

void MorphModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    amountSmooth.setTarget (amount.get());
    trimSmooth.setTarget (dbToGain (bTrim.get()));
    curveSmooth.setTarget (juce::jlimit (0.0f, 1.0f, curve.get()));
    widthSmooth.setTarget (juce::jlimit (0.0f, 1.5f, bWidth.get()));

    auto* left = context.buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? context.buffer.getWritePointer (1) : nullptr;

    const auto useSidechain = source.getIndex() != 2
                           && context.sidechainActive
                           && context.sidechain != nullptr
                           && context.sidechain->getNumSamples() >= numSamples;

    const auto* scLeft = useSidechain ? context.sidechain->getReadPointer (0) : nullptr;
    const auto* scRight = useSidechain && context.sidechain->getNumChannels() > 1
                            ? context.sidechain->getReadPointer (1)
                            : scLeft;

    lfo.generate (numSamples);
    const auto lfoDepth = lfo.isEnabled() ? lfo.getMorphDepth() : 0.0f;
    const auto* lfoData = lfo.data();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;

        // The internal B voice is a soft, slowly detuned double. It makes the
        // morph and its LFO useful even when no sidechain is connected.
        chorusPhase = wrapPhase (chorusPhase + 0.083f / static_cast<float> (sampleRate));

        const auto delayL = (11.0f + 2.6f * std::sin (twoPi * chorusPhase)
                                   + 0.7f * std::sin (twoPi * chorusPhase * 0.37f))
                            * 0.001f * static_cast<float> (sampleRate);
        const auto delayR = (14.5f + 2.9f * std::sin (twoPi * wrapPhase (chorusPhase + 0.29f))
                                   + 0.9f * std::sin (twoPi * chorusPhase * 0.41f + 1.3f))
                            * 0.001f * static_cast<float> (sampleRate);

        internalDelay[0].write (dryLeft);
        internalDelay[1].write (dryRight);

        auto bLeft = internalHighpass[0].process (0.64f * internalDelay[0].readCubic (delayL) + 0.36f * dryLeft);
        auto bRight = internalHighpass[1].process (0.64f * internalDelay[1].readCubic (delayR) + 0.36f * dryRight);

        bLeft = fastTanh (bLeft * 1.1f) / 1.1f;
        bRight = fastTanh (bRight * 1.1f) / 1.1f;

        if (useSidechain)
        {
            bLeft = scLeft[i];
            bRight = scRight[i];
        }

        const auto trim = trimSmooth.next();
        bLeft *= trim;
        bRight *= trim;

        const auto width = widthSmooth.next();
        const auto mid = 0.5f * (bLeft + bRight);
        const auto side = 0.5f * (bLeft - bRight) * width;
        bLeft = mid + side;
        bRight = mid - side;

        const auto mod = lfoDepth * (lfoData != nullptr ? lfoData[i] : 0.0f);
        const auto morph = juce::jlimit (0.0f, 1.0f, amountSmooth.next() + mod);
        lastMorph = morph;

        const auto linearA = 1.0f - morph;
        const auto linearB = morph;
        const auto equalPowerA = std::cos (morph * halfPi);
        const auto equalPowerB = std::sin (morph * halfPi);
        const auto blend = curveSmooth.next();

        const auto gainA = linearA + blend * (equalPowerA - linearA);
        const auto gainB = linearB + blend * (equalPowerB - linearB);

        const auto outLeft = dryLeft * gainA + bLeft * gainB;
        const auto outRight = dryRight * gainA + bRight * gainB;

        left[i] = outLeft;

        if (right != nullptr)
            right[i] = outRight;
    }
}

} // namespace iridescent
