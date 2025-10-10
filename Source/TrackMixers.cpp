/*
  ==============================================================================

    TrackMixers.cpp
    Created: 7 Aug 2025 3:53:04pm
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "TrackMixers.h"

//==============================================================================
TrackMixers::TrackMixers()
{
#pragma region TrackMixer
    juce::File trackMixerFile(TRACKMIXER_DIR_PATH);
    if (trackMixerFile.existsAsFile())
    {
        juce::Image trackImg = juce::ImageFileFormat::loadFrom(trackMixerFile);
        trackMixerImg.setImage(trackImg);
        addAndMakeVisible(trackMixerImg);
    }
#pragma endregion
#pragma region DealyButton
    //juce::File delayOnButtonFile(DELAY_ON_DIR_PATH);
    //juce::File delayOffButtonFile(DELAY_OFF_DIR_PATH);
    //if (delayOnButtonFile.existsAsFile() && delayOffButtonFile.existsAsFile())
    //{
    //    juce::Image delayOnImg = juce::ImageFileFormat::loadFrom(delayOnButtonFile);
    //    juce::Image delayOffImg = juce::ImageFileFormat::loadFrom(delayOffButtonFile);

    //    delayToggleButton.setImages(delayOnImg, delayOffImg);
    //    addAndMakeVisible(delayToggleButton);
    //    delayToggleButton.setBounds(225, 100, 33, 33);
    //}
#pragma endregion
//#pragma region ReverbButton
//    juce::File reverbOnButtonFile(REVERB_ON_DIR_PATH);
//    juce::File reverbOffButtonFile(REVERB_OFF_DIR_PATH);
//    if (reverbOnButtonFile.existsAsFile() && reverbOffButtonFile.existsAsFile())
//    {
//        juce::Image reverbOnImg = juce::ImageFileFormat::loadFrom(reverbOnButtonFile);
//        juce::Image reverbOffImg = juce::ImageFileFormat::loadFrom(reverbOffButtonFile);
//
//        reverbToggleButton.setImages(reverbOnImg, reverbOffImg);
//        addAndMakeVisible(reverbToggleButton);
//        reverbToggleButton.setBounds(45, 100, 33, 33);
//    }
//#pragma endregion
#pragma region VolumeKnob
    volumeKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeKnob.setRange(-1.0, 1.0, 0.01);
    volumeKnob.setValue(0.0);
    volumeKnob.setLookAndFeel(&volumeKnobLookAndFeel);
    addAndMakeVisible(volumeKnob);
#pragma endregion
}

TrackMixers::~TrackMixers()
{
}

void TrackMixers::paint (juce::Graphics& g)
{

}

void TrackMixers::resized()
{
    trackMixerImg.setBounds(getLocalBounds());
    volumeKnob.setBounds(96, 50, 100, 100);
}
