#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "Randomizer.h"

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
