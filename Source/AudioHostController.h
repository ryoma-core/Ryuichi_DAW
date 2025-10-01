/*
  ==============================================================================

    AudioHostController.h
    Created: 23 Sep 2025 5:46:12pm
    Author:  KGA

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


static inline void deinterleaveToBuffer(const float* inter, int frames, int ch, juce::AudioBuffer<float>& buf) 
{
    jassert(buf.getNumChannels() >= ch);
    jassert(buf.getNumSamples() >= frames);
    for (int c = 0; c < ch; ++c) {
        float* dst = buf.getWritePointer(c); //in L , R startpoint get
        const float* src = inter + c;       // inter[0] L / inter[1] R   
        for (int i = 0; i < frames; ++i)            
            dst[i] = src[i * ch]; //startpoint[i] = L ,R
    }
}

static inline void copyBufferToDeviceOutputs(const juce::AudioBuffer<float>& buf, float* const* outputs, int ch, int frames)
{
    for (int c = 0; c < ch; ++c) { 
        if (auto* out = outputs[c]) {
            const float* src = buf.getReadPointer(c);
            std::memcpy(out, src, (size_t)frames * sizeof(float)); //memorey copy 
            //src copy in the out a size is (frames * sizeof(float) bytes) 
        }
    }
}

//==============================================================================
/*
*/
class AudioHostController : public juce::AudioIODeviceCallback
{
public:
    using renderFn = std::function<size_t(float* interleaved, size_t frames, int channels)>; //interleaved is one callback data
                                                                                            //frames is BS  one callback frames
                                                                                           //channels is mono streotype
    std::function<void(double sr, int bs, int ch)> onAboutToStart;
    explicit AudioHostController(renderFn fn = {});
    ~AudioHostController() override;
    
    bool start();   // 기본 장치 열기 + 콜백 등록
    void stop();    // 콜백 제거 + 장치 닫기

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,           //real data
                                                           int numInputChannels,         // input channels 1 mono 2 stereotype
                                                float* const* outputChannelData,        // sound out data
                                                           int numOutputChannels,      // out channels 
                                                           int numSamples,            // BS
                     const juce::AudioIODeviceCallbackContext& context) override;    //?

    void addPlugin(juce::AudioProcessor* p);
    void removePlugin(juce::AudioProcessor* p);
    void clearPlugins();


    inline double currentSampleRate() const noexcept { return sampleRate_; } 
    inline int    currentBlockSize()  const noexcept { return blockSize_; }
    inline int    currentOutChannels() const noexcept { return outCh_; }

private:
    juce::AudioDeviceManager dm;

    renderFn render_;                 // 생성자에서 받은 렌더 콜백 보관
    double sampleRate_ = 0.0;
    int blockSize_ = 0;
    int outCh_ = 0;

    // interleaved 임시 버퍼(콜백마다 재할당 피하려고 보관)
    std::vector<float> interBuf_;

    juce::AudioBuffer<float> procBuf_;                  // 플러그인 처리용 버퍼
    juce::MidiBuffer midi_;                             // (지금은 비움)
    juce::SpinLock plugLock_;                           // 체인 교체 보호
    std::vector<juce::AudioProcessor*> plugs_;
};
