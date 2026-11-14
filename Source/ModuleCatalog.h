#pragma once

#include <juce_core/juce_core.h>

namespace iridescent
{

enum ModuleId
{
    Morph = 0,
    Comp,
    Crush,
    Filter,
    Lfo,
    Echo,
    Grain,
    Cloud,
    Eq,
    Chorus,
    Distortion,
    Hyper,
    Custom0,
    Custom1,
    Custom2,
    Custom3,
    Custom4,
    Custom5,
    Custom6,
    Custom7,
    NumModules
};

struct ModuleSpec
{
    const char* name;
    const char* shortName;
    const char* tagline;
    juce::uint32 colour;
    const char* onId;
};

inline const ModuleSpec& moduleSpec (int id) noexcept
{
    static const ModuleSpec specs[NumModules] =
    {
        { "MORPH", "MORPH", "INPUT A/B",       0xff35e6ff, "morph_on" },
        { "COMP",  "COMP",  "GLUE",            0xffffc247, "comp_on" },
        { "CRUSH", "CRUSH", "BITS/RATE",       0xffff3d8e, "crush_on" },
        { "FILTER","FILT",  "24DB FILTER",     0xff5ad1ff, "filter_on" },
        { "LFO",   "LFO",   "MOTION",          0xffc24dff, "lfo_on" },
        { "ECHO",  "ECHO",  "SYNC DELAY",      0xffff678e, "echo_on" },
        { "GRAIN", "GRAIN", "GRAIN CLOUD",     0xffd8ff4a, "grain_on" },
        { "CLOUD", "CLOUD", "FDN VERB",        0xffe2d4ff, "cloud_on" },
        { "EQ",    "EQ",    "3 BAND EQ",       0xff9dff6a, "eq_on" },
        { "CHORUS","CHOR",  "CHORUS/FLANGER", 0xff6affd7, "chorus_on" },
        { "DIST",  "DIST",  "DIST/BITCRUSH",   0xffff6a3d, "dist_on" },
        { "HYPER", "HYPER", "WIDE DIMENSION",  0xffb58cff, "hyper_on" },
        { "CUSTOM 1", "C1", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 2", "C2", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 3", "C3", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 4", "C4", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 5", "C5", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 6", "C6", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 7", "C7", "USER DLL", 0xffd8ff4a, nullptr },
        { "CUSTOM 8", "C8", "USER DLL", 0xffd8ff4a, nullptr }
    };

    if (id < 0 || id >= NumModules)
        id = 0;

    return specs[id];
}

namespace ids
{
inline juce::String customParamId (int moduleIndex, int paramIndex)
{
    return "custom_" + juce::String (moduleIndex) + "_" + juce::String (paramIndex);
}

constexpr auto order = "moduleOrder";

constexpr auto morphAmount = "morph_amount";
constexpr auto morphBTrim = "morph_btrim";
constexpr auto morphCurve = "morph_curve";
constexpr auto morphBWidth = "morph_bwidth";
constexpr auto morphSource = "morph_source";

constexpr auto compThreshold = "comp_threshold";
constexpr auto compRatio = "comp_ratio";
constexpr auto compAttack = "comp_attack";
constexpr auto compRelease = "comp_release";
constexpr auto compMakeup = "comp_makeup";
constexpr auto compMix = "comp_mix";

constexpr auto crushBits = "crush_bits";
constexpr auto crushDown = "crush_down";
constexpr auto crushDrive = "crush_drive";
constexpr auto crushTone = "crush_tone";
constexpr auto crushMix = "crush_mix";

constexpr auto filterCutoff = "filter_cutoff";
constexpr auto filterReso = "filter_reso";
constexpr auto filterDrive = "filter_drive";
constexpr auto filterMix = "filter_mix";
constexpr auto filterType = "filter_type";

constexpr auto eqLow = "eq_low";
constexpr auto eqMid = "eq_mid";
constexpr auto eqMidFreq = "eq_midfreq";
constexpr auto eqHigh = "eq_high";
constexpr auto eqOutput = "eq_output";

constexpr auto chorusMode = "chorus_mode";
constexpr auto chorusRate = "chorus_rate";
constexpr auto chorusDepth = "chorus_depth";
constexpr auto chorusFeedback = "chorus_feedback";
constexpr auto chorusMix = "chorus_mix";

constexpr auto distDrive = "dist_drive";
constexpr auto distTone = "dist_tone";
constexpr auto distBits = "dist_bits";
constexpr auto distDown = "dist_down";
constexpr auto distMix = "dist_mix";
constexpr auto distMode = "dist_mode";

constexpr auto hyperDetune = "hyper_detune";
constexpr auto hyperWidth = "hyper_width";
constexpr auto hyperRate = "hyper_rate";
constexpr auto hyperMix = "hyper_mix";

constexpr auto lfoRate = "lfo_rate";
constexpr auto lfoDepth = "lfo_depth";
constexpr auto lfoShape = "lfo_shape";
constexpr auto lfoPhase = "lfo_phase";
constexpr auto lfoPan = "lfo_pan";
constexpr auto lfoStereo = "lfo_stereo";

constexpr auto echoTime = "echo_time";
constexpr auto echoFeedback = "echo_feedback";
constexpr auto echoDiffusion = "echo_diffusion";
constexpr auto echoTone = "echo_tone";
constexpr auto echoMix = "echo_mix";
constexpr auto echoPingpong = "echo_pingpong";
constexpr auto echoSync = "echo_sync";
constexpr auto echoDiv = "echo_div";
constexpr auto echoDivision = "echo_division";

constexpr auto grainSize = "grain_size";
constexpr auto grainDensity = "grain_density";
constexpr auto grainPitch = "grain_pitch";
constexpr auto grainSpray = "grain_spray";
constexpr auto grainFeedback = "grain_feedback";
constexpr auto grainMix = "grain_mix";
constexpr auto grainReverse = "grain_reverse";
constexpr auto grainWidth = "grain_width";
constexpr auto grainFreeze = "grain_freeze";
constexpr auto grainSync = "grain_sync";
constexpr auto grainDiv = "grain_div";
constexpr auto grainDivision = "grain_division";

constexpr auto cloudSize = "cloud_size";
constexpr auto cloudDecay = "cloud_decay";
constexpr auto cloudDamping = "cloud_damping";
constexpr auto cloudMod = "cloud_mod";
constexpr auto cloudPredelay = "cloud_predelay";
constexpr auto cloudLowcut = "cloud_lowcut";
constexpr auto cloudWidth = "cloud_width";
constexpr auto cloudMix = "cloud_mix";

constexpr auto bus1Mix = "bus1_mix";
constexpr auto bus2Mix = "bus2_mix";
constexpr auto bus3Mix = "bus3_mix";
constexpr auto grainBuffer = "grain_buffer";
constexpr auto grainOffset = "grain_offset";
constexpr auto grainRate = "grain_rate";
constexpr auto grainSnap = "grain_snap";

constexpr auto globalMix = "global_mix";
constexpr auto globalOutput = "global_output";
constexpr auto visuals = "visuals_enabled";

constexpr auto morphOn = "morph_on";
constexpr auto compOn = "comp_on";
constexpr auto crushOn = "crush_on";
constexpr auto filterOn = "filter_on";
constexpr auto lfoOn = "lfo_on";
constexpr auto echoOn = "echo_on";
constexpr auto grainOn = "grain_on";
constexpr auto cloudOn = "cloud_on";
} // namespace ids

inline bool isValidOrder (const int* order) noexcept
{
    bool seen[NumModules] = {};

    for (int i = 0; i < NumModules; ++i)
    {
        const auto v = order[i];

        if (v < 0 || v >= NumModules || seen[v])
            return false;

        seen[v] = true;
    }

    return true;
}

inline float divisionBeats (int index) noexcept
{
    static constexpr float divisions[] =
    {
        0.125f,  // 1/32
        1.0f / 6.0f, // 1/16T
        0.25f,   // 1/16
        1.0f / 3.0f, // 1/8T
        0.5f,    // 1/8
        2.0f / 3.0f, // 1/4T
        1.0f,    // 1/4
        2.0f,    // 1/2
        4.0f     // 1 BAR
    };

    return divisions[juce::jlimit (0, 8, index)];
}

} // namespace iridescent
