#pragma once

#include "Module.h"
#include "Utils.h"

namespace iridescent
{

class CompressorModule final : public Module
{
public:
    explicit CompressorModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

private:
    ParamRef threshold, ratio, attack, release, makeup, mix;

    Smooth makeupSmooth, mixSmooth;

    float envelope = 0.0f;
    float attackCoefficient = 0.99f;
    float releaseCoefficient = 0.999f;
    double sampleRate = 48000.0;
};

} // namespace iridescent
