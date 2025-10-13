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
    
    bool start();   // ±âº» ÀåÄ¡ ¿­±â + ÄÝ¹é µî·Ï
    void stop();    // ÄÝ¹é Á¦°Å + ÀåÄ¡ ´Ý±â

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
private:
    juce::AudioDeviceManager dm;

    renderFn render_;                 // »ý¼ºÀÚ¿¡¼­ ¹ÞÀº ·»´õ ÄÝ¹é º¸°ü
    double sampleRate_ = 0.0;
    int blockSize_ = 0;
    int outCh_ = 0;
    std::vector<uint8_t> bypass_;

    // interleaved ÀÓ½Ã ¹öÆÛ(ÄÝ¹é¸¶´Ù ÀçÇÒ´ç ÇÇÇÏ·Á°í º¸°ü)
    std::vector<float> interBuf_;

    juce::AudioBuffer<float> procBuf_;                  // ÇÃ·¯±×ÀÎ Ã³¸®¿ë ¹öÆÛ
    juce::MidiBuffer midi_;                             // (Áö±ÝÀº ºñ¿ò)
    juce::SpinLock plugLock_;                           // Ã¼ÀÎ ±³Ã¼ º¸È£
    std::vector<juce::AudioProcessor*> plugs_;
};
