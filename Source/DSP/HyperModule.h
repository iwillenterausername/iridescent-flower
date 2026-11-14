#pragma once

#include "Module.h"
#include "Utils.h"

namespace iridescent
{

class HyperModule final : public Module
{
public:
    explicit HyperModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

private:
    ParamRef detune, width, rate, mix;
    Smooth detuneSmooth, widthSmooth, rateSmooth, mixSmooth;
    DelayLine delayL, delayR;
    float phase = 0.0f;
    double sampleRate = 48000.0;
};

} // namespace iridescent
