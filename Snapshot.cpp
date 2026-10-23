// Renders the plugin editor to a PNG without a host.
#include "PluginProcessor.h"
#include "PluginEditor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    TwelveStringerProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    for (int i = 0; i < 15; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.f);
    juce::File out (argv[1]); out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", argv[1], img.getWidth(), img.getHeight());
    proc.editorBeingDeleted (ed.get());
    return 0;
}
