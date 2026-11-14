#include "RackEngine.h"
#include "CustomModule.h"

namespace iridescent
{

RackEngine::RackEngine (juce::AudioProcessorValueTreeState& state)
    : parameters (state),
      morph (state)
{
    for (auto& slot : units)
    {
        slot = std::make_unique<SlotUnit>();
        slot->slot = { Morph, true };
    }

    // All slots start with the same defaults; every slot owns an independent copy.
    units[0]->state.initialise (state);

    for (int i = 1; i < totalSlots; ++i)
        units[static_cast<size_t> (i)]->state.copyFrom (units[0]->state);

    const ModuleId defaultChain[] = { Comp, Eq, Filter, Chorus, Distortion, Echo, Grain, Cloud };

    for (int i = 0; i < slotsPerBus; ++i)
        units[static_cast<size_t> (i)]->slot = { defaultChain[i], true };
}

RackEngine::SlotUnit& RackEngine::unit (int bus, int index) noexcept
{
    const auto chain = (bus >= 0 && bus < numBuses) ? bus : masterChain;
    return *units[static_cast<size_t> (chain * slotsPerBus + index)];
}

const RackEngine::SlotUnit& RackEngine::unit (int bus, int index) const noexcept
{
    const auto chain = (bus >= 0 && bus < numBuses) ? bus : masterChain;
    return *units[static_cast<size_t> (chain * slotsPerBus + index)];
}

std::unique_ptr<Module> RackEngine::createModule (SlotUnit& slot, ModuleId type)
{
    switch (type)
    {
        case Comp:       return std::make_unique<CompressorModule> (slot.state);
        case Eq:         return std::make_unique<EqModule> (slot.state);
        case Filter:     return std::make_unique<FilterModule> (slot.state);
        case Chorus:     return std::make_unique<ChorusModule> (slot.state);
        case Distortion: return std::make_unique<DistortionModule> (slot.state);
        case Hyper:      return std::make_unique<HyperModule> (slot.state);
        case Echo:       return std::make_unique<EchoModule> (slot.state);
        case Grain:      return std::make_unique<GrainModule> (slot.state);
        case Cloud:      return std::make_unique<CloudModule> (slot.state);
        default:
            break;
    }

    if (type >= Custom0 && type < NumModules)
        return std::make_unique<CustomModule> (slot.state, type - Custom0);

    return nullptr;
}

void RackEngine::prepareUnit (SlotUnit& slot)
{
    slot.module = createModule (slot, slot.slot.type);

    if (slot.module != nullptr)
        slot.module->prepare (sampleRate, maximumBlockSize);
}

void RackEngine::prepare (double newSampleRate, int maximumBlockSizeIn)
{
    const juce::ScopedLock lock (processLock);
    sampleRate = newSampleRate;
    maximumBlockSize = maximumBlockSizeIn;

    bus1.setSize (2, maximumBlockSize, false, true, true);
    bus2.setSize (2, maximumBlockSize, false, true, true);
    bus3.setSize (2, maximumBlockSize, false, true, true);
    dryBuffer.setSize (2, maximumBlockSize, false, true, true);

    for (auto& slot : units)
        if (slot != nullptr && slot->slot.type != Morph)
            prepareUnit (*slot);

    morph.prepare (sampleRate, maximumBlockSize);
    morph.reset();

    globalMixSmooth.prepare (sampleRate, 0.04f);
    outputSmooth.prepare (sampleRate, 0.04f);
    bus1Smooth.prepare (sampleRate, 0.04f);
    bus2Smooth.prepare (sampleRate, 0.04f);
    bus3Smooth.prepare (sampleRate, 0.04f);
    globalMixSmooth.snap (1.0f);
    outputSmooth.snap (1.0f);
    bus1Smooth.snap (1.0f);
    bus2Smooth.snap (0.0f);
    bus3Smooth.snap (0.0f);

    for (auto& value : scopeBuffer)
        value.store (0.0f, std::memory_order_relaxed);

    scopeWriteIndex.store (0);
    scopeCounter = 0;
}

void RackEngine::reset()
{
    const juce::ScopedLock lock (processLock);

    for (auto& slot : units)
        if (slot != nullptr && slot->module != nullptr)
            slot->module->reset();

    morph.reset();
    bus1.clear();
    bus2.clear();
    bus3.clear();
    dryBuffer.clear();

    for (auto& value : scopeBuffer)
        value.store (0.0f, std::memory_order_relaxed);

    scopeWriteIndex.store (0);
    scopeCounter = 0;
}

RackEngine::Slot RackEngine::getSlot (int bus, int index) const noexcept
{
    return unit (bus, juce::jlimit (0, slotsPerBus - 1, index)).slot;
}

void RackEngine::setSlotType (int bus, int index, ModuleId type) noexcept
{
    if (index < 0 || index >= slotsPerBus || type == Lfo)
        return;

    const juce::ScopedLock lock (processLock);
    auto& slot = unit (bus, index);
    slot.slot = { type, true };
    slot.module.reset();
    slot.state.resetToDefaults();

    if (sampleRate > 0.0)
        prepareUnit (slot);
}

void RackEngine::setSlotOn (int bus, int index, bool on) noexcept
{
    if (index < 0 || index >= slotsPerBus)
        return;

    const juce::ScopedLock lock (processLock);
    unit (bus, index).slot.on = on;
}

void RackEngine::moveSlot (int bus, int from, int to) noexcept
{
    if (from < 0 || from >= slotsPerBus || to < 0 || to >= slotsPerBus || from == to)
        return;

    const juce::ScopedLock lock (processLock);
    auto& slots = units;
    const auto base = static_cast<size_t> ((bus >= 0 && bus < numBuses ? bus : masterChain) * slotsPerBus);

    if (from < to)
        std::rotate (slots.begin() + base + from, slots.begin() + base + from + 1,
                     slots.begin() + base + to + 1);
    else
        std::rotate (slots.begin() + base + to, slots.begin() + base + from,
                     slots.begin() + base + from + 1);
}

void RackEngine::moveSlotTo (int bus, int from, int insertion) noexcept
{
    if (from < 0 || from >= slotsPerBus || insertion < 0 || insertion > slotsPerBus)
        return;

    const juce::ScopedLock lock (processLock);
    const auto base = static_cast<size_t> ((bus >= 0 && bus < numBuses ? bus : masterChain) * slotsPerBus);

    if (insertion > from)
        --insertion;

    std::array<std::unique_ptr<SlotUnit>, slotsPerBus> old;
    std::array<std::unique_ptr<SlotUnit>, slotsPerBus> result;

    for (int i = 0; i < slotsPerBus; ++i)
        old[static_cast<size_t> (i)] = std::move (units[base + static_cast<size_t> (i)]);

    auto moved = std::move (old[static_cast<size_t> (from)]);
    int write = 0;

    for (int i = 0; i < slotsPerBus; ++i)
    {
        if (i == from)
            continue;

        if (write == insertion)
            result[static_cast<size_t> (write++)] = std::move (moved);

        result[static_cast<size_t> (write++)] = std::move (old[static_cast<size_t> (i)]);
    }

    if (write == insertion)
        result[static_cast<size_t> (write++)] = std::move (moved);

    for (int i = 0; i < slotsPerBus; ++i)
        units[base + static_cast<size_t> (i)] = std::move (result[static_cast<size_t> (i)]);
}

void RackEngine::removeSlot (int bus, int index) noexcept
{
    if (index < 0 || index >= slotsPerBus)
        return;

    const juce::ScopedLock lock (processLock);
    const auto base = static_cast<size_t> ((bus >= 0 && bus < numBuses ? bus : masterChain) * slotsPerBus);

    for (int i = index; i < slotsPerBus - 1; ++i)
        units[base + static_cast<size_t> (i)] = std::move (units[base + static_cast<size_t> (i + 1)]);

    auto empty = std::make_unique<SlotUnit>();
    empty->slot = { Morph, true };
    empty->state.copyFrom (units[0]->state);
    empty->state.resetToDefaults();
    units[base + static_cast<size_t> (slotsPerBus - 1)] = std::move (empty);
}

SlotState& RackEngine::getSlotState (int bus, int index) noexcept
{
    return unit (bus, index).state;
}

const SlotState& RackEngine::getSlotState (int bus, int index) const noexcept
{
    return unit (bus, index).state;
}

void RackEngine::processChain (int chainIndex, juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto base = static_cast<size_t> (chainIndex * slotsPerBus);

    for (int i = 0; i < slotsPerBus; ++i)
    {
        auto& slot = *units[base + static_cast<size_t> (i)];

        if (! slot.slot.on || slot.slot.type <= Morph || slot.slot.type >= NumModules)
            continue;

        if (slot.module == nullptr)
            continue;

        ProcessContext context {
            buffer, nullptr, nullptr, nullptr, numSamples, 0.0f, false, false,
            tempo.load (std::memory_order_relaxed)
        };

        slot.module->process (context);
    }
}

void RackEngine::process (juce::AudioBuffer<float>& buffer,
                          const juce::AudioBuffer<float>* sidechain,
                          bool sidechainBusActive)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const juce::ScopedLock lock (processLock);

    if (bus1.getNumSamples() < numSamples)
    {
        bus1.setSize (2, numSamples, false, true, true);
        bus2.setSize (2, numSamples, false, true, true);
        bus3.setSize (2, numSamples, false, true, true);
        dryBuffer.setSize (2, numSamples, false, true, true);
    }

    dryBuffer.makeCopyOf (buffer, true);

    auto copyInput = [&] (juce::AudioBuffer<float>& target)
    {
        target.clear();

        for (int ch = 0; ch < 2; ++ch)
            target.copyFrom (ch, 0, buffer, juce::jmin (ch, numChannels - 1), 0, numSamples);
    };

    copyInput (bus1);
    copyInput (bus2);
    copyInput (bus3);

    globalMixSmooth.setTarget (juce::jlimit (0.0f, 1.0f,
                                             parameters.getRawParameterValue (ids::globalMix)->load()));
    outputSmooth.setTarget (dbToGain (juce::jlimit (-24.0f, 12.0f,
                                                    parameters.getRawParameterValue (ids::globalOutput)->load())));
    bus1Smooth.setTarget (juce::jlimit (0.0f, 1.0f,
                                        parameters.getRawParameterValue (ids::bus1Mix)->load()));
    bus2Smooth.setTarget (juce::jlimit (0.0f, 1.0f,
                                        parameters.getRawParameterValue (ids::bus2Mix)->load()));
    bus3Smooth.setTarget (juce::jlimit (0.0f, 1.0f,
                                        parameters.getRawParameterValue (ids::bus3Mix)->load()));

    processChain (0, bus1);
    processChain (1, bus2);
    processChain (2, bus3);

    const auto bus1Gain = bus1Smooth.next();
    const auto bus2Gain = bus2Smooth.next();

    for (int i = 0; i < numSamples; ++i)
    {
        bus1.getWritePointer (0)[i] *= bus1Gain;
        bus1.getWritePointer (1)[i] *= bus1Gain;
    }

    const auto source = static_cast<int> (parameters.getRawParameterValue (ids::morphSource)->load());
    const juce::AudioBuffer<float>* bBuffer = &bus2;
    const auto useRealSidechain = source == 0 && sidechain != nullptr && sidechainBusActive
                               && sidechain->getNumSamples() >= numSamples;

    if (useRealSidechain)
        bBuffer = sidechain;

    ProcessContext morphContext {
        bus1, bBuffer, nullptr, nullptr, numSamples, 0.0f, false,
        source == 1 || useRealSidechain, tempo.load (std::memory_order_relaxed)
    };

    morph.process (morphContext);

    const auto bus3Gain = bus3Smooth.next();

    for (int i = 0; i < numSamples; ++i)
    {
        // Bus 2 OUT is its direct parallel send; Morph's B input stays
        // independent so opening Morph still blends Bus 2 at 0% direct level.
        bus1.getWritePointer (0)[i] += bus2.getReadPointer (0)[i] * bus2Gain;
        bus1.getWritePointer (1)[i] += bus2.getReadPointer (1)[i] * bus2Gain;
        bus1.getWritePointer (0)[i] += bus3.getReadPointer (0)[i] * bus3Gain;
        bus1.getWritePointer (1)[i] += bus3.getReadPointer (1)[i] * bus3Gain;
    }

    // Master chain: after bus 1/2 morph and bus 2/3 direct sends.
    processChain (masterChain, bus1);

    const auto mix = globalMixSmooth.next();
    const auto output = outputSmooth.next();
    auto* outLeft = buffer.getWritePointer (0);
    auto* outRight = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    auto* busLeft = bus1.getReadPointer (0);
    auto* busRight = bus1.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto processedL = busLeft[i] * output;
        const auto processedR = busRight[i] * output;
        const auto dryL = dryBuffer.getSample (0, i);
        const auto dryR = numChannels > 1 ? dryBuffer.getSample (1, i) : dryL;

        outLeft[i] = dryL + mix * (processedL - dryL);

        if (outRight != nullptr)
            outRight[i] = dryR + mix * (processedR - dryR);

        if ((scopeCounter++ % scopeStride) == 0)
        {
            const auto index = scopeWriteIndex.load (std::memory_order_relaxed);
            scopeBuffer[static_cast<size_t> (index)].store (outLeft[i], std::memory_order_relaxed);
            scopeWriteIndex.store ((index + 1) % scopeSize, std::memory_order_relaxed);
        }
    }

    inputMeter.store (juce::jmax (inputMeter.load() * 0.92f, 0.0f), std::memory_order_relaxed);
    outputMeter.store (inputMeter.load(), std::memory_order_relaxed);
    sidechainActive.store (useRealSidechain, std::memory_order_relaxed);
    morphValue.store (morph.getLastMorph(), std::memory_order_relaxed);
    lfoValue.store (morph.getLfoValue(), std::memory_order_relaxed);
}

int RackEngine::getActiveGrainCount() const noexcept
{
    int count = 0;

    for (const auto& slot : units)
        if (slot != nullptr)
            if (const auto* grainModule = dynamic_cast<GrainModule*> (slot->module.get()))
                count += grainModule->getActiveGrainCount();

    return count;
}

void RackEngine::copyScope (float* destination, int numberOfSamples) const noexcept
{
    if (destination == nullptr || numberOfSamples <= 0)
        return;

    const auto amount = juce::jmin (numberOfSamples, scopeSize);
    const auto write = scopeWriteIndex.load (std::memory_order_relaxed);

    for (int i = 0; i < amount; ++i)
    {
        const auto index = (write - amount + i + scopeSize * 2) % scopeSize;
        destination[i] = scopeBuffer[static_cast<size_t> (index)].load (std::memory_order_relaxed);
    }
}

} // namespace iridescent
