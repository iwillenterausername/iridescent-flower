#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace copperline
{

constexpr float kPi = 3.14159265358979323846f;

/** A cheap, stable tanh approximation. */
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const auto x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

class OnePoleLowpass
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void setCutoff (float cutoffHz) noexcept
    {
        const auto nyquistGuard = static_cast<float> (sampleRate) * 0.45f;
        cutoffHz = std::clamp (cutoffHz, 20.0f, nyquistGuard);
        coefficient = 1.0f - std::exp (-kPi * 2.0f * cutoffHz / static_cast<float> (sampleRate));
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

class DcBlocker
{
public:
    void prepare (double newSampleRate, float cutoffHz = 12.0f) noexcept
    {
        // A one-pole high-pass is enough here; it is inside the feedback loop
        // and must not add meaningful phase shift around the delay time.
        const auto x = std::exp (-kPi * 2.0f * cutoffHz / static_cast<float> (newSampleRate));
        pole = std::clamp (x, 0.0f, 0.99999f);
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

/**
    One channel of the virtual bucket-brigade line.

    The audio is stored in a normal fractional delay line, but the line is
    surrounded by the things that make a BBD sound like a BBD: a bandwidth
    limit that follows the clock rate, soft charge-transfer saturation, a
    damped feedback path and the compander (handled by the engine).
*/
class BbdVoice
{
public:
    void prepare (double newSampleRate, float maximumDelaySeconds)
    {
        sampleRate = newSampleRate;
        maxDelaySamples = std::max (8.0f, maximumDelaySeconds * static_cast<float> (sampleRate));
        bufferSize = static_cast<int> (std::ceil (maxDelaySamples)) + 8;

        buffer.assign (static_cast<size_t> (bufferSize), 0.0f);

        inputLp1.prepare (sampleRate);
        inputLp2.prepare (sampleRate);
        feedbackLp.prepare (sampleRate);
        outputLp1.prepare (sampleRate);
        outputLp2.prepare (sampleRate);

        inputDc.prepare (sampleRate);
        feedbackDc.prepare (sampleRate, 9.0f);
        outputDc.prepare (sampleRate);

        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        inputLp1.reset();
        inputLp2.reset();
        feedbackLp.reset();
        outputLp1.reset();
        outputLp2.reset();
        inputDc.reset();
        feedbackDc.reset();
        outputDc.reset();
    }

    void setCutoffs (float inputHz, float feedbackHz, float outputHz) noexcept
    {
        inputLp1.setCutoff (inputHz);
        inputLp2.setCutoff (inputHz);
        feedbackLp.setCutoff (feedbackHz);
        outputLp1.setCutoff (outputHz);
        outputLp2.setCutoff (outputHz);
    }

    float process (float input,
                   float delaySamples,
                   float driveGain,
                   float feedback,
                   float inputSaturation,
                   float feedbackSaturation,
                   float noise) noexcept
    {
        const auto driven = input * driveGain;
        const auto inputSat = 1.0f + inputSaturation * 1.5f;
        auto x = fastTanh (driven * inputSat + 0.055f * driven * driven) / inputSat;
        x = inputDc.process (x);
        x = inputLp1.process (x);
        x = inputLp2.process (x);

        const auto d = std::clamp (delaySamples, 4.0f, maxDelaySamples);
        const auto delayed = readInterpolated (d);

        auto fb = delayed * feedback;
        fb = feedbackLp.process (fb);
        const auto fbSat = 1.0f + feedbackSaturation * 1.5f;
        fb = fastTanh (fb * fbSat) / fbSat;
        fb = feedbackDc.process (fb);

        buffer[static_cast<size_t> (writeIndex)] = x + fb;
        if (++writeIndex >= bufferSize)
            writeIndex = 0;

        auto wet = delayed + noise;
        wet = outputLp1.process (wet);
        wet = outputLp2.process (wet);
        wet = outputDc.process (wet);
        return wet;
    }

    float getMaxDelaySamples() const noexcept { return maxDelaySamples; }

private:
    float readInterpolated (float delaySamples) const noexcept
    {
        const auto readPosition = static_cast<float> (writeIndex) - delaySamples;
        auto base = static_cast<int> (std::floor (readPosition));
        const auto fraction = readPosition - static_cast<float> (base);

        const auto y0 = at (base - 1);
        const auto y1 = at (base);
        const auto y2 = at (base + 1);
        const auto y3 = at (base + 2);

        // Catmull-Rom interpolation. It is a little brighter than linear
        // interpolation, which suits the slightly gritty BBD character.
        const auto a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const auto b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c = -0.5f * y0 + 0.5f * y2;
        const auto d = y1;

        return ((a * fraction + b) * fraction + c) * fraction + d;
    }

    float at (int index) const noexcept
    {
        while (index < 0)
            index += bufferSize;

        while (index >= bufferSize)
            index -= bufferSize;

        return buffer[static_cast<size_t> (index)];
    }

    double sampleRate = 48000.0;
    float maxDelaySamples = 8.0f;
    int bufferSize = 0;
    int writeIndex = 0;
    std::vector<float> buffer;

    OnePoleLowpass inputLp1, inputLp2, feedbackLp, outputLp1, outputLp2;
    DcBlocker inputDc, feedbackDc, outputDc;
};

} // namespace copperline
