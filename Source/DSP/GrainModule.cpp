#include "GrainModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

GrainModule::GrainModule (SlotState& state)
{
    size.bind (state, ids::grainSize);
    density.bind (state, ids::grainDensity);
    pitch.bind (state, ids::grainPitch);
    spray.bind (state, ids::grainSpray);
    feedback.bind (state, ids::grainFeedback);
    mix.bind (state, ids::grainMix);
    reverse.bind (state, ids::grainReverse);
    width.bind (state, ids::grainWidth);
    freeze.bind (state, ids::grainFreeze);
    sync.bind (state, ids::grainSync);
    division.bind (state, ids::grainDivision);
    buffer.bind (state, ids::grainBuffer);
    offset.bind (state, ids::grainOffset);
    rate.bind (state, ids::grainRate);
    snap.bind (state, ids::grainSnap);
}

void GrainModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    bufferSize = static_cast<int> (sampleRate * 3.0) + 8;
    bufferL.assign (static_cast<size_t> (bufferSize), 0.0f);
    bufferR.assign (static_cast<size_t> (bufferSize), 0.0f);

    sizeSmooth.prepare (sampleRate, 0.08f);
    densitySmooth.prepare (sampleRate, 0.08f);
    feedbackSmooth.prepare (sampleRate, 0.04f);
    mixSmooth.prepare (sampleRate, 0.04f);
    widthSmooth.prepare (sampleRate, 0.06f);
    reverseSmooth.prepare (sampleRate, 0.06f);

    sizeSmooth.snap (juce::jlimit (10.0f, 500.0f, size.get()));
    densitySmooth.snap (juce::jlimit (1.0f, 60.0f, density.get()));
    feedbackSmooth.snap (juce::jlimit (0.0f, 0.9f, feedback.get()));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));
    widthSmooth.snap (juce::jlimit (0.0f, 1.0f, width.get()));
    reverseSmooth.snap (juce::jlimit (0.0f, 1.0f, reverse.get()));

    toneL.prepare (sampleRate);
    toneR.prepare (sampleRate);
    toneL.setCutoff (12500.0f);
    toneR.setCutoff (12500.0f);
    dcL.prepare (sampleRate, 8.0f);
    dcR.prepare (sampleRate, 8.0f);

    reset();
}

void GrainModule::reset()
{
    std::fill (bufferL.begin(), bufferL.end(), 0.0f);
    std::fill (bufferR.begin(), bufferR.end(), 0.0f);

    for (auto& grain : grains)
        grain.active = false;

    writeIndex = 0;
    samplesWritten = 0;
    samplesUntilNextGrain = 0.0f;
    lastWetL = 0.0f;
    lastWetR = 0.0f;
    toneL.reset();
    toneR.reset();
    dcL.reset();
    dcR.reset();
    activeGrainCount.store (0, std::memory_order_relaxed);
}

int GrainModule::wrapIndex (int index) const noexcept
{
    while (index < 0)
        index += bufferSize;

    while (index >= bufferSize)
        index -= bufferSize;

    return index;
}

float GrainModule::readBuffer (const std::vector<float>& buffer, float position) const noexcept
{
    auto base = static_cast<int> (std::floor (position));
    const auto fraction = position - static_cast<float> (base);

    const auto y1 = buffer[static_cast<size_t> (wrapIndex (base))];
    const auto y2 = buffer[static_cast<size_t> (wrapIndex (base + 1))];
    return y1 + (y2 - y1) * fraction;
}

void GrainModule::spawnGrain (float sizeSamples,
                               float pitchSemitones,
                               float sprayValue,
                               float reverseValue,
                               float widthValue,
                               float bufferSeconds,
                               float offsetValue,
                               int snapValue)
{
    Grain* slot = nullptr;

    for (auto& grain : grains)
    {
        if (! grain.active)
        {
            slot = &grain;
            break;
        }
    }

    if (slot == nullptr)
    {
        // Steal the grain that has been alive the longest.
        slot = &grains[0];

        for (auto& grain : grains)
            if (grain.active && grain.age > slot->age)
                slot = &grain;
    }

    const auto pitchRatio = std::pow (2.0f, juce::jlimit (-24.0f, 24.0f, pitchSemitones) / 12.0f);
    const auto reversed = noise.nextUnipolar() < juce::jlimit (0.0f, 1.0f, reverseValue);
    // Rate is a grain-trigger rate, not a playback-speed control. Only pitch
    // changes the read speed, so the displayed pitch always matches the audio.
    const auto increment = reversed ? -pitchRatio : pitchRatio;
    const auto span = sizeSamples * std::abs (increment);
    const auto windowSamples = juce::jlimit (0.1f, 3.0f, bufferSeconds)
                             * static_cast<float> (sampleRate);

    if (static_cast<float> (samplesWritten) < windowSamples + span + 8.0f)
        return;

    const auto spraySamples = juce::jlimit (0.0f, 1.0f, sprayValue) * 0.55f * static_cast<float> (sampleRate);
    const auto jitter = noise.nextUnipolar() * spraySamples;
    auto start = static_cast<float> (writeIndex) - windowSamples
               + juce::jlimit (0.0f, 1.0f, offsetValue) * windowSamples
               + jitter;

    if (snapValue > 0)
    {
        const auto grid = windowSamples / (snapValue == 1 ? 16.0f
                                          : (snapValue == 2 ? 8.0f : 4.0f));
        start = std::round (start / grid) * grid;
    }

    while (start < 0.0f)
        start += static_cast<float> (bufferSize);

    while (start >= static_cast<float> (bufferSize))
        start -= static_cast<float> (bufferSize);

    const auto pan = 0.5f + 0.5f * (noise.nextUnipolar() * 2.0f - 1.0f) * juce::jlimit (0.0f, 1.0f, widthValue);
    const auto angle = pan * halfPi;

    slot->active = true;
    slot->readPos = start;
    slot->increment = increment;
    slot->age = 0;
    slot->length = std::max (16, static_cast<int> (sizeSamples));
    slot->panL = std::cos (angle);
    slot->panR = std::sin (angle);

    // When a grain reads leftwards its starting point is close to the write
    // head; the wrapping helper handles a buffer boundary underneath it.
}

void GrainModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    sizeSmooth.setTarget (juce::jlimit (10.0f, 500.0f, size.get()));
    densitySmooth.setTarget (juce::jlimit (1.0f, 60.0f, density.get()));
    feedbackSmooth.setTarget (juce::jlimit (0.0f, 0.9f, feedback.get()));
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));
    widthSmooth.setTarget (juce::jlimit (0.0f, 1.0f, width.get()));
    reverseSmooth.setTarget (juce::jlimit (0.0f, 1.0f, reverse.get()));

    const auto freezeValue = freeze.get() > 0.5f;
    auto* left = context.buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? context.buffer.getWritePointer (1) : nullptr;

    const auto pitchValue = pitch.get();
    const auto sprayValue = spray.get();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryLeft = left[i];
        const auto dryRight = right != nullptr ? right[i] : dryLeft;

        const auto sizeSamples = sizeSmooth.next() * 0.001f * static_cast<float> (sampleRate);
        const auto densityValue = densitySmooth.next();
        const auto rateValue = juce::jlimit (0.25f, 4.0f, rate.get());
        auto interval = static_cast<float> (sampleRate) / std::max (0.001f, densityValue * rateValue);

        if (sync.get() > 0.5f)
        {
            const auto seconds = divisionBeats (juce::roundToInt (division.get())) * 60.0f
                               / static_cast<float> (juce::jmax (20.0, context.bpm));
            interval = juce::jmax (1.0f, seconds * static_cast<float> (sampleRate));
        }

        const auto reverseValue = reverseSmooth.next();
        const auto widthValue = widthSmooth.next();
        const auto feedbackValue = feedbackSmooth.next();
        const auto gain = 0.9f / std::sqrt (std::max (1.0f, sizeSamples * densityValue
                                                             / static_cast<float> (sampleRate)));

        if (! freezeValue)
        {
            const auto writeLeft = fastTanh (dryLeft + feedbackValue * lastWetL);
            const auto writeRight = fastTanh (dryRight + feedbackValue * lastWetR);
            bufferL[static_cast<size_t> (writeIndex)] = writeLeft;
            bufferR[static_cast<size_t> (writeIndex)] = writeRight;

            if (++writeIndex >= bufferSize)
                writeIndex = 0;

            ++samplesWritten;
        }

        samplesUntilNextGrain -= 1.0f;

        if (samplesUntilNextGrain <= 0.0f)
        {
            spawnGrain (sizeSamples, pitchValue, sprayValue, reverseValue, widthValue,
                        buffer.get(), offset.get(), snap.getIndex());
            samplesUntilNextGrain += interval;
        }

        auto wetLeft = 0.0f;
        auto wetRight = 0.0f;

        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;

            const auto phase = static_cast<float> (grain.age) / static_cast<float> (grain.length);
            const auto window = 0.5f - 0.5f * std::cos (twoPi * phase);
            const auto sampleL = readBuffer (bufferL, grain.readPos);
            const auto sampleR = readBuffer (bufferR, grain.readPos);

            wetLeft += sampleL * window * grain.panL;
            wetRight += sampleR * window * grain.panR;

            grain.readPos += grain.increment;

            if (grain.readPos >= static_cast<float> (bufferSize))
                grain.readPos -= static_cast<float> (bufferSize);
            else if (grain.readPos < 0.0f)
                grain.readPos += static_cast<float> (bufferSize);

            if (++grain.age >= grain.length)
                grain.active = false;
        }

        wetLeft = dcL.process (toneL.process (wetLeft * gain));
        wetRight = dcR.process (toneR.process (wetRight * gain));
        lastWetL = wetLeft;
        lastWetR = wetRight;

        const auto wetMix = mixSmooth.next();
        const auto outLeft = dryLeft * (1.0f - wetMix) + wetLeft * wetMix;
        const auto outRight = dryRight * (1.0f - wetMix) + wetRight * wetMix;

        left[i] = outLeft;

        if (right != nullptr)
            right[i] = outRight;
    }

    int activeNow = 0;
    for (const auto& grain : grains)
        if (grain.active)
            ++activeNow;

    activeGrainCount.store (activeNow, std::memory_order_relaxed);
}

} // namespace iridescent
