#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "SlotState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace iridescent
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * kPi;
constexpr float halfPi = 0.5f * kPi;

inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const auto x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float wrapPhase (float phase) noexcept
{
    return phase - std::floor (phase);
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain) noexcept
{
    return 20.0f * std::log10 (std::max (1.0e-7f, gain));
}

struct ParamRef
{
    std::atomic<float>* pointer = nullptr;

    void bind (juce::AudioProcessorValueTreeState& state, const char* id)
    {
        pointer = state.getRawParameterValue (id);
    }

    void bind (SlotState& state, const char* id)
    {
        pointer = state.getRawParameterValue (id);
    }

    float get() const noexcept
    {
        return pointer != nullptr ? pointer->load (std::memory_order_relaxed) : 0.0f;
    }
};

struct ChoiceRef
{
    std::atomic<float>* pointer = nullptr;

    void bind (juce::AudioProcessorValueTreeState& state, const char* id)
    {
        pointer = state.getRawParameterValue (id);
    }

    void bind (SlotState& state, const char* id)
    {
        pointer = state.getRawParameterValue (id);
    }

    int getIndex() const noexcept
    {
        return pointer != nullptr ? juce::roundToInt (pointer->load (std::memory_order_relaxed)) : 0;
    }
};

class Smooth
{
public:
    void prepare (double sampleRate, float rampSeconds = 0.03f)
    {
        smoother.reset (sampleRate, rampSeconds);
        smoother.setCurrentAndTargetValue (smoother.getTargetValue());
    }

    void setTarget (float value) noexcept { smoother.setTargetValue (value); }
    void snap (float value) noexcept { smoother.setCurrentAndTargetValue (value); }
    float next() noexcept { return smoother.getNextValue(); }
    float current() const noexcept { return smoother.getCurrentValue(); }
    void skip (int numSamples) noexcept { smoother.skip (numSamples); }

private:
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoother;
};

class OnePoleLowpass
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void setCutoff (float hz) noexcept
    {
        const auto safe = std::clamp (hz, 20.0f, static_cast<float> (sampleRate) * 0.45f);
        coefficient = 1.0f - std::exp (-twoPi * safe / static_cast<float> (sampleRate));
    }

    float process (float x) noexcept
    {
        state += coefficient * (x - state);
        return state;
    }

    void reset() noexcept { state = 0.0f; }

private:
    double sampleRate = 48000.0;
    float coefficient = 1.0f;
    float state = 0.0f;
};

class OnePoleHighpass
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void setCutoff (float hz) noexcept
    {
        const auto safe = std::clamp (hz, 10.0f, static_cast<float> (sampleRate) * 0.45f);
        pole = std::exp (-twoPi * safe / static_cast<float> (sampleRate));
    }

    float process (float x) noexcept
    {
        const auto y = x - previous + pole * output;
        previous = x;
        output = y;
        return y;
    }

    void reset() noexcept
    {
        previous = 0.0f;
        output = 0.0f;
    }

private:
    double sampleRate = 48000.0;
    float pole = 0.999f;
    float previous = 0.0f;
    float output = 0.0f;
};

class DcBlocker
{
public:
    void prepare (double newSampleRate, float cutoffHz = 12.0f)
    {
        pole = std::exp (-twoPi * cutoffHz / static_cast<float> (newSampleRate));
        reset();
    }

    float process (float x) noexcept
    {
        const auto y = x - previousInput + pole * previousOutput;
        previousInput = x;
        previousOutput = y;
        return y;
    }

    void reset() noexcept
    {
        previousInput = 0.0f;
        previousOutput = 0.0f;
    }

private:
    float pole = 0.999f;
    float previousInput = 0.0f;
    float previousOutput = 0.0f;
};

class DelayLine
{
public:
    void prepare (int maximumDelaySamples)
    {
        size = std::max (8, maximumDelaySamples + 8);
        buffer.assign (static_cast<size_t> (size), 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    void write (float sample) noexcept
    {
        buffer[static_cast<size_t> (writeIndex)] = sample;

        if (++writeIndex >= size)
            writeIndex = 0;
    }

    float readLinear (float delaySamples) const noexcept
    {
        const auto delay = std::clamp (delaySamples, 1.0f, static_cast<float> (size - 3));
        const auto position = static_cast<float> (writeIndex) - delay;
        auto base = static_cast<int> (std::floor (position));
        const auto fraction = position - static_cast<float> (base);
        const auto a = at (base);
        const auto b = at (base + 1);
        return a + (b - a) * fraction;
    }

    float readCubic (float delaySamples) const noexcept
    {
        const auto delay = std::clamp (delaySamples, 2.0f, static_cast<float> (size - 4));
        const auto position = static_cast<float> (writeIndex) - delay;
        auto base = static_cast<int> (std::floor (position));
        const auto fraction = position - static_cast<float> (base);

        const auto y0 = at (base - 1);
        const auto y1 = at (base);
        const auto y2 = at (base + 1);
        const auto y3 = at (base + 2);

        const auto a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const auto b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c = -0.5f * y0 + 0.5f * y2;
        const auto d = y1;
        return ((a * fraction + b) * fraction + c) * fraction + d;
    }

    int getSize() const noexcept { return size; }

private:
    float at (int index) const noexcept
    {
        while (index < 0)
            index += size;

        while (index >= size)
            index -= size;

        return buffer[static_cast<size_t> (index)];
    }

    int size = 0;
    int writeIndex = 0;
    std::vector<float> buffer;
};

class Allpass
{
public:
    void prepare (int delaySamples, float feedback)
    {
        size = std::max (2, delaySamples);
        buffer.assign (static_cast<size_t> (size), 0.0f);
        index = 0;
        gain = feedback;
    }

    void setGain (float newGain) noexcept { gain = std::clamp (newGain, 0.0f, 0.85f); }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        index = 0;
    }

    float process (float x) noexcept
    {
        const auto delayed = buffer[static_cast<size_t> (index)];
        const auto y = delayed - gain * x;
        buffer[static_cast<size_t> (index)] = x + gain * y;

        if (++index >= size)
            index = 0;

        return y;
    }

private:
    int size = 2;
    int index = 0;
    float gain = 0.5f;
    std::vector<float> buffer;
};

class Noise
{
public:
    explicit Noise (std::uint32_t seed = 0x9e3779b9u) : state (seed) {}

    float nextBipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state) * 2.3283064365e-10f * 2.0f - 1.0f;
    }

    float nextUnipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state) * 2.3283064365e-10f;
    }

private:
    std::uint32_t state;
};

} // namespace iridescent
