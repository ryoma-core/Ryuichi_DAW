/*
  ==============================================================================

    SubTrackVolumeSlider.cpp
    Created: 6 Aug 2025 3:37:55pm
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SubTrackVolumeSlider.h"

//==============================================================================
SubTrackVolumeSlider::SubTrackVolumeSlider()
{
    setSliderStyle(juce::Slider::LinearHorizontal); // ?? ¼öÆò ½½¶óÀÌ´õ
    setRange(0.0, 1.0, 0.01); // º¼·ý ¹üÀ§
    setValue(0.5); // ÃÊ±â°ª
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // ÅØ½ºÆ® ¹Ú½º ¼û±è

    onValueChange = [this]()
        {
            DBG("º¼·ý º¯°æµÊ: " << getValue());
            // ¿©±â¼­ Rust ÂÊÀ¸·Î °ª Àü´ÞÇÏ´Â ·ÎÁ÷ ³ÖÀ¸¸é µÊ
        };
}

SubTrackVolumeSlider::~SubTrackVolumeSlider()
{
}

void SubTrackVolumeSlider::paint (juce::Graphics& g)
{
  
}

void SubTrackVolumeSlider::resized()
{
   
}
