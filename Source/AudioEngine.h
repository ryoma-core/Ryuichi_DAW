/*
  ==============================================================================

    AudioEngine.h
    Created: 11 Aug 2025 11:50:37am
    Author:  KGA

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cstdint>
#include "soundData.h"
#include "AudioHostController.h"
extern "C"
{
    struct Engine;
    struct TrackDatas;
    TrackDatas* rust_audio_track_new();
    void rust_audio_track_free(TrackDatas* track);

    Engine* rust_audio_engine_new(TrackDatas* track0, TrackDatas* track1, TrackDatas* track2, TrackDatas* track3);
    void rust_audio_engine_free(Engine* engine);

    bool rust_sound_play(Engine* engine);
    bool rust_sound_stop(Engine* engine);

    bool rust_sound_add_clip(Engine* engine, int32_t number, const char* path, uint64_t tl_start, uint64_t tl_len, uint32_t src);
    bool rust_sound_move_clip_by_start(Engine* engine, int32_t old_track, uint64_t old_start, int32_t new_track, uint64_t new_start);
    bool rust_sound_delete_clip_by_start(Engine* engine, int32_t track, uint64_t start);
    bool rust_sound_volume_update(Engine* engine, float volume, std::int32_t number);
    bool rust_sound_mute_update(Engine* engine, bool mute, std::int32_t number);
    bool rust_sound_pan_update(Engine* engine, float pan, std::int32_t number);
    bool rust_sound_bpm_update(Engine* engine, float bpm);

    size_t rust_render_interleaved(Engine* engine, float* inter, size_t frames, uint32_t ch);
    void rust_engine_set_sr(Engine* engine, uint32_t sr);

    uint64_t rust_transport_pos(Engine* engine);
    uint32_t rust_transport_sr (Engine* engine);
    bool rust_transport_is_playing(Engine* engine);
    bool rust_sound_seek(Engine* engine, uint64_t s);
    uint32_t rust_audio_params_out_sr(Engine* engine);
    uint32_t rust_audio_params_out_bs(Engine* engine);

    bool rust_request_load_single_sample(Engine* engine, const char* path);
    bool rust_pad_note_on(Engine* engine);
    bool rust_pad_note_off(Engine* engine);

    uint64_t rust_project_length_frames(Engine* engine);
    double rust_project_length_seconds(Engine* engine);

    uint64_t rust_metrics_get_xrun_callbacks(Engine* engine);
    uint64_t rust_metrics_get_xrun_zero_samples(Engine* engine);
    void rust_metrics_reset(Engine* engine);
}
struct EngineDeleter {
    void operator()(Engine* e) const noexcept {
        if (e) rust_audio_engine_free(e);
    }
};
using EnginePtr = std::unique_ptr<Engine, EngineDeleter>;

class MainComponent;
//==============================================================================
/*
*/
class AudioEngine  : public juce::Component
{
public:
    AudioEngine();
    ~AudioEngine() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void rust_start_sound(bool bstart);
    bool rust_file_update(int32_t number, const char* path, uint64_t tl_start, uint64_t tl_len, uint32_t src);
    bool rust_file_move(int32_t old_track, uint64_t old_start, int32_t new_track, uint64_t new_start);
    bool rust_file_delet(int32_t track, uint64_t start);
    bool rust_volume_update(float volume , int tracknum);
    bool rust_mute_update(bool muted , int tracknum);
    bool rust_pan_update(float pan, int tracknum);
    bool rust_bpm_update(float bpm);
    uint64_t rust_get_pos();
    uint32_t rust_get_sr();
    bool rust_get_is_playing();
    bool rust_set_play_time(uint64_t s);
    uint32_t rust_get_out_sr();
    uint32_t rust_get_out_bs();
    void rust_sample_add(const char* path);
    void rust_sample_play();
    void rust_sample_stop();
    void rust_save_wav();
    uint64_t getXrunCallbacks() const;
    uint64_t getXrunZeroSamples() const;
    void     resetMetrics();
    std::shared_ptr<SoundCore::soundVecterData> audioTrack_0;
    std::shared_ptr<SoundCore::soundVecterData> audioTrack_1;
    std::shared_ptr<SoundCore::soundVecterData> audioTrack_2;
    std::shared_ptr<SoundCore::soundVecterData> audioTrack_3;
    EnginePtr eng;

    std::unique_ptr<AudioHostController> host_ = nullptr;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
