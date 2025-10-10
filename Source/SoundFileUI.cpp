/*
  ==============================================================================

    SoundFileUI.cpp
    Created: 6 Aug 2025 4:52:15pm
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SoundFileUI.h"

//==============================================================================
SoundFileUI::SoundFileUI()
{
    soundListBox.setModel(soundPanel.get());
    soundListBox.setRowHeight(24);
    addAndMakeVisible(soundListBox);
    soundListBox.addMouseListener(this, true);
    soundPanel->onItemClicked = [this](const juce::File& f, int row, const juce::MouseEvent& e)
        {
            // ¼±ÅÃ ¹Ý¿µÇÏ°í
            soundListBox.selectRow(row);
            auto p = f.getFullPathName();
            sample_path(p.toRawUTF8());
            // ¿©±â¼­ MainComponent·Î Àü´ÞÇÏ°Å³ª, »ùÇÃ ·Îµå/¹Ì¸®µè±â/¿ìÅ¬¸¯ ¸Þ´º µî Ã³¸®
        };
}

SoundFileUI::~SoundFileUI()
{
}

void SoundFileUI::paint (juce::Graphics& g)
{
    
}

void SoundFileUI::resized()
{
    soundListBox.setBounds(getLocalBounds());
}

void SoundFileUI::addItem(const juce::File& file)
{
    soundPanel->items.add(file);
    soundListBox.updateContent();
}
void SoundFileUI::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (soundPanel->items.size() > 24)
    {
        soundListBox.mouseWheelMove(event, wheel);
    }
}

