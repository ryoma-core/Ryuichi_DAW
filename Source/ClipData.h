/*
  ==============================================================================

    ClipData.h
    Created: 2 Sep 2025 1:34:26pm
    Author:  KGA

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

struct ClipData {
    std::unique_ptr<juce::AudioThumbnail> thumb;
    juce::File file;

    // 소스 좌표(파일 기준)
    uint64_t startSrcSamples = 0;
    uint64_t lenSrcSamples = 0;
    double   srcSampleRate = 48000.0;

    uint64_t startProjFrames = 0;
    uint64_t lenProjFrames = 0;
    static constexpr double kBaseBpm = 60.0;

    ClipData(juce::AudioFormatManager& fm,
        juce::AudioThumbnailCache& cache,
        const juce::File& f,
        uint64_t startSrcS,
        uint64_t lenSrcS) : thumb(std::make_unique<juce::AudioThumbnail>(512, fm, cache))
        , file(f)
        , startSrcSamples(startSrcS)
        , lenSrcSamples(lenSrcS)
    {
        thumb->setSource(new juce::FileInputSource(file));
        if (auto* r = fm.createReaderFor(file)) { srcSampleRate = r->sampleRate; delete r; }
    }
    
    void recalcProjectFrames(double outputSR, double bpm) {
        const double tempoRatio = juce::jlimit(0.25, 4.0, bpm / kBaseBpm);
        const double scale = (outputSR / srcSampleRate) / tempoRatio;
        startProjFrames = (uint64_t)std::llround((double)startSrcSamples * scale);
        lenProjFrames = (uint64_t)std::llround((double)lenSrcSamples * scale);

    }
};