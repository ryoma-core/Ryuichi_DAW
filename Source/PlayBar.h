/*
  ==============================================================================

    PlayBar.h
    Created: 8 Aug 2025 10:23:44am
    Author:  KGA

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PlayToggleButton.h"
#include "StopToggleButton.h"
#include "ReverbToggleButton.h"
#include "BPM.h"
#include "AssetsPath.h"

#define TITLE_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("PlayBar.png"))
#define PLAY_ON_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("play_Button_on.png"))
#define PLAY_OFF_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("play_Button_off.png"))
#define STOP_ON_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("Stop_Button_on.png"))
#define STOP_OFF_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("Stop_Button_off.png"))
#define BPMTEXT_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("BPMText.png"))
#define REVERB_ON_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("reverb_on.png"))
#define REVERB_OFF_DIR_PATH (Path::assetsDir().getChildFile("UI_Image").getChildFile("reverb_off.png"))
//==============================================================================
/*
*/
class PlayBar  : public juce::Component
{
public:
    PlayBar();
    ~PlayBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    PlayToggleButton playToggleButton;
    StopToggleButton stopToggleButton;
    ReverbToggleButton reverbToggleButton;
    BPM bpm;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayBar)
    juce::ImageComponent titleImage;
    juce::ImageComponent bpmTextImage;
};
