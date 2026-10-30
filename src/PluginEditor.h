#pragma once

#include "PluginProcessor.h"

class MiniSynthEditor final : public juce::AudioProcessorEditor {
  public:
    explicit MiniSynthEditor(MiniSynthProcessor &owner);

    void resized() override;

  private:
    juce::Slider levelSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label levelLabel{{}, "Level"};
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;
};
