#pragma once

#include "Module.h"
#include "Utils.h"

namespace iridescent
{

class ChorusModule final : public Module
{
public:
    explicit ChorusModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

private:
    ParamRef rate, depth, feedback, mix;
    ChoiceRef mode;

    Smooth rateSmooth, depthSmooth, feedbackSmooth, mixSmooth;
    DelayLine delayL, delayR;
    OnePoleLowpass toneL, toneR;
    float phase = 0.0f;
    double sampleRate = 48000.0;
};

} // namespace iridescent
