#pragma once
#include "Parameters.h"
#include <array>

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
