/*
  ==============================================================================

    TimeHandler.cpp
    Created: 8 Sep 2025 9:43:53am
    Author:  KGA

  ==============================================================================
*/

#include "TimeHandler.h"
#include "AudioEngine.h"

TimeHandler::TimeHandler(AudioEngine& aeng, juce::Slider& playhead, TimeLine::timeLineState& tl, bool& isplay, uint64_t& subtrack) : playhead(playhead), aEng(aeng), timeline(tl), isPlaying(isplay), subTime(&subtrack)
{
    startTimerHz(60);
}

TimeHandler::~TimeHandler()
{
}

void TimeHandler::timerCallback()
{
    const auto sr = aEng.rust_get_sr();
    const auto pos = aEng.rust_get_pos();
    isPlaying = aEng.rust_get_is_playing();
    *subTime = pos;
    timeline.sr = sr;
    playhead.setValue((double)pos, juce::dontSendNotification);
    if (isPlaying) {
        aEng.rust_eng_tick();
    }
}
