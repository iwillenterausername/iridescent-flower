#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace iridescent
{

struct ProcessContext;

class Module
{
public:
    virtual ~Module() = default;

    virtual void prepare (double sampleRate, int maximumBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process (ProcessContext& context) = 0;
};

struct ProcessContext
{
    juce::AudioBuffer<float>& buffer;
    const juce::AudioBuffer<float>* sidechain = nullptr;
    const float* lfo = nullptr;
    const float* lfoQuad = nullptr;
    int numSamples = 0;
    float lfoDepth = 0.0f;
    bool lfoEnabled = false;
    bool sidechainActive = false;
    double bpm = 120.0;
};

} // namespace iridescent
