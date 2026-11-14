#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace iridescent
{

// Every rack slot owns one of these. Modules bind their ParamRef/ChoiceRef to
// the slot state instead of the global APVTS, so duplicate module types on
// different buses/slots never share values.
class SlotState
{
public:
    SlotState() = default;

    void initialise (juce::AudioProcessorValueTreeState& state);
    void copyFrom (const SlotState& other);
    void resetToDefaults();

    std::atomic<float>* getRawParameterValue (const char* id) noexcept;
    const std::atomic<float>* getRawParameterValue (const char* id) const noexcept;

    float get (const char* id) const noexcept;
    void set (const char* id, float value) noexcept;

    bool has (const char* id) const noexcept;
    int getNumValues() const noexcept { return valueCount; }
    juce::String getParameterId (int index) const;
    float getValueAt (int index) const noexcept;
    void setValueAt (int index, float value) noexcept;

private:
    static constexpr int maxValues = 256;

    std::atomic<float> values[maxValues] {};
    std::array<float, maxValues> defaults {};
    std::array<juce::String, maxValues> ids;
    std::unordered_map<std::string, int> indexById;
    int valueCount = 0;
};

} // namespace iridescent
