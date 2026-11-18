#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================
//  Vega Productions Tape (VT-1) — parameter IDs and layout
// =====================================================================
namespace ID
{
    // machine
    inline constexpr const char* formula   = "formula";     // 0 Classic, 1 Hot, 2 Smooth
    inline constexpr const char* speed     = "speed";       // 0 = 7.5 ips, 1 = 15 ips
    inline constexpr const char* bias      = "bias";        // 0 Nom, 1 +3, 2 +5 (over-bias)
    inline constexpr const char* input     = "input";       // dB
    inline constexpr const char* output    = "output";      // dB
    inline constexpr const char* levelLink = "levelLink";   // output follows -input
    inline constexpr const char* sat       = "sat";         // %
    inline constexpr const char* mix       = "mix";         // %
    inline constexpr const char* bypass    = "bypass";

    // transport imperfections
    inline constexpr const char* wowRate      = "wowRate";
    inline constexpr const char* wowDepth     = "wowDepth";
    inline constexpr const char* flutterRate  = "flutterRate";
    inline constexpr const char* flutterDepth = "flutterDepth";
    inline constexpr const char* hiss         = "hiss";

    // tape delay
    inline constexpr const char* dlyOn     = "dlyOn";
    inline constexpr const char* dlyType   = "dlyType";     // 0 Slap, 1 Feedback, 2 Ping-Pong
    inline constexpr const char* dlyMode   = "dlyMode";     // 0 Insert, 1 Send / Return
    inline constexpr const char* dlySync   = "dlySync";
    inline constexpr const char* dlyLink   = "dlyLink";     // R time + level follow L
    inline constexpr const char* dlyTimeL  = "dlyTimeL";    // ms
    inline constexpr const char* dlyTimeR  = "dlyTimeR";
    inline constexpr const char* dlyDivL   = "dlyDivL";     // note division (sync)
    inline constexpr const char* dlyDivR   = "dlyDivR";
    inline constexpr const char* dlyLevelL = "dlyLevelL";   // dB
    inline constexpr const char* dlyLevelR = "dlyLevelR";
    inline constexpr const char* dlyFb     = "dlyFb";       // %
    inline constexpr const char* dlyHp     = "dlyHp";       // Hz
    inline constexpr const char* dlyLp     = "dlyLp";       // Hz
}

namespace Divisions
{
    inline const juce::StringArray names { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                           "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    // length in quarter notes
    inline constexpr float beats[] { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f,
                                     0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v)) + " %"; };
    auto db  = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " dB"; };
    auto hz  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (juce::roundToInt (v)) + " Hz"; };
    auto rateFmt = [] (float v, int) { return String (v, 2) + " Hz"; };
    auto ms  = [] (float v, int) { return String (juce::roundToInt (v)) + " ms"; };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto addC = [&] (const char* id, const char* name, StringArray items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // ---- machine
    addC (ID::formula, "Tape Formula", { "Classic", "Hot", "Smooth" }, 0);
    addC (ID::speed,   "Tape Speed",   { "7.5 ips", "15 ips" }, 1);
    addC (ID::bias,    "Bias",         { "Nom", "+3", "+5" }, 0);
    addF (ID::input,   "Input",  { -18.0f, 18.0f, 0.1f }, 0.0f, db);
    addF (ID::output,  "Output", { -18.0f, 18.0f, 0.1f }, 0.0f, db);
    addB (ID::levelLink, "Level Link", false);
    addF (ID::sat,     "Saturation", { 0.0f, 100.0f, 0.1f }, 35.0f, pct);
    addF (ID::mix,     "Mix",        { 0.0f, 100.0f, 0.1f }, 100.0f, pct);
    addB (ID::bypass,  "Bypass", false);

    // ---- wow / flutter / hiss
    addF (ID::wowRate,      "Wow Rate",      { 0.1f, 3.0f, 0.01f, 0.5f }, 0.55f, rateFmt);
    addF (ID::wowDepth,     "Wow Depth",     { 0.0f, 100.0f, 0.1f }, 12.0f, pct);
    addF (ID::flutterRate,  "Flutter Rate",  { 3.0f, 25.0f, 0.01f, 0.5f }, 9.0f, rateFmt);
    addF (ID::flutterDepth, "Flutter Depth", { 0.0f, 100.0f, 0.1f }, 10.0f, pct);
    addF (ID::hiss,         "Hiss",          { 0.0f, 100.0f, 0.1f }, 0.0f, pct);

    // ---- delay
    addB (ID::dlyOn,   "Delay On", false);
    addC (ID::dlyType, "Delay Type", { "Slap", "Feedback", "Ping-Pong" }, 0);
    addC (ID::dlyMode, "Delay Mode", { "Insert", "Send / Return" }, 0);
    addB (ID::dlySync, "Delay Sync", false);
    addB (ID::dlyLink, "Delay Link", true);
    addF (ID::dlyTimeL, "Delay Time L", { 10.0f, 1500.0f, 1.0f, 0.45f }, 120.0f, ms);
    addF (ID::dlyTimeR, "Delay Time R", { 10.0f, 1500.0f, 1.0f, 0.45f }, 120.0f, ms);
    addC (ID::dlyDivL, "Delay Division L", Divisions::names, 5);
    addC (ID::dlyDivR, "Delay Division R", Divisions::names, 5);
    addF (ID::dlyLevelL, "Delay Level L", { -36.0f, 6.0f, 0.1f }, -8.0f, db);
    addF (ID::dlyLevelR, "Delay Level R", { -36.0f, 6.0f, 0.1f }, -8.0f, db);
    addF (ID::dlyFb,   "Delay Feedback", { 0.0f, 95.0f, 0.1f }, 35.0f, pct);
    addF (ID::dlyHp,   "Delay HP", { 20.0f, 2000.0f, 1.0f, 0.3f }, 150.0f, hz);
    addF (ID::dlyLp,   "Delay LP", { 1000.0f, 20000.0f, 1.0f, 0.3f }, 6000.0f, hz);

    return layout;
}
