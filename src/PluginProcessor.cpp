#include "PluginProcessor.h"

MiniSynthProcessor::MiniSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
}

void MiniSynthProcessor::prepareToPlay(double, int) {
}

void MiniSynthProcessor::releaseResources() {
}

bool MiniSynthProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MiniSynthProcessor::processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) {
}

juce::AudioProcessorEditor *MiniSynthProcessor::createEditor() {
    return nullptr;
}

bool MiniSynthProcessor::hasEditor() const {
    return false;
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

void MiniSynthProcessor::getStateInformation(juce::MemoryBlock &) {
}

void MiniSynthProcessor::setStateInformation(const void *, int) {
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new MiniSynthProcessor();
}
