#pragma once

#include "Module.h"
#include "Utils.h"

namespace iridescent
{

class EqModule final : public Module
{
public:
    explicit EqModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept { z1 = z2 = 0.0f; }
        float process (float x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;

            if (std::abs (z1) < 1.0e-20f) z1 = 0.0f;
            if (std::abs (z2) < 1.0e-20f) z2 = 0.0f;
            return y;
        }
    };

private:
    void updateCoefficients();

    ParamRef lowGain, midGain, midFreq, highGain, output;
    Biquad lowL, lowR, midL, midR, highL, highR;
    double sampleRate = 48000.0;
};

} // namespace iridescent
