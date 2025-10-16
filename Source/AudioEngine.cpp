/*
  ==============================================================================

    AudioEngine.cpp
    Created: 11 Aug 2025 11:50:37am
    Author:  KGA

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioEngine.h"
#include "MainComponent.h"

//==============================================================================
AudioEngine::AudioEngine()
{
    TrackDatas* rust_track_0 =rust_audio_track_new();
    TrackDatas* rust_track_1 = rust_audio_track_new();
    TrackDatas* rust_track_2 = rust_audio_track_new();
    TrackDatas* rust_track_3 = rust_audio_track_new();
    Engine* raw = rust_audio_engine_new(rust_track_0, rust_track_1, rust_track_2, rust_track_3);
    if (!raw) {
        rust_audio_track_free(rust_track_0);
        rust_audio_track_free(rust_track_1);
        rust_audio_track_free(rust_track_2);
        rust_audio_track_free(rust_track_3);
        return;
    }
    eng.reset(raw);

    auto renderFromRust = [this](float* inter, size_t frames, int ch)->size_t { // 0. Lamda callback fun output
        if (!eng) return 0;
        return rust_render_interleaved(eng.get(), inter, frames, static_cast<uint32_t>(ch));
        };
    host_ = std::make_unique<AudioHostController>(renderFromRust); //create obj audio is Lama callback fun input
    host_->onAboutToStart = [this] (double sr, int,int) { //output onAboutToStart
        if (eng) rust_engine_set_sr(eng.get(), (uint32_t)sr);
        };
    host_->start(); //start Just App open one App delete is stop
}

AudioEngine::~AudioEngine()
{
    if (host_) host_->stop();
}

void AudioEngine::paint (juce::Graphics& g)
{

}

void AudioEngine::resized()
{

}

void AudioEngine::rust_start_sound(bool bstart)
{
    if (bstart)
    {  
        if (rust_sound_play(eng.get())) { DBG("[rust_sound_play] ok");}
        else { DBG("[rust_sound_play] error"); }
    }
    else 
    {
        if (rust_sound_stop(eng.get())) { DBG("[rust_sound_stop] ok"); }
        else { { DBG("[rust_sound_stop] error"); } }
    }
}



bool AudioEngine::rust_file_update(int32_t number, const char* path, uint64_t tl_start, uint64_t tl_len, uint32_t src)
{
    return rust_sound_add_clip(eng.get(), number, path, tl_start, tl_len, src);
}

bool AudioEngine::rust_file_move(int32_t old_track, uint64_t old_start, int32_t new_track, uint64_t new_start)
{
    return rust_sound_move_clip_by_start(eng.get(), old_track, old_start, new_track, new_start);
}

bool AudioEngine::rust_file_delet(int32_t track, uint64_t start)
{
    return rust_sound_delete_clip_by_start(eng.get(), track, start);
}

bool AudioEngine::rust_volume_update(float volume, int tracknum)
{
    if (tracknum < 0 || tracknum >= 4) { return false; }
    return rust_sound_volume_update(eng.get(), volume, tracknum);
}

bool AudioEngine::rust_mute_update(bool muted, int tracknum)
{
    if (tracknum < 0 || tracknum >= 4) { return false; }
    return rust_sound_mute_update(eng.get(), muted, tracknum);
}

bool AudioEngine::rust_pan_update(float pan, int tracknum)
{
    if (tracknum < 0 || tracknum >= 4) { return false; }
    return rust_sound_pan_update(eng.get(), pan, tracknum);
}

bool AudioEngine::rust_bpm_update(float bpm)
{
    return rust_sound_bpm_update(eng.get(),bpm);
}

uint64_t AudioEngine::rust_get_pos()
{
    return rust_transport_pos(eng.get());
   
}

uint32_t AudioEngine::rust_get_sr()
{
    return rust_transport_sr(eng.get());
    
}

bool AudioEngine::rust_get_is_playing()
{
    return rust_transport_is_playing(eng.get());
}

bool AudioEngine::rust_set_play_time(uint64_t s)
{
    return rust_sound_seek(eng.get(),s);
}

uint32_t AudioEngine::rust_get_out_sr()
{
    return rust_audio_params_out_sr(eng.get());
}

uint32_t AudioEngine::rust_get_out_bs()
{
    return rust_audio_params_out_bs(eng.get());
}

void AudioEngine::rust_sample_add(const char* path)
{
    if (rust_request_load_single_sample(eng.get(), path))
    {
        DBG("RUST_Sample_ADD_OK");
    }
}

void AudioEngine::rust_sample_play()
{
    if (rust_pad_note_on(eng.get()))
    {
        DBG("RUST_Sample_PLAY");
    }
}

void AudioEngine::rust_sample_stop()
{
    if (rust_pad_note_off(eng.get()))
    {
        DBG("RUST_Sample_STOP");
    }
}

void AudioEngine::rust_save_wav()
{
    juce::File outFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
        .getChildFile("Ryuicni.wav");
    outFile.deleteFile();

    const bool     wasPlaying = rust_get_is_playing();
    const uint64_t prevPos = rust_get_pos();
    if (host_) host_->stop();

    const uint32_t sr = juce::jmax<uint32_t>(44100u, rust_get_out_sr());
    const uint32_t block = juce::jmax<uint32_t>(256u, rust_get_out_bs());
    const uint64_t songFrames = rust_project_length_frames(eng.get());

    if (!host_ || !host_->prepareForOffline((double)sr, (int)block)) {
        DBG("[Export] offline prepare failed");
        if (host_) host_->start();
        return;
    }
    const int latency = juce::jmax(0, host_->getTotalLatencySamples());
    const uint64_t tailFrames = (uint64_t)latency; 

    rust_engine_set_sr(eng.get(), sr);
    rust_set_play_time(0);
    rust_start_sound(true);

 
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> fos(outFile.createOutputStream());
    if (!fos || !fos->openedOk()) {
        DBG("[Export] cannot open: " + outFile.getFullPathName());
        rust_start_sound(false);
        rust_set_play_time(prevPos);
        host_->releaseOffline();
        if (wasPlaying) rust_start_sound(true);
        if (host_) host_->start();
        return;
    }
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(fos.release(), (double)sr, 2, 24, {}, 0));
    if (!writer) {
        DBG("[Export] createWriterFor failed");
        rust_start_sound(false);
        rust_set_play_time(prevPos);
        host_->releaseOffline();
        if (wasPlaying) rust_start_sound(true);
        if (host_) host_->start();
        return;
    }

    std::vector<float> inter(block * 2, 0.0f);       
    juce::AudioBuffer<float> buf(2, (int)block); 
    juce::MidiBuffer midi;

    int headLeft = latency;


    uint64_t rendered = 0;
    const uint64_t targetFrames = songFrames + tailFrames;

    while (rendered < targetFrames)
    {
        const uint32_t todo = (uint32_t)juce::jmin<uint64_t>(block, targetFrames - rendered);

        size_t got = rust_render_interleaved(eng.get(), inter.data(), (size_t)todo, 2);
        if (got == 0) { juce::Thread::sleep(1); continue; }

        buf.clear();
        float* L = buf.getWritePointer(0);
        float* R = buf.getWritePointer(1);
        for (size_t i = 0; i < got; ++i) {
            L[i] = inter[i * 2 + 0];
            R[i] = inter[i * 2 + 1];
        }

        midi.clear();
        host_->processChainOffline(buf, midi);


        int writeOffset = 0;
        int writeCount = (int)got;
        if (headLeft > 0) {
            const int skip = juce::jmin(headLeft, (int)got);
            headLeft -= skip;
            writeOffset += skip;
            writeCount -= skip;
        }

        if (writeCount > 0)
            writer->writeFromAudioSampleBuffer(buf, writeOffset, writeCount);

        rendered += got;
    }

    writer.reset();
    rust_start_sound(false);
    rust_set_play_time(prevPos);
    host_->releaseOffline();

    if (wasPlaying) rust_start_sound(true);
    if (host_) host_->start();

    DBG("[Export] DONE -> " + outFile.getFullPathName());
}
