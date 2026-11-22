#include "Parameters.h"

namespace ux
{
namespace
{
using V = std::vector<ParamDef>;

// NOTE: the returned reference is only valid until the next push_back. Use it immediately.
ParamDef& add(V& v, const juce::String& id, const juce::String& name,
              float mn, float mx, float def, Section s,
              float interval = 0.0f, float skew = 1.0f)
{
    ParamDef d;
    d.id = id; d.name = name;
    d.min = mn; d.max = mx; d.def = def;
    d.interval = interval; d.skew = skew;
    d.section = s;
    v.push_back(d);
    return v.back();
}

ParamDef& addChoice(V& v, const juce::String& id, const juce::String& name,
                    const juce::StringArray& choices, int defIndex, Section s)
{
    ParamDef d;
    d.id = id; d.name = name;
    d.choices = choices;
    d.min = 0.0f; d.max = (float) (choices.size() - 1);
    d.def = (float) defIndex;
    d.interval = 1.0f;
    d.section = s;
    v.push_back(d);
    return v.back();
}

V buildDefs()
{
    V v;
    const juce::StringArray offOn { "Off", "On" };

    // ---------------- Oscillators ----------------
    for (int i = 1; i <= 3; ++i)
    {
        const auto p = "osc" + juce::String(i) + "_";
        const auto n = "Osc " + juce::String(i) + " ";
        add(v, p + "wave",   n + "Wave Pos", 0.0f, 1.0f, 0.0f, Section::Oscillators);
        add(v, p + "level",  n + "Level",    0.0f, 1.0f, i == 1 ? 0.8f : 0.0f, Section::Oscillators);
        add(v, p + "pan",    n + "Pan",     -1.0f, 1.0f, 0.0f, Section::Oscillators);
        add(v, p + "tune",   n + "Fine Tune (cents)", -50.0f, 50.0f, 0.0f, Section::Oscillators).randLo = 0.4f;
        v.back().randHi = 0.6f;
        add(v, p + "phase",  n + "Phase",    0.0f, 1.0f, 0.0f, Section::Oscillators);
        add(v, p + "warp",   n + "Warp",     0.0f, 1.0f, 0.0f, Section::Oscillators);
        // Octave is a safe boundary: never randomized.
        add(v, p + "octave", n + "Octave",  -3.0f, 3.0f, 0.0f, Section::Oscillators, 1.0f).randomizable = false;
    }
    add(v, "sub_level",   "Sub Level",   0.0f, 1.0f, 0.0f, Section::Oscillators);
    add(v, "noise_level", "Noise Level", 0.0f, 1.0f, 0.0f, Section::Oscillators);

    // ---------------- Filters ----------------
    addChoice(v, "filter_type", "Filter Type", { "LP", "BP", "HP", "Notch" }, 0, Section::Filters);
    {
        auto& c = add(v, "filter_cutoff", "Cutoff", 20.0f, 20000.0f, 8000.0f, Section::Filters, 0.0f, 0.3f);
        c.randLo = 0.25f; c.randHi = 0.95f;    // never fully closed
    }
    add(v, "filter_reso",     "Resonance", 0.0f, 1.0f, 0.1f, Section::Filters).randHi = 0.8f;
    add(v, "filter_drive",    "Drive",     0.0f, 1.0f, 0.0f, Section::Filters).randHi = 0.6f;
    add(v, "filter_keytrack", "KeyTrack",  0.0f, 1.0f, 0.0f, Section::Filters);

    // ---------------- LFOs ----------------
    for (int i = 1; i <= 4; ++i)
    {
        const auto p = "lfo" + juce::String(i) + "_";
        const auto n = "LFO " + juce::String(i) + " ";
        addChoice(v, p + "shape", n + "Shape", { "Sine", "Triangle", "Square", "Saw", "Random" }, 0, Section::LFOs);
        add(v, p + "rate",     n + "Rate",     0.01f, 20.0f, 1.0f, Section::LFOs, 0.0f, 0.4f);
        add(v, p + "depth",    n + "Depth",    0.0f, 1.0f, 0.5f, Section::LFOs);
        add(v, p + "phase",    n + "Phase",    0.0f, 1.0f, 0.0f, Section::LFOs);
        add(v, p + "symmetry", n + "Symmetry", 0.0f, 1.0f, 0.5f, Section::LFOs);
        addChoice(v, p + "sync", n + "Sync", offOn, 0, Section::LFOs);
    }

    // ---------------- Envelopes ----------------
    for (int i = 1; i <= 4; ++i)
    {
        const auto p = "env" + juce::String(i) + "_";
        const auto n = "Env " + juce::String(i) + " ";
        add(v, p + "a",     n + "Attack",  0.001f, 10.0f, 0.01f, Section::Envelopes, 0.0f, 0.3f);
        add(v, p + "d",     n + "Decay",   0.001f, 10.0f, 0.30f, Section::Envelopes, 0.0f, 0.3f);
        add(v, p + "s",     n + "Sustain", 0.0f,   1.0f,  0.70f, Section::Envelopes);
        add(v, p + "r",     n + "Release", 0.001f, 10.0f, 0.30f, Section::Envelopes, 0.0f, 0.3f);
        add(v, p + "curve", n + "Curve",  -1.0f,   1.0f,  0.0f,  Section::Envelopes);
    }

    // ---------------- Mod matrix (6 slots) ----------------
    const juce::StringArray sources { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                                      "Env 1", "Env 2", "Env 3", "Env 4",
                                      "Step Seq", "XY Pad",
                                      "Macro 1", "Macro 2", "Macro 3", "Macro 4",
                                      "Macro 5", "Macro 6", "Macro 7", "Macro 8" };
    const juce::StringArray targets { "Cutoff", "Resonance", "Drive", "Pitch", "Pan",
                                      "Volume", "Warp", "Reverb", "Delay", "Distortion" };
    for (int i = 1; i <= 6; ++i)
    {
        const auto p = "mod" + juce::String(i) + "_";
        const auto n = "Mod " + juce::String(i) + " ";
        addChoice(v, p + "src", n + "Source", sources, 0, Section::ModMatrix);
        addChoice(v, p + "dst", n + "Target", targets, 0, Section::ModMatrix);
        auto& a = add(v, p + "amt", n + "Amount", -1.0f, 1.0f, 0.0f, Section::ModMatrix);
        a.randLo = 0.3f; a.randHi = 0.7f;      // moderate amounts (about +/-40%)
    }

    // ---------------- XY pad ----------------
    add(v, "xy_x", "XY X", 0.0f, 1.0f, 0.5f, Section::XYPad);
    add(v, "xy_y", "XY Y", 0.0f, 1.0f, 0.5f, Section::XYPad);
    addChoice(v, "xy_x_target", "XY X Target", targets, 0, Section::XYPad);
    addChoice(v, "xy_y_target", "XY Y Target", targets, 7, Section::XYPad);

    // ---------------- Step sequencer ----------------
    add(v, "seq_steps",  "Seq Steps",  1.0f, 16.0f, 16.0f, Section::StepSeq, 1.0f).randomizable = false;
    add(v, "seq_rate",   "Seq Rate",   0.0f, 1.0f, 0.5f, Section::StepSeq);
    add(v, "seq_smooth", "Seq Smooth", 0.0f, 1.0f, 0.0f, Section::StepSeq);
    add(v, "seq_depth",  "Seq Depth",  0.0f, 1.0f, 0.5f, Section::StepSeq);
    for (int i = 1; i <= 16; ++i)
        add(v, "seq_step" + juce::String(i), "Seq Step " + juce::String(i), 0.0f, 1.0f, 0.5f, Section::StepSeq);

    // ---------------- Arpeggiator ----------------
    addChoice(v, "arp_mode", "Arp Mode", { "Off", "Up", "Down", "Up/Down", "Random" }, 0, Section::Arp);
    add(v, "arp_rate",    "Arp Rate",    0.0f, 1.0f, 0.5f, Section::Arp);
    add(v, "arp_octaves", "Arp Octaves", 1.0f, 4.0f, 1.0f, Section::Arp, 1.0f);

    // ---------------- Effects ----------------
    add(v, "fx_reverb_mix",  "Reverb Mix",     0.0f, 1.0f, 0.2f, Section::Effects).randHi = 0.6f;
    add(v, "fx_reverb_size", "Reverb Size",    0.0f, 1.0f, 0.5f, Section::Effects);
    add(v, "fx_delay_mix",   "Delay Mix",      0.0f, 1.0f, 0.0f, Section::Effects).randHi = 0.6f;
    add(v, "fx_delay_time",  "Delay Time",     0.01f, 2.0f, 0.35f, Section::Effects, 0.0f, 0.5f);
    add(v, "fx_delay_fb",    "Delay Feedback", 0.0f, 0.95f, 0.3f, Section::Effects).randHi = 0.7f;
    add(v, "fx_chorus_mix",  "Chorus Mix",     0.0f, 1.0f, 0.0f, Section::Effects).randHi = 0.6f;
    add(v, "fx_phaser_mix",  "Phaser Mix",     0.0f, 1.0f, 0.0f, Section::Effects).randHi = 0.6f;
    add(v, "fx_dist_drive",  "Distortion",     0.0f, 1.0f, 0.0f, Section::Effects).randHi = 0.5f;
    add(v, "fx_eq_tilt",     "EQ Tilt",       -1.0f, 1.0f, 0.0f, Section::Effects).randLo = 0.25f;
    v.back().randHi = 0.75f;

    // ---------------- Macros ----------------
    for (int i = 1; i <= 8; ++i)
        add(v, "macro" + juce::String(i), "Macro " + juce::String(i), 0.0f, 1.0f, 0.0f, Section::Macros);

    // ---------------- Global (never randomized) ----------------
    add(v, "master_volume", "Master Volume", 0.0f, 1.0f, 0.7f, Section::Global).randomizable = false;
    add(v, "polyphony",     "Polyphony", 1.0f, 32.0f, 16.0f, Section::Global, 1.0f).randomizable = false;
    add(v, "unison_voices", "Unison",    1.0f, 8.0f, 1.0f, Section::Global, 1.0f).randomizable = false;
    add(v, "unison_detune", "Detune",    0.0f, 1.0f, 0.2f, Section::Global).randomizable = false;
    add(v, "unison_spread", "Spread",    0.0f, 1.0f, 0.5f, Section::Global).randomizable = false;
    add(v, "glide",         "Glide",     0.0f, 2.0f, 0.0f, Section::Global).randomizable = false;
    add(v, "portamento",    "Portamento",0.0f, 2.0f, 0.0f, Section::Global).randomizable = false;

    return v;
}
} // namespace

const std::vector<ParamDef>& getParamDefs()
{
    static const V defs = buildDefs();
    return defs;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& d : getParamDefs())
    {
        if (d.isChoice())
            layout.add(std::make_unique<juce::AudioParameterChoice>(d.id, d.name, d.choices, (int) d.def));
        else
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                d.id, d.name,
                juce::NormalisableRange<float>(d.min, d.max, d.interval, d.skew),
                d.def));
    }
    return layout;
}
} // namespace ux
