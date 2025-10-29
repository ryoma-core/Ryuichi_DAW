use crate::Clip;
use crate::Engine;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::sync::atomic::Ordering;

#[no_mangle]
pub extern "C" fn rust_sound_add_clip(
    engine: *mut Engine,
    number: i32,
    path: *const c_char,
    tl_start: u64,
    tl_len: u64,
    src: u32,
) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng: &mut Engine = unsafe { &mut *engine };
    let idx = match usize::try_from(number) {
        Ok(number) => number,
        Err(_) => return false,
    };
    if idx >= eng.track.len() || idx >= eng.track_run_time.len() {
        return false;
    }
    if tl_len == 0 {
        return false;
    }

    let c_str = unsafe { CStr::from_ptr(path) };
    let path_str = c_str.to_string_lossy().into_owned();

    if let Some(mx) = eng.track_run_time.get(idx) {
        if let Ok(mut tr) = mx.lock() {
            if tr.clips.contains_key(&tl_start) {
                return false;
            } // 중복 금지
            let clip = Clip {
                file_path: path_str,
                src_sr: src,
                tl_start,
                tl_len,
            };
            tr.clips.insert(tl_start, clip);
            return true;
        }
    }
    false
}

#[no_mangle]
pub extern "C" fn rust_sound_move_clip_by_start(
    engine: *mut Engine,
    old_track: i32,
    old_start: u64,
    new_track: i32,
    new_start: u64,
) -> bool {
    if engine.is_null() {
        return false;
    }

    let eng: &mut Engine = unsafe { &mut *engine };

    let old_idx = match usize::try_from(old_track) {
        Ok(n) => n,
        Err(_) => return false,
    };
    let new_idx = match usize::try_from(new_track) {
        Ok(n) => n,
        Err(_) => return false,
    };

    if old_idx >= eng.track_run_time.len() || new_idx >= eng.track_run_time.len() {
        return false;
    }
    if old_idx == new_idx && old_start == new_start {
        return true;
    }

    // 동일 트랙: 단일 락으로 원자적 처리
    if old_idx == new_idx {
        if let Some(mx) = eng.track_run_time.get(old_idx) {
            if let Ok(mut tr) = mx.lock() {
                // 락 획득
                let Some(mut clip) = tr.clips.remove(&old_start) else {
                    return false;
                }; // 이동할 클립이 없으면 실패
                if tr.clips.contains_key(&new_start) {
                    // 충돌 검사
                    tr.clips.insert(old_start, clip); // 원복
                    return false;
                }
                clip.tl_start = new_start;
                tr.clips.insert(new_start, clip);
                return true;
            }
        }
        return false;
    }

    // 서로 다른 트랙: 락 순서 고정(min→max)로 데드락 방지 + 원복 보장
    let (first, second) = if old_idx < new_idx {
        (old_idx, new_idx)
    } else {
        (new_idx, old_idx)
    };
    let (mx_first, mx_second) = match (
        eng.track_run_time.get(first),
        eng.track_run_time.get(second),
    ) {
        (Some(a), Some(b)) => (a, b),
        _ => return false,
    };

    let g1 = mx_first.lock();
    if g1.is_err() {
        return false;
    }
    let mut t_first = g1.unwrap();
    let g2 = mx_second.lock();
    if g2.is_err() {
        return false;
    }
    let mut t_second = g2.unwrap();

    let (src, dst) = if old_idx == first {
        (&mut *t_first, &mut *t_second)
    } else {
        (&mut *t_second, &mut *t_first)
    };

    let Some(mut clip) = src.clips.remove(&old_start) else {
        return false;
    };
    if dst.clips.contains_key(&new_start) {
        src.clips.insert(old_start, clip); // 충돌 → 원복
        return false;
    }
    clip.tl_start = new_start;
    dst.clips.insert(new_start, clip);
    true
}

#[no_mangle]
pub extern "C" fn rust_sound_delete_clip_by_start(
    engine: *mut Engine,
    track: i32,
    start: u64,
) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng: &mut Engine = unsafe { &mut *engine };
    let idx = match usize::try_from(track) {
        Ok(number) => number,
        Err(_) => return false,
    };
    if idx >= eng.track.len() || idx >= eng.track_run_time.len() {
        return false;
    }

    if let Some(tr_mx) = eng.track_run_time.get(idx) {
        if let Ok(mut tr) = tr_mx.lock() {
            return tr.clips.remove(&start).is_some();
        }
    }
    false
}

#[no_mangle]
pub extern "C" fn rust_sound_volume_update(engine: *mut Engine, volume: f32, number: i32) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng: &mut Engine = unsafe { &mut *engine };
    let idx = match usize::try_from(number) {
        Ok(number) => number,
        Err(_) => return false,
    };
    let v = volume.clamp(0.0, 1.0);
    eng.track[idx].volume = v;
    eng.real_time_params.volume[idx].store(v.to_bits(), Ordering::Relaxed); //실시간 반영
    true
}

#[no_mangle]
pub extern "C" fn rust_sound_mute_update(engine: *mut Engine, mute: bool, number: i32) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng: &mut Engine = unsafe { &mut *engine };
    let idx = match usize::try_from(number) {
        Ok(number) => number,
        Err(_) => return false,
    };
    if idx >= eng.track.len() {
        return false;
    }
    eng.track[idx].muted = mute;
    eng.real_time_params.muted[idx].store(mute, Ordering::Relaxed); //실시간 반영
    true
}

#[no_mangle]
pub extern "C" fn rust_sound_pan_update(engine: *mut Engine, pan: f32, number: i32) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng: &mut Engine = unsafe { &mut *engine };
    let idx = match usize::try_from(number) {
        Ok(number) => number,
        Err(_) => return false,
    };
    if idx >= eng.track.len() {
        return false;
    }
    let p = if pan.is_finite() {
        pan.clamp(-1.0, 1.0)
    } else {
        0.0
    };
    eng.track[idx].pan = p;
    eng.real_time_params.pan[idx].store(p.to_bits(), Ordering::Relaxed); //실시간 반영
    true
}

#[no_mangle]
pub extern "C" fn rust_sound_bpm_update(engine: *mut Engine, bpm: f32) -> bool {
     if engine.is_null() { return false; }
    let eng: &mut Engine = unsafe { &mut *engine };

    let b = bpm.clamp(20.0, 300.0);
    let cur = f32::from_bits(eng.real_time_params.bpm.load(Ordering::Relaxed));
    if (cur - b).abs() < 0.0001 { return true; }

    // 방법 A) pending 플래그 써서 rebuffer에서 적용
    eng.pending_bpm.store(b.to_bits(), Ordering::Relaxed);
    eng.has_pending_bpm.store(true, Ordering::Release);
    eng.rebuffer_current();
    true
}

#[no_mangle]
pub extern "C" fn rust_transport_pos(engine: *mut Engine) -> u64 {
    if engine.is_null() {
        return 0;
    }
    let eng = unsafe { &mut *engine };
    eng.play_time_manager.pos_frames()
}

#[no_mangle]
pub extern "C" fn rust_transport_sr(engine: *mut Engine) -> u32 {
    if engine.is_null() {
        return 48000;
    }
    let eng = unsafe { &mut *engine };
    eng.play_time_manager.sr()
}

#[no_mangle]
pub extern "C" fn rust_transport_is_playing(engine: *const Engine) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng = unsafe { &*engine };
    eng.play_time_manager.in_playing()
}

#[no_mangle]
pub extern "C" fn rust_audio_params_out_sr(engine: *mut Engine) -> u32 {
    if engine.is_null() {
        return 0;
    }
    let eng = unsafe { &*engine };
    eng.play_time_manager.sr()
}
pub const ENGINE_BLOCK_SIZE: u32 = 256;
#[no_mangle]
pub extern "C" fn rust_audio_params_out_bs(_engine: *mut Engine) -> u32 {
    ENGINE_BLOCK_SIZE
}

#[no_mangle]
pub extern "C" fn rust_engine_set_sr(engine: *mut Engine, sr: u32) {
    if engine.is_null() {
        return;
    }
    let eng = unsafe { &*engine };
    eng.play_time_manager.set_sr(sr);
}

#[no_mangle]
pub extern "C" fn rust_project_length_frames(engine: *mut Engine) -> u64 {
    if engine.is_null() {
        return 0;
    }
    let eng = unsafe { &mut *engine };
    eng.project_end_frames()
}

#[no_mangle]
pub extern "C" fn rust_project_length_seconds(engine: *const Engine) -> f64 {
    if engine.is_null() { return 0.0; }
    let eng = unsafe { &*engine };
    eng.project_length_seconds()
}

#[no_mangle]
pub extern "C" fn rust_metrics_get_xrun_callbacks(eng: *mut Engine) -> u64 {
    if eng.is_null() { return 0; }
    unsafe { (&*eng).underrun_callbacks.load(Ordering::Relaxed) }
}

#[no_mangle]
pub extern "C" fn rust_metrics_get_xrun_zero_samples(eng: *mut Engine) -> u64 {
    if eng.is_null() { return 0; }
    unsafe { (&*eng).underrun_samples.load(Ordering::Relaxed) }
}

#[no_mangle]
pub extern "C" fn rust_metrics_reset(eng: *mut Engine) {
    if eng.is_null() { return; }
    let e = unsafe { &*eng };
    e.underrun_callbacks.store(0, Ordering::Relaxed);
    e.underrun_samples.store(0, Ordering::Relaxed);
}