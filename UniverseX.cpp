// Universe X - Phase 1, single-file version.
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <array>


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


// ===== PluginProcessor.h =====

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Declared in this order on purpose: randomizer needs apvts to exist first.
    juce::AudioProcessorValueTreeState apvts;
    ux::Randomizer& getRandomizer() { return randomizer; }

private:
    ux::Randomizer randomizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UniverseXAudioProcessor)
};


// ===== PluginEditor.h =====

// Phase 1 editor: a working Lazy Randomizer strip (10 buttons, locks, amount, undo/redo).
// The full Universe X layout is built panel by panel on top of this in later phases.
class UniverseXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit UniverseXAudioProcessorEditor(UniverseXAudioProcessor&);
    ~UniverseXAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void refreshUndoButtons();

    UniverseXAudioProcessor& processor;

    juce::Label title, amountLabel;
    juce::TextButton lazyButtons[ux::Randomizer::numLazyButtons];
    juce::ToggleButton lockButtons[ux::Randomizer::numLazyButtons - 1];   // Full Mix has no lock
    juce::Slider amountSlider;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UniverseXAudioProcessorEditor)
};


// ===== PluginProcessor.cpp =====

UniverseXAudioProcessor::UniverseXAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "UNIVERSE_X", ux::createLayout()),
      randomizer(apvts)
{
}

void UniverseXAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Phase 2: prepare voices, oscillators, filters, effects here.
}

bool UniverseXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void UniverseXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midi);

    // Phase 1: silent. The audio engine arrives in Phase 2.
    buffer.clear();
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


// ===== PluginEditor.cpp =====

UniverseXAudioProcessorEditor::UniverseXAudioProcessorEditor(UniverseXAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    static const char* names[ux::Randomizer::numLazyButtons] = {
        "Oscillators", "Filters", "LFOs", "Envelopes", "Mod Matrix",
        "XY Pad", "Step Seq", "Arpeggiator", "Effects", "Full Mix" };

    title.setText("UNIVERSE X  -  Lazy Randomizer (Phase 1)", juce::dontSendNotification);
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

    setSize(1060, 190);
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
}
