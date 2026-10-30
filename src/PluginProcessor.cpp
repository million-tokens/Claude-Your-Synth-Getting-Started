#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "SynthMath.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double rampSeconds = 0.005;
} // namespace

MiniSynthProcessor::MiniSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "MiniSynth", createLayout()) {
    levelParam = state.getRawParameterValue(levelId);
    levelParameter = state.getParameter(levelId);
    waveformParam = state.getRawParameterValue(waveformId);
    cutoffParam = state.getRawParameterValue(cutoffId);
    cutoffParameter = state.getParameter(cutoffId);
    resonanceParam = state.getRawParameterValue(resonanceId);
    resonanceParameter = state.getParameter(resonanceId);
}

juce::AudioProcessorValueTreeState::ParameterLayout MiniSynthProcessor::createLayout() {
    const auto toText = [](float value, int) { return juce::String(juce::roundToInt(value * 100.0F)) + " %"; };
    const auto fromText = [](const juce::String &text) { return text.getFloatValue() / 100.0F; };
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(levelId, 1), "Level", juce::NormalisableRange<float>(0.0F, 1.0F), 0.4F,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(toText).withValueFromStringFunction(
            fromText)));
    const auto cutoffToText = [](float value, int) {
        return value < 1000.0F ? juce::String(juce::roundToInt(value)) + " Hz"
                               : juce::String(value / 1000.0F, value < 10000.0F ? 2 : 1) + " kHz";
    };
    const auto cutoffFromText = [](const juce::String &text) {
        const auto value = text.getFloatValue();
        return text.containsIgnoreCase("k") ? value * 1000.0F : value;
    };
    // Centre at the geometric mean, which makes the skew exactly logarithmic.
    juce::NormalisableRange<float> cutoffRange(synthmath::minCutoffHz, synthmath::maxCutoffHz);
    cutoffRange.setSkewForCentre(std::sqrt(synthmath::minCutoffHz * synthmath::maxCutoffHz));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(cutoffId, 1), "Cutoff", cutoffRange,
                                                           2000.0F,
                                                           juce::AudioParameterFloatAttributes()
                                                               .withStringFromValueFunction(cutoffToText)
                                                               .withValueFromStringFunction(cutoffFromText)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(resonanceId, 1), "Resonance", juce::NormalisableRange<float>(0.0F, 1.0F), 0.2F,
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
    cutoff.reset(sampleRate, rampSeconds * 4.0);
    cutoff.setCurrentAndTargetValue(cutoffParam->load());
    resonance.reset(sampleRate, rampSeconds * 4.0);
    resonance.setCurrentAndTargetValue(resonanceParam->load());
    filterState = {};
    filterCoefficients = synthmath::filterCoefficients(cutoffParam->load(), resonanceParam->load(), sampleRate);
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
    } else if (message.isController()) {
        const auto value = synthmath::controlChangeToNormalized(message.getControllerValue());
        switch (message.getControllerNumber()) {
        case synthmath::levelControlChange:
            levelParameter->setValueNotifyingHost(value);
            break;
        case synthmath::cutoffControlChange:
            cutoffParameter->setValueNotifyingHost(value);
            break;
        case synthmath::resonanceControlChange:
            resonanceParameter->setValueNotifyingHost(value);
            break;
        default:
            break;
        }
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
        cutoff.setTargetValue(cutoffParam->load());
        resonance.setTargetValue(resonanceParam->load());
        const auto waveform = static_cast<synthmath::Waveform>(juce::roundToInt(waveformParam->load()));
        const int next = event != midi.end() ? std::min((*event).samplePosition, numSamples) : numSamples;
        for (; position < next; ++position) {
            // Coefficients are recomputed only while a smoother is still moving.
            const bool moving = cutoff.isSmoothing() || resonance.isSmoothing();
            const float cutoffHz = cutoff.getNextValue();
            const float resonanceValue = resonance.getNextValue();
            if (moving) {
                filterCoefficients = synthmath::filterCoefficients(cutoffHz, resonanceValue, sampleRate);
            }
            const auto filtered = synthmath::filterTick(filterState, filterCoefficients,
                                                        synthmath::waveSample(waveform, phase, noiseState));
            const auto sample = filtered * noteGain.getNextValue() * level.getNextValue();
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
