/*
  ==============================================================================

    MainTrack.h
    Created: 5 Aug 2025 5:45:23pm
    Author:  KGA

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "CloseButtonBrowser.h"
#include "SubTrackController.h"
#include "SubTrack.h"

//==============================================================================
/*
*/
class MainTrack  : public juce::Component,public juce::DragAndDropTarget
{
public:
    MainTrack();
    ~MainTrack() override;
    std::function<void(int trackIndex, const juce::File& file, float laneLocalX)>onDropIntoSubTrack;
    std::function<void(int)> handleMenuSelection;
    std::function<void()> onExportWav;
    void mouseDown(const juce::MouseEvent& event) override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& d) override;
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    std::unique_ptr<SubTrackController> subTrackController_0 = std::make_unique<SubTrackController>();
    std::unique_ptr<SubTrackController> subTrackController_1 =  std::make_unique<SubTrackController>();
    std::unique_ptr<SubTrackController> subTrackController_2 = std::make_unique<SubTrackController>();
    std::unique_ptr<SubTrackController> subTrackController_3 = std::make_unique<SubTrackController>();
    std::unique_ptr<SubTrack> subTrack_0 = std::make_unique<SubTrack>();
    std::unique_ptr<SubTrack> subTrack_1 = std::make_unique<SubTrack>();
    std::unique_ptr<SubTrack> subTrack_2 = std::make_unique<SubTrack>();
    std::unique_ptr<SubTrack> subTrack_3 = std::make_unique<SubTrack>();

    juce::Slider playhead;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainTrack)
    juce::ImageComponent mainTrackBackGround;
    juce::ImageComponent WindowBarComponent;
    std::unique_ptr<CloseButtonBrowser> mainTrackCloseButton = std::make_unique<CloseButtonBrowser>();
};
