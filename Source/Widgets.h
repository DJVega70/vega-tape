#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "VegaLookAndFeel.h"

using APVTS = juce::AudioProcessorValueTreeState;

// =====================================================================
//  Knob with title (same as the rest of the Vega Productions suite)
// =====================================================================
class KnobWithLabel : public juce::Component
{
public:
    KnobWithLabel (APVTS& state, const juce::String& paramId, const juce::String& title, bool big = false)
    {
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (VegaFonts::ui (big ? 12.5f : 11.5f));
        label.setColour (juce::Label::textColourId, VegaColours::text);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 16);
        slider.setMouseDragSensitivity (big ? 320 : 220);
        slider.getProperties().set ("big", big);
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        auto* p = state.getParameter (paramId);
        slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (16));
        slider.setBounds (b);
    }
    juce::Slider slider;
    juce::Label label;
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// =====================================================================
//  Row of illuminated push‑buttons bound to a choice parameter
// =====================================================================
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (APVTS& state, const juce::String& paramId, const juce::String& title,
                   const juce::StringArray& labels, juce::Colour onColour = VegaColours::accent)
    {
        label.setText (title, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (VegaFonts::ui (11.5f));
        label.setColour (juce::Label::textColourId, VegaColours::text);
        addAndMakeVisible (label);

        auto* param = state.getParameter (paramId);
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, onColour);
            b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { show ((int) std::lround (v)); });
        attachment->sendInitialUpdate();
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (18));
        b = b.withSizeKeepingCentre (b.getWidth(), std::min (b.getHeight(), 30));
        const int w = b.getWidth() / std::max (1, buttons.size());
        for (auto* btn : buttons) btn->setBounds (b.removeFromLeft (w).reduced (2, 0));
    }
private:
    void show (int idx)
    {
        for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
    }
    juce::Label label;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// =====================================================================
//  Small latching button bound to a bool parameter
// =====================================================================
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (APVTS& state, const juce::String& paramId, const juce::String& buttonText,
                 juce::Colour onColour = VegaColours::accent)
        : juce::TextButton (buttonText)
    {
        setClickingTogglesState (true);
        setColour (juce::TextButton::buttonOnColourId, onColour);
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    }
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// =====================================================================
//  Analog VU meter  (0 VU = -18 dBFS RMS, 300 ms ballistics)
// =====================================================================
class VuMeter : public juce::Component
{
public:
    explicit VuMeter (juce::String channelName) : name (std::move (channelName)) {}

    void setRms (float rms, float frameSeconds)
    {
        const float vu = juce::jlimit (-24.0f, 6.0f, vega::gainToDb (rms) + 18.0f);
        const float k = 1.0f - std::exp (-frameSeconds / 0.3f);
        needle += (vu - needle) * k;
        peakLamp = vu > 2.0f ? 1.0f : std::max (0.0f, peakLamp - 0.08f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        // bezel
        g.setColour (juce::Colour (0xff0e0e0e));
        g.fillRoundedRectangle (b, 6.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4a), b.getX(), b.getY(),
                                                 juce::Colour (0xff1c1c1c), b.getX(), b.getBottom(), false));
        g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 2.0f);

        auto face = b.reduced (7.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3e9cb), face.getCentreX(), face.getY(),
                                                 juce::Colour (0xffd9cba3), face.getCentreX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 3.0f);

        const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.18f);
        const float rArc = face.getHeight() * 0.80f;
        auto angleFor = [] (float db)
        {
            // map -20 … +3 VU to the arc (roughly logarithmic like a real meter)
            const float t = juce::jmap (std::pow (10.0f, db / 20.0f), std::pow (10.0f, -1.0f), std::pow (10.0f, 0.15f), 0.0f, 1.0f);
            return juce::jmap (t, -0.70f, 0.70f);
        };
        auto pt = [&] (float a, float r) { return juce::Point<float> (pivot.x + r * std::sin (a), pivot.y - r * std::cos (a)); };

        // red zone
        {
            juce::Path red;
            red.addCentredArc (pivot.x, pivot.y, rArc, rArc, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
            g.setColour (juce::Colour (0xffc8322b));
            g.strokePath (red, juce::PathStrokeType (5.0f));
            juce::Path blk;
            blk.addCentredArc (pivot.x, pivot.y, rArc, rArc, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
            g.setColour (juce::Colour (0xff222222));
            g.strokePath (blk, juce::PathStrokeType (1.4f));
        }
        // ticks + labels
        g.setFont (VegaFonts::ui (9.0f, true));
        for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
        {
            const float a = angleFor (db);
            const bool hot = db > -0.5f;
            g.setColour (hot ? juce::Colour (0xffc8322b) : juce::Colour (0xff222222));
            g.drawLine (juce::Line<float> (pt (a, rArc - 1), pt (a, rArc + 7)), 1.2f);
            const int idb = (int) db;
            if (idb == -20 || idb == -10 || idb == -7 || idb == -5 || idb == -3 || idb == 0 || idb == 3)
            {
                const auto lp = pt (a, rArc + 15);
                g.drawText (juce::String (std::abs ((int) db)), juce::Rectangle<float> (22, 11).withCentre (lp),
                            juce::Justification::centred);
            }
        }
        g.setColour (juce::Colour (0xff222222));
        g.setFont (VegaFonts::ui (13.0f, true));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (16), juce::Justification::centred);

        // peak lamp
        auto lamp = juce::Rectangle<float> (7, 7).withCentre ({ face.getRight() - 11, face.getY() + 11 });
        g.setColour (juce::Colour (0xffff3b30).withAlpha (0.25f + 0.75f * peakLamp));
        g.fillEllipse (lamp);

        // needle
        g.saveState();
        g.reduceClipRegion (face.toNearestInt());
        const float a = angleFor (needle);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine (juce::Line<float> (pivot.translated (2, 2), pt (a, rArc + 6).translated (2, 2)), 1.6f);
        g.setColour (juce::Colour (0xff111111));
        g.drawLine (juce::Line<float> (pivot, pt (a, rArc + 6)), 1.4f);
        g.restoreState();

        // pivot cover
        auto cover = juce::Rectangle<float> (face.getWidth(), face.getHeight() * 0.16f).withBottomY (face.getBottom()).withX (face.getX());
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b2b), cover.getX(), cover.getY(),
                                                 juce::Colour (0xff121212), cover.getX(), cover.getBottom(), false));
        g.fillRoundedRectangle (cover, 2.0f);
        g.setColour (VegaColours::textDim);
        g.setFont (VegaFonts::ui (8.5f, true));
        g.drawText (name, cover, juce::Justification::centred);

        // glass glare
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), face.getX(), face.getY(),
                                                 juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
        g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.45f), 3.0f);
    }
private:
    juce::String name;
    float needle = -24.0f, peakLamp = 0.0f;
};

// =====================================================================
//  Animated tape transport: two reels, guides, head block, capstan
// =====================================================================
class ReelDeck : public juce::Component
{
public:
    void advance (float radians, float wobble)
    {
        angle += radians;
        if (angle > juce::MathConstants<float>::twoPi) angle -= juce::MathConstants<float>::twoPi;
        wob = wobble;
        repaint();
    }
    void setTapeInfo (juce::String formula, juce::String speed) { formulaText = std::move (formula); speedText = std::move (speed); }

    void paint (juce::Graphics& g) override
    {
        using namespace VegaColours;
        auto b = getLocalBounds().toFloat();

        // deck plate
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e2e), b.getX(), b.getY(),
                                                 juce::Colour (0xff1b1b1b), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        for (float y = b.getY() + 3; y < b.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
        for (auto p : { b.getTopLeft().translated (10, 10), b.getTopRight().translated (-10, 10),
                        b.getBottomLeft().translated (10, -10), b.getBottomRight().translated (-10, -10) })
            screw (g, p);

        const float r = std::min (b.getHeight() * 0.47f, b.getWidth() * 0.19f);
        const auto cL = juce::Point<float> (b.getX() + b.getWidth() * 0.2f, b.getCentreY() - 2);
        const auto cR = juce::Point<float> (b.getRight() - b.getWidth() * 0.2f, b.getCentreY() - 2);
        const float tapeY = b.getBottom() - 30.0f;
        const auto gL = juce::Point<float> (b.getCentreX() - 175, tapeY), gR = juce::Point<float> (b.getCentreX() + 175, tapeY);

        // tape path (behind reels)
        const float packL = r * 0.78f, packR = r * 0.55f;
        g.setColour (juce::Colour (0xff3a2418));
        g.drawLine (juce::Line<float> (cL.translated (packL * 0.3f, packL * 0.95f), gL.translated (0, 9)), 3.0f);
        g.drawLine (juce::Line<float> (gR.translated (0, 9), cR.translated (-packR * 0.3f, packR * 0.95f)), 3.0f);
        g.drawLine (juce::Line<float> (gL.translated (0, 9), gR.translated (0, 9)), 3.0f);

        reel (g, cL, r, packL, angle);
        reel (g, cR, r, packR, angle * (packL / packR) * 0.62f + 0.7f);

        // guide rollers + capstan + pinch roller
        roller (g, gL, 10.0f, false);
        roller (g, gR, 10.0f, false);
        roller (g, { b.getCentreX() + 118, tapeY - 2 }, 6.0f, true);
        roller (g, { b.getCentreX() + 140, tapeY - 6 }, 12.0f, false);

        // head block
        auto head = juce::Rectangle<float> (190, 92).withCentre ({ b.getCentreX() - 8, tapeY - 32 });
        juce::Path sh; sh.addRoundedRectangle (head.translated (0, 5), 6.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 14, {}).drawForPath (g, sh);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9a96), head.getX(), head.getY(),
                                                 juce::Colour (0xff5a5a58), head.getX(), head.getBottom(), false));
        g.fillRoundedRectangle (head, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (head.reduced (1.0f), 6.0f, 1.0f);
        for (int i = 0; i < 3; ++i)   // erase / record / play heads
        {
            auto h = juce::Rectangle<float> (22, 26).withCentre ({ head.getX() + 45.0f + i * 50.0f, head.getBottom() - 12 });
            g.setColour (juce::Colour (0xff2a2a2a));
            g.fillRoundedRectangle (h, 3.0f);
            g.setColour (juce::Colour (0xffb0b0b0));
            g.fillRect (h.getCentreX() - 0.5f, h.getY() + 3, 1.0f, h.getHeight() - 6);
        }
        // badge
        auto badge = juce::Rectangle<float> (92, 30).withCentre ({ head.getCentreX(), head.getY() + 30 });
        g.setColour (juce::Colour (0xff151515));
        g.fillRoundedRectangle (badge, 15.0f);
        g.setColour (gold);
        g.drawRoundedRectangle (badge.reduced (1.5f), 13.5f, 1.2f);
        g.setFont (VegaFonts::ui (17.0f, true));
        g.drawText ("VT-1", badge, juce::Justification::centred);
        screw (g, { head.getX() + 14, head.getY() + 14 });
        screw (g, { head.getRight() - 14, head.getY() + 14 });

        // plate text
        g.setColour (gold);
        g.setFont (VegaFonts::ui (13.0f, true));
        g.drawText ("Vega Productions", juce::Rectangle<float> (b.getCentreX() - 120, b.getY() + 10, 240, 18), juce::Justification::centred);
        g.setColour (textDim);
        g.setFont (VegaFonts::ui (10.0f));
        g.drawText ("TWO-TRACK VALVE TAPE MACHINE", juce::Rectangle<float> (b.getCentreX() - 140, b.getY() + 27, 280, 14),
                    juce::Justification::centred);
        g.setColour (accent);
        g.setFont (VegaFonts::mono (10.5f));
        g.drawText (formulaText + "  |  " + speedText, juce::Rectangle<float> (b.getCentreX() - 140, b.getY() + 44, 280, 14),
                    juce::Justification::centred);
    }

private:
    static void screw (juce::Graphics& g, juce::Point<float> p)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8a8a), p.x - 3, p.y - 3, juce::Colour (0xff3a3a3a), p.x + 3, p.y + 3, false));
        g.fillEllipse (p.x - 4, p.y - 4, 8, 8);
        g.setColour (juce::Colour (0xff222222));
        g.drawLine (p.x - 2.5f, p.y + 1, p.x + 2.5f, p.y - 1, 1.0f);
    }
    static void roller (juce::Graphics& g, juce::Point<float> c, float r, bool dark)
    {
        auto e = juce::Rectangle<float> (r * 2, r * 2).withCentre (c);
        juce::Path sh; sh.addEllipse (e.translated (0, 3));
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 6, {}).drawForPath (g, sh);
        g.setGradientFill (juce::ColourGradient (dark ? juce::Colour (0xff3a3a3a) : juce::Colour (0xffe2e2de), e.getX(), e.getY(),
                                                 dark ? juce::Colour (0xff101010) : juce::Colour (0xff7a7a76), e.getRight(), e.getBottom(), false));
        g.fillEllipse (e);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (e, 1.0f);
        g.fillEllipse (e.reduced (r * 0.62f));
    }
    void reel (juce::Graphics& g, juce::Point<float> c, float r, float pack, float rot) const
    {
        auto outer = juce::Rectangle<float> (r * 2, r * 2).withCentre (c);
        juce::Path sh; sh.addEllipse (outer.translated (0, 6));
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 18, {}).drawForPath (g, sh);

        // tape pack (seen through the flange windows)
        g.setColour (juce::Colour (0xff4a2c1c));
        g.fillEllipse (juce::Rectangle<float> (pack * 2, pack * 2).withCentre (c));
        g.setColour (juce::Colour (0xff5d3a26));
        for (float rr = r * 0.3f; rr < pack; rr += 3.0f)
            g.drawEllipse (juce::Rectangle<float> (rr * 2, rr * 2).withCentre (c), 0.4f);

        // flange with three windows
        juce::Path flange;
        flange.addEllipse (outer);
        flange.setUsingNonZeroWinding (false);
        const auto spin = juce::AffineTransform::rotation (rot + wob * 0.02f, c.x, c.y);
        for (int i = 0; i < 3; ++i)
        {
            const float a0 = i * juce::MathConstants<float>::twoPi / 3.0f + 0.32f;
            const float a1 = a0 + juce::MathConstants<float>::twoPi / 3.0f - 0.64f;
            juce::Path w;
            w.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0.0f, a0, a1, true);
            w.addCentredArc (c.x, c.y, r * 0.30f, r * 0.30f, 0.0f, a1, a0, false);
            w.closeSubPath();
            flange.addPath (w, spin);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd7d7d2), outer.getX(), outer.getY(),
                                                 juce::Colour (0xff7d7d78), outer.getRight(), outer.getBottom(), false));
        g.fillPath (flange);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (flange, juce::PathStrokeType (1.0f));

        // machined rings
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawEllipse (outer.reduced (r * 0.04f), 1.0f);
        g.drawEllipse (outer.reduced (r * 0.68f), 1.0f);

        // hub
        auto hub = juce::Rectangle<float> (r * 0.42f, r * 0.42f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeeea), hub.getX(), hub.getY(),
                                                 juce::Colour (0xff6e6e6a), hub.getRight(), hub.getBottom(), false));
        g.fillEllipse (hub);
        g.setColour (juce::Colour (0xff1a1a1a));
        for (int i = 0; i < 3; ++i)
        {
            const float a = rot + i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path key; key.addRectangle (-2.0f, -r * 0.2f, 4.0f, r * 0.08f);
            g.fillPath (key, juce::AffineTransform::rotation (a).translated (c.x, c.y));
        }
        g.fillEllipse (juce::Rectangle<float> (r * 0.12f, r * 0.12f).withCentre (c));

        // label on flange (rotates)
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (rot, c.x, c.y));
        g.setColour (juce::Colour (0xff1e3a5f));
        auto lbl = juce::Rectangle<float> (r * 0.5f, r * 0.13f).withCentre ({ c.x, c.y - r * 0.55f });
        g.fillRoundedRectangle (lbl, 2.0f);
        g.setColour (VegaColours::gold);
        g.setFont (VegaFonts::ui (std::max (7.0f, r * 0.08f), true));
        g.drawText ("VEGA TAPE", lbl, juce::Justification::centred);
        g.restoreState();
    }

    float angle = 0.0f, wob = 0.0f;
    juce::String formulaText { "CLASSIC" }, speedText { "15 IPS" };
};
