#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "SynthMath.h"

#include <algorithm>

namespace {
constexpr double rampSeconds = 0.005;
} // namespace

MiniSynthProcessor::MiniSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "MiniSynth", createLayout()) {
    levelParam = state.getRawParameterValue(levelId);
    levelParameter = state.getParameter(levelId);
    waveformParam = state.getRawParameterValue(waveformId);
}

juce::AudioProcessorValueTreeState::ParameterLayout MiniSynthProcessor::createLayout() {
    const auto toText = [](float value, int) { return juce::String(juce::roundToInt(value * 100.0F)) + " %"; };
    const auto fromText = [](const juce::String &text) { return text.getFloatValue() / 100.0F; };
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(levelId, 1), "Level", juce::NormalisableRange<float>(0.0F, 1.0F), 0.4F,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(toText).withValueFromStringFunction(
            fromText)));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(waveformId, 1), "Waveform",
                                                            juce::StringArray{"Sine", "Saw", "Square", "Noise"}, 1));
    return layout;
}

void MiniSynthProcessor::prepareToPlay(double newSampleRate, int) {
    sampleRate = newSampleRate;
    level.reset(sampleRate, rampSeconds * 4.0);
    level.setCurrentAndTargetValue(levelParam->load());
    noteGain.reset(sampleRate, rampSeconds);
    noteGain.setCurrentAndTargetValue(0.0F);
    phase = 0.0;
    currentNote = -1;
}

void MiniSynthProcessor::releaseResources() {
}

bool MiniSynthProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MiniSynthProcessor::handleMidi(const juce::MidiMessage &message) {
    if (message.isNoteOn()) {
        currentNote = message.getNoteNumber();
        phaseIncrement = synthmath::midiNoteToFrequency(currentNote) / sampleRate;
        noteGain.setTargetValue(synthmath::velocityToAmplitude(message.getVelocity()));
    } else if (message.isNoteOff()) {
        if (message.getNoteNumber() == currentNote) {
            currentNote = -1;
            noteGain.setTargetValue(0.0F);
        }
    } else if (message.isController() && message.getControllerNumber() == synthmath::levelControlChange) {
        levelParameter->setValueNotifyingHost(synthmath::controlChangeToLevel(message.getControllerValue()));
    }
}

void MiniSynthProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    buffer.clear();

    auto event = midi.begin();
    int position = 0;
    while (position < numSamples) {
        while (event != midi.end() && (*event).samplePosition <= position) {
            handleMidi((*event).getMessage());
            ++event;
        }
        // A CC may just have moved the parameter, so read the target after the MIDI is handled.
        level.setTargetValue(levelParam->load());
        const auto waveform = static_cast<synthmath::Waveform>(juce::roundToInt(waveformParam->load()));
        const int next = event != midi.end() ? std::min((*event).samplePosition, numSamples) : numSamples;
        for (; position < next; ++position) {
            const auto sample =
                synthmath::waveSample(waveform, phase, noiseState) * noteGain.getNextValue() * level.getNextValue();
            phase += phaseIncrement;
            if (phase >= 1.0) {
                phase -= 1.0;
            }
            for (int channel = 0; channel < numChannels; ++channel) {
                buffer.getWritePointer(channel)[position] = sample;
            }
        }
    }
}

juce::AudioProcessorEditor *MiniSynthProcessor::createEditor() {
    return new MiniSynthEditor(*this);
}

bool MiniSynthProcessor::hasEditor() const {
    return true;
}

const juce::String MiniSynthProcessor::getName() const {
    return JucePlugin_Name;
}

bool MiniSynthProcessor::acceptsMidi() const {
    return true;
}

bool MiniSynthProcessor::producesMidi() const {
    return false;
}

double MiniSynthProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int MiniSynthProcessor::getNumPrograms() {
    return 1;
}

int MiniSynthProcessor::getCurrentProgram() {
    return 0;
}

void MiniSynthProcessor::setCurrentProgram(int) {
}

const juce::String MiniSynthProcessor::getProgramName(int) {
    return {};
}

void MiniSynthProcessor::changeProgramName(int, const juce::String &) {
}

void MiniSynthProcessor::getStateInformation(juce::MemoryBlock &destData) {
    if (const auto xml = state.copyState().createXml()) {
        copyXmlToBinary(*xml, destData);
    }
}

void MiniSynthProcessor::setStateInformation(const void *data, int sizeInBytes) {
    if (const auto xml = getXmlFromBinary(data, sizeInBytes)) {
        if (xml->hasTagName(state.state.getType())) {
            state.replaceState(juce::ValueTree::fromXml(*xml));
        }
    }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new MiniSynthProcessor();
}
