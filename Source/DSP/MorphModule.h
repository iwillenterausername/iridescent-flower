#pragma once

#include "Module.h"
#include "Utils.h"
#include "LfoModule.h"

#include <array>

namespace iridescent
{

class MorphModule final : public Module
{
public:
    explicit MorphModule (juce::AudioProcessorValueTreeState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

    float getLastMorph() const noexcept { return lastMorph; }
    float getLfoValue() const noexcept { return lfo.getLastValue(); }

private:
    LfoModule lfo;
    ParamRef amount, bTrim, curve, bWidth;
    ChoiceRef source;

    Smooth amountSmooth, trimSmooth, curveSmooth, widthSmooth;

    std::array<DelayLine, 2> internalDelay;
    std::array<OnePoleHighpass, 2> internalHighpass;

    float chorusPhase = 0.0f;
    float lastMorph = 0.0f;
    double sampleRate = 48000.0;
};

} // namespace iridescent
