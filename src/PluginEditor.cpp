#include "PluginEditor.h"

MiniSynthEditor::MiniSynthEditor(MiniSynthProcessor &owner)
    : AudioProcessorEditor(owner), levelAttachment(owner.getState(), MiniSynthProcessor::levelId, levelSlider),
      cutoffAttachment(owner.getState(), MiniSynthProcessor::cutoffId, cutoffSlider),
      resonanceAttachment(owner.getState(), MiniSynthProcessor::resonanceId, resonanceSlider),
      attackAttachment(owner.getState(), MiniSynthProcessor::attackId, attackSlider),
      decayAttachment(owner.getState(), MiniSynthProcessor::decayId, decaySlider),
      sustainAttachment(owner.getState(), MiniSynthProcessor::sustainId, sustainSlider),
      releaseAttachment(owner.getState(), MiniSynthProcessor::releaseId, releaseSlider) {
    addKnob(levelSlider, levelLabel);
    addKnob(cutoffSlider, cutoffLabel);
    addKnob(resonanceSlider, resonanceLabel);
    addKnob(attackSlider, attackLabel);
    addKnob(decaySlider, decayLabel);
    addKnob(sustainSlider, sustainLabel);
    addKnob(releaseSlider, releaseLabel);

    waveformLabel.setJustificationType(juce::Justification::centred);
    waveformLabel.attachToComponent(&waveformBox, false);
    // The attachment only selects an item, so fill the box from the parameter's own choices first.
    if (const auto *choice =
            dynamic_cast<juce::AudioParameterChoice *>(owner.getState().getParameter(MiniSynthProcessor::waveformId))) {
        waveformBox.addItemList(choice->choices, 1);
    }
    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        owner.getState(), MiniSynthProcessor::waveformId, waveformBox);
    addAndMakeVisible(waveformBox);
    addAndMakeVisible(waveformLabel);
    setSize(560, 400);
}

void MiniSynthEditor::addKnob(juce::Slider &slider, juce::Label &label) {
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void MiniSynthEditor::resized() {
    // Split into the two rows first, then each row into four equal columns. The top of each row is
    // left free for the labels, which sit above their controls.
    auto area = getLocalBounds().reduced(20);
    auto topRow = area.removeFromTop(area.getHeight() / 2).withTrimmedTop(30);
    auto bottomRow = area.withTrimmedTop(30);
    const int columnWidth = topRow.getWidth() / 4;
    const auto column = [columnWidth](juce::Rectangle<int> &row) {
        return row.removeFromLeft(columnWidth).reduced(10, 0);
    };
    levelSlider.setBounds(column(topRow));
    cutoffSlider.setBounds(column(topRow));
    resonanceSlider.setBounds(column(topRow));
    const auto waveformColumn = column(topRow);
    waveformBox.setBounds(waveformColumn.withHeight(28).withY(waveformColumn.getCentreY() - 14));
    attackSlider.setBounds(column(bottomRow));
    decaySlider.setBounds(column(bottomRow));
    sustainSlider.setBounds(column(bottomRow));
    releaseSlider.setBounds(column(bottomRow));
}
