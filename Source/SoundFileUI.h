/*
  ==============================================================================

    SoundFileUI.h
    Created: 6 Aug 2025 4:52:15pm
    Author:  KGA

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SoundFilePanel.h"
//==============================================================================
/*
*/
class SoundFileUI  : public juce::Component 
{
public:
    SoundFileUI();
    ~SoundFileUI() override;
    std::function<void(const char* path)> sample_path;
    void paint (juce::Graphics&) override;
    void resized() override;
    void addItem(const juce::File& file);
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    juce::ListBox soundListBox;
    std::unique_ptr<SoundFilePanel> soundPanel= std::make_unique<SoundFilePanel>();
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundFileUI)
};
