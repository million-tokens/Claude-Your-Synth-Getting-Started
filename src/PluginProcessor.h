#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>

class MiniSynthProcessor final : public juce::AudioProcessor {
  public:
    MiniSynthProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState &getState() {
        return state;
    }

    static constexpr auto levelId = "level";
    static constexpr auto waveformId = "waveform";

  private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleMidi(const juce::MidiMessage &message);

    juce::AudioProcessorValueTreeState state;
    std::atomic<float> *levelParam = nullptr;
    juce::RangedAudioParameter *levelParameter = nullptr;
    std::atomic<float> *waveformParam = nullptr;

    double sampleRate = 0.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    std::uint32_t noiseState = 0x12345678U;
    int currentNote = -1;
    juce::SmoothedValue<float> level;
    juce::SmoothedValue<float> noteGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniSynthProcessor)
};
