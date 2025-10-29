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
    lastTick = 0;
    if (outCh_ <= 0) outCh_ = 2;

    this->interBuf_.resize((size_t)this->blockSize_ * (size_t)outCh_);
    this->procBuf_.setSize(outCh_, blockSize_, false, false, true); //false is origin data not preservation
                                                                    //false is inside the new space no erase
                                                                    //true is possible underneath rebuffer
    if (onAboutToStart) onAboutToStart(sampleRate_, blockSize_, outCh_);

    const juce::SpinLock::ScopedLockType sl(plugLock_);
    if (bypass_.size() < plugs_.size())
        bypass_.resize(plugs_.size(), 0u);
    else if (bypass_.size() > plugs_.size())
        bypass_.resize(plugs_.size());
    const auto wanted = (outCh_ == 1) ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    const juce::AudioProcessor::BusesLayout desired{ wanted, wanted };

    for (size_t i = 0; i < plugs_.size(); ++i)
    {
        auto* p = plugs_[i];
        if (!p) continue;

        // 레이아웃 같으면 안 건드림
        if (p->getBusesLayout() != desired)
            (void)p->setBusesLayout(desired);

        p->prepareToPlay(sampleRate_, blockSize_);

        const bool bp = (i < bypass_.size()) ? (bypass_[i] != 0u) : false;
        p->suspendProcessing(bp);
    }
}

void AudioHostController::audioDeviceStopped()
{
    this->sampleRate_ = 0.0;
    this->blockSize_ = 0;
    this->outCh_ = 0;
    this->interBuf_.clear();
    this->interBuf_.shrink_to_fit(); //resize
    lastTick = 0;
}

void AudioHostController::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    auto now = juce::Time::getHighResolutionTicks();
    if (lastTick != 0 && sampleRate_ > 0.0 && blockSize_ > 0) {
        const double dt = double(now - lastTick) / double(juce::Time::getHighResolutionTicksPerSecond()); // s
        const double ideal = double(numSamples) / sampleRate_;
        const double j_ms = std::abs(dt - ideal) * 1000.0;

        // lock-free 링버퍼 스타일로 누적
        size_t i = jitterIdx.fetch_add(1, std::memory_order_relaxed) % jitterBuf.size();
        jitterBuf[i] = j_ms;
        size_t n = jitterCount.fetch_add(1, std::memory_order_relaxed) + 1;

        // 일정 주기로 p95 근사 업데이트 (부하 적게)
        if ((i % 64) == 0) {
            const size_t take = std::min(n, jitterBuf.size());
            // 로컬 복사 후 nth_element
            std::vector<double> tmp;
            tmp.reserve(take);
            for (size_t k = 0; k < take; ++k) tmp.push_back(jitterBuf[k]);
            if (!tmp.empty()) {
                size_t p = size_t(std::floor(tmp.size() * 0.95));
                std::nth_element(tmp.begin(), tmp.begin() + p, tmp.end());
                jitterP95Ms.store(tmp[p], std::memory_order_relaxed);
            }
        }
    }
    lastTick = now;

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
        for (size_t i = 0; i < plugs_.size(); ++i)
        {
            auto* p = plugs_[i];
            if (!p) continue;

            const int needIn = p->getTotalNumInputChannels();
            const int needOut = p->getTotalNumOutputChannels();
            const int needCh = std::max({ needIn, needOut, numOutputChannels });

            if (procBuf_.getNumChannels() < needCh)
                procBuf_.setSize(needCh, numSamples, false, false, true);

            const bool bp = (i < bypass_.size()) ? (bypass_[i] != 0u) : false;
            if (bp) continue;

            p->processBlock(procBuf_, midi_);
        }
    }
    copyBufferToDeviceOutputs(procBuf_, outputChannelData, numOutputChannels, numSamples); //change procBuf_ is callback outdata
}

void AudioHostController::addPlugin(juce::AudioProcessor* p, bool initiallyBypassed)
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
        p->suspendProcessing(initiallyBypassed);
    }
    plugs_.push_back(p);
    bypass_.push_back(initiallyBypassed ? 1 : 0);
}

void AudioHostController::removePlugin(juce::AudioProcessor* p)
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    auto it = std::find(plugs_.begin(), plugs_.end(), p);
    if (it != plugs_.end())
    {
        const size_t idx = (size_t)std::distance(plugs_.begin(), it);
        plugs_.erase(it);
        if (idx < bypass_.size()) bypass_.erase(bypass_.begin() + (ptrdiff_t)idx);
    }
}

void AudioHostController::clearPlugins()
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    plugs_.clear(); // 소유권 없으니 release는 MainComponent에서 함
    bypass_.clear();
}

void AudioHostController::setBypassed(juce::AudioProcessor* p, bool shouldBypass)
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    auto it = std::find(plugs_.begin(), plugs_.end(), p);
    if (it == plugs_.end()) return;
    const size_t idx = (size_t)std::distance(plugs_.begin(), it);
    if (idx >= bypass_.size()) return;

    bypass_[idx] = shouldBypass ? 1u : 0u;

    // CPU 절약용으로 실제 suspend도 동기화
    p->suspendProcessing(shouldBypass);
}

bool AudioHostController::isBypassed(juce::AudioProcessor* p) const
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    auto it = std::find(plugs_.begin(), plugs_.end(), p);
    if (it == plugs_.end()) return true;
    const size_t idx = (size_t)std::distance(plugs_.begin(), it);
    if (idx >= bypass_.size()) return true;
    return bypass_[idx] != 0u;
}

bool AudioHostController::prepareForOffline(double sampleRate, int blockSize)
{
    sampleRate_ = sampleRate;
    blockSize_ = blockSize;
    outCh_ = 2; // 오프라인은 스테레오로 고정

    if (procBuf_.getNumChannels() < outCh_ || procBuf_.getNumSamples() < blockSize_)
        procBuf_.setSize(outCh_, blockSize_, false, false, true);

    {
        const juce::SpinLock::ScopedLockType sl(plugLock_);
        auto set = juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout{ set, set };

        if (bypass_.size() < plugs_.size())      bypass_.resize(plugs_.size(), 0u);
        else if (bypass_.size() > plugs_.size()) bypass_.resize(plugs_.size());

        for (auto* p : plugs_)
        {
            if (!p) continue;
            (void)p->setBusesLayout(layout); // 실패해도 진행
            p->prepareToPlay(sampleRate_, blockSize_);
            // bypass 상태는 기존 bypass_ 플래그로 유지; 실사용 시 processBlock만 skip
        }
    }
    return true;
}

void AudioHostController::processChainOffline(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midi)
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    for (size_t i = 0; i < plugs_.size(); ++i)
    {
        auto* p = plugs_[i];
        if (!p) continue;
        const bool bp = (i < bypass_.size()) ? (bypass_[i] != 0u) : false;
        if (bp) continue; // 바이패스면 스킵
        p->processBlock(buffer, midi);
    }
}

void AudioHostController::releaseOffline()
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    for (auto* p : plugs_) if (p) p->releaseResources();
}

int AudioHostController::getTotalLatencySamples() const
{
    const juce::SpinLock::ScopedLockType sl(plugLock_);
    int sum = 0;
    for (auto* p : plugs_) if (p) sum += p->getLatencySamples();
    return sum;
}