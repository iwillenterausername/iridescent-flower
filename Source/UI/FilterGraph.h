#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace iridescent
{

/** Interactive log-frequency filter response display. */
class FilterGraph final : public juce::Component,
                          private juce::Timer
{
public:
    explicit FilterGraph (IridescentFlowerProcessor& processor);
    ~FilterGraph() override = default;

    void paint (juce::Graphics&) override;
    void setActive (bool shouldBeActive);

private:
    void timerCallback() override;

    float responseDb (float frequency) const;
    float read (const char* id) const;

    IridescentFlowerProcessor& processor;
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterGraph)
};

} // namespace iridescent
