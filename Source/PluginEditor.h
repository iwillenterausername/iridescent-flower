#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "UI/EqGraph.h"
#include "UI/FilterGraph.h"
#include "UI/IridescentLookAndFeel.h"

class IridescentFlowerEditor final : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit IridescentFlowerEditor (IridescentFlowerProcessor&);
    ~IridescentFlowerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void selectSlotForSnapshot (int slot) { selectSlot (slot); }
    void setPageForSnapshot (int page, bool selectCustom = false);
    juce::String getKnobTextForSnapshot (int index)
    {
        if (index < 0 || index >= static_cast<int> (knobs.size()))
            return {};

        return knobs[static_cast<size_t> (index)].getTextFromValue (
            knobs[static_cast<size_t> (index)].getValue());
    }

private:
    class KnobSlider final : public juce::Slider
    {
    public:
        std::function<void()> onNumericClick;

        void mouseDown (const juce::MouseEvent& event) override
        {
            const auto diameter = juce::jmin (getWidth() - 12, 68);
            const auto valueTop = diameter + 20;
            const auto y = event.getPosition().y;

            if (event.mods.isLeftButtonDown()
                && y >= valueTop && y <= valueTop + 16)
            {
                if (onNumericClick != nullptr)
                    onNumericClick();
                return;
            }

            juce::Slider::mouseDown (event);
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (onNumericClick != nullptr)
                onNumericClick();
        }
    };

    struct KnobDesc { const char* id; const char* label; };
    struct ToggleDesc { const char* id; const char* label; };
    struct ChoiceDesc { const char* id; const char* label; juce::StringArray options; };
    enum class GraphKind { none, filter, eq };

    struct Description
    {
        std::vector<KnobDesc> knobs;
        std::vector<ToggleDesc> toggles;
        std::vector<ChoiceDesc> choices;
        GraphKind graph = GraphKind::none;
    };

    static Description describe (iridescent::ModuleId type);

    void timerCallback() override;
    void selectSlot (int slot);
    void refreshControls();
    void showNumericEditor (juce::Slider& slider);
    void showSlotMenu (int bus, int slot, juce::Point<int> screenPosition);
    void setSlotType (int bus, int slot, iridescent::ModuleId type);
    void savePreset();
    void loadPreset();
    void openPresetFolder();
    void toggleVisuals();
    void showAvatarMenu (juce::Point<int> screenPosition);
    void reloadAvatars();
    void loadAvatarFile (const juce::File& file);
    void openAvatarFolder();

    juce::Rectangle<int> cardBounds (int localIndex) const;
    int slotAt (juce::Point<int> position) const;
    juce::Rectangle<int> busBadgeBounds (int localIndex) const;
    juce::Rectangle<int> powerBounds (int localIndex) const;

    void paintHeader (juce::Graphics&);
    void paintRack (juce::Graphics&);
    void paintDetail (juce::Graphics&);
    void paintVisual (juce::Graphics&);
    void paintAvatar (juce::Graphics&, juce::Rectangle<int>);
    void paintChorusVisual (juce::Graphics&, juce::Rectangle<int>);
    void paintGrainVisual (juce::Graphics&, juce::Rectangle<int>);
    void paintLfoVisual (juce::Graphics&, juce::Rectangle<int>);
    void paintFooter (juce::Graphics&);

    IridescentFlowerProcessor& processorRef;
    iridescent::IridescentLookAndFeel lookAndFeel;
    iridescent::FilterGraph filterGraph;
    iridescent::EqGraph eqGraph;

    std::array<KnobSlider, 10> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> knobAttachments;
    std::array<juce::ToggleButton, 2> toggles;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 2> toggleAttachments;
    std::array<juce::ComboBox, 2> combos;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 2> comboAttachments;

    juce::TextButton presetButton { "PRESET" };
    juce::TextButton visualsButton { "VISUALS F" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> visualsAttachment;
    juce::TextButton previousButton { "<" };
    juce::TextButton nextButton { ">" };

    juce::TextButton bus1Button { "BUS 1" };
    juce::TextButton bus2Button { "BUS 2" };
    juce::TextButton bus3Button { "BUS 3" };
    juce::TextButton masterButton { "MASTER" };
    juce::TextButton morphButton { "MORPH" };

    juce::Slider globalMix, globalOutput, bus1Out, bus2Out, bus3Out;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 5> globalAttachments;

    juce::Image textureImage;
    std::vector<juce::Image> avatarImages;
    juce::StringArray avatarNames;
    juce::String avatarStateName;
    int avatarIndex = 0;
    int avatarClock = 0;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> avatarChooser;

    int selectedSlot = 0;
    int busPage = 1;
    int dragSource = -1;
    int dragTarget = -1;
    bool dragging = false;
    juce::Point<int> dragStart;
    int animFrame = 0;
    juce::String presetMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IridescentFlowerEditor)
};
