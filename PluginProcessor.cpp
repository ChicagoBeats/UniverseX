#include "PluginProcessor.h"
#include "PluginEditor.h"

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
