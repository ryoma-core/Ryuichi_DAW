/*
  ==============================================================================

    AudioHostController.cpp
    Created: 23 Sep 2025 5:46:12pm
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioHostController.h"

//==============================================================================
AudioHostController::AudioHostController(renderFn fn) : render_(std::move(fn))
{

}

AudioHostController::~AudioHostController() = default; //default is {} same

bool AudioHostController::start()
{
    auto err = dm.initialise(/*numInput*/ 0, /*numOutput*/ 2, nullptr, true);
    if (err.isNotEmpty())
    {
        return false;
    }
    dm.addAudioCallback(this);
    return true;
}

void AudioHostController::stop()
{
    dm.removeAudioCallback(this);
    dm.closeAudioDevice();
}


void AudioHostController::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    this->sampleRate_ = device ? device->getCurrentSampleRate() : 0.0;  //get SR
    this->blockSize_ = device ? device->getCurrentBufferSizeSamples() : 0; //get BS
    this->outCh_ = device ? device->getActiveOutputChannels().countNumberOfSetBits() : 0;

    if (outCh_ <= 0) outCh_ = 2;

    this->interBuf_.resize((size_t)this->blockSize_ * (size_t)outCh_);
    this->procBuf_.setSize(outCh_, blockSize_, false, false, true); //false is origin data not preservation
                                                                    //false is inside the new space no erase
                                                                    //true is possible underneath rebuffer
    if (onAboutToStart) onAboutToStart(sampleRate_, blockSize_, outCh_);

    const juce::SpinLock::ScopedLockType sl(plugLock_);
    for (auto& p : plugs_)
    {
        const auto set = (outCh_ == 1) ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout{ set, set }; //inLayout , outLayout

        if (!p->setBusesLayout(layout))
        {
            // stereo만 강제 시도
            juce::AudioProcessor::BusesLayout stereo{ juce::AudioChannelSet::stereo(),
                                                      juce::AudioChannelSet::stereo() };
            p->setBusesLayout(stereo); // 이것도 실패할 수 있음
        }

        p->prepareToPlay(sampleRate_, blockSize_);
    }
}

void AudioHostController::audioDeviceStopped()
{
    this->sampleRate_ = 0.0;
    this->blockSize_ = 0;
    this->outCh_ = 0;
    this->interBuf_.clear();
    this->interBuf_.shrink_to_fit(); //resize
}

void AudioHostController::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    juce::ScopedNoDenormals guard; //is cpu poor performance prevention. just Denormal Numbers is cpu calculation be heavy so occur Denormal Numbers the 0 return
    if (numSamples <= 0 || numOutputChannels <= 0 || outputChannelData == nullptr) return; //nullpoint return;

    //interleaved temp
    const size_t need = static_cast<size_t>(numSamples) * static_cast<size_t>(numOutputChannels); // mono ? stereotype get frame
    if (interBuf_.size() < need) //buffer setting 
    {
        interBuf_.resize(need); //buffer setting is the BS 
    }
    float* inter = interBuf_.data(); //get one soundcallback data?

    //get engine audio (interleaved)
    size_t producedFrames = 0; //create Frames
    if (render_)  //callback start
    {
        producedFrames = render_(inter, static_cast<size_t>(numSamples), numOutputChannels); //return one Callback Frames enclose
        if (producedFrames > static_cast<size_t>(numSamples))  // BS < oneCallBack  UP is Error defense
        {
            producedFrames = static_cast<size_t>(numSamples);
        }
    }
    if (producedFrames < static_cast<size_t>(numSamples)) // BS < oneCallBack Data Down is Error underrun
    {
        const size_t start = producedFrames * static_cast<size_t>(numOutputChannels); 
                            //(mute point) start = enclose * channels
        const size_t remain = (static_cast<size_t>(numSamples) - producedFrames) * static_cast<size_t>(numOutputChannels);
                            //(mute lan) remain = (BS - enclose) * channels
        std::fill(inter + start, inter + start + remain, 0.0f);
    }

    //ByePass
    {
        const juce::SpinLock::ScopedLockType sl(plugLock_);
        if (plugs_.empty())
        {
            // interleaved -> device outputs (채널별로 건네주기)
            for (int ch = 0; ch < numOutputChannels; ++ch) {
                float* out = outputChannelData[ch];
                if (!out) continue;
                const float* src = inter + ch;
                for (int i = 0; i < numSamples; ++i)
                    out[i] = src[i * numOutputChannels];
            }
            return;
        }
    }

    //pluging
    if (procBuf_.getNumChannels() < numOutputChannels || procBuf_.getNumSamples() < numSamples)
    {
        procBuf_.setSize(numOutputChannels, numSamples, false, false, true);
    }
    deinterleaveToBuffer(inter, numSamples, numOutputChannels, procBuf_);
    midi_.clear();
    {
        const juce::SpinLock::ScopedLockType sl(plugLock_);
        for (auto& p : plugs_) {
            const int needIn = p->getTotalNumInputChannels(); //getInChannels
            const int needOut = p->getTotalNumOutputChannels(); //getOutChannels
            const int needCh = std::max({ needIn, needOut, numOutputChannels }); //maxer plugin Channels
            if (procBuf_.getNumChannels() < needCh) //resetting procBuf_ Channels
                procBuf_.setSize(needCh, numSamples, false, false, true); 

            p->processBlock(procBuf_, midi_); //in VST the proceBuf
        }
    }
    copyBufferToDeviceOutputs(procBuf_, outputChannelData, numOutputChannels, numSamples); //change procBuf_ is callback outdata
}

void AudioHostController::addPlugin(juce::AudioProcessor* p)
{
    if (!p) return;
    const juce::SpinLock::ScopedLockType sl(plugLock_);

    // 현재 디바이스 레이아웃하고 맞추기
    if (outCh_ > 0 && sampleRate_ > 0.0 && blockSize_ > 0)
    {
        auto set = (outCh_ == 1) ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout{ set, set };
        (void)p->setBusesLayout(layout); // 실패해도 일단 진행 (플러그인에 따라 다름)
        p->prepareToPlay(sampleRate_, blockSize_);
        p->suspendProcessing(false);
    }
    plugs_.push_back(p);
}

void AudioHostController::removePlugin(juce::AudioProcessor* p)
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    auto it = std::find(plugs_.begin(), plugs_.end(), p);
    if (it != plugs_.end())
        plugs_.erase(it);
}

void AudioHostController::clearPlugins()
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    plugs_.clear(); // 소유권 없으니 release는 MainComponent에서 함
}
