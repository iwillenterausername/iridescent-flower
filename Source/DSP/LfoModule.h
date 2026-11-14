#pragma once

#include "Module.h"
#include "Utils.h"

#include <vector>

namespace iridescent
{

class LfoModule final : public Module
{
public:
    explicit LfoModule (juce::AudioProcessorValueTreeState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

    void generate (int numSamples);

    const float* data() const noexcept { return values.data(); }
    const float* quadData() const noexcept { return quadValues.data(); }
    float getMorphDepth() const noexcept { return enabledValue > 0.5f ? depthValue : 0.0f; }
    bool isEnabled() const noexcept { return enabledValue > 0.5f; }
    float getLastValue() const noexcept { return lastValue; }

private:
    ParamRef rate, depth, phase, pan, stereo, on;
    ChoiceRef shape;

    Smooth rateSmooth, panSmooth, stereoSmooth, depthSmooth;
    Noise noise { 0x51f15e5du };

    std::vector<float> values;
    std::vector<float> quadValues;

    float freePhase = 0.0f;
    float phaseOffset = 0.0f;
    float randomCurrent = 0.0f;
    float randomTarget = 0.0f;
    float lastValue = 0.0f;
    float depthValue = 0.0f;
    float enabledValue = 1.0f;
    int lastShape = 0;
    double sampleRate = 48000.0;
};

} // namespace iridescent
