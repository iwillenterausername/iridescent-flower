#include "CustomModule.h"
#include "../ModuleCatalog.h"

namespace iridescent
{

CustomModule::CustomModule (SlotState& state, int moduleIndex)
    : index (moduleIndex)
{
    auto& registry = CustomModuleRegistry::get();
    registry.scan();
    definition = registry.getModule (index);

    for (int i = 0; i < static_cast<int> (params.size()); ++i)
    {
        const auto id = iridescent::ids::customParamId (index, i);
        params[static_cast<size_t> (i)].bind (state, id.toRawUTF8());
    }
}

CustomModule::~CustomModule()
{
    if (instance != nullptr && definition != nullptr)
        definition->destroy (instance);
}

void CustomModule::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = maximumBlockSize;

    if (instance != nullptr && definition != nullptr)
        definition->destroy (instance);

    instance = nullptr;
}

void CustomModule::reset()
{
    if (instance != nullptr && definition != nullptr)
    {
        definition->destroy (instance);
        instance = nullptr;
    }
}

void CustomModule::process (ProcessContext& context)
{
    if (definition == nullptr || context.numSamples <= 0)
        return;

    if (instance == nullptr)
        instance = definition->create (sampleRate, maxBlock);

    if (instance == nullptr)
        return;

    std::array<float, 8> values {};

    for (int i = 0; i < static_cast<int> (values.size()); ++i)
        values[static_cast<size_t> (i)] = params[static_cast<size_t> (i)].get();

    auto* left = context.buffer.getWritePointer (0);
    auto* right = context.buffer.getNumChannels() > 1
                      ? context.buffer.getWritePointer (1)
                      : left;

    definition->process (instance, left, right, context.numSamples,
                         values.data(), static_cast<int> (values.size()),
                         context.bpm);
}

} // namespace iridescent
