#include "PluginEditor.h"

MiniSynthEditor::MiniSynthEditor(MiniSynthProcessor &owner)
    : AudioProcessorEditor(owner), levelAttachment(owner.getState(), MiniSynthProcessor::levelId, levelSlider) {
    levelLabel.setJustificationType(juce::Justification::centred);
    levelLabel.attachToComponent(&levelSlider, false);
    addAndMakeVisible(levelSlider);
    addAndMakeVisible(levelLabel);
    setSize(160, 180);
}

void MiniSynthEditor::resized() {
    levelSlider.setBounds(getLocalBounds().withTrimmedTop(30).reduced(20));
}
