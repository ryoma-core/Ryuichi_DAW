/*
  ==============================================================================

    TimeHandler.h
    Created: 8 Sep 2025 9:43:53am
    Author:  KGA

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "TimeLineState.h"

class AudioEngine;

class TimeHandler : public juce::Timer
{
public:
    TimeHandler(AudioEngine& aeng, juce::Slider& playhead, TimeLine::timeLineState& tl,bool& isplay, uint64_t& subtrack);
    ~TimeHandler();

    void timerCallback() override;
private:
    juce::Slider& playhead;
    AudioEngine& aEng;
    TimeLine::timeLineState& timeline;
    bool& isPlaying;
    uint64_t* subTime;
};