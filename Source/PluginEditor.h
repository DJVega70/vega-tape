#pragma once
#include "Widgets.h"

// =====================================================================
//  Fixed‑size panel, scaled by the editor (75 – 150 %)
// =====================================================================
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1000, H = 700;
    explicit MainPanel (VegaTapeProcessor&);
    ~MainPanel() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void section (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    VegaTapeProcessor& proc;
    APVTS& state;

    // top bar
    juce::ComboBox presetBox;
    ParamToggle bypassBtn { state, ID::bypass, "BYPASS", juce::Colour (0xffff6a5a) };

    // transport
    ReelDeck deck;

    // machine
    ChoiceButtons formula { state, ID::formula, "Formula", { "CLASSIC", "HOT", "SMOOTH" } };
    ChoiceButtons speed   { state, ID::speed,   "Speed",   { "7.5 IPS", "15 IPS" }, VegaColours::gold };
    ChoiceButtons bias    { state, ID::bias,    "Bias",    { "NOM", "+3", "+5" } };
    KnobWithLabel input   { state, ID::input,  "Input", true };
    KnobWithLabel output  { state, ID::output, "Output", true };
    ParamToggle   levelLink { state, ID::levelLink, "LINK", VegaColours::gold };
    KnobWithLabel satK    { state, ID::sat, "Saturation" };
    KnobWithLabel mixK    { state, ID::mix, "Mix" };

    // imperfections
    KnobWithLabel wowRate { state, ID::wowRate, "Wow Rate" }, wowDepth { state, ID::wowDepth, "Wow Depth" };
    KnobWithLabel flRate  { state, ID::flutterRate, "Flutter Rate" }, flDepth { state, ID::flutterDepth, "Flutter Depth" };
    KnobWithLabel hissK   { state, ID::hiss, "Hiss" };

    // meters
    VuMeter vuL { "LEFT" }, vuR { "RIGHT" };
    juce::TextButton meterIn { "IN" }, meterOut { "OUT" };

    // delay
    ParamToggle   dlyOn   { state, ID::dlyOn, "DELAY ON" };
    ChoiceButtons dlyType { state, ID::dlyType, "Type", { "SLAP", "FDBK", "PING-PONG" } };
    ChoiceButtons dlyMode { state, ID::dlyMode, "Mode", { "INSERT", "SEND" }, VegaColours::gold };
    ParamToggle   dlySync { state, ID::dlySync, "SYNC", VegaColours::gold };
    ParamToggle   dlyLink { state, ID::dlyLink, "L = R", VegaColours::gold };
    KnobWithLabel timeL { state, ID::dlyTimeL, "Time L" }, timeR { state, ID::dlyTimeR, "Time R" };
    KnobWithLabel divL  { state, ID::dlyDivL,  "Time L" }, divR  { state, ID::dlyDivR,  "Time R" };
    KnobWithLabel levL  { state, ID::dlyLevelL, "Level L" }, levR { state, ID::dlyLevelR, "Level R" };
    KnobWithLabel fbK   { state, ID::dlyFb, "Feedback" };
    KnobWithLabel hpK   { state, ID::dlyHp, "HP" }, lpK { state, ID::dlyLp, "LP" };

    juce::Rectangle<int> rMachine, rImperf, rMeters, rDelay;
    int lastProgram = -1;
    float reelSpeed = 0.0f, wobPhase = 0.0f;
};

class VegaTapeEditor : public juce::AudioProcessorEditor
{
public:
    explicit VegaTapeEditor (VegaTapeProcessor&);
    ~VegaTapeEditor() override;
    void resized() override;

private:
    VegaLookAndFeel lnf;
    MainPanel panel;
};
