#include "../../Source/CustomModuleApi.h"
#include "BbdEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>

namespace
{
struct BbdInstance
{
    copperline::BbdEngine engine;
    copperline::BbdParameters params;
};
}

IF_MODULE_EXPORT const IFModuleInfo* if_get_module_info()
{
    static const IFModuleInfo info {
        "BBD DELAY",
        "COPPERLINE BBD",
        0xffe0a45a,
        {
            { "TIME", 0.38f },
            { "FEEDBACK", 0.42f },
            { "MIX", 0.32f },
            { "TONE", 0.62f },
            { "DRIVE", 0.50f },
            { "AGE", 0.28f },
            { "MOD DEPTH", 0.30f },
            { "WIDTH", 0.50f }
        }
    };

    return &info;
}

IF_MODULE_EXPORT void* if_create (double sampleRate, int maxBlockSize)
{
    auto* instance = new BbdInstance();
    instance->engine.prepare (sampleRate, std::max (1, maxBlockSize), 2);
    return instance;
}

IF_MODULE_EXPORT void if_destroy (void* raw)
{
    delete static_cast<BbdInstance*> (raw);
}

IF_MODULE_EXPORT void if_process (void* raw,
                                  float* left,
                                  float* right,
                                  int numSamples,
                                  const float* parameters,
                                  int numParameters,
                                  double)
{
    auto* instance = static_cast<BbdInstance*> (raw);

    if (instance == nullptr || left == nullptr || numSamples <= 0)
        return;

    auto p = [&] (int index, float fallback) -> float
    {
        return index < numParameters && parameters != nullptr ? parameters[index] : fallback;
    };

    instance->params.timeMs = 30.0f + p (0, 0.38f) * 970.0f;
    instance->params.feedback = juce::jlimit (0.0f, 0.88f, p (1, 0.42f) * 0.88f);
    instance->params.mix = juce::jlimit (0.0f, 1.0f, p (2, 0.32f));
    instance->params.tone = juce::jlimit (0.05f, 0.95f, p (3, 0.62f));
    instance->params.driveDb = -12.0f + p (4, 0.5f) * 24.0f;
    instance->params.age = juce::jlimit (0.0f, 1.0f, p (5, 0.28f));
    instance->params.modDepthMs = juce::jlimit (0.0f, 4.0f, p (6, 0.3f) * 4.0f);
    instance->params.width = juce::jlimit (0.0f, 1.5f, p (7, 0.5f) * 1.5f);
    instance->params.modRateHz = 0.55f;
    instance->params.levelDb = 0.0f;
    instance->params.compander = true;
    instance->params.freeze = false;

    instance->engine.setParameters (instance->params);

    float* channels[2] = { left, right != nullptr ? right : left };
    juce::AudioBuffer<float> buffer (channels, 2, numSamples);
    instance->engine.process (buffer);
}
