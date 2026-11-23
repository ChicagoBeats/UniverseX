// Universe X - Phase 3 (artwork interface), single-file version.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <BinaryData.h>


// ===== Parameters.h =====

namespace ux
{
// Sections follow the Lazy Randomizer order: Lazy buttons 1-9 map to the first nine entries.
enum class Section
{
    Oscillators = 0,
    Filters,
    LFOs,
    Envelopes,
    ModMatrix,
    XYPad,
    StepSeq,
    Arp,
    Effects,
    Macros,
    Sampler,     // sample / granular source (only touched by Full Mix, and only the grain controls)
    Global,      // master volume, octave, polyphony... never randomized
    Count
};

struct ParamDef
{
    juce::String id, name;
    float min = 0.0f, max = 1.0f, def = 0.0f;   // for choices: def = default index
    float interval = 0.0f, skew = 1.0f;
    Section section = Section::Global;

    bool randomizable = true;                   // false = "safe boundary", never touched by randomizer
    float randLo = 0.0f, randHi = 1.0f;         // randomizer range, in normalised 0..1 units

    juce::StringArray choices;                  // non-empty => choice parameter
    bool isChoice() const { return choices.size() > 0; }
};

// Single source of truth: the UI, the audio engine and the randomizer all use this list.
const std::vector<ParamDef>& getParamDefs();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}


// ===== Parameters.cpp =====

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
        add(v, p + "wave",   n + "Wave Pos", 0.0f, 1.0f, i == 1 ? 2.0f / 63.0f : 0.0f, Section::Oscillators);
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

    // ---------------- Sampler / granular ----------------
    addChoice(v, "sample_mode", "Sample Mode", { "Sample", "Granular", "Spectral" }, 0, Section::Sampler).randomizable = false;
    add(v, "sample_level", "Sample Level", 0.0f, 1.0f, 0.8f, Section::Sampler).randomizable = false;
    add(v, "sample_start", "Sample Start", 0.0f, 1.0f, 0.0f, Section::Sampler).randomizable = false;
    add(v, "sample_end",   "Sample End",   0.0f, 1.0f, 1.0f, Section::Sampler).randomizable = false;
    addChoice(v, "sample_loop", "Sample Loop", offOn, 0, Section::Sampler).randomizable = false;
    add(v, "sample_root", "Sample Root Note", 12.0f, 108.0f, 60.0f, Section::Sampler, 1.0f).randomizable = false;
    add(v, "grain_size",    "Grain Size (ms)", 5.0f, 500.0f, 80.0f, Section::Sampler, 0.0f, 0.4f);
    add(v, "grain_density", "Grain Density",   1.0f, 100.0f, 20.0f, Section::Sampler, 0.0f, 0.5f);
    add(v, "grain_spread",  "Grain Spread",    0.0f, 1.0f, 0.3f, Section::Sampler);
    add(v, "grain_pos",     "Grain Position",  0.0f, 1.0f, 0.5f, Section::Sampler);

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


// ===== Randomizer.h =====

namespace ux
{
/** Lazy Randomizer: 10 one-click generators with section locks, amount and undo/redo.

    Buttons 1-9 randomize one section each (Oscillators, Filters, LFOs, Envelopes,
    Mod Matrix, XY Pad, Step Seq, Arpeggiator, Effects). Button 10 is "Full Mix".

    Safe boundaries: parameters flagged randomizable=false (master volume, octave,
    polyphony, unison, glide...) are never touched.

    Locks: a locked section is skipped by Full Mix. Clicking a section's own button
    always works, so you can still re-roll it deliberately.

    Amount: 0 = no change, 1 = full random. In between, values move part of the way
    toward a random target (subtle mutation).

    Threading: call from the message thread only (the UI).
*/
class Randomizer
{
public:
    static constexpr int numLazyButtons = 10;

    explicit Randomizer(juce::AudioProcessorValueTreeState& state);

    void lazy(int buttonIndex);                 // 0..9
    void randomizeSection(Section s);
    void randomizeAll();                        // respects locks

    void setAmount(float a)                     { amount = juce::jlimit(0.0f, 1.0f, a); }
    float getAmount() const                     { return amount; }

    void setSectionLocked(Section s, bool lock) { locked[(size_t) s] = lock; }
    bool isSectionLocked(Section s) const       { return locked[(size_t) s]; }

    bool canUndo() const                        { return ! undoStack.empty(); }
    bool canRedo() const                        { return ! redoStack.empty(); }
    bool undo();
    bool redo();

    static Section sectionForLazy(int buttonIndex);

private:
    std::vector<float> snapshot() const;
    void apply(const std::vector<float>& values);
    void pushUndo();
    void randomizeSectionInternal(Section s);
    void randomizeParam(const ParamDef& d);
    void keepSoundAudible();
    void setNormalised(const juce::String& id, float v);

    juce::AudioProcessorValueTreeState& apvts;
    juce::Random rng;
    float amount = 1.0f;
    std::array<bool, (size_t) Section::Count> locked {};
    std::vector<std::vector<float>> undoStack, redoStack;
    static constexpr size_t maxHistory = 64;
};
}


// ===== Randomizer.cpp =====

namespace ux
{
Randomizer::Randomizer(juce::AudioProcessorValueTreeState& state) : apvts(state) {}

Section Randomizer::sectionForLazy(int buttonIndex)
{
    return static_cast<Section>(juce::jlimit(0, 8, buttonIndex));
}

void Randomizer::lazy(int buttonIndex)
{
    if (buttonIndex >= numLazyButtons - 1)
        randomizeAll();
    else
        randomizeSection(sectionForLazy(buttonIndex));
}

void Randomizer::randomizeSection(Section s)
{
    pushUndo();
    randomizeSectionInternal(s);
}

void Randomizer::randomizeAll()
{
    pushUndo();

    for (int i = 0; i < (int) Section::Global; ++i)
    {
        const auto s = static_cast<Section>(i);
        if (! isSectionLocked(s))
            randomizeSectionInternal(s);
    }
}

void Randomizer::randomizeSectionInternal(Section s)
{
    for (const auto& d : getParamDefs())
        if (d.section == s && d.randomizable)
            randomizeParam(d);

    if (s == Section::Oscillators)
        keepSoundAudible();
}

void Randomizer::randomizeParam(const ParamDef& d)
{
    auto* p = apvts.getParameter(d.id);
    if (p == nullptr)
        return;

    const float current = p->getValue();
    float target;

    if (d.isChoice())
    {
        // Choices cannot be blended: change them with probability = amount.
        if (rng.nextFloat() > amount)
            return;

        target = p->convertTo0to1((float) rng.nextInt(d.choices.size()));
        p->beginChangeGesture();
        p->setValueNotifyingHost(target);
        p->endChangeGesture();
        return;
    }

    target = d.randLo + rng.nextFloat() * (d.randHi - d.randLo);
    const float newValue = juce::jlimit(0.0f, 1.0f, current + (target - current) * amount);

    p->beginChangeGesture();
    p->setValueNotifyingHost(newValue);
    p->endChangeGesture();
}

// Avoid generating a silent patch: at least one sound source must be audible.
void Randomizer::keepSoundAudible()
{
    auto get = [this](const char* id)
    {
        auto* p = apvts.getParameter(id);
        return p != nullptr ? p->getValue() : 0.0f;
    };

    if (get("osc1_level") < 0.1f && get("osc2_level") < 0.1f && get("osc3_level") < 0.1f
        && get("sub_level") < 0.1f && get("noise_level") < 0.1f)
    {
        setNormalised("osc1_level", 0.7f);
    }
}

void Randomizer::setNormalised(const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(v);
        p->endChangeGesture();
    }
}

// ---------------- undo / redo ----------------

std::vector<float> Randomizer::snapshot() const
{
    std::vector<float> s;
    s.reserve(getParamDefs().size());

    for (const auto& d : getParamDefs())
    {
        auto* p = apvts.getParameter(d.id);
        s.push_back(p != nullptr ? p->getValue() : 0.0f);
    }
    return s;
}

void Randomizer::apply(const std::vector<float>& values)
{
    const auto& defs = getParamDefs();

    for (size_t i = 0; i < defs.size() && i < values.size(); ++i)
        setNormalised(defs[i].id, values[i]);
}

void Randomizer::pushUndo()
{
    undoStack.push_back(snapshot());

    if (undoStack.size() > maxHistory)
        undoStack.erase(undoStack.begin());

    redoStack.clear();
}

bool Randomizer::undo()
{
    if (undoStack.empty())
        return false;

    redoStack.push_back(snapshot());
    apply(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool Randomizer::redo()
{
    if (redoStack.empty())
        return false;

    undoStack.push_back(snapshot());
    apply(redoStack.back());
    redoStack.pop_back();
    return true;
}
}



// ===== Phase 2: audio engine =====
namespace ux
{
static constexpr float kPi = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// Band-limited wavetable bank: 8 waveforms x 11 mip levels (one per octave).
// ---------------------------------------------------------------------------
class WavetableBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numMips = 11;
    static constexpr int numWaves = 64;

    static const WavetableBank& get()
    {
        static const WavetableBank bank;
        return bank;
    }

    static const juce::StringArray& familyNames()
    {
        static const juce::StringArray f { "Basic", "Organ", "Vowels", "Bells & Metal", "Glass & Digital", "Reed & Hollow", "Pads & Soft", "Sync & FM" };
        return f;
    }

    static const juce::StringArray& waveNames()
    {
        static const juce::StringArray n {
            "Sine", "Triangle", "Saw", "Square", "Pulse 33", "Pulse 20", "Pulse 10", "Soft Saw",
            "Full Organ", "Jazz Organ", "Rock Organ", "Flute", "Principal", "Reed Organ", "Mellow Organ", "Bright Organ",
            "Vowel A", "Vowel E", "Vowel I", "Vowel O", "Vowel U", "Choir Ah", "Nasal", "Throat",
            "Soft Bell", "Bright Bell", "Chime", "Tubular", "Metal Hit", "Gong", "Glassy Bell", "Music Box",
            "Glass", "Crystal", "Shard", "Static", "Noise Tone", "Cyber", "Bit Wave", "Spectral Dust",
            "Clarinet", "Hollow Odd", "Even Heavy", "Hollow 3", "Comb 4", "Comb 5", "Nasal Reed", "Sax",
            "Soft Pad", "Warm Pad", "Airy Pad", "Dark Pad", "Strings", "Glow", "Dream", "Haze",
            "Sync Soft", "Sync Sweep", "Sync Hard", "FM Bell", "FM Clang", "FM Glass", "FM Growl", "FM Chirp" };
        return n;
    }

    float lookup(float wavePos, int mip, float phase) const noexcept
    {
        const float wp = juce::jlimit(0.0f, 1.0f, wavePos) * (float) (numWaves - 1);
        const int w0 = (int) wp;
        const int w1 = juce::jmin(w0 + 1, numWaves - 1);
        const float wf = wp - (float) w0;

        const float idx = phase * (float) tableSize;
        int i0 = (int) idx;
        const float fr = idx - (float) i0;
        const int i1 = (i0 + 1) & (tableSize - 1);
        i0 &= (tableSize - 1);

        const float* t0 = &data[(size_t) ((w0 * numMips + mip) * tableSize)];
        const float* t1 = &data[(size_t) ((w1 * numMips + mip) * tableSize)];
        const float a = t0[i0] + fr * (t0[i1] - t0[i0]);
        const float b = t1[i0] + fr * (t1[i1] - t1[i0]);
        return a + wf * (b - a);
    }

    static int mipForIncrement(float inc) noexcept
    {
        const float x = 2048.0f * inc;
        if (x <= 1.0f)
            return 0;
        return juce::jmin(numMips - 1, (int) std::ceil(std::log2(x)));
    }

private:
    WavetableBank()
    {
        data.assign((size_t) (numWaves * numMips * tableSize), 0.0f);

        std::vector<float> sinT((size_t) tableSize);
        for (int i = 0; i < tableSize; ++i)
            sinT[(size_t) i] = std::sin(2.0f * kPi * (float) i / (float) tableSize);

        for (int w = 0; w < numWaves; ++w)
        {
            // Mip m holds harmonics 1..(1024 >> m). Build from the smallest table up, adding only the new harmonics.
            for (int m = numMips - 1; m >= 0; --m)
            {
                float* t = &data[(size_t) ((w * numMips + m) * tableSize)];
                if (m < numMips - 1)
                {
                    const float* prev = &data[(size_t) ((w * numMips + m + 1) * tableSize)];
                    std::copy(prev, prev + tableSize, t);
                }

                const int hiH = juce::jmin(1023, 1024 >> m);
                const int loH = (m == numMips - 1) ? 1 : juce::jmin(1023, 1024 >> (m + 1)) + 1;

                for (int h = loH; h <= hiH; ++h)
                {
                    const float a = harmonic(w, h);
                    if (std::abs(a) < 1.0e-5f)
                        continue;
                    for (int i = 0; i < tableSize; ++i)
                        t[i] += a * sinT[(size_t) ((h * i) & (tableSize - 1))];
                }
            }

            float peak = 0.0f;
            const float* t0 = &data[(size_t) ((w * numMips) * tableSize)];
            for (int i = 0; i < tableSize; ++i)
                peak = juce::jmax(peak, std::abs(t0[i]));

            const float scale = peak > 0.0f ? 0.9f / peak : 1.0f;
            for (int m = 0; m < numMips; ++m)
            {
                float* t = &data[(size_t) ((w * numMips + m) * tableSize)];
                for (int i = 0; i < tableSize; ++i)
                    t[i] *= scale;
            }
        }
    }

    static float frac(float x) { return x - std::floor(x); }

    // 64 tables = 8 families x 8 variations, all defined by harmonic recipes (clean at every pitch).
    static float harmonic(int w, int h)
    {
        const float n = (float) h;
        const int fam = w / 8, v = w % 8;
        const float fv = (float) v;

        switch (fam)
        {
            case 0:     // Basic
                switch (v)
                {
                    case 0:  return h == 1 ? 1.0f : 0.0f;
                    case 1:  return (h & 1) ? ((((h - 1) / 2) & 1) ? -1.0f : 1.0f) / (n * n) : 0.0f;
                    case 2:  return 1.0f / n;
                    case 3:  return (h & 1) ? 1.0f / n : 0.0f;
                    case 4:  return std::sin(n * kPi * 0.33f) / n;
                    case 5:  return std::sin(n * kPi * 0.20f) / n;
                    case 6:  return std::sin(n * kPi * 0.10f) / n;
                    default: return 1.0f / std::pow(n, 1.6f);
                }

            case 1:     // Organ drawbar sets (harmonics 1..9)
            {
                static const float draw[8][9] = {
                    { 1.0f, 0.9f, 0.8f, 0.7f, 0.5f, 0.4f, 0.3f, 0.3f, 0.2f }, { 1.0f, 0.1f, 0.8f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f },
                    { 1.0f, 0.8f, 0.0f, 0.0f, 0.6f, 0.0f, 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
                    { 1.0f, 0.6f, 0.0f, 0.4f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f }, { 1.0f, 0.0f, 0.7f, 0.0f, 0.5f, 0.0f, 0.3f, 0.0f, 0.2f },
                    { 1.0f, 0.5f, 0.3f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 0.7f, 0.9f, 0.6f, 0.8f, 0.5f, 0.6f, 0.4f, 0.5f } };
                return h <= 9 ? draw[v][h - 1] : 0.0f;
            }

            case 2:     // Vowels: formant bumps (centres are harmonic numbers of a ~110 Hz voice)
            {
                static const float F[8][3] = { { 6.6f, 9.9f, 22.0f }, { 4.8f, 16.7f, 22.5f }, { 2.5f, 20.8f, 27.0f }, { 5.2f, 7.6f, 21.9f },
                                               { 2.7f, 7.9f, 20.4f }, { 5.0f, 10.0f, 14.0f }, { 3.0f, 12.0f, 18.0f }, { 8.0f, 13.0f, 26.0f } };
                static const float A[3] = { 1.0f, 0.7f, 0.35f }, Wd[3] = { 1.6f, 2.4f, 3.5f };
                float f = 0.0f;
                for (int k = 0; k < 3; ++k)
                    f += A[k] * std::exp(-((n - F[v][k]) * (n - F[v][k])) / (2.0f * Wd[k] * Wd[k]));
                return f + 0.012f / n;
            }

            case 3:     // Bells and metal
                return (1.0f / std::pow(n, 0.7f + 0.08f * fv)) * ((h % (v + 2)) == 0 ? 1.0f : 0.12f);

            case 4:     // Glass and digital: deterministic pseudo-random spectra
            {
                const float r = frac(std::sin((float) w * 97.1f + n * 12.9898f) * 43758.5453f);
                return (0.25f + 0.75f * r) / std::pow(n, 0.55f + 0.05f * fv);
            }

            case 5:     // Reed and hollow
                switch (v)
                {
                    case 0:  return (h & 1) ? 1.0f / n : 0.0f;
                    case 1:  return (h & 1) ? 1.0f / std::pow(n, 1.5f) : 0.0f;
                    case 2:  return ((h % 2) == 0 ? 1.0f : 0.3f) / n;
                    case 3:  return (h % 3) != 0 ? 1.0f / n : 0.0f;
                    case 4:  return ((h % 4) == 1 ? 1.0f : 0.2f) / n;
                    case 5:  return ((h % 5) == 0 ? 1.0f : 0.1f) / std::pow(n, 0.8f);
                    case 6:  return std::exp(-((n - 10.0f) * (n - 10.0f)) / 18.0f) + 0.05f / n;
                    default: return (1.0f / std::pow(n, 0.9f)) * (1.0f + 0.5f * std::sin(n * 0.9f));
                }

            case 6:     // Pads and soft textures
                return juce::jmax(0.0f, (1.0f / std::pow(n, 1.4f + 0.15f * fv)) * (1.0f + 0.6f * std::sin(n * (0.5f + 0.35f * fv))));

            default:    // Sync and FM-like
                switch (v)
                {
                    case 0:  return (1.0f / n) * (0.5f + 0.5f * std::cos(2.0f * kPi * n / 5.0f));
                    case 1:  return (1.0f / n) * (0.5f + 0.5f * std::cos(2.0f * kPi * n / 8.0f));
                    case 2:  return (1.0f / n) * (0.5f + 0.5f * std::cos(2.0f * kPi * n / 12.0f));
                    case 3:  return std::abs(std::sin(n * 0.9f)) / std::pow(n, 0.9f);
                    case 4:  return std::abs(std::sin(n * 1.7f)) / std::pow(n, 0.7f);
                    case 5:  return std::abs(std::sin(n * 0.45f)) / std::pow(n, 1.1f);
                    case 6:  return std::abs(std::sin(n * 2.3f)) / std::pow(n, 0.6f);
                    default: return (std::abs(std::sin(n * 0.3f)) + 0.2f) / std::pow(n, 1.0f);
                }
        }
    }

    std::vector<float> data;
};

// ---------------------------------------------------------------------------
// Small DSP building blocks
// ---------------------------------------------------------------------------
struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;

    void setCoeffs(float cutoff, float q, float sampleRate)
    {
        const float g = std::tan(kPi * cutoff / sampleRate);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process(float x, int type) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        const float low = v2, band = v1 * k, high = x - k * v1 - v2;
        switch (type)
        {
            case 1:  return band;
            case 2:  return high;
            case 3:  return low + high;
            default: return low;
        }
    }
};

struct Env
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage st = Idle;
    float level = 0.0f, relStart = 0.0f;

    void noteOn() { st = Attack; }

    void noteOff()
    {
        if (st == Idle)
            return;
        relStart = level;
        st = level > 0.0f ? Release : Idle;
    }

    float advance(int n, float a, float d, float s, float r, float sampleRate) noexcept
    {
        const float t = (float) n / sampleRate;
        switch (st)
        {
            case Attack:
                level += t / juce::jmax(a, 0.0005f);
                if (level >= 1.0f) { level = 1.0f; st = Decay; }
                break;
            case Decay:
                level -= (1.0f - s) * t / juce::jmax(d, 0.0005f);
                if (level <= s) { level = s; st = Sustain; }
                break;
            case Sustain:
                level = s;
                break;
            case Release:
                level -= relStart * t / juce::jmax(r, 0.0005f);
                if (level <= 0.0f) { level = 0.0f; st = Idle; }
                break;
            default:
                break;
        }
        return level;
    }
};

inline float shapeEnv(float v, float curve) noexcept
{
    v = juce::jlimit(0.0f, 1.0f, v);
    if (curve >= 0.0f)
        return std::pow(v, 1.0f + 2.0f * curve);
    return 1.0f - std::pow(1.0f - v, 1.0f - 2.0f * curve);
}

// A loaded sample (kept whole in memory, shared with the audio thread through a shared_ptr).
struct SampleData
{
    std::vector<float> L, R;
    double sampleRate = 44100.0;
    int length = 0;
    juce::String name;
};

struct Voice
{
    bool active = false, released = false;
    int note = 60;
    float vel = 0.8f;
    std::uint64_t age = 0;
    float curNote = 60.0f, targetNote = 60.0f;
    float phase[3][8] = {};
    float subPhase = 0.0f, lastAmp = 0.0f;
    Env env[4];
    SVF fl, fr;

    // sampler / granular state
    double sPos = 0.0;
    bool sOn = false;
    struct Grain { bool on = false; double pos = 0.0; float inc = 1.0f, gl = 0.7f, gr = 0.7f; int age = 0, len = 1; };
    Grain grains[16];
    float gTimer = 0.0f;
};

// ---------------------------------------------------------------------------
// Cached pointers to the raw parameter values (safe to read on the audio thread)
// ---------------------------------------------------------------------------
struct ParamPtrs
{
    using P = std::atomic<float>*;
    P oscWave[3], oscLevel[3], oscPan[3], oscTune[3], oscPhase[3], oscWarp[3], oscOctave[3];
    P subLevel, noiseLevel;
    P fType, fCut, fReso, fDrive, fKey;
    P lfoShape[4], lfoRate[4], lfoDepth[4], lfoPhase[4], lfoSym[4], lfoSync[4];
    P envA[4], envD[4], envS[4], envR[4], envCurve[4];
    P modSrc[6], modDst[6], modAmt[6];
    P xyX, xyY, xyXT, xyYT;
    P seqSteps, seqRate, seqSmooth, seqDepth, seqStep[16];
    P arpMode, arpRate, arpOct;
    P revMix, revSize, delMix, delTime, delFb, choMix, phaMix, distDrive, eqTilt;
    P macro[8];
    P master, poly, uniVoices, uniDetune, uniSpread, glide, porta;
    P sampMode, sampLevel, sampStart, sampEnd, sampLoop, sampRoot, grainSize, grainDensity, grainSpread, grainPos;

    void bind(juce::AudioProcessorValueTreeState& s)
    {
        auto g = [&s](const juce::String& id) -> P
        {
            auto* p = s.getRawParameterValue(id);
            jassert(p != nullptr);
            return p;
        };

        for (int i = 0; i < 3; ++i)
        {
            const auto p = "osc" + juce::String(i + 1) + "_";
            oscWave[i] = g(p + "wave");   oscLevel[i] = g(p + "level");   oscPan[i] = g(p + "pan");
            oscTune[i] = g(p + "tune");   oscPhase[i] = g(p + "phase");   oscWarp[i] = g(p + "warp");
            oscOctave[i] = g(p + "octave");
        }
        subLevel = g("sub_level");  noiseLevel = g("noise_level");

        fType = g("filter_type");  fCut = g("filter_cutoff");  fReso = g("filter_reso");
        fDrive = g("filter_drive");  fKey = g("filter_keytrack");

        for (int i = 0; i < 4; ++i)
        {
            const auto l = "lfo" + juce::String(i + 1) + "_";
            lfoShape[i] = g(l + "shape");  lfoRate[i] = g(l + "rate");  lfoDepth[i] = g(l + "depth");
            lfoPhase[i] = g(l + "phase");  lfoSym[i] = g(l + "symmetry");  lfoSync[i] = g(l + "sync");

            const auto e = "env" + juce::String(i + 1) + "_";
            envA[i] = g(e + "a");  envD[i] = g(e + "d");  envS[i] = g(e + "s");
            envR[i] = g(e + "r");  envCurve[i] = g(e + "curve");
        }

        for (int i = 0; i < 6; ++i)
        {
            const auto m = "mod" + juce::String(i + 1) + "_";
            modSrc[i] = g(m + "src");  modDst[i] = g(m + "dst");  modAmt[i] = g(m + "amt");
        }

        xyX = g("xy_x");  xyY = g("xy_y");  xyXT = g("xy_x_target");  xyYT = g("xy_y_target");

        seqSteps = g("seq_steps");  seqRate = g("seq_rate");  seqSmooth = g("seq_smooth");  seqDepth = g("seq_depth");
        for (int i = 0; i < 16; ++i)
            seqStep[i] = g("seq_step" + juce::String(i + 1));

        arpMode = g("arp_mode");  arpRate = g("arp_rate");  arpOct = g("arp_octaves");

        revMix = g("fx_reverb_mix");  revSize = g("fx_reverb_size");  delMix = g("fx_delay_mix");
        delTime = g("fx_delay_time");  delFb = g("fx_delay_fb");  choMix = g("fx_chorus_mix");
        phaMix = g("fx_phaser_mix");  distDrive = g("fx_dist_drive");  eqTilt = g("fx_eq_tilt");

        for (int i = 0; i < 8; ++i)
            macro[i] = g("macro" + juce::String(i + 1));

        master = g("master_volume");  poly = g("polyphony");  uniVoices = g("unison_voices");
        uniDetune = g("unison_detune");  uniSpread = g("unison_spread");  glide = g("glide");  porta = g("portamento");

        sampMode = g("sample_mode");  sampLevel = g("sample_level");  sampStart = g("sample_start");  sampEnd = g("sample_end");
        sampLoop = g("sample_loop");  sampRoot = g("sample_root");  grainSize = g("grain_size");  grainDensity = g("grain_density");
        grainSpread = g("grain_spread");  grainPos = g("grain_pos");
    }
};

// ---------------------------------------------------------------------------
// The synth engine: voices, modulation, arpeggiator, step sequencer, effects
// ---------------------------------------------------------------------------
class Engine
{
public:
    static constexpr int maxVoices = 32;
    static constexpr int chunk = 32;

    void prepare(double sampleRate)
    {
        sr = sampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;
        chorus.prepare(spec);
        phaser.prepare(spec);
        chorus.setCentreDelay(7.0f);  chorus.setFeedback(0.0f);  chorus.setRate(0.8f);  chorus.setDepth(0.3f);
        phaser.setRate(0.4f);  phaser.setDepth(0.7f);  phaser.setCentreFrequency(900.0f);  phaser.setFeedback(0.3f);
        chorus.reset();
        phaser.reset();

        reverb.setSampleRate(sampleRate);
        reverb.reset();

        delaySize = (int) (2.5 * sampleRate) + 8;
        delayL.assign((size_t) delaySize, 0.0f);
        delayR.assign((size_t) delaySize, 0.0f);
        delayPos = 0;
        delayTimeS = 0.35f;
        eqLowL = eqLowR = 0.0f;

        for (auto& v : voices)
            v = Voice();

        numHeld = 0;
        arpNote = -1;
        lastArpMode = 0;
        arpTimer = 0.0;
        pitchBend = 0.0f;
        lastNote = -1.0f;
    }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const ParamPtrs& p, double bpmIn, const SampleData* sample = nullptr)
    {
        params = &p;
        sampleDataPtr = sample;
        bpm = bpmIn > 0.0 ? bpmIn : 120.0;
        buffer.clear();

        const int numSamples = buffer.getNumSamples();
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);

        int pos = 0;
        for (const auto meta : midi)
        {
            const int t = juce::jlimit(pos, numSamples, meta.samplePosition);
            renderRange(L, R, pos, t);
            pos = t;
            handleMidi(meta.getMessage());
        }
        renderRange(L, R, pos, numSamples);

        processEffects(buffer);
    }

private:
    // ----- MIDI and voice management -----
    void handleMidi(const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            const float vel = m.getFloatVelocity();
            addHeld(note);

            if ((int) params->arpMode->load() > 0)
            {
                arpVel = vel;
                if (numHeld == 1) { arpIdx = 0; arpDir = 1; arpTimer = 1.0e9; }
            }
            else
                triggerVoice(note, vel);
        }
        else if (m.isNoteOff())
        {
            const int note = m.getNoteNumber();
            removeHeld(note);

            if ((int) params->arpMode->load() > 0)
            {
                if (numHeld == 0 && arpNote >= 0) { releaseNote(arpNote); arpNote = -1; }
            }
            else
                releaseNote(note);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            numHeld = 0;
            arpNote = -1;
            for (auto& v : voices)
                if (v.active && ! v.released)
                    releaseVoice(v);
        }
        else if (m.isPitchWheel())
        {
            pitchBend = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
        }
    }

    void addHeld(int n)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[i] == n)
                return;
        if (numHeld >= 64)
            return;
        int j = numHeld++;
        while (j > 0 && held[j - 1] > n) { held[j] = held[j - 1]; --j; }
        held[j] = n;
    }

    void removeHeld(int n)
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[i] == n)
            {
                for (int j = i; j < numHeld - 1; ++j)
                    held[j] = held[j + 1];
                --numHeld;
                return;
            }
    }

    void releaseVoice(Voice& v)
    {
        v.released = true;
        for (auto& e : v.env)
            e.noteOff();
    }

    void releaseNote(int note)
    {
        for (auto& v : voices)
            if (v.active && ! v.released && v.note == note)
                releaseVoice(v);
    }

    void triggerVoice(int note, float vel)
    {
        const auto& P = *params;
        const int poly = juce::jlimit(1, maxVoices, (int) P.poly->load());

        int idx = -1;
        for (int i = 0; i < poly; ++i)
            if (! voices[i].active) { idx = i; break; }

        if (idx < 0)
        {
            std::uint64_t best = ~(std::uint64_t) 0;
            for (int i = 0; i < poly; ++i)
                if (voices[i].age < best) { best = voices[i].age; idx = i; }
        }

        Voice& v = voices[idx];
        if (! v.active)
        {
            for (auto& e : v.env) e.level = 0.0f;
            v.lastAmp = 0.0f;
            v.fl = SVF();
            v.fr = SVF();
        }

        const float glideT = juce::jmax(P.glide->load(), P.porta->load());
        v.active = true;
        v.released = false;
        v.note = note;
        v.vel = vel;
        v.age = ++ageCounter;
        v.targetNote = (float) note;
        v.curNote = (glideT > 0.0005f && lastNote >= 0.0f) ? lastNote : (float) note;
        lastNote = (float) note;

        for (int o = 0; o < 3; ++o)
            for (int u = 0; u < 8; ++u)
                v.phase[o][u] = u == 0 ? juce::jlimit(0.0f, 0.999f, P.oscPhase[o]->load()) : rng.nextFloat();
        v.subPhase = 0.0f;

        v.sOn = true;
        {
            const int len = sampleDataPtr != nullptr ? sampleDataPtr->length : 0;
            v.sPos = (double) juce::jlimit(0.0f, 1.0f, P.sampStart->load()) * (double) len;
        }
        v.gTimer = 0.0f;
        for (auto& gr : v.grains)
            gr.on = false;

        for (auto& e : v.env)
            e.noteOn();
    }

    // ----- Rendering -----
    void renderRange(float* L, float* R, int start, int end)
    {
        while (start < end)
        {
            const int n = juce::jmin(chunk, end - start);
            renderChunk(L + start, R + start, n);
            start += n;
        }
    }

    void renderChunk(float* L, float* R, int n)
    {
        updateGlobals(n);

        for (auto& v : voices)
            if (v.active)
                renderVoice(v, L, R, n);
    }

    void updateGlobals(int n)
    {
        const auto& P = *params;
        const double dt = (double) n / sr;

        gsrc[0] = 0.0f;

        // LFOs (global)
        for (int l = 0; l < 4; ++l)
        {
            const int shape = (int) P.lfoShape[l]->load();
            const float rate = P.lfoRate[l]->load();
            const double f = P.lfoSync[l]->load() > 0.5f ? (bpm / 240.0) * (double) rate : (double) rate;

            auto& s = lfo[l];
            s.phase += f * dt;
            if (s.phase >= 1.0)
            {
                s.phase -= std::floor(s.phase);
                s.sh = rng.nextFloat() * 2.0f - 1.0f;
            }

            float ph = (float) s.phase + P.lfoPhase[l]->load();
            ph -= std::floor(ph);
            const float sym = juce::jlimit(0.02f, 0.98f, P.lfoSym[l]->load());
            ph = ph < sym ? 0.5f * ph / sym : 0.5f + 0.5f * (ph - sym) / (1.0f - sym);

            float val;
            switch (shape)
            {
                case 1:  val = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
                case 2:  val = ph < 0.5f ? 1.0f : -1.0f; break;
                case 3:  val = 2.0f * ph - 1.0f; break;
                case 4:  val = s.sh; break;
                default: val = std::sin(2.0f * kPi * ph); break;
            }
            gsrc[1 + l] = val * P.lfoDepth[l]->load();
        }

        // Step sequencer (global)
        const int steps = juce::jlimit(1, 16, (int) P.seqSteps->load());
        const double stepHz = 0.5 * std::pow(2.0, (double) P.seqRate->load() * 6.0);
        seq.phase += stepHz * dt;
        while (seq.phase >= 1.0) { seq.phase -= 1.0; seq.step = (seq.step + 1) % steps; }
        if (seq.step >= steps) seq.step = 0;
        const float target = P.seqStep[seq.step]->load();
        const float tc = 0.002f + P.seqSmooth->load() * 0.25f;
        seq.cur += (target - seq.cur) * (1.0f - std::exp(-(float) dt / tc));
        gsrc[9] = seq.cur * P.seqDepth->load();

        // XY pad and macros
        gsrc[10] = (P.xyX->load() - 0.5f) * 2.0f;
        for (int m = 0; m < 8; ++m)
            gsrc[11 + m] = P.macro[m]->load();

        // Arpeggiator
        updateArp(dt);

        // Modulation of the global effects (envelope sources do not apply here)
        const float envZero[4] = { 0, 0, 0, 0 };
        float gm[10];
        computeMod(envZero, gm);
        revAdd = gm[7] * 0.5f;
        delAdd = gm[8] * 0.5f;
        distAdd = gm[9] * 0.5f;
    }

    void updateArp(double dt)
    {
        const auto& P = *params;
        const int mode = (int) P.arpMode->load();

        if (mode != lastArpMode)
        {
            if (mode == 0 && arpNote >= 0) { releaseNote(arpNote); arpNote = -1; }
            if (mode > 0 && lastArpMode == 0) { arpTimer = 1.0e9; arpIdx = 0; arpDir = 1; }
            lastArpMode = mode;
        }

        if (mode > 0 && numHeld > 0)
        {
            const double stepsPerSec = 2.0 * std::pow(2.0, (double) P.arpRate->load() * 4.0);
            arpTimer += dt;
            if (arpTimer >= 1.0 / stepsPerSec)
            {
                arpTimer = 0.0;
                stepArp(mode);
            }
        }
    }

    void stepArp(int mode)
    {
        int seqBuf[64 * 4];
        int sz = 0;
        const int octs = juce::jlimit(1, 4, (int) params->arpOct->load());
        for (int o = 0; o < octs; ++o)
            for (int i = 0; i < numHeld; ++i)
                seqBuf[sz++] = juce::jmin(127, held[i] + 12 * o);
        if (sz == 0)
            return;

        int pn;
        if (mode == 1)
        {
            pn = seqBuf[arpIdx % sz];
            arpIdx = (arpIdx + 1) % sz;
        }
        else if (mode == 2)
        {
            pn = seqBuf[sz - 1 - (arpIdx % sz)];
            arpIdx = (arpIdx + 1) % sz;
        }
        else if (mode == 3)
        {
            arpIdx = juce::jlimit(0, sz - 1, arpIdx);
            pn = seqBuf[arpIdx];
            if (sz > 1)
            {
                arpIdx += arpDir;
                if (arpIdx >= sz)     { arpDir = -1; arpIdx = sz - 2; }
                else if (arpIdx < 0)  { arpDir = 1;  arpIdx = 1; }
            }
        }
        else
            pn = seqBuf[rng.nextInt(sz)];

        if (arpNote >= 0)
            releaseNote(arpNote);
        triggerVoice(pn, arpVel);
        arpNote = pn;
    }

    // Mod matrix targets: 0 Cutoff, 1 Resonance, 2 Drive, 3 Pitch, 4 Pan, 5 Volume, 6 Warp, 7 Reverb, 8 Delay, 9 Distortion
    void computeMod(const float* envV, float* mod) const
    {
        const auto& P = *params;
        for (int i = 0; i < 10; ++i)
            mod[i] = 0.0f;

        for (int s = 0; s < 6; ++s)
        {
            const int src = (int) P.modSrc[s]->load();
            if (src <= 0 || src > 18)
                continue;
            const float amt = P.modAmt[s]->load();
            if (amt == 0.0f)
                continue;

            const float val = (src >= 5 && src <= 8) ? envV[src - 5] : gsrc[src];
            const int dst = juce::jlimit(0, 9, (int) P.modDst[s]->load());
            mod[dst] += amt * val;
        }

        const int xt = juce::jlimit(0, 9, (int) P.xyXT->load());
        const int yt = juce::jlimit(0, 9, (int) P.xyYT->load());
        mod[xt] += (P.xyX->load() - 0.5f) * 2.0f;
        mod[yt] += (P.xyY->load() - 0.5f) * 2.0f;
    }

    void renderVoice(Voice& v, float* L, float* R, int n)
    {
        const auto& P = *params;
        const auto& bank = WavetableBank::get();
        const float srf = (float) sr;

        // Envelopes
        float envShaped[4];
        for (int e = 0; e < 4; ++e)
        {
            const float lvl = v.env[e].advance(n, P.envA[e]->load(), P.envD[e]->load(),
                                               P.envS[e]->load(), P.envR[e]->load(), srf);
            envShaped[e] = shapeEnv(lvl, P.envCurve[e]->load());
        }

        float mod[10];
        computeMod(envShaped, mod);

        // Glide
        const float glideT = juce::jmax(P.glide->load(), P.porta->load());
        if (glideT > 0.0005f)
            v.curNote += (v.targetNote - v.curNote) * (1.0f - std::exp(-(float) n / (srf * glideT * 0.3f)));
        else
            v.curNote = v.targetNote;

        const float baseNote = v.curNote + pitchBend + mod[3] * 12.0f;

        // Filter
        const int fType = (int) P.fType->load();
        float cutoff = P.fCut->load() * std::exp2(mod[0] * 4.0f + P.fKey->load() * (v.curNote - 60.0f) / 12.0f);
        cutoff = juce::jlimit(20.0f, juce::jmin(20000.0f, 0.45f * srf), cutoff);
        const float reso = juce::jlimit(0.0f, 1.0f, P.fReso->load() + mod[1] * 0.5f);
        const float drive = juce::jlimit(0.0f, 1.0f, P.fDrive->load() + mod[2] * 0.5f);
        const float q = 0.7f + reso * reso * 11.0f;
        v.fl.setCoeffs(cutoff, q, srf);
        v.fr.setCoeffs(cutoff, q, srf);
        const float pre = 1.0f + drive * 6.0f;
        const float post = 1.0f / (1.0f + drive * 0.8f);

        // Oscillator set-up
        const int nUni = juce::jlimit(1, 8, (int) P.uniVoices->load());
        const float uniDet = P.uniDetune->load();
        const float uniSpr = P.uniSpread->load();

        struct OscSetup
        {
            bool on = false;
            float inc[8] = {}, gl[8] = {}, gr[8] = {};
            int mip[8] = {};
            float wave = 0.0f, k = 0.5f;
        } os[3];

        for (int o = 0; o < 3; ++o)
        {
            const float level = P.oscLevel[o]->load();
            if (level <= 0.001f)
                continue;

            os[o].on = true;
            os[o].wave = P.oscWave[o]->load();
            const float warp = juce::jlimit(0.0f, 1.0f, P.oscWarp[o]->load() + mod[6] * 0.5f);
            os[o].k = 0.5f - warp * 0.45f;

            const float baseSemi = baseNote + P.oscOctave[o]->load() * 12.0f + P.oscTune[o]->load() / 100.0f;
            const float gain = level * 0.3f / std::sqrt((float) nUni);

            for (int u = 0; u < nUni; ++u)
            {
                const float off = nUni > 1 ? ((float) u / (float) (nUni - 1)) * 2.0f - 1.0f : 0.0f;
                const float semis = baseSemi + off * uniDet * 0.5f;
                const float freq = 440.0f * std::exp2((semis - 69.0f) / 12.0f);
                const float inc = juce::jlimit(0.0f, 0.49f, freq / srf);
                os[o].inc[u] = inc;
                os[o].mip[u] = WavetableBank::mipForIncrement(inc);

                const float pan = juce::jlimit(-1.0f, 1.0f, P.oscPan[o]->load() + mod[4] + off * uniSpr);
                const float ang = (pan + 1.0f) * 0.25f * kPi;
                os[o].gl[u] = std::cos(ang) * gain;
                os[o].gr[u] = std::sin(ang) * gain;
            }
        }

        const float subLevel = P.subLevel->load() * 0.25f;
        const float noiseLevel = P.noiseLevel->load() * 0.15f;
        const float subInc = juce::jlimit(0.0f, 0.49f, 440.0f * std::exp2((baseNote - 12.0f - 69.0f) / 12.0f) / srf);

        // Sampler / granular source
        const SampleData* smp = sampleDataPtr;
        const float sampGain = P.sampLevel->load() * 0.5f;
        const int sampMode = (int) P.sampMode->load();
        const bool smpOn = smp != nullptr && smp->length > 32 && sampGain > 0.001f;
        double sRatio = 1.0, sStart = 0.0, sEnd = 0.0, sLoopLen = 1.0, gPos = 0.0;
        bool sLoop = false;
        float gInterval = 1.0f, gGain = 0.0f, gSpread = 0.0f;
        int gSize = 16;
        if (smpOn)
        {
            const double len = (double) smp->length;
            sRatio = std::exp2(((double) baseNote - (double) P.sampRoot->load()) / 12.0) * smp->sampleRate / sr;
            sEnd = juce::jmin(len - 1.0, juce::jmax(17.0, (double) juce::jlimit(0.0f, 1.0f, P.sampEnd->load()) * len));
            sStart = juce::jmin(sEnd - 16.0, (double) juce::jlimit(0.0f, 1.0f, P.sampStart->load()) * len);
            sLoopLen = sEnd - sStart;
            sLoop = P.sampLoop->load() > 0.5f;
            gSpread = P.grainSpread->load();
            gSize = juce::jmax(16, (int) (P.grainSize->load() * 0.001f * (float) sr));
            gInterval = (float) sr / juce::jmax(1.0f, P.grainDensity->load());
            const float overlap = juce::jmax(1.0f, P.grainDensity->load() * P.grainSize->load() * 0.001f);
            gGain = sampGain / std::sqrt(overlap);
            gPos = (double) P.grainPos->load() * len;
        }

        // Amplitude (linear ramp across the chunk)
        const float velGain = 0.4f + 0.6f * v.vel;
        const float volMod = juce::jlimit(0.0f, 2.0f, 1.0f + mod[5]);
        const float gStart = v.lastAmp * velGain * volMod;
        const float gEnd = envShaped[0] * velGain * volMod;
        v.lastAmp = envShaped[0];
        const float gStep = (gEnd - gStart) / (float) n;
        float g = gStart;

        for (int i = 0; i < n; ++i)
        {
            g += gStep;
            float sl = 0.0f, sr_ = 0.0f;

            for (int o = 0; o < 3; ++o)
            {
                if (! os[o].on)
                    continue;

                for (int u = 0; u < nUni; ++u)
                {
                    float ph = v.phase[o][u];
                    const float k = os[o].k;
                    const float pw = ph < k ? 0.5f * ph / k : 0.5f + 0.5f * (ph - k) / (1.0f - k);
                    const float s = bank.lookup(os[o].wave, os[o].mip[u], pw);
                    sl += s * os[o].gl[u];
                    sr_ += s * os[o].gr[u];

                    ph += os[o].inc[u];
                    if (ph >= 1.0f) ph -= 1.0f;
                    v.phase[o][u] = ph;
                }
            }

            if (subLevel > 0.0f)
            {
                const float s = bank.lookup(0.0f, 0, v.subPhase) * subLevel;
                sl += s;
                sr_ += s;
                v.subPhase += subInc;
                if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
            }

            if (noiseLevel > 0.0f)
            {
                sl += (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;
                sr_ += (rng.nextFloat() * 2.0f - 1.0f) * noiseLevel;
            }

            if (smpOn)
            {
                if (sampMode == 1)      // granular
                {
                    v.gTimer -= 1.0f;
                    if (v.gTimer <= 0.0f)
                    {
                        v.gTimer += gInterval;
                        for (auto& gr : v.grains)
                            if (! gr.on)
                            {
                                gr.on = true;
                                gr.age = 0;
                                gr.len = gSize;
                                gr.inc = (float) sRatio;
                                const double jitter = ((double) rng.nextFloat() * 2.0 - 1.0) * (double) gSpread * 0.2 * (double) smp->length;
                                gr.pos = juce::jlimit(0.0, (double) smp->length - 2.0, gPos + jitter);
                                const float pn = (rng.nextFloat() * 2.0f - 1.0f) * gSpread;
                                const float ang = (pn + 1.0f) * 0.25f * kPi;
                                gr.gl = std::cos(ang);
                                gr.gr = std::sin(ang);
                                break;
                            }
                    }

                    for (auto& gr : v.grains)
                    {
                        if (! gr.on)
                            continue;
                        const int i0 = (int) gr.pos;
                        if (i0 >= smp->length - 1) { gr.on = false; continue; }
                        const float fr = (float) (gr.pos - (double) i0);
                        const float win = 0.5f - 0.5f * std::cos(2.0f * kPi * (float) gr.age / (float) gr.len);
                        const float xl = smp->L[(size_t) i0] + fr * (smp->L[(size_t) i0 + 1] - smp->L[(size_t) i0]);
                        const float xr = smp->R[(size_t) i0] + fr * (smp->R[(size_t) i0 + 1] - smp->R[(size_t) i0]);
                        sl += xl * win * gr.gl * gGain * 1.4142f;
                        sr_ += xr * win * gr.gr * gGain * 1.4142f;
                        gr.pos += (double) gr.inc;
                        if (++gr.age >= gr.len)
                            gr.on = false;
                    }
                }
                else if (v.sOn)        // plain sample playback (also used for the Spectral tab for now)
                {
                    while (v.sPos >= sEnd)
                    {
                        if (sLoop) v.sPos -= sLoopLen;
                        else { v.sOn = false; break; }
                    }
                    if (v.sOn)
                    {
                        const int i0 = (int) v.sPos;
                        const int i1 = juce::jmin(i0 + 1, smp->length - 1);
                        const float fr = (float) (v.sPos - (double) i0);
                        sl += (smp->L[(size_t) i0] + fr * (smp->L[(size_t) i1] - smp->L[(size_t) i0])) * sampGain;
                        sr_ += (smp->R[(size_t) i0] + fr * (smp->R[(size_t) i1] - smp->R[(size_t) i0])) * sampGain;
                        v.sPos += sRatio;
                    }
                }
            }

            if (drive > 0.001f)
            {
                sl = std::tanh(sl * pre) * post;
                sr_ = std::tanh(sr_ * pre) * post;
            }

            sl = v.fl.process(sl, fType);
            sr_ = v.fr.process(sr_, fType);

            L[i] += sl * g;
            R[i] += sr_ * g;
        }

        if (v.env[0].st == Env::Idle)
            v.active = false;
    }

    // ----- Effects -----
    template <typename Proc>
    static void processSliced(Proc& proc, juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        const size_t total = block.getNumSamples();
        for (size_t pos = 0; pos < total; pos += 512)
        {
            auto sub = block.getSubBlock(pos, std::min<size_t>(512, total - pos));
            juce::dsp::ProcessContextReplacing<float> ctx(sub);
            proc.process(ctx);
        }
    }

    void processEffects(juce::AudioBuffer<float>& buffer)
    {
        const auto& P = *params;
        const int n = buffer.getNumSamples();
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
        const float srf = (float) sr;

        // Distortion
        const float dist = juce::jlimit(0.0f, 1.0f, P.distDrive->load() + distAdd);
        if (dist > 0.001f)
        {
            const float pre = 1.0f + dist * 10.0f, post = 1.0f / (1.0f + dist * 3.0f);
            for (int i = 0; i < n; ++i)
            {
                L[i] = std::tanh(L[i] * pre) * post;
                R[i] = std::tanh(R[i] * pre) * post;
            }
        }

        // EQ tilt
        const float tilt = P.eqTilt->load();
        if (std::abs(tilt) > 0.01f)
        {
            const float a = 1.0f - std::exp(-2.0f * kPi * 700.0f / srf);
            const float lowG = 1.0f - tilt * 0.6f, highG = 1.0f + tilt * 0.6f;
            for (int i = 0; i < n; ++i)
            {
                eqLowL += a * (L[i] - eqLowL);
                eqLowR += a * (R[i] - eqLowR);
                L[i] = eqLowL * lowG + (L[i] - eqLowL) * highG;
                R[i] = eqLowR * lowG + (R[i] - eqLowR) * highG;
            }
        }

        // Chorus and phaser
        const float cm = P.choMix->load();
        if (cm > 0.005f && buffer.getNumChannels() >= 2)
        {
            chorus.setMix(cm);
            processSliced(chorus, buffer);
        }

        const float pm = P.phaMix->load();
        if (pm > 0.005f && buffer.getNumChannels() >= 2)
        {
            phaser.setMix(pm);
            processSliced(phaser, buffer);
        }

        // Stereo delay with cross-feedback
        const float dm = juce::jlimit(0.0f, 1.0f, P.delMix->load() + delAdd);
        if (dm > 0.005f)
        {
            const float targetTime = juce::jlimit(0.01f, 2.0f, P.delTime->load());
            const float fb = juce::jlimit(0.0f, 0.95f, P.delFb->load());

            for (int i = 0; i < n; ++i)
            {
                delayTimeS += (targetTime - delayTimeS) * 0.001f;

                float rp = (float) delayPos - delayTimeS * srf;
                while (rp < 0.0f) rp += (float) delaySize;
                const int i0 = (int) rp % delaySize;
                const int i1 = (i0 + 1) % delaySize;
                const float fr = rp - std::floor(rp);

                const float yl = delayL[(size_t) i0] + fr * (delayL[(size_t) i1] - delayL[(size_t) i0]);
                const float yr = delayR[(size_t) i0] + fr * (delayR[(size_t) i1] - delayR[(size_t) i0]);

                delayL[(size_t) delayPos] = std::tanh(L[i] + fb * yr);
                delayR[(size_t) delayPos] = std::tanh(R[i] + fb * yl);
                delayPos = (delayPos + 1) % delaySize;

                L[i] += yl * dm;
                R[i] += yr * dm;
            }
        }

        // Reverb
        const float rm = juce::jlimit(0.0f, 1.0f, P.revMix->load() + revAdd);
        if (rm > 0.005f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = juce::jlimit(0.0f, 1.0f, P.revSize->load());
            rp.damping = 0.5f;
            rp.wetLevel = rm * 0.3f;
            rp.dryLevel = 0.5f * (1.0f - rm * 0.4f);
            rp.width = 1.0f;
            rp.freezeMode = 0.0f;
            reverb.setParameters(rp);
            reverb.processStereo(L, R, n);
        }

        // Master volume with soft clipping
        const float mv = P.master->load();
        const float gain = mv * mv * 2.6f;
        for (int i = 0; i < n; ++i)
        {
            L[i] = std::tanh(L[i] * gain);
            R[i] = std::tanh(R[i] * gain);
        }
    }

    // ----- State -----
    struct LfoState { double phase = 0.0; float sh = 0.0f; };
    struct SeqState { double phase = 0.0; int step = 0; float cur = 0.5f; };

    const ParamPtrs* params = nullptr;
    const SampleData* sampleDataPtr = nullptr;
    double sr = 44100.0, bpm = 120.0;
    juce::Random rng;

    Voice voices[maxVoices];
    std::uint64_t ageCounter = 0;
    float lastNote = -1.0f, pitchBend = 0.0f;

    LfoState lfo[4];
    SeqState seq;
    float gsrc[19] = {};
    float revAdd = 0.0f, delAdd = 0.0f, distAdd = 0.0f;

    int held[64] = {};
    int numHeld = 0;
    int arpIdx = 0, arpDir = 1, arpNote = -1, lastArpMode = 0;
    double arpTimer = 0.0;
    float arpVel = 0.8f;

    juce::Reverb reverb;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    std::vector<float> delayL, delayR;
    int delaySize = 1, delayPos = 0;
    float delayTimeS = 0.35f;
    float eqLowL = 0.0f, eqLowR = 0.0f;
};
} // namespace ux

// ===== Plugin processor =====
class UniverseXAudioProcessor : public juce::AudioProcessor
{
public:
    UniverseXAudioProcessor();
    ~UniverseXAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Universe X"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    ux::Randomizer& getRandomizer() { return randomizer; }
    std::atomic<float> uiScale { 0.75f };    // window zoom, saved with the plugin state

    // Sample loading (message thread). The audio thread reads the sample through an atomic shared_ptr.
    bool loadSampleFile(const juce::File& f);
    void clearSample();
    std::shared_ptr<const ux::SampleData> getSample() const { return std::atomic_load(&sampleData); }
    juce::String getSamplePath() const { const juce::ScopedLock sl(pathLock); return samplePath; }

private:
    ux::Randomizer randomizer;
    ux::ParamPtrs ptrs;
    ux::Engine engine;
    std::shared_ptr<const ux::SampleData> sampleData;
    juce::AudioFormatManager formatManager;
    juce::String samplePath;
    mutable juce::CriticalSection pathLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UniverseXAudioProcessor)
};


// ===== Phase 3: artwork-based editor =====
// The artwork (UniverseX_bg.png) is the window background. Invisible hit areas sit on top of
// every control; live-drawn knobs, tabs and displays cover the matching parts of the picture.
namespace uiv
{
using namespace juce;

constexpr int kW = 1536, kH = 1024;

struct UIState
{
    int osc = 0, lfo = 0, env = 0;
    String presetName { "Init" };
    float scale = 0.75f;                         // current window zoom (1.0 = 1536 x 1024)
    std::function<void(float)> requestScale;     // set by the editor
};

class UView : public Component
{
public:
    virtual void update() {}
};

static Colour sampleRing(const Image& img, float cx, float cy, float r)
{
    float rr = 0, gg = 0, bb = 0;
    int n = 0;
    for (int a = 0; a < 24; ++a)
    {
        const float t = (float) a * MathConstants<float>::twoPi / 24.0f;
        const int x = (int) (cx + std::cos(t) * r), y = (int) (cy + std::sin(t) * r);
        if (img.isValid() && x >= 0 && y >= 0 && x < img.getWidth() && y < img.getHeight())
        {
            const auto c = img.getPixelAt(x, y);
            rr += c.getFloatRed(); gg += c.getFloatGreen(); bb += c.getFloatBlue();
            ++n;
        }
    }
    return n > 0 ? Colour::fromFloatRGBA(rr / (float) n, gg / (float) n, bb / (float) n, 1.0f) : Colour(0xff0a0a20);
}

static Colour sampleEdges(const Image& img, Rectangle<int> r)
{
    float rr = 0, gg = 0, bb = 0;
    int n = 0;
    for (int y = r.getY(); y < r.getBottom(); y += 3)
        for (int side = 0; side < 2; ++side)
        {
            const int x = side == 0 ? r.getX() - 3 : r.getRight() + 3;
            if (img.isValid() && x >= 0 && y >= 0 && x < img.getWidth() && y < img.getHeight())
            {
                const auto c = img.getPixelAt(x, y);
                rr += c.getFloatRed(); gg += c.getFloatGreen(); bb += c.getFloatBlue();
                ++n;
            }
        }
    return n > 0 ? Colour::fromFloatRGBA(rr / (float) n, gg / (float) n, bb / (float) n, 1.0f) : Colour(0xff0a0a20);
}

static Colour sampleArea(const Image& img, Rectangle<int> r)
{
    float rr = 0, gg = 0, bb = 0;
    int n = 0;
    for (int y = r.getY(); y < r.getBottom(); y += 2)
        for (int x = r.getX(); x < r.getRight(); x += 2)
            if (img.isValid() && x >= 0 && y >= 0 && x < img.getWidth() && y < img.getHeight())
            {
                const auto c = img.getPixelAt(x, y);
                rr += c.getFloatRed(); gg += c.getFloatGreen(); bb += c.getFloatBlue();
                ++n;
            }
    return n > 0 ? Colour::fromFloatRGBA(rr / (float) n, gg / (float) n, bb / (float) n, 1.0f) : Colour(0xff0a0a20);
}

// Shared access to the plugin: parameters, randomizer, artwork and UI state.
struct Ctx
{
    Ctx(UniverseXAudioProcessor& p, const Image& b, UIState& s) : proc(p), bg(b), st(s) {}

    RangedAudioParameter* P(const String& id) const { return proc.apvts.getParameter(id); }

    float raw(const String& id) const
    {
        auto* v = proc.apvts.getRawParameterValue(id);
        return v != nullptr ? v->load() : 0.0f;
    }

    void setNorm(RangedAudioParameter* p, float n) const
    {
        if (p == nullptr) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(jlimit(0.0f, 1.0f, n));
        p->endChangeGesture();
    }

    void setReal(const String& id, float v) const
    {
        if (auto* p = P(id)) setNorm(p, p->convertTo0to1(v));
    }

    void setChoice(const String& id, int idx) const { setReal(id, (float) idx); }

    UniverseXAudioProcessor& proc;
    const Image& bg;
    UIState& st;
};

static float hashIds(const Ctx& c, std::initializer_list<String> ids)
{
    float s = 0.0f, w = 1.0f;
    for (const auto& id : ids) { s += c.raw(id) * w; w += 1.37f; }
    return s;
}

// ---------------------------------------------------------------------------
class KnobView : public UView
{
public:
    KnobView(Ctx& c, Point<float> centre, bool dimWhenNull = false, int clipBelowCentre = 21)
        : cx(c), dim(dimWhenNull), clipY(58 + clipBelowCentre)
    {
        setBounds((int) centre.x - 60, (int) centre.y - 58, 120, 92);
        ring = sampleRing(c.bg, centre.x, centre.y, 35.0f);
    }

    void setParam(RangedAudioParameter* p) { param = p; shown = -9.0f; repaint(); }

    void update() override
    {
        const float v = param != nullptr ? param->getValue() : -1.0f;
        if (v != shown) { shown = v; repaint(); }
    }

    bool hitTest(int x, int y) override
    {
        return param != nullptr && Point<float>((float) x, (float) y).getDistanceFrom(Point<float>(60.0f, 58.0f)) <= 30.0f;
    }

    void paint(Graphics& g) override
    {
        if (param == nullptr && ! dim)
            return;

        g.reduceClipRegion(0, 0, 120, clipY);   // keep the printed label under the knob visible
        const float px = 60.0f, py = 58.0f;
        g.setColour(ring);
        g.fillEllipse(px - 30, py - 30, 60, 60);

        const float v = param != nullptr ? param->getValue() : 0.0f;
        const float a0 = -2.356f, a1 = 2.356f, ang = a0 + v * (a1 - a0);
        const float alpha = param != nullptr ? 1.0f : 0.35f;
        const PathStrokeType thin(4.0f, PathStrokeType::curved, PathStrokeType::rounded);

        Path track;
        track.addCentredArc(px, py, 26.0f, 26.0f, 0.0f, a0, a1, true);
        g.setColour(Colour(0xff0f1c46).withAlpha(alpha));
        g.strokePath(track, thin);

        if (param != nullptr && v > 0.002f)
        {
            Path val;
            val.addCentredArc(px, py, 26.0f, 26.0f, 0.0f, a0, ang, true);
            g.setColour(Colour(0x553aa0ff));
            g.strokePath(val, PathStrokeType(9.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(Colour(0xff58c0ff));
            g.strokePath(val, thin);
        }

        ColourGradient body(Colour(0xff46579a).withAlpha(alpha), px - 8, py - 10,
                            Colour(0xff050818).withAlpha(alpha), px + 10, py + 14, true);
        g.setGradientFill(body);
        g.fillEllipse(px - 20, py - 20, 40, 40);
        g.setColour(Colour(0xff8aa0ff).withAlpha(0.55f * alpha));
        g.drawEllipse(px - 20, py - 20, 40, 40, 1.3f);

        if (param != nullptr)
        {
            const float dx = std::sin(ang), dy = -std::cos(ang);
            g.setColour(Colours::white);
            g.drawLine(px + dx * 7, py + dy * 7, px + dx * 17, py + dy * 17, 2.6f);
        }

        if (dragging && param != nullptr)
        {
            const String t = param->getName(16) + "  " + param->getCurrentValueAsText();
            const Font f(15.0f, Font::bold);
            const float w = f.getStringWidthFloat(t) + 16.0f;
            const Rectangle<float> r(px - w / 2.0f, 2.0f, w, 22.0f);
            g.setColour(Colour(0xf0101a40));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(Colour(0xff58c0ff));
            g.drawRoundedRectangle(r, 6.0f, 1.2f);
            g.setColour(Colours::white);
            g.setFont(f);
            g.drawText(t, r, Justification::centred);
        }
    }

    void mouseDown(const MouseEvent&) override
    {
        if (param == nullptr) return;
        dragging = true;
        startV = param->getValue();
        param->beginChangeGesture();
        repaint();
    }

    void mouseDrag(const MouseEvent& e) override
    {
        if (param == nullptr) return;
        const float s = e.mods.isShiftDown() ? 900.0f : 220.0f;
        param->setValueNotifyingHost(jlimit(0.0f, 1.0f, startV - (float) e.getDistanceFromDragStartY() / s));
        repaint();
    }

    void mouseUp(const MouseEvent&) override
    {
        if (param == nullptr) return;
        param->endChangeGesture();
        dragging = false;
        repaint();
    }

    void mouseDoubleClick(const MouseEvent&) override
    {
        if (param == nullptr) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getDefaultValue());
        param->endChangeGesture();
    }

    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) { Component::mouseWheelMove(e, w); return; }
        if (param == nullptr) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost(jlimit(0.0f, 1.0f, param->getValue() + w.deltaY * 0.15f));
        param->endChangeGesture();
    }

private:
    Ctx& cx;
    bool dim;
    int clipY;
    Colour ring;
    RangedAudioParameter* param = nullptr;
    float shown = -9.0f, startV = 0.0f;
    bool dragging = false;
};

// ---------------------------------------------------------------------------
class TabView : public UView
{
public:
    TabView(Ctx&, std::vector<Rectangle<int>> rects, StringArray names,
            std::function<int()> getter, std::function<void(int)> setter)
        : labels(names), get(std::move(getter)), set(std::move(setter))
    {
        Rectangle<int> u = rects[0];
        for (auto& r : rects) u = u.getUnion(r);
        setBounds(u.expanded(2));
        for (auto& r : rects) local.push_back(r.translated(-getX(), -getY()));
    }

    void update() override
    {
        const int s = get();
        if (s != shown) { shown = s; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const int sel = get();
        for (int i = 0; i < (int) local.size(); ++i)
        {
            const auto r = local[(size_t) i].toFloat().expanded(1.0f);
            if (i == sel)
            {
                g.setGradientFill(ColourGradient(Colour(0xff4aa0ff), r.getX(), r.getY(), Colour(0xff0f48d8), r.getX(), r.getBottom(), false));
                g.fillRoundedRectangle(r, 7.0f);
                g.setColour(Colour(0xff7fc4ff));
                g.drawRoundedRectangle(r, 7.0f, 1.5f);
            }
            else
            {
                g.setColour(Colour(0xff0a1030));
                g.fillRoundedRectangle(r, 7.0f);
                g.setColour(Colour(0xff26357a));
                g.drawRoundedRectangle(r, 7.0f, 1.2f);
            }
            g.setColour(i == sel ? Colours::white : Colour(0xffe2e8ff));
            g.setFont(Font(15.0f));
            g.drawText(labels[i], r, Justification::centred);
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        for (int i = 0; i < (int) local.size(); ++i)
            if (local[(size_t) i].contains(e.getPosition())) { set(i); repaint(); return; }
    }

private:
    std::vector<Rectangle<int>> local;
    StringArray labels;
    std::function<int()> get;
    std::function<void(int)> set;
    int shown = -1;
};

// ---------------------------------------------------------------------------
class DisplayView : public UView
{
public:
    DisplayView(Ctx& c, Rectangle<int> area, std::function<float()> stateFn) : cx(c), state(std::move(stateFn))
    {
        setBounds(area);
        setInterceptsMouseClicks(false, false);
    }

    void update() override
    {
        const float s = state();
        if (s != last) { last = s; repaint(); }
    }

protected:
    void cover(Graphics& g) const
    {
        const auto r = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff010110), 0, 0, Colour(0xff04123a), 0, r.getHeight(), false));
        g.fillRoundedRectangle(r.reduced(0.5f), 8.0f);
    }

    static void glowStroke(Graphics& g, const Path& p, Colour c)
    {
        g.setColour(c.withAlpha(0.18f));
        g.strokePath(p, PathStrokeType(7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(c.withAlpha(0.55f));
        g.strokePath(p, PathStrokeType(3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour(c.brighter(0.6f));
        g.strokePath(p, PathStrokeType(1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    Ctx& cx;

private:
    std::function<float()> state;
    float last = -1.0e9f;
};

class WaveDisplay : public DisplayView
{
public:
    WaveDisplay(Ctx& c, Rectangle<int> a)
        : DisplayView(c, a, [&c] { return (float) c.st.osc * 100.0f + hashIds(c, { "osc1_wave", "osc1_warp", "osc2_wave", "osc2_warp", "osc3_wave", "osc3_warp" }); }) {}

    void paint(Graphics& g) override
    {
        cover(g);
        const float w = (float) getWidth(), h = (float) getHeight();
        const int o = cx.st.osc;
        const int N = 240;

        for (int layer = 0; layer < 3; ++layer)
        {
            Path p;
            for (int i = 0; i < N; ++i)
            {
                const float x = (float) i * w / (float) (N - 1);
                float phase = std::fmod(x / w * 2.5f + (float) layer * 0.03f, 1.0f);
                float s;
                if (o <= 2)
                {
                    const String pre = "osc" + String(o + 1) + "_";
                    const float k = 0.5f - cx.raw(pre + "warp") * 0.45f;
                    const float pw = phase < k ? 0.5f * phase / k : 0.5f + 0.5f * (phase - k) / (1.0f - k);
                    s = ux::WavetableBank::get().lookup(cx.raw(pre + "wave"), 0, pw);
                }
                else if (o == 3)
                    s = std::sin(2.0f * MathConstants<float>::pi * phase);
                else
                {
                    float n = std::sin((float) i * 12.9898f + (float) layer) * 43758.5453f;
                    n -= std::floor(n);
                    s = (n * 2.0f - 1.0f) * 0.7f;
                }
                const float y = h * 0.5f - s * h * (0.36f - 0.07f * (float) layer);
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            glowStroke(g, p, layer == 0 ? Colour(0xff2f9bff) : Colour(0xff6a5cff));
        }
    }
};

class FilterDisplay : public DisplayView
{
public:
    FilterDisplay(Ctx& c, Rectangle<int> a)
        : DisplayView(c, a, [&c] { return hashIds(c, { "filter_type", "filter_cutoff", "filter_reso" }); }) {}

    void paint(Graphics& g) override
    {
        cover(g);
        const float w = (float) getWidth(), h = (float) getHeight();
        const int type = (int) cx.raw("filter_type");
        const float cutoff = jmax(20.0f, cx.raw("filter_cutoff"));
        const float reso = cx.raw("filter_reso");
        const float k = 1.0f / (0.7f + reso * reso * 11.0f);

        Path line;
        const int N = 200;
        for (int i = 0; i < N; ++i)
        {
            const float f = 20.0f * std::pow(1000.0f, (float) i / (float) (N - 1));
            const float wv = f / cutoff, w2 = wv * wv;
            const float den = std::sqrt((1.0f - w2) * (1.0f - w2) + wv * k * wv * k);
            float mag;
            switch (type) { case 1: mag = wv * k / den; break; case 2: mag = w2 / den; break;
                            case 3: mag = std::abs(1.0f - w2) / den; break; default: mag = 1.0f / den; break; }
            const float db = 20.0f * std::log10(jmax(mag, 1.0e-5f));
            const float y = jlimit(4.0f, h - 3.0f, h * 0.68f - db * h * 0.0095f);
            const float x = (float) i * w / (float) (N - 1);
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }

        Path fill(line);
        fill.lineTo(w, h);
        fill.lineTo(0, h);
        fill.closeSubPath();
        g.setGradientFill(ColourGradient(Colour(0x552a7fff), 0, 0, Colour(0x002a7fff), 0, h, false));
        g.fillPath(fill);
        glowStroke(g, line, Colour(0xff2f9bff));
    }
};

class LfoDisplay : public DisplayView
{
public:
    LfoDisplay(Ctx& c, Rectangle<int> a)
        : DisplayView(c, a, [&c]
          {
              const String p = "lfo" + String(c.st.lfo + 1) + "_";
              return (float) c.st.lfo * 100.0f + hashIds(c, { p + "shape", p + "depth", p + "phase", p + "symmetry" });
          }) {}

    void paint(Graphics& g) override
    {
        cover(g);
        const float w = (float) getWidth(), h = (float) getHeight();
        const String p = "lfo" + String(cx.st.lfo + 1) + "_";
        const int shape = (int) cx.raw(p + "shape");
        const float depth = cx.raw(p + "depth"), phOff = cx.raw(p + "phase");
        const float sym = jlimit(0.02f, 0.98f, cx.raw(p + "symmetry"));

        Path line;
        const int N = 300;
        for (int i = 0; i < N; ++i)
        {
            const float cyc = (float) i / (float) (N - 1) * 2.0f;
            float ph = cyc + phOff;
            const float whole = std::floor(ph);
            ph -= whole;
            ph = ph < sym ? 0.5f * ph / sym : 0.5f + 0.5f * (ph - sym) / (1.0f - sym);
            float v;
            switch (shape)
            {
                case 1:  v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
                case 2:  v = ph < 0.5f ? 1.0f : -1.0f; break;
                case 3:  v = 2.0f * ph - 1.0f; break;
                case 4:  { float n = std::sin(whole * 78.233f) * 43758.5453f; n -= std::floor(n); v = n * 2.0f - 1.0f; break; }
                default: v = std::sin(2.0f * MathConstants<float>::pi * ph); break;
            }
            const float x = (float) i * w / (float) (N - 1);
            const float y = h * 0.5f - v * h * 0.40f * (0.25f + 0.75f * depth);
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        glowStroke(g, line, Colour(0xff2f9bff));
    }
};

class EnvDisplay : public DisplayView
{
public:
    EnvDisplay(Ctx& c, Rectangle<int> a)
        : DisplayView(c, a, [&c]
          {
              const String p = "env" + String(c.st.env + 1) + "_";
              return (float) c.st.env * 100.0f + hashIds(c, { p + "a", p + "d", p + "s", p + "r", p + "curve" });
          }) {}

    void paint(Graphics& g) override
    {
        cover(g);
        const float w = (float) getWidth() - 20.0f, h = (float) getHeight() - 16.0f;
        const String p = "env" + String(cx.st.env + 1) + "_";
        const float a = std::sqrt(cx.raw(p + "a")), d = std::sqrt(cx.raw(p + "d")), r = std::sqrt(cx.raw(p + "r"));
        const float s = cx.raw(p + "s"), curve = cx.raw(p + "curve");
        const float hold = 0.5f * (a + d + r) * 0.4f + 0.2f;
        const float total = a + d + hold + r;

        const float xa = 10.0f + w * a / total, xd = xa + w * d / total, xs = xd + w * hold / total, xr = 10.0f + w;
        auto yOf = [&](float level) { return 8.0f + h * (1.0f - ux::shapeEnv(level, curve)); };

        Path line;
        const int N = 160;
        for (int i = 0; i < N; ++i)
        {
            const float x = 10.0f + w * (float) i / (float) (N - 1);
            float lvl;
            if (x < xa)       lvl = (x - 10.0f) / jmax(1.0f, xa - 10.0f);
            else if (x < xd)  lvl = 1.0f - (1.0f - s) * (x - xa) / jmax(1.0f, xd - xa);
            else if (x < xs)  lvl = s;
            else              lvl = s * (1.0f - (x - xs) / jmax(1.0f, xr - xs));
            const float y = yOf(jlimit(0.0f, 1.0f, lvl));
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }

        Path fill(line);
        fill.lineTo(xr, 8.0f + h);
        fill.lineTo(10.0f, 8.0f + h);
        fill.closeSubPath();
        g.setGradientFill(ColourGradient(Colour(0x886a2cff), 0, 0, Colour(0x116a2cff), 0, (float) getHeight(), false));
        g.fillPath(fill);
        glowStroke(g, line, Colour(0xffa070ff));

        const float pts[4][2] = { { 10.0f, 8.0f + h }, { xa, yOf(1.0f) }, { xd, yOf(s) }, { xr, 8.0f + h } };
        for (auto& q : pts)
        {
            g.setColour(Colour(0xffe8dcff));
            g.fillEllipse(q[0] - 3.5f, q[1] - 3.5f, 7.0f, 7.0f);
        }
    }
};

// ---------------------------------------------------------------------------
class StepSeqView : public UView
{
public:
    explicit StepSeqView(Ctx& c) : cx(c) { setBounds(405, 616, 338, 78); }

    void update() override
    {
        float s = cx.raw("seq_steps") * 7.0f;
        for (int i = 0; i < 16; ++i) s += cx.raw("seq_step" + String(i + 1)) * (float) (i + 1);
        if (s != last) { last = s; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff010108), 0, 0, Colour(0xff0a0526), 0, r.getHeight(), false));
        g.fillRoundedRectangle(r.reduced(0.5f), 6.0f);

        const int steps = (int) cx.raw("seq_steps");
        const float slot = (r.getWidth() - 8.0f) / 16.0f;
        for (int i = 0; i < 16; ++i)
        {
            const float v = cx.raw("seq_step" + String(i + 1));
            const float bh = jmax(3.0f, v * (r.getHeight() - 12.0f));
            const Rectangle<float> b(4.0f + (float) i * slot + 2.0f, r.getHeight() - 5.0f - bh, slot - 4.0f, bh);
            const float a = i < steps ? 1.0f : 0.28f;
            g.setGradientFill(ColourGradient(Colour(0xffff4af0).withAlpha(a), 0, b.getY(), Colour(0xff6a2cff).withAlpha(a), 0, b.getBottom(), false));
            g.fillRoundedRectangle(b, 2.5f);
            g.setColour(Colour(0xffffb0ff).withAlpha(0.5f * a));
            g.drawRoundedRectangle(b, 2.5f, 1.0f);
        }
    }

    void mouseDown(const MouseEvent& e) override { edit(e); }
    void mouseDrag(const MouseEvent& e) override { edit(e); }

private:
    void edit(const MouseEvent& e)
    {
        const float slot = ((float) getWidth() - 8.0f) / 16.0f;
        const int i = jlimit(0, 15, (int) (((float) e.x - 4.0f) / slot));
        const float v = 1.0f - ((float) e.y - 6.0f) / ((float) getHeight() - 12.0f);
        cx.setNorm(cx.P("seq_step" + String(i + 1)), v);
    }

    Ctx& cx;
    float last = -1.0f;
};

class StepsStepper : public UView
{
public:
    explicit StepsStepper(Ctx& c) : cx(c), cover(sampleEdges(c.bg, { 438, 694, 116, 18 })) { setBounds(408, 692, 330, 22); }

    void update() override { const int s = (int) cx.raw("seq_steps"); if (s != shown) { shown = s; repaint(); } }

    void paint(Graphics& g) override
    {
        g.setColour(cover);
        g.fillRect(34, 1, 120, 20);
        g.setColour(Colour(0xff58d8ff));
        g.setFont(Font(15.0f));
        g.drawText("Steps: " + String((int) cx.raw("seq_steps")), 34, 1, 120, 20, Justification::centred);
    }

    void mouseDown(const MouseEvent& e) override
    {
        const int s = (int) cx.raw("seq_steps");
        if (e.x < 40)       cx.setReal("seq_steps", (float) jlimit(1, 16, s - 1));
        else if (e.x > 290) cx.setReal("seq_steps", (float) jlimit(1, 16, s + 1));
    }

    bool hitTest(int x, int) override { return x < 40 || x > 290; }

private:
    Ctx& cx;
    Colour cover;
    int shown = -1;
};

// ---------------------------------------------------------------------------
class XYPadView : public UView
{
public:
    explicit XYPadView(Ctx& c) : cx(c) { setBounds(604, 386, 506, 162); }

    void update() override
    {
        const float s = cx.raw("xy_x") * 3.0f + cx.raw("xy_y");
        if (s != last) { last = s; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const float x = cx.raw("xy_x") * (float) getWidth(), y = (1.0f - cx.raw("xy_y")) * (float) getHeight();
        g.setColour(Colour(0x889fd8ff));
        g.drawLine(x, 0.0f, x, (float) getHeight(), 1.0f);
        g.drawLine(0.0f, y, (float) getWidth(), y, 1.0f);
        g.setGradientFill(ColourGradient(Colour(0x883ac8ff), x, y, Colour(0x003ac8ff), x + 18.0f, y, true));
        g.fillEllipse(x - 18.0f, y - 18.0f, 36.0f, 36.0f);
        g.setColour(Colour(0xff31c6ff));
        g.fillEllipse(x - 7.5f, y - 7.5f, 15.0f, 15.0f);
        g.setColour(Colours::white);
        g.fillEllipse(x - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }

    void mouseDown(const MouseEvent& e) override
    {
        if (auto* p = cx.P("xy_x")) p->beginChangeGesture();
        if (auto* p = cx.P("xy_y")) p->beginChangeGesture();
        move(e);
    }
    void mouseDrag(const MouseEvent& e) override { move(e); }
    void mouseUp(const MouseEvent&) override
    {
        if (auto* p = cx.P("xy_x")) p->endChangeGesture();
        if (auto* p = cx.P("xy_y")) p->endChangeGesture();
    }

private:
    void move(const MouseEvent& e)
    {
        if (auto* p = cx.P("xy_x")) p->setValueNotifyingHost(jlimit(0.0f, 1.0f, (float) e.x / (float) getWidth()));
        if (auto* p = cx.P("xy_y")) p->setValueNotifyingHost(jlimit(0.0f, 1.0f, 1.0f - (float) e.y / (float) getHeight()));
    }

    Ctx& cx;
    float last = -1.0f;
};

// ---------------------------------------------------------------------------
class ModMatrixView : public UView
{
public:
    explicit ModMatrixView(Ctx& c) : cx(c) { setBounds(1146, 432, 340, 152); }

    void update() override
    {
        float s = 0.0f;
        for (int i = 1; i <= 6; ++i)
        {
            const String p = "mod" + String(i) + "_";
            s += (cx.raw(p + "src") * 3.0f + cx.raw(p + "dst") * 5.0f + cx.raw(p + "amt") * 7.0f) * (float) i;
        }
        if (s != last) { last = s; repaint(); }
    }

    void paint(Graphics& g) override
    {
        for (int i = 0; i < 6; ++i)
        {
            const String p = "mod" + String(i + 1) + "_";
            const int src = (int) cx.raw(p + "src"), dst = (int) cx.raw(p + "dst");
            const float amt = cx.raw(p + "amt");
            const auto r = rowRect(i);

            g.setGradientFill(ColourGradient(Colour(0xff0a1134), 0, r.getY(), Colour(0xff03061a), 0, r.getBottom(), false));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(Colour(0xff2a3c88));
            g.drawRoundedRectangle(r, 6.0f, 1.0f);

            g.setFont(Font(15.0f));
            g.setColour(src == 0 ? Colour(0xff7480b0) : Colours::white);
            g.drawText(choiceName(p + "src", src), (int) r.getX() + 14, (int) r.getY(), 100, (int) r.getHeight(), Justification::centredLeft);
            g.setColour(Colour(0xffb0bcff));
            g.drawText(String::charToString((juce_wchar) 0x2192), (int) r.getX() + 118, (int) r.getY(), 24, (int) r.getHeight(), Justification::centred);
            g.setColour(Colours::white);
            g.drawText(choiceName(p + "dst", dst), (int) r.getX() + 148, (int) r.getY(), 100, (int) r.getHeight(), Justification::centredLeft);
            g.drawText(String(amt >= 0.0f ? "+" : "") + String(amt * 100.0f, 1) + " %", (int) r.getX() + 236, (int) r.getY(), 84, (int) r.getHeight(), Justification::centredRight);
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        dragRow = -1;
        const int row = (int) (((float) e.y - 1.0f) / 24.6f);
        if (row < 0 || row > 5) return;
        const String p = "mod" + String(row + 1) + "_";

        if (e.x < 120)        popup(p + "src");
        else if (e.x < 250)   popup(p + "dst");
        else if (auto* a = cx.P(p + "amt"))
        {
            dragRow = row;
            startV = a->getValue();
            a->beginChangeGesture();
        }
    }

    void mouseDrag(const MouseEvent& e) override
    {
        if (dragRow < 0) return;
        if (auto* a = cx.P("mod" + String(dragRow + 1) + "_amt"))
            a->setValueNotifyingHost(jlimit(0.0f, 1.0f, startV + (float) e.getDistanceFromDragStartX() / 220.0f));
    }

    void mouseUp(const MouseEvent&) override
    {
        if (dragRow < 0) return;
        if (auto* a = cx.P("mod" + String(dragRow + 1) + "_amt")) a->endChangeGesture();
        dragRow = -1;
    }

    void mouseDoubleClick(const MouseEvent& e) override
    {
        const int row = (int) (((float) e.y - 1.0f) / 24.6f);
        if (row >= 0 && row <= 5 && e.x >= 250)
            cx.setReal("mod" + String(row + 1) + "_amt", 0.0f);
    }

private:
    static Rectangle<float> rowRect(int i) { return { 2.0f, 1.0f + (float) i * 24.6f, 336.0f, 23.0f }; }

    String choiceName(const String& id, int idx) const
    {
        if (auto* ch = dynamic_cast<AudioParameterChoice*>(cx.P(id)))
            return ch->choices[jlimit(0, ch->choices.size() - 1, idx)];
        return {};
    }

    void popup(const String& id)
    {
        auto* ch = dynamic_cast<AudioParameterChoice*>(cx.P(id));
        if (ch == nullptr) return;
        const int cur = (int) cx.raw(id);
        PopupMenu m;
        for (int i = 0; i < ch->choices.size(); ++i)
            m.addItem(i + 1, ch->choices[i], true, i == cur);
        Ctx* c = &cx;
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [c, id](int r) { if (r > 0) c->setChoice(id, r - 1); });
    }

    Ctx& cx;
    float last = -1.0f, startV = 0.0f;
    int dragRow = -1;
};

// ---------------------------------------------------------------------------
class ArpView : public UView
{
public:
    explicit ArpView(Ctx& c) : cx(c) { setBounds(924, 622, 84, 86); }

    void update() override { const int m = (int) cx.raw("arp_mode"); if (m != shown) { shown = m; repaint(); } }

    void paint(Graphics& g) override
    {
        const int mode = (int) cx.raw("arp_mode");
        for (int i = 0; i < 4; ++i)
        {
            const float cy = 11.0f + 21.0f * (float) i, px = 10.0f;
            g.setColour(Colour(0xff02030f));
            g.fillEllipse(px - 10.0f, cy - 10.0f, 20.0f, 20.0f);
            g.setColour(Colour(0xffa9b8ff));
            g.drawEllipse(px - 6.5f, cy - 6.5f, 13.0f, 13.0f, 1.8f);
            if (mode == i + 1)
            {
                g.setColour(Colour(0x66b8a0ff));
                g.fillEllipse(px - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                g.setColour(Colour(0xffe0d0ff));
                g.fillEllipse(px - 3.6f, cy - 3.6f, 7.2f, 7.2f);
            }
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        const int row = jlimit(0, 3, (int) (((float) e.y - 1.0f) / 21.0f));
        const int mode = (int) cx.raw("arp_mode");
        cx.setChoice("arp_mode", mode == row + 1 ? 0 : row + 1);
    }

private:
    Ctx& cx;
    int shown = -1;
};

class PolyView : public UView
{
public:
    explicit PolyView(Ctx& c) : cx(c) { setBounds(1395, 733, 102, 25); }

    void update() override { const int p = (int) cx.raw("polyphony"); if (p != shown) { shown = p; repaint(); } }

    void paint(Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(Colour(0xff0d1233));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(Colour(0xff2f3f94));
        g.drawRoundedRectangle(r, 7.0f, 1.2f);
        g.setColour(Colours::white);
        g.setFont(Font(16.0f, Font::bold));
        g.drawText(String((int) cx.raw("polyphony")), getLocalBounds(), Justification::centred);
        g.setFont(Font(17.0f));
        g.drawText(String::charToString((juce_wchar) 0x2039), 0, 0, 28, getHeight(), Justification::centred);
        g.drawText(String::charToString((juce_wchar) 0x203A), getWidth() - 28, 0, 28, getHeight(), Justification::centred);
    }

    void mouseDown(const MouseEvent& e) override
    {
        const int p = (int) cx.raw("polyphony");
        if (e.x < 30)                  cx.setReal("polyphony", (float) jlimit(1, 32, p - 1));
        else if (e.x > getWidth() - 30) cx.setReal("polyphony", (float) jlimit(1, 32, p + 1));
    }

private:
    Ctx& cx;
    int shown = -1;
};

class SyncToggle : public UView
{
public:
    explicit SyncToggle(Ctx& c) : cx(c) { setBounds(507, 511, 44, 34); }

    void update() override
    {
        const float s = cx.raw("lfo" + String(cx.st.lfo + 1) + "_sync") + (float) cx.st.lfo * 2.0f;
        if (s != last) { last = s; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const bool on = cx.raw("lfo" + String(cx.st.lfo + 1) + "_sync") > 0.5f;
        const auto r = getLocalBounds().toFloat().reduced(4.0f);
        g.setColour(Colour(0xff03040f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setGradientFill(on ? ColourGradient(Colour(0xffa86bff), 0, r.getY(), Colour(0xff5a2ad8), 0, r.getBottom(), false)
                             : ColourGradient(Colour(0xff1a1c48), 0, r.getY(), Colour(0xff0b0c26), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(on ? Colour(0xffd8b8ff) : Colour(0xff3a3f8a));
        g.drawRoundedRectangle(r, 7.0f, 1.5f);
        g.setColour(on ? Colours::white : Colour(0xff6a70c0));
        g.fillEllipse(r.getCentreX() - 4.5f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
    }

    void mouseDown(const MouseEvent&) override
    {
        const String id = "lfo" + String(cx.st.lfo + 1) + "_sync";
        cx.setChoice(id, cx.raw(id) > 0.5f ? 0 : 1);
    }

private:
    Ctx& cx;
    float last = -1.0f;
};

class LabelPatch : public UView
{
public:
    LabelPatch(Ctx& c, Rectangle<int> a, String t) : text(std::move(t)), col(sampleEdges(c.bg, a))
    {
        setBounds(a);
        setInterceptsMouseClicks(false, false);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(col);
        g.setColour(Colours::white);
        g.setFont(Font(15.5f));
        g.drawText(text, getLocalBounds(), Justification::centred);
    }

private:
    String text;
    Colour col;
};

class PresetNameView : public UView
{
public:
    PresetNameView(Ctx& c) : cx(c), col(sampleEdges(c.bg, { 628, 24, 284, 34 }))
    {
        setBounds(628, 24, 284, 34);
        setInterceptsMouseClicks(false, false);
    }

    void update() override { if (cx.st.presetName != shown) { shown = cx.st.presetName; repaint(); } }

    void paint(Graphics& g) override
    {
        g.fillAll(col);
        g.setColour(Colour(0xff58d8ff));
        g.setFont(Font(26.0f, Font::bold));
        g.drawText(cx.st.presetName, getLocalBounds(), Justification::centred);
    }

private:
    Ctx& cx;
    Colour col;
    String shown;
};

class Hotspot : public UView
{
public:
    Hotspot(Rectangle<int> a, std::function<void(Component&)> f) : fn(std::move(f)) { setBounds(a); }

    void paint(Graphics& g) override
    {
        if (down)
        {
            g.setColour(Colour(0x30ffffff));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f);
        }
    }

    void mouseDown(const MouseEvent&) override { down = true; repaint(); }
    void mouseUp(const MouseEvent& e) override
    {
        down = false;
        repaint();
        if (getLocalBounds().contains(e.getPosition()))
            fn(*this);
    }

private:
    std::function<void(Component&)> fn;
    bool down = false;
};

// ---------------------------------------------------------------------------
class LazyStrip : public UView
{
public:
    explicit LazyStrip(Ctx& c) : cx(c)
    {
        static const int xs[10][2] = { { 85, 170 }, { 187, 270 }, { 285, 372 }, { 388, 495 }, { 503, 610 },
                                       { 622, 722 }, { 736, 836 }, { 852, 955 }, { 965, 1062 }, { 1070, 1165 } };
        for (int i = 0; i < 10; ++i) rects[i] = Rectangle<int>(xs[i][0], 820, xs[i][1] - xs[i][0], 68);
        setBounds(83, 818, 1084, 72);
    }

    void update() override
    {
        bool dirty = false;
        for (int i = 0; i < 10; ++i)
            if (flash[i] > 0.0f) { flash[i] = jmax(0.0f, flash[i] - 0.12f); dirty = true; }
        for (int i = 0; i < 9; ++i)
        {
            const bool l = cx.proc.getRandomizer().isSectionLocked(ux::Randomizer::sectionForLazy(i));
            if (l != locked[i]) { locked[i] = l; dirty = true; }
        }
        if (dirty) repaint();
    }

    void paint(Graphics& g) override
    {
        for (int i = 0; i < 10; ++i)
        {
            const auto r = rects[i].translated(-getX(), -getY()).toFloat();
            if (flash[i] > 0.0f)
            {
                g.setColour(Colour(0xff7fc4ff).withAlpha(flash[i] * 0.45f));
                g.fillRoundedRectangle(r, 10.0f);
            }
            if (i < 9 && locked[i])
            {
                const float x = r.getRight() - 22.0f, y = r.getY() + 6.0f;
                g.setColour(Colour(0xffffc04d));
                Path sh;
                sh.addCentredArc(x + 7.0f, y + 6.0f, 4.5f, 5.0f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
                g.strokePath(sh, PathStrokeType(2.0f));
                g.fillRoundedRectangle(x, y + 6.0f, 14.0f, 10.0f, 2.0f);
            }
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        for (int i = 0; i < 10; ++i)
            if (rects[i].translated(-getX(), -getY()).contains(e.getPosition()))
            {
                auto& rnd = cx.proc.getRandomizer();
                if (e.mods.isPopupMenu() || e.mods.isCtrlDown() || e.mods.isCommandDown())
                {
                    if (i < 9) rnd.setSectionLocked(ux::Randomizer::sectionForLazy(i), ! rnd.isSectionLocked(ux::Randomizer::sectionForLazy(i)));
                }
                else
                {
                    rnd.lazy(i);
                    flash[i] = 1.0f;
                }
                repaint();
                return;
            }
    }

private:
    Ctx& cx;
    Rectangle<int> rects[10];
    float flash[10] = {};
    bool locked[10] = {};
};

class RandomPanel : public UView
{
public:
    explicit RandomPanel(Ctx& c) : cx(c) { setBounds(1196, 806, 256, 80); }

    void update() override
    {
        auto& r = cx.proc.getRandomizer();
        const float s = r.getAmount() * 10.0f + (r.canUndo() ? 1.0f : 0.0f) + (r.canRedo() ? 0.5f : 0.0f);
        if (s != last) { last = s; repaint(); }
    }

    void paint(Graphics& g) override
    {
        auto& r = cx.proc.getRandomizer();
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff030320), 0, 0, Colour(0xff050a30), 0, b.getHeight(), false));
        g.fillRoundedRectangle(b, 10.0f);
        g.setColour(Colour(0xff58c0ff));
        g.setFont(Font(15.0f));
        g.drawText("Click = randomize   Right-click = lock", 10, 6, 240, 20, Justification::centredLeft);
        g.setColour(Colour(0xff8aa0ff));
        g.setFont(Font(13.5f));
        g.drawText("Locked sections are skipped by Full Mix", 10, 26, 240, 18, Justification::centredLeft);

        g.setColour(Colours::white);
        g.setFont(Font(14.5f));
        g.drawText("Amount", 10, 52, 56, 20, Justification::centredLeft);
        g.setColour(Colour(0xff10204a));
        g.fillRoundedRectangle(70.0f, 60.0f, 90.0f, 5.0f, 2.5f);
        g.setColour(Colour(0xff58c0ff));
        g.fillRoundedRectangle(70.0f, 60.0f, 90.0f * r.getAmount(), 5.0f, 2.5f);
        g.setColour(Colours::white);
        g.fillEllipse(70.0f + 90.0f * r.getAmount() - 6.0f, 62.5f - 6.0f, 12.0f, 12.0f);

        auto button = [&](Rectangle<float> rc, const String& t, bool enabled)
        {
            g.setColour(Colour(0xff0d1540).withAlpha(enabled ? 1.0f : 0.5f));
            g.fillRoundedRectangle(rc, 6.0f);
            g.setColour(Colour(0xff3a4aa0).withAlpha(enabled ? 1.0f : 0.5f));
            g.drawRoundedRectangle(rc, 6.0f, 1.0f);
            g.setColour(Colours::white.withAlpha(enabled ? 1.0f : 0.4f));
            g.drawText(t, rc, Justification::centred);
        };
        button({ 168.0f, 50.0f, 40.0f, 22.0f }, "Undo", r.canUndo());
        button({ 212.0f, 50.0f, 40.0f, 22.0f }, "Redo", r.canRedo());
    }

    void mouseDown(const MouseEvent& e) override
    {
        auto& r = cx.proc.getRandomizer();
        if (e.x >= 168 && e.x < 208 && e.y >= 50) { r.undo(); }
        else if (e.x >= 212 && e.y >= 50)         { r.redo(); }
        else if (e.x >= 62 && e.x <= 168 && e.y >= 46) { setAmount((float) e.x); }
        repaint();
    }

    void mouseDrag(const MouseEvent& e) override
    {
        if (e.getMouseDownPosition().x >= 62 && e.getMouseDownPosition().x <= 168 && e.getMouseDownPosition().y >= 46)
            setAmount((float) e.x);
    }

private:
    void setAmount(float x) { cx.proc.getRandomizer().setAmount((x - 70.0f) / 90.0f); repaint(); }

    Ctx& cx;
    float last = -1.0f;
};

static String noteName(int midi)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String(n[((midi % 12) + 12) % 12]) + String(midi / 12 - 1);
}

// Small pill-shaped button used for the little sample controls (Load, Loop, Root note).
class MiniPill : public UView
{
public:
    MiniPill(Rectangle<int> a, std::function<String()> labelFn, std::function<void(bool)> clickFn, std::function<bool()> activeFn)
        : label(std::move(labelFn)), click(std::move(clickFn)), active(std::move(activeFn))
    {
        setBounds(a);
    }

    void update() override
    {
        const String t = label();
        const bool a = active();
        if (t != shownText || a != shownActive) { shownText = t; shownActive = a; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(shownActive ? Colour(0xee1d3f9a) : Colour(0xdd050820));
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(shownActive ? Colour(0xff58c0ff) : Colour(0xff3a4aa0));
        g.drawRoundedRectangle(r, 8.0f, 1.2f);
        g.setColour(Colours::white);
        g.setFont(Font(13.0f, Font::bold));
        g.drawText(shownText, getLocalBounds(), Justification::centred);
    }

    void mouseDown(const MouseEvent& e) override { click(e.mods.isPopupMenu() || e.mods.isCtrlDown() || e.mods.isShiftDown()); }

    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) { Component::mouseWheelMove(e, w); return; }
        click(w.deltaY < 0.0f);
    }

private:
    std::function<String()> label;
    std::function<void(bool)> click;
    std::function<bool()> active;
    String shownText;
    bool shownActive = false;
};

// A label whose text can change (covers the printed label in the picture).
class DynLabel : public UView
{
public:
    DynLabel(Ctx& c, Rectangle<int> a, std::function<String()> textFn)
        : text(std::move(textFn)), col(0xff090d26)
    {
        setBounds(a);
        setInterceptsMouseClicks(false, false);
    }

    void update() override
    {
        const String t = text();
        if (t != shown) { shown = t; repaint(); }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(col);
        g.setColour(Colours::white);
        g.setFont(Font(15.5f));
        g.drawText(shown, getLocalBounds(), Justification::centred);
    }

private:
    std::function<String()> text;
    Colour col;
    String shown;
};

// Wavetable name under the oscillator display: arrows step through the 64 tables, the name opens a menu.
class WaveSelector : public UView
{
public:
    explicit WaveSelector(Ctx& c) : cx(c), cover(sampleEdges(c.bg, { 70, 270, 190, 18 }))
    {
        setBounds(38, 268, 254, 22);
    }

    void update() override
    {
        const bool vis = cx.st.osc <= 2;
        if (vis != isVisible()) setVisible(vis);
        const float v = cx.st.osc <= 2 ? cx.raw("osc" + String(cx.st.osc + 1) + "_wave") : -1.0f;
        if (v != last) { last = v; repaint(); }
    }

    void paint(Graphics& g) override
    {
        g.setColour(cover);
        g.fillRect(30, 1, 194, 20);
        g.setColour(Colours::white);
        g.setFont(Font(15.0f));
        g.drawText(ux::WavetableBank::waveNames()[currentIndex()], 30, 1, 194, 20, Justification::centred);
    }

    void mouseDown(const MouseEvent& e) override
    {
        const int n = ux::WavetableBank::numWaves;
        const int idx = currentIndex();
        if (e.x < 28)             select((idx + n - 1) % n);
        else if (e.x > 226)       select((idx + 1) % n);
        else                      menu();
    }

private:
    int currentIndex() const
    {
        if (cx.st.osc > 2) return 0;
        return jlimit(0, ux::WavetableBank::numWaves - 1,
                      roundToInt(cx.raw("osc" + String(cx.st.osc + 1) + "_wave") * (float) (ux::WavetableBank::numWaves - 1)));
    }

    void select(int idx)
    {
        cx.setReal("osc" + String(cx.st.osc + 1) + "_wave", (float) idx / (float) (ux::WavetableBank::numWaves - 1));
    }

    void menu()
    {
        const int cur = currentIndex();
        PopupMenu root;
        for (int fam = 0; fam < 8; ++fam)
        {
            PopupMenu sub;
            for (int v = 0; v < 8; ++v)
                sub.addItem(fam * 8 + v + 1, ux::WavetableBank::waveNames()[fam * 8 + v], true, fam * 8 + v == cur);
            root.addSubMenu(ux::WavetableBank::familyNames()[fam], sub);
        }
        root.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [this](int r) { if (r > 0) select(r - 1); });
    }

    Ctx& cx;
    Colour cover;
    float last = -9.0f;
};

// Waveform display of the loaded sample, with start/end markers (Sample) or the grain position (Granular).
class SampleDisplay : public UView
{
public:
    SampleDisplay(Ctx& c, std::function<void()> loadFn) : cx(c), load(std::move(loadFn)) { setBounds(455, 197, 296, 85); }

    void update() override
    {
        auto s = cx.proc.getSample();
        const float h = hashIds(cx, { "sample_mode", "sample_start", "sample_end", "grain_pos", "grain_spread" });
        if (s != cached) { cached = s; rebuildPeaks(); repaint(); }
        else if (h != lastHash) { lastHash = h; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff08021c), 0, 0, Colour(0xff150838), 0, r.getHeight(), false));
        g.fillRoundedRectangle(r.reduced(0.5f), 6.0f);

        if (cached == nullptr)
        {
            g.setColour(Colour(0xffcfa8ff));
            g.setFont(Font(15.0f));
            g.drawText("Click to load a sample (WAV, AIFF, FLAC)", getLocalBounds().withTrimmedBottom(24), Justification::centred);
            g.setColour(Colour(0xff9a80d0));
            g.setFont(Font(13.0f));
            g.drawText("or drop a file on the window", getLocalBounds().withTrimmedTop(28).withTrimmedBottom(10), Justification::centred);
            return;
        }

        const float w = r.getWidth(), h = r.getHeight(), mid = h * 0.46f, amp = h * 0.38f;
        g.setColour(Colour(0xffb85cff));
        for (int x = 0; x < (int) w && x < (int) peaks.size() / 2; ++x)
        {
            const float lo = peaks[(size_t) x * 2], hi = peaks[(size_t) x * 2 + 1];
            const float y0 = mid - hi * amp, y1 = mid - lo * amp;
            g.fillRect((float) x, y0, 1.0f, jmax(1.0f, y1 - y0));
        }

        g.setColour(Colour(0xffcfa8ff));
        g.setFont(Font(12.0f));
        g.drawText(cached->name, 6, 2, (int) w - 60, 14, Justification::centredLeft);

        const int mode = (int) cx.raw("sample_mode");
        if (mode == 1)
        {
            const float p = cx.raw("grain_pos"), sp = cx.raw("grain_spread") * 0.2f;
            g.setColour(Colour(0x2258c0ff));
            g.fillRect((p - sp) * w, 0.0f, jmax(2.0f, 2.0f * sp * w), h);
            g.setColour(Colour(0xffa8f0ff));
            g.drawLine(p * w, 0.0f, p * w, h, 2.0f);
        }
        else
        {
            const float st = cx.raw("sample_start"), en = jmax(st, cx.raw("sample_end"));
            g.setColour(Colour(0x99000010));
            g.fillRect(0.0f, 0.0f, st * w, h);
            g.fillRect(en * w, 0.0f, w - en * w, h);
            g.setColour(Colour(0xff58ffc8));
            g.drawLine(st * w, 0.0f, st * w, h, 2.0f);
            g.setColour(Colour(0xffff8858));
            g.drawLine(en * w, 0.0f, en * w, h, 2.0f);
        }
    }

    void mouseDown(const MouseEvent& e) override { if (cached != nullptr) edit(e); }
    void mouseDrag(const MouseEvent& e) override { if (cached != nullptr) edit(e); }
    void mouseUp(const MouseEvent& e) override { if (cached == nullptr && getLocalBounds().contains(e.getPosition())) load(); }
    void mouseDoubleClick(const MouseEvent&) override { if (cached != nullptr) load(); }

private:
    void edit(const MouseEvent& e)
    {
        const float n = jlimit(0.0f, 1.0f, (float) e.x / (float) getWidth());
        if ((int) cx.raw("sample_mode") == 1)
            cx.setReal("grain_pos", n);
        else if (e.mods.isRightButtonDown() || e.mods.isCtrlDown() || e.mods.isShiftDown())
            cx.setReal("sample_end", jmax(n, cx.raw("sample_start")));
        else
            cx.setReal("sample_start", jmin(n, cx.raw("sample_end")));
    }

    void rebuildPeaks()
    {
        peaks.clear();
        if (cached == nullptr) return;
        const int w = getWidth();
        peaks.assign((size_t) w * 2, 0.0f);
        const double per = (double) cached->length / (double) w;
        for (int x = 0; x < w; ++x)
        {
            const int a = (int) (x * per), b = jmin(cached->length, jmax(a + 1, (int) ((x + 1) * per)));
            float lo = 0.0f, hi = 0.0f;
            const int step = jmax(1, (b - a) / 64);
            for (int i = a; i < b; i += step)
            {
                const float v = 0.5f * (cached->L[(size_t) i] + cached->R[(size_t) i]);
                lo = jmin(lo, v); hi = jmax(hi, v);
            }
            peaks[(size_t) x * 2] = lo;
            peaks[(size_t) x * 2 + 1] = hi;
        }
    }

    Ctx& cx;
    std::function<void()> load;
    std::shared_ptr<const ux::SampleData> cached;
    std::vector<float> peaks;
    float lastHash = -1.0f;
};

// Resize handle in the bottom-right corner: drag to scale the whole window, double-click for 75% / 100%.
class ResizeHandle : public UView
{
public:
    explicit ResizeHandle(Ctx& c) : cx(c)
    {
        // Sits just above the "Made with JUCE" logo box, which covers the very corner on the free JUCE license.
        setBounds(kW - 40, 886, 38, 38);
        setMouseCursor(MouseCursor::BottomRightCornerResizeCursor);
    }

    void paint(Graphics& g) override
    {
        const Colour col = (hover || dragging) ? Colours::white : Colour(0xffa8dcff);
        const auto pad = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(Colour(0xcc050820));
        g.fillRoundedRectangle(pad, 9.0f);
        g.setColour(Colour(0xff3ac8ff).withAlpha((hover || dragging) ? 0.95f : 0.6f));
        g.drawRoundedRectangle(pad, 9.0f, 1.4f);
        for (int i = 0; i < 3; ++i)
        {
            const float o = 10.0f + (float) i * 8.0f;
            g.setColour(Colour(0xff3ac8ff).withAlpha(0.25f));
            g.drawLine(36.0f - o, 31.0f, 31.0f, 36.0f - o, 5.0f);
            g.setColour(col.withAlpha(0.95f));
            g.drawLine(36.0f - o, 31.0f, 31.0f, 36.0f - o, 2.4f);
        }
    }

    void mouseEnter(const MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const MouseEvent&) override { hover = false; repaint(); }

    void mouseDown(const MouseEvent& e) override
    {
        dragging = true;
        if (auto* ed = findParentComponentOfClass<AudioProcessorEditor>())
        {
            const auto p = e.getEventRelativeTo(ed).position;
            grabX = (float) ed->getWidth() - p.x;
            grabY = (float) ed->getHeight() - p.y;
        }
        repaint();
    }

    void mouseDrag(const MouseEvent& e) override
    {
        auto* ed = findParentComponentOfClass<AudioProcessorEditor>();
        if (ed == nullptr || ! cx.st.requestScale) return;
        const auto p = e.getEventRelativeTo(ed).position;
        const float sx = (p.x + grabX) / (float) kW, sy = (p.y + grabY) / (float) kH;
        cx.st.requestScale(0.5f * (sx + sy));
    }

    void mouseUp(const MouseEvent&) override { dragging = false; repaint(); }

    void mouseDoubleClick(const MouseEvent&) override
    {
        if (cx.st.requestScale) cx.st.requestScale(cx.st.scale < 0.99f ? 1.0f : 0.75f);
    }

private:
    Ctx& cx;
    bool hover = false, dragging = false;
    float grabX = 0.0f, grabY = 0.0f;
};

class ZoomPill : public UView
{
public:
    explicit ZoomPill(Ctx& c) : cx(c) { setBounds(10, 8, 96, 26); }

    void update() override
    {
        if (cx.st.scale != shown) { shown = cx.st.scale; repaint(); }
    }

    void paint(Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(Colour(0xdd070a24));
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(Colour(0xff3a4aa0));
        g.drawRoundedRectangle(r, 12.0f, 1.2f);
        g.setColour(Colour(0xff9ab4ff));
        g.drawEllipse(13.0f, 6.0f, 10.0f, 10.0f, 1.8f);
        g.drawLine(21.0f, 15.0f, 26.0f, 20.0f, 2.0f);
        g.setColour(Colours::white);
        g.setFont(Font(14.5f, Font::bold));
        g.drawText(String(roundToInt(cx.st.scale * 100.0f)) + "%", 32, 0, 60, getHeight(), Justification::centredLeft);
    }

    void mouseDown(const MouseEvent&) override
    {
        PopupMenu m;
        m.addItem(1000, "Fit to screen");
        m.addSeparator();
        static const int pct[] = { 50, 65, 75, 85, 100, 125, 150 };
        const int cur = roundToInt(cx.st.scale * 100.0f);
        for (int p : pct)
            m.addItem(p, String(p) + "%", true, p == cur);

        Ctx* c = &cx;
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [c](int r)
        {
            if (r <= 0 || ! c->st.requestScale) return;
            if (r == 1000)
            {
                float s = 0.75f;
                if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
                {
                    const auto a = d->userArea;
                    s = jmin((float) a.getWidth() * 0.96f / (float) kW, ((float) a.getHeight() - 70.0f) / (float) kH);
                }
                c->st.requestScale(s);
            }
            else
                c->st.requestScale((float) r / 100.0f);
        });
    }

private:
    Ctx& cx;
    float shown = -1.0f;
};

} // namespace uiv

// ===== Plugin editor (artwork overlay) =====
class UniverseXAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit UniverseXAudioProcessorEditor(UniverseXAudioProcessor& p)
        : AudioProcessorEditor(&p), processor(p),
          bg(juce::ImageCache::getFromMemory(BinaryData::UniverseX_bg_png, BinaryData::UniverseX_bg_pngSize)),
          ctx(p, bg, state)
    {
        using namespace uiv;
        canvas.bg = bg;
        canvas.setInterceptsMouseClicks(false, true);
        addAndMakeVisible(canvas);

        auto add = [this](auto* view) { canvas.addAndMakeVisible(view); views.emplace_back(view); return view; };

        // --- Preset bar ---
        add(new PresetNameView(ctx));
        add(new Hotspot({ 1090, 22, 96, 38 }, [this](Component&) { /* reserved for the preset browser */ }));
        add(new Hotspot({ 1190, 22, 82, 38 }, [this](Component&) { initPatch(); }));
        add(new Hotspot({ 1276, 22, 68, 38 }, [this](Component&) { savePreset(); }));
        add(new Hotspot({ 1348, 22, 78, 38 }, [this](Component&) { loadPreset(); }));

        // --- Oscillators ---
        add(new WaveDisplay(ctx, { 33, 193, 382, 70 }));
        add(new TabView(ctx, { { 38, 166, 72, 28 }, { 114, 166, 74, 28 }, { 194, 166, 70, 28 }, { 272, 166, 68, 28 }, { 344, 166, 72, 28 } },
                        { "OSC 1", "OSC 2", "OSC 3", "SUB", "NOISE" },
                        [this] { return state.osc; }, [this](int i) { state.osc = i; rebindOsc(); }));
        const juce::Point<float> oscPts[5] = { { 84, 310 }, { 175, 310 }, { 246, 310 }, { 314, 310 }, { 381, 310 } };
        for (int i = 0; i < 5; ++i) oscKnobs[i] = add(new KnobView(ctx, oscPts[i], true));
        rebindOsc();
        add(new WaveSelector(ctx));

        // --- Sample & Granular ---
        add(new TabView(ctx, { { 460, 166, 76, 28 }, { 548, 166, 100, 28 }, { 656, 166, 86, 28 } }, { "Sample", "Granular", "Spectral" },
                        [this] { return (int) ctx.raw("sample_mode"); }, [this](int i) { ctx.setChoice("sample_mode", i); rebindSampler(); }));
        add(new SampleDisplay(ctx, [this] { loadSample(); }));
        const juce::Point<float> smpPts[3] = { { 497, 310 }, { 599, 310 }, { 704, 310 } };
        for (int i = 0; i < 3; ++i)
        {
            smpKnobs[i] = add(new KnobView(ctx, smpPts[i], true));
            const int idx = i;
            add(new DynLabel(ctx, { (int) smpPts[i].x - 43, 332, 86, 19 }, [this, idx] { return samplerLabel(idx); }));
        }
        add(new MiniPill({ 700, 201, 46, 20 }, [] { return String("Load"); }, [this](bool) { loadSample(); }, [] { return false; }));
        add(new MiniPill({ 462, 259, 48, 20 }, [] { return String("Loop"); },
                         [this](bool) { ctx.setChoice("sample_loop", ctx.raw("sample_loop") > 0.5f ? 0 : 1); },
                         [this] { return ctx.raw("sample_loop") > 0.5f; }));
        add(new MiniPill({ 516, 259, 92, 20 }, [this] { return "Root " + uiv::noteName((int) ctx.raw("sample_root")); },
                         [this](bool down) { ctx.setReal("sample_root", (float) juce::jlimit(12, 108, (int) ctx.raw("sample_root") + (down ? -1 : 1))); },
                         [] { return false; }));
        rebindSampler();

        // --- Filters ---
        add(new FilterDisplay(ctx, { 786, 196, 300, 84 }));
        add(new TabView(ctx, { { 789, 168, 65, 24 }, { 861, 168, 60, 24 }, { 928, 168, 60, 24 }, { 996, 168, 62, 24 } },
                        { "LP", "BP", "HP", "Notch" },
                        [this] { return (int) ctx.raw("filter_type"); }, [this](int i) { ctx.setChoice("filter_type", i); }));
        knob(add, 818, 310, "filter_cutoff");  knob(add, 893, 310, "filter_reso");
        knob(add, 968, 310, "filter_drive");   knob(add, 1042, 310, "filter_keytrack");

        // --- Modulation (LFO) ---
        add(new LfoDisplay(ctx, { 33, 436, 529, 59 }));
        add(new TabView(ctx, { { 35, 404, 63, 26 }, { 104, 404, 61, 26 }, { 171, 404, 59, 26 }, { 236, 404, 58, 26 } },
                        { "LFO 1", "LFO 2", "LFO 3", "LFO 4" },
                        [this] { return state.lfo; }, [this](int i) { state.lfo = i; rebindLfo(); }));
        const juce::Point<float> lfoPts[4] = { { 85, 527 }, { 203, 527 }, { 316, 527 }, { 428, 527 } };
        for (int i = 0; i < 4; ++i) lfoKnobs[i] = add(new KnobView(ctx, lfoPts[i], true));
        add(new SyncToggle(ctx));
        rebindLfo();

        // --- XY pad ---
        add(new XYPadView(ctx));
        add(new Hotspot({ 610, 552, 92, 26 }, [this](Component& c) { targetMenu("xy_x_target", c); }));
        add(new Hotspot({ 714, 552, 92, 26 }, [this](Component& c) { targetMenu("xy_y_target", c); }));

        // --- Mod matrix ---
        add(new ModMatrixView(ctx));

        // --- Envelopes ---
        add(new EnvDisplay(ctx, { 36, 640, 328, 72 }));
        add(new TabView(ctx, { { 34, 617, 66, 24 }, { 112, 617, 66, 24 }, { 195, 617, 67, 24 }, { 278, 617, 68, 24 } },
                        { "Env 1", "Env 2", "Env 3", "Env 4" },
                        [this] { return state.env; }, [this](int i) { state.env = i; rebindEnv(); }));
        const juce::Point<float> envPts[5] = { { 56, 733 }, { 126, 733 }, { 196, 733 }, { 266, 733 }, { 336, 733 } };
        for (int i = 0; i < 5; ++i) envKnobs[i] = add(new KnobView(ctx, envPts[i], true));
        rebindEnv();

        // --- Step sequencer ---
        add(new StepSeqView(ctx));
        add(new StepsStepper(ctx));
        knob(add, 450, 735, "seq_rate");  knob(add, 578, 735, "seq_smooth");  knob(add, 681, 735, "seq_depth");

        // --- Arpeggiator ---
        add(new ArpView(ctx));
        knob(add, 840, 733, "arp_rate");  knob(add, 950, 733, "arp_octaves");

        // --- Effects ---
        knob(add, 1110, 668, "fx_reverb_mix");  knob(add, 1207, 668, "fx_delay_mix");  knob(add, 1300, 668, "fx_chorus_mix");
        knob(add, 1112, 733, "fx_phaser_mix");  knob(add, 1209, 733, "fx_dist_drive");  knob(add, 1302, 733, "fx_eq_tilt");

        // --- Global ---
        knob(add, 1408, 640, "unison_voices", 17);  knob(add, 1478, 640, "unison_detune", 17);
        knob(add, 1408, 695, "unison_spread", 17);  knob(add, 1478, 695, "glide", 17);
        add(new LabelPatch(ctx, { 1448, 708, 62, 22 }, "Glide"));
        add(new PolyView(ctx));

        // --- Master ---
        knob(add, 1466, 318, "master_volume");

        // --- Lazy Randomizer ---
        add(new LazyStrip(ctx));
        add(new RandomPanel(ctx));
        add(new ZoomPill(ctx));
        add(new ResizeHandle(ctx));
        state.scale = jlimit(0.5f, 1.5f, processor.uiScale.load());
        state.requestScale = [this](float sc) { applyScale(sc); };

        setResizable(true, false);   // our own handle (bottom-right) replaces the plain corner grip
        getConstrainer()->setFixedAspectRatio((double) kW / (double) kH);
        setResizeLimits(768, 512, kW * 3 / 2, kH * 3 / 2);
        setSize(juce::roundToInt((float) kW * state.scale), juce::roundToInt((float) kH * state.scale));
        ready = true;
        state.scale = (float) getWidth() / (float) kW;
        startTimerHz(30);
    }

    ~UniverseXAudioProcessorEditor() override { stopTimer(); }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

    void resized() override
    {
        const float s = (float) getWidth() / (float) uiv::kW;
        canvas.setBounds(0, 0, uiv::kW, uiv::kH);
        canvas.setTransform(juce::AffineTransform::scale(s));
        if (ready)                    // ignore the size changes made while the window is being built
        {
            state.scale = s;
            processor.uiScale = s;
        }
    }

    // Ctrl + mouse wheel anywhere on the window zooms it.
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
            applyScale(state.scale + (w.deltaY > 0.0f ? 0.05f : -0.05f));
    }

private:
    struct Canvas : juce::Component
    {
        void paint(juce::Graphics& g) override
        {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImageAt(bg, 0, 0);
        }
        juce::Image bg;
    };

    template <typename Adder>
    void knob(Adder& add, int x, int y, const char* id, int clip = 21)
    {
        auto* k = add(new uiv::KnobView(ctx, { (float) x, (float) y }, false, clip));
        k->setParam(processor.apvts.getParameter(id));
    }

    void applyScale(float sc)
    {
        sc = juce::jlimit(0.5f, 1.5f, sc);
        setSize(juce::roundToInt((float) uiv::kW * sc), juce::roundToInt((float) uiv::kH * sc));
    }

    void rebindOsc()
    {
        static const char* suffix[5] = { "level", "pan", "tune", "phase", "warp" };
        for (int i = 0; i < 5; ++i)
        {
            juce::String id;
            if (state.osc <= 2)               id = "osc" + juce::String(state.osc + 1) + "_" + suffix[i];
            else if (i == 0)                  id = state.osc == 3 ? "sub_level" : "noise_level";
            oscKnobs[i]->setParam(id.isNotEmpty() ? processor.apvts.getParameter(id) : nullptr);
        }
    }

    juce::String samplerLabel(int i) const
    {
        static const char* sampleNames[3] = { "Start", "End", "Level" };
        static const char* grainNames[3] = { "Grain Size", "Density", "Spread" };
        const int mode = (int) ctx.raw("sample_mode");
        return mode == 0 ? sampleNames[i] : mode == 1 ? grainNames[i] : "-";
    }

    void rebindSampler()
    {
        static const char* sampleIds[3] = { "sample_start", "sample_end", "sample_level" };
        static const char* grainIds[3] = { "grain_size", "grain_density", "grain_spread" };
        const int mode = (int) ctx.raw("sample_mode");
        lastSampleMode = mode;
        for (int i = 0; i < 3; ++i)
            smpKnobs[i]->setParam(mode == 0 ? processor.apvts.getParameter(sampleIds[i])
                                  : mode == 1 ? processor.apvts.getParameter(grainIds[i]) : nullptr);
    }

    void loadSample()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        chooser = std::make_unique<juce::FileChooser>("Load a sample", dir, "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& fc)
                             {
                                 const auto f = fc.getResult();
                                 if (f.existsAsFile())
                                     tryLoad(f);
                             });
    }

    void tryLoad(const juce::File& f)
    {
        if (! processor.loadSampleFile(f))
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Couldn't load that file",
                                                   "Universe X can load WAV, AIFF, FLAC and OGG files.");
    }

    void rebindLfo()
    {
        static const char* suffix[4] = { "rate", "depth", "phase", "symmetry" };
        for (int i = 0; i < 4; ++i)
            lfoKnobs[i]->setParam(processor.apvts.getParameter("lfo" + juce::String(state.lfo + 1) + "_" + suffix[i]));
    }

    void rebindEnv()
    {
        static const char* suffix[5] = { "a", "d", "s", "r", "curve" };
        for (int i = 0; i < 5; ++i)
            envKnobs[i]->setParam(processor.apvts.getParameter("env" + juce::String(state.env + 1) + "_" + suffix[i]));
    }

    void targetMenu(const juce::String& id, juce::Component& target)
    {
        auto* ch = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(id));
        if (ch == nullptr) return;
        const int cur = (int) ctx.raw(id);
        juce::PopupMenu m;
        for (int i = 0; i < ch->choices.size(); ++i)
            m.addItem(i + 1, ch->choices[i], true, i == cur);
        uiv::Ctx* c = &ctx;
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target), [c, id](int r) { if (r > 0) c->setChoice(id, r - 1); });
    }

    void initPatch()
    {
        for (const auto& d : ux::getParamDefs())
            if (auto* p = processor.apvts.getParameter(d.id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getDefaultValue());
                p->endChangeGesture();
            }
        state.presetName = "Init";
    }

    void savePreset()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Universe X");
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser>("Save preset", dir.getChildFile(state.presetName + ".uxpreset"), "*.uxpreset");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this](const juce::FileChooser& fc)
                             {
                                 auto f = fc.getResult();
                                 if (f == juce::File()) return;
                                 f = f.withFileExtension("uxpreset");
                                 if (auto xml = processor.apvts.copyState().createXml())
                                     xml->writeTo(f);
                                 state.presetName = f.getFileNameWithoutExtension();
                             });
    }

    void loadPreset()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Universe X");
        chooser = std::make_unique<juce::FileChooser>("Load preset", dir, "*.uxpreset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& fc)
                             {
                                 auto f = fc.getResult();
                                 if (! f.existsAsFile()) return;
                                 if (auto xml = juce::parseXML(f))
                                     if (xml->hasTagName(processor.apvts.state.getType()))
                                     {
                                         processor.apvts.replaceState(juce::ValueTree::fromXml(*xml));
                                         state.presetName = f.getFileNameWithoutExtension();
                                     }
                             });
    }

    void timerCallback() override
    {
        for (auto& v : views)
            v->update();

        const int m = (int) ctx.raw("sample_mode");      // also follows presets and host automation
        if (m != lastSampleMode)
            rebindSampler();
    }

public:
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (auto& f : files)
        {
            const auto ext = juce::File(f).getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg")
                return true;
        }
        return false;
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        for (auto& f : files)
            if (isInterestedInFileDrag(juce::StringArray(f)))
            {
                tryLoad(juce::File(f));
                break;
            }
    }

private:

    UniverseXAudioProcessor& processor;
    juce::Image bg;
    uiv::UIState state;
    uiv::Ctx ctx;
    Canvas canvas;
    std::vector<std::unique_ptr<uiv::UView>> views;
    uiv::KnobView* oscKnobs[5] = {};
    uiv::KnobView* lfoKnobs[4] = {};
    uiv::KnobView* envKnobs[5] = {};
    uiv::KnobView* smpKnobs[3] = {};
    int lastSampleMode = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    bool ready = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UniverseXAudioProcessorEditor)
};

// ===== Processor implementation =====
UniverseXAudioProcessor::UniverseXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "UNIVERSE_X", ux::createLayout()),
      randomizer(apvts)
{
    ptrs.bind(apvts);
    formatManager.registerBasicFormats();
    ux::WavetableBank::get();     // build the 64 wavetables now, not on the audio thread
}

void UniverseXAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    engine.prepare(sampleRate);
}

bool UniverseXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void UniverseXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo info;
        if (ph->getCurrentPosition(info) && info.bpm > 0.0)
            bpm = info.bpm;
    }

    auto smp = std::atomic_load(&sampleData);
    engine.process(buffer, midi, ptrs, bpm, smp.get());
}

bool UniverseXAudioProcessor::loadSampleFile(const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(f));
    if (reader == nullptr)
        return false;

    const juce::int64 maxLen = (juce::int64) (reader->sampleRate * 300.0);       // up to 5 minutes
    const int n = (int) juce::jmin(reader->lengthInSamples, maxLen);
    if (n < 64)
        return false;

    const bool stereo = reader->numChannels > 1;
    juce::AudioBuffer<float> buf(stereo ? 2 : 1, n);
    reader->read(&buf, 0, n, 0, true, stereo);

    auto sd = std::make_shared<ux::SampleData>();
    sd->length = n;
    sd->sampleRate = reader->sampleRate;
    sd->name = f.getFileNameWithoutExtension();
    sd->L.assign(buf.getReadPointer(0), buf.getReadPointer(0) + n);
    sd->R.assign(buf.getReadPointer(stereo ? 1 : 0), buf.getReadPointer(stereo ? 1 : 0) + n);

    std::atomic_store(&sampleData, std::shared_ptr<const ux::SampleData>(sd));
    {
        const juce::ScopedLock sl(pathLock);
        samplePath = f.getFullPathName();
    }
    return true;
}

void UniverseXAudioProcessor::clearSample()
{
    std::atomic_store(&sampleData, std::shared_ptr<const ux::SampleData>());
    const juce::ScopedLock sl(pathLock);
    samplePath = juce::String();
}

juce::AudioProcessorEditor* UniverseXAudioProcessor::createEditor()
{
    return new UniverseXAudioProcessorEditor(*this);
}

void UniverseXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute("uiScale", (double) uiScale.load());
        const auto sp = getSamplePath();
        if (sp.isNotEmpty())
            xml->setAttribute("samplePath", sp);
        copyXmlToBinary(*xml, destData);
    }
}

void UniverseXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            uiScale = (float) juce::jlimit(0.5, 1.5, xml->getDoubleAttribute("uiScale", 0.75));
            xml->removeAttribute("uiScale");
            const juce::String sp = xml->getStringAttribute("samplePath");
            xml->removeAttribute("samplePath");
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            if (sp.isNotEmpty() && juce::File(sp).existsAsFile())
                loadSampleFile(juce::File(sp));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UniverseXAudioProcessor();
}

