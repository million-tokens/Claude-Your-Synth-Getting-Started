#include "PluginEditor.h"

MiniSynthEditor::MiniSynthEditor(MiniSynthProcessor &owner)
    : AudioProcessorEditor(owner), levelAttachment(owner.getState(), MiniSynthProcessor::levelId, levelSlider),
      cutoffAttachment(owner.getState(), MiniSynthProcessor::cutoffId, cutoffSlider),
      resonanceAttachment(owner.getState(), MiniSynthProcessor::resonanceId, resonanceSlider),
      attackAttachment(owner.getState(), MiniSynthProcessor::attackId, attackSlider),
      decayAttachment(owner.getState(), MiniSynthProcessor::decayId, decaySlider),
      sustainAttachment(owner.getState(), MiniSynthProcessor::sustainId, sustainSlider),
      releaseAttachment(owner.getState(), MiniSynthProcessor::releaseId, releaseSlider),
      lfoRateAttachment(owner.getState(), MiniSynthProcessor::lfoRateId, lfoRateSlider),
      lfoDepthAttachment(owner.getState(), MiniSynthProcessor::lfoDepthId, lfoDepthSlider) {
    addKnob(levelSlider, levelLabel);
    addKnob(cutoffSlider, cutoffLabel);
    addKnob(resonanceSlider, resonanceLabel);
    addKnob(attackSlider, attackLabel);
    addKnob(decaySlider, decayLabel);
    addKnob(sustainSlider, sustainLabel);
    addKnob(releaseSlider, releaseLabel);

    addChoiceBox(owner.getState(), waveformBox, waveformLabel, MiniSynthProcessor::waveformId, waveformAttachment);
    addChoiceBox(owner.getState(), lfoShapeBox, lfoShapeLabel, MiniSynthProcessor::lfoShapeId, lfoShapeAttachment);
    addKnob(lfoRateSlider, lfoRateLabel);
    addKnob(lfoDepthSlider, lfoDepthLabel);
    setSize(560, 580);
}

void MiniSynthEditor::addChoiceBox(
    juce::AudioProcessorValueTreeState &state, juce::ComboBox &box, juce::Label &label, const char *parameterId,
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> &attachment) {
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&box, false);
    // The attachment only selects an item, so fill the box from the parameter's own choices first.
    if (const auto *choice = dynamic_cast<juce::AudioParameterChoice *>(state.getParameter(parameterId))) {
        box.addItemList(choice->choices, 1);
    }
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, parameterId, box);
    addAndMakeVisible(box);
    addAndMakeVisible(label);
}

void MiniSynthEditor::addKnob(juce::Slider &slider, juce::Label &label) {
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void MiniSynthEditor::resized() {
    // Split into the three rows first, then each row into four equal columns. The top of each row is
    // left free for the labels, which sit above their controls.
    auto area = getLocalBounds().reduced(20);
    const int rowHeight = area.getHeight() / 3;
    auto topRow = area.removeFromTop(rowHeight).withTrimmedTop(30);
    auto middleRow = area.removeFromTop(rowHeight).withTrimmedTop(30);
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
    attackSlider.setBounds(column(middleRow));
    decaySlider.setBounds(column(middleRow));
    sustainSlider.setBounds(column(middleRow));
    releaseSlider.setBounds(column(middleRow));
    const auto lfoShapeColumn = column(bottomRow);
    lfoShapeBox.setBounds(lfoShapeColumn.withHeight(28).withY(lfoShapeColumn.getCentreY() - 14));
    lfoRateSlider.setBounds(column(bottomRow));
    lfoDepthSlider.setBounds(column(bottomRow));
}
