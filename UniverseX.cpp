// Universe X - Phase 2 (sound engine), single-file version.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <memory>


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
        add(v, p + "wave",   n + "Wave Pos", 0.0f, 1.0f, i == 1 ? 2.0f / 7.0f : 0.0f, Section::Oscillators);
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
    static constexpr int numWaves = 8;

    static const WavetableBank& get()
    {
        static const WavetableBank bank;
        return bank;
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
            float peak = 0.0f;

            for (int m = 0; m < numMips; ++m)
            {
                float* t = &data[(size_t) ((w * numMips + m) * tableSize)];
                const int maxH = juce::jmin(1023, 1024 >> m);

                for (int h = 1; h <= maxH; ++h)
                {
                    const float a = harmonic(w, h);
                    if (a == 0.0f)
                        continue;

                    for (int i = 0; i < tableSize; ++i)
                        t[i] += a * sinT[(size_t) ((h * i) & (tableSize - 1))];
                }

                if (m == 0)
                    for (int i = 0; i < tableSize; ++i)
                        peak = juce::jmax(peak, std::abs(t[i]));
            }

            const float scale = peak > 0.0f ? 0.9f / peak : 1.0f;
            for (int m = 0; m < numMips; ++m)
            {
                float* t = &data[(size_t) ((w * numMips + m) * tableSize)];
                for (int i = 0; i < tableSize; ++i)
                    t[i] *= scale;
            }
        }
    }

    static float harmonic(int w, int h)
    {
        const float n = (float) h;
        switch (w)
        {
            case 0:  return h == 1 ? 1.0f : 0.0f;                                                   // sine
            case 1:  return (h & 1) ? ((((h - 1) / 2) & 1) ? -1.0f : 1.0f) / (n * n) : 0.0f;       // triangle
            case 2:  return 1.0f / n;                                                               // saw
            case 3:  return (h & 1) ? 1.0f / n : 0.0f;                                              // square
            case 4:  return std::sin(n * kPi * 0.25f) / n;                                          // pulse 25%
            case 5:
            {
                static const float organ[9] = { 0.0f, 1.0f, 0.7f, 0.5f, 0.35f, 0.25f, 0.18f, 0.12f, 0.08f };
                return h <= 8 ? organ[h] : 0.0f;                                                    // organ
            }
            case 6:  return ((h & 1) ? 1.0f : 0.3f) / std::pow(n, 0.9f) * (0.6f + 0.4f * std::sin(n * 1.9f)); // glass
            default:
            {
                float f = 0.0f;                                                                     // vocal / formant
                const float centres[3] = { 3.0f, 7.0f, 12.0f };
                for (float c : centres)
                    f += std::exp(-((n - c) * (n - c)) / 6.0f);
                return f + 0.03f / n;
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

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const ParamPtrs& p, double bpmIn)
    {
        params = &p;
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

private:
    ux::Randomizer randomizer;
    ux::ParamPtrs ptrs;
    ux::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UniverseXAudioProcessor)
};

// ===== Plugin editor =====
class UniverseXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UniverseXAudioProcessorEditor(UniverseXAudioProcessor&);
    ~UniverseXAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    void addKnob(const juce::String& id, const juce::String& text);
    void addCombo(const juce::String& id, const juce::String& text);
    void refreshUndoButtons();

    UniverseXAudioProcessor& processor;

    juce::Label title, amountLabel;
    juce::TextButton lazyButtons[ux::Randomizer::numLazyButtons];
    juce::ToggleButton lockButtons[ux::Randomizer::numLazyButtons - 1];
    juce::Slider amountSlider;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    std::vector<juce::Component*> cells;     // layout order

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UniverseXAudioProcessorEditor)
};

// ===== Processor implementation =====
UniverseXAudioProcessor::UniverseXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "UNIVERSE_X", ux::createLayout()),
      randomizer(apvts)
{
    ptrs.bind(apvts);
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

    engine.process(buffer, midi, ptrs, bpm);
}

juce::AudioProcessorEditor* UniverseXAudioProcessor::createEditor()
{
    return new UniverseXAudioProcessorEditor(*this);
}

void UniverseXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void UniverseXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UniverseXAudioProcessor();
}

// ===== Editor implementation =====
void UniverseXAudioProcessorEditor::addKnob(const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
    k->slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff7a6bff));
    k->slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    k->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->label.setText(text, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setColour(juce::Label::textColourId, juce::Colour(0xffb9b0ff));
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id, k->slider);
    addAndMakeVisible(k->slider);
    addAndMakeVisible(k->label);
    cells.push_back(&k->slider);
    knobs.push_back(std::move(k));
}

void UniverseXAudioProcessorEditor::addCombo(const juce::String& id, const juce::String& text)
{
    auto c = std::make_unique<Combo>();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(id)))
        c->box.addItemList(choice->choices, 1);
    c->label.setText(text, juce::dontSendNotification);
    c->label.setJustificationType(juce::Justification::centred);
    c->label.setColour(juce::Label::textColourId, juce::Colour(0xffb9b0ff));
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, id, c->box);
    addAndMakeVisible(c->box);
    addAndMakeVisible(c->label);
    cells.push_back(&c->box);
    combos.push_back(std::move(c));
}

UniverseXAudioProcessorEditor::UniverseXAudioProcessorEditor(UniverseXAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    static const char* names[ux::Randomizer::numLazyButtons] = {
        "Oscillators", "Filters", "LFOs", "Envelopes", "Mod Matrix",
        "XY Pad", "Step Seq", "Arpeggiator", "Effects", "Full Mix" };

    title.setText("UNIVERSE X  -  Phase 2: sound engine + Lazy Randomizer", juce::dontSendNotification);
    title.setFont(juce::Font(20.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    for (int i = 0; i < ux::Randomizer::numLazyButtons; ++i)
    {
        lazyButtons[i].setButtonText(juce::String(i + 1) + "  " + names[i]);
        lazyButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a1a5e));
        lazyButtons[i].onClick = [this, i]
        {
            processor.getRandomizer().lazy(i);
            refreshUndoButtons();
        };
        addAndMakeVisible(lazyButtons[i]);

        if (i < ux::Randomizer::numLazyButtons - 1)
        {
            lockButtons[i].setButtonText("Lock");
            lockButtons[i].onClick = [this, i]
            {
                processor.getRandomizer().setSectionLocked(ux::Randomizer::sectionForLazy(i),
                                                           lockButtons[i].getToggleState());
            };
            addAndMakeVisible(lockButtons[i]);
        }
    }

    amountLabel.setText("Amount", juce::dontSendNotification);
    amountLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(amountLabel);

    amountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    amountSlider.setRange(0.0, 1.0, 0.01);
    amountSlider.setValue(processor.getRandomizer().getAmount(), juce::dontSendNotification);
    amountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    amountSlider.onValueChange = [this]
    {
        processor.getRandomizer().setAmount((float) amountSlider.getValue());
    };
    addAndMakeVisible(amountSlider);

    undoButton.onClick = [this] { processor.getRandomizer().undo(); refreshUndoButtons(); };
    redoButton.onClick = [this] { processor.getRandomizer().redo(); refreshUndoButtons(); };
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);
    refreshUndoButtons();

    // Row 1: oscillators
    addKnob("osc1_wave", "Osc1 Wave");   addKnob("osc1_level", "Osc1 Level");  addKnob("osc1_tune", "Osc1 Tune");
    addKnob("osc1_warp", "Osc1 Warp");   addKnob("osc2_wave", "Osc2 Wave");    addKnob("osc2_level", "Osc2 Level");
    addKnob("osc2_tune", "Osc2 Tune");   addKnob("osc3_level", "Osc3 Level");  addKnob("sub_level", "Sub");
    addKnob("noise_level", "Noise");
    // Row 2: filter and envelope
    addCombo("filter_type", "Filter");   addKnob("filter_cutoff", "Cutoff");   addKnob("filter_reso", "Reso");
    addKnob("filter_drive", "Drive");    addKnob("env1_a", "Attack");          addKnob("env1_d", "Decay");
    addKnob("env1_s", "Sustain");        addKnob("env1_r", "Release");         addKnob("filter_keytrack", "KeyTrack");
    addKnob("lfo1_rate", "LFO1 Rate");
    // Row 3: voices and effects
    addKnob("unison_voices", "Unison");  addKnob("unison_detune", "Detune");   addKnob("unison_spread", "Spread");
    addKnob("glide", "Glide");           addKnob("fx_reverb_mix", "Reverb");   addKnob("fx_delay_mix", "Delay");
    addKnob("fx_chorus_mix", "Chorus");  addKnob("fx_phaser_mix", "Phaser");   addKnob("fx_dist_drive", "Distort");
    addKnob("master_volume", "Volume");
    // Row 4: arp and extras
    addCombo("arp_mode", "Arp");         addKnob("arp_rate", "Arp Rate");      addKnob("arp_octaves", "Arp Oct");
    addKnob("lfo1_depth", "LFO1 Depth"); addKnob("osc1_octave", "Osc1 Oct");   addKnob("osc2_octave", "Osc2 Oct");
    addKnob("polyphony", "Polyphony");   addKnob("fx_delay_time", "Dly Time"); addKnob("fx_delay_fb", "Dly Fdbk");
    addKnob("fx_reverb_size", "Rev Size");

    setSize(1060, 560);
}

void UniverseXAudioProcessorEditor::refreshUndoButtons()
{
    undoButton.setEnabled(processor.getRandomizer().canUndo());
    redoButton.setEnabled(processor.getRandomizer().canRedo());
}

void UniverseXAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0820));
}

void UniverseXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    title.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);

    auto strip = area.removeFromTop(60);
    const int w = strip.getWidth() / ux::Randomizer::numLazyButtons;
    for (int i = 0; i < ux::Randomizer::numLazyButtons; ++i)
    {
        auto cell = strip.removeFromLeft(w).reduced(3);
        lazyButtons[i].setBounds(cell.removeFromTop(34));
        if (i < ux::Randomizer::numLazyButtons - 1)
            lockButtons[i].setBounds(cell);
    }

    area.removeFromTop(8);
    auto bottom = area.removeFromTop(28);
    amountLabel.setBounds(bottom.removeFromLeft(70));
    undoButton.setBounds(bottom.removeFromRight(70).reduced(2));
    redoButton.setBounds(bottom.removeFromRight(70).reduced(2));
    amountSlider.setBounds(bottom);

    area.removeFromTop(10);

    // Controls grid: 10 columns, 4 rows
    const int cols = 10;
    const int cellW = area.getWidth() / cols;
    const int cellH = 98;
    int index = 0;

    auto place = [&](juce::Component* c, juce::Label& label)
    {
        const int col = index % cols, row = index / cols;
        juce::Rectangle<int> r(area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH);
        label.setBounds(r.removeFromTop(16));
        if (dynamic_cast<juce::ComboBox*>(c) != nullptr)
            c->setBounds(r.reduced(8, 12).withHeight(26));
        else
            c->setBounds(r.reduced(4, 0));
        ++index;
    };

    for (auto* c : cells)
    {
        bool done = false;
        for (auto& k : knobs)
            if (&k->slider == c) { place(c, k->label); done = true; break; }
        if (done) continue;
        for (auto& cb : combos)
            if (&cb->box == c) { place(c, cb->label); break; }
    }
}
