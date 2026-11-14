#pragma once

#include "Module.h"
#include "ModuleCatalog.h"
#include "SlotState.h"

#include "ChorusModule.h"
#include "CloudModule.h"
#include "CompressorModule.h"
#include "DistortionModule.h"
#include "EchoModule.h"
#include "EqModule.h"
#include "FilterModule.h"
#include "GrainModule.h"
#include "HyperModule.h"
#include "MorphModule.h"

#include <array>
#include <atomic>
#include <memory>

namespace iridescent
{

class RackEngine
{
public:
    static constexpr int slotsPerBus = 8;
    static constexpr int numSlots = slotsPerBus;
    static constexpr int numBuses = 3;
    static constexpr int masterSlotCount = 8;
    static constexpr int numChains = 4;
    static constexpr int masterChain = 3;
    static constexpr int totalSlots = numChains * slotsPerBus;

    struct Slot
    {
        ModuleId type = Morph; // Morph means empty here; the real Morph is the fixed output stage.
        bool on = true;
    };

    struct SlotUnit
    {
        Slot slot;
        std::unique_ptr<Module> module;
        SlotState state;
    };

    explicit RackEngine (juce::AudioProcessorValueTreeState& state);

    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    void process (juce::AudioBuffer<float>& buffer,
                  const juce::AudioBuffer<float>* sidechain,
                  bool sidechainBusActive);

    Slot getSlot (int bus, int index) const noexcept;
    void setSlotType (int bus, int index, ModuleId type) noexcept;
    void setSlotOn (int bus, int index, bool on) noexcept;
    void moveSlot (int bus, int from, int to) noexcept;
    void moveSlotTo (int bus, int from, int insertion) noexcept;
    void removeSlot (int bus, int index) noexcept;

    SlotState& getSlotState (int bus, int index) noexcept;
    const SlotState& getSlotState (int bus, int index) const noexcept;

    // Convenience for the current bus-1 page and legacy tests.
    Slot getSlot (int index) const noexcept { return getSlot (0, index); }
    void setSlotType (int index, ModuleId type) noexcept { setSlotType (0, index, type); }
    void setSlotOn (int index, bool on) noexcept { setSlotOn (0, index, on); }
    void moveSlot (int from, int to) noexcept { moveSlot (0, from, to); }
    void moveSlotTo (int from, int insertion) noexcept { moveSlotTo (0, from, insertion); }
    void removeSlot (int index) noexcept { removeSlot (0, index); }
    SlotState& getSlotState (int index) noexcept { return getSlotState (0, index); }

    void setTempo (double newTempo) noexcept { tempo.store (newTempo, std::memory_order_relaxed); }
    double getTempo() const noexcept { return tempo.load (std::memory_order_relaxed); }
    void setPlaying (bool shouldPlay) noexcept { playing.store (shouldPlay, std::memory_order_relaxed); }
    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }

    float getMorphValue() const noexcept { return morphValue.load (std::memory_order_relaxed); }
    float getLfoValue() const noexcept { return lfoValue.load (std::memory_order_relaxed); }
    int getActiveGrainCount() const noexcept;
    bool getSidechainActive() const noexcept { return sidechainActive.load (std::memory_order_relaxed); }
    float getInputMeter() const noexcept { return inputMeter.load (std::memory_order_relaxed); }
    float getOutputMeter() const noexcept { return outputMeter.load (std::memory_order_relaxed); }

    void copyScope (float* destination, int numberOfSamples) const noexcept;

private:
    static constexpr int scopeSize = 256;

    SlotUnit& unit (int bus, int index) noexcept;
    const SlotUnit& unit (int bus, int index) const noexcept;
    void prepareUnit (SlotUnit& slot);
    std::unique_ptr<Module> createModule (SlotUnit& slot, ModuleId type);
    void processChain (int chainIndex, juce::AudioBuffer<float>& buffer);

    juce::AudioProcessorValueTreeState& parameters;

    std::array<std::unique_ptr<SlotUnit>, totalSlots> units;
    MorphModule morph;

    juce::AudioBuffer<float> bus1, bus2, bus3, dryBuffer;
    Smooth globalMixSmooth, outputSmooth, bus1Smooth, bus2Smooth, bus3Smooth;

    juce::CriticalSection processLock;
    double sampleRate = 48000.0;
    int maximumBlockSize = 512;

    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> playing { false };
    std::atomic<float> morphValue { 0.0f };
    std::atomic<float> lfoValue { 0.0f };
    std::atomic<bool> sidechainActive { false };
    std::atomic<float> inputMeter { 0.0f };
    std::atomic<float> outputMeter { 0.0f };

    std::array<std::atomic<float>, scopeSize> scopeBuffer {};
    std::atomic<int> scopeWriteIndex { 0 };
    int scopeCounter = 0;
    int scopeStride = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackEngine)
};

} // namespace iridescent
