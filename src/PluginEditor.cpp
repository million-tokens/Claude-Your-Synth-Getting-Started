#include "PluginEditor.h"

MiniSynthEditor::MiniSynthEditor(MiniSynthProcessor &owner)
    : AudioProcessorEditor(owner), levelAttachment(owner.getState(), MiniSynthProcessor::levelId, levelSlider),
      cutoffAttachment(owner.getState(), MiniSynthProcessor::cutoffId, cutoffSlider),
      resonanceAttachment(owner.getState(), MiniSynthProcessor::resonanceId, resonanceSlider) {
    addKnob(levelSlider, levelLabel);
    addKnob(cutoffSlider, cutoffLabel);
    addKnob(resonanceSlider, resonanceLabel);

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
    setSize(560, 180);
}

void MiniSynthEditor::addKnob(juce::Slider &slider, juce::Label &label) {
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void MiniSynthEditor::resized() {
    auto area = getLocalBounds().withTrimmedTop(30).reduced(20);
    const int columnWidth = area.getWidth() / 4;
    levelSlider.setBounds(area.removeFromLeft(columnWidth).reduced(10, 0));
    cutoffSlider.setBounds(area.removeFromLeft(columnWidth).reduced(10, 0));
    resonanceSlider.setBounds(area.removeFromLeft(columnWidth).reduced(10, 0));
    waveformBox.setBounds(area.reduced(10, 0).withHeight(28).withY(area.getCentreY() - 14));
}
