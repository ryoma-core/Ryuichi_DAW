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
    
    bool start();   
    void stop();    

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,           //real data
                                                           int numInputChannels,         // input channels 1 mono 2 stereotype
                                                float* const* outputChannelData,        // sound out data
                                                           int numOutputChannels,      // out channels 
                                                           int numSamples,            // BS
                     const juce::AudioIODeviceCallbackContext& context) override;    //?

    void addPlugin(juce::AudioProcessor* p, bool initiallyBypassed = false);
    void removePlugin(juce::AudioProcessor* p);
    void clearPlugins();


    inline double currentSampleRate() const noexcept { return sampleRate_; } 
    inline int    currentBlockSize()  const noexcept { return blockSize_; }
    inline int    currentOutChannels() const noexcept { return outCh_; }

    void setBypassed(juce::AudioProcessor* p, bool shouldBypass);
    bool isBypassed(juce::AudioProcessor* p) const;

    bool prepareForOffline(double sampleRate, int blockSize);
    void processChainOffline(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void releaseOffline();
    int  getTotalLatencySamples() const;
    inline double getJitterP95Ms() const { return jitterP95Ms.load(std::memory_order_relaxed); }
private:
    juce::AudioDeviceManager dm;

    renderFn render_;                 
    double sampleRate_ = 0.0;
    int blockSize_ = 0;
    int outCh_ = 0;
    std::vector<uint8_t> bypass_;

    // interleaved
    std::vector<float> interBuf_;

    juce::AudioBuffer<float> procBuf_;                  
    juce::MidiBuffer midi_;                             
    juce::SpinLock plugLock_;                      
    std::vector<juce::AudioProcessor*> plugs_;

    // Jitter metrics
    std::atomic<double> jitterP95Ms{ 0.0 };
    juce::int64 lastTick = 0;

    // 가벼운 링버퍼 (최근 512개)
    std::array<double, 512> jitterBuf{};
    std::atomic<size_t>     jitterCount{ 0 };
    std::atomic<size_t>     jitterIdx{ 0 };
};
