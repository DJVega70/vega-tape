#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// =====================================================================
//  Vega Productions — hardware‑style look & feel
//  (charcoal panel, machined knobs, cyan illuminated switches, OLED)
// =====================================================================
namespace VegaColours
{
    const juce::Colour background { 0xff1e1e1e };
    const juce::Colour panel      { 0xff2a2a2a };
    const juce::Colour panelLight { 0xff303030 };
    const juce::Colour bar        { 0xff232323 };
    const juce::Colour line       { 0xff3e3e3e };
    const juce::Colour text       { 0xffd8d8d8 };
    const juce::Colour textDim    { 0xff8e8e8e };
    const juce::Colour accent     { 0xff7ff7f2 };
    const juce::Colour accentDim  { 0xff2f6f6c };
    const juce::Colour oled       { 0xff050505 };
    const juce::Colour gold       { 0xffd9b45a };
}

namespace VegaFonts
{
    inline juce::Font ui (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    inline juce::Font mono (float h)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
    }
}

class VegaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VegaLookAndFeel()
    {
        using namespace VegaColours;
        setColour (juce::Slider::textBoxTextColourId, textDim);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accentDim);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Label::textWhenEditingColourId, text);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151515));
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accentDim);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1b1b));
        setColour (juce::ComboBox::textColourId, textDim);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::arrowColourId, textDim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1c1c));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3a3a));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0d0d0));
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1a1a));
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
    }

    // ------------------------------------------------------------------ knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        using namespace VegaColours;
        const bool big = s.getProperties()["big"];
        const auto stepLabels = s.getProperties()["stepLabels"].toString();

        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float tickSpace = big ? 16.0f : 9.0f;
        const float labelSpace = stepLabels.isNotEmpty() ? 18.0f : 0.0f;
        const float size = std::min (area.getWidth() - 2 * (tickSpace + labelSpace),
                                     area.getHeight() - 2 * tickSpace - labelSpace * 1.4f);
        auto knob = juce::Rectangle<float> (size, size).withCentre (area.getCentre().translated (0.0f, labelSpace * 0.5f));
        const auto c = knob.getCentre();
        const float r = size * 0.5f;
        const float angle = startA + pos * (endA - startA);

        // tick marks
        {
            const int numTicks = stepLabels.isNotEmpty() ? juce::StringArray::fromTokens (stepLabels, "|", "").size()
                                                         : (big ? 11 : 11);
            for (int i = 0; i < numTicks; ++i)
            {
                const float t = numTicks > 1 ? (float) i / (float) (numTicks - 1) : 0.0f;
                const float a = startA + t * (endA - startA);
                const bool major = (i == 0 || i == numTicks - 1 || (big && i == numTicks / 2));
                const float r1 = r + (big ? 7.0f : 3.5f), r2 = r1 + (major ? (big ? 7.0f : 5.0f) : (big ? 4.5f : 3.0f));
                g.setColour (textDim.withAlpha (major ? 0.9f : 0.55f));
                g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                            c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), big ? 1.6f : 1.1f);
            }
            if (stepLabels.isNotEmpty())
            {
                auto labels = juce::StringArray::fromTokens (stepLabels, "|", "");
                g.setFont (VegaFonts::ui (9.5f));
                for (int i = 0; i < labels.size(); ++i)
                {
                    const float t = labels.size() > 1 ? (float) i / (float) (labels.size() - 1) : 0.0f;
                    const float a = startA + t * (endA - startA);
                    const float rl = r + 18.0f;
                    auto lc = juce::Point<float> (c.x + rl * std::sin (a), c.y - rl * std::cos (a));
                    const bool sel = juce::roundToInt (pos * (labels.size() - 1)) == i;
                    g.setColour (sel ? accent : textDim);
                    g.drawFittedText (labels[i], juce::Rectangle<int> (40, 12).withCentre (lc.toInt()),
                                      juce::Justification::centred, 1);
                }
            }
        }

        // drop shadow
        {
            juce::Path sh; sh.addEllipse (knob.translated (0, big ? 5.0f : 3.0f).expanded (big ? 3.0f : 1.5f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f), big ? 18 : 9, {}).drawForPath (g, sh);
        }

        if (big)
        {
            // outer machined collar
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3b), c.x, knob.getY(),
                                                     juce::Colour (0xff121212), c.x, knob.getBottom(), false));
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0xff4a4a4a));
            g.drawEllipse (knob.reduced (0.5f), 1.0f);
            knob = knob.reduced (r * 0.13f);
        }

        // knob body
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff353535), c.x - r * 0.4f, c.y - r * 0.6f,
                                                 juce::Colour (0xff141414), c.x + r * 0.5f, c.y + r * 0.8f, true));
        g.fillEllipse (knob);
        // face
        auto face = knob.reduced (knob.getWidth() * 0.06f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c2c), c.x, face.getY(),
                                                 juce::Colour (0xff1a1a1a), c.x, face.getBottom(), false));
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (face, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (knob, 1.0f);

        // pointer
        const float kr = knob.getWidth() * 0.5f;
        const float p1 = big ? kr * 0.68f : kr * 0.35f, p2 = kr * 0.94f;
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawLine (c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle),
                    c.x + p2 * std::sin (angle), c.y - p2 * std::cos (angle), big ? 2.6f : 2.0f);

        if (s.isMouseOverOrDragging())
        {
            g.setColour (accent.withAlpha (0.12f));
            g.drawEllipse (knob.expanded (2.0f), 1.2f);
        }
    }

    // ------------------------------------------------------------------ vertical fader
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace VegaColours;
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float cx = area.getCentreX();
        const float top = area.getY() + 8.0f, bottom = area.getBottom() - 8.0f;

        // ticks
        for (int i = 0; i <= 8; ++i)
        {
            const float ty = top + (bottom - top) * (float) i / 8.0f;
            const bool major = (i % 4 == 0);
            g.setColour (textDim.withAlpha (major ? 0.8f : 0.4f));
            g.drawLine (cx - 20, ty, cx - (major ? 11.0f : 13.0f), ty, 1.0f);
            g.drawLine (cx + (major ? 11.0f : 13.0f), ty, cx + 20, ty, 1.0f);
        }
        // slot
        g.setColour (juce::Colour (0xff121212));
        g.fillRoundedRectangle (cx - 2.0f, top - 4, 4.0f, bottom - top + 8, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (cx - 2.0f, top - 4, 4.0f, bottom - top + 8, 2.0f, 0.8f);

        // cap
        auto cap = juce::Rectangle<float> (28.0f, 16.0f).withCentre ({ cx, sliderPos });
        juce::Path sh; sh.addRoundedRectangle (cap.translated (0, 3), 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 8, {}).drawForPath (g, sh);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5a5a), cx, cap.getY(),
                                                 juce::Colour (0xff222222), cx, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.drawRoundedRectangle (cap, 2.0f, 1.0f);
        g.setColour (s.isMouseOverOrDragging() ? accent : juce::Colours::white);
        g.drawLine (cap.getX() + 3, cap.getCentreY(), cap.getRight() - 3, cap.getCentreY(), 1.6f);
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        auto layout = LookAndFeel_V4::getSliderLayout (s);
        if (s.getTextBoxPosition() == juce::Slider::TextBoxBelow)
        {
            auto b = s.getLocalBounds();
            layout.textBoxBounds = b.removeFromBottom (16);
            layout.sliderBounds = b;
        }
        return layout;
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
            return VegaFonts::ui (11.0f);
        return l.getFont();
    }

    void drawLabel (juce::Graphics& g, juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr && ! l.isBeingEdited())
        {
            g.setColour (VegaColours::textDim);
            g.setFont (VegaFonts::ui (11.0f));
            g.drawFittedText (l.getText(), l.getLocalBounds(), juce::Justification::centred, 1);
            return;
        }
        LookAndFeel_V4::drawLabel (g, l);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::textColourId, VegaColours::textDim);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    // ------------------------------------------------------------------ toggles
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace VegaColours;
        const bool on = b.getToggleState();
        auto area = b.getLocalBounds().toFloat();

        if (b.getProperties()["switch"])
        {
            // small vertical toggle switch with two labels (e.g. Stereo / M‑S)
            const auto labels = juce::StringArray::fromTokens (b.getButtonText(), "|", "");
            auto sw = juce::Rectangle<float> (10.0f, 22.0f).withCentre ({ area.getX() + 12.0f, area.getCentreY() });
            g.setColour (juce::Colour (0xff111111));
            g.fillRoundedRectangle (sw, 5.0f);
            g.setColour (line);
            g.drawRoundedRectangle (sw, 5.0f, 1.0f);
            auto lever = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ sw.getCentreX(), on ? sw.getBottom() - 5.5f : sw.getY() + 5.5f });
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8e8), lever.getX(), lever.getY(),
                                                     juce::Colour (0xff7a7a7a), lever.getRight(), lever.getBottom(), false));
            g.fillEllipse (lever);
            g.setFont (VegaFonts::ui (10.0f));
            if (labels.size() >= 2)
            {
                g.setColour (on ? textDim : text);
                g.drawText (labels[0], (int) sw.getRight() + 5, (int) sw.getY() - 5, 70, 12, juce::Justification::left);
                g.setColour (on ? text : textDim);
                g.drawText (labels[1], (int) sw.getRight() + 5, (int) sw.getBottom() - 7, 70, 12, juce::Justification::left);
            }
            return;
        }

        // illuminated square push‑button
        const float sz = std::min (area.getWidth(), area.getHeight() - 16.0f);
        auto sq = juce::Rectangle<float> (sz, sz).withCentre ({ area.getCentreX(), area.getY() + sz * 0.5f + 1.0f });
        juce::Path sh; sh.addRoundedRectangle (sq.translated (0, 2), 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 6, {}).drawForPath (g, sh);

        g.setColour (juce::Colour (0xff161616));
        g.fillRoundedRectangle (sq, 2.0f);
        auto inner = sq.reduced (sz * 0.14f);
        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (sq.expanded (2.0f), 3.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (sq.reduced (1.0f), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.fillRoundedRectangle (inner.reduced (sz * 0.12f), 1.5f);
            g.setColour (accent.brighter (0.4f));
            g.drawRoundedRectangle (inner, 1.5f, 1.2f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4a), inner.getX(), inner.getY(),
                                                     juce::Colour (0xff262626), inner.getRight(), inner.getBottom(), false));
            g.fillRoundedRectangle (inner, 1.5f);
            g.setColour (juce::Colour (0xff5c5c5c));
            g.drawRoundedRectangle (inner, 1.5f, 1.0f);
        }
        if (over)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (sq, 2.0f);
        }
        g.setColour (textDim);
        g.setFont (VegaFonts::ui (11.0f));
        g.drawText (on ? "On" : "Off", area.withTop (sq.getBottom() + 3.0f).toNearestInt(), juce::Justification::centredTop);
    }

    // ------------------------------------------------------------------ text buttons (top bar)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto col = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId)
                                      : b.findColour (juce::TextButton::buttonColourId);
        if (over) col = col.brighter (0.08f);
        if (down) col = col.darker (0.15f);
        g.setColour (col);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return VegaFonts::ui (10.5f, true); }

    // ------------------------------------------------------------------ combo boxes
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        // chevron
        juce::Path p;
        const float cx = (float) w - 10.0f, cy = (float) h * 0.5f;
        p.startNewSubPath (cx - 4, cy + 2); p.lineTo (cx, cy - 2); p.lineTo (cx + 4, cy + 2);
        g.setColour (box.isMouseOver() ? VegaColours::text : VegaColours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return VegaFonts::ui (10.5f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centred);
    }
    juce::Font getPopupMenuFont() override { return VegaFonts::ui (13.0f); }
};
