#pragma once

#include "Module.h"
#include "Utils.h"

namespace iridescent
{

class DistortionModule final : public Module
{
public:
    explicit DistortionModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

private:
    ParamRef drive, tone, bits, downsample, mix;
    ChoiceRef mode;

    Smooth driveSmooth, mixSmooth;
    OnePoleLowpass toneL, toneR;
    float heldL = 0.0f, heldR = 0.0f;
    int holdCounter = 0;
    double sampleRate = 48000.0;
};

} // namespace iridescent
