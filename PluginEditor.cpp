#include "PluginEditor.h"

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
