#pragma once

#include "Module.h"
#include "Utils.h"
#include "../CustomModuleRegistry.h"

#include <array>

namespace iridescent
{

class CustomModule final : public Module
{
public:
    CustomModule (SlotState& state, int moduleIndex);
    ~CustomModule() override;

    void prepare (double sampleRate, int maximumBlockSize) override;
    void reset() override;
    void process (ProcessContext& context) override;

    int getModuleIndex() const noexcept { return index; }
    bool isLoaded() const noexcept { return definition != nullptr; }

private:
    int index = 0;
    const LoadedCustomModule* definition = nullptr;
    void* instance = nullptr;
    std::array<ParamRef, 8> params;
    double sampleRate = 48000.0;
    int maxBlock = 512;
};

} // namespace iridescent
