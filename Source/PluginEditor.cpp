#include "PluginEditor.h"
#include "CustomModuleRegistry.h"
#include "UI/PixelFont.h"

#include <BinaryData.h>

#include <algorithm>
#include <cmath>
#include <iterator>

using iridescent::IridescentLookAndFeel;
using iridescent::ModuleId;
using iridescent::PixelFont;
using iridescent::RackEngine;

namespace
{
constexpr int headerHeight = 96;
constexpr int rackY = 122;
constexpr int rackHeight = 96;
constexpr int detailY = 226;
constexpr int detailHeight = 378;
constexpr int footerY = 608;
constexpr int cardWidth = 112;
constexpr int cardGap = 4;
constexpr int rackX = 20;

const juce::StringArray& divisions()
{
    static const juce::StringArray d { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 BAR" };
    return d;
}

juce::String moduleDisplayName (ModuleId type)
{
    if (type >= ModuleId::Custom0 && type < ModuleId::NumModules)
        return iridescent::CustomModuleRegistry::get().getDisplayName (type - ModuleId::Custom0);

    return iridescent::moduleSpec (type).name;
}

juce::String moduleDisplayTagline (ModuleId type)
{
    if (type >= ModuleId::Custom0 && type < ModuleId::NumModules)
        return iridescent::CustomModuleRegistry::get().getTagline (type - ModuleId::Custom0);

    return iridescent::moduleSpec (type).tagline;
}

juce::Colour moduleDisplayColour (ModuleId type)
{
    if (type >= ModuleId::Custom0 && type < ModuleId::NumModules)
        return juce::Colour (iridescent::CustomModuleRegistry::get().getColour (type - ModuleId::Custom0));

    return juce::Colour (iridescent::moduleSpec (type).colour);
}

juce::String shortTypeName (ModuleId type)
{
    switch (type)
    {
        case ModuleId::Comp: return "COMP";
        case ModuleId::Eq: return "EQ";
        case ModuleId::Filter: return "FILTER";
        case ModuleId::Chorus: return "CHORUS";
        case ModuleId::Distortion: return "DIST";
        case ModuleId::Hyper: return "HYPER";
        case ModuleId::Echo: return "ECHO";
        case ModuleId::Grain: return "GRAIN";
        case ModuleId::Cloud: return "CLOUD";
        default:
            if (type >= ModuleId::Custom0 && type < ModuleId::NumModules)
                return iridescent::CustomModuleRegistry::get().getDisplayName (type - ModuleId::Custom0);
            return "MORPH";
    }
}
} // namespace

IridescentFlowerEditor::IridescentFlowerEditor (IridescentFlowerProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      filterGraph (p),
      eqGraph (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setWantsKeyboardFocus (true);

    textureImage = juce::ImageFileFormat::loadFrom (BinaryData::texture_pixel_png,
                                                    static_cast<size_t> (BinaryData::texture_pixel_pngSize));
    reloadAvatars();

    const auto savedAvatar = processorRef.parameters.state.getProperty ("avatarImage").toString();
    const auto savedIndex = avatarNames.indexOf (savedAvatar);

    if (savedIndex >= 0)
        avatarIndex = savedIndex;

    avatarStateName = avatarNames.isEmpty() ? juce::String() : avatarNames[avatarIndex];

    for (auto& knob : knobs)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
        knob.setMouseDragSensitivity (150);
        knob.setScrollWheelEnabled (true);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.onNumericClick = [this, &knob] { showNumericEditor (knob); };
        addAndMakeVisible (knob);
        knob.setVisible (false);
    }

    for (auto& toggle : toggles)
    {
        toggle.setClickingTogglesState (true);
        addAndMakeVisible (toggle);
        toggle.setVisible (false);
    }

    for (auto& combo : combos)
    {
        addAndMakeVisible (combo);
        combo.setVisible (false);
    }

    addAndMakeVisible (filterGraph);
    addAndMakeVisible (eqGraph);
    filterGraph.setVisible (false);
    eqGraph.setVisible (false);

    addAndMakeVisible (presetButton);
    presetButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const auto factoryNames = IridescentFlowerProcessor::getFactoryPresetNames();

        if (! factoryNames.isEmpty())
        {
            juce::PopupMenu factoryMenu;

            for (int i = 0; i < factoryNames.size(); ++i)
                factoryMenu.addItem (100 + i, factoryNames[i]);

            menu.addSubMenu ("FACTORY PRESETS", factoryMenu);
        }

        menu.addItem (1, "SAVE PRESET");
        menu.addItem (2, "LOAD PRESET");
        menu.addItem (3, "OPEN PRESET FOLDER");
        menu.addItem (4, "OPEN CUSTOM MODULES FOLDER");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetButton),
                            [this, factoryNames] (int result)
                            {
                                if (result == 1)
                                {
                                    savePreset();
                                }
                                else if (result == 2)
                                {
                                    loadPreset();
                                }
                                else if (result == 3)
                                {
                                    openPresetFolder();
                                }
                                else if (result == 4)
                                {
                                    iridescent::CustomModuleRegistry::get().revealFolder();
                                }
                                else if (result >= 100)
                                {
                                    const auto index = result - 100;

                                    if (processorRef.loadFactoryPreset (index))
                                    {
                                        presetMessage = factoryNames[index].toUpperCase();
                                        refreshControls();
                                        repaint();
                                    }
                                }
                            });
    };

    visualsButton.setClickingTogglesState (true);
    addAndMakeVisible (visualsButton);
    visualsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.parameters, iridescent::ids::visuals, visualsButton);

    addAndMakeVisible (bus1Button);
    addAndMakeVisible (bus2Button);
    addAndMakeVisible (bus3Button);
    addAndMakeVisible (masterButton);
    addAndMakeVisible (morphButton);

    for (auto* button : { &bus1Button, &bus2Button, &bus3Button, &masterButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x5151);
    }

    morphButton.setClickingTogglesState (true);
    morphButton.setRadioGroupId (0x5152);
    bus1Button.setToggleState (true, juce::dontSendNotification);

    bus1Button.onClick = [this] { busPage = 1; selectSlot (0); };
    bus2Button.onClick = [this] { busPage = 2; selectSlot (0); };
    bus3Button.onClick = [this] { busPage = 3; selectSlot (0); };
    masterButton.onClick = [this] { busPage = 4; selectSlot (0); };
    morphButton.onClick = [this] { selectSlot (RackEngine::slotsPerBus); };

    addAndMakeVisible (previousButton);
    addAndMakeVisible (nextButton);
    previousButton.onClick = [this]
    {
        if (selectedSlot > 0 && selectedSlot < RackEngine::slotsPerBus)
        {
            processorRef.moveSlot (busPage - 1, selectedSlot, selectedSlot - 1);
            selectedSlot--;
            processorRef.setActiveSlot (busPage - 1, selectedSlot, false);
            refreshControls();
        }
    };
    nextButton.onClick = [this]
    {
        if (selectedSlot >= 0 && selectedSlot < RackEngine::slotsPerBus - 1)
        {
            processorRef.moveSlot (busPage - 1, selectedSlot, selectedSlot + 1);
            selectedSlot++;
            processorRef.setActiveSlot (busPage - 1, selectedSlot, false);
            refreshControls();
        }
    };

    globalMix.setSliderStyle (juce::Slider::LinearHorizontal);
    globalMix.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    globalMix.setName ("MASTER MIX");
    addAndMakeVisible (globalMix);
    globalAttachments[0] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.parameters, iridescent::ids::globalMix, globalMix);

    globalOutput.setSliderStyle (juce::Slider::LinearHorizontal);
    globalOutput.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    globalOutput.setName ("OUTPUT");
    addAndMakeVisible (globalOutput);
    globalAttachments[1] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.parameters, iridescent::ids::globalOutput, globalOutput);

    bus1Out.setSliderStyle (juce::Slider::LinearHorizontal);
    bus1Out.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bus1Out.setName ("BUS 1 OUT");
    addAndMakeVisible (bus1Out);
    globalAttachments[2] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.parameters, iridescent::ids::bus1Mix, bus1Out);

    bus2Out.setSliderStyle (juce::Slider::LinearHorizontal);
    bus2Out.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bus2Out.setName ("BUS 2 OUT");
    addAndMakeVisible (bus2Out);
    globalAttachments[3] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.parameters, iridescent::ids::bus2Mix, bus2Out);

    bus3Out.setSliderStyle (juce::Slider::LinearHorizontal);
    bus3Out.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bus3Out.setName ("BUS 3 OUT");
    addAndMakeVisible (bus3Out);
    globalAttachments[4] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.parameters, iridescent::ids::bus3Mix, bus3Out);

    selectSlot (0);
    setSize (1100, 680);
    startTimerHz (30);
}

IridescentFlowerEditor::~IridescentFlowerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

IridescentFlowerEditor::Description IridescentFlowerEditor::describe (ModuleId type)
{
    Description d;

    switch (type)
    {
        case ModuleId::Morph:
            d.knobs = { { iridescent::ids::morphAmount, "MORPH" },
                        { iridescent::ids::morphBTrim, "B TRIM" },
                        { iridescent::ids::morphCurve, "CURVE" },
                        { iridescent::ids::morphBWidth, "B WIDTH" },
                        { iridescent::ids::lfoRate, "LFO RATE" },
                        { iridescent::ids::lfoDepth, "LFO DEPTH" },
                        { iridescent::ids::lfoPhase, "LFO PHASE" } };
            d.choices.push_back ({ iridescent::ids::morphSource, "B SOURCE",
                                   { "SIDECHAIN", "BUS 2", "INTERNAL" } });
            d.choices.push_back ({ iridescent::ids::lfoShape, "LFO SHAPE",
                                   { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "RANDOM" } });
            break;

        case ModuleId::Comp:
            d.knobs = { { iridescent::ids::compThreshold, "THRESHOLD" },
                        { iridescent::ids::compRatio, "RATIO" },
                        { iridescent::ids::compAttack, "ATTACK" },
                        { iridescent::ids::compRelease, "RELEASE" },
                        { iridescent::ids::compMakeup, "MAKEUP" },
                        { iridescent::ids::compMix, "MIX" } };
            break;

        case ModuleId::Eq:
            d.knobs = { { iridescent::ids::eqLow, "LOW" },
                        { iridescent::ids::eqMid, "MID" },
                        { iridescent::ids::eqMidFreq, "MID FREQ" },
                        { iridescent::ids::eqHigh, "HIGH" },
                        { iridescent::ids::eqOutput, "OUTPUT" } };
            d.graph = GraphKind::eq;
            break;

        case ModuleId::Filter:
            d.knobs = { { iridescent::ids::filterCutoff, "CUTOFF" },
                        { iridescent::ids::filterReso, "RESONANCE" },
                        { iridescent::ids::filterDrive, "DRIVE" },
                        { iridescent::ids::filterMix, "MIX" } };
            d.graph = GraphKind::filter;
            d.choices.push_back ({ iridescent::ids::filterType, "TYPE",
                                   { "LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH" } });
            break;

        case ModuleId::Chorus:
            d.knobs = { { iridescent::ids::chorusRate, "RATE" },
                        { iridescent::ids::chorusDepth, "DEPTH" },
                        { iridescent::ids::chorusFeedback, "FEEDBACK" },
                        { iridescent::ids::chorusMix, "MIX" } };
            d.choices.push_back ({ iridescent::ids::chorusMode, "MODE", { "CHORUS", "FLANGER" } });
            break;

        case ModuleId::Distortion:
            d.knobs = { { iridescent::ids::distDrive, "DRIVE" },
                        { iridescent::ids::distTone, "TONE" },
                        { iridescent::ids::distBits, "BITS" },
                        { iridescent::ids::distDown, "DOWNSAMPLE" },
                        { iridescent::ids::distMix, "MIX" } };
            d.choices.push_back ({ iridescent::ids::distMode, "MODE", { "SOFT", "HARD", "FOLD" } });
            break;

        case ModuleId::Hyper:
            d.knobs = { { iridescent::ids::hyperDetune, "DETUNE" },
                        { iridescent::ids::hyperWidth, "WIDTH" },
                        { iridescent::ids::hyperRate, "RATE" },
                        { iridescent::ids::hyperMix, "MIX" } };
            break;

        case ModuleId::Echo:
            d.knobs = { { iridescent::ids::echoTime, "TIME" },
                        { iridescent::ids::echoFeedback, "FEEDBACK" },
                        { iridescent::ids::echoDiffusion, "DIFFUSION" },
                        { iridescent::ids::echoTone, "TONE" },
                        { iridescent::ids::echoMix, "MIX" } };
            d.toggles = { { iridescent::ids::echoPingpong, "PING PONG" },
                          { iridescent::ids::echoSync, "SYNC" } };
            break;

        case ModuleId::Grain:
            d.knobs = { { iridescent::ids::grainSize, "SIZE" },
                        { iridescent::ids::grainDensity, "DENSITY" },
                        { iridescent::ids::grainPitch, "PITCH" },
                        { iridescent::ids::grainSpray, "SPRAY" },
                        { iridescent::ids::grainFeedback, "FEEDBACK" },
                        { iridescent::ids::grainMix, "MIX" },
                        { iridescent::ids::grainBuffer, "BUFFER" },
                        { iridescent::ids::grainOffset, "OFFSET" },
                        { iridescent::ids::grainRate, "GRAIN RATE" },
                        { iridescent::ids::grainWidth, "WIDTH" } };
            d.toggles = { { iridescent::ids::grainFreeze, "FREEZE" },
                          { iridescent::ids::grainSync, "SYNC" } };
            d.choices.push_back ({ iridescent::ids::grainSnap, "SNAP", { "OFF", "1/16", "1/8", "1/4" } });
            break;

        case ModuleId::Cloud:
            d.knobs = { { iridescent::ids::cloudSize, "SIZE" },
                        { iridescent::ids::cloudDecay, "DECAY" },
                        { iridescent::ids::cloudDamping, "DAMPING" },
                        { iridescent::ids::cloudMod, "MOD" },
                        { iridescent::ids::cloudPredelay, "PRE-DELAY" },
                        { iridescent::ids::cloudLowcut, "LOW CUT" },
                        { iridescent::ids::cloudWidth, "WIDTH" },
                        { iridescent::ids::cloudMix, "MIX" } };
            break;

        case ModuleId::Lfo:
            d.knobs = { { iridescent::ids::lfoRate, "RATE" },
                        { iridescent::ids::lfoDepth, "MORPH DEPTH" },
                        { iridescent::ids::lfoPhase, "PHASE" },
                        { iridescent::ids::lfoPan, "AUTO PAN" },
                        { iridescent::ids::lfoStereo, "STEREO" } };
            d.choices.push_back ({ iridescent::ids::lfoShape, "SHAPE",
                                   { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "RANDOM" } });
            break;

        default:
            if (type >= ModuleId::Custom0 && type < ModuleId::NumModules)
            {
                static std::array<std::array<juce::String, 8>, iridescent::CustomModuleRegistry::maxModules> customIds;
                static std::array<std::array<juce::String, 8>, iridescent::CustomModuleRegistry::maxModules> customLabels;
                const auto index = type - ModuleId::Custom0;
                const auto* loaded = iridescent::CustomModuleRegistry::get().getModule (index);

                for (int p = 0; p < iridescent::CustomModuleRegistry::paramsPerModule; ++p)
                {
                    customIds[static_cast<size_t> (index)][static_cast<size_t> (p)]
                        = iridescent::ids::customParamId (index, p);
                    customLabels[static_cast<size_t> (index)][static_cast<size_t> (p)]
                        = loaded != nullptr && loaded->info->params[p].name != nullptr
                              ? juce::String (loaded->info->params[p].name).toUpperCase()
                              : "P" + juce::String (p + 1);

                    d.knobs.push_back ({ customIds[static_cast<size_t> (index)][static_cast<size_t> (p)].toRawUTF8(),
                                         customLabels[static_cast<size_t> (index)][static_cast<size_t> (p)].toRawUTF8() });
                }
            }
            break;
    }

    return d;
}

void IridescentFlowerEditor::selectSlot (int slot)
{
    selectedSlot = juce::jlimit (0, RackEngine::numSlots, slot);

    if (selectedSlot < RackEngine::slotsPerBus)
        processorRef.setActiveSlot (busPage - 1, selectedSlot);

    refreshControls();
    repaint();
}

void IridescentFlowerEditor::setPageForSnapshot (int page, bool selectCustom)
{
    busPage = juce::jlimit (1, 4, page);
    selectedSlot = 0;
    processorRef.setActiveSlot (busPage - 1, 0);

    if (selectCustom)
        processorRef.setSlotType (busPage - 1, 0, ModuleId::Custom0);

    refreshControls();
    repaint();
}

void IridescentFlowerEditor::refreshControls()
{
    for (auto& a : knobAttachments) a.reset();
    for (auto& a : toggleAttachments) a.reset();
    for (auto& a : comboAttachments) a.reset();

    for (auto& k : knobs) k.setVisible (false);
    for (auto& t : toggles) t.setVisible (false);
    for (auto& c : combos) c.setVisible (false);
    filterGraph.setVisible (false);
    eqGraph.setVisible (false);
    filterGraph.setActive (false);

    const auto slot = selectedSlot >= RackEngine::numSlots
                          ? RackEngine::Slot { ModuleId::Morph, true }
                          : processorRef.getSlot (busPage - 1, selectedSlot);

    if (selectedSlot < RackEngine::numSlots && slot.type == ModuleId::Morph)
    {
        previousButton.setEnabled (selectedSlot > 0);
        nextButton.setEnabled (selectedSlot < RackEngine::numSlots - 1);
        resized();
        repaint();
        return;
    }

    const auto desc = describe (slot.type == ModuleId::Morph ? ModuleId::Morph : slot.type);

    for (size_t i = 0; i < desc.knobs.size() && i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.setName (desc.knobs[i].label);
        knob.setVisible (true);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.parameters, desc.knobs[i].id, knob);
    }

    for (auto& t : toggles)
        t.onClick = nullptr;

    for (size_t i = 0; i < desc.toggles.size() && i < toggles.size(); ++i)
    {
        toggles[i].setButtonText (desc.toggles[i].label);
        toggles[i].setVisible (true);
        toggleAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processorRef.parameters, desc.toggles[i].id, toggles[i]);
    }

    if (slot.type == ModuleId::Echo)
    {
        const auto sync = processorRef.parameters.getRawParameterValue (iridescent::ids::echoSync)->load() > 0.5f;
        auto& knob = knobs[0];
        knobAttachments[0].reset();

        if (sync)
        {
            knob.setName ("DIVISION");
            knob.setNumDecimalPlacesToDisplay (0);
            knob.textFromValueFunction = [] (double value)
            {
                return divisions()[juce::jlimit (0, 8, juce::roundToInt (value))];
            };
            knob.valueFromTextFunction = [] (const juce::String& text)
            {
                const auto index = divisions().indexOf (text);
                return index >= 0 ? static_cast<double> (index) : 0.0;
            };
            knobAttachments[0] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.parameters, iridescent::ids::echoDivision, knob);
            knob.setNumDecimalPlacesToDisplay (0);
            knob.textFromValueFunction = [] (double value)
            {
                return divisions()[juce::jlimit (0, 8, juce::roundToInt (value))];
            };
            knob.valueFromTextFunction = [] (const juce::String& text)
            {
                const auto index = divisions().indexOf (text);
                return index >= 0 ? static_cast<double> (index) : 0.0;
            };
            knob.updateText();
        }
        else
        {
            knob.setName ("TIME");
            knob.textFromValueFunction = nullptr;
            knob.valueFromTextFunction = nullptr;
            knob.onValueChange = nullptr;
            knobAttachments[0] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.parameters, iridescent::ids::echoTime, knob);
        }

        toggles[1].onClick = [this]
        {
            auto safe = juce::Component::SafePointer<IridescentFlowerEditor> (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refreshControls(); });
        };
    }
    else if (slot.type == ModuleId::Grain)
    {
        const auto sync = processorRef.parameters.getRawParameterValue (iridescent::ids::grainSync)->load() > 0.5f;
        auto& knob = knobs[1];
        knobAttachments[1].reset();

        if (sync)
        {
            knob.setName ("DIVISION");
            knob.setNumDecimalPlacesToDisplay (0);
            knob.textFromValueFunction = [] (double value)
            {
                return divisions()[juce::jlimit (0, 8, juce::roundToInt (value))];
            };
            knob.valueFromTextFunction = [] (const juce::String& text)
            {
                const auto index = divisions().indexOf (text);
                return index >= 0 ? static_cast<double> (index) : 0.0;
            };
            knobAttachments[1] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.parameters, iridescent::ids::grainDivision, knob);
            knob.setNumDecimalPlacesToDisplay (0);
            knob.textFromValueFunction = [] (double value)
            {
                return divisions()[juce::jlimit (0, 8, juce::roundToInt (value))];
            };
            knob.valueFromTextFunction = [] (const juce::String& text)
            {
                const auto index = divisions().indexOf (text);
                return index >= 0 ? static_cast<double> (index) : 0.0;
            };
            knob.updateText();
        }
        else
        {
            knob.setName ("DENSITY");
            knob.textFromValueFunction = nullptr;
            knob.valueFromTextFunction = nullptr;
            knob.onValueChange = nullptr;
            knobAttachments[1] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.parameters, iridescent::ids::grainDensity, knob);
        }

        toggles[1].onClick = [this]
        {
            auto safe = juce::Component::SafePointer<IridescentFlowerEditor> (this);
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refreshControls(); });
        };
    }

    for (size_t i = 0; i < desc.choices.size() && i < combos.size(); ++i)
    {
        auto& combo = combos[i];
        combo.clear (juce::dontSendNotification);
        combo.addItemList (desc.choices[i].options, 1);
        combo.setVisible (true);
        comboAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            processorRef.parameters, desc.choices[i].id, combo);
    }

    if (desc.graph == GraphKind::filter)
    {
        filterGraph.setVisible (true);
        filterGraph.setActive (true);
    }
    else if (desc.graph == GraphKind::eq)
    {
        eqGraph.setVisible (true);
    }

    previousButton.setEnabled (selectedSlot > 0 && selectedSlot < RackEngine::numSlots);
    nextButton.setEnabled (selectedSlot >= 0 && selectedSlot < RackEngine::numSlots - 1);
    resized();
    repaint();
}

juce::Rectangle<int> IridescentFlowerEditor::cardBounds (int localIndex) const
{
    return { rackX + localIndex * (cardWidth + cardGap), rackY, cardWidth, rackHeight };
}

int IridescentFlowerEditor::slotAt (juce::Point<int> position) const
{
    for (int i = 0; i < RackEngine::slotsPerBus; ++i)
        if (cardBounds (i).contains (position))
            return i;

    return -1;
}

juce::Rectangle<int> IridescentFlowerEditor::busBadgeBounds (int localIndex) const
{
    return cardBounds (localIndex).withTrimmedTop (72).withTrimmedLeft (8).withWidth (38).withHeight (14);
}

juce::Rectangle<int> IridescentFlowerEditor::powerBounds (int localIndex) const
{
    return cardBounds (localIndex).withTrimmedTop (8).withTrimmedLeft (cardWidth - 22).withWidth (16).withHeight (16);
}

void IridescentFlowerEditor::resized()
{
    presetButton.setBounds (760, 56, 100, 26);
    visualsButton.setBounds (940, 56, 142, 26);
    previousButton.setBounds (570, 224, 26, 26);
    nextButton.setBounds (599, 224, 26, 26);

    bus1Button.setBounds (20, 98, 64, 20);
    bus2Button.setBounds (88, 98, 64, 20);
    bus3Button.setBounds (156, 98, 64, 20);
    masterButton.setBounds (224, 98, 74, 20);
    morphButton.setBounds (304, 98, 70, 20);

    const auto detailX = 20;
    const auto slot = selectedSlot >= RackEngine::numSlots
                          ? RackEngine::Slot { ModuleId::Morph, true }
                          : processorRef.getSlot (busPage - 1, selectedSlot);
    const auto desc = describe (selectedSlot >= RackEngine::numSlots ? ModuleId::Morph : slot.type);
    const auto knobCount = static_cast<int> (desc.knobs.size());

    const auto stackY = detailY + 60;
    int columns = 4;
    int cellWidth = 160;
    int rowHeight = 118;

    if (knobCount <= 4)
    {
        columns = 2;
        cellWidth = 210;
        rowHeight = 120;
    }
    else if (knobCount <= 6)
    {
        columns = 3;
        cellWidth = 180;
        rowHeight = 120;
    }
    else if (knobCount > 8)
    {
        columns = 5;
        cellWidth = 140;
        rowHeight = 105;
    }

    const auto totalWidth = columns * cellWidth;
    const auto startX = detailX + (724 - totalWidth) / 2;

    for (int i = 0; i < knobCount && i < static_cast<int> (knobs.size()); ++i)
        knobs[i].setBounds (startX + (i % columns) * cellWidth,
                            stackY + (i / columns) * rowHeight,
                            cellWidth, rowHeight);

    const auto visualGraphBounds = juce::Rectangle<int> (764, 244, 308, 300);
    filterGraph.setBounds (visualGraphBounds);
    eqGraph.setBounds (visualGraphBounds);

    const auto bottomY = detailY + (knobCount > 6 ? 304 : 296);
    int x = detailX + 22;

    for (size_t i = 0; i < desc.choices.size() && i < combos.size(); ++i)
    {
        combos[i].setBounds (x, bottomY, 190, 26);
        x += 202;
    }

    for (size_t i = 0; i < desc.toggles.size() && i < toggles.size(); ++i)
    {
        toggles[i].setBounds (x, bottomY, 118, 26);
        x += 124;
    }

    globalMix.setBounds (24, 628, 170, 44);
    globalOutput.setBounds (214, 628, 170, 44);
    bus1Out.setBounds (404, 628, 170, 44);
    bus2Out.setBounds (594, 628, 170, 44);
    bus3Out.setBounds (784, 628, 170, 44);
}

void IridescentFlowerEditor::paint (juce::Graphics& g)
{
    bus1Button.setToggleState (busPage == 1 && selectedSlot < RackEngine::numSlots, juce::dontSendNotification);
    bus2Button.setToggleState (busPage == 2 && selectedSlot < RackEngine::numSlots, juce::dontSendNotification);
    bus3Button.setToggleState (busPage == 3 && selectedSlot < RackEngine::numSlots, juce::dontSendNotification);
    masterButton.setToggleState (busPage == 4 && selectedSlot < RackEngine::numSlots, juce::dontSendNotification);
    morphButton.setToggleState (selectedSlot >= RackEngine::numSlots, juce::dontSendNotification);

    g.fillAll (juce::Colour (0xff050309));
    paintHeader (g);
    paintRack (g);
    paintDetail (g);
    paintVisual (g);
    paintFooter (g);
}

void IridescentFlowerEditor::paintHeader (juce::Graphics& g)
{
    const auto width = getWidth();
    g.setColour (juce::Colour (0xff090410));
    g.fillRect (0, 0, width, headerHeight);

    if (textureImage.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.setOpacity (0.18f);
        g.drawImage (textureImage, 0, 0, width, headerHeight, 0, 0,
                     textureImage.getWidth(), textureImage.getHeight());
        g.setOpacity (1.0f);
    }

    juce::Random spray (0x9d12u);
    for (int i = 0; i < 80; ++i)
    {
        g.setColour ((i % 3 == 0 ? IridescentLookAndFeel::magenta
                                 : IridescentLookAndFeel::violet).withAlpha (0.04f + spray.nextFloat() * 0.13f));
        g.fillRect (12 + spray.nextInt (480), 4 + spray.nextInt (88), 1 + spray.nextInt (3), 1 + spray.nextInt (3));
    }

    const auto brand = juce::String ("IRIDESCENT FLOWER");
    g.setFont (IridescentLookAndFeel::y2kFont (38.0f));
    g.setColour (IridescentLookAndFeel::magenta.withAlpha (0.72f));
    g.drawText (brand, juce::Rectangle<int> (24, 10, 600, 44), juce::Justification::centredLeft, false);
    g.setColour (IridescentLookAndFeel::cyan.withAlpha (0.65f));
    g.drawText (brand, juce::Rectangle<int> (27, 10, 600, 44), juce::Justification::centredLeft, false);
    g.setColour (IridescentLookAndFeel::cream);
    g.drawText (brand, juce::Rectangle<int> (25, 10, 600, 44), juce::Justification::centredLeft, false);

    g.setFont (IridescentLookAndFeel::y2kFont (9.0f));
    g.setColour (IridescentLookAndFeel::dim);
    g.drawText ("FX RACK / 3 BUS / BUS MORPH / Y2K ROT",
                juce::Rectangle<int> (27, 51, 600, 13), juce::Justification::centredLeft, false);
    g.setColour (IridescentLookAndFeel::acid.withAlpha (0.8f));
    g.drawText ("RIGHT CLICK A SLOT TO LOAD / DRAG TO REORDER",
                juce::Rectangle<int> (27, 65, 600, 13), juce::Justification::centredLeft, false);

    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-0.05f, 760.0f, 28.0f));
    g.setColour (juce::Colour (0xffcdbb9e).withAlpha (0.8f));
    g.fillRect (520, 16, 180, 22);
    PixelFont::draw (g, "BUS 1/2/3", 528, 23, 1, juce::Colour (0xff190a24));
    g.restoreState();

    const auto playing = processorRef.getEngine().isPlaying();
    PixelFont::drawRight (g, playing ? "PLAYING" : "STOPPED",
                          { 760, 15, 160, 8 }, 1,
                          playing ? IridescentLookAndFeel::acid : IridescentLookAndFeel::dim);
    PixelFont::drawRight (g, "BPM " + juce::String (processorRef.getEngine().getTempo(), 1),
                          { 760, 30, 160, 8 }, 1, IridescentLookAndFeel::dim);
    PixelFont::drawRight (g, "BUS 1 " + juce::String (processorRef.getEngine().getMorphValue() * 100.0f, 0) + "%",
                          { 760, 45, 160, 8 }, 1, IridescentLookAndFeel::magenta);

    if (presetMessage.isNotEmpty())
        PixelFont::drawRight (g, presetMessage, { 520, 78, 540, 8 }, 1, IridescentLookAndFeel::acid);

    g.setColour (IridescentLookAndFeel::violet);
    g.fillRect (0, headerHeight - 2, width, 2);
}

void IridescentFlowerEditor::paintRack (juce::Graphics& g)
{
    for (int i = 0; i < RackEngine::slotsPerBus; ++i)
    {
        auto card = cardBounds (i);

        if (dragging && dragTarget >= 0 && i >= dragTarget && i != dragSource)
            card.translate (12, 0);

        const auto bounds = card.toFloat();
        const auto slot = processorRef.getSlot (busPage - 1, i);
        const auto empty = slot.type == ModuleId::Morph;
        const auto spec = iridescent::moduleSpec (slot.type);
        const auto accent = moduleDisplayColour (slot.type);
        const auto selected = selectedSlot == i;

        g.setColour (empty ? juce::Colour (0xff08040e) : juce::Colour (0xff0b0613));
        g.fillRect (bounds);
        g.setColour (selected ? accent : accent.withAlpha (empty ? 0.25f : 0.55f));
        g.drawRect (bounds, selected ? 2.0f : 1.0f);

        PixelFont::draw (g, juce::String (i + 1).paddedLeft ('0', 2),
                         static_cast<int> (bounds.getX()) + 7,
                         static_cast<int> (bounds.getY()) + 7, 1, accent);

        if (empty)
        {
            PixelFont::draw (g, "+ FX", static_cast<int> (bounds.getX()) + 8,
                             static_cast<int> (bounds.getY()) + 32, 2, accent.withAlpha (0.8f));
            PixelFont::draw (g, "RIGHT CLICK", static_cast<int> (bounds.getX()) + 8,
                             static_cast<int> (bounds.getY()) + 58, 1, IridescentLookAndFeel::dim);
        }
        else
        {
            g.setFont (IridescentLookAndFeel::moduleFont (slot.type, 17.0f));
            g.setColour (selected ? IridescentLookAndFeel::cream
                                  : IridescentLookAndFeel::cream.withAlpha (0.85f));
            g.drawText (shortTypeName (slot.type),
                        bounds.toNearestInt().withTrimmedLeft (7).withTrimmedTop (21).withHeight (24),
                        juce::Justification::centredLeft, false);
            PixelFont::draw (g, moduleDisplayTagline (slot.type),
                             static_cast<int> (bounds.getX()) + 7,
                             static_cast<int> (bounds.getY()) + 47, 1,
                             IridescentLookAndFeel::dim);
        }

        if (! empty)
        {
            const auto badge = busBadgeBounds (i);
            g.setColour (accent.withAlpha (0.16f));
            g.fillRect (badge);
            g.setColour (accent);
            g.drawRect (badge, 1);
            PixelFont::draw (g, busPage == 4 ? "M" : "B" + juce::String (busPage),
                             badge.getX() + 4, badge.getY() + 3, 1, accent);
        }

        const auto power = powerBounds (i).toFloat();
        g.setColour (slot.on ? accent : juce::Colour (0xff352145));
        g.fillRect (power);
        g.setColour (IridescentLookAndFeel::cream);
        g.drawRect (power, 1);

        g.setColour (IridescentLookAndFeel::dim.withAlpha (0.35f));
        for (int j = 0; j < 3; ++j)
            g.fillRect (bounds.getX() + 7.0f + j * 4.0f, bounds.getBottom() - 10.0f, 2.0f, 2.0f);
    }

    if (dragging && dragSource >= 0 && dragTarget >= 0)
    {
        const auto insert = juce::jlimit (0, RackEngine::slotsPerBus, dragTarget);
        const auto gapX = rackX + insert * (cardWidth + cardGap) - (insert == 0 ? 5 : 2);

        g.setColour (juce::Colour (0xff120a1c).withAlpha (0.92f));
        g.fillRect (gapX - 5, rackY - 2, 12, rackHeight + 4);

        g.setColour (IridescentLookAndFeel::acid);
        g.fillRect (gapX, rackY - 3, 3, rackHeight + 6);

        juce::Path arrow;
        arrow.addTriangle (static_cast<float> (gapX - 3), static_cast<float> (rackY - 3),
                           static_cast<float> (gapX + 6), static_cast<float> (rackY - 3),
                           static_cast<float> (gapX + 1.5f), static_cast<float> (rackY + 5));
        g.fillPath (arrow);

        PixelFont::draw (g, "DROP", gapX - 14, rackY + rackHeight + 1, 1,
                         IridescentLookAndFeel::acid);
    }
}

void IridescentFlowerEditor::paintDetail (juce::Graphics& g)
{
    const auto bounds = juce::Rectangle<int> (20, detailY, 724, detailHeight);
    g.setColour (juce::Colour (0xff08040e));
    g.fillRect (bounds);

    if (textureImage.isValid())
    {
        g.saveState();
        g.reduceClipRegion (bounds);
        g.setOpacity (0.06f);
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (textureImage, bounds.toFloat(), juce::RectanglePlacement::fillDestination);
        g.setOpacity (1.0f);
        g.restoreState();
    }

    juce::Random scribbleRng (selectedSlot * 313 + 9);
    for (int i = 0; i < 24; ++i)
    {
        g.setColour (IridescentLookAndFeel::violet.withAlpha (0.05f));
        g.fillRect (bounds.getX() + 8 + scribbleRng.nextInt (bounds.getWidth() - 16),
                    bounds.getY() + 8 + scribbleRng.nextInt (bounds.getHeight() - 16),
                    1 + scribbleRng.nextInt (3), 1);
    }

    g.setColour (IridescentLookAndFeel::violet);
    g.drawRect (bounds, 2);

    const auto isMorph = selectedSlot >= RackEngine::numSlots;
    const auto slot = isMorph ? RackEngine::Slot { ModuleId::Morph, true }
                               : processorRef.getSlot (busPage - 1, selectedSlot);
    const auto empty = ! isMorph && slot.type == ModuleId::Morph;

    if (empty)
    {
        PixelFont::draw (g, "EMPTY SLOT", 42, 250, 4, IridescentLookAndFeel::dim);
        PixelFont::draw (g, "RIGHT CLICK THE CARD TO LOAD AN EFFECT",
                         42, 294, 1, IridescentLookAndFeel::acid);
        return;
    }

    const auto spec = iridescent::moduleSpec (isMorph ? ModuleId::Morph : slot.type);
    const auto accent = moduleDisplayColour (slot.type);

    const auto chainLabel = busPage == 4 ? juce::String ("MASTER")
                                          : "BUS " + juce::String (busPage);
    const auto headerText = isMorph
                                ? juce::String ("MORPH / BUS OUT")
                                : "MODULE " + juce::String (selectedSlot + 1).paddedLeft ('0', 2)
                                      + "/08  " + chainLabel;
    PixelFont::draw (g, headerText, 42, 228, 1, accent);
    g.setFont (IridescentLookAndFeel::moduleFont (isMorph ? ModuleId::Morph : slot.type, 30.0f));
    g.setColour (IridescentLookAndFeel::cream);
    g.drawText (isMorph ? juce::String ("MORPH") : moduleDisplayName (slot.type), juce::Rectangle<int> (38, 238, 420, 30),
                juce::Justification::centredLeft, false);
    PixelFont::draw (g, isMorph ? juce::String ("BUS 1 / BUS 2 / SIDECHAIN")
                                : moduleDisplayTagline (slot.type),
                     42, 270, 1, IridescentLookAndFeel::dim);

    if (! isMorph)
        PixelFont::draw (g, "/// " + moduleDisplayName (slot.type) + " / NO RULES",
                         42, 566, 1, accent.withAlpha (0.38f));

    const auto desc = describe (isMorph ? ModuleId::Morph : slot.type);

    for (size_t i = 0; i < desc.choices.size() && i < combos.size(); ++i)
    {
        if (combos[i].isVisible())
            PixelFont::draw (g, desc.choices[i].label, combos[i].getX(),
                             combos[i].getY() - 10, 1, IridescentLookAndFeel::dim);
    }



    if (isMorph)
    {
        const auto morph = processorRef.getEngine().getMorphValue();
        auto bar = juce::Rectangle<int> (42, 560, 620, 12);
        g.setColour (juce::Colour (0xff21132d));
        g.fillRect (bar);
        g.setColour (IridescentLookAndFeel::cyan);
        g.fillRect (bar.withWidth (static_cast<int> (bar.getWidth() * juce::jlimit (0.0f, 1.0f, morph))));
        g.setColour (IridescentLookAndFeel::magenta);
        g.fillRect (bar.getX() + static_cast<int> (bar.getWidth() * morph) - 2, bar.getY() - 2, 4, 16);
        PixelFont::draw (g, "A // B", 42, 544, 1, IridescentLookAndFeel::dim);
    }
}

void IridescentFlowerEditor::paintVisual (juce::Graphics& g)
{
    const auto bounds = juce::Rectangle<int> (756, detailY, 324, detailHeight);
    g.setColour (juce::Colour (0xff050309));
    g.fillRect (bounds);
    g.setColour (IridescentLookAndFeel::violet);
    g.drawRect (bounds, 2);

    const auto visualsOn = processorRef.parameters.getRawParameterValue (iridescent::ids::visuals)->load() > 0.5f;
    const auto playing = processorRef.getEngine().isPlaying();
    const auto animate = visualsOn && playing;
    const auto type = selectedSlot >= RackEngine::numSlots
                          ? ModuleId::Morph
                          : processorRef.getSlot (busPage - 1, selectedSlot).type;

    if (type == ModuleId::Eq || type == ModuleId::Filter)
    {
        // The read-only graph child fills the upper part of this panel; the
        // lower strip keeps the key values readable without a second graph.
        if (type == ModuleId::Filter)
        {
            const auto cutoff = processorRef.parameters.getRawParameterValue (iridescent::ids::filterCutoff)->load();
            const auto reso = processorRef.parameters.getRawParameterValue (iridescent::ids::filterReso)->load();
            PixelFont::draw (g, "CUTOFF " + juce::String (juce::roundToInt (cutoff)) + " HZ / RES "
                                  + juce::String (juce::roundToInt (reso * 100.0f)) + "%",
                             772, 560, 1, IridescentLookAndFeel::cyan);
        }
        else
        {
            const auto low = processorRef.parameters.getRawParameterValue (iridescent::ids::eqLow)->load();
            const auto mid = processorRef.parameters.getRawParameterValue (iridescent::ids::eqMid)->load();
            const auto high = processorRef.parameters.getRawParameterValue (iridescent::ids::eqHigh)->load();
            PixelFont::draw (g, "LOW " + juce::String (low, 1) + " / MID " + juce::String (mid, 1)
                                  + " / HIGH " + juce::String (high, 1),
                             772, 560, 1, IridescentLookAndFeel::acid);
        }

        PixelFont::draw (g, "READ ONLY / NO DRAG", 772, 580, 1, IridescentLookAndFeel::dim);
        return;
    }

    auto image = bounds.reduced (8).removeFromTop (236);
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    const auto shake = animate ? static_cast<float> ((animFrame % 9) - 4) * 1.6f : 0.0f;
    const auto imageDraw = image.toFloat().translated (shake, animate ? static_cast<float> (animFrame % 5) - 2.0f : 0.0f);

    const auto currentAvatar = avatarImages.empty()
                                   ? juce::Image()
                                   : avatarImages[static_cast<size_t> (juce::jlimit (
                                         0, static_cast<int> (avatarImages.size()) - 1, avatarIndex))];
    g.saveState();
    g.reduceClipRegion (image);

    if (currentAvatar.isValid())
    {
        g.drawImage (currentAvatar, imageDraw, juce::RectanglePlacement::fillDestination);

        if (animate && (animFrame % 11) < 3)
        {
            juce::Random rng (animFrame * 977 + 3);
            for (int i = 0; i < 5; ++i)
            {
                const auto y = image.getY() + rng.nextInt (image.getHeight());
                const auto h = 2 + rng.nextInt (12);
                const auto shift = rng.nextInt (16) - 8;
                g.setOpacity (0.65f);
                g.drawImage (currentAvatar, image.getX() + shift, y, image.getWidth(), h,
                             0, (y - image.getY()) * currentAvatar.getHeight() / image.getHeight(),
                             currentAvatar.getWidth(),
                             juce::jmax (1, h * currentAvatar.getHeight() / image.getHeight()));
                g.setOpacity (1.0f);
            }
        }
    }

    if (animate)
    {
        juce::Random rng (animFrame * 313 + 7);
        for (int i = 0; i < 7; ++i)
        {
            const auto y = image.getY() + rng.nextInt (image.getHeight());
            const auto h = 2 + rng.nextInt (8);
            g.setColour ((i % 2 == 0 ? IridescentLookAndFeel::cyan
                                     : IridescentLookAndFeel::magenta).withAlpha (0.35f));
            g.fillRect (image.getX(), y, image.getWidth(), h);
        }
    }

    for (int y = image.getY(); y < image.getBottom(); y += 4)
    {
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillRect (image.getX(), y, image.getWidth(), 1);
    }

    PixelFont::draw (g, "A // SIGNAL", image.getX() + 8, image.getY() + 8, 1,
                     IridescentLookAndFeel::cyan);
    PixelFont::drawRight (g, "BUS 1 / BUS 2",
                          { image.getX() + 8, image.getY() + 8, image.getWidth() - 16, 8 },
                          1, IridescentLookAndFeel::magenta);

    juce::Path star;
    const auto sx = static_cast<float> (image.getRight() - 25);
    const auto sy = static_cast<float> (image.getBottom() - 24);
    for (int i = 0; i < 10; ++i)
    {
        const auto a = -juce::MathConstants<float>::halfPi + i * juce::MathConstants<float>::pi / 5.0f;
        const auto r = (i % 2 == 0) ? 14.0f : 6.0f;
        const auto p = juce::Point<float> (sx + std::cos (a) * r, sy + std::sin (a) * r);
        if (i == 0) star.startNewSubPath (p);
        else star.lineTo (p);
    }
    star.closeSubPath();
    g.setColour (IridescentLookAndFeel::acid.withAlpha (0.8f));
    g.fillPath (star);
    PixelFont::draw (g, "IF", static_cast<int> (sx) - 5, static_cast<int> (sy) - 3, 1,
                     juce::Colour (0xff160a20));

    g.restoreState();

    const auto morph = processorRef.getEngine().getMorphValue();
    auto bar = juce::Rectangle<int> (bounds.getX() + 12, bounds.getY() + 258, bounds.getWidth() - 24, 10);
    g.setColour (juce::Colour (0xff21132d));
    g.fillRect (bar);
    g.setColour (IridescentLookAndFeel::cyan);
    g.fillRect (bar.withWidth (static_cast<int> (bar.getWidth() * juce::jlimit (0.0f, 1.0f, morph))));
    g.setColour (IridescentLookAndFeel::magenta);
    g.fillRect (bar.getX() + static_cast<int> (bar.getWidth() * morph) - 2, bar.getY() - 2, 4, 14);
    PixelFont::draw (g, "MORPH A//B", bounds.getX() + 12, bounds.getY() + 244, 1,
                     IridescentLookAndFeel::dim);

    const auto scopeArea = juce::Rectangle<int> (bounds.getX() + 12, bounds.getY() + 288,
                                                  bounds.getWidth() - 24, 48);
    g.setColour (juce::Colour (0xff07040c));
    g.fillRect (scopeArea);
    g.setColour (IridescentLookAndFeel::violet.withAlpha (0.35f));
    g.drawRect (scopeArea, 1);
    PixelFont::draw (g, "LFO / BUS MOTION", scopeArea.getX() + 4, scopeArea.getY() + 3, 1,
                     IridescentLookAndFeel::dim);

    juce::Path trace;
    const auto gain = processorRef.getEngine().getLfoValue();
    for (int i = 0; i < scopeArea.getWidth() - 8; ++i)
    {
        const auto phase = (i + animFrame * (animate ? 2 : 0)) * 0.06f;
        const auto y = scopeArea.getCentreY() + std::sin (phase) * (10.0f + gain * 10.0f);
        if (i == 0) trace.startNewSubPath (static_cast<float> (scopeArea.getX() + 4 + i), y);
        else trace.lineTo (static_cast<float> (scopeArea.getX() + 4 + i), y);
    }
    g.setColour (IridescentLookAndFeel::acid);
    g.strokePath (trace, juce::PathStrokeType (1.5f));

    PixelFont::draw (g, "GRAINS " + juce::String (processorRef.getEngine().getActiveGrainCount()).paddedLeft ('0', 2),
                     bounds.getX() + 12, bounds.getBottom() - 30, 1, IridescentLookAndFeel::acid);
    PixelFont::drawRight (g, animate ? "RUN" : "IDLE",
                          { bounds.getX() + 200, bounds.getBottom() - 30, 100, 8 }, 1,
                          animate ? IridescentLookAndFeel::acid : IridescentLookAndFeel::dim);

    if (animate)
    {
        const auto flash = (animFrame / 2) % 2 == 0;
        g.setColour ((flash ? IridescentLookAndFeel::acid : IridescentLookAndFeel::magenta).withAlpha (0.85f));
        g.drawRect (bounds, 3);
        PixelFont::draw (g, flash ? "NO SIGNAL" : "/////", bounds.getX() + 12,
                         bounds.getY() + 214, 2,
                         flash ? IridescentLookAndFeel::acid : IridescentLookAndFeel::magenta);
    }

    if (! visualsOn)
        PixelFont::draw (g, "VISUALS OFF", bounds.getX() + 12, bounds.getBottom() - 14, 1,
                         IridescentLookAndFeel::dim);

    PixelFont::drawRight (g, "CLICK IMAGE", { bounds.getX() + 190, bounds.getBottom() - 14, 110, 8 },
                          1, IridescentLookAndFeel::dim.withAlpha (0.7f));
}

void IridescentFlowerEditor::paintChorusVisual (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto area = bounds.reduced (12, 14).toFloat();
    const auto mode = processorRef.parameters.getRawParameterValue (iridescent::ids::chorusMode)->load();
    const auto depth = processorRef.parameters.getRawParameterValue (iridescent::ids::chorusDepth)->load();
    const auto rate = processorRef.parameters.getRawParameterValue (iridescent::ids::chorusRate)->load();
    const auto playing = processorRef.getEngine().isPlaying();
    const auto visualsOn = processorRef.parameters.getRawParameterValue (iridescent::ids::visuals)->load() > 0.5f;

    PixelFont::draw (g, mode > 0.5f ? "FLANGER / MOTION" : "CHORUS / MOTION",
                     static_cast<int> (area.getX()), static_cast<int> (area.getY()), 1,
                     IridescentLookAndFeel::acid);
    PixelFont::drawRight (g, "RATE " + juce::String (rate, 2) + " HZ",
                          { static_cast<int> (area.getX()), static_cast<int> (area.getY()),
                            static_cast<int> (area.getWidth()), 8 }, 1,
                          IridescentLookAndFeel::dim);

    for (int voice = 0; voice < 3; ++voice)
    {
        juce::Path wave;
        for (int i = 0; i < 100; ++i)
        {
            const auto x = area.getX() + area.getWidth() * static_cast<float> (i) / 99.0f;
            const auto phase = i * 0.10f + (playing && visualsOn ? animFrame * 0.035f : 0.0f) + voice * 0.5f;
            const auto y = area.getCentreY() + std::sin (phase) * area.getHeight() * (0.06f + depth * 0.08f)
                         + (voice - 1) * 18.0f;
            if (i == 0) wave.startNewSubPath (x, y);
            else wave.lineTo (x, y);
        }
        g.setColour ((voice == 1 ? IridescentLookAndFeel::magenta : IridescentLookAndFeel::cyan)
                         .withAlpha (0.75f));
        g.strokePath (wave, juce::PathStrokeType (1.5f));
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto x = area.getX() + area.getWidth() * (0.2f + i * 0.2f);
        g.setColour (IridescentLookAndFeel::violet.withAlpha (0.35f));
        g.drawVerticalLine (static_cast<int> (x), area.getY() + 30.0f, area.getBottom());
        PixelFont::draw (g, "TAP " + juce::String (i + 1),
                         static_cast<int> (x) + 2, static_cast<int> (area.getBottom()) - 10, 1,
                         IridescentLookAndFeel::dim);
    }

    PixelFont::draw (g, "DEPTH " + juce::String (juce::roundToInt (depth * 100.0f)) + "%",
                     static_cast<int> (area.getX()), static_cast<int> (area.getBottom()) - 24, 1,
                     IridescentLookAndFeel::dim);
    PixelFont::drawRight (g, mode > 0.5f ? "SHORT / METALLIC" : "WIDE / SOFT",
                          { static_cast<int> (area.getX()), static_cast<int> (area.getBottom()) - 24,
                            static_cast<int> (area.getWidth()), 8 }, 1,
                          IridescentLookAndFeel::violet);
}

void IridescentFlowerEditor::paintGrainVisual (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto area = bounds.reduced (12, 14).toFloat();
    const auto buffer = processorRef.parameters.getRawParameterValue (iridescent::ids::grainBuffer)->load();
    const auto offset = processorRef.parameters.getRawParameterValue (iridescent::ids::grainOffset)->load();
    const auto rate = processorRef.parameters.getRawParameterValue (iridescent::ids::grainRate)->load();
    const auto density = processorRef.parameters.getRawParameterValue (iridescent::ids::grainDensity)->load();
    const auto snap = processorRef.parameters.getRawParameterValue (iridescent::ids::grainSnap)->load();
    const auto playing = processorRef.getEngine().isPlaying();
    const auto visualsOn = processorRef.parameters.getRawParameterValue (iridescent::ids::visuals)->load() > 0.5f;
    const auto animate = playing && visualsOn;

    PixelFont::draw (g, "AUTOCHROMA BUFFER", static_cast<int> (area.getX()),
                     static_cast<int> (area.getY()), 1, IridescentLookAndFeel::acid);
    PixelFont::drawRight (g, juce::String (buffer, 2) + " S / OFF " + juce::String (juce::roundToInt (offset * 100.0f)) + "%",
                          { static_cast<int> (area.getX()), static_cast<int> (area.getY()),
                            static_cast<int> (area.getWidth()), 8 }, 1,
                          IridescentLookAndFeel::dim);

    const auto circle = juce::Rectangle<float> (area.getX() + 10.0f, area.getY() + 28.0f,
                                                 area.getWidth() - 20.0f, area.getHeight() - 84.0f);
    g.setColour (juce::Colour (0xff07040c));
    g.fillEllipse (circle);
    g.setColour (IridescentLookAndFeel::violet.withAlpha (0.45f));
    g.drawEllipse (circle, 1.0f);

    const auto centre = circle.getCentre();
    const auto radius = circle.getWidth() * 0.5f;

    // Buffer spokes: a quick radial view of the 1.5 s buffer.
    for (int i = 0; i < 64; ++i)
    {
        const auto angle = static_cast<float> (i) / 64.0f * juce::MathConstants<float>::twoPi;
        const auto energy = 0.45f + 0.55f * std::sin (i * 1.7f + (animate ? animFrame * 0.01f : 0.0f));
        const auto inner = radius * (0.78f + 0.08f * energy);
        g.setColour (IridescentLookAndFeel::violet.withAlpha (0.10f + 0.22f * energy));
        g.drawLine (centre.x + std::cos (angle) * inner,
                    centre.y + std::sin (angle) * inner,
                    centre.x + std::cos (angle) * radius * 0.94f,
                    centre.y + std::sin (angle) * radius * 0.94f, 1.0f);
    }

    // Offset arc and scanning playhead.
    const auto offsetAngle = -juce::MathConstants<float>::halfPi + offset * juce::MathConstants<float>::twoPi;
    g.setColour (IridescentLookAndFeel::acid.withAlpha (0.5f));
    g.drawLine (centre.x, centre.y,
                centre.x + std::cos (offsetAngle) * radius,
                centre.y + std::sin (offsetAngle) * radius, 1.0f);

    if (animate)
    {
        const auto scanAngle = -juce::MathConstants<float>::halfPi
                             + std::fmod (animFrame * 0.012f, 1.0f) * juce::MathConstants<float>::twoPi;
        g.setColour (IridescentLookAndFeel::magenta.withAlpha (0.75f));
        g.drawLine (centre.x, centre.y,
                    centre.x + std::cos (scanAngle) * radius,
                    centre.y + std::sin (scanAngle) * radius, 1.0f);
    }

    // Buffer waveform.
    juce::Path wave;
    for (int i = 0; i <= 90; ++i)
    {
        const auto t = static_cast<float> (i) / 90.0f;
        const auto x = circle.getX() + circle.getWidth() * t;
        const auto y = circle.getCentreY()
                     + std::sin (t * juce::MathConstants<float>::twoPi * 4.0f
                                 + (animate ? animFrame * 0.02f : 0.0f))
                           * circle.getHeight() * 0.28f;
        if (i == 0) wave.startNewSubPath (x, y);
        else wave.lineTo (x, y);
    }
    g.setColour (IridescentLookAndFeel::cyan);
    g.strokePath (wave, juce::PathStrokeType (2.0f));

    // Offset / read window.
    const auto windowWidth = circle.getWidth() * 0.22f;
    const auto windowX = circle.getX() + (circle.getWidth() - windowWidth) * offset;
    g.setColour (IridescentLookAndFeel::magenta.withAlpha (0.20f));
    g.fillRect (windowX, circle.getY(), windowWidth, circle.getHeight());
    g.setColour (IridescentLookAndFeel::magenta);
    g.drawRect (windowX, circle.getY(), windowWidth, circle.getHeight(), 1.0f);
    PixelFont::draw (g, "WINDOW", static_cast<int> (windowX) + 2,
                     static_cast<int> (circle.getY()) + 3, 1, IridescentLookAndFeel::magenta);

    // Grain particles.
    const auto count = animate ? juce::jlimit (3, 24, juce::roundToInt (density / 3.0f)) : 8;
    for (int i = 0; i < count; ++i)
    {
        const auto t = std::fmod (0.17f * i + (animate ? animFrame * 0.004f : 0.0f), 1.0f);
        const auto angle = t * juce::MathConstants<float>::twoPi;
        const auto radius = circle.getWidth() * (0.25f + 0.18f * std::sin (i * 2.1f));
        g.setColour ((i % 2 == 0 ? IridescentLookAndFeel::acid : IridescentLookAndFeel::cyan)
                         .withAlpha (0.8f));
        g.fillRect (circle.getCentreX() + std::cos (angle) * radius,
                    circle.getCentreY() + std::sin (angle) * radius, 3.0f, 3.0f);
    }

    PixelFont::draw (g, "RATE " + juce::String (rate, 2) + "X / DENS " + juce::String (density, 0),
                     static_cast<int> (area.getX()), static_cast<int> (area.getBottom()) - 28, 1,
                     IridescentLookAndFeel::dim);
    PixelFont::drawRight (g, "SNAP " + juce::String (snap < 0.5f ? "OFF" : (snap < 1.5f ? "1/16" : "1/8")),
                          { static_cast<int> (area.getX()), static_cast<int> (area.getBottom()) - 28,
                            static_cast<int> (area.getWidth()), 8 }, 1,
                          IridescentLookAndFeel::acid);
}

void IridescentFlowerEditor::paintLfoVisual (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto area = bounds.reduced (12, 14).toFloat();
    const auto rate = processorRef.parameters.getRawParameterValue (iridescent::ids::lfoRate)->load();
    const auto depth = processorRef.parameters.getRawParameterValue (iridescent::ids::lfoDepth)->load();
    const auto shape = processorRef.parameters.getRawParameterValue (iridescent::ids::lfoShape)->load();
    const auto animate = processorRef.getEngine().isPlaying()
                      && processorRef.parameters.getRawParameterValue (iridescent::ids::visuals)->load() > 0.5f;

    PixelFont::draw (g, "LFO / MORPH MOD", static_cast<int> (area.getX()),
                     static_cast<int> (area.getY()), 1, IridescentLookAndFeel::acid);
    PixelFont::drawRight (g, juce::String (rate, 2) + " HZ / " + juce::String (juce::roundToInt (depth * 100.0f)) + "%",
                          { static_cast<int> (area.getX()), static_cast<int> (area.getY()),
                            static_cast<int> (area.getWidth()), 8 }, 1,
                          IridescentLookAndFeel::dim);

    juce::Path wave;
    for (int i = 0; i <= 140; ++i)
    {
        const auto t = static_cast<float> (i) / 140.0f;
        const auto p = std::fmod (t + (animate ? animFrame * 0.003f : 0.0f), 1.0f);
        float v = 0.0f;

        if (shape < 0.5f) v = std::sin (p * juce::MathConstants<float>::twoPi);
        else if (shape < 1.5f) v = 1.0f - 4.0f * std::abs (p - 0.5f);
        else if (shape < 2.5f) v = p * 2.0f - 1.0f;
        else if (shape < 3.5f) v = 1.0f - p * 2.0f;
        else v = std::sin (p * 19.0f) * 0.5f;

        const auto x = area.getX() + area.getWidth() * t;
        const auto y = area.getCentreY() - v * area.getHeight() * 0.35f;
        if (i == 0) wave.startNewSubPath (x, y);
        else wave.lineTo (x, y);
    }

    g.setColour (IridescentLookAndFeel::cyan);
    g.strokePath (wave, juce::PathStrokeType (2.0f));
    g.setColour (IridescentLookAndFeel::magenta.withAlpha (0.5f));
    g.drawHorizontalLine (static_cast<int> (area.getCentreY()), area.getX(), area.getRight());

    PixelFont::draw (g, "SHAPE " + juce::String (shape < 0.5f ? "SINE"
                                                     : (shape < 1.5f ? "TRI"
                                                        : (shape < 2.5f ? "SAW UP" : "RAND"))),
                     static_cast<int> (area.getX()), static_cast<int> (area.getBottom()) - 18, 1,
                     IridescentLookAndFeel::dim);
}

void IridescentFlowerEditor::paintFooter (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff090410));
    g.fillRect (0, footerY, getWidth(), getHeight() - footerY);
    g.setColour (IridescentLookAndFeel::violet);
    g.fillRect (0, footerY, getWidth(), 2);

    PixelFont::draw (g, "CLICK VALUE TO TYPE / SPACE + F FOR VISUALS",
                     24, footerY + 4, 1, IridescentLookAndFeel::acid.withAlpha (0.65f));
}

void IridescentFlowerEditor::timerCallback()
{
    const auto playing = processorRef.getEngine().isPlaying();
    const auto visualsOn = processorRef.parameters.getRawParameterValue (iridescent::ids::visuals)->load() > 0.5f;

    if (playing && visualsOn)
        ++animFrame;

    ++avatarClock;

    if (avatarClock % 240 == 0 && ! avatarImages.empty())
        avatarIndex = (avatarIndex + 1) % static_cast<int> (avatarImages.size());

    const auto stateName = processorRef.parameters.state.getProperty ("avatarImage").toString();

    if (stateName != avatarStateName)
    {
        avatarStateName = stateName;
        reloadAvatars();
        const auto index = avatarNames.indexOf (stateName);

        if (index >= 0)
            avatarIndex = index;
    }

    repaint();
}

void IridescentFlowerEditor::showNumericEditor (juce::Slider& slider)
{
    auto* window = new juce::AlertWindow ("SET " + slider.getName().toUpperCase(),
                                          "Type a value and press OK.",
                                          juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("value", slider.getTextFromValue (slider.getValue()));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safe = juce::Component::SafePointer<IridescentFlowerEditor> (this);
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create (
                                 [safe, &slider, window] (int result)
                                 {
                                     if (result == 1 && safe != nullptr)
                                     {
                                         const auto text = window->getTextEditorContents ("value");
                                         slider.setValue (slider.getValueFromText (text),
                                                          juce::sendNotificationSync);
                                     }
                                 }),
                             true);
}

void IridescentFlowerEditor::showSlotMenu (int bus, int slot, juce::Point<int> screenPosition)
{
    juce::PopupMenu menu;
    juce::PopupMenu fx;

    const ModuleId types[] = { ModuleId::Comp, ModuleId::Eq, ModuleId::Filter,
                               ModuleId::Chorus, ModuleId::Distortion, ModuleId::Hyper,
                               ModuleId::Echo, ModuleId::Grain, ModuleId::Cloud };

    for (int i = 0; i < static_cast<int> (std::size (types)); ++i)
        fx.addItem (100 + i, iridescent::moduleSpec (types[i]).name);

    const auto& registry = iridescent::CustomModuleRegistry::get();

    if (registry.getNumLoaded() > 0)
    {
        fx.addSeparator();

        for (int c = 0; c < iridescent::CustomModuleRegistry::maxModules; ++c)
            if (registry.getModule (c) != nullptr)
                fx.addItem (300 + c, registry.getDisplayName (c));
    }

    const auto empty = processorRef.getSlot (bus, slot).type == ModuleId::Morph;
    menu.addSubMenu (empty ? "LOAD FX" : "REPLACE FX", fx);

    if (! empty)
    {
        menu.addSeparator();
        menu.addItem (999, "REMOVE FX");
    }

    auto safe = juce::Component::SafePointer<IridescentFlowerEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [safe, bus, slot, types] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result >= 100 && result < 100 + static_cast<int> (std::size (types)))
                                safe->setSlotType (bus, slot, types[result - 100]);
                            else if (result >= 300 && result < 300 + iridescent::CustomModuleRegistry::maxModules)
                                safe->setSlotType (bus, slot,
                                                   static_cast<ModuleId> (ModuleId::Custom0 + result - 300));
                            else if (result == 999)
                            {
                                safe->processorRef.removeSlot (bus, slot);
                                safe->processorRef.setActiveSlot (bus, slot, false);
                                safe->refreshControls();
                                safe->repaint();
                            }
                        });
}

void IridescentFlowerEditor::setSlotType (int bus, int slot, ModuleId type)
{
    if (slot < 0 || slot >= RackEngine::numSlots)
        return;

    processorRef.setSlotType (bus, slot, type);
    selectSlot (slot);
}

void IridescentFlowerEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto visualBounds = juce::Rectangle<int> (756, detailY, 324, detailHeight);

    if (visualBounds.contains (event.getPosition()))
    {
        const auto type = selectedSlot >= RackEngine::numSlots
                              ? ModuleId::Morph
                              : processorRef.getSlot (busPage - 1, selectedSlot).type;

        if (type != ModuleId::Eq && type != ModuleId::Filter)
        {
            showAvatarMenu (event.getScreenPosition());
            return;
        }
    }

    const auto slot = slotAt (event.getPosition());

    if (slot < 0)
        return;

    selectSlot (slot);

    if (event.mods.isPopupMenu())
    {
        showSlotMenu (busPage - 1, slot, event.getScreenPosition());
        return;
    }

    if (powerBounds (slot).contains (event.getPosition()))
    {
        processorRef.setSlotOn (busPage - 1, slot, ! processorRef.getSlot (busPage - 1, slot).on);
        repaint();
        return;
    }

    dragSource = slot;
    dragTarget = slot;
    dragStart = event.getPosition();
    dragging = false;
}

void IridescentFlowerEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (dragSource < 0)
        return;

    if (! dragging && event.getDistanceFromDragStart() > 6)
        dragging = true;

    if (dragging)
    {
        int insertion = RackEngine::slotsPerBus;
        const auto x = event.getPosition().x;

        for (int i = 0; i < RackEngine::slotsPerBus; ++i)
        {
            if (x < cardBounds (i).getCentreX())
            {
                insertion = i;
                break;
            }
        }

        dragTarget = insertion;
        repaint();
    }
}

void IridescentFlowerEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragging && dragSource >= 0 && dragTarget >= 0)
    {
        const auto finalIndex = dragTarget > dragSource ? dragTarget - 1 : dragTarget;
        processorRef.moveSlotTo (busPage - 1, dragSource, dragTarget);

        if (finalIndex >= 0 && finalIndex < RackEngine::slotsPerBus)
        {
            selectedSlot = finalIndex;
            processorRef.setActiveSlot (busPage - 1, selectedSlot, false);
        }

        refreshControls();
    }

    dragging = false;
    dragSource = -1;
    dragTarget = -1;
    repaint();
}

bool IridescentFlowerEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        toggleVisuals();
        return true;
    }

    return false;
}

void IridescentFlowerEditor::toggleVisuals()
{
    if (auto* p = processorRef.parameters.getParameter (iridescent::ids::visuals))
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);

    repaint();
}

void IridescentFlowerEditor::reloadAvatars()
{
    const auto selectedName = avatarIndex >= 0 && avatarIndex < avatarNames.size()
                                  ? avatarNames[avatarIndex] : juce::String();

    avatarImages.clear();
    avatarNames.clear();

    auto addImage = [this] (const void* data, size_t size, const juce::String& name)
    {
        auto image = juce::ImageFileFormat::loadFrom (data, size);

        if (image.isValid())
        {
            avatarImages.push_back (std::move (image));
            avatarNames.add (name);
        }
    };

    addImage (BinaryData::portrait_pixel_png,
              static_cast<size_t> (BinaryData::portrait_pixel_pngSize), "BUILT-IN / ORIGINAL");
    addImage (BinaryData::avatar_b_pixel_png,
              static_cast<size_t> (BinaryData::avatar_b_pixel_pngSize), "BUILT-IN / GLITCH");
    addImage (BinaryData::avatar_c_pixel_png,
              static_cast<size_t> (BinaryData::avatar_c_pixel_pngSize), "BUILT-IN / TEXTURE");
    addImage (BinaryData::avatar_d_pixel_png,
              static_cast<size_t> (BinaryData::avatar_d_pixel_pngSize), "BUILT-IN / NEGATIVE");

    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("Iridescent Flower")
                            .getChildFile ("Avatars");
    folder.createDirectory();

    auto files = folder.findChildFiles (juce::File::findFiles, false, "*");
    files.sort();

    for (const auto& file : files)
    {
        if (! file.hasFileExtension ("png;jpg;jpeg;webp"))
            continue;

        const auto image = juce::ImageFileFormat::loadFrom (file);

        if (image.isValid())
        {
            avatarImages.push_back (image);
            avatarNames.add ("CUSTOM / " + file.getFileName());
        }
    }

    if (avatarImages.empty())
    {
        avatarImages.push_back ({});
        avatarNames.add ("NO IMAGE");
    }

    const auto preserved = avatarNames.indexOf (selectedName);
    avatarIndex = preserved >= 0 ? preserved : juce::jlimit (0, avatarNames.size() - 1, avatarIndex);
}

void IridescentFlowerEditor::loadAvatarFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    auto image = juce::ImageFileFormat::loadFrom (file);

    if (! image.isValid())
    {
        presetMessage = "IMAGE LOAD FAILED";
        repaint();
        return;
    }

    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("Iridescent Flower")
                            .getChildFile ("Avatars");
    folder.createDirectory();

    auto target = folder.getChildFile (file.getFileName());
    int suffix = 1;

    while (target.existsAsFile() && target != file)
    {
        target = folder.getChildFile (file.getFileNameWithoutExtension()
                                      + "_" + juce::String (suffix++)
                                      + file.getFileExtension());
    }

    if (target != file && ! file.copyFileTo (target))
    {
        presetMessage = "IMAGE COPY FAILED";
        repaint();
        return;
    }

    reloadAvatars();
    const auto savedName = "CUSTOM / " + target.getFileName();
    const auto index = avatarNames.indexOf (savedName);

    if (index >= 0)
        avatarIndex = index;

    avatarStateName = avatarNames[avatarIndex];
    processorRef.parameters.state.setProperty ("avatarImage", avatarStateName, nullptr);
    presetMessage = "AVATAR UPDATED";
    repaint();
}

void IridescentFlowerEditor::openAvatarFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Iridescent Flower")
                      .getChildFile ("Avatars");
    folder.createDirectory();
    folder.startAsProcess();
}

void IridescentFlowerEditor::showAvatarMenu (juce::Point<int> screenPosition)
{
    reloadAvatars();

    juce::PopupMenu menu;

    for (int i = 0; i < avatarNames.size(); ++i)
        menu.addItem (100 + i, avatarNames[i], true, i == avatarIndex);

    menu.addSeparator();
    menu.addItem (1, "LOAD IMAGE...");
    menu.addItem (2, "OPEN AVATAR FOLDER");

    auto safe = juce::Component::SafePointer<IridescentFlowerEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [safe] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result >= 100)
                            {
                                const auto index = result - 100;

                                if (index >= 0 && index < safe->avatarImages.size())
                                {
                                    safe->avatarIndex = index;
                                    safe->avatarStateName = safe->avatarNames[index];
                                    safe->processorRef.parameters.state.setProperty (
                                        "avatarImage", safe->avatarStateName, nullptr);
                                    safe->repaint();
                                }
                            }
                            else if (result == 1)
                            {
                                safe->avatarChooser = std::make_unique<juce::FileChooser> (
                                    "Choose avatar image",
                                    juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                    "*.png;*.jpg;*.jpeg;*.webp");

                                safe->avatarChooser->launchAsync (
                                    juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectFiles,
                                    [safe] (const juce::FileChooser& chooser)
                                    {
                                        if (safe != nullptr)
                                            safe->loadAvatarFile (chooser.getResult());
                                    });
                            }
                            else if (result == 2)
                            {
                                safe->openAvatarFolder();
                            }
                        });
}

void IridescentFlowerEditor::savePreset()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save Iridescent Flower preset",
        IridescentFlowerProcessor::getPresetFolder().getChildFile ("untitled.iflower"),
        "*.iflower");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File())
                                      return;

                                  const auto target = file.hasFileExtension ("iflower")
                                                          ? file : file.withFileExtension ("iflower");
                                  presetMessage = processorRef.savePreset (target)
                                                      ? "PRESET SAVED" : "SAVE FAILED";
                                  repaint();
                              });
}

void IridescentFlowerEditor::loadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Iridescent Flower preset",
        IridescentFlowerProcessor::getPresetFolder(),
        "*.iflower");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file == juce::File())
                                      return;

                                  presetMessage = processorRef.loadPreset (file)
                                                      ? "PRESET LOADED" : "LOAD FAILED";
                                  refreshControls();
                                  repaint();
                              });
}

void IridescentFlowerEditor::openPresetFolder()
{
    IridescentFlowerProcessor::getPresetFolder().createDirectory();
    IridescentFlowerProcessor::getPresetFolder().revealToUser();
}
