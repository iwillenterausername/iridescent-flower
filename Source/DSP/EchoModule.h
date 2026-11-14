#pragma once

#include "Module.h"
#include "Utils.h"

#include <array>

namespace iridescent
{

class EchoModule final : public Module
{
public:
    explicit EchoModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

private:
    ParamRef time, feedback, diffusion, tone, mix, pingpong, sync;
    ParamRef division;

    Smooth timeSmooth, feedbackSmooth, mixSmooth;

    std::array<DelayLine, 2> delays;
    std::array<std::array<Allpass, 4>, 2> diffusers;
    std::array<OnePoleLowpass, 2> feedbackTone;
    std::array<OnePoleLowpass, 2> outputTone;
    std::array<DcBlocker, 2> dcBlockers;

    double sampleRate = 48000.0;
};

} // namespace iridescent
