#include "Engine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace pw
{

//==============================================================================
// Formatting helpers

namespace fmt
{
    static juce::String sign (float v) { int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i); }
    static juce::String sec (float v)  { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; }
    static juce::String hz (float v)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 0 : 1) + " kHz";
        if (v < 10.0f)    return juce::String (v, 2) + " Hz";
        return juce::String (juce::roundToInt (v)) + " Hz";
    }
    static juce::String pct (float v)  { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    static juce::String bip (float v)  { int i = juce::roundToInt (v * 100.0f); return (i > 0 ? "+" : "") + juce::String (i) + "%"; }
    static juce::String ct (float v)   { int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " ct"; }
    static juce::String q (float v)    { return juce::String (v, 1); }
    static juce::String integer (float v) { return juce::String (juce::roundToInt (v)); }
    static juce::String bpm (float v)  { return juce::String (juce::roundToInt (v)) + " BPM"; }
    static juce::String note (float v)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","H" };
        int i = juce::roundToInt (v);
        return juce::String (names[((i % 12) + 12) % 12]) + juce::String (4 + (int) std::floor (i / 12.0));
    }
}

juce::Colour catColour (Cat c)
{
    switch (c)
    {
        case Cat::Ctl:  return juce::Colour (0xfff2b544);
        case Cat::Src:  return juce::Colour (0xff3cc8e8);
        case Cat::Mod:  return juce::Colour (0xffb48cf2);
        case Cat::Proc: return juce::Colour (0xfff08a3c);
        case Cat::Fx:   return juce::Colour (0xff5fd49a);
        case Cat::Out:  return juce::Colour (0xffe4e8ee);
    }
    return juce::Colours::white;
}

float ParamSpec::toNorm (float v) const
{
    if (max <= min) return 0.0f;
    if (log) return std::log (v / min) / std::log (max / min);
    return (v - min) / (max - min);
}

float ParamSpec::fromNorm (float n) const
{
    n = juce::jlimit (0.0f, 1.0f, n);
    float v = log ? min * std::pow (max / min, n) : min + n * (max - min);
    if (step > 0.0f) v = std::round (v / step) * step;
    return juce::jlimit (min, max, v);
}

int ModuleSpec::paramIndex (const juce::String& id) const { for (size_t i = 0; i < params.size(); ++i) if (params[i].id == id) return (int) i; return -1; }
int ModuleSpec::inIndex (const juce::String& id) const    { for (size_t i = 0; i < ins.size(); ++i)    if (ins[i].id == id)    return (int) i; return -1; }
int ModuleSpec::outIndex (const juce::String& id) const   { for (size_t i = 0; i < outs.size(); ++i)   if (outs[i].id == id)   return (int) i; return -1; }

//==============================================================================
// Spec table

static ParamSpec knob (const char* id, const juce::String& label, float mn, float mx, float def,
                       std::function<juce::String (float)> f, bool log = false, float step = 0.0f)
{
    ParamSpec p; p.id = id; p.label = label; p.kind = PKind::Knob;
    p.min = mn; p.max = mx; p.def = def; p.log = log; p.step = step; p.fmt = std::move (f);
    return p;
}
static ParamSpec choice (const char* id, const juce::String& label, juce::StringArray items, int def)
{
    ParamSpec p; p.id = id; p.label = label; p.kind = PKind::Choice;
    p.min = 0; p.max = (float) (items.size() - 1); p.def = (float) def; p.step = 1; p.choices = items;
    return p;
}
static ParamSpec toggle (const char* id, const juce::String& label, bool def)
{
    ParamSpec p; p.id = id; p.label = label; p.kind = PKind::Toggle;
    p.min = 0; p.max = 1; p.def = def ? 1.0f : 0.0f; p.step = 1;
    return p;
}

static const juce::StringArray waves { "Sinus", "Dreieck", juce::String::fromUTF8 ("Sägezahn"), "Rechteck" };

static std::vector<ModuleSpec> makeSpecs()
{
    std::vector<ModuleSpec> v;

    { ModuleSpec s; s.type = "keyboard"; s.title = "Tastatur"; s.sub = "MIDI > Pitch + Gate"; s.cat = Cat::Ctl; s.width = 360; s.extra = Extra::KeysBelow;
      s.params = { knob ("oct", "Oktave", -3, 3, 0, fmt::sign, false, 1), knob ("glide", "Glide", 0, 1, 0, fmt::sec) };
      s.outs = { { "pitch", "Pitch" }, { "gate", "Gate", true } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "seq"; s.title = "Sequenzer"; s.sub = "8 Schritte"; s.cat = Cat::Ctl; s.width = 400;
      s.params = { toggle ("run", "Lauf", true), knob ("bpm", "Tempo", 40, 240, 118, fmt::bpm, false, 1),
                   toggle ("host", "Host", false), knob ("len", juce::String::fromUTF8 ("Länge"), 1, 8, 8, fmt::integer, false, 1),
                   knob ("gl", "Gate", 0.05f, 0.95f, 0.45f, fmt::pct) };
      const int notes[] = { 0, 0, 12, 0, 3, 0, 7, 10 };
      const bool gates[] = { true, true, false, true, true, false, true, true };
      for (int i = 0; i < 8; ++i) { auto p = knob (("s" + juce::String (i)).toRawUTF8(), juce::String (i + 1), -12, 12, (float) notes[i], fmt::note, false, 1); p.small = true; p.grid = true; s.params.push_back (p); }
      for (int i = 0; i < 8; ++i) { auto p = toggle (("g" + juce::String (i)).toRawUTF8(), juce::String (i + 1), gates[i]); p.small = true; p.grid = true; s.params.push_back (p); }
      s.outs = { { "pitch", "Pitch" }, { "gate", "Gate", true } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "vco"; s.title = "VCO"; s.sub = "Oszillator"; s.cat = Cat::Src; s.width = 190;
      s.params = { choice ("wave", "Welle", waves, 2), knob ("oct", "Oktave", -3, 3, 0, fmt::sign, false, 1),
                   knob ("fine", "Fein", -100, 100, 0, fmt::ct), knob ("fm", "FM", 0, 1, 0, fmt::pct) };
      s.ins = { { "pitch", "1V/Okt" }, { "fm", "FM" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "noise"; s.title = "Rauschen"; s.sub = "Noise"; s.cat = Cat::Src; s.width = 140;
      s.params = { knob ("tone", "Farbe", 200, 18000, 9000, fmt::hz, true), knob ("lvl", "Pegel", 0, 1, 0.5f, fmt::pct) };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "lfo"; s.title = "LFO"; s.sub = "Modulation"; s.cat = Cat::Mod; s.width = 140;
      s.params = { choice ("wave", "Form", waves, 0), knob ("rate", "Rate", 0.05f, 30, 2, fmt::hz, true), knob ("depth", "Tiefe", 0, 1, 0.5f, fmt::pct) };
      s.ins = { { "rate", "Rate" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "adsr"; s.title = juce::String::fromUTF8 ("Hüllkurve"); s.sub = "ADSR"; s.cat = Cat::Mod; s.width = 248; s.extra = Extra::EnvAbove;
      s.params = { knob ("a", "Attack", 0.001f, 5, 0.01f, fmt::sec, true), knob ("d", "Decay", 0.001f, 5, 0.3f, fmt::sec, true),
                   knob ("s", "Sustain", 0, 1, 0.6f, fmt::pct), knob ("r", "Release", 0.001f, 8, 0.5f, fmt::sec, true) };
      s.ins = { { "gate", "Gate", true } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "vcf"; s.title = "Filter"; s.sub = "VCF"; s.cat = Cat::Proc; s.width = 190;
      s.params = { choice ("type", "Typ", { "Tiefpass", "Hochpass", "Bandpass", "Notch" }, 0),
                   knob ("cutoff", "Cutoff", 20, 18000, 1200, fmt::hz, true), knob ("reso", "Reso", 0.1f, 24, 2, fmt::q, true),
                   knob ("cv1", "CV 1", -1, 1, 0.5f, fmt::bip), knob ("cv2", "CV 2", -1, 1, 0, fmt::bip) };
      s.ins = { { "in", "In" }, { "cv1", "CV 1" }, { "cv2", "CV 2" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "vca"; s.title = "VCA"; s.sub = juce::String::fromUTF8 ("Verstärker"); s.cat = Cat::Proc; s.width = 140;
      s.params = { knob ("gain", "Grund", 0, 1, 0, fmt::pct), knob ("cv", "CV", 0, 1, 1, fmt::pct) };
      s.ins = { { "in", "In" }, { "cv", "CV" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "mixer"; s.title = "Mixer"; s.sub = juce::String::fromUTF8 ("4 Kanäle"); s.cat = Cat::Proc; s.width = 212;
      s.params = { knob ("l1", "Kanal 1", 0, 1, 0.8f, fmt::pct), knob ("l2", "Kanal 2", 0, 1, 0.8f, fmt::pct),
                   knob ("l3", "Kanal 3", 0, 1, 0.8f, fmt::pct), knob ("l4", "Kanal 4", 0, 1, 0.8f, fmt::pct),
                   knob ("main", "Summe", 0, 1, 0.8f, fmt::pct) };
      s.ins = { { "in1", "1" }, { "in2", "2" }, { "in3", "3" }, { "in4", "4" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "delay"; s.title = "Delay"; s.sub = "Echo"; s.cat = Cat::Fx; s.width = 190;
      s.params = { knob ("time", "Zeit", 0.01f, 2, 0.33f, fmt::sec, true), knob ("fb", "Feedback", 0, 0.95f, 0.4f, fmt::pct),
                   knob ("tone", "Ton", 400, 14000, 3500, fmt::hz, true), knob ("mix", "Mix", 0, 1, 0.3f, fmt::pct) };
      s.ins = { { "in", "In" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "reverb"; s.title = "Hall"; s.sub = "Reverb"; s.cat = Cat::Fx; s.width = 140;
      s.params = { knob ("size", juce::String::fromUTF8 ("Größe"), 0.3f, 8, 2.5f, fmt::sec, true), knob ("mix", "Mix", 0, 1, 0.3f, fmt::pct) };
      s.ins = { { "in", "In" } };
      s.outs = { { "out", "Out" } };
      v.push_back (s); }

    { ModuleSpec s; s.type = "output"; s.title = "Ausgang"; s.sub = "Stereo"; s.cat = Cat::Out; s.width = 232; s.extra = Extra::ScopeAbove;
      s.params = { knob ("vol", juce::String::fromUTF8 ("Lautstärke"), 0, 1, 0.7f, fmt::pct) };
      s.ins = { { "l", "L/Mono" }, { "r", "R" } };
      v.push_back (s); }

    return v;
}

const std::vector<ModuleSpec>& allSpecs()
{
    static const std::vector<ModuleSpec> specs = makeSpecs();
    return specs;
}

const ModuleSpec* findSpec (const juce::String& type)
{
    for (auto& s : allSpecs()) if (s.type == type) return &s;
    return nullptr;
}

//==============================================================================
// Layout

ModuleLayout computeLayout (const ModuleSpec& s)
{
    ModuleLayout L;
    const int w = s.width, pad = 10, header = 30;
    int y = header + pad;
    L.params.resize (s.params.size());

    auto extraHeight = [&] { return s.extra == Extra::EnvAbove ? 46 : s.extra == Extra::ScopeAbove ? 70 : 72; };

    if (s.extra == Extra::EnvAbove || s.extra == Extra::ScopeAbove)
    {
        L.extra = { pad, y, w - 2 * pad, extraHeight() };
        y += extraHeight() + pad;
    }

    int x = pad, rowH = 0;
    auto newline = [&] { if (rowH > 0) { y += rowH + pad; } x = pad; rowH = 0; };

    for (size_t i = 0; i < s.params.size(); ++i)
    {
        auto& p = s.params[i];
        if (p.grid) continue;
        if (p.kind == PKind::Choice)
        {
            newline();
            L.params[i] = { pad, y, w - 2 * pad, 24 };
            y += 24 + pad;
            continue;
        }
        const int cw = p.kind == PKind::Toggle ? 56 : 52, ch = 66;
        if (x + cw > w - pad) newline();
        L.params[i] = { x, y, cw, ch };
        x += cw + 6;
        rowH = juce::jmax (rowH, ch);
    }
    newline();

    if (s.extra == Extra::KeysBelow)
    {
        L.extra = { pad, y, w - 2 * pad, 72 };
        y += 72 + pad;
    }

    // sequencer step grid
    bool hasGrid = false;
    for (auto& p : s.params) hasGrid |= p.grid;
    if (hasGrid)
    {
        L.gridTop = y;
        y += 8;
        const float colW = (float) (w - 2 * pad) / 8.0f;
        int k = 0, t = 0;
        for (size_t i = 0; i < s.params.size(); ++i)
        {
            auto& p = s.params[i];
            if (! p.grid) continue;
            if (p.kind == PKind::Knob)
            {
                L.params[i] = { pad + juce::roundToInt (colW * (float) k + (colW - 38) / 2), y, 38, 54 };
                ++k;
            }
            else
            {
                L.params[i] = { pad + juce::roundToInt (colW * (float) t + (colW - 38) / 2), y + 60, 38, 22 };
                ++t;
            }
        }
        y += 60 + 22 + pad;
    }

    // jack row
    L.jackRowTop = y;
    const int jackY = y + 8 + 4 + 9;
    for (size_t i = 0; i < s.ins.size(); ++i)
        L.inJacks.push_back ({ 8 + 3 + 17 + (int) i * 36, jackY });
    if (! s.outs.empty())
    {
        const int plateW = (int) s.outs.size() * 36 + 6;
        L.outPlate = { w - 8 - plateW, y + 8, plateW, 41 };
        for (size_t i = 0; i < s.outs.size(); ++i)
            L.outJacks.push_back ({ L.outPlate.getX() + 3 + 18 + (int) i * 36, jackY });
    }
    L.height = y + 8 + 41 + 8;
    return L;
}

//==============================================================================
// DSP modules

Module::Module (const ModuleSpec& s, int id) : spec (s), uid (id)
{
    for (int i = 0; i < kMaxParams; ++i)
        p[i].store (i < (int) s.params.size() ? s.params[(size_t) i].def : 0.0f);
}

static inline float polyBlep (double t, double dt)
{
    dt = std::abs (dt);
    if (dt <= 0.0) return 0.0f;
    if (t < dt)        { t /= dt;               return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt)  { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

static inline float onePoleCoef (double seconds, double sr)
{
    if (seconds <= 0.0) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
}

struct VCO : Module
{
    using Module::Module;
    double phase = 0.0;
    void process() noexcept override
    {
        const int wave = (int) P (0);
        double f = 261.6256 * std::exp2 ((double) P (1) + (double) in[0] + (double) P (2) / 1200.0)
                 + (double) in[1] * (double) P (3) * 800.0;
        f = juce::jlimit (-sr * 0.45, sr * 0.45, f);
        const double dt = f / sr;
        phase += dt;
        phase -= std::floor (phase);
        const double t = phase;
        float v = 0.0f;
        switch (wave)
        {
            case 0:  v = (float) std::sin (juce::MathConstants<double>::twoPi * t); break;
            case 1:  v = (float) (4.0 * std::abs (t - 0.5) - 1.0); break;
            case 2:  v = (float) (2.0 * t - 1.0) - polyBlep (t, dt); break;
            default:
            {
                double t2 = t + 0.5; t2 -= std::floor (t2);
                v = (t < 0.5 ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt);
            }
        }
        out[0] = v * 0.5f;
    }
};

struct Noise : Module
{
    using Module::Module;
    juce::Random rng;
    float y = 0.0f;
    void process() noexcept override
    {
        const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * P (0) / (float) sr);
        y += a * ((rng.nextFloat() * 2.0f - 1.0f) - y);
        out[0] = y * P (1);
    }
};

struct LFO : Module
{
    using Module::Module;
    double phase = 0.0;
    int ledCounter = 0;
    void process() noexcept override
    {
        const double rate = (double) P (1) * std::exp2 ((double) in[0] * 3.0);
        phase += rate / sr;
        phase -= std::floor (phase);
        float v = 0.0f;
        switch ((int) P (0))
        {
            case 0:  v = (float) std::sin (juce::MathConstants<double>::twoPi * phase); break;
            case 1:  v = (float) (1.0 - 4.0 * std::abs (phase - 0.5)); break;
            case 2:  v = (float) (2.0 * phase - 1.0); break;
            default: v = phase < 0.5 ? 1.0f : -1.0f;
        }
        out[0] = v * P (2);
        if (++ledCounter >= 256) { ledCounter = 0; led.store (v); }
    }
};

struct ADSR : Module
{
    using Module::Module;
    bool gate = false;
    int stage = 0;      // 0 idle, 1 attack, 2 decay/sustain, 3 release
    float v = 0.0f;
    void process() noexcept override
    {
        const bool g = in[0] > 0.5f;
        if (g && ! gate) stage = 1;
        else if (! g && gate) stage = 3;
        gate = g;

        switch (stage)
        {
            case 1:
                v += 1.0f / (float) (juce::jmax (0.0005f, P (0)) * sr);
                if (v >= 1.0f) { v = 1.0f; stage = 2; }
                break;
            case 2: v += (P (2) - v) * onePoleCoef (P (1) / 4.0, sr); break;
            case 3: v += (0.0f - v) * onePoleCoef (P (3) / 4.0, sr); if (v < 1.0e-5f) { v = 0.0f; stage = 0; } break;
            default: break;
        }
        out[0] = v;
    }
};

struct VCF : Module
{
    using Module::Module;
    float ic1 = 0.0f, ic2 = 0.0f;
    void process() noexcept override
    {
        float fc = P (1) * std::exp2 ((in[1] * P (3) + in[2] * P (4)) * 5.0f);
        fc = juce::jlimit (10.0f, (float) sr * 0.45f, fc);
        const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        const float k = 1.0f / juce::jmax (0.05f, P (2));
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float x = in[0];
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (! std::isfinite (ic1) || ! std::isfinite (ic2)) { ic1 = ic2 = 0.0f; }
        switch ((int) P (0))
        {
            case 0:  out[0] = v2; break;
            case 1:  out[0] = x - k * v1 - v2; break;
            case 2:  out[0] = v1; break;
            default: out[0] = x - k * v1; break;
        }
    }
};

struct VCA : Module
{
    using Module::Module;
    float g = 0.0f, c = 1.0f;
    void process() noexcept override
    {
        const float a = onePoleCoef (0.005, sr);
        g += (P (0) - g) * a;
        c += (P (1) - c) * a;
        out[0] = in[0] * juce::jlimit (-2.0f, 2.0f, g + in[1] * c);
    }
};

struct Mixer : Module
{
    using Module::Module;
    void process() noexcept override
    {
        out[0] = (in[0] * P (0) + in[1] * P (1) + in[2] * P (2) + in[3] * P (3)) * P (4);
    }
};

struct Delay : Module
{
    using Module::Module;
    std::vector<float> buf;
    int w = 0;
    float time = 0.33f, lp = 0.0f;
    void prepare (double s) override
    {
        Module::prepare (s);
        buf.assign ((size_t) (s * 2.5) + 8, 0.0f);
        w = 0;
        time = P (0);
    }
    void process() noexcept override
    {
        if (buf.empty()) { out[0] = in[0]; return; }
        const int n = (int) buf.size();
        time += (P (0) - time) * onePoleCoef (0.05, sr);
        float rd = (float) w - time * (float) sr;
        while (rd < 0.0f) rd += (float) n;
        const int i0 = (int) rd;
        const float fr = rd - (float) i0;
        const float d = buf[(size_t) (i0 % n)] * (1.0f - fr) + buf[(size_t) ((i0 + 1) % n)] * fr;
        lp += (d - lp) * (1.0f - std::exp (-juce::MathConstants<float>::twoPi * P (2) / (float) sr));
        float fbv = in[0] + lp * P (1);
        if (! std::isfinite (fbv)) { fbv = 0.0f; lp = 0.0f; }
        buf[(size_t) w] = fbv;
        if (++w >= n) w = 0;
        out[0] = in[0] + lp * P (3);
    }
};

struct Reverb : Module
{
    using Module::Module;
    juce::Reverb rv;
    float lastSize = -1.0f, lastMix = -1.0f;
    void prepare (double s) override { Module::prepare (s); rv.setSampleRate (s); rv.reset(); lastSize = -1.0f; }
    void process() noexcept override
    {
        if (P (0) != lastSize || P (1) != lastMix)
        {
            lastSize = P (0); lastMix = P (1);
            juce::Reverb::Parameters par;
            par.roomSize = juce::jlimit (0.2f, 0.985f, 0.2f + 0.8f * std::log (lastSize / 0.3f) / std::log (8.0f / 0.3f));
            par.damping = 0.45f;
            par.wetLevel = lastMix * 0.5f;
            par.dryLevel = 0.5f;
            par.width = 1.0f;
            rv.setParameters (par);
        }
        float s = in[0];
        rv.processMono (&s, 1);
        out[0] = s;
    }
};

struct Output : Module
{
    using Module::Module;
    float vol = 0.7f;
    void process() noexcept override
    {
        vol += (P (0) - vol) * onePoleCoef (0.01, sr);
        const float l = in[0];
        const float r = inConn[1] ? in[1] : in[0];
        out[0] = l * vol;
        out[1] = r * vol;
        int wi = scopeWrite.load (std::memory_order_relaxed);
        scope[wi] = 0.5f * (out[0] + out[1]);
        scopeWrite.store ((wi + 1) % scopeSize, std::memory_order_release);
    }
};

struct Keyboard : Module
{
    Keyboard (const ModuleSpec& s, int id) : Module (s, id) { held.reserve (128); }
    std::vector<int> held;
    float target = 0.0f, cur = 0.0f, bend = 0.0f;
    bool gate = false;
    int retrig = 0;

    void noteOn (int n) noexcept override
    {
        held.erase (std::remove (held.begin(), held.end(), n), held.end());
        held.push_back (n);
        target = (float) (n - 60) / 12.0f;
        if (gate) retrig = 2;   // short low pulse so envelopes retrigger
        gate = true;
    }
    void noteOff (int n) noexcept override
    {
        held.erase (std::remove (held.begin(), held.end(), n), held.end());
        if (held.empty()) gate = false;
        else target = (float) (held.back() - 60) / 12.0f;
    }
    void allNotesOff() noexcept override { held.clear(); gate = false; }
    void pitchBend (float semis) noexcept override { bend = semis / 12.0f; }

    void process() noexcept override
    {
        const float gl = P (1);
        if (gl > 0.001f) cur += (target - cur) * onePoleCoef (gl / 3.0, sr);
        else cur = target;
        out[0] = cur + P (0) + bend;
        out[1] = (gate && retrig == 0) ? 1.0f : 0.0f;
        if (retrig > 0) --retrig;
    }
};

struct Sequencer : Module
{
    using Module::Module;
    double toNext = 0.0;
    int step = 0, gateLeft = 0;
    void process() noexcept override
    {
        if (P (0) < 0.5f)
        {
            out[1] = 0.0f; step = 0; toNext = 0.0; gateLeft = 0;
            curStep.store (-1, std::memory_order_relaxed);
            return;
        }
        double bpm = P (1);
        if (P (2) > 0.5f && hostBpm != nullptr)
        {
            const double hb = hostBpm->load (std::memory_order_relaxed);
            if (hb > 1.0) bpm = hb;
        }
        if (toNext <= 0.0)
        {
            const int len = juce::jlimit (1, 8, (int) P (3));
            if (step >= len) step = 0;
            const double dur = 60.0 / bpm / 4.0 * sr;
            out[0] = P (5 + step) / 12.0f;
            gateLeft = P (13 + step) > 0.5f ? (int) (dur * P (4)) : 0;
            curStep.store (step, std::memory_order_relaxed);
            step = (step + 1) % len;
            toNext += dur;
        }
        toNext -= 1.0;
        out[1] = gateLeft > 0 ? 1.0f : 0.0f;
        if (gateLeft > 0) --gateLeft;
    }
};

static std::shared_ptr<Module> createModule (const ModuleSpec& s, int uid)
{
    const auto& t = s.type;
    if (t == "vco")      return std::make_shared<VCO> (s, uid);
    if (t == "noise")    return std::make_shared<Noise> (s, uid);
    if (t == "lfo")      return std::make_shared<LFO> (s, uid);
    if (t == "adsr")     return std::make_shared<ADSR> (s, uid);
    if (t == "vcf")      return std::make_shared<VCF> (s, uid);
    if (t == "vca")      return std::make_shared<VCA> (s, uid);
    if (t == "mixer")    return std::make_shared<Mixer> (s, uid);
    if (t == "delay")    return std::make_shared<Delay> (s, uid);
    if (t == "reverb")   return std::make_shared<Reverb> (s, uid);
    if (t == "output")   return std::make_shared<Output> (s, uid);
    if (t == "keyboard") return std::make_shared<Keyboard> (s, uid);
    if (t == "seq")      return std::make_shared<Sequencer> (s, uid);
    return nullptr;
}

//==============================================================================
// Engine

Engine::Engine() {}

juce::Colour Engine::cableColour (int i)
{
    static const juce::uint32 cols[] = { 0xfff2b544, 0xff3cc8e8, 0xffe8607a, 0xff5fd49a, 0xffb48cf2, 0xfff08a3c, 0xffe4e8ee };
    return juce::Colour (cols[((i % 7) + 7) % 7]);
}
juce::Colour Engine::nextColour() { return cableColour (colourIndex++); }
juce::Colour Engine::peekColour() const { return cableColour (colourIndex); }

void Engine::prepare (double sr)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    sampleRate = sr;
    for (auto& m : modules) m->prepare (sr);
    limGain = 1.0f;
}

Module* Engine::findModule (int uid) const
{
    for (auto& m : modules) if (m->uid == uid) return m.get();
    return nullptr;
}

void Engine::rebuildCaches()
{
    raw.clear(); keyboards.clear(); outputs.clear();
    for (auto& m : modules)
    {
        raw.push_back (m.get());
        if (m->spec.type == "keyboard") keyboards.push_back (m.get());
        if (m->spec.type == "output")   outputs.push_back (m.get());
        std::fill (std::begin (m->inConn), std::end (m->inConn), false);
    }
    for (auto& c : cables)
    {
        c.src = findModule (c.srcUid);
        c.dst = findModule (c.dstUid);
        if (c.dst != nullptr) c.dst->inConn[c.dstPort] = true;
    }
    cables.erase (std::remove_if (cables.begin(), cables.end(), [] (const Cable& c) { return c.src == nullptr || c.dst == nullptr; }), cables.end());
}

std::shared_ptr<Module> Engine::addModule (const juce::String& type, int x, int y, int uid)
{
    auto* spec = findSpec (type);
    if (spec == nullptr) return nullptr;
    if (uid <= 0) uid = nextUid;
    nextUid = juce::jmax (nextUid, uid + 1);
    auto m = createModule (*spec, uid);
    m->x = x; m->y = y;
    m->hostBpm = &hostBpm;
    m->prepare (sampleRate);
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        modules.push_back (m);
        rebuildCaches();
    }
    ++version;
    return m;
}

void Engine::removeModule (int uid)
{
    std::shared_ptr<Module> keep;   // destroyed outside the lock
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        cables.erase (std::remove_if (cables.begin(), cables.end(), [uid] (const Cable& c) { return c.srcUid == uid || c.dstUid == uid; }), cables.end());
        for (auto it = modules.begin(); it != modules.end(); ++it)
            if ((*it)->uid == uid) { keep = *it; modules.erase (it); break; }
        rebuildCaches();
    }
    ++version;
}

bool Engine::canConnect (int srcUid, int srcPort, int dstUid, int dstPort) const
{
    auto* s = findModule (srcUid);
    auto* d = findModule (dstUid);
    if (s == nullptr || d == nullptr) return false;
    if (srcPort < 0 || srcPort >= (int) s->spec.outs.size()) return false;
    if (dstPort < 0 || dstPort >= (int) d->spec.ins.size()) return false;
    for (auto& c : cables)
        if (c.srcUid == srcUid && c.srcPort == srcPort && c.dstUid == dstUid && c.dstPort == dstPort) return false;
    return true;
}

bool Engine::addCable (int srcUid, int srcPort, int dstUid, int dstPort, juce::Colour col)
{
    if (! canConnect (srcUid, srcPort, dstUid, dstPort)) return false;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        cables.push_back ({ srcUid, srcPort, dstUid, dstPort, col });
        rebuildCaches();
    }
    ++version;
    return true;
}

void Engine::removeCable (size_t index)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        if (index >= cables.size()) return;
        cables.erase (cables.begin() + (std::ptrdiff_t) index);
        rebuildCaches();
    }
    ++version;
}

void Engine::clear()
{
    std::vector<std::shared_ptr<Module>> keep;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        cables.clear();
        keep.swap (modules);
        rebuildCaches();
    }
    nextUid = 1;
    colourIndex = 0;
    ++version;
}

void Engine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept
{
    const int n = buffer.getNumSamples();
    const juce::SpinLock::ScopedTryLockType tl (lock);
    if (! tl.isLocked()) { buffer.clear(); return; }

    float* L = buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr;
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (L == nullptr) return;

    const float rel = onePoleCoef (0.15, sampleRate);
    auto it = midi.cbegin();

    for (int s = 0; s < n; ++s)
    {
        for (; it != midi.cend() && (*it).samplePosition <= s; ++it)
        {
            const auto msg = (*it).getMessage();
            if (msg.isNoteOn())
                for (auto* k : keyboards) k->noteOn (msg.getNoteNumber());
            else if (msg.isNoteOff())
                for (auto* k : keyboards) k->noteOff (msg.getNoteNumber());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                for (auto* k : keyboards) k->allNotesOff();
            else if (msg.isPitchWheel())
                for (auto* k : keyboards) k->pitchBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
        }

        for (auto* m : raw) std::fill (m->in, m->in + kMaxPorts, 0.0f);
        for (auto& c : cables) c.dst->in[c.dstPort] += c.src->out[c.srcPort];
        for (auto* m : raw) m->process();

        float l = 0.0f, r = 0.0f;
        for (auto* o : outputs) { l += o->out[0]; r += o->out[1]; }
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;

        // safety peak limiter
        const float peak = juce::jmax (std::abs (l), std::abs (r));
        if (peak * limGain > 0.97f) limGain = 0.97f / peak;
        else limGain += (1.0f - limGain) * rel;

        L[s] = l * limGain;
        if (R != nullptr) R[s] = r * limGain;
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

//==============================================================================
// State

juce::ValueTree Engine::toState() const
{
    juce::ValueTree v ("PATCH");
    v.setProperty ("colourIndex", colourIndex, nullptr);
    for (auto& m : modules)
    {
        juce::ValueTree mt ("M");
        mt.setProperty ("uid", m->uid, nullptr);
        mt.setProperty ("type", m->spec.type, nullptr);
        mt.setProperty ("x", m->x, nullptr);
        mt.setProperty ("y", m->y, nullptr);
        for (size_t i = 0; i < m->spec.params.size(); ++i)
            mt.setProperty (juce::Identifier ("p_" + m->spec.params[i].id), m->P ((int) i), nullptr);
        v.appendChild (mt, nullptr);
    }
    for (auto& c : cables)
    {
        juce::ValueTree ct ("C");
        auto* s = findModule (c.srcUid);
        auto* d = findModule (c.dstUid);
        if (s == nullptr || d == nullptr) continue;
        ct.setProperty ("su", c.srcUid, nullptr);
        ct.setProperty ("sp", s->spec.outs[(size_t) c.srcPort].id, nullptr);
        ct.setProperty ("du", c.dstUid, nullptr);
        ct.setProperty ("dp", d->spec.ins[(size_t) c.dstPort].id, nullptr);
        ct.setProperty ("col", c.colour.toString(), nullptr);
        v.appendChild (ct, nullptr);
    }
    return v;
}

void Engine::fromState (const juce::ValueTree& v)
{
    if (! v.hasType ("PATCH")) return;
    clear();
    for (auto mt : v)
    {
        if (! mt.hasType ("M")) continue;
        auto m = addModule (mt["type"].toString(), (int) mt["x"], (int) mt["y"], (int) mt["uid"]);
        if (m == nullptr) continue;
        for (size_t i = 0; i < m->spec.params.size(); ++i)
        {
            juce::Identifier id ("p_" + m->spec.params[i].id);
            if (mt.hasProperty (id)) m->setParam ((int) i, (float) mt[id]);
        }
    }
    for (auto ct : v)
    {
        if (! ct.hasType ("C")) continue;
        auto* s = findModule ((int) ct["su"]);
        auto* d = findModule ((int) ct["du"]);
        if (s == nullptr || d == nullptr) continue;
        addCable (s->uid, s->spec.outIndex (ct["sp"].toString()), d->uid, d->spec.inIndex (ct["dp"].toString()),
                  juce::Colour::fromString (ct["col"].toString()));
    }
    colourIndex = (int) v.getProperty ("colourIndex", (int) cables.size());
}

//==============================================================================
// Presets

struct PresetModule { const char* name; const char* type; std::vector<std::pair<const char*, float>> params; };
struct Preset
{
    const char* title;
    std::vector<std::vector<PresetModule>> rows;
    std::vector<std::pair<const char*, const char*>> cables;
};

static const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p = {
        { "Klassische Stimme",
          { { { "kb", "keyboard", { { "oct", -1 }, { "glide", 0.04f } } },
              { "lfo", "lfo", { { "wave", 1 }, { "rate", 0.25f }, { "depth", 0.6f } } },
              { "envF", "adsr", { { "a", 0.004f }, { "d", 0.45f }, { "s", 0.2f }, { "r", 0.5f } } },
              { "envA", "adsr", { { "a", 0.008f }, { "d", 0.3f }, { "s", 0.7f }, { "r", 0.6f } } },
              { "out", "output", { { "vol", 0.7f } } } },
            { { "vco1", "vco", { { "wave", 2 }, { "oct", 0 } } },
              { "vco2", "vco", { { "wave", 3 }, { "oct", -1 }, { "fine", 7 } } },
              { "mix", "mixer", { { "l1", 0.8f }, { "l2", 0.55f }, { "l3", 0.5f }, { "l4", 0.5f } } },
              { "vcf", "vcf", { { "cutoff", 520 }, { "reso", 5 }, { "cv1", 0.5f }, { "cv2", 0.12f } } },
              { "vca", "vca", { { "gain", 0 } } },
              { "dly", "delay", { { "time", 0.33f }, { "fb", 0.35f }, { "mix", 0.25f } } },
              { "rev", "reverb", { { "size", 2.6f }, { "mix", 0.3f } } } } },
          { { "kb.pitch", "vco1.pitch" }, { "kb.pitch", "vco2.pitch" }, { "kb.gate", "envF.gate" }, { "kb.gate", "envA.gate" },
            { "vco1.out", "mix.in1" }, { "vco2.out", "mix.in2" }, { "mix.out", "vcf.in" }, { "envF.out", "vcf.cv1" },
            { "lfo.out", "vcf.cv2" }, { "vcf.out", "vca.in" }, { "envA.out", "vca.cv" }, { "vca.out", "dly.in" },
            { "dly.out", "rev.in" }, { "rev.out", "out.l" } } },

        { "Sequenzer-Groove",
          { { { "seq", "seq", { { "bpm", 122 } } },
              { "lfo", "lfo", { { "wave", 0 }, { "rate", 0.12f }, { "depth", 0.8f } } },
              { "envF", "adsr", { { "a", 0.002f }, { "d", 0.18f }, { "s", 0.05f }, { "r", 0.15f } } },
              { "envA", "adsr", { { "a", 0.002f }, { "d", 0.25f }, { "s", 0.4f }, { "r", 0.12f } } },
              { "out", "output", { { "vol", 0.7f } } } },
            { { "vco1", "vco", { { "wave", 2 }, { "oct", -2 } } },
              { "vco2", "vco", { { "wave", 2 }, { "oct", -2 }, { "fine", -9 } } },
              { "mix", "mixer", { { "l1", 0.7f }, { "l2", 0.7f } } },
              { "vcf", "vcf", { { "cutoff", 260 }, { "reso", 9 }, { "cv1", 0.55f }, { "cv2", 0.18f } } },
              { "vca", "vca", { { "gain", 0 } } },
              { "dly", "delay", { { "time", 0.366f }, { "fb", 0.45f }, { "tone", 2400 }, { "mix", 0.28f } } },
              { "rev", "reverb", { { "size", 1.8f }, { "mix", 0.22f } } } } },
          { { "seq.pitch", "vco1.pitch" }, { "seq.pitch", "vco2.pitch" }, { "seq.gate", "envF.gate" }, { "seq.gate", "envA.gate" },
            { "vco1.out", "mix.in1" }, { "vco2.out", "mix.in2" }, { "mix.out", "vcf.in" }, { "envF.out", "vcf.cv1" },
            { "lfo.out", "vcf.cv2" }, { "vcf.out", "vca.in" }, { "envA.out", "vca.cv" }, { "vca.out", "dly.in" },
            { "dly.out", "rev.in" }, { "rev.out", "out.l" } } },

        { "Rausch-Drone",
          { { { "lfo1", "lfo", { { "wave", 0 }, { "rate", 0.07f }, { "depth", 1 } } },
              { "lfo2", "lfo", { { "wave", 1 }, { "rate", 0.19f }, { "depth", 0.7f } } },
              { "noise", "noise", { { "tone", 12000 }, { "lvl", 0.6f } } },
              { "vco", "vco", { { "wave", 0 }, { "oct", -2 } } },
              { "out", "output", { { "vol", 0.65f } } } },
            { { "mix", "mixer", { { "l1", 0.7f }, { "l2", 0.35f } } },
              { "vcf", "vcf", { { "type", 2 }, { "cutoff", 700 }, { "reso", 14 }, { "cv1", 0.4f }, { "cv2", 0.2f } } },
              { "vca", "vca", { { "gain", 0.8f } } },
              { "dly", "delay", { { "time", 0.9f }, { "fb", 0.6f }, { "tone", 2000 }, { "mix", 0.45f } } },
              { "rev", "reverb", { { "size", 7 }, { "mix", 0.6f } } } } },
          { { "noise.out", "mix.in1" }, { "vco.out", "mix.in2" }, { "mix.out", "vcf.in" }, { "lfo1.out", "vcf.cv1" },
            { "lfo2.out", "vcf.cv2" }, { "vcf.out", "vca.in" }, { "vca.out", "dly.in" }, { "dly.out", "rev.in" },
            { "rev.out", "out.l" } } },

        { "Leeres Rack",
          { { { "kb", "keyboard", {} }, { "out", "output", {} } } },
          {} },
    };
    return p;
}

juce::StringArray Engine::presetNames()
{
    juce::StringArray a;
    for (auto& p : presets()) a.add (p.title);
    return a;
}

void Engine::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets().size()) return;
    const auto& pr = presets()[(size_t) index];
    clear();

    std::map<juce::String, Module*> names;
    int y = 16;
    for (auto& row : pr.rows)
    {
        int x = 16, maxH = 0;
        for (auto& pm : row)
        {
            auto m = addModule (pm.type, x, y);
            if (m == nullptr) continue;
            for (auto& kv : pm.params)
            {
                const int i = m->spec.paramIndex (kv.first);
                if (i >= 0) m->setParam (i, kv.second);
            }
            names[pm.name] = m.get();
            x += m->spec.width + 14;
            maxH = juce::jmax (maxH, computeLayout (m->spec).height);
        }
        y += maxH + 34;
    }

    for (auto& c : pr.cables)
    {
        auto a = juce::StringArray::fromTokens (c.first, ".", "");
        auto b = juce::StringArray::fromTokens (c.second, ".", "");
        auto* s = names[a[0]];
        auto* d = names[b[0]];
        if (s == nullptr || d == nullptr) continue;
        addCable (s->uid, s->spec.outIndex (a[1]), d->uid, d->spec.inIndex (b[1]), nextColour());
    }
}

} // namespace pw
