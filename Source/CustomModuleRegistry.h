#pragma once

#include "CustomModuleApi.h"

#include <juce_core/juce_core.h>

#include <array>
#include <memory>

namespace iridescent
{

struct LoadedCustomModule
{
    juce::DynamicLibrary library;
    const IFModuleInfo* info = nullptr;
    IFGetInfoFn getInfo = nullptr;
    IFCreateFn create = nullptr;
    IFDestroyFn destroy = nullptr;
    IFProcessFn process = nullptr;
    juce::String fileName;

    bool isValid() const noexcept
    {
        return info != nullptr && create != nullptr && destroy != nullptr && process != nullptr;
    }
};

class CustomModuleRegistry
{
public:
    static constexpr int maxModules = 8;
    static constexpr int paramsPerModule = 8;

    static CustomModuleRegistry& get();

    void scan();
    void revealFolder() const;

    int getNumLoaded() const noexcept;
    const LoadedCustomModule* getModule (int index) const noexcept;
    juce::String getDisplayName (int index) const;
    juce::String getTagline (int index) const;
    juce::uint32 getColour (int index) const;
    static juce::File getFolder();

private:
    CustomModuleRegistry() = default;

    void loadFile (int index, const juce::File& file);

    std::array<std::unique_ptr<LoadedCustomModule>, maxModules> modules;
    bool scanned = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomModuleRegistry)
};

} // namespace iridescent
