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
    uint64_t startS = 0;   // ½ÃÀÛ(»ùÇÃ)
    uint64_t lenS = 0;   // ±æÀÌ(»ùÇÃ)

    ClipData(juce::AudioFormatManager& fm,
        juce::AudioThumbnailCache& cache,
        const juce::File& f,
        uint64_t startSamples,
        uint64_t lengthSamples)
        : thumb(std::make_unique<juce::AudioThumbnail>(512, fm, cache)),
        file(f), startS(startSamples), lenS(lengthSamples)
    {
        thumb->setSource(new juce::FileInputSource(file));
    }
};