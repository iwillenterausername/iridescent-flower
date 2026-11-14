#include "BbdEngine.h"

#include <algorithm>
#include <cmath>

namespace copperline
{

namespace
{
constexpr float twoPi = 2.0f * kPi;
constexpr float halfPi = 0.5f * kPi;
constexpr float maximumDelaySeconds = 1.25f;
constexpr int virtualStages = 4096;

float wrapPhase (float phase) noexcept
{
    phase -= std::floor (phase);
    return phase;
}
} // namespace

BbdEngine::BbdEngine()
{
    outputDcLeft.prepare (sampleRate);
    outputDcRight.prepare (sampleRate);
}

void BbdEngine::prepare (double newSampleRate, int maximumBlockSize, int numberOfChannels)
{
    sampleRate = newSampleRate;
    maximumBlock = std::max (1, maximumBlockSize);
    activeChannels = std::clamp (numberOfChannels, 1, 2);

    leftVoice.prepare (sampleRate, maximumDelaySeconds);
    rightVoice.prepare (sampleRate, maximumDelaySeconds);
    compander.prepare (sampleRate);
    outputDcLeft.prepare (sampleRate);
    outputDcRight.prepare (sampleRate);

    timeSamples.reset (sampleRate, 0.12);
    feedbackAmount.reset (sampleRate, 0.03);
    mixAmount.reset (sampleRate, 0.03);
    toneAmount.reset (sampleRate, 0.05);
    driveGain.reset (sampleRate, 0.03);
    ageAmount.reset (sampleRate, 0.05);
    modulationDepthSamples.reset (sampleRate, 0.08);
    modulationRate.reset (sampleRate, 0.15);
    stereoWidth.reset (sampleRate, 0.05);
    outputGain.reset (sampleRate, 0.03);
    companderAmount.reset (sampleRate, 0.04);
    freezeAmount.reset (sampleRate, 0.02);

    const BbdParameters defaults;
    timeSamples.setCurrentAndTargetValue (defaults.timeMs * 0.001f * static_cast<float> (sampleRate));
    feedbackAmount.setCurrentAndTargetValue (defaults.feedback);
    mixAmount.setCurrentAndTargetValue (defaults.mix);
    toneAmount.setCurrentAndTargetValue (defaults.tone);
    driveGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (defaults.driveDb));
    ageAmount.setCurrentAndTargetValue (defaults.age);
    modulationDepthSamples.setCurrentAndTargetValue (defaults.modDepthMs * 0.001f * static_cast<float> (sampleRate));
    modulationRate.setCurrentAndTargetValue (defaults.modRateHz);
    stereoWidth.setCurrentAndTargetValue (defaults.width);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (defaults.levelDb));
    companderAmount.setCurrentAndTargetValue (defaults.compander ? 1.0f : 0.0f);
    freezeAmount.setCurrentAndTargetValue (defaults.freeze ? 1.0f : 0.0f);

    scopeStride = std::max (1, static_cast<int> (sampleRate / 16000.0));
    scopeCounter = 0;
    scopeWriteInternal = 0;
    scopeWriteIndex.store (0, std::memory_order_relaxed);

    for (auto& sample : scopeBuffer)
        sample.store (0.0f, std::memory_order_relaxed);

    reset();
}

void BbdEngine::reset()
{
    leftVoice.reset();
    rightVoice.reset();
    compander.reset();
    outputDcLeft.reset();
    outputDcRight.reset();

    lfoPhase = 0.0f;
    wowPhase1 = 0.0f;
    wowPhase2 = 0.0f;
    wowPhase3 = 0.37f;
    wowPhase4 = 0.11f;

    inputMeterValue = 0.0f;
    outputMeterValue = 0.0f;
    inputMeter.store (0.0f, std::memory_order_relaxed);
    outputMeter.store (0.0f, std::memory_order_relaxed);

    scopeCounter = 0;
    scopeWriteInternal = 0;
    scopeWriteIndex.store (0, std::memory_order_relaxed);

    for (auto& sample : scopeBuffer)
        sample.store (0.0f, std::memory_order_relaxed);
}

void BbdEngine::setParameters (const BbdParameters& p) noexcept
{
    timeSamples.setTargetValue (std::clamp (p.timeMs, 20.0f, 1200.0f) * 0.001f * static_cast<float> (sampleRate));
    feedbackAmount.setTargetValue (std::clamp (p.feedback, 0.0f, 1.05f));
    mixAmount.setTargetValue (std::clamp (p.mix, 0.0f, 1.0f));
    toneAmount.setTargetValue (std::clamp (p.tone, 0.0f, 1.0f));
    driveGain.setTargetValue (juce::Decibels::decibelsToGain (std::clamp (p.driveDb, -12.0f, 18.0f)));
    ageAmount.setTargetValue (std::clamp (p.age, 0.0f, 1.0f));
    modulationDepthSamples.setTargetValue (std::clamp (p.modDepthMs, 0.0f, 6.0f)
                                           * 0.001f * static_cast<float> (sampleRate));
    modulationRate.setTargetValue (std::clamp (p.modRateHz, 0.05f, 8.0f));
    stereoWidth.setTargetValue (std::clamp (p.width, 0.0f, 1.5f));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (std::clamp (p.levelDb, -24.0f, 12.0f)));
    companderAmount.setTargetValue (p.compander ? 1.0f : 0.0f);
    freezeAmount.setTargetValue (p.freeze ? 1.0f : 0.0f);
}

void BbdEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numberOfSamples = buffer.getNumSamples();
    const auto numberOfChannels = buffer.getNumChannels();

    if (numberOfSamples == 0 || numberOfChannels == 0)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = numberOfChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    float inputPeak = 0.0f;

    for (int channel = 0; channel < std::min (2, numberOfChannels); ++channel)
        inputPeak = std::max (inputPeak, buffer.getMagnitude (channel, 0, numberOfSamples));

    // The cutoff follows the delay time: long delays are clocked more slowly,
    // so the bucket brigade loses high end just like the real thing.
    const auto delayMsForFilter = std::max (20.0f, timeSamples.getCurrentValue()
                                                        / static_cast<float> (sampleRate) * 1000.0f);
    const auto ageForFilter = ageAmount.getCurrentValue();
    const auto toneForFilter = toneAmount.getCurrentValue();

    const auto timeNorm = std::clamp (std::log (delayMsForFilter / 20.0f) / std::log (60.0f), 0.0f, 1.0f);
    auto cutoff = 1400.0f * std::pow (18.0f, toneForFilter);
    cutoff *= 1.0f - 0.5f * timeNorm;
    cutoff *= 1.0f - 0.38f * ageForFilter;
    cutoff = std::clamp (cutoff, 320.0f, static_cast<float> (sampleRate) * 0.43f);

    leftVoice.setCutoffs (cutoff * 1.1f, cutoff * 0.72f, cutoff);
    rightVoice.setCutoffs (cutoff * 1.1f, cutoff * 0.72f, cutoff);

    const auto noiseFloor = 0.0022f;
    const auto scopeMonoScale = right != nullptr ? 0.5f : 1.0f;

    for (int sample = 0; sample < numberOfSamples; ++sample)
    {
        const auto dryLeft = left[sample];
        const auto dryRight = right != nullptr ? right[sample] : dryLeft;

        const auto freeze = freezeAmount.getNextValue();
        const auto time = timeSamples.getNextValue();
        const auto feedback = std::max (feedbackAmount.getNextValue(), freeze);
        const auto age = ageAmount.getNextValue();
        const auto drive = driveGain.getNextValue();
        const auto companderMix = companderAmount.getNextValue();

        lfoPhase = wrapPhase (lfoPhase + modulationRate.getNextValue() / static_cast<float> (sampleRate));

        const auto modDepth = modulationDepthSamples.getNextValue();
        const auto lfoLeft = std::sin (twoPi * lfoPhase) * modDepth;
        const auto lfoRight = std::sin (twoPi * wrapPhase (lfoPhase + 0.25f)) * modDepth;

        // Slow wow and a faster flutter. Both are tied to the "age" control.
        wowPhase1 = wrapPhase (wowPhase1 + 0.17f / static_cast<float> (sampleRate));
        wowPhase2 = wrapPhase (wowPhase2 + 4.3f / static_cast<float> (sampleRate));
        wowPhase3 = wrapPhase (wowPhase3 + 0.13f / static_cast<float> (sampleRate));
        wowPhase4 = wrapPhase (wowPhase4 + 5.1f / static_cast<float> (sampleRate));

        const auto wowDepth = age * 0.0012f * static_cast<float> (sampleRate);
        const auto wowLeft = (std::sin (twoPi * wowPhase1) + 0.28f * std::sin (twoPi * wowPhase2))
                             * wowDepth * 0.5f;
        const auto wowRight = (std::sin (twoPi * wowPhase3) + 0.28f * std::sin (twoPi * wowPhase4))
                              * wowDepth * 0.5f;

        auto wetLeft = dryLeft * (1.0f - freeze);
        auto wetRight = dryRight * (1.0f - freeze);

        compander.processInput (wetLeft, wetRight, companderMix);

        const auto noiseAmplitude = age * age * noiseFloor;
        const auto noiseLeft = nextNoise() * noiseAmplitude;
        const auto noiseRight = nextNoise() * noiseAmplitude;

        const auto inputSaturation = age * 0.75f;
        const auto feedbackSaturation = age * 0.9f;

        auto delayedLeft = leftVoice.process (wetLeft,
                                              time + lfoLeft + wowLeft,
                                              drive,
                                              feedback,
                                              inputSaturation,
                                              feedbackSaturation,
                                              noiseLeft);

        auto delayedRight = rightVoice.process (wetRight,
                                                time + lfoRight + wowRight,
                                                drive,
                                                feedback,
                                                inputSaturation,
                                                feedbackSaturation,
                                                noiseRight);

        compander.processOutput (delayedLeft, delayedRight, companderMix);

        delayedLeft = outputDcLeft.process (delayedLeft);
        delayedRight = outputDcRight.process (delayedRight);

        if (right != nullptr)
        {
            const auto width = stereoWidth.getNextValue();
            const auto mid = 0.5f * (delayedLeft + delayedRight);
            const auto side = 0.5f * (delayedLeft - delayedRight) * width;
            delayedLeft = mid + side;
            delayedRight = mid - side;
        }
        else
        {
            (void) stereoWidth.getNextValue();
        }

        const auto mix = mixAmount.getNextValue();
        const auto dryGain = std::cos (mix * halfPi);
        const auto wetGain = std::sin (mix * halfPi);
        const auto level = outputGain.getNextValue();

        auto outLeft = (dryLeft * dryGain + delayedLeft * wetGain) * level;
        auto outRight = (dryRight * dryGain + delayedRight * wetGain) * level;

        if (! std::isfinite (outLeft)) outLeft = 0.0f;
        if (! std::isfinite (outRight)) outRight = 0.0f;

        left[sample] = outLeft;
        if (right != nullptr)
            right[sample] = outRight;

        if (++scopeCounter >= scopeStride)
        {
            scopeCounter = 0;
            const auto mono = (outLeft + outRight) * scopeMonoScale;
            scopeBuffer[static_cast<size_t> (scopeWriteInternal)].store (mono, std::memory_order_relaxed);
            if (++scopeWriteInternal >= scopeSize)
                scopeWriteInternal = 0;
        }
    }

    scopeWriteIndex.store (scopeWriteInternal, std::memory_order_relaxed);

    const auto outputPeak = [&]
    {
        float peak = 0.0f;
        for (int channel = 0; channel < std::min (2, numberOfChannels); ++channel)
            peak = std::max (peak, buffer.getMagnitude (channel, 0, numberOfSamples));
        return peak;
    }();

    inputMeterValue = std::max (inputPeak, inputMeterValue * 0.82f);
    outputMeterValue = std::max (outputPeak, outputMeterValue * 0.82f);
    inputMeter.store (inputMeterValue, std::memory_order_relaxed);
    outputMeter.store (outputMeterValue, std::memory_order_relaxed);

    const auto currentDelayMs = timeSamples.getCurrentValue() / static_cast<float> (sampleRate) * 1000.0f;
    updateDisplayValues (currentDelayMs);
}

float BbdEngine::nextNoise() noexcept
{
    noiseStateLeft ^= noiseStateLeft << 13;
    noiseStateLeft ^= noiseStateLeft >> 17;
    noiseStateLeft ^= noiseStateLeft << 5;

    noiseStateRight ^= noiseStateRight << 17;
    noiseStateRight ^= noiseStateRight >> 13;
    noiseStateRight ^= noiseStateRight << 7;

    const auto a = static_cast<float> (noiseStateLeft) * 2.3283064365e-10f;
    const auto b = static_cast<float> (noiseStateRight) * 2.3283064365e-10f;
    return (a + b) - 1.0f;
}

void BbdEngine::updateDisplayValues (float delayMs) noexcept
{
    displayDelayMs.store (delayMs, std::memory_order_relaxed);

    const auto seconds = std::max (0.001f, delayMs * 0.001f);
    const auto clock = static_cast<float> (virtualStages) / (2.0f * seconds);
    displayClockHz.store (clock, std::memory_order_relaxed);
}

void BbdEngine::copyScope (float* destination, int numberOfSamples) const noexcept
{
    if (destination == nullptr || numberOfSamples <= 0)
        return;

    numberOfSamples = std::min (numberOfSamples, scopeSize);
    auto end = scopeWriteIndex.load (std::memory_order_relaxed);
    auto start = end - numberOfSamples;

    while (start < 0)
        start += scopeSize;

    for (int i = 0; i < numberOfSamples; ++i)
        destination[i] = scopeBuffer[static_cast<size_t> ((start + i) % scopeSize)].load (std::memory_order_relaxed);
}

} // namespace copperline
