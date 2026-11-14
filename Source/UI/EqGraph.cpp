#include "EqGraph.h"
#include "PixelFont.h"
#include "IridescentLookAndFeel.h"

#include <cmath>

namespace iridescent
{

EqGraph::EqGraph (IridescentFlowerProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (15);
}

void EqGraph::timerCallback()
{
    if (isVisible())
        repaint();
}

float EqGraph::read (const char* id) const
{
    if (auto* value = processor.parameters.getRawParameterValue (id))
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}

float EqGraph::responseDb (float frequency) const
{
    const auto low = juce::jlimit (-18.0f, 18.0f, read (ids::eqLow));
    const auto mid = juce::jlimit (-18.0f, 18.0f, read (ids::eqMid));
    const auto high = juce::jlimit (-18.0f, 18.0f, read (ids::eqHigh));
    const auto midFreq = juce::jlimit (200.0f, 8000.0f, read (ids::eqMidFreq));

    const auto lowDb = low / (1.0f + std::pow (frequency / 120.0f, 2.0f));
    const auto highDb = high * (frequency * frequency)
                      / (frequency * frequency + 8000.0f * 8000.0f);
    const auto distance = std::log2 (frequency / midFreq) / 0.7f;
    const auto midDb = mid * std::exp (-0.5f * distance * distance);
    return juce::jlimit (-24.0f, 24.0f, lowDb + midDb + highDb);
}

void EqGraph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const auto area = bounds.toFloat().reduced (10.0f, 14.0f);
    const auto accent = juce::Colour (0xff9dff6a);

    g.setColour (juce::Colour (0xff07040c));
    g.fillRect (bounds);
    g.setColour (accent.withAlpha (0.55f));
    g.drawRect (bounds, 2);

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

    auto dbToY = [&area] (float db)
    {
        return area.getCentreY() - (db / 24.0f) * area.getHeight() * 0.5f;
    };

    g.setColour (juce::Colour (0xff21102f));
    g.drawHorizontalLine (static_cast<int> (dbToY (0.0f)), area.getX(), area.getRight());

    for (int db = -18; db <= 18; db += 6)
    {
        const auto y = dbToY (static_cast<float> (db));
        g.setColour (juce::Colour (0xff21102f));
        g.drawHorizontalLine (static_cast<int> (y), area.getX(), area.getRight());
        PixelFont::draw (g, db > 0 ? "+" + juce::String (db) : juce::String (db),
                         static_cast<int> (area.getX()) + 2, static_cast<int> (y) + 1, 1,
                         IridescentLookAndFeel::dim.withAlpha (0.5f));
    }

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
    fill.lineTo (area.getRight(), area.getCentreY());
    fill.lineTo (area.getX(), area.getCentreY());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    for (int i = 0; i <= 32; ++i)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (i) / 32.0f;
        const auto frequency = 20.0f * std::pow (1000.0f, static_cast<float> (i) / 32.0f);
        const auto y = dbToY (responseDb (frequency));
        g.fillRect (x - 1.0f, y - 1.0f, 3.0f, 3.0f);
    }

    PixelFont::draw (g, "3 BAND / READ ONLY", static_cast<int> (area.getX()) + 4,
                     static_cast<int> (area.getY()) + 3, 1, IridescentLookAndFeel::acid);
}

} // namespace iridescent
