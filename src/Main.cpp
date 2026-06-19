#include <juce_core/juce_core.h>

int main() {
    const juce::String version = juce::SystemStats::getJUCEVersion();
    std::cout << version.toStdString() << " — build successful!\n";
    std::cout << "Get ready for the workshop by million-tokens.com\n";
    return 0;
}
