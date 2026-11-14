#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CustomModuleRegistry.h"

#include <cmath>

namespace
{
using namespace iridescent;

juce::String formatPercent (float v)
{
    return juce::String (juce::roundToInt (v * 100.0f)) + "%";
}

juce::String formatDb (float v)
{
    return (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
}

juce::String formatMs (float v)
{
    if (v < 1000.0f)
        return juce::String (juce::roundToInt (v)) + " ms";

    return juce::String (v / 1000.0f, 2) + " s";
}

juce::String formatSeconds (float v)
{
    return juce::String (v, 2) + " s";
}

juce::String formatHz (float v)
{
    return juce::String (v, 2) + " Hz";
}

juce::String formatSemitones (float v)
{
    return (v >= 0.0f ? "+" : "") + juce::String (v, 2) + " st";
}

float parseNumber (const juce::String& text)
{
    return text.getFloatValue();
}

float parsePercent (const juce::String& text)
{
    return text.getFloatValue() / 100.0f;
}

juce::AudioParameterFloatAttributes floatAttributes (std::function<juce::String (float, int)> display,
                                                      std::function<float (const juce::String&)> parse,
                                                      const juce::String& label)
{
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction (std::move (display))
        .withValueFromStringFunction (std::move (parse))
        .withLabel (label);
}

void addPercent (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                 const char* id, const char* name, float minimum, float maximum, float defaultValue,
                 float skew = 1.0f)
{
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { minimum, maximum, 0.001f, skew }, defaultValue,
        floatAttributes ([] (float v, int) { return formatPercent (v); },
                         [] (const juce::String& s) { return parsePercent (s); }, "%")));
}

void addDb (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
            const char* id, const char* name, float minimum, float maximum, float defaultValue)
{
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { minimum, maximum, 0.1f }, defaultValue,
        floatAttributes ([] (float v, int) { return formatDb (v); },
                         [] (const juce::String& s) { return parseNumber (s); }, "dB")));
}

void addTime (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
              const char* id, const char* name, float minimum, float maximum, float defaultValue)
{
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> { minimum, maximum, 0.1f, 0.35f }, defaultValue,
        floatAttributes ([] (float v, int) { return formatMs (v); },
                         [] (const juce::String& s) { return parseNumber (s); }, "ms")));
}

void addToggle (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                const char* id, const char* name, bool defaultValue)
{
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, 1 }, name, defaultValue));
}

const juce::StringArray& rhythmDivisions()
{
    static const juce::StringArray divisions { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 BAR" };
    return divisions;
}
} // namespace

IridescentFlowerProcessor::IridescentFlowerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout()),
      engine (parameters)
{
    setLatencySamples (0);

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            parameters.addParameterListener (ranged->getParameterID(), this);
}

IridescentFlowerProcessor::~IridescentFlowerProcessor()
{
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            parameters.removeParameterListener (ranged->getParameterID(), this);
}

juce::AudioProcessorValueTreeState::ParameterLayout IridescentFlowerProcessor::createParameterLayout()
{
    using namespace juce;
    using namespace iridescent;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    addPercent (params, ids::morphAmount, "Morph", 0.0f, 1.0f, 0.0f);
    addDb (params, ids::morphBTrim, "B Trim", -24.0f, 12.0f, 0.0f);
    addPercent (params, ids::morphCurve, "Curve", 0.0f, 1.0f, 0.65f);
    addPercent (params, ids::morphBWidth, "B Width", 0.0f, 1.5f, 1.0f);
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::morphSource, 1 }, "B Source",
        StringArray { "SIDECHAIN", "BUS 2", "INTERNAL" }, 1));

    addDb (params, ids::compThreshold, "Threshold", -60.0f, 0.0f, -18.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::compRatio, 1 }, "Ratio",
        NormalisableRange<float> { 1.0f, 20.0f, 0.01f }, 3.0f,
        floatAttributes ([] (float v, int) { return String (v, 1) + ":1"; },
                         [] (const String& s) { return parseNumber (s); }, ":1")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::compAttack, 1 }, "Attack",
        NormalisableRange<float> { 0.5f, 100.0f, 0.1f, 0.5f }, 10.0f,
        floatAttributes ([] (float v, int) { return String (v, 1) + " ms"; },
                         [] (const String& s) { return parseNumber (s); }, "ms")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::compRelease, 1 }, "Release",
        NormalisableRange<float> { 10.0f, 1000.0f, 0.1f, 0.5f }, 150.0f,
        floatAttributes ([] (float v, int) { return String (v, 0) + " ms"; },
                         [] (const String& s) { return parseNumber (s); }, "ms")));
    addDb (params, ids::compMakeup, "Makeup", 0.0f, 24.0f, 12.0f);
    addPercent (params, ids::compMix, "Mix", 0.0f, 1.0f, 1.0f);

    addDb (params, ids::eqLow, "Low", -18.0f, 18.0f, 0.0f);
    addDb (params, ids::eqMid, "Mid", -18.0f, 18.0f, 0.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::eqMidFreq, 1 }, "Mid Freq",
        NormalisableRange<float> { 200.0f, 8000.0f, 0.1f, 0.4f }, 1000.0f,
        floatAttributes ([] (float v, int) { return formatHz (v); },
                         [] (const String& s) { return parseNumber (s); }, "Hz")));
    addDb (params, ids::eqHigh, "High", -18.0f, 18.0f, 0.0f);
    addDb (params, ids::eqOutput, "Output", -12.0f, 12.0f, 0.0f);

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::filterCutoff, 1 }, "Cutoff",
        NormalisableRange<float> { 20.0f, 20000.0f, 0.01f, 0.28f }, 3200.0f,
        floatAttributes ([] (float v, int) { return String (roundToInt (v)) + " Hz"; },
                         [] (const String& s) { return parseNumber (s); }, "Hz")));
    addPercent (params, ids::filterReso, "Resonance", 0.0f, 1.0f, 0.35f);
    addDb (params, ids::filterDrive, "Drive", 0.0f, 24.0f, 0.0f);
    addPercent (params, ids::filterMix, "Mix", 0.0f, 1.0f, 1.0f);
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::filterType, 1 }, "Type",
        StringArray { "LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH" }, 0));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::chorusMode, 1 }, "Mode",
        StringArray { "CHORUS", "FLANGER" }, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::chorusRate, 1 }, "Rate",
        NormalisableRange<float> { 0.05f, 5.0f, 0.001f, 0.45f }, 0.35f,
        floatAttributes ([] (float v, int) { return String (v, 2) + " Hz"; },
                         [] (const String& s) { return parseNumber (s); }, "Hz")));
    addPercent (params, ids::chorusDepth, "Depth", 0.0f, 1.0f, 0.5f);
    addPercent (params, ids::chorusFeedback, "Feedback", 0.0f, 0.9f, 0.15f);
    addPercent (params, ids::chorusMix, "Mix", 0.0f, 1.0f, 0.45f);

    addDb (params, ids::distDrive, "Drive", 0.0f, 36.0f, 8.0f);
    addPercent (params, ids::distTone, "Tone", 0.0f, 1.0f, 0.65f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::distBits, 1 }, "Bits",
        NormalisableRange<float> { 2.0f, 16.0f, 1.0f }, 12.0f,
        floatAttributes ([] (float v, int) { return String (roundToInt (v)) + " bit"; },
                         [] (const String& s) { return parseNumber (s); }, "bit")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::distDown, 1 }, "Downsample",
        NormalisableRange<float> { 1.0f, 64.0f, 1.0f }, 2.0f,
        floatAttributes ([] (float v, int) { return String (roundToInt (v)) + "x"; },
                         [] (const String& s) { return parseNumber (s); }, "x")));
    addPercent (params, ids::distMix, "Mix", 0.0f, 1.0f, 0.55f);
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::distMode, 1 }, "Mode",
        StringArray { "SOFT", "HARD", "FOLD" }, 0));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::hyperDetune, 1 }, "Detune",
        NormalisableRange<float> { 0.0f, 50.0f, 0.01f }, 18.0f,
        floatAttributes ([] (float v, int) { return String (v, 1) + " ct"; },
                         [] (const String& s) { return parseNumber (s); }, "ct")));
    addPercent (params, ids::hyperWidth, "Width", 0.0f, 1.0f, 0.8f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::hyperRate, 1 }, "Rate",
        NormalisableRange<float> { 0.02f, 2.0f, 0.001f, 0.5f }, 0.18f,
        floatAttributes ([] (float v, int) { return String (v, 2) + " Hz"; },
                         [] (const String& s) { return parseNumber (s); }, "Hz")));
    addPercent (params, ids::hyperMix, "Mix", 0.0f, 1.0f, 0.65f);

    addTime (params, ids::echoTime, "Delay Time", 20.0f, 2000.0f, 420.0f);
    addPercent (params, ids::echoFeedback, "Feedback", 0.0f, 0.95f, 0.38f);
    addPercent (params, ids::echoDiffusion, "Diffusion", 0.0f, 1.0f, 0.45f);
    addPercent (params, ids::echoTone, "Tone", 0.0f, 1.0f, 0.55f);
    addPercent (params, ids::echoMix, "Mix", 0.0f, 1.0f, 0.35f);
    addToggle (params, ids::echoPingpong, "Ping Pong", false);
    addToggle (params, ids::echoSync, "Sync", false);
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::echoDiv, 1 }, "Division", rhythmDivisions(), 4));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::echoDivision, 1 }, "Division Step",
        NormalisableRange<float> { 0.0f, 8.0f, 1.0f }, 4.0f));

    addTime (params, ids::grainSize, "Size", 10.0f, 500.0f, 120.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::grainDensity, 1 }, "Density",
        NormalisableRange<float> { 1.0f, 60.0f, 0.01f }, 18.0f,
        floatAttributes ([] (float v, int) { return String (v, 1) + " /s"; },
                         [] (const String& s) { return parseNumber (s); }, "/s")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::grainPitch, 1 }, "Pitch",
        NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        floatAttributes ([] (float v, int) { return formatSemitones (v); },
                         [] (const String& s) { return parseNumber (s); }, "st")));
    addPercent (params, ids::grainSpray, "Spray", 0.0f, 1.0f, 0.35f);
    addPercent (params, ids::grainFeedback, "Feedback", 0.0f, 0.9f, 0.15f);
    addPercent (params, ids::grainMix, "Mix", 0.0f, 1.0f, 0.35f);
    addPercent (params, ids::grainReverse, "Reverse", 0.0f, 1.0f, 0.1f);
    addPercent (params, ids::grainWidth, "Width", 0.0f, 1.0f, 0.8f);
    addToggle (params, ids::grainFreeze, "Freeze", false);
    addToggle (params, ids::grainSync, "Sync", false);
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::grainDiv, 1 }, "Division", rhythmDivisions(), 2));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::grainDivision, 1 }, "Division Step",
        NormalisableRange<float> { 0.0f, 8.0f, 1.0f }, 2.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::grainBuffer, 1 }, "Buffer",
        NormalisableRange<float> { 0.1f, 3.0f, 0.001f, 0.6f }, 1.5f,
        floatAttributes ([] (float v, int) { return String (v, 2) + " s"; },
                         [] (const String& s) { return parseNumber (s); }, "s")));
    addPercent (params, ids::grainOffset, "Offset", 0.0f, 1.0f, 0.5f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::grainRate, 1 }, "Grain Rate",
        NormalisableRange<float> { 0.25f, 4.0f, 0.01f, 0.5f }, 1.0f,
        floatAttributes ([] (float v, int) { return String (v, 2) + "x"; },
                         [] (const String& s) { return parseNumber (s); }, "x")));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::grainSnap, 1 }, "Snap",
        StringArray { "OFF", "1/16", "1/8", "1/4" }, 0));

    addPercent (params, ids::cloudSize, "Size", 0.1f, 1.0f, 0.62f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::cloudDecay, 1 }, "Decay",
        NormalisableRange<float> { 0.2f, 12.0f, 0.001f, 0.45f }, 5.5f,
        floatAttributes ([] (float v, int) { return formatSeconds (v); },
                         [] (const String& s) { return parseNumber (s); }, "s")));
    addPercent (params, ids::cloudDamping, "Damping", 0.0f, 1.0f, 0.55f);
    addPercent (params, ids::cloudMod, "Mod", 0.0f, 1.0f, 0.35f);
    addTime (params, ids::cloudPredelay, "Pre-Delay", 0.0f, 100.0f, 12.0f);
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::cloudLowcut, 1 }, "Low Cut",
        NormalisableRange<float> { 20.0f, 500.0f, 0.1f, 0.5f }, 110.0f,
        floatAttributes ([] (float v, int) { return formatHz (v); },
                         [] (const String& s) { return parseNumber (s); }, "Hz")));
    addPercent (params, ids::cloudWidth, "Width", 0.0f, 1.0f, 0.8f);
    addPercent (params, ids::cloudMix, "Mix", 0.0f, 1.0f, 0.42f);

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::lfoRate, 1 }, "Rate",
        NormalisableRange<float> { 0.01f, 8.0f, 0.001f, 0.4f }, 0.08f,
        floatAttributes ([] (float v, int) { return String (v, 2) + " Hz"; },
                         [] (const String& s) { return parseNumber (s); }, "Hz")));
    addPercent (params, ids::lfoDepth, "Morph Depth", 0.0f, 1.0f, 0.0f);
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::lfoShape, 1 }, "Shape",
        StringArray { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "RANDOM" }, 0));
    addPercent (params, ids::lfoPhase, "Phase", 0.0f, 1.0f, 0.0f);
    addPercent (params, ids::lfoPan, "Auto Pan", 0.0f, 1.0f, 0.0f);
    addPercent (params, ids::lfoStereo, "Stereo", 0.0f, 1.0f, 0.5f);
    addToggle (params, ids::lfoOn, "LFO On", true);

    addPercent (params, ids::globalMix, "Global Mix", 0.0f, 1.0f, 1.0f);
    addDb (params, ids::globalOutput, "Output", -24.0f, 12.0f, 4.0f);
    addPercent (params, ids::bus1Mix, "Bus 1 Out", 0.0f, 1.0f, 1.0f);
    addPercent (params, ids::bus2Mix, "Bus 2 Out", 0.0f, 1.0f, 0.0f);
    addPercent (params, ids::bus3Mix, "Bus 3 Out", 0.0f, 1.0f, 0.0f);
    addToggle (params, ids::visuals, "Visuals", true);

    auto& customRegistry = CustomModuleRegistry::get();
    customRegistry.scan();

    for (int module = 0; module < CustomModuleRegistry::maxModules; ++module)
    {
        const auto* loaded = customRegistry.getModule (module);

        for (int p = 0; p < CustomModuleRegistry::paramsPerModule; ++p)
        {
            const auto id = ids::customParamId (module, p);
            auto name = "C" + juce::String (module + 1) + " P" + juce::String (p + 1);
            float defaultValue = 0.5f;

            if (loaded != nullptr && loaded->info->params[p].name != nullptr)
                name = loaded->info->params[p].name;

            if (loaded != nullptr)
                defaultValue = juce::jlimit (0.0f, 1.0f, loaded->info->params[p].defaultValue);

            addPercent (params, id.toRawUTF8(), name.toRawUTF8(), 0.0f, 1.0f, defaultValue);
        }
    }

    return { params.begin(), params.end() };
}

juce::AudioProcessorEditor* IridescentFlowerProcessor::createEditor()
{
    return new IridescentFlowerEditor (*this);
}

void IridescentFlowerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void IridescentFlowerProcessor::releaseResources()
{
    engine.reset();
}

bool IridescentFlowerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (main != output || (main != juce::AudioChannelSet::mono()
                           && main != juce::AudioChannelSet::stereo()))
        return false;

    return layouts.getChannelSet (true, 1).isDisabled()
        || layouts.getChannelSet (true, 1) == juce::AudioChannelSet::stereo();
}

void IridescentFlowerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    juce::AudioBuffer<float> sidechain;
    bool sidechainBusActive = false;

    if (auto* bus = getBus (true, 1); bus != nullptr
        && bus->isEnabled()
        && bus->getNumberOfChannels() > 0
        && buffer.getNumChannels() >= getTotalNumInputChannels())
    {
        const auto scBuffer = getBusBuffer (buffer, true, 1);
        sidechainBusActive = scBuffer.getNumSamples() > 0;
        if (sidechainBusActive)
            sidechain.makeCopyOf (scBuffer, true);
    }

    double bpm = engine.getTempo();
    bool isPlaying = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                bpm = juce::jlimit (20.0, 300.0, *hostBpm);

            isPlaying = position->getIsPlaying();
        }
    }

    engine.setTempo (bpm);
    engine.setPlaying (isPlaying);
    engine.process (buffer, sidechainBusActive ? &sidechain : nullptr, sidechainBusActive);
}

iridescent::RackEngine::Slot IridescentFlowerProcessor::getSlot (int index) const
{
    return engine.getSlot (index);
}

iridescent::RackEngine::Slot IridescentFlowerProcessor::getSlot (int bus, int index) const
{
    return engine.getSlot (bus, index);
}

void IridescentFlowerProcessor::setSlotType (int index, ModuleId type)
{
    setSlotType (0, index, type);
}

void IridescentFlowerProcessor::syncActiveSlotFromParameters()
{
    if (syncingParameters)
        return;

    auto& state = engine.getSlotState (activeBus, activeSlot);

    for (int i = 0; i < state.getNumValues(); ++i)
    {
        const auto id = state.getParameterId (i);

        if (auto* raw = parameters.getRawParameterValue (id))
            state.setValueAt (i, raw->load (std::memory_order_relaxed));
    }
}

void IridescentFlowerProcessor::loadActiveSlotToParameters()
{
    auto& state = engine.getSlotState (activeBus, activeSlot);
    const juce::ScopedValueSetter<bool> guard (syncingParameters, true);

    for (int i = 0; i < state.getNumValues(); ++i)
    {
        const auto id = state.getParameterId (i);

        if (auto* parameter = parameters.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (state.getValueAt (i)));
    }
}

void IridescentFlowerProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (syncingParameters)
        return;

    auto& state = engine.getSlotState (activeBus, activeSlot);

    if (state.has (parameterID.toRawUTF8()))
        state.set (parameterID.toRawUTF8(), newValue);
}

void IridescentFlowerProcessor::setActiveSlot (int bus, int index, bool syncPrevious)
{
    bus = juce::jlimit (0, 3, bus);
    index = juce::jlimit (0, RackEngine::slotsPerBus - 1, index);

    if (bus == activeBus && index == activeSlot)
    {
        if (syncPrevious)
            syncActiveSlotFromParameters();
        return;
    }

    if (syncPrevious)
        syncActiveSlotFromParameters();

    activeBus = bus;
    activeSlot = index;
    loadActiveSlotToParameters();
}

void IridescentFlowerProcessor::setSlotType (int bus, int index, ModuleId type)
{
    engine.setSlotType (bus, index, type);

    if (bus == activeBus && index == activeSlot)
        loadActiveSlotToParameters();
}

void IridescentFlowerProcessor::setSlotOn (int index, bool on)
{
    engine.setSlotOn (index, on);
}

void IridescentFlowerProcessor::setSlotOn (int bus, int index, bool on)
{
    engine.setSlotOn (bus, index, on);
}

void IridescentFlowerProcessor::moveSlot (int from, int to)
{
    moveSlot (0, from, to);
}

void IridescentFlowerProcessor::moveSlot (int bus, int from, int to)
{
    syncActiveSlotFromParameters();
    engine.moveSlot (bus, from, to);
}

void IridescentFlowerProcessor::moveSlotTo (int bus, int from, int insertion)
{
    syncActiveSlotFromParameters();
    engine.moveSlotTo (bus, from, insertion);
}

void IridescentFlowerProcessor::removeSlot (int bus, int index)
{
    syncActiveSlotFromParameters();
    engine.removeSlot (bus, index);
}

void IridescentFlowerProcessor::writeSlotState (juce::ValueTree& tree) const
{
    juce::String types, ons;
    bool first = true;

    for (int chain = 0; chain < RackEngine::numChains; ++chain)
    {
        for (int i = 0; i < RackEngine::slotsPerBus; ++i)
        {
            const auto slot = engine.getSlot (chain, i);
            if (! first) { types << ","; ons << ","; }
            first = false;
            types << static_cast<int> (slot.type);
            ons << (slot.on ? 1 : 0);
        }
    }

    tree.setProperty ("slotTypes", types, nullptr);
    tree.setProperty ("slotOn", ons, nullptr);

    juce::String values;
    bool firstValue = true;

    for (int chain = 0; chain < RackEngine::numChains; ++chain)
    {
        for (int i = 0; i < RackEngine::slotsPerBus; ++i)
        {
            const auto& state = engine.getSlotState (chain, i);

            for (int p = 0; p < state.getNumValues(); ++p)
            {
                if (! firstValue)
                    values << ";";
                firstValue = false;
                values << chain << "_" << i << "_" << p << "="
                       << juce::String (state.getValueAt (p), 6);
            }
        }
    }

    tree.setProperty ("slotParams", values, nullptr);
}

void IridescentFlowerProcessor::readSlotState (const juce::ValueTree& tree)
{
    const auto types = juce::StringArray::fromTokens (tree.getProperty ("slotTypes").toString(), ",", "");
    const auto ons = juce::StringArray::fromTokens (tree.getProperty ("slotOn").toString(), ",", "");

    for (int chain = 0; chain < RackEngine::numChains; ++chain)
    {
        for (int i = 0; i < RackEngine::slotsPerBus; ++i)
        {
            const auto index = chain * RackEngine::slotsPerBus + i;

            if (index < types.size())
                engine.setSlotType (chain, i, static_cast<ModuleId> (types[index].getIntValue()));
            if (index < ons.size())
                engine.setSlotOn (chain, i, ons[index].getIntValue() > 0);
        }

        const auto valueTokens = juce::StringArray::fromTokens (
            tree.getProperty ("slotParams").toString(), ";", "");

        for (const auto& token : valueTokens)
        {
            const auto equals = token.indexOfChar ('=');
            if (equals <= 0)
                continue;

            const auto key = token.substring (0, equals);
            const auto parts = juce::StringArray::fromTokens (key, "_", "");
            if (parts.size() != 3 || parts[0].getIntValue() != chain)
                continue;

            const auto slotIndex = parts[1].getIntValue();
            const auto paramIndex = parts[2].getIntValue();
            engine.getSlotState (chain, slotIndex).setValueAt (
                paramIndex, token.substring (equals + 1).getFloatValue());
        }

        // Delete gaps left by older presets: modules slide left, empties stay at the end.
        int lastNonEmpty = -1;

        for (int i = 0; i < RackEngine::slotsPerBus; ++i)
            if (engine.getSlot (chain, i).type != ModuleId::Morph)
                lastNonEmpty = i;

        for (int i = 0; i < lastNonEmpty;)
        {
            if (engine.getSlot (chain, i).type == ModuleId::Morph)
            {
                engine.removeSlot (chain, i);
                --lastNonEmpty;
            }
            else
            {
                ++i;
            }
        }
    }
}

void IridescentFlowerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    syncActiveSlotFromParameters();
    writeSlotState (parameters.state);
    juce::MemoryOutputStream stream (destData, true);
    parameters.state.writeToStream (stream);
}

void IridescentFlowerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto tree = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));

    if (tree.isValid())
    {
        parameters.replaceState (tree);
        readSlotState (tree);
        loadActiveSlotToParameters();
    }
}

juce::File IridescentFlowerProcessor::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Iridescent Flower")
        .getChildFile ("Presets");
}

bool IridescentFlowerProcessor::savePreset (const juce::File& file)
{
    juce::MemoryBlock state;
    getStateInformation (state);
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    if (auto stream = file.createOutputStream())
    {
        stream->write (state.getData(), state.getSize());
        stream->flush();
        return true;
    }

    return false;
}

bool IridescentFlowerProcessor::loadPreset (const juce::File& file)
{
    juce::MemoryBlock state;
    if (! file.loadFileAsData (state))
        return false;

    setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IridescentFlowerProcessor();
}
