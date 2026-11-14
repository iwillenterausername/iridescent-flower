#pragma once

#include <algorithm>
#include <cmath>

namespace copperline
{

/**
    A simplified NE570-style compander.

    The input is compressed with a 2:1 characteristic and expanded again after
    the delay line. Because the BBD's own hiss is added before the expander it
    is pushed down along with the quiet parts of the signal, which is exactly
    what the compander in a vintage BBD pedal does.
*/
class Compander
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        inputEnvelope.setTimes (3.0f, 120.0f, sampleRate);
        outputEnvelope.setTimes (2.0f, 180.0f, sampleRate);

        compressorAttack = timeCoefficient (5.0f, sampleRate);
        compressorRelease = timeCoefficient (140.0f, sampleRate);
        expanderAttack = timeCoefficient (2.0f, sampleRate);
        expanderRelease = timeCoefficient (220.0f, sampleRate);

        reset();
    }

    void reset() noexcept
    {
        inputEnvelope.reset();
        outputEnvelope.reset();
        compressorGain = 1.0f;
        expanderGain = 1.0f;
    }

    void processInput (float& left, float& right, float amount) noexcept
    {
        const auto detector = std::max (std::abs (left), std::abs (right));
        inputEnvelope.process (detector);

        auto target = 1.0f;

        // The gate keeps the compander from applying huge gain to digital
        // silence or the noise floor between phrases.
        if (inputEnvelope.value > 1.0e-5f)
            target = std::clamp (1.0f / std::sqrt (inputEnvelope.value), 0.08f, 12.0f);

        const auto smoothing = target < compressorGain ? compressorAttack : compressorRelease;
        compressorGain += (target - compressorGain) * smoothing;

        const auto gain = 1.0f + amount * (compressorGain - 1.0f);
        left *= gain;
        right *= gain;
    }

    void processOutput (float& left, float& right, float amount) noexcept
    {
        const auto detector = std::max (std::abs (left), std::abs (right));
        outputEnvelope.process (detector);

        auto target = 1.0f;

        if (outputEnvelope.value > 5.0e-6f)
            target = std::clamp (outputEnvelope.value, 0.01f, 8.0f);

        // On a rising signal the expander must open quickly, otherwise the
        // beginning of every note is swallowed. On a falling signal it closes
        // slowly, which keeps the noise reduction from sounding like a gate.
        const auto smoothing = target > expanderGain ? expanderAttack : expanderRelease;
        expanderGain += (target - expanderGain) * smoothing;

        const auto gain = 1.0f + amount * (expanderGain - 1.0f);
        left *= gain;
        right *= gain;
    }

private:
    struct Envelope
    {
        void setTimes (float attackMs, float releaseMs, double sr)
        {
            attack = timeCoefficient (attackMs, sr);
            release = timeCoefficient (releaseMs, sr);
        }

        void process (float detector) noexcept
        {
            const auto coefficient = detector > value ? attack : release;
            value += (detector - value) * coefficient;
        }

        void reset() noexcept { value = 0.0f; }

        float attack = 0.99f;
        float release = 0.999f;
        float value = 0.0f;
    };

    static float timeCoefficient (float milliseconds, double sr) noexcept
    {
        return 1.0f - std::exp (-1.0f / (0.001f * milliseconds * static_cast<float> (sr)));
    }

    double sampleRate = 48000.0;
    Envelope inputEnvelope;
    Envelope outputEnvelope;
    float compressorGain = 1.0f;
    float expanderGain = 1.0f;
    float compressorAttack = 0.99f;
    float compressorRelease = 0.999f;
    float expanderAttack = 0.99f;
    float expanderRelease = 0.999f;
};

} // namespace copperline
