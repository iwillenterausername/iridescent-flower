#pragma once

#include "Module.h"
#include "Utils.h"

#include <array>

namespace iridescent
{

class CloudModule final : public Module
{
public:
    explicit CloudModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

private:
    static constexpr int numLines = 8;

    ParamRef size, decay, damping, mod, predelay, lowcut, width, mix;

    Smooth sizeSmooth, decaySmooth, dampingSmooth, modSmooth, predelaySmooth,
           lowcutSmooth, widthSmooth, mixSmooth;

    std::array<DelayLine, numLines> lines;
    std::array<DelayLine, 2> predelayLine;
    std::array<std::array<Allpass, 4>, 2> inputDiffusion;
    std::array<float, numLines> dampingState {};
    std::array<float, numLines> modPhase {};

    OnePoleHighpass lowcutLeft, lowcutRight;

    double sampleRate = 48000.0;
};

} // namespace iridescent
