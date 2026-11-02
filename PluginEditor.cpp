#include "PluginEditor.h"

using namespace pwui;

namespace
{
    juce::Font uiFont (float size, bool bold = false, float kerning = 0.0f)
    {
        juce::Font f { juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain) };
        if (kerning != 0.0f) f.setExtraKerningFactor (kerning);
        return f;
    }

    juce::Font monoFont (float size)
    {
        return juce::Font { juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain) };
    }

    juce::String upper (const juce::String& t)
    {
        return t.toUpperCase().replace (juce::String::fromUTF8 ("ä"), juce::String::fromUTF8 ("Ä"))
                              .replace (juce::String::fromUTF8 ("ö"), juce::String::fromUTF8 ("Ö"))
                              .replace (juce::String::fromUTF8 ("ü"), juce::String::fromUTF8 ("Ü"));
    }

    juce::Path cablePath (juce::Point<float> a, juce::Point<float> b)
    {
        const float dist = a.getDistanceFrom (b);
        const float sag = juce::jmin (150.0f, 24.0f + dist * 0.28f);
        juce::Path p;
        p.startNewSubPath (a);
        p.cubicTo (a.x, a.y + sag, b.x, b.y + sag, b.x, b.y);
        return p;
    }

    //==========================================================================
    struct LedToggle : juce::Component
    {
        LedToggle (juce::String t, bool small_, juce::Colour c) : text (std::move (t)), small (small_), cat (c)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setWantsKeyboardFocus (false);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (current ? col::plate : col::knob);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (current ? col::fg : col::line);
            g.drawRoundedRectangle (b, 4.0f, 1.0f);

            const float d = 7.0f;
            juce::Rectangle<float> ledR;
            juce::Rectangle<float> textR;
            if (small)
            {
                ledR = { b.getX() + 6.0f, b.getCentreY() - d / 2, d, d };
                textR = b.withTrimmedLeft (14.0f);
            }
            else
            {
                ledR = { b.getX() + 8.0f, b.getCentreY() - d / 2, d, d };
                textR = b.withTrimmedLeft (19.0f);
            }
            if (on)
            {
                g.setColour (cat.withAlpha (0.35f));
                g.fillEllipse (ledR.expanded (2.5f));
                g.setColour (cat);
            }
            else g.setColour (col::track);
            g.fillEllipse (ledR);

            g.setColour (on ? col::fg : col::muted);
            g.setFont (small ? monoFont (10.0f) : uiFont (10.5f, true, 0.08f));
            g.drawText (small ? text : upper (text), textR, juce::Justification::centredLeft, false);

            if (dim)
            {
                g.setColour (col::panel.withAlpha (0.6f));
                g.fillRoundedRectangle (b, 4.0f);
            }
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            on = ! on;
            if (onChange) onChange (on);
            repaint();
        }

        juce::String text;
        bool small, on = false, current = false, dim = false;
        juce::Colour cat;
        std::function<void (bool)> onChange;
    };

    //==========================================================================
    struct EnvDisplay : juce::Component
    {
        explicit EnvDisplay (pw::Module& m) : mod (m) { setInterceptsMouseClicks (false, false); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (col::hole);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (col::line);
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

            const auto& ps = mod.spec.params;
            auto seg = [&] (int i) { return 0.06f + 0.24f * ps[(size_t) i].toNorm (mod.P (i)); };
            const float a = seg (0), d = seg (1), s = 0.18f, r = seg (3), S = mod.P (2);
            const float tot = a + d + s + r;
            const float pad = 6.0f, w = b.getWidth() - 2 * pad, h = b.getHeight() - 2 * pad;
            auto X = [&] (float f) { return pad + f / tot * w; };
            auto Y = [&] (float v) { return pad + (1.0f - v) * h; };

            juce::Path p;
            p.startNewSubPath (X (0), Y (0));
            p.lineTo (X (a), Y (1));
            for (int i = 1; i <= 16; ++i) { float f = (float) i / 16.0f; p.lineTo (X (a + d * f), Y (S + (1.0f - S) * std::exp (-4.0f * f))); }
            p.lineTo (X (a + d + s), Y (S));
            for (int i = 1; i <= 16; ++i) { float f = (float) i / 16.0f; p.lineTo (X (a + d + s + r * f), Y (S * std::exp (-4.0f * f))); }
            p.lineTo (X (tot), Y (0));
            juce::Path fill (p);
            fill.closeSubPath();
            const auto c = pw::catColour (pw::Cat::Mod);
            g.setColour (c.withAlpha (0.2f));
            g.fillPath (fill);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        pw::Module& mod;
    };

    //==========================================================================
    struct ScopeDisplay : juce::Component
    {
        explicit ScopeDisplay (pw::Module& m) : mod (m) { setInterceptsMouseClicks (false, false); }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (col::hole);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (col::line);
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
            g.drawHorizontalLine ((int) b.getCentreY(), b.getX() + 1, b.getRight() - 1);

            constexpr int N = pw::Module::scopeSize;
            float buf[N];
            const int w0 = mod.scopeWrite.load (std::memory_order_acquire);
            for (int i = 0; i < N; ++i) buf[i] = mod.scope[(w0 + i) % N];

            int start = 0;
            for (int i = 1; i < N / 2; ++i)
                if (buf[i - 1] < 0.0f && buf[i] >= 0.0f) { start = i; break; }

            const int n = 700;
            const float W = b.getWidth() - 8.0f, H = b.getHeight();
            juce::Path p;
            double rms = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float v = juce::jlimit (-1.2f, 1.2f, buf[start + i]);
                rms += (double) v * v;
                const float x = b.getX() + (float) i / (float) (n - 1) * W;
                const float y = b.getCentreY() - v * H * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col::accent);
            g.strokePath (p, juce::PathStrokeType (1.5f));

            const float level = juce::jmin (1.0f, (float) std::sqrt (rms / n) * 2.2f);
            g.setColour (col::accent.withAlpha (0.7f));
            g.fillRect (b.getRight() - 6.0f, b.getBottom() - level * H, 4.0f, level * H);
        }

        pw::Module& mod;
    };
}

//==============================================================================
// LookAndFeel

LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::textColourId, col::fg);
    setColour (juce::ComboBox::arrowColourId, col::muted);
    setColour (juce::ComboBox::focusedOutlineColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::textColourId, col::fg);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::plate);
    setColour (juce::PopupMenu::highlightedTextColourId, col::accent);
    setColour (juce::PopupMenu::headerTextColourId, col::muted);
    setColour (juce::Slider::textBoxTextColourId, col::fg);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, col::plate);
    setColour (juce::ScrollBar::trackColourId, col::chrome);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const float size = (float) juce::jmin (w, h) - 2.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float r = size * 0.4f;
    const float th = juce::jmax (2.0f, size * 0.075f);

    juce::Path trackP;
    trackP.addCentredArc (cx, cy, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (col::track);
    g.strokePath (trackP, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float ang = startAngle + pos * (endAngle - startAngle);
    const float zeroPos = (s.getMinimum() < 0.0 && s.getMaximum() > 0.0) ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    const float za = startAngle + zeroPos * (endAngle - startAngle);
    if (std::abs (ang - za) > 0.01f)
    {
        juce::Path v;
        v.addCentredArc (cx, cy, r, r, 0.0f, juce::jmin (za, ang), juce::jmax (za, ang), true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (v, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float br = r * 0.72f;
    g.setColour (s.isMouseOverOrDragging() ? col::plate : col::knob);
    g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
    g.setColour (col::line);
    g.drawEllipse (cx - br, cy - br, br * 2, br * 2, 1.0f);

    const float l0 = br * 0.1f, l1 = br * 0.78f;
    g.setColour (col::fg);
    g.drawLine (cx + std::sin (ang) * l0, cy - std::cos (ang) * l0, cx + std::sin (ang) * l1, cy - std::cos (ang) * l1, 2.0f);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (12.5f); }
juce::Font LookAndFeel::getPopupMenuFont() { return uiFont (13.5f); }

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (monoFont (9.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, col::fg);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setInterceptsMouseClicks (false, false);
    return l;
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (col::track);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (col::accent);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    g.setColour (col::fg);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

//==============================================================================
// ModuleComponent

ModuleComponent::ModuleComponent (RackComponent& r, std::shared_ptr<pw::Module> m)
    : mod (std::move (m)), layout (pw::computeLayout (mod->spec)), rack (r), cat (pw::catColour (mod->spec.cat))
{
    const auto& spec = mod->spec;
    setSize (spec.width, layout.height);
    setTopLeftPosition (mod->x, mod->y);
    closeRect = { spec.width - 28, 4, 22, 22 };

    sliders.resize (spec.params.size());
    combos.resize (spec.params.size());
    toggles.resize (spec.params.size());

    for (size_t i = 0; i < spec.params.size(); ++i)
    {
        const auto& p = spec.params[i];
        const auto cell = layout.params[i];
        const int idx = (int) i;

        if (p.kind == pw::PKind::Knob)
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            juce::NormalisableRange<double> range;
            if (p.log)
                range = juce::NormalisableRange<double> ((double) p.min, (double) p.max,
                    [] (double a, double b, double n) { return a * std::pow (b / a, n); },
                    [] (double a, double b, double v) { return std::log (v / a) / std::log (b / a); },
                    [] (double a, double b, double v) { return juce::jlimit (a, b, v); });
            else
                range = juce::NormalisableRange<double> ((double) p.min, (double) p.max, (double) p.step);
            s->setNormalisableRange (range);
            s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            auto f = p.fmt;
            s->textFromValueFunction = [f] (double v) { return f ? f ((float) v) : juce::String (v, 2); };
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, true, cell.getWidth(), 13);
            s->setMouseDragSensitivity (170);
            s->setDoubleClickReturnValue (true, (double) p.def);
            s->setColour (juce::Slider::rotarySliderFillColourId, cat);
            s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            s->setColour (juce::Slider::textBoxTextColourId, col::fg);
            s->setLookAndFeel (&rack.getLnf());
            s->setValue ((double) mod->P (idx), juce::dontSendNotification);
            s->updateText();
            s->setPopupMenuEnabled (false);
            s->setTitle (spec.title + " " + p.label);
            auto* raw = s.get();
            s->onValueChange = [this, idx, raw]
            {
                mod->setParam (idx, (float) raw->getValue());
                if (extra != nullptr && mod->spec.extra == pw::Extra::EnvAbove) extra->repaint();
            };
            s->setBounds (cell.getX(), cell.getY() + 12, cell.getWidth(), cell.getHeight() - 12);
            addAndMakeVisible (*s);
            sliders[i] = std::move (s);
        }
        else if (p.kind == pw::PKind::Choice)
        {
            auto c = std::make_unique<juce::ComboBox>();
            c->setLookAndFeel (&rack.getLnf());
            c->addItemList (p.choices, 1);
            c->setSelectedItemIndex ((int) mod->P (idx), juce::dontSendNotification);
            auto* raw = c.get();
            c->onChange = [this, idx, raw] { mod->setParam (idx, (float) raw->getSelectedItemIndex()); };
            c->setBounds (cell.withTrimmedLeft (48));
            c->setTitle (spec.title + " " + p.label);
            addAndMakeVisible (*c);
            combos[i] = std::move (c);
        }
        else
        {
            auto t = std::make_unique<LedToggle> (p.label, p.small, cat);
            t->on = mod->P (idx) > 0.5f;
            t->onChange = [this, idx] (bool on) { mod->setParam (idx, on ? 1.0f : 0.0f); };
            t->setBounds (p.grid ? cell : juce::Rectangle<int> (cell.getX(), cell.getY() + 22, cell.getWidth(), 22));
            addAndMakeVisible (*t);
            toggles[i] = std::move (t);
        }
    }

    switch (spec.extra)
    {
        case pw::Extra::KeysBelow:
        {
            auto kb = std::make_unique<juce::MidiKeyboardComponent> (rack.proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
            kb->setAvailableRange (24, 108);
            kb->setScrollButtonWidth (14);
            kb->setKeyWidth ((float) (layout.extra.getWidth() - 28) / 14.0f);
            kb->setLowestVisibleKey (48);
            kb->setColour (juce::MidiKeyboardComponent::whiteNoteColourId, col::keyW);
            kb->setColour (juce::MidiKeyboardComponent::blackNoteColourId, col::keyB);
            kb->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6c747e));
            kb->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::accent);
            kb->setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::accent.withAlpha (0.25f));
            kb->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
            kb->setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, col::plate);
            kb->setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, col::fg);
            extra = std::move (kb);
            break;
        }
        case pw::Extra::EnvAbove:   extra = std::make_unique<EnvDisplay> (*mod); break;
        case pw::Extra::ScopeAbove: extra = std::make_unique<ScopeDisplay> (*mod); break;
        case pw::Extra::None:       break;
    }
    if (extra != nullptr)
    {
        extra->setBounds (layout.extra);
        addAndMakeVisible (*extra);
    }
}

ModuleComponent::~ModuleComponent()
{
    for (auto& s : sliders) if (s) s->setLookAndFeel (nullptr);
    for (auto& c : combos)  if (c) c->setLookAndFeel (nullptr);
}

void ModuleComponent::paint (juce::Graphics& g)
{
    const auto& spec = mod->spec;
    auto b = getLocalBounds().toFloat();
    const float rad = 7.0f;

    juce::Path shape;
    shape.addRoundedRectangle (b.reduced (0.5f), rad);
    g.setGradientFill (juce::ColourGradient (col::panelHi, 0, 0, col::panel, 0, 70, false));
    g.fillPath (shape);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        g.setColour (cat);
        g.fillRect (b.withHeight (3.0f));
    }
    g.setColour (col::line);
    g.strokePath (shape, juce::PathStrokeType (1.0f));
    g.drawHorizontalLine (30, 1.0f, b.getRight() - 1.0f);

    // header
    g.setFont (uiFont (12.5f, true, 0.16f));
    g.setColour (cat);
    const auto title = upper (spec.title);
    const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title);
    g.drawText (title, 10, 6, tw + 4, 22, juce::Justification::centredLeft, false);
    g.setFont (uiFont (11.0f, false, 0.04f));
    g.setColour (col::muted);
    g.drawText (spec.sub, 10 + tw + 10, 6, juce::jmax (0, closeRect.getX() - (tw + 24) - 16), 22, juce::Justification::centredLeft, true);

    if (spec.type == "lfo")
    {
        const float v = mod->led.load();
        const auto c = pw::catColour (pw::Cat::Mod).withAlpha (0.15f + 0.85f * (v * 0.5f + 0.5f));
        g.setColour (c);
        g.fillEllipse ((float) closeRect.getX() - 12.0f, 13.0f, 9.0f, 9.0f);
    }

    g.setColour (closeRect.contains (getMouseXYRelative()) && isMouseOver() ? col::fg : col::muted);
    g.setFont (uiFont (17.0f));
    g.drawText (juce::String::fromUTF8 ("×"), closeRect, juce::Justification::centred, false);

    // labels
    for (size_t i = 0; i < spec.params.size(); ++i)
    {
        const auto& p = spec.params[i];
        const auto cell = layout.params[i];
        if (p.kind == pw::PKind::Knob && ! p.grid)
        {
            g.setFont (uiFont (10.0f, true, 0.1f));
            g.setColour (col::muted);
            g.drawText (upper (p.label), cell.getX() - 14, cell.getY(), cell.getWidth() + 28, 12, juce::Justification::centred, false);
        }
        else if (p.kind == pw::PKind::Choice)
        {
            g.setFont (uiFont (10.0f, true, 0.1f));
            g.setColour (col::muted);
            g.drawText (upper (p.label), cell.withWidth (46), juce::Justification::centredLeft, false);
        }
        else if (p.kind == pw::PKind::Knob && p.grid)
        {
            g.setFont (uiFont (9.5f, true));
            g.setColour (col::muted);
            g.drawText (p.label, cell.getX(), cell.getY(), cell.getWidth(), 11, juce::Justification::centred, false);
        }
    }

    if (layout.gridTop >= 0)
    {
        g.setColour (col::line);
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (10.0f, (float) layout.gridTop, b.getRight() - 10.0f, (float) layout.gridTop), dash, 2);
    }

    // jacks
    g.setColour (col::line);
    g.drawHorizontalLine (layout.jackRowTop, 1.0f, b.getRight() - 1.0f);
    if (! layout.outPlate.isEmpty())
    {
        g.setColour (col::plate);
        g.fillRoundedRectangle (layout.outPlate.toFloat(), 5.0f);
    }
    for (size_t i = 0; i < spec.ins.size(); ++i)
        drawJack (g, layout.inJacks[i], spec.ins[i].gate, { mod->uid, false, (int) i });
    for (size_t i = 0; i < spec.outs.size(); ++i)
        drawJack (g, layout.outJacks[i], spec.outs[i].gate, { mod->uid, true, (int) i });

    g.setFont (uiFont (9.0f, true, 0.06f));
    g.setColour (col::muted);
    for (size_t i = 0; i < spec.ins.size(); ++i)
        g.drawText (upper (spec.ins[i].label), layout.inJacks[i].x - 18, layout.inJacks[i].y + 11, 36, 12, juce::Justification::centred, false);
    for (size_t i = 0; i < spec.outs.size(); ++i)
        g.drawText (upper (spec.outs[i].label), layout.outJacks[i].x - 18, layout.outJacks[i].y + 11, 36, 12, juce::Justification::centred, false);
}

void ModuleComponent::drawJack (juce::Graphics& g, juce::Point<int> c, bool gate, const JackRef& ref)
{
    const auto cf = c.toFloat();
    const float r = 9.0f;
    auto circle = [&] (float rr) { return juce::Rectangle<float> (rr * 2, rr * 2).withCentre (cf); };

    const auto conn = rack.jackColour (ref);
    bool ok = false, no = false;
    if (rack.dragging)
    {
        if (ref == rack.anchor) {}
        else if (rack.isCompatible (rack.anchor, ref)) ok = true;
        else no = true;
    }

    if (ok)
    {
        g.setColour (col::accent.withAlpha (0.35f));
        g.fillEllipse (circle (r + 5));
    }
    g.setColour (gate ? col::gateRing : col::ring);
    g.fillEllipse (circle (r));
    g.setColour (col::ringIn);
    g.fillEllipse (circle (r * 0.62f));
    g.setColour (col::hole);
    g.fillEllipse (circle (r * 0.36f));

    if (ok)                         { g.setColour (col::fg); g.drawEllipse (circle (r + 1.5f), 2.0f); }
    else if (! conn.isTransparent()) { g.setColour (conn);   g.drawEllipse (circle (r + 1.5f), 2.0f); }
    else                            { g.setColour (col::line); g.drawEllipse (circle (r + 0.5f), 1.0f); }

    if (no)
    {
        g.setColour (col::panel.withAlpha (0.7f));
        g.fillEllipse (circle (r + 2));
    }
}

juce::Point<int> ModuleComponent::jackCentreLocal (bool out, int port) const
{
    const auto& v = out ? layout.outJacks : layout.inJacks;
    if (port < 0 || port >= (int) v.size()) return {};
    return v[(size_t) port];
}

JackRef ModuleComponent::jackAtLocal (juce::Point<int> p) const
{
    for (size_t i = 0; i < layout.inJacks.size(); ++i)
        if (layout.inJacks[i].getDistanceFrom (p) <= 12) return { mod->uid, false, (int) i };
    for (size_t i = 0; i < layout.outJacks.size(); ++i)
        if (layout.outJacks[i].getDistanceFrom (p) <= 12) return { mod->uid, true, (int) i };
    return {};
}

void ModuleComponent::mouseMove (const juce::MouseEvent& e)
{
    if (jackAtLocal (e.getPosition()).valid())     setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else if (closeRect.contains (e.getPosition())) setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else if (e.y < 30)                             setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    else                                           setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint (closeRect);
}

void ModuleComponent::mouseDown (const juce::MouseEvent& e)
{
    toFront (false);
    rack.cablesChanged();
    dragMode = 0;

    if (closeRect.contains (e.getPosition()))
    {
        rack.removeModuleLater (mod->uid);
        return;
    }
    const auto j = jackAtLocal (e.getPosition());
    if (j.valid())
    {
        dragMode = 2;
        rack.beginCableDrag (j, e.getEventRelativeTo (&rack).getPosition());
        return;
    }
    if (e.y < 30)
    {
        dragMode = 1;
        dragStart = { mod->x, mod->y };
    }
}

void ModuleComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == 1)
    {
        const auto off = e.getOffsetFromDragStart();
        mod->x = juce::jmax (0, dragStart.x + off.x);
        mod->y = juce::jmax (0, dragStart.y + off.y);
        setTopLeftPosition (mod->x, mod->y);
        rack.cablesChanged();
    }
    else if (dragMode == 2)
    {
        rack.dragCable (e.getEventRelativeTo (&rack).getPosition());
    }
}

void ModuleComponent::mouseUp (const juce::MouseEvent& e)
{
    if (dragMode == 1) rack.fitSize();
    else if (dragMode == 2) rack.endCableDrag (e.getEventRelativeTo (&rack).getPosition());
    dragMode = 0;
}

void ModuleComponent::tick()
{
    const auto& spec = mod->spec;
    if (spec.type == "lfo")
    {
        const float v = mod->led.load();
        if (std::abs (v - lastLed) > 0.02f) { lastLed = v; repaint (closeRect.getX() - 14, 10, 14, 14); }
    }
    else if (spec.type == "output")
    {
        if (extra != nullptr) extra->repaint();
    }
    else if (spec.type == "seq")
    {
        const int step = mod->curStep.load();
        const int len = (int) mod->P (3);
        if (step != lastStep || len != lastLen)
        {
            lastStep = step; lastLen = len;
            for (int k = 0; k < 8; ++k)
                if (auto* t = dynamic_cast<LedToggle*> (toggles[(size_t) (13 + k)].get()))
                {
                    t->current = (k == step);
                    t->dim = k >= len;
                    t->repaint();
                }
            for (int k = 0; k < 8; ++k)
                if (auto* s = sliders[(size_t) (5 + k)].get())
                    s->setAlpha (k >= len ? 0.35f : 1.0f);
        }
    }
}

//==============================================================================
// CableLayer

int CableLayer::cableAt (juce::Point<float> p) const
{
    for (int i = (int) hits.size() - 1; i >= 0; --i)
        if (hits[(size_t) i].contains (p)) return i;
    return -1;
}

bool CableLayer::hitTest (int x, int y)
{
    if (rack.dragging) return false;
    if (rack.jackAt ({ x, y }).valid()) return false;
    return cableAt ({ (float) x, (float) y }) >= 0;
}

void CableLayer::paint (juce::Graphics& g)
{
    const auto& cables = rack.cableCache;
    hits.clear();
    for (size_t i = 0; i < cables.size(); ++i)
    {
        const auto& c = cables[i];
        const auto a = rack.jackPos ({ c.srcUid, true, c.srcPort });
        const auto b = rack.jackPos ({ c.dstUid, false, c.dstPort });
        const auto path = cablePath (a, b);
        const bool hv = (int) i == hover;

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (path, juce::PathStrokeType (hv ? 10.0f : 8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.colour);
        g.strokePath (path, juce::PathStrokeType (hv ? 6.5f : 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.strokePath (path, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto pt : { a, b })
        {
            g.setColour (c.colour);
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (pt));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (pt), 1.0f);
        }

        juce::Path hit;
        juce::PathStrokeType (12.0f).createStrokedPath (hit, path);
        hits.push_back (hit);
    }

    if (rack.dragging)
    {
        const auto a = rack.jackPos (rack.anchor);
        const auto path = rack.anchor.out ? cablePath (a, rack.dragPos) : cablePath (rack.dragPos, a);
        juce::Path dashed;
        const float dashes[] = { 3.0f, 7.0f };
        juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createDashedStroke (dashed, path, dashes, 2);
        g.setColour (rack.dragColour);
        g.fillPath (dashed);
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (rack.dragPos));
    }
}

void CableLayer::mouseMove (const juce::MouseEvent& e)
{
    const int h = cableAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void CableLayer::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void CableLayer::mouseDown (const juce::MouseEvent& e)
{
    pressed = cableAt (e.position);
}

void CableLayer::mouseUp (const juce::MouseEvent& e)
{
    const int i = cableAt (e.position);
    if (i >= 0 && i == pressed && e.getDistanceFromDragStart() < 4)
    {
        rack.proc.engine.removeCable ((size_t) i);
        hover = -1;
        rack.sync();
    }
    pressed = -1;
}

//==============================================================================
// RackComponent

RackComponent::RackComponent (PatchwerkProcessor& p, LookAndFeel& l) : proc (p), lnf (l)
{
    cableLayer = std::make_unique<CableLayer> (*this);
    addAndMakeVisible (*cableLayer);
    cableLayer->setAlpha (proc.cableOpacity);
}

RackComponent::~RackComponent() = default;

ModuleComponent* RackComponent::find (int uid) const
{
    for (auto& c : comps) if (c->mod->uid == uid) return c.get();
    return nullptr;
}

void RackComponent::sync()
{
    knownVersion = proc.engine.version.load();
    const auto mods = proc.engine.getModules();

    comps.erase (std::remove_if (comps.begin(), comps.end(), [&] (const std::unique_ptr<ModuleComponent>& c)
    {
        for (auto& m : mods) if (m == c->mod) return false;
        return true;
    }), comps.end());

    for (auto& m : mods)
    {
        if (find (m->uid) != nullptr) continue;
        auto c = std::make_unique<ModuleComponent> (*this, m);
        addAndMakeVisible (*c);
        comps.push_back (std::move (c));
    }

    cableCache = proc.engine.getCables();
    cableLayer->toFront (false);
    fitSize();
    for (auto& c : comps) c->repaint();
    cableLayer->repaint();
}

void RackComponent::paint (juce::Graphics& g)
{
    g.fillAll (col::rack);
    const auto clip = g.getClipBounds();
    g.setColour (col::dot);
    const int step = 22;
    for (int y = (clip.getY() / step) * step; y < clip.getBottom(); y += step)
        for (int x = (clip.getX() / step) * step; x < clip.getRight(); x += step)
            g.fillRect ((float) x + 10.5f, (float) y + 10.5f, 1.6f, 1.6f);

    const juce::DropShadow shadow (juce::Colours::black.withAlpha (0.45f), 20, { 0, 8 });
    for (auto& c : comps)
        shadow.drawForRectangle (g, c->getBounds());
}

void RackComponent::fitSize (juce::Rectangle<int> viewArea)
{
    if (! viewArea.isEmpty()) lastViewArea = viewArea;
    int w = 0, h = 0;
    for (auto& c : comps) { w = juce::jmax (w, c->getRight()); h = juce::jmax (h, c->getBottom()); }
    setSize (juce::jmax (w + 320, lastViewArea.getWidth()), juce::jmax (h + 240, lastViewArea.getHeight()));
    cableLayer->setBounds (getLocalBounds());
    repaint();
}

void RackComponent::tick()
{
    for (auto& c : comps) c->tick();
}

void RackComponent::cablesChanged()
{
    cableLayer->toFront (false);
    cableLayer->repaint();
    repaint();
}

void RackComponent::setCableOpacity (float a)
{
    cableLayer->setAlpha (a);
}

void RackComponent::removeModuleLater (int uid)
{
    juce::Component::SafePointer<RackComponent> safe (this);
    juce::MessageManager::callAsync ([safe, uid]
    {
        if (safe == nullptr) return;
        safe->proc.engine.removeModule (uid);
        safe->sync();
    });
}

JackRef RackComponent::jackAt (juce::Point<int> p) const
{
    for (int i = getNumChildComponents() - 1; i >= 0; --i)
    {
        auto* c = dynamic_cast<ModuleComponent*> (getChildComponent (i));
        if (c == nullptr || ! c->getBounds().contains (p)) continue;
        return c->jackAtLocal (p - c->getPosition());
    }
    return {};
}

juce::Point<float> RackComponent::jackPos (const JackRef& j) const
{
    if (auto* c = find (j.uid))
        return (c->getPosition() + c->jackCentreLocal (j.out, j.port)).toFloat();
    return {};
}

bool RackComponent::isCompatible (const JackRef& a, const JackRef& b) const
{
    if (! a.valid() || ! b.valid() || a.out == b.out) return false;
    const auto& o = a.out ? a : b;
    const auto& i = a.out ? b : a;
    return proc.engine.canConnect (o.uid, o.port, i.uid, i.port);
}

juce::Colour RackComponent::jackColour (const JackRef& j) const
{
    juce::Colour c = juce::Colours::transparentBlack;
    for (auto& cb : cableCache)
        if ((j.out && cb.srcUid == j.uid && cb.srcPort == j.port) || (! j.out && cb.dstUid == j.uid && cb.dstPort == j.port))
            c = cb.colour;
    return c;
}

void RackComponent::beginCableDrag (const JackRef& j, juce::Point<int> pos)
{
    anchor = j;
    dragColour = proc.engine.peekColour();
    if (! j.out)
    {
        const auto cables = proc.engine.getCables();
        for (int i = (int) cables.size() - 1; i >= 0; --i)
        {
            const auto& c = cables[(size_t) i];
            if (c.dstUid == j.uid && c.dstPort == j.port)
            {
                anchor = { c.srcUid, true, c.srcPort };
                dragColour = c.colour;
                proc.engine.removeCable ((size_t) i);
                cableCache = proc.engine.getCables();
                knownVersion = proc.engine.version.load();
                break;
            }
        }
    }
    dragging = true;
    dragPos = pos.toFloat();
    for (auto& c : comps) c->repaint();
    cableLayer->repaint();
}

void RackComponent::dragCable (juce::Point<int> pos)
{
    dragPos = pos.toFloat();
    cableLayer->repaint();
}

void RackComponent::endCableDrag (juce::Point<int> pos)
{
    dragging = false;
    const auto t = jackAt (pos);
    if (t.valid() && isCompatible (anchor, t))
    {
        const auto& o = anchor.out ? anchor : t;
        const auto& i = anchor.out ? t : anchor;
        auto colour = dragColour;
        if (colour == proc.engine.peekColour()) colour = proc.engine.nextColour();
        proc.engine.addCable (o.uid, o.port, i.uid, i.port, colour);
    }
    sync();
}

//==============================================================================
// Editor

PatchwerkEditor::PatchwerkEditor (PatchwerkProcessor& p)
    : AudioProcessorEditor (&p), proc (p), rack (p, lnf)
{
    setLookAndFeel (&lnf);

    presetBox.addItemList (pw::Engine::presetNames(), 1);
    presetBox.setTextWhenNothingSelected (juce::String::fromUTF8 ("Preset laden…"));
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx < 0) return;
        proc.engine.loadPreset (idx);
        presetBox.setSelectedId (0, juce::dontSendNotification);
        viewport.setViewPosition (0, 0);
        rack.sync();
    };
    addAndMakeVisible (presetBox);

    int id = 1;
    for (auto& s : pw::allSpecs())
        addBox.addItem (s.title + juce::String::fromUTF8 (" · ") + s.sub, id++);
    addBox.setTextWhenNothingSelected (juce::String::fromUTF8 ("+ Hinzufügen…"));
    addBox.onChange = [this]
    {
        const int idx = addBox.getSelectedItemIndex();
        if (idx < 0) return;
        addBox.setSelectedId (0, juce::dontSendNotification);
        const auto& spec = pw::allSpecs()[(size_t) idx];
        const int off = ((int) proc.engine.getModules().size() % 6) * 24;
        const auto vp = viewport.getViewPosition();
        proc.engine.addModule (spec.type, vp.x + 40 + off, vp.y + 40 + off);
        rack.sync();
    };
    addAndMakeVisible (addBox);

    opacity.setSliderStyle (juce::Slider::LinearHorizontal);
    opacity.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    opacity.setRange (0.15, 1.0, 0.01);
    opacity.setValue (proc.cableOpacity, juce::dontSendNotification);
    opacity.onValueChange = [this]
    {
        proc.cableOpacity = (float) opacity.getValue();
        rack.setCableOpacity (proc.cableOpacity);
    };
    addAndMakeVisible (opacity);

    viewport.setViewedComponent (&rack, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    const int w0 = proc.editorWidth, h0 = proc.editorHeight;
    setResizable (true, true);
    setResizeLimits (1000, 560, 3200, 2200);
    setSize (juce::jlimit (1000, 3200, w0), juce::jlimit (560, 2200, h0));
    rack.sync();
    startTimerHz (30);
}

PatchwerkEditor::~PatchwerkEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PatchwerkEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto b = getLocalBounds();

    auto header = b.removeFromTop (46);
    g.setColour (col::chrome);
    g.fillRect (header);
    g.setColour (col::line);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    g.setFont (uiFont (18.0f, true, 0.2f));
    g.setColour (col::accent);
    g.drawText ("PATCHWERK", 16, 0, 140, 46, juce::Justification::centredLeft, false);
    g.setFont (uiFont (11.0f, false, 0.14f));
    g.setColour (col::muted);
    g.drawText ("MODULAR", 158, 2, 70, 46, juce::Justification::centredLeft, false);

    g.setFont (uiFont (11.0f, true, 0.1f));
    g.drawText ("PRESET", presetLabel, juce::Justification::centredRight, false);
    g.drawText ("MODUL", addLabel, juce::Justification::centredRight, false);
    g.drawText ("KABEL", opacityLabel, juce::Justification::centredRight, false);

    auto footer = b.removeFromBottom (26);
    g.setColour (col::chrome);
    g.fillRect (footer);
    g.setColour (col::line);
    g.drawHorizontalLine (footer.getY(), 0.0f, (float) getWidth());
    g.setFont (uiFont (11.5f));
    g.setColour (col::muted);
    g.drawText (juce::String::fromUTF8 ("Verbinden: von Buchse zu Buchse ziehen   ·   Umstecken: an belegtem Eingang ziehen   ·   "
                                        "Lösen: Kabel anklicken   ·   Regler: ziehen, Doppelklick = Standard   ·   "
                                        "Spielen: MIDI aus der DAW oder Bildschirmtastatur   ·   Goldener Ring = Gate"),
                footer.reduced (16, 0), juce::Justification::centredLeft, true);
}

void PatchwerkEditor::resized()
{
    auto b = getLocalBounds();
    auto header = b.removeFromTop (46).reduced (16, 0);
    b.removeFromBottom (26);

    header.removeFromLeft (230);
    presetLabel = header.removeFromLeft (50).withY (0).withHeight (46);
    header.removeFromLeft (8);
    presetBox.setBounds (header.removeFromLeft (190).withSizeKeepingCentre (190, 26));
    header.removeFromLeft (14);
    addLabel = header.removeFromLeft (48).withY (0).withHeight (46);
    header.removeFromLeft (8);
    addBox.setBounds (header.removeFromLeft (210).withSizeKeepingCentre (210, 26));
    header.removeFromLeft (14);
    opacityLabel = header.removeFromLeft (48).withY (0).withHeight (46);
    header.removeFromLeft (8);
    opacity.setBounds (header.removeFromLeft (110).withSizeKeepingCentre (110, 24));

    viewport.setBounds (b);
    rack.fitSize ({ 0, 0, viewport.getMaximumVisibleWidth(), viewport.getMaximumVisibleHeight() });

    proc.editorWidth = getWidth();
    proc.editorHeight = getHeight();
}

void PatchwerkEditor::timerCallback()
{
    if (proc.engine.version.load() != rack.knownVersion && ! rack.dragging)
        rack.sync();
    rack.tick();
}
