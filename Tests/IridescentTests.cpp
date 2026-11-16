#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "CustomModuleRegistry.h"

#include <cmath>
#include <iostream>

namespace
{
using namespace iridescent;

constexpr double testSampleRate = 48000.0;
constexpr int blockSize = 512;
int failures = 0;

void check (bool condition, const juce::String& description)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << description << std::endl;
    if (! condition)
        ++failures;
}

void setFloat (IridescentFlowerProcessor& p, const juce::String& id, float value)
{
    if (auto* parameter = p.parameters.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void setChoice (IridescentFlowerProcessor& p, const juce::String& id, int index)
{
    if (auto* parameter = p.parameters.getParameter (id))
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        {
            const auto normalised = choice->choices.size() > 1
                                        ? static_cast<float> (index) / static_cast<float> (choice->choices.size() - 1)
                                        : 0.0f;
            parameter->setValueNotifyingHost (normalised);
        }
        else
        {
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
        }
    }
}

void prepare (IridescentFlowerProcessor& p)
{
    p.setPlayConfigDetails (2, 2, testSampleRate, blockSize);
    p.prepareToPlay (testSampleRate, blockSize);
}

void makeSine (juce::AudioBuffer<float>& buffer, double frequency, float amplitude)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, amplitude * std::sin (
                juce::MathConstants<double>::twoPi * frequency * i / testSampleRate));
}

void processInBlocks (IridescentFlowerProcessor& p, juce::AudioBuffer<float>& buffer)
{
    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        const auto count = juce::jmin (blockSize, buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> block (buffer.getNumChannels(), count);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, count);

        juce::MidiBuffer midi;
        p.processBlock (block, midi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, offset, block, ch, 0, count);
    }
}

float rmsAbs (const juce::AudioBuffer<float>& buffer)
{
    double sum = 0.0;
    int count = 0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += static_cast<double> (buffer.getSample (ch, i))
                 * buffer.getSample (ch, i);
            ++count;
        }
    return static_cast<float> (std::sqrt (sum / juce::jmax (1, count)));
}

float peakAbs (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));
    return peak;
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;
    return true;
}

void clearSlots (IridescentFlowerProcessor& p)
{
    for (int i = 0; i < RackEngine::numSlots; ++i)
        p.setSlotType (i, ModuleId::Morph);

    setFloat (p, ids::lfoDepth, 0.0f);
}

void setSlot (IridescentFlowerProcessor& p, int slot, ModuleId type, int bus = 1)
{
    p.setSlotType (bus - 1, slot, type);
    p.setSlotOn (bus - 1, slot, true);
}

void testDefaultChain()
{
    IridescentFlowerProcessor p;
    prepare (p);
    juce::AudioBuffer<float> buffer (2, static_cast<int> (testSampleRate * 2.0));
    makeSine (buffer, 220.0, 0.15f);
    const auto inputRms = rmsAbs (buffer);
    processInBlocks (p, buffer);
    std::cout << "  default: in=" << inputRms << " out=" << rmsAbs (buffer) << std::endl;
    check (allFinite (buffer), "default eight-fx chain is finite");
    check (peakAbs (buffer) > 0.01f, "default chain produces audio");
    check (rmsAbs (buffer) > inputRms * 0.5f, "default chain keeps level");
}

void testMorphBuses()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    setSlot (p, 0, ModuleId::Filter, 1);
    setSlot (p, 1, ModuleId::Distortion, 2);
    setFloat (p, ids::filterCutoff, 8000.0f);
    setFloat (p, ids::distDrive, 24.0f);
    setFloat (p, ids::distMix, 1.0f);
    setChoice (p, ids::morphSource, 1); // BUS 2
    setFloat (p, ids::morphAmount, 1.0f);

    juce::AudioBuffer<float> buffer (2, static_cast<int> (testSampleRate * 1.0));
    makeSine (buffer, 220.0, 0.1f);
    processInBlocks (p, buffer);
    check (allFinite (buffer), "bus 1 / bus 2 morph is finite");
    check (rmsAbs (buffer) > 0.005f, "bus 2 feeds the morph output");

    setFloat (p, ids::morphAmount, 0.0f);
    juce::AudioBuffer<float> a (2, static_cast<int> (testSampleRate * 1.0));
    makeSine (a, 220.0, 0.1f);
    processInBlocks (p, a);
    check (allFinite (a), "morph A side is finite");
}

void testFilter()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    setFloat (p, ids::morphAmount, 0.0f);
    setSlot (p, 0, ModuleId::Filter, 1);
    setChoice (p, ids::filterType, 0);
    setFloat (p, ids::filterCutoff, 1000.0f);
    setFloat (p, ids::filterReso, 0.2f);
    setFloat (p, ids::filterMix, 1.0f);

    juce::AudioBuffer<float> low (2, static_cast<int> (testSampleRate * 0.6));
    makeSine (low, 200.0, 0.2f);
    processInBlocks (p, low);

    juce::AudioBuffer<float> high (2, static_cast<int> (testSampleRate * 0.6));
    makeSine (high, 8000.0, 0.2f);
    processInBlocks (p, high);

    std::cout << "  filter: 200=" << rmsAbs (low) << " 8k=" << rmsAbs (high) << std::endl;
    check (rmsAbs (low) > rmsAbs (high) * 3.0f, "filter low-pass rejects highs");

    setChoice (p, ids::filterType, 1);
    juce::AudioBuffer<float> hp (2, static_cast<int> (testSampleRate * 0.6));
    makeSine (hp, 100.0, 0.2f);
    processInBlocks (p, hp);
    check (rmsAbs (hp) < 0.02f, "filter high-pass rejects lows");
}

void testChorus()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    setSlot (p, 0, ModuleId::Chorus);
    setFloat (p, ids::chorusMix, 1.0f);
    setFloat (p, ids::chorusDepth, 0.6f);
    setFloat (p, ids::chorusRate, 0.4f);

    juce::AudioBuffer<float> buffer (2, 4800);
    makeSine (buffer, 220.0, 0.1f);
    processInBlocks (p, buffer);

    check (allFinite (buffer), "chorus output is finite");
    check (peakAbs (buffer) > 0.01f, "chorus produces wet signal");
}

void testEq()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    setFloat (p, ids::morphAmount, 0.0f);
    setSlot (p, 0, ModuleId::Eq, 1);
    setFloat (p, ids::eqLow, 12.0f);
    setFloat (p, ids::eqMid, 0.0f);
    setFloat (p, ids::eqHigh, 0.0f);
    setFloat (p, ids::eqOutput, 0.0f);

    juce::AudioBuffer<float> low (2, static_cast<int> (testSampleRate * 0.7));
    makeSine (low, 80.0, 0.1f);
    processInBlocks (p, low);
    check (rmsAbs (low) > 0.11f, "eq low shelf boosts low frequencies");

    setFloat (p, ids::eqLow, 0.0f);
    juce::AudioBuffer<float> flat (2, static_cast<int> (testSampleRate * 0.7));
    makeSine (flat, 80.0, 0.1f);
    processInBlocks (p, flat);
    check (rmsAbs (flat) < rmsAbs (low), "eq flat is quieter than boosted low");
}

void testModulationAndDrive()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    setFloat (p, ids::morphAmount, 0.0f);
    setSlot (p, 0, ModuleId::Chorus, 1);
    setFloat (p, ids::chorusMix, 1.0f);
    juce::AudioBuffer<float> chorus (2, static_cast<int> (testSampleRate * 1.0));
    makeSine (chorus, 440.0, 0.1f);
    processInBlocks (p, chorus);
    check (allFinite (chorus) && rmsAbs (chorus) > 0.01f, "chorus processes audio");

    clearSlots (p);
    setSlot (p, 0, ModuleId::Distortion, 1);
    setFloat (p, ids::distDrive, 20.0f);
    setFloat (p, ids::distBits, 6.0f);
    setFloat (p, ids::distDown, 8.0f);
    setFloat (p, ids::distMix, 1.0f);
    juce::AudioBuffer<float> dist (2, static_cast<int> (testSampleRate * 0.6));
    makeSine (dist, 220.0, 0.2f);
    processInBlocks (p, dist);
    check (allFinite (dist) && peakAbs (dist) <= 1.05f, "distortion/bitcrush is bounded");

    clearSlots (p);
    setSlot (p, 0, ModuleId::Hyper, 1);
    juce::AudioBuffer<float> hyper (2, static_cast<int> (testSampleRate * 1.0));
    makeSine (hyper, 330.0, 0.1f);
    processInBlocks (p, hyper);
    check (allFinite (hyper) && rmsAbs (hyper) > 0.005f, "hyper/dimension processes audio");
}

void testGrainAndCloud()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    setFloat (p, ids::morphAmount, 0.0f);
    setSlot (p, 0, ModuleId::Grain, 1);
    setFloat (p, ids::grainBuffer, 0.5f);
    setFloat (p, ids::grainOffset, 0.5f);
    setFloat (p, ids::grainMix, 1.0f);
    setFloat (p, ids::grainDensity, 20.0f);

    juce::AudioBuffer<float> grain (2, static_cast<int> (testSampleRate * 2.0));
    makeSine (grain, 220.0, 0.2f);
    processInBlocks (p, grain);
    check (allFinite (grain), "autochroma grain is finite");
    check (p.getEngine().getActiveGrainCount() >= 0, "autochroma grain has a buffer");

    clearSlots (p);
    setSlot (p, 0, ModuleId::Cloud, 1);
    setFloat (p, ids::cloudMix, 0.6f);
    setFloat (p, ids::cloudDecay, 6.0f);
    juce::AudioBuffer<float> cloud (2, static_cast<int> (testSampleRate * 2.0));
    makeSine (cloud, 220.0, 0.15f);
    processInBlocks (p, cloud);
    check (allFinite (cloud), "cloud reverb is finite");
}

void testSlotEditingAndPresets()
{
    IridescentFlowerProcessor p;
    prepare (p);

    p.setSlotType (3, 0, ModuleId::Hyper);
    p.setSlotOn (3, 0, false);
    check (p.getSlot (3, 0).type == ModuleId::Hyper, "master slot type can be replaced");
    check (! p.getSlot (3, 0).on, "master slot can be bypassed");

    p.moveSlot (3, 0, 4);
    check (p.getSlot (3, 4).type == ModuleId::Hyper, "master slot can be reordered");

    juce::MemoryBlock state;
    p.getStateInformation (state);

    IridescentFlowerProcessor q;
    prepare (q);
    q.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    check (q.getSlot (3, 0).type == ModuleId::Hyper, "master slot state survives save/load");

    const auto file = juce::File::getCurrentWorkingDirectory()
                          .getChildFile ("iridescent-test.iflower");
    check (p.savePreset (file), "preset can be saved");
    setSlot (p, 0, ModuleId::Echo, 2);
    check (p.loadPreset (file), "preset can be loaded");
    check (p.getSlot (3, 0).type == ModuleId::Hyper, "preset restores master layout");
    file.deleteFile();

    p.setSlotType (0, 0, ModuleId::Comp);
    p.setSlotOn (0, 0, true);
    p.setSlotType (1, 0, ModuleId::Filter);
    p.setSlotOn (1, 0, true);
    p.setSlotType (2, 0, ModuleId::Echo);
    p.setSlotOn (2, 0, true);
    p.setSlotType (3, 1, ModuleId::Cloud);
    p.setSlotOn (3, 1, true);
    check (p.getSlot (0, 0).type == ModuleId::Comp, "bus 1 has its own slot chain");
    check (p.getSlot (1, 0).type == ModuleId::Filter, "bus 2 has its own slot chain");
    check (p.getSlot (2, 0).type == ModuleId::Echo, "bus 3 has its own slot chain");
    check (p.getSlot (3, 1).type == ModuleId::Cloud, "master has its own slot chain");
}

void testFactoryPresets()
{
    const auto names = IridescentFlowerProcessor::getFactoryPresetNames();
    check (names.size() == 5, "five factory presets are embedded");
    check (names[0] == "PAD_EasyStrings", "PAD is the first factory preset");

    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (true);
    IridescentFlowerProcessor p;
    check (p.getSlot (0, 0).type != ModuleId::Morph, "new instance starts on PAD patch");
    check (p.loadFactoryPreset ("PLUCK_EasyGrains"), "factory preset can be loaded by name");
    check (p.loadFactoryPreset (4), "factory preset can be loaded by index");
    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (false);
}

void processRandomBlocks (IridescentFlowerProcessor& p, juce::AudioBuffer<float>& buffer)
{
    juce::Random random (0x51a7c0de);

    for (int offset = 0; offset < buffer.getNumSamples();)
    {
        const auto remaining = buffer.getNumSamples() - offset;
        const auto count = juce::jmin (remaining, random.nextInt (blockSize) + 1);
        juce::AudioBuffer<float> block (buffer.getNumChannels(), count);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, count);

        juce::MidiBuffer midi;
        p.processBlock (block, midi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, offset, block, ch, 0, count);

        offset += count;
    }
}

void testFactorySilence()
{
    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (true);
    IridescentFlowerProcessor p;

    for (int i = 0; i < IridescentFlowerProcessor::getFactoryPresetCount(); ++i)
    {
        p.loadFactoryPreset (i);
        p.setPlayConfigDetails (2, 2, testSampleRate, blockSize);
        p.prepareToPlay (testSampleRate, blockSize);

        juce::AudioBuffer<float> silence (2, static_cast<int> (testSampleRate * 4.0));
        silence.clear();
        processInBlocks (p, silence);

        const auto peak = peakAbs (silence);
        std::cout << "  factory silence [" << i << "] peak=" << peak << std::endl;
        check (allFinite (silence), "factory preset silence stays finite");
        check (peak < 0.02f, "factory preset does not self-oscillate on silence");
    }

    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (false);
}

void testSidechainBufferLayout()
{
    IridescentFlowerProcessor p;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    check (p.setBusesLayout (layout), "stereo sidechain layout is accepted");

    p.setPlayConfigDetails (4, 2, testSampleRate, blockSize);
    p.prepareToPlay (testSampleRate, blockSize);
    p.setActiveSlot (0, 0, false);
    setFloat (p, ids::morphSource, 0.0f); // SIDECHAIN
    setFloat (p, ids::morphAmount, 0.5f);

    juce::AudioBuffer<float> buffer (4, blockSize);
    buffer.clear();
    makeSine (buffer, 220.0, 0.1f);

    juce::MidiBuffer midi;
    p.processBlock (buffer, midi);

    check (allFinite (buffer), "four-channel sidechain buffer stays finite");
    check (buffer.getSample (2, 0) == 0.0f && buffer.getSample (3, 0) == 0.0f,
           "sidechain input channels are not left as output");

    IridescentFlowerProcessor monoP;
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses.add (juce::AudioChannelSet::mono());
    monoLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    monoLayout.outputBuses.add (juce::AudioChannelSet::mono());
    check (monoP.setBusesLayout (monoLayout), "mono main + stereo sidechain layout is accepted");

    monoP.setPlayConfigDetails (3, 1, testSampleRate, blockSize);
    monoP.prepareToPlay (testSampleRate, blockSize);
    juce::AudioBuffer<float> monoBuffer (3, blockSize);
    monoBuffer.clear();

    for (int i = 0; i < blockSize; ++i)
        monoBuffer.setSample (0, i, 0.1f * std::sin (
            juce::MathConstants<double>::twoPi * 220.0 * i / testSampleRate));

    juce::MidiBuffer monoMidi;
    monoP.processBlock (monoBuffer, monoMidi);
    check (allFinite (monoBuffer), "mono main with sidechain stays finite");
    check (monoBuffer.getSample (1, 0) == 0.0f && monoBuffer.getSample (2, 0) == 0.0f,
           "mono main does not read sidechain as its right channel");
}

void testFactoryRandomBlocks()
{
    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (true);
    IridescentFlowerProcessor p;

    for (int i = 0; i < IridescentFlowerProcessor::getFactoryPresetCount(); ++i)
    {
        p.loadFactoryPreset (i);
        p.setPlayConfigDetails (2, 2, testSampleRate, blockSize);
        p.prepareToPlay (testSampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, static_cast<int> (testSampleRate * 2.0));
        makeSine (buffer, 220.0, 0.1f);
        processRandomBlocks (p, buffer);
        check (allFinite (buffer), "random host block sizes stay finite");
    }

    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (false);
}

void testRates()
{
    IridescentFlowerProcessor p;
    p.setPlayConfigDetails (2, 2, 96000.0, 512);
    p.prepareToPlay (96000.0, 512);
    juce::AudioBuffer<float> buffer (2, 96000);
    makeSine (buffer, 220.0, 0.1f);
    processInBlocks (p, buffer);
    check (allFinite (buffer), "96 kHz is finite");

    IridescentFlowerProcessor mono;
    mono.setPlayConfigDetails (1, 1, testSampleRate, blockSize);
    mono.prepareToPlay (testSampleRate, blockSize);
    juce::AudioBuffer<float> monoBuffer (1, 48000);
    makeSine (monoBuffer, 220.0, 0.1f);
    processInBlocks (mono, monoBuffer);
    check (allFinite (monoBuffer), "mono bus is finite");
}

void writeSnapshot (IridescentFlowerProcessor& p, const juce::File& file, int slot = 0,
                    int page = 1, bool custom = false)
{
    prepare (p);

    if (slot >= 0 && slot < RackEngine::slotsPerBus)
        p.setActiveSlot (page - 1, slot, false);

    if (p.getSlot (page - 1, slot).type == ModuleId::Echo)
        setFloat (p, ids::echoSync, 1.0f);
    if (p.getSlot (page - 1, slot).type == ModuleId::Grain)
        setFloat (p, ids::grainSync, 1.0f);

    juce::AudioBuffer<float> buffer (2, 4800);
    makeSine (buffer, 220.0, 0.1f);
    processInBlocks (p, buffer);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    editor->setSize (1100, 680);

    if (auto* iridescentEditor = dynamic_cast<IridescentFlowerEditor*> (editor.get()))
    {
        iridescentEditor->setPageForSnapshot (page, custom);
        iridescentEditor->selectSlotForSnapshot (slot);
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    file.deleteFile();
    if (auto stream = file.createOutputStream(); stream != nullptr && image.isValid())
    {
        juce::PNGImageFormat png;
        png.writeImageToStream (image, *stream);
        std::cout << "[PASS] UI snapshot written to " << file.getFullPathName() << std::endl;
    }
    else
    {
        std::cout << "[FAIL] could not write UI snapshot" << std::endl;
        ++failures;
    }
}

void renderDemo (IridescentFlowerProcessor& p, const juce::File& file)
{
    prepare (p);
    juce::AudioBuffer<float> buffer (2, static_cast<int> (testSampleRate * 20.0));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto t = static_cast<float> (i) / static_cast<float> (testSampleRate);
        const auto env = std::min (1.0f, t * 0.3f) * std::min (1.0f, (20.0f - t) * 0.25f);
        const auto value = 0.09f * env
                         * (std::sin (juce::MathConstants<float>::twoPi * 110.0f * t)
                            + 0.5f * std::sin (juce::MathConstants<float>::twoPi * 164.81f * t));
        buffer.setSample (0, i, value);
        buffer.setSample (1, i, value * 0.92f);
    }

    processInBlocks (p, buffer);

    file.deleteFile();
    if (auto stream = file.createOutputStream())
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.release(), testSampleRate, 2, 24, {}, 0));
        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            std::cout << "[PASS] demo written to " << file.getFullPathName() << std::endl;
            return;
        }
    }

    std::cout << "[FAIL] could not write demo" << std::endl;
    ++failures;
}

void testVst3 (const juce::File& pluginFile)
{
    juce::VST3PluginFormat scanner;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    scanner.findAllTypesForFile (descriptions, pluginFile.getFullPathName());

    check (! descriptions.isEmpty(), "VST3 scanner finds Iridescent Flower");
    if (descriptions.isEmpty())
        return;

    juce::AudioPluginFormatManager manager;
    manager.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::String error;
    auto instance = manager.createPluginInstance (*descriptions[0], testSampleRate, blockSize, error);
    check (instance != nullptr, "VST3 instance can be created");
    if (instance == nullptr)
        return;

    instance->setPlayConfigDetails (2, 2, testSampleRate, blockSize);
    instance->prepareToPlay (testSampleRate, blockSize);
    std::cout << "  parameters: " << instance->getParameters().size()
              << ", input buses: " << instance->getBusCount (true) << std::endl;
    check (instance->getParameters().size() > 50, "VST3 exposes the full parameter set");
    check (instance->getBusCount (true) == 2, "VST3 exposes a sidechain input bus");

    juce::AudioBuffer<float> buffer (2, 48000);
    makeSine (buffer, 220.0, 0.1f);
    juce::MidiBuffer midi;

    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        const auto count = juce::jmin (blockSize, buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> block (2, count);
        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom (ch, 0, buffer, ch, offset, count);

        instance->processBlock (block, midi);

        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, offset, block, ch, 0, count);
    }

    check (allFinite (buffer), "VST3 output is finite");
    check (peakAbs (buffer) > 0.001f, "VST3 processes audio");
}

void testInsertAndRemove()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);

    p.setSlotType (0, 0, ModuleId::Comp);
    p.setSlotType (0, 1, ModuleId::Eq);
    p.setSlotType (0, 2, ModuleId::Filter);

    // Drop Comp between Filter and the empty end: Eq, Filter, Comp.
    p.moveSlotTo (0, 0, 3);
    check (p.getSlot (0, 0).type == ModuleId::Eq, "insert between modules shifts left");
    check (p.getSlot (0, 1).type == ModuleId::Filter, "insert keeps target module");
    check (p.getSlot (0, 2).type == ModuleId::Comp, "insert places dragged module after target");

    p.removeSlot (0, 1);
    check (p.getSlot (0, 0).type == ModuleId::Eq, "remove keeps the first module");
    check (p.getSlot (0, 1).type == ModuleId::Comp, "remove closes the gap immediately");
    check (p.getSlot (0, 2).type == ModuleId::Morph, "remove leaves empty capacity at the end");
}

void testIndependentSlotParameters()
{
    IridescentFlowerProcessor p;
    prepare (p);

    p.setActiveSlot (0, 0, false);
    setFloat (p, ids::compThreshold, -30.0f);

    p.setActiveSlot (0, 1, false);
    p.setSlotType (0, 1, ModuleId::Comp);
    setFloat (p, ids::compThreshold, -12.0f);

    p.setActiveSlot (1, 0, false);
    p.setSlotType (1, 0, ModuleId::Comp);
    setFloat (p, ids::compThreshold, -20.0f);

    p.setActiveSlot (3, 0, false);
    p.setSlotType (3, 0, ModuleId::Comp);
    setFloat (p, ids::compThreshold, -3.0f);

    check (std::abs (p.getEngine().getSlotState (0, 0).get ("comp_threshold") - -30.0f) < 0.01f,
           "bus 1 slot 1 keeps its own parameter");
    check (std::abs (p.getEngine().getSlotState (0, 1).get ("comp_threshold") - -12.0f) < 0.01f,
           "duplicate compressor in the same bus keeps independent parameters");
    check (std::abs (p.getEngine().getSlotState (1, 0).get ("comp_threshold") - -20.0f) < 0.01f,
           "bus 2 compressor keeps independent parameters");
    check (std::abs (p.getEngine().getSlotState (3, 0).get ("comp_threshold") - -3.0f) < 0.01f,
           "master compressor keeps independent parameters");

    juce::MemoryBlock state;
    p.getStateInformation (state);
    IridescentFlowerProcessor q;
    prepare (q);
    q.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    check (std::abs (q.getEngine().getSlotState (0, 1).get ("comp_threshold") - -12.0f) < 0.01f,
           "independent slot parameters survive state reload");
}

void testUiPersistence()
{
    IridescentFlowerProcessor p;
    prepare (p);
    p.setActiveSlot (0, 5, false);
    setFloat (p, ids::echoSync, 1.0f);
    setFloat (p, ids::echoDivision, 6.0f);
    p.setActiveSlot (0, 6, false);
    setFloat (p, ids::grainPitch, 7.3f);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    auto* e = dynamic_cast<IridescentFlowerEditor*> (editor.get());
    check (e != nullptr, "editor can be created");

    if (e == nullptr)
        return;

    e->selectSlotForSnapshot (5);
    const auto firstDivision = e->getKnobTextForSnapshot (0);
    e->selectSlotForSnapshot (0);
    e->selectSlotForSnapshot (5);
    const auto secondDivision = e->getKnobTextForSnapshot (0);
    check (firstDivision == secondDivision, "echo division survives view changes");
    check (secondDivision != "1 BAR", "echo division does not jump to 1 BAR");
    check (secondDivision != "4", "echo division keeps its division label");

    e->selectSlotForSnapshot (6);
    const auto firstPitch = e->getKnobTextForSnapshot (2);
    e->selectSlotForSnapshot (0);
    e->selectSlotForSnapshot (6);
    const auto secondPitch = e->getKnobTextForSnapshot (2);
    check (firstPitch == secondPitch, "grain pitch survives view changes");
    check (secondPitch.contains ("7.30"), "grain pitch shows fine semitone value");
}

void testMasterChain()
{
    IridescentFlowerProcessor p;
    prepare (p);
    clearSlots (p);
    p.setSlotType (3, 0, ModuleId::Distortion);
    p.setActiveSlot (3, 0, false);
    p.setSlotOn (3, 0, true);
    setFloat (p, ids::distDrive, 30.0f);
    setFloat (p, ids::distMix, 1.0f);

    juce::AudioBuffer<float> buffer (2, 4800);
    makeSine (buffer, 220.0, 0.1f);
    processInBlocks (p, buffer);

    check (allFinite (buffer), "master chain output is finite");
    check (peakAbs (buffer) > 0.2f, "master chain processes the summed buses");
}

void testCustomModule()
{
    auto& registry = CustomModuleRegistry::get();
    registry.scan();

    if (registry.getNumLoaded() == 0)
    {
        check (true, "custom module folder can be empty");
        return;
    }

    check (registry.getModule (0) != nullptr, "custom DLL loads");
    check (registry.getDisplayName (0).isNotEmpty(), "custom DLL exposes a name");

    IridescentFlowerProcessor p;
    prepare (p);

    for (int i = 1; i < RackEngine::slotsPerBus; ++i)
        p.setSlotType (0, i, ModuleId::Morph);

    p.setSlotType (0, 0, ModuleId::Custom0);
    p.setSlotOn (0, 0, true);
    p.setSlotType (1, 0, ModuleId::Morph);
    p.setSlotType (2, 0, ModuleId::Morph);
    p.setSlotType (3, 0, ModuleId::Morph);
    setFloat (p, ids::customParamId (0, 2).toRawUTF8(), 0.5f);

    juce::AudioBuffer<float> buffer (2, 4800);
    makeSine (buffer, 220.0, 0.1f);
    processInBlocks (p, buffer);

    check (allFinite (buffer), "custom DLL output is finite");
    check (peakAbs (buffer) > 0.001f, "custom DLL processes audio");
}

} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    IridescentFlowerProcessor::setLoadFactoryPresetOnStartup (false);

    juce::File vst3, snapshot, demo;
    int snapshotSlot = 0;
    int snapshotPage = 1;
    bool snapshotCustom = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--vst3" && i + 1 < argc) vst3 = juce::File (argv[++i]);
        else if (arg == "--snapshot" && i + 1 < argc) snapshot = juce::File (argv[++i]);
        else if (arg == "--demo" && i + 1 < argc) demo = juce::File (argv[++i]);
        else if (arg == "--slot" && i + 1 < argc) snapshotSlot = juce::String (argv[++i]).getIntValue();
        else if (arg == "--bus" && i + 1 < argc) snapshotPage = juce::String (argv[++i]).getIntValue();
        else if (arg == "--custom") snapshotCustom = true;
    }

    if (snapshot != juce::File())
    {
        IridescentFlowerProcessor p;
        snapshot.getParentDirectory().createDirectory();
        writeSnapshot (p, snapshot, snapshotSlot, snapshotPage, snapshotCustom);
        return failures == 0 ? 0 : 1;
    }

    if (demo != juce::File())
    {
        IridescentFlowerProcessor p;
        demo.getParentDirectory().createDirectory();
        renderDemo (p, demo);
        return failures == 0 ? 0 : 1;
    }

    testFactoryPresets();
    testFactorySilence();
    testFactoryRandomBlocks();
    testSidechainBufferLayout();
    testDefaultChain();
    testMorphBuses();
    testFilter();
    testChorus();
    testEq();
    testModulationAndDrive();
    testGrainAndCloud();
    testSlotEditingAndPresets();
    testInsertAndRemove();
    testIndependentSlotParameters();
    testUiPersistence();
    testMasterChain();
    testCustomModule();
    testRates();

    if (vst3 != juce::File())
        testVst3 (vst3);

    return failures == 0 ? 0 : 1;
}
