#pragma once

#include "Module.h"
#include "Utils.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace iridescent
{

class GrainModule final : public Module
{
public:
    explicit GrainModule (SlotState& state);

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

    int getActiveGrainCount() const noexcept { return activeGrainCount.load (std::memory_order_relaxed); }

private:
    static constexpr int maxGrains = 64;

    struct Grain
    {
        bool active = false;
        float readPos = 0.0f;
        float increment = 1.0f;
        int age = 0;
        int length = 0;
        float panL = 0.7f;
        float panR = 0.7f;
    };

    void spawnGrain (float sizeSamples, float pitchSemitones,
                     float spray, float reverse, float width,
                     float bufferSeconds, float offsetValue, int snapValue);
    float readBuffer (const std::vector<float>& buffer, float position) const noexcept;
    int wrapIndex (int index) const noexcept;

    ParamRef size, density, pitch, spray, feedback, mix, reverse, width, freeze, sync;
    ParamRef buffer, offset, rate;
    ParamRef division;
    ChoiceRef snap;

    Smooth sizeSmooth, densitySmooth, feedbackSmooth, mixSmooth, widthSmooth, reverseSmooth;

    std::vector<float> bufferL;
    std::vector<float> bufferR;
    std::array<Grain, maxGrains> grains;

    Noise noise { 0x7a11c0deu };
    OnePoleLowpass toneL, toneR;
    DcBlocker dcL, dcR;
    std::atomic<int> activeGrainCount { 0 };

    int bufferSize = 0;
    int writeIndex = 0;
    std::int64_t samplesWritten = 0;
    float samplesUntilNextGrain = 0.0f;
    float lastWetL = 0.0f;
    float lastWetR = 0.0f;
    double sampleRate = 48000.0;
};

} // namespace iridescent
