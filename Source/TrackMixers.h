/*
  ==============================================================================

    TrackMixers.h
    Created: 7 Aug 2025 3:53:04pm
    Author:  KGA

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DelayToggleButton.h"
//#include "ReverbToggleButton.h"
#include "VolumeKnobLookAndFeel.h"
#include "AssetsPath.h"

#define TRACKMIXER_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("MixerTrackImg.png"))
//#define REVERB_ON_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("reverb_on.png"))
//#define REVERB_OFF_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("reverb_off.png"))
#define DELAY_ON_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("delay_on.png"))
#define DELAY_OFF_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("delay_off.png"))
//==============================================================================
/*
*/
class TrackMixers  : public juce::Component
{
public:
    TrackMixers();
    ~TrackMixers() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider volumeKnob;
    /*DelayToggleButton delayToggleButton;*/
    /*ReverbToggleButton reverbToggleButton;*/
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackMixers)
    juce::ImageComponent trackMixerImg;
    VolumeKnobLookAndFeel volumeKnobLookAndFeel;
};
