#pragma once

#include "Module.h"
#include "Utils.h"

namespace iridescent
{

class FilterModule final : public Module
{
public:
    explicit FilterModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

    struct SvfState
    {
        float ic1 = 0.0f;
        float ic2 = 0.0f;
        void reset() noexcept { ic1 = ic2 = 0.0f; }
    };

private:
    ParamRef cutoff, resonance, drive, mix;
    ChoiceRef type;

    Smooth cutoffSmooth, resonanceSmooth, driveSmooth, mixSmooth;

    SvfState leftStages[2];
    SvfState rightStages[2];
    double sampleRate = 48000.0;
};

} // namespace iridescent
