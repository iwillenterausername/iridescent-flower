#include "EqModule.h"
#include "ModuleCatalog.h"

namespace iridescent
{

namespace
{
void setLowShelf (EqModule::Biquad& b, double sampleRate, float frequency, float gainDb)
{
    const auto A = std::pow (10.0f, gainDb / 40.0f);
    const auto w0 = juce::MathConstants<float>::twoPi * frequency / static_cast<float> (sampleRate);
    const auto cosw = std::cos (w0);
    const auto sinw = std::sin (w0);
    const auto alpha = sinw / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.7071f - 1.0f) + 2.0f);
    const auto beta = 2.0f * std::sqrt (A) * alpha;
    const auto a0 = (A + 1.0f) + (A - 1.0f) * cosw + beta;

    b.b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw + beta) / a0;
    b.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
    b.b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw - beta) / a0;
    b.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
    b.a2 = ((A + 1.0f) + (A - 1.0f) * cosw - beta) / a0;
}

void setHighShelf (EqModule::Biquad& b, double sampleRate, float frequency, float gainDb)
{
    const auto A = std::pow (10.0f, gainDb / 40.0f);
    const auto w0 = juce::MathConstants<float>::twoPi * frequency / static_cast<float> (sampleRate);
    const auto cosw = std::cos (w0);
    const auto sinw = std::sin (w0);
    const auto alpha = sinw / 2.0f * std::sqrt ((A + 1.0f / A) * (1.0f / 0.7071f - 1.0f) + 2.0f);
    const auto beta = 2.0f * std::sqrt (A) * alpha;
    const auto a0 = (A + 1.0f) - (A - 1.0f) * cosw + beta;

    b.b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw + beta) / a0;
    b.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
    b.b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw - beta) / a0;
    b.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
    b.a2 = ((A + 1.0f) - (A - 1.0f) * cosw - beta) / a0;
}

void setPeak (EqModule::Biquad& b, double sampleRate, float frequency, float gainDb, float q)
{
    const auto A = std::pow (10.0f, gainDb / 40.0f);
    const auto w0 = juce::MathConstants<float>::twoPi * frequency / static_cast<float> (sampleRate);
    const auto cosw = std::cos (w0);
    const auto alpha = std::sin (w0) / (2.0f * q);
    const auto a0 = 1.0f + alpha / A;

    b.b0 = (1.0f + alpha * A) / a0;
    b.b1 = -2.0f * cosw / a0;
    b.b2 = (1.0f - alpha * A) / a0;
    b.a1 = -2.0f * cosw / a0;
    b.a2 = (1.0f - alpha / A) / a0;
}
} // namespace

EqModule::EqModule (SlotState& state)
{
    lowGain.bind (state, ids::eqLow);
    midGain.bind (state, ids::eqMid);
    midFreq.bind (state, ids::eqMidFreq);
    highGain.bind (state, ids::eqHigh);
    output.bind (state, ids::eqOutput);
}

void EqModule::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    updateCoefficients();
    reset();
}

void EqModule::reset()
{
    for (auto* b : { &lowL, &lowR, &midL, &midR, &highL, &highR })
        b->reset();
}

void EqModule::updateCoefficients()
{
    const auto midFrequency = juce::jlimit (200.0f, 8000.0f, midFreq.get());
    const auto lowDb = juce::jlimit (-18.0f, 18.0f, lowGain.get());
    const auto midDb = juce::jlimit (-18.0f, 18.0f, midGain.get());
    const auto highDb = juce::jlimit (-18.0f, 18.0f, highGain.get());

    setLowShelf (lowL, sampleRate, 120.0f, lowDb);
    setLowShelf (lowR, sampleRate, 120.0f, lowDb);
    setPeak (midL, sampleRate, midFrequency, midDb, 0.8f);
    setPeak (midR, sampleRate, midFrequency, midDb, 0.8f);
    setHighShelf (highL, sampleRate, 8000.0f, highDb);
    setHighShelf (highR, sampleRate, 8000.0f, highDb);
}

void EqModule::process (ProcessContext& context)
{
    if (context.numSamples <= 0 || context.buffer.getNumChannels() <= 0)
        return;

    updateCoefficients();

    auto* left = context.buffer.getWritePointer (0);
    auto* right = context.buffer.getNumChannels() > 1 ? context.buffer.getWritePointer (1) : nullptr;
    const auto makeUp = dbToGain (juce::jlimit (-18.0f, 18.0f, output.get()));

    for (int i = 0; i < context.numSamples; ++i)
    {
        auto l = lowL.process (midL.process (highL.process (left[i]))) * makeUp;
        left[i] = l;

        if (right != nullptr)
            right[i] = lowR.process (midR.process (highR.process (right[i]))) * makeUp;
    }
}

} // namespace iridescent
