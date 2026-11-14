#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace iridescent
{

class IridescentLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    IridescentLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isMouseOverButton, bool isButtonDown) override;

    juce::Font getLabelFont (juce::Label&) override;

    static juce::Font uiFont (float height, bool bold = false);
    static juce::Font displayFont (float height, bool bold = false);
    static juce::Font monoFont (float height, bool bold = false);
    static juce::Font y2kFont (float height);
    static juce::Font serifFont (float height);
    static juce::Font glitchFont (float height);
    static juce::Font moduleFont (int moduleId, float height);

    inline static const juce::Colour voidBlack { 0xff050308 };
    inline static const juce::Colour panel { 0xff0d0814 };
    inline static const juce::Colour panelLight { 0xff171022 };
    inline static const juce::Colour violet { 0xffb34dff };
    inline static const juce::Colour violetBright { 0xffd8a6ff };
    inline static const juce::Colour magenta { 0xffff2e88 };
    inline static const juce::Colour cyan { 0xff42e8ff };
    inline static const juce::Colour acid { 0xffd8ff4a };
    inline static const juce::Colour cream { 0xfff5eaff };
    inline static const juce::Colour dim { 0xff8e7ca6 };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IridescentLookAndFeel)
};

} // namespace iridescent
