#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

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
