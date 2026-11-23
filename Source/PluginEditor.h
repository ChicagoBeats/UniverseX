#pragma once
#include "PluginProcessor.h"

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
