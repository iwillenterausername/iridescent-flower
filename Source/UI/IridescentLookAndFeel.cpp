#include "IridescentLookAndFeel.h"
#include "PixelFont.h"
#include "../ModuleCatalog.h"

#include <BinaryData.h>

namespace iridescent
{

IridescentLookAndFeel::IridescentLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff120a1d));
    setColour (juce::PopupMenu::textColourId, cream);
    setColour (juce::PopupMenu::highlightedTextColourId, voidBlack);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan);
    setColour (juce::ComboBox::textColourId, cream);
    setColour (juce::ComboBox::arrowColourId, cyan);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a2d66));
    setColour (juce::Label::textColourId, cream);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff120a1d));
    setColour (juce::TooltipWindow::textColourId, cream);
    setColour (juce::TooltipWindow::outlineColourId, violet);
}

juce::Font IridescentLookAndFeel::uiFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Bahnschrift", height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font IridescentLookAndFeel::displayFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Bahnschrift", height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font IridescentLookAndFeel::monoFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("Consolas", height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font IridescentLookAndFeel::getLabelFont (juce::Label& label)
{
    return uiFont (juce::jmax (11.0f, static_cast<float> (label.getHeight()) * 0.5f), false);
}

void IridescentLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto diameter = juce::jmin (bounds.getWidth() - 12.0f, 68.0f);
    const auto centre = juce::Point<float> (bounds.getCentreX(), bounds.getY() + diameter * 0.5f + 5.0f);
    const auto radius = diameter * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto active = slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (false);
    const auto arcRadius = radius + 5.0f;

    // Chunky pixel ticks.
    for (int i = 0; i <= 12; ++i)
    {
        const auto tickAngle = rotaryStartAngle
                             + (static_cast<float> (i) / 12.0f) * (rotaryEndAngle - rotaryStartAngle);
        const auto inner = radius + 10.0f;
        const auto outer = radius + (i % 3 == 0 ? 15.0f : 12.0f);
        const auto p1 = centre + juce::Point<float> (std::sin (tickAngle), -std::cos (tickAngle)) * inner;
        const auto p2 = centre + juce::Point<float> (std::sin (tickAngle), -std::cos (tickAngle)) * outer;
        g.setColour (i % 3 == 0 ? cream.withAlpha (0.32f) : violet.withAlpha (0.22f));
        g.drawLine ({ p1, p2 }, i % 3 == 0 ? 2.0f : 1.0f);
    }

    // Hard-edged segmented arc, not a smooth gradient.
    constexpr int segments = 26;
    for (int i = 0; i < segments; ++i)
    {
        const auto p0 = static_cast<float> (i) / segments;
        const auto p1 = static_cast<float> (i + 1) / segments;
        const auto a0 = rotaryStartAngle + p0 * (rotaryEndAngle - rotaryStartAngle);
        const auto a1 = rotaryStartAngle + p1 * (rotaryEndAngle - rotaryStartAngle) - 0.025f;
        const auto lit = p0 <= sliderPos;

        juce::Path segment;
        segment.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, a0, a1, true);
        g.setColour (lit ? (active ? cyan : violetBright) : juce::Colour (0xff1b0b2b));
        g.strokePath (segment, juce::PathStrokeType (5.0f, juce::PathStrokeType::mitered,
                                                      juce::PathStrokeType::butt));
    }

    const auto knob = juce::Rectangle<float> (diameter, diameter).withCentre (centre);
    g.setColour (juce::Colour (0xff050309));
    g.fillEllipse (knob.translated (3.0f, 3.0f));
    g.setColour (juce::Colour (0xff0b0613));
    g.fillEllipse (knob);
    g.setColour (active ? cyan : juce::Colour (0xff4d2b6e));
    g.drawEllipse (knob, 2.0f);
    g.setColour (juce::Colour (0xff2b1640));
    g.drawEllipse (knob.reduced (7.0f), 1.0f);

    const auto pointerStart = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (radius * 0.25f);
    const auto pointerEnd = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (radius * 0.79f);
    g.setColour (active ? acid : cream);
    g.drawLine ({ pointerStart, pointerEnd }, 4.0f);
    g.setColour (magenta);
    g.fillRect (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));

    const auto nameArea = juce::Rectangle<int> (static_cast<int> (bounds.getX()),
                                                 static_cast<int> (bounds.getY() + diameter + 11.0f),
                                                 static_cast<int> (bounds.getWidth()), 10);
    PixelFont::drawCentred (g, slider.getName().toUpperCase(), nameArea, 1,
                            cream.withAlpha (0.68f));

    const auto valueArea = nameArea.translated (0, 11);
    PixelFont::drawCentred (g, slider.getTextFromValue (slider.getValue()).toUpperCase(), valueArea, 1,
                            active ? acid : cream);
}

void IridescentLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = button.getLocalBounds();
    const auto on = button.getToggleState();
    auto fill = on ? juce::Colour (0xff3b1252) : juce::Colour (0xff0b0613);

    if (isButtonDown)
        fill = fill.brighter (0.10f);
    else if (isMouseOverButton)
        fill = fill.brighter (0.05f);

    g.setColour (fill);
    g.fillRect (bounds);
    g.setColour (on ? cyan : juce::Colour (0xff432a5b));
    g.drawRect (bounds, 2);

    const auto led = juce::Rectangle<int> (8, 8)
                         .withCentre ({ bounds.getX() + 12, bounds.getCentreY() });
    g.setColour (on ? acid : juce::Colour (0xff49365b));
    g.fillRect (led);

    PixelFont::draw (g, button.getButtonText().toUpperCase(),
                     bounds.getX() + 22, bounds.getCentreY() - 3, 1,
                     on ? cream : dim);
}

void IridescentLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                       int buttonX, int buttonY, int buttonW, int buttonH,
                                       juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<int> (0, 0, width, height);
    g.setColour (isButtonDown ? juce::Colour (0xff21102e) : juce::Colour (0xff0b0613));
    g.fillRect (bounds);
    g.setColour (box.hasKeyboardFocus (false) ? cyan : juce::Colour (0xff49315e));
    g.drawRect (bounds, 2);

    const auto cx = width - 16;
    const auto cy = height / 2;
    juce::Path arrow;
    arrow.startNewSubPath (static_cast<float> (cx - 5), static_cast<float> (cy - 3));
    arrow.lineTo (static_cast<float> (cx), static_cast<float> (cy + 3));
    arrow.lineTo (static_cast<float> (cx + 5), static_cast<float> (cy - 3));
    g.setColour (acid);
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered,
                                                juce::PathStrokeType::butt));
}

void IridescentLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (7, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (monoFont (10.0f, true));
    label.setColour (juce::Label::textColourId, cream);
}

void IridescentLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos);

    if (style != juce::Slider::LinearHorizontal)
        return;

    auto bounds = juce::Rectangle<int> (x, y, width, height);
    const auto labelArea = bounds.removeFromTop (13);
    PixelFont::draw (g, slider.getName().toUpperCase(), labelArea.getX(), labelArea.getY(), 1, dim);
    PixelFont::drawRight (g, slider.getTextFromValue (slider.getValue()).toUpperCase(),
                          labelArea, 1, cream.withAlpha (0.78f));

    const auto track = juce::Rectangle<float> (static_cast<float> (bounds.getX()),
                                                static_cast<float> (bounds.getCentreY() - 2),
                                                static_cast<float> (bounds.getWidth()), 4.0f);
    g.setColour (juce::Colour (0xff241031));
    g.fillRect (track);

    const auto position = juce::jlimit (track.getX(), track.getRight(), sliderPos);
    auto filled = track;
    filled.setRight (position);
    g.setColour (cyan);
    g.fillRect (filled);
    g.setColour (magenta);
    g.fillRect (juce::Rectangle<float> (10.0f, 10.0f)
                    .withCentre ({ position, track.getCentreY() }));
    g.setColour (cream);
    g.drawRect (juce::Rectangle<float> (10.0f, 10.0f)
                    .withCentre ({ position, track.getCentreY() }), 1.0f);
}

void IridescentLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour&, bool isMouseOverButton,
                                               bool isButtonDown)
{
    const auto bounds = button.getLocalBounds();
    g.setColour (button.getToggleState() ? juce::Colour (0xff3b1252)
                                         : (isButtonDown ? juce::Colour (0xff2c1440) : juce::Colour (0xff0b0613)));
    g.fillRect (bounds);
    g.setColour (button.getToggleState() ? acid : (isMouseOverButton ? acid : juce::Colour (0xff42305a)));
    g.drawRect (bounds, button.getToggleState() ? 2 : 1);
}

void IridescentLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                         bool, bool)
{
    g.setFont (y2kFont (13.0f));
    g.setColour ((button.isMouseOver() || button.getToggleState()) ? acid : cream);
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(),
                juce::Justification::centred, false);
}

namespace
{
juce::Typeface::Ptr loadTypeface (const void* data, size_t size)
{
    return juce::Typeface::createSystemTypefaceFor (data, size);
}

juce::Typeface::Ptr orbitronTypeface()
{
    static auto typeface = loadTypeface (BinaryData::Orbitron_ttf,
                                         static_cast<size_t> (BinaryData::Orbitron_ttfSize));
    return typeface;
}

juce::Typeface::Ptr serifTypeface()
{
    static auto typeface = loadTypeface (BinaryData::CormorantGaramond_ttf,
                                         static_cast<size_t> (BinaryData::CormorantGaramond_ttfSize));
    return typeface;
}

juce::Typeface::Ptr glitchTypeface()
{
    static auto typeface = loadTypeface (BinaryData::RubikGlitch_ttf,
                                         static_cast<size_t> (BinaryData::RubikGlitch_ttfSize));
    return typeface;
}

juce::Typeface::Ptr pixelTypeface()
{
    static auto typeface = loadTypeface (BinaryData::PressStart2P_ttf,
                                         static_cast<size_t> (BinaryData::PressStart2P_ttfSize));
    return typeface;
}
} // namespace

juce::Font IridescentLookAndFeel::y2kFont (float height)
{
    return juce::Font (juce::FontOptions (orbitronTypeface()).withHeight (height));
}

juce::Font IridescentLookAndFeel::serifFont (float height)
{
    return juce::Font (juce::FontOptions (serifTypeface()).withHeight (height));
}

juce::Font IridescentLookAndFeel::glitchFont (float height)
{
    return juce::Font (juce::FontOptions (glitchTypeface()).withHeight (height));
}

juce::Font IridescentLookAndFeel::moduleFont (int moduleId, float height)
{
    if (moduleId == Comp)
        return serifFont (height);

    if (moduleId == Distortion)
        return juce::Font (juce::FontOptions (pixelTypeface()).withHeight (height));

    if (moduleId == Grain)
        return glitchFont (height);

    return y2kFont (height);
}

} // namespace iridescent
