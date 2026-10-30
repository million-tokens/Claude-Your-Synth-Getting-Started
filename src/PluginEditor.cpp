#include "PluginEditor.h"

MiniSynthEditor::MiniSynthEditor(MiniSynthProcessor &owner)
    : AudioProcessorEditor(owner), levelAttachment(owner.getState(), MiniSynthProcessor::levelId, levelSlider) {
    levelLabel.setJustificationType(juce::Justification::centred);
    levelLabel.attachToComponent(&levelSlider, false);
    addAndMakeVisible(levelSlider);
    addAndMakeVisible(levelLabel);

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
    setSize(320, 180);
}

void MiniSynthEditor::resized() {
    auto area = getLocalBounds().withTrimmedTop(30).reduced(20);
    levelSlider.setBounds(area.removeFromLeft(120));
    area.removeFromLeft(20);
    waveformBox.setBounds(area.withHeight(28).withY(area.getCentreY() - 14));
}
