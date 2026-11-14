#include "FilterGraph.h"
#include "PixelFont.h"
#include "IridescentLookAndFeel.h"

#include <cmath>

namespace iridescent
{

FilterGraph::FilterGraph (IridescentFlowerProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
}

void FilterGraph::setActive (bool shouldBeActive)
{
    active = shouldBeActive;

    if (active)
        startTimerHz (30);
    else
        stopTimer();
}

float FilterGraph::read (const char* id) const
{
    if (auto* value = processor.parameters.getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}

void FilterGraph::timerCallback()
{
    if (active)
        repaint();
}

float FilterGraph::responseDb (float frequency) const
{
    const auto cutoff = juce::jlimit (20.0f, 20000.0f, read (ids::filterCutoff));
    const auto resonance = juce::jlimit (0.0f, 1.0f, read (ids::filterReso));
    const auto type = juce::jlimit (0, 3, juce::roundToInt (read (ids::filterType)));

    const auto q = 0.5 + resonance * 9.0;
    const auto w = frequency / cutoff;
    const auto denominator = std::sqrt (std::pow (1.0 - w * w, 2.0)
                                        + std::pow (w / q, 2.0));
    double magnitude = 1.0;

    switch (type)
    {
        case 1: magnitude = (w * w) / denominator; break;
        case 2: magnitude = (w / q) / denominator; break;
        case 3: magnitude = std::abs (1.0 - w * w) / denominator; break;
        default: magnitude = 1.0 / denominator; break;
    }

    // Two cascaded 2-pole sections = 24 dB/oct.
    return juce::jlimit (-60.0f, 24.0f,
                         static_cast<float> (40.0 * std::log10 (std::max (1.0e-6, magnitude))));
}

void FilterGraph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const auto area = bounds.toFloat().reduced (10.0f, 14.0f);
    const auto type = juce::jlimit (0, 3, juce::roundToInt (read (ids::filterType)));
    const auto cutoff = juce::jlimit (20.0f, 20000.0f, read (ids::filterCutoff));
    const auto accent = juce::Colour (0xff5ad1ff);

    g.setColour (juce::Colour (0xff07040c));
    g.fillRect (bounds);
    g.setColour (accent.withAlpha (0.55f));
    g.drawRect (bounds, 2);

    // Log-frequency grid.
    for (const auto frequency : { 100.0f, 1000.0f, 10000.0f })
    {
        const auto x = area.getX() + area.getWidth()
                                 * std::log (frequency / 20.0f) / std::log (1000.0f);
        g.setColour (juce::Colour (0xff2b1740));
        g.drawVerticalLine (static_cast<int> (x), area.getY(), area.getBottom());
        PixelFont::draw (g, frequency >= 1000.0f ? juce::String (frequency / 1000.0f, 0) + "K"
                                                  : juce::String (frequency, 0),
                         static_cast<int> (x) + 2, static_cast<int> (area.getBottom()) - 8, 1,
                         IridescentLookAndFeel::dim.withAlpha (0.65f));
    }

    for (int db = 12; db >= -48; db -= 12)
    {
        const auto y = area.getY() + (24.0f - static_cast<float> (db)) / 84.0f * area.getHeight();
        g.setColour (juce::Colour (0xff21102f));
        g.drawHorizontalLine (static_cast<int> (y), area.getX(), area.getRight());
        PixelFont::draw (g, db > 0 ? "+" + juce::String (db) : juce::String (db),
                         static_cast<int> (area.getX()) + 2, static_cast<int> (y) + 1, 1,
                         IridescentLookAndFeel::dim.withAlpha (0.5f));
    }

    auto dbToY = [&area] (float db)
    {
        return area.getY() + (24.0f - db) / 84.0f * area.getHeight();
    };

    juce::Path curve;
    for (int i = 0; i <= 160; ++i)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (i) / 160.0f;
        const auto frequency = 20.0f * std::pow (1000.0f, static_cast<float> (i) / 160.0f);
        const auto y = dbToY (responseDb (frequency));

        if (i == 0) curve.startNewSubPath (x, y);
        else curve.lineTo (x, y);
    }

    auto fill = curve;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.10f));
    g.fillPath (fill);

    g.setColour (accent.withAlpha (0.22f));
    g.strokePath (curve, juce::PathStrokeType (4.0f));
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Chunky sampled points.
    for (int i = 0; i <= 32; ++i)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (i) / 32.0f;
        const auto frequency = 20.0f * std::pow (1000.0f, static_cast<float> (i) / 32.0f);
        const auto y = dbToY (responseDb (frequency));
        g.fillRect (x - 1.0f, y - 1.0f, 3.0f, 3.0f);
    }

    // Cutoff marker / drag handle.
    const auto cutoffX = area.getX() + area.getWidth()
                                      * std::log (cutoff / 20.0f) / std::log (1000.0f);
    const auto handleY = dbToY (responseDb (cutoff));
    g.setColour (IridescentLookAndFeel::magenta.withAlpha (0.42f));
    g.drawVerticalLine (static_cast<int> (cutoffX), area.getY(), area.getBottom());
    g.setColour (IridescentLookAndFeel::magenta);
    g.fillRect (cutoffX - 4.0f, handleY - 4.0f, 8.0f, 8.0f);
    g.setColour (IridescentLookAndFeel::cream);
    g.drawRect (cutoffX - 4.0f, handleY - 4.0f, 8.0f, 8.0f, 1.0f);

    const auto typeName = type == 1 ? "HP" : (type == 2 ? "BP" : (type == 3 ? "NOTCH" : "LP"));
    PixelFont::draw (g, juce::String (typeName) + " / 24DB", static_cast<int> (area.getX()) + 4,
                     static_cast<int> (area.getY()) + 3, 1, IridescentLookAndFeel::acid);
}

} // namespace iridescent
