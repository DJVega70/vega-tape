#include "PluginEditor.h"

// =====================================================================
MainPanel::MainPanel (VegaTapeProcessor& p) : proc (p), state (p.apvts)
{
    // ---- top bar
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) proc.setCurrentProgram (idx);
        lastProgram = idx;
    };
    refreshPresetBox();
    addAndMakeVisible (presetBox);
    addAndMakeVisible (bypassBtn);

    addAndMakeVisible (deck);

    for (auto* c : std::initializer_list<juce::Component*> {
             &formula, &speed, &bias, &input, &output, &levelLink, &satK, &mixK,
             &wowRate, &wowDepth, &flRate, &flDepth, &hissK,
             &vuL, &vuR, &meterIn, &meterOut,
             &dlyOn, &dlyType, &dlyMode, &dlySync, &dlyLink,
             &timeL, &timeR, &levL, &levR, &fbK, &hpK, &lpK })
        addAndMakeVisible (c);
    addChildComponent (divL);
    addChildComponent (divR);

    levelLink.setTooltip ("Output follows the Input in reverse, so you hear more tape colour at the same loudness");
    dlyLink.setTooltip ("Right delay time and level follow the left");
    dlySync.setTooltip ("Lock delay times to the host tempo");

    for (auto* b : { &meterIn, &meterOut })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        b->setColour (juce::TextButton::buttonOnColourId, VegaColours::accent);
    }
    meterOut.setToggleState (! proc.meterShowsInput.load(), juce::dontSendNotification);
    meterIn.setToggleState (proc.meterShowsInput.load(), juce::dontSendNotification);
    meterIn.onClick  = [this] { proc.meterShowsInput.store (true); };
    meterOut.onClick = [this] { proc.meterShowsInput.store (false); };

    setSize (W, H);
    startTimerHz (30);
}

void MainPanel::refreshPresetBox()
{
    lastProgram = proc.getCurrentProgram();
    presetBox.setSelectedId (lastProgram + 1, juce::dontSendNotification);
}

void MainPanel::section (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    auto f = r.toFloat();
    g.setColour (VegaColours::panel);
    g.fillRoundedRectangle (f, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (f.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine (r.getY() + 1, f.getX() + 4, f.getRight() - 4);
    g.setColour (VegaColours::textDim);
    g.setFont (VegaFonts::ui (10.5f, true));
    g.drawText (title, r.getX() + 10, r.getY() + 4, 300, 14, juce::Justification::left);
}

void MainPanel::paint (juce::Graphics& g)
{
    using namespace VegaColours;
    g.fillAll (background);

    // top bar
    g.setColour (bar);
    g.fillRect (0, 0, W, 36);
    g.setColour (line);
    g.drawHorizontalLine (36, 0, (float) W);
    // logo mark: a small gold "V" in a ring
    g.setColour (gold);
    g.drawEllipse (12, 8, 20, 20, 1.4f);
    juce::Path v; v.startNewSubPath (17, 13); v.lineTo (22, 23); v.lineTo (27, 13);
    g.strokePath (v, juce::PathStrokeType (1.8f));
    g.setFont (VegaFonts::ui (15.0f, true));
    g.drawText ("Vega Productions", 38, 0, 160, 36, juce::Justification::centredLeft);
    g.setColour (text);
    g.setFont (VegaFonts::ui (13.0f));
    g.drawText ("Tape  |  VT-1", 196, 0, 160, 36, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (VegaFonts::ui (10.5f));
    g.drawText ("PRESET", 560, 0, 60, 36, juce::Justification::centredRight);

    section (g, rMachine, "RECORD / MACHINE");
    section (g, rImperf,  "TRANSPORT");
    section (g, rMeters,  "METERS");
    section (g, rDelay,   "TAPE DELAY");

    // divider lines inside machine section
    g.setColour (line);
    for (auto* c : std::initializer_list<juce::Component*> { &speed, &input, &bias, &satK })
        g.drawVerticalLine (c->getX() - 6, (float) rMachine.getY() + 22, (float) rMachine.getBottom() - 10);
    for (auto* c : std::initializer_list<juce::Component*> { &dlyType, &dlyMode, &dlySync, &timeL })
        g.drawVerticalLine (c->getX() - 6, (float) rDelay.getY() + 22, (float) rDelay.getBottom() - 10);
}

void MainPanel::resized()
{
    presetBox.setBounds (628, 7, 220, 22);
    bypassBtn.setBounds (W - 128, 7, 116, 22);

    deck.setBounds (10, 44, W - 20, 236);

    // ---- machine row
    rMachine = { 10, 288, W - 20, 122 };
    {
        auto r = rMachine.reduced (12, 0).withTrimmedTop (20).withTrimmedBottom (6);
        auto take = [&] (int w) { auto t = r.removeFromLeft (w); r.removeFromLeft (9); return t; };
        formula.setBounds (take (204).withSizeKeepingCentre (204, 60));
        speed.setBounds (take (140).withSizeKeepingCentre (140, 60));
        input.setBounds (take (92));
        levelLink.setBounds (take (52).withSizeKeepingCentre (52, 24));
        output.setBounds (take (92));
        bias.setBounds (take (150).withSizeKeepingCentre (150, 60));
        satK.setBounds (take (84));
        mixK.setBounds (take (84));
    }

    // ---- transport row
    rImperf = { 10, 418, 540, 150 };
    {
        auto r = rImperf.reduced (14, 0).withTrimmedTop (24).withTrimmedBottom (14);
        const int w = r.getWidth() / 5;
        for (auto* k : { &wowRate, &wowDepth, &flRate, &flDepth, &hissK }) k->setBounds (r.removeFromLeft (w).reduced (4, 0));
    }
    rMeters = { 558, 418, W - 568, 150 };
    {
        auto r = rMeters.reduced (12, 0).withTrimmedTop (22).withTrimmedBottom (10);
        auto btns = r.removeFromRight (52);
        meterIn.setBounds (btns.withSizeKeepingCentre (48, 24).translated (0, -16));
        meterOut.setBounds (btns.withSizeKeepingCentre (48, 24).translated (0, 16));
        r.removeFromRight (8);
        const int w = (r.getWidth() - 10) / 2;
        vuL.setBounds (r.removeFromLeft (w));
        r.removeFromLeft (10);
        vuR.setBounds (r.removeFromLeft (w));
    }

    // ---- delay row
    rDelay = { 10, 576, W - 20, 116 };
    {
        auto r = rDelay.reduced (12, 0).withTrimmedTop (20).withTrimmedBottom (6);
        auto take = [&] (int w) { auto t = r.removeFromLeft (w); r.removeFromLeft (12); return t; };
        dlyOn.setBounds (take (84).withSizeKeepingCentre (84, 28));
        dlyType.setBounds (take (220).withSizeKeepingCentre (220, 60));
        dlyMode.setBounds (take (130).withSizeKeepingCentre (130, 60));
        auto sl = take (64);
        dlySync.setBounds (sl.withSizeKeepingCentre (64, 24).translated (0, -15));
        dlyLink.setBounds (sl.withSizeKeepingCentre (64, 24).translated (0, 15));
        const int kw = r.getWidth() / 7;
        auto knob = [&] { return r.removeFromLeft (kw); };
        auto t1 = knob(); timeL.setBounds (t1); divL.setBounds (t1);
        auto t2 = knob(); timeR.setBounds (t2); divR.setBounds (t2);
        levL.setBounds (knob()); levR.setBounds (knob());
        fbK.setBounds (knob()); hpK.setBounds (knob()); lpK.setBounds (knob());
    }
}

void MainPanel::timerCallback()
{
    const float dt = 1.0f / 30.0f;
    const bool showIn = proc.meterShowsInput.load();
    vuL.setRms ((showIn ? proc.vuIn : proc.vuOut)[0].load(), dt);
    vuR.setRms ((showIn ? proc.vuIn : proc.vuOut)[1].load(), dt);

    // reels: 15 ips spins twice as fast as 7.5 ips; ease in / out like a real motor
    const bool fast = (int) state.getRawParameterValue (ID::speed)->load() == 1;
    const float target = proc.transportActivity.load() * (fast ? 2.4f : 1.2f);
    reelSpeed += (target - reelSpeed) * 0.06f;
    deck.advance (reelSpeed * dt, state.getRawParameterValue (ID::wowDepth)->load() * 0.01f * std::sin (wobPhase += 0.2f));

    static const char* fNames[] { "CLASSIC FORMULA", "HOT FORMULA", "SMOOTH FORMULA" };
    deck.setTapeInfo (fNames[juce::jlimit (0, 2, (int) state.getRawParameterValue (ID::formula)->load())],
                      fast ? "15 IPS" : "7.5 IPS");

    // delay: show ms or note‑division knobs, grey‑out R when linked
    const bool sync = state.getRawParameterValue (ID::dlySync)->load() > 0.5f;
    const bool link = state.getRawParameterValue (ID::dlyLink)->load() > 0.5f;
    timeL.setVisible (! sync); timeR.setVisible (! sync);
    divL.setVisible (sync);    divR.setVisible (sync);
    for (auto* k : { &timeR, &divR, &levR }) { k->setEnabled (! link); k->setAlpha (link ? 0.4f : 1.0f); }
    const bool on = state.getRawParameterValue (ID::dlyOn)->load() > 0.5f;
    for (auto* k : std::initializer_list<juce::Component*> { &dlyType, &dlyMode, &dlySync, &dlyLink, &timeL, &divL, &levL, &fbK, &hpK, &lpK })
        k->setAlpha (on ? 1.0f : 0.55f);
    if (! link) for (auto* k : { &timeR, &divR, &levR }) k->setAlpha (on ? 1.0f : 0.55f);

    if (proc.getCurrentProgram() != lastProgram) refreshPresetBox();
}

// =====================================================================
VegaTapeEditor::VegaTapeEditor (VegaTapeProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);
    panel.setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (MainPanel::W * 3 / 4, MainPanel::H * 3 / 4, MainPanel::W * 3 / 2, MainPanel::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainPanel::W / MainPanel::H);
    setSize (MainPanel::W, MainPanel::H);
}

VegaTapeEditor::~VegaTapeEditor()
{
    panel.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void VegaTapeEditor::resized()
{
    const float s = (float) getWidth() / (float) MainPanel::W;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, MainPanel::W, MainPanel::H);
}
