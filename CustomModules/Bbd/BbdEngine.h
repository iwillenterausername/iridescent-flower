#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

#include "BbdVoice.h"
#include "Compander.h"

namespace copperline
{

struct BbdParameters
{
    float timeMs = 380.0f;
    float feedback = 0.42f;
    float mix = 0.32f;
    float tone = 0.62f;
    float driveDb = 0.0f;
    float age = 0.28f;
    float modDepthMs = 1.2f;
    float modRateHz = 0.55f;
    float width = 1.0f;
    float levelDb = 0.0f;
    bool compander = true;
    bool freeze = false;
};

class BbdEngine
{
public:
    BbdEngine();

    void prepare (double sampleRate, int maximumBlockSize, int numberOfChannels);
    void reset();

    void setParameters (const BbdParameters& newParameters) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getDisplayDelayMs() const noexcept { return displayDelayMs.load (std::memory_order_relaxed); }
    float getDisplayClockHz() const noexcept { return displayClockHz.load (std::memory_order_relaxed); }
    float getInputMeter() const noexcept { return inputMeter.load (std::memory_order_relaxed); }
    float getOutputMeter() const noexcept { return outputMeter.load (std::memory_order_relaxed); }

    void copyScope (float* destination, int numberOfSamples) const noexcept;

private:
    static constexpr int scopeSize = 256;

    float nextNoise() noexcept;
    void updateDisplayValues (float delayMs) noexcept;

    double sampleRate = 48000.0;
    int maximumBlock = 512;
    int activeChannels = 2;

    BbdVoice leftVoice;
    BbdVoice rightVoice;
    Compander compander;
    DcBlocker outputDcLeft;
    DcBlocker outputDcRight;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> timeSamples;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ageAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modulationDepthSamples;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modulationRate;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> stereoWidth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> companderAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freezeAmount;

    float lfoPhase = 0.0f;
    float wowPhase1 = 0.0f;
    float wowPhase2 = 0.0f;
    float wowPhase3 = 0.0f;
    float wowPhase4 = 0.0f;

    uint32_t noiseStateLeft = 0x12345678u;
    uint32_t noiseStateRight = 0x9e3779b9u;

    float inputMeterValue = 0.0f;
    float outputMeterValue = 0.0f;
    int scopeStride = 4;
    int scopeCounter = 0;
    int scopeWriteInternal = 0;

    std::array<std::atomic<float>, scopeSize> scopeBuffer {};
    std::atomic<int> scopeWriteIndex { 0 };

    std::atomic<float> displayDelayMs { 380.0f };
    std::atomic<float> displayClockHz { 5400.0f };
    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BbdEngine)
};

} // namespace copperline
