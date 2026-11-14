#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace iridescent
{

/** Read-only three-band EQ curve. */
class EqGraph final : public juce::Component,
                      private juce::Timer
{
public:
    explicit EqGraph (IridescentFlowerProcessor& processor);
    ~EqGraph() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float responseDb (float frequency) const;
    float read (const char* id) const;

    IridescentFlowerProcessor& processor;
};

} // namespace iridescent
