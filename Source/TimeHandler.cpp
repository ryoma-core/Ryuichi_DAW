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
    timeline.sr = static_cast<double>(sr);
    if (playhead.getMaximum() < (double)pos) playhead.setRange(0.0, (double)pos * 1.1, 1.0);

    playhead.setValue((double)pos, juce::dontSendNotification);

    // UI 갱신(트랙만)
    if (auto* p = playhead.getParentComponent())
        p->repaint(); // 또는 각 SubTrack만 repaint하도록 콜백 훅 쓰기
}
