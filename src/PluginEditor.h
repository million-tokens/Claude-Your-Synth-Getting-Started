#pragma once

#include "PluginProcessor.h"

class MiniSynthEditor final : public juce::AudioProcessorEditor {
  public:
    explicit MiniSynthEditor(MiniSynthProcessor &owner);

    void resized() override;

  private:
    void addKnob(juce::Slider &slider, juce::Label &label);
    void addChoiceBox(juce::AudioProcessorValueTreeState &state, juce::ComboBox &box, juce::Label &label,
                      const char *parameterId,
                      std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> &attachment);

    juce::Slider levelSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label levelLabel{{}, "Level"};
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;
    juce::Slider cutoffSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label cutoffLabel{{}, "Cutoff"};
    juce::AudioProcessorValueTreeState::SliderAttachment cutoffAttachment;
    juce::Slider resonanceSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label resonanceLabel{{}, "Resonance"};
    juce::AudioProcessorValueTreeState::SliderAttachment resonanceAttachment;
    juce::Slider attackSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label attackLabel{{}, "Attack"};
    juce::AudioProcessorValueTreeState::SliderAttachment attackAttachment;
    juce::Slider decaySlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label decayLabel{{}, "Decay"};
    juce::AudioProcessorValueTreeState::SliderAttachment decayAttachment;
    juce::Slider sustainSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label sustainLabel{{}, "Sustain"};
    juce::AudioProcessorValueTreeState::SliderAttachment sustainAttachment;
    juce::Slider releaseSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label releaseLabel{{}, "Release"};
    juce::AudioProcessorValueTreeState::SliderAttachment releaseAttachment;
    juce::ComboBox waveformBox;
    juce::Label waveformLabel{{}, "Waveform"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
    juce::ComboBox lfoShapeBox;
    juce::Label lfoShapeLabel{{}, "LFO Shape"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;
    juce::Slider lfoRateSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label lfoRateLabel{{}, "LFO Rate"};
    juce::AudioProcessorValueTreeState::SliderAttachment lfoRateAttachment;
    juce::Slider lfoDepthSlider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label lfoDepthLabel{{}, "LFO Depth"};
    juce::AudioProcessorValueTreeState::SliderAttachment lfoDepthAttachment;
};
