#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "DSP/RackEngine.h"

class IridescentFlowerProcessor final : public juce::AudioProcessor,
                                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    IridescentFlowerProcessor();
    ~IridescentFlowerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    iridescent::RackEngine& getEngine() noexcept { return engine; }
    const iridescent::RackEngine& getEngine() const noexcept { return engine; }

    iridescent::RackEngine::Slot getSlot (int index) const;
    iridescent::RackEngine::Slot getSlot (int bus, int index) const;
    void setSlotType (int index, iridescent::ModuleId type);
    void setSlotType (int bus, int index, iridescent::ModuleId type);
    void setSlotOn (int index, bool on);
    void setSlotOn (int bus, int index, bool on);
    void moveSlot (int from, int to);
    void moveSlot (int bus, int from, int to);
    void moveSlotTo (int bus, int from, int insertion);
    void removeSlot (int bus, int index);

    // The UI edits the selected slot through the normal APVTS parameters.
    // This call snapshots the previous slot and loads the new slot's values.
    void setActiveSlot (int bus, int index, bool syncPrevious = true);
    void syncActiveSlotFromParameters();
    int getActiveBus() const noexcept { return activeBus; }
    int getActiveSlot() const noexcept { return activeSlot; }

    bool savePreset (const juce::File& file);
    bool loadPreset (const juce::File& file);
    static juce::File getPresetFolder();

    // Factory presets are embedded in the binary. Index 0 is the startup patch.
    static juce::StringArray getFactoryPresetNames();
    static int getFactoryPresetCount();
    static void setLoadFactoryPresetOnStartup (bool shouldLoad);
    bool loadFactoryPreset (int index);
    bool loadFactoryPreset (const juce::String& name);

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void writeSlotState (juce::ValueTree& tree) const;
    void readSlotState (const juce::ValueTree& tree);
    void loadActiveSlotToParameters();
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    iridescent::RackEngine engine;
    int activeBus = 0;
    int activeSlot = 0;
    bool syncingParameters = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IridescentFlowerProcessor)
};
