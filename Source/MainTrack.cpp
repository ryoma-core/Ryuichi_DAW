/*
  ==============================================================================

    MainTrack.cpp
    Created: 5 Aug 2025 5:45:23pm
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainTrack.h"

//==============================================================================
MainTrack::MainTrack()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
#pragma region Imag
    
    juce::File windowImg((Path::assetsDir().getChildFile("UI_Image").getChildFile("TrackBar.png")));
    if (windowImg.existsAsFile())
    {
        juce::Image img = juce::ImageFileFormat::loadFrom(windowImg);
        WindowBarComponent.setImage(img);
        addAndMakeVisible(&WindowBarComponent);
    }
   
    juce::File mainTrackImg((Path::assetsDir().getChildFile("UI_Image").getChildFile("TrackBackGround.png")));
    if (mainTrackImg.existsAsFile())
    {
        juce::Image img = juce::ImageFileFormat::loadFrom(mainTrackImg);
        mainTrackBackGround.setImage(img);
        addAndMakeVisible(&mainTrackBackGround);
        mainTrackBackGround.setInterceptsMouseClicks(false, false);
    }
#pragma endregion
#pragma region SubTrack
    addAndMakeVisible(subTrack_0.get());
    addAndMakeVisible(subTrack_1.get());
    addAndMakeVisible(subTrack_2.get());
    addAndMakeVisible(subTrack_3.get());
    addAndMakeVisible(subTrackController_0.get());
    addAndMakeVisible(subTrackController_1.get());
    addAndMakeVisible(subTrackController_2.get());
    addAndMakeVisible(subTrackController_3.get());
#pragma endregion
#pragma region CloseButton
    if (mainTrackCloseButton != nullptr)
    {
        addAndMakeVisible(mainTrackCloseButton.get());
        setVisible(true);
    mainTrackCloseButton->onClick = [this]()
        {
            DBG("MainTrackExit");
            setVisible(false);
        };
    }
#pragma endregion
#pragma region playhead
    playhead.setRange(0.0, 48000.0 * 600.0, 1.0);
    playhead.setSliderStyle(juce::Slider::LinearBar);
    playhead.setColour(juce::Slider::trackColourId, juce::Colours::whitesmoke);
    playhead.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(playhead);
#pragma endregion
}

MainTrack::~MainTrack()
{
}

void MainTrack::paint (juce::Graphics& g)
{
   
}

void MainTrack::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
#pragma region Imag or CloseButton
    WindowBarComponent.setBounds(0, 0, 1200, 40);
    mainTrackCloseButton->setBounds(1160, 5, 30, 30);
    mainTrackBackGround.setBounds(0, 40, 1200, 600);
#pragma endregion
#pragma region SubTrackController
    subTrackController_0->setBounds(1, 105, 110, 110);
    subTrackController_1->setBounds(1, 220, 110, 110);
    subTrackController_2->setBounds(1, 335, 110, 110);
    subTrackController_3->setBounds(1, 450, 110, 110);
    subTrack_0->setBounds(109, 105, 1090, 110);
    subTrack_1->setBounds(109, 220, 1090, 110);
    subTrack_2->setBounds(109, 335, 1090, 110);
    subTrack_3->setBounds(109, 450, 1090, 110);
#pragma endregion
#pragma region playhead
    playhead.setBounds(109, 90 , 1090, 10);
#pragma endregion
}

void MainTrack::itemDropped(const juce::DragAndDropTarget::SourceDetails& d)
{
    auto p = d.localPosition.toInt(); // MainTrack 기준

    struct Lane { SubTrack* comp; int index; } 
    lanes[] = {
        { subTrack_0.get(), 0 }, { subTrack_1.get(), 1 },
        { subTrack_2.get(), 2 }, { subTrack_3.get(), 3 }
    };

    for (auto& L : lanes)
    {
        if (L.comp && L.comp->getBounds().contains(p))
        {
            auto lanePt = L.comp->getLocalPoint(this, p);
            const float laneX = (float)lanePt.x;


            const juce::String droppedPath = d.description.toString();
            const juce::File droppedFile(droppedPath);

            if (droppedFile.existsAsFile() && onDropIntoSubTrack)
                onDropIntoSubTrack(L.index, droppedFile, laneX);
            return;
        }
    }
}
bool MainTrack::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    return true;
}

void MainTrack::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) // 맥 ctrl+클릭도 지원
    {
        juce::PopupMenu menu;
        menu.addItem(1, "export Wav");

        auto screenPt = event.getScreenPosition();
        juce::Rectangle<int> anchor(screenPt.x, screenPt.y, 1, 1);

        menu.showMenuAsync(
            juce::PopupMenu::Options()
            .withTargetScreenArea(anchor),   // **이것만** 쓰기!
            [this](int choice)
            {
                if (choice == 1 && onExportWav)
                    onExportWav();
            }
        );
    }
}