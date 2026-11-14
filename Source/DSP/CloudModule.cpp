#include "CloudModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

namespace
{
constexpr float baseDelays[8] =
{
    0.0297f, 0.0371f, 0.0411f, 0.0439f,
    0.0533f, 0.0599f, 0.0671f, 0.0719f
};

void hadamard8 (float* v) noexcept
{
    for (int step = 1; step < 8; step <<= 1)
    {
        for (int i = 0; i < 8; i += step * 2)
        {
            for (int j = 0; j < step; ++j)
            {
                const auto a = v[i + j];
                const auto b = v[i + j + step];
                v[i + j] = a + b;
                v[i + j + step] = a - b;
            }
        }
    }

    constexpr auto normalise = 0.35355339059f; // 1 / sqrt(8)

    for (int i = 0; i < 8; ++i)
        v[i] *= normalise;
}
} // namespace

CloudModule::CloudModule (SlotState& state)
{
    size.bind (state, ids::cloudSize);
    decay.bind (state, ids::cloudDecay);
    damping.bind (state, ids::cloudDamping);
    mod.bind (state, ids::cloudMod);
    predelay.bind (state, ids::cloudPredelay);
    lowcut.bind (state, ids::cloudLowcut);
    width.bind (state, ids::cloudWidth);
    mix.bind (state, ids::cloudMix);
}

void CloudModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto& line : lines)
        line.prepare (static_cast<int> (sampleRate * 0.22));

    for (auto& line : predelayLine)
        line.prepare (static_cast<int> (sampleRate * 0.12));

    constexpr float diffusionTimes[] = { 4.7f, 7.9f, 11.3f, 17.3f };

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < 4; ++i)
        {
            const auto samples = static_cast<int> ((diffusionTimes[i] + channel * 0.71f)
                                                   * 0.001f * static_cast<float> (sampleRate));
            inputDiffusion[static_cast<size_t> (channel)][static_cast<size_t> (i)]
                .prepare (std::max (2, samples), 0.62f);
        }
    }

    lowcutLeft.prepare (sampleRate);
    lowcutRight.prepare (sampleRate);

    sizeSmooth.prepare (sampleRate, 0.2f);
    decaySmooth.prepare (sampleRate, 0.2f);
    dampingSmooth.prepare (sampleRate, 0.12f);
    modSmooth.prepare (sampleRate, 0.12f);
    predelaySmooth.prepare (sampleRate, 0.06f);
    lowcutSmooth.prepare (sampleRate, 0.08f);
    widthSmooth.prepare (sampleRate, 0.06f);
    mixSmooth.prepare (sampleRate, 0.04f);

    sizeSmooth.snap (juce::jlimit (0.0f, 1.0f, size.get()));
    decaySmooth.snap (juce::jlimit (0.2f, 12.0f, decay.get()));
    dampingSmooth.snap (juce::jlimit (0.0f, 1.0f, damping.get()));
    modSmooth.snap (juce::jlimit (0.0f, 1.0f, mod.get()));
    predelaySmooth.snap (juce::jlimit (0.0f, 100.0f, predelay.get()) * 0.001f
                         * static_cast<float> (sampleRate));
    lowcutSmooth.snap (juce::jlimit (20.0f, 500.0f, lowcut.get()));
    widthSmooth.snap (juce::jlimit (0.0f, 1.5f, width.get()));
    mixSmooth.snap (juce::jlimit (0.0f, 1.0f, mix.get()));

    reset();
}

void CloudModule::reset()
{
    for (auto& line : lines)
        line.reset();

    for (auto& line : predelayLine)
        line.reset();

    for (auto& channel : inputDiffusion)
        for (auto& ap : channel)
            ap.reset();

    dampingState.fill (0.0f);

    for (int i = 0; i < numLines; ++i)
        modPhase[static_cast<size_t> (i)] = static_cast<float> (i) * 0.137f;

    lowcutLeft.reset();
    lowcutRight.reset();
}

void CloudModule::process (ProcessContext& context)
{
    const auto numSamples = context.numSamples;
    const auto numChannels = context.buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    sizeSmooth.setTarget (juce::jlimit (0.0f, 1.0f, size.get()));
    decaySmooth.setTarget (juce::jlimit (0.2f, 12.0f, decay.get()));
    dampingSmooth.setTarget (juce::jlimit (0.0f, 1.0f, damping.get()));
    modSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mod.get()));
    predelaySmooth.setTarget (juce::jlimit (0.0f, 100.0f, predelay.get()) * 0.001f
                              * static_cast<float> (sampleRate));
    lowcutSmooth.setTarget (juce::jlimit (20.0f, 500.0f, lowcut.get()));
    widthSmooth.setTarget (juce::jlimit (0.0f, 1.5f, width.get()));
    mixSmooth.setTarget (juce::jlimit (0.0f, 1.0f, mix.get()));

    const auto scale = 0.55f + 1.1f * sizeSmooth.current();
    const auto decayValue = decaySmooth.current();
    const auto dampingValue = dampingSmooth.current();
    const auto modValue = modSmooth.current();
    const auto predelaySamples = predelaySmooth.current();
    const auto lowcutHz = lowcutSmooth.current();

    std::array<float, numLines> delaySamples {};
    std::array<float, numLines> lineGain {};

    const auto averageDelay = 0.050f * scale;

    for (int i = 0; i < numLines; ++i)
    {
        delaySamples[static_cast<size_t> (i)] = baseDelays[i] * scale * static_cast<float> (sampleRate);
        const auto rt60 = std::pow (0.001f, averageDelay / std::max (0.1f, decayValue));
        lineGain[static_cast<size_t> (i)] = juce::jlimit (0.10f, 0.995f, rt60 + 0.045f);
    }

    const auto dampingCutoff = 11500.0f * std::pow (0.11f, dampingValue);
    const auto dampingCoefficient = 1.0f - std::exp (-twoPi * dampingCutoff
                                                     / static_cast<float> (sampleRate));
    const auto modDepth = modValue * 0.0032f * static_cast<float> (sampleRate);

    for (auto& channel : inputDiffusion)
        for (auto& ap : channel)
            ap.setGain (0.62f);

    lowcutLeft.setCutoff (lowcutHz);
    lowcutRight.setCutoff (lowcutHz);

    auto* left = context.buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? context.buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const auto dryLeft = left[n];
        const auto dryRight = right != nullptr ? right[n] : dryLeft;

        predelayLine[0].write (dryLeft);
        predelayLine[1].write (dryRight);

        auto preLeft = predelaySamples < 1.5f ? dryLeft : predelayLine[0].readCubic (predelaySamples);
        auto preRight = predelaySamples < 1.5f ? dryRight : predelayLine[1].readCubic (predelaySamples);

        for (auto& ap : inputDiffusion[0])
            preLeft = ap.process (preLeft);

        for (auto& ap : inputDiffusion[1])
            preRight = ap.process (preRight);

        std::array<float, numLines> delayed {};

        for (int i = 0; i < numLines; ++i)
        {
            const auto phase = modPhase[static_cast<size_t> (i)];
            const auto modulatedDelay = delaySamples[static_cast<size_t> (i)]
                                      + std::sin (twoPi * phase) * modDepth;
            delayed[static_cast<size_t> (i)] = lines[static_cast<size_t> (i)].readCubic (modulatedDelay);
            modPhase[static_cast<size_t> (i)] = wrapPhase (phase + (0.047f + 0.013f * i)
                                                           / static_cast<float> (sampleRate));
        }

        const auto mono = 0.5f * (preLeft + preRight);
        const auto side = 0.5f * (preLeft - preRight);

        auto feedback = delayed;
        hadamard8 (feedback.data());

        for (int i = 0; i < numLines; ++i)
        {
            const auto index = static_cast<size_t> (i);
            auto& state = dampingState[index];
            state += dampingCoefficient * (feedback[index] - state);
            const auto injection = mono * 0.68f
                                 + side * (i < 4 ? 0.24f : -0.24f);
            lines[index].write (state * lineGain[index] + injection);
        }

        auto wetLeft = 0.54f * (delayed[0] + delayed[2] + delayed[4] + delayed[6]);
        auto wetRight = 0.54f * (delayed[1] + delayed[3] + delayed[5] + delayed[7]);
        wetLeft = lowcutLeft.process (wetLeft + 0.08f * mono);
        wetRight = lowcutRight.process (wetRight + 0.08f * mono);

        if (right != nullptr)
        {
            const auto widthValue = widthSmooth.next();
            const auto mid = 0.5f * (wetLeft + wetRight);
            const auto sideWet = 0.5f * (wetLeft - wetRight) * widthValue;
            wetLeft = mid + sideWet;
            wetRight = mid - sideWet;
        }
        else
        {
            (void) widthSmooth.next();
        }

        const auto wetMix = mixSmooth.next();
        const auto outLeft = dryLeft * (1.0f - wetMix) + wetLeft * wetMix;
        const auto outRight = dryRight * (1.0f - wetMix) + wetRight * wetMix;

        left[n] = outLeft;

        if (right != nullptr)
            right[n] = outRight;
    }

    sizeSmooth.skip (numSamples);
    decaySmooth.skip (numSamples);
    dampingSmooth.skip (numSamples);
    modSmooth.skip (numSamples);
    predelaySmooth.skip (numSamples);
    lowcutSmooth.skip (numSamples);
}

} // namespace iridescent
