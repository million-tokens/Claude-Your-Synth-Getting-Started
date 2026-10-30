#pragma once

#include "PluginProcessor.h"

class MiniSynthEditor final : public juce::AudioProcessorEditor {
  public:
    explicit MiniSynthEditor(MiniSynthProcessor &owner);

    void resized() override;

  private:
    void addKnob(juce::Slider &slider, juce::Label &label);

    juce::Slider levelSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label levelLabel{{}, "Level"};
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;
    juce::Slider cutoffSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label cutoffLabel{{}, "Cutoff"};
    juce::AudioProcessorValueTreeState::SliderAttachment cutoffAttachment;
    juce::Slider resonanceSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label resonanceLabel{{}, "Resonance"};
    juce::AudioProcessorValueTreeState::SliderAttachment resonanceAttachment;
    juce::ComboBox waveformBox;
    juce::Label waveformLabel{{}, "Waveform"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
};
