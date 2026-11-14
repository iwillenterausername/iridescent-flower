#include "CustomModuleRegistry.h"

namespace iridescent
{

CustomModuleRegistry& CustomModuleRegistry::get()
{
    static CustomModuleRegistry registry;
    return registry;
}

juce::File CustomModuleRegistry::getFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Iridescent Flower")
        .getChildFile ("Custom Modules");
}

void CustomModuleRegistry::scan()
{
    if (scanned)
        return;

    scanned = true;
    const auto folder = getFolder();

    if (! folder.isDirectory())
        folder.createDirectory();

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.dll");
    files.sort();

    int next = 0;

    for (const auto& file : files)
    {
        if (next >= maxModules)
            break;

        loadFile (next, file);
        ++next;
    }
}

void CustomModuleRegistry::loadFile (int index, const juce::File& file)
{
    if (index < 0 || index >= maxModules)
        return;

    auto loaded = std::make_unique<LoadedCustomModule>();
    loaded->fileName = file.getFileName();

    if (! loaded->library.open (file.getFullPathName()))
        return;

    loaded->getInfo = reinterpret_cast<IFGetInfoFn> (loaded->library.getFunction ("if_get_module_info"));
    loaded->create = reinterpret_cast<IFCreateFn> (loaded->library.getFunction ("if_create"));
    loaded->destroy = reinterpret_cast<IFDestroyFn> (loaded->library.getFunction ("if_destroy"));
    loaded->process = reinterpret_cast<IFProcessFn> (loaded->library.getFunction ("if_process"));

    if (loaded->getInfo == nullptr)
        return;

    loaded->info = loaded->getInfo();

    if (loaded->info == nullptr || loaded->info->name == nullptr)
        return;

    if (! loaded->isValid())
        return;

    modules[static_cast<size_t> (index)] = std::move (loaded);
}

int CustomModuleRegistry::getNumLoaded() const noexcept
{
    int count = 0;

    for (const auto& module : modules)
        if (module != nullptr && module->isValid())
            ++count;

    return count;
}

const LoadedCustomModule* CustomModuleRegistry::getModule (int index) const noexcept
{
    if (index < 0 || index >= maxModules)
        return nullptr;

    const auto& module = modules[static_cast<size_t> (index)];
    return module != nullptr && module->isValid() ? module.get() : nullptr;
}

juce::String CustomModuleRegistry::getDisplayName (int index) const
{
    if (const auto* module = getModule (index))
        return juce::String (module->info->name).toUpperCase();

    return "CUSTOM " + juce::String (index + 1);
}

juce::String CustomModuleRegistry::getTagline (int index) const
{
    if (const auto* module = getModule (index))
        return juce::String (module->info->tagline);

    return "EMPTY DLL SLOT";
}

juce::uint32 CustomModuleRegistry::getColour (int index) const
{
    if (const auto* module = getModule (index))
        return module->info->colour;

    return 0xff8e7ca6u;
}

void CustomModuleRegistry::revealFolder() const
{
    getFolder().startAsProcess();
}

} // namespace iridescent
