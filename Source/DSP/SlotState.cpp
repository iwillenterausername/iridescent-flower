#include "SlotState.h"

namespace iridescent
{

void SlotState::initialise (juce::AudioProcessorValueTreeState& state)
{
    valueCount = 0;
    indexById.clear();

    for (auto* parameter : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);

        if (ranged == nullptr)
            continue;

        if (valueCount >= maxValues)
            break;

        const auto id = ranged->getParameterID();

        // Global/morph controls belong to the rack, not to one slot.
        if (id == "global_mix" || id == "global_output"
            || id == "bus1_mix" || id == "bus2_mix" || id == "bus3_mix"
            || id == "visuals" || id == "moduleOrder"
            || id.startsWith ("morph_") || id.startsWith ("lfo_"))
            continue;

        const auto index = valueCount++;
        ids[static_cast<size_t> (index)] = id;
        defaults[static_cast<size_t> (index)] =
            ranged->convertFrom0to1 (ranged->getDefaultValue());

        if (auto* raw = state.getRawParameterValue (id))
            values[index].store (raw->load (std::memory_order_relaxed),
                                 std::memory_order_relaxed);

        indexById[id.toStdString()] = index;
    }
}

void SlotState::copyFrom (const SlotState& other)
{
    valueCount = other.valueCount;
    indexById = other.indexById;

    for (int i = 0; i < valueCount; ++i)
    {
        ids[static_cast<size_t> (i)] = other.ids[static_cast<size_t> (i)];
        defaults[static_cast<size_t> (i)] = other.defaults[static_cast<size_t> (i)];
        values[i].store (other.values[i].load (std::memory_order_relaxed),
                         std::memory_order_relaxed);
    }
}

void SlotState::resetToDefaults()
{
    for (int i = 0; i < valueCount; ++i)
        values[i].store (defaults[static_cast<size_t> (i)], std::memory_order_relaxed);
}

std::atomic<float>* SlotState::getRawParameterValue (const char* id) noexcept
{
    if (id == nullptr)
        return nullptr;

    const auto it = indexById.find (id);
    return it == indexById.end() ? nullptr : &values[it->second];
}

const std::atomic<float>* SlotState::getRawParameterValue (const char* id) const noexcept
{
    return const_cast<SlotState*> (this)->getRawParameterValue (id);
}

float SlotState::get (const char* id) const noexcept
{
    if (const auto* value = getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}

void SlotState::set (const char* id, float value) noexcept
{
    if (auto* target = getRawParameterValue (id))
        target->store (value, std::memory_order_relaxed);
}

bool SlotState::has (const char* id) const noexcept
{
    return id != nullptr && indexById.find (id) != indexById.end();
}

juce::String SlotState::getParameterId (int index) const
{
    return index >= 0 && index < valueCount ? ids[static_cast<size_t> (index)] : juce::String();
}

float SlotState::getValueAt (int index) const noexcept
{
    return index >= 0 && index < valueCount ? values[index].load (std::memory_order_relaxed)
                                            : 0.0f;
}

void SlotState::setValueAt (int index, float value) noexcept
{
    if (index >= 0 && index < valueCount)
        values[index].store (value, std::memory_order_relaxed);
}

} // namespace iridescent
