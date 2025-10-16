/*
  ==============================================================================

    SubTrack.cpp
    Created: 6 Aug 2025 4:30:42pm
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SubTrack.h"

//==============================================================================
SubTrack::SubTrack()
{
#pragma region Img
    
    juce::File subTrackFile((Path::assetsDir().getChildFile("UI_Image").getChildFile("Track_Note.png")));
    if (subTrackFile.existsAsFile())
    {
        subTrackBackGround = juce::ImageFileFormat::loadFrom(subTrackFile);
    }
#pragma endregion
}

SubTrack::~SubTrack()
{
}

void SubTrack::paint (juce::Graphics& g)
{
    g.drawImage(subTrackBackGround, getLocalBounds().toFloat());

    if (timeline != nullptr)
        drawBeatGrid(g, getLocalBounds());

    if (clips && timeline)
    {
        // 프로젝트 좌표(출력 SR 기준 프레임) → 픽셀
        const double pxPerFrame = 1.0 / timeline->samplesPerPixel();
        const double viewStartF = timeline->xToSamples((float)getLocalBounds().getX());

        for (auto* c : *clips)
        {
            // 위치/폭: 프로젝트 프레임 사용
            const double xpx = ((double)c->startProjFrames - viewStartF) * pxPerFrame;
            const double wpx = (double)c->lenProjFrames * pxPerFrame;

            const int x = (int)std::floor(xpx);
            const int w = (int)std::ceil(juce::jmax(8.0, wpx));
            juce::Rectangle<int> r(x, 0, juce::jmax(8, w), getHeight());

            g.setColour(juce::Colours::dimgrey.darker(0.2f));
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);

            // 썸네일: 소스 구간만 그리기
            if (c->thumb && c->thumb->getTotalLength() > 0.0)
            {
                g.setColour(juce::Colours::lime);
                auto drawArea = r.reduced(2);
                const double srcStartSec = (double)c->startSrcSamples / c->srcSampleRate;
                const double srcEndSec = (double)(c->startSrcSamples + c->lenSrcSamples) / c->srcSampleRate;
                c->thumb->drawChannels(g, drawArea, srcStartSec, srcEndSec, 1.0f);
            }
        }
    }

    if (timeline != nullptr && playheadSamples)
    {
        const double frames = (double)*playheadSamples;        // 프로젝트 프레임
        const float  px = timeline->samplesToX(frames);    // 프레임→픽셀

        auto area = getLocalBounds();

        g.setColour(juce::Colours::red.withAlpha(0.10f));
        g.fillRect(juce::Rectangle<float>(px - 10.0f, (float)area.getY(),
            20.0f, (float)area.getHeight()));

        g.setColour(juce::Colours::red.withAlpha(0.95f));
        g.drawLine(px + 0.5f, (float)area.getY(), px + 0.5f, (float)area.getBottom(), 2.0f);

        juce::Path head;
        head.addTriangle(px - 4.0f, (float)area.getY(),
            px + 4.0f, (float)area.getY(),
            px, (float)area.getY() + 8.0f);
        g.fillPath(head);
    }
}

void SubTrack::resized()
{
}


void SubTrack::drawBeatGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    const double spb = timeline->samplesPerBeat();
    const double s0 = timeline->xToSamples((float)area.getX());
    const double s1 = timeline->xToSamples((float)area.getRight());
    const long long b0 = (long long)std::floor(s0 / spb) - 1;
    const long long b1 = (long long)std::ceil(s1 / spb) + 1;

    const double pxPerBar = timeline->pxPerBeat * timeline->num; // num=박자수(보통 4)
    const bool showLabels = pxPerBar > 40.0;
    const bool showSub = pxPerBar > 20.0;

    for (long long b = b0; b <= b1; ++b)
    {
        const bool isBar = (timeline->num > 0) ? ((b % timeline->num) == 0) : (b == 0);
        const float x = timeline->samplesToX((double)b * spb);

        if (isBar) {
            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.drawVerticalLine((int)std::round(x), area.getY(), area.getBottom());
            if (showLabels) {
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.drawText("Bar " + juce::String((int)(b / timeline->num) + 1),
                    (int)x + 3, area.getY() + 2, 60, 16, juce::Justification::left, false);
            }
        }
        else {
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawVerticalLine((int)std::round(x), area.getY(), area.getBottom());
        }

        if (showSub) {
            for (int k = 1; k < 4; ++k) {
                const double subs = (double)b * spb + k * (spb / 4.0);
                const float xs = timeline->samplesToX(subs);
                g.setColour(juce::Colours::white.withAlpha(0.06f));
                g.drawVerticalLine((int)std::round(xs), area.getY(), area.getBottom());
            }
        }
    }
}
