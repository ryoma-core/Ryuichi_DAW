pub mod unit;
pub use unit::*;
mod sound_track_update;
pub use sound_track_update::*;
mod sound_play;
pub use sound_play::*;
use std::os::raw::c_char;
use crossbeam_utils::atomic::AtomicCell;
use std::collections::BTreeMap;
use std::sync::Condvar;


fn bump_priority_worker_thread() {
    unsafe {
        let h = GetCurrentThread(); //Windows API 핸들 얻기
        let _ = SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL); //우선순위 올리기
    }
}

fn pin_to_core(core_id: usize) {
    if let Some(core) = core_affinity::get_core_ids().and_then(|v| v.get(core_id).cloned()) //코어 ID 유효성 검사
    {
        let _ = core_affinity::set_for_current(core); //현재 스레드를 해당 코어에 고정
    }
}

pub struct CircularBuffer {
    producer: Option<Producer<f32>>, //디코더/프로듀서가 push할 핸들
    consumer: Option<Consumer<f32>>, //소비(믹서/출력)가 pop할 핸들
}

pub struct TrackConfig {
    volume: f32,
    muted: bool,
    pan: f32,
    circularbuffer: CircularBuffer,
}
impl TrackConfig {
    fn new() -> Result<Self, String> {
        let (tx, rx) = RingBuffer::<f32>::new(slots(RB1_FRAMES));
        //f32 타입에 배열을 생성 [CAPACITY_SAMPLES] 만큼
        let circularbuffer = CircularBuffer {
            producer: Some(tx),
            consumer: Some(rx),
        };
        Ok(Self {
            volume: 0.5,
            muted: false,
            pan: 0.0,
            circularbuffer: circularbuffer,
        })
    }
}

pub struct Parameters {
    volume: Vec<AtomicU32>,
    pan: Vec<AtomicU32>,
    muted: Vec<AtomicBool>,
    bpm: AtomicU32,
}
impl Parameters {
    fn from_tracks(track: &Vec<TrackConfig>) -> Self {
        let volume = track
            .iter()
            .map(|t| AtomicU32::new(t.volume.to_bits()))
            .collect();
        let pan = track
            .iter()
            .map(|t| AtomicU32::new(t.pan.to_bits()))
            .collect();
        let muted = track.iter().map(|t| AtomicBool::new(t.muted)).collect();
        let bpm = AtomicU32::new((60.0f32).to_bits());
        Self {
            volume,
            pan,
            muted,
            bpm,
        }
    }
}

pub struct Clip {
    file_path: String,
    src_sr: u32,
    tl_start: u64,
    tl_len: u64,
}
pub struct TrackTimeline {
    clips: BTreeMap<u64, Clip>, //시작시간,클립
    write_pos_frames: u64,      //현재 재생 위치
}
pub struct DecoderState {
    format: Box<dyn symphonia::core::formats::FormatReader>,
    decoder: Box<dyn symphonia::core::codecs::Decoder>,
    sample_buf: SampleBuffer<f32>,
    src_sr: u32,
    src_pos_samples: u64,
    file_path: String,
}

pub struct Transport {
    playing: AtomicBool,
    playhead_frames: AtomicU64,
    sample_rate: AtomicU32,
}
impl Transport {
    fn new(sr: u32) -> Self {
        Self {
            playing: AtomicBool::new(false),
            playhead_frames: AtomicU64::new(0),
            sample_rate: AtomicU32::new(sr),
        }
    }
    fn set_sr(&self, sr: u32) {
        //샘플링 레이트 설정
        self.sample_rate.store(sr, Ordering::Relaxed);
    }
    fn sr(&self) -> u32 {
        //샘플링 레이트
        self.sample_rate.load(Ordering::Relaxed)
    }
    fn start(&self) {
        //재생
        self.playing.store(true, Ordering::Relaxed);
    }
    fn stop(&self) {
        //정지
        self.playing.store(false, Ordering::Relaxed);
    }
    fn in_playing(&self) -> bool {
        //재생중인지
        self.playing.load(Ordering::Relaxed)
    }
    fn seek_frames(&self, s: u64) {
        //재생 위치를 s로 이동
        self.playhead_frames.store(s, Ordering::Relaxed);
    }
    fn pos_frames(&self) -> u64 {
        //현재 재생 위치
        self.playhead_frames.load(Ordering::Relaxed)
    }
    fn advance_frames(&self, s: u64) {
        //재생 위치를 s만큼 증가
        self.playhead_frames.fetch_add(s, Ordering::Relaxed);
    }
}

pub struct Sample {
    // interleaved L R L R … 길이 = 48_000 * 2 (1초, 48kHz, 스테레오)
    pub data: Arc<[f32]>,
    pub nframes: u32, // 48_000
}
struct SfxState {
    sample: Arc<Sample>,
    frame: usize,           // 현재 재생 위치 0..nframes
}

pub struct Engine {
    producers: Arc<Vec<Mutex<Producer<f32>>>>,
    consumers: Arc<Vec<Mutex<Consumer<f32>>>>,

    thread_worker: Vec<JoinHandle<()>>,
    thread_stop: Arc<AtomicBool>,             //스레드 종료
    thread_wait: Arc<(Mutex<bool>, Condvar)>, //전체 대기

    real_time_params: Arc<Parameters>,
    track_run_time: Arc<Vec<Mutex<TrackTimeline>>>,
    decod: Arc<Vec<Mutex<Option<DecoderState>>>>,

    play_time_manager: Arc<Transport>,
    seek_epoch: Arc<AtomicU64>,
    track: Vec<TrackConfig>,
    seek_lock: Arc<Mutex<()>>,
    pending_bpm: AtomicU32,
    has_pending_bpm: AtomicBool,
    pad_sample: AtomicCell<Option<Arc<Sample>>>,
    sfx_state: Mutex<Option<SfxState>>, 
}
impl Engine {
    fn new(mut tk: Vec<TrackConfig>) -> Self {
        // 1) 1차 링버퍼 소유권: TrackConfig에서 꺼내 Engine이 보관
        let mut prod_vec = Vec::with_capacity(tk.len());
        let mut cons_vec = Vec::with_capacity(tk.len());
        for tks in &mut tk {
            let tx = match tks.circularbuffer.producer.take() {
                Some(tx) => tx,
                None => panic!("[Engine::new] producer already taken (TrackConfig 재사용 가능성)"),
            };
            let rx = match tks.circularbuffer.consumer.take() {
                Some(rx) => rx,
                None => panic!("[Engine::new] consumer already taken (TrackConfig 재사용 가능성)"),
            };
            prod_vec.push(Mutex::new(tx));
            cons_vec.push(Mutex::new(rx));
        }
        let producers = Arc::new(prod_vec);
        let consumers = Arc::new(cons_vec);

        // 3) 생성
        let params = Arc::new(Parameters::from_tracks(&tk));
        let stop = Arc::new(AtomicBool::new(false));
        let wait = Arc::new((Mutex::new(false), Condvar::new()));
        let rt: Arc<Vec<Mutex<TrackTimeline>>> = Arc::new(
            (0..tk.len())
                .map(|_| {
                    Mutex::new(TrackTimeline {
                        clips: BTreeMap::new(),
                        write_pos_frames: 0,
                    })
                })
                .collect(),
        );
        let decs: Arc<Vec<Mutex<Option<DecoderState>>>> =
            Arc::new((0..tk.len()).map(|_| Mutex::new(None)).collect());
        let playing = Arc::new(Transport::new(48_000));

        let seek_epoch = Arc::new(AtomicU64::new(0));

        let seek_lock = Arc::new(Mutex::new(()));

        // 4) 디코딩 스레드 포인터 클론
        let decoding_workers: usize = 4;
        let mut worker = Vec::with_capacity(decoding_workers + 1);
        for worker_id in 0..decoding_workers {
            let rt_c = Arc::clone(&rt);
            let prod_c = Arc::clone(&producers);
            let stop_c = Arc::clone(&stop);
            let wait_c = Arc::clone(&wait);
            let dec_c = Arc::clone(&decs);
            let playing_c = Arc::clone(&playing);
            let params_c = Arc::clone(&params);
            worker.push(thread::spawn(move || {
                bump_priority_worker_thread();
                pin_to_core(
                    1 + (worker_id
                        % (core_affinity::get_core_ids().map(|v| v.len()).unwrap_or(1)).max(1)),
                );
                let mut rr = 0usize;

                loop {
                    if stop_c.load(Ordering::Acquire) {
                        break;
                    }

                    let ntracks = rt_c.len(); //트랙 수
                    if ntracks == 0 {
                        std::thread::yield_now(); //트랙이 없으면 양보
                        continue; //다음 루프
                    }

                    // 라운드로빈 시작점만 한 칸씩 밀기
                    let start = rr;
                    rr = rr.wrapping_add(1); //다음 시작점 계산 (오버플로우 허용)

                    // ★ 이번 사이클에 모든 트랙을 한 번씩 훑는다
                    for off in 0..ntracks {
                        if stop_c.load(Ordering::Acquire) {
                            break;
                        }
                        let track_idx = (start + off) % ntracks;

                        // 1차 Prod 꽉 차면 스킵 (짧은 try)
                        if let Ok(p) = prod_c[track_idx].lock() {
                            if p.is_full() {
                                continue;
                            }

                        }

                        // 전역 일시정지 게이트 (seek 등)
                        {
                            let (lock, cvar) = &*wait_c; //전체 대기
                            let mut guard = lock.lock().unwrap(); //뮤텍스 잠금
                            guard = cvar.wait_while(guard, |waiting| *waiting).unwrap(); //대기
                            if stop_c.load(Ordering::Relaxed) { //종료 신호
                                return;
                            }
                        }

                        // 로컬 포화면 짧게 대기
                        {
                            let (mx, cv) = &*wait_c; //전체 대기
                            let mut g = mx.lock().unwrap(); //뮤텍스 잠금
                            while { // 1차 Prod 꽉 찼으면 대기
                                //while condition
                                if let Ok(p) = prod_c[track_idx].lock()  //잠깐만 잡고
                                {
                                    p.is_full() //꽉 찼으면
                                } else {
                                    true    //잠금 실패하면 안전하게 대기
                                }
                            } && !stop_c.load(Ordering::Acquire) //종료 신호 아니면
                            { //while inner
                                g = cv  //조건변수로 짧게 대기
                                    .wait_timeout(g, std::time::Duration::from_millis(1))  // 1ms 대기
                                    .unwrap() //실패시 패닉
                                    .0; //반환값에서 guard만 취함
                            }
                        }

                        if stop_c.load(Ordering::Acquire) {
                            break;
                        }

                        let engine_sr = playing_c.sr();

                        let mut per_iter = CHUNK_DECODE;
                        let mut produced_total = 0usize;
                        loop {
                            if stop_c.load(Ordering::Acquire) {
                                break;
                            }

                            // RB1 꽉 찼으면 다음 트랙
                            let full = if let Ok(p) = prod_c[track_idx].lock() {
                                p.is_full()
                            } else {
                                true
                            };
                            if full {
                                break;
                            }

                            // tr/dec/prod 잠깐만 잡고 최대 per_iter 만큼 생산
                            let n = {
                                let mut tr = match rt_c[track_idx].lock() {
                                    Ok(g) => g,
                                    Err(_) => continue,
                                };
                                let mut dc = match dec_c[track_idx].lock() {
                                    Ok(g) => g,
                                    Err(_) => continue,
                                };
                                let mut pd = match prod_c[track_idx].lock() {
                                    Ok(g) => g,
                                    Err(_) => continue,
                                };
                                let tempo_ratio = {
                                    let bpm_bits = params_c.bpm.load(Ordering::Relaxed);
                                    let bpm = f32::from_bits(bpm_bits);
                                    (bpm / BASE_BPM).clamp(0.25, 4.0)
                                };
                                let tpos = playing_c.pos_frames();
                                match fill_track_once(
                                    &mut *tr,
                                    &mut *dc,
                                    &mut *pd,
                                    per_iter,
                                    engine_sr,
                                    tempo_ratio,
                                    tpos,
                                ) {
                                    Ok(n) => n,
                                    Err(e) => {
                                        eprintln!(
                                            "[worker {worker_id}] fill_track_once error: {e}"
                                        );
                                        0
                                    }
                                }
                            };
                            if n == 0 {
                                // 더 만들 게 없으면 양보하고 탈출
                                std::thread::yield_now();
                                break;
                            }

                            produced_total += n;
                        }

                        // 트랙 단위로 아무 것도 못했으면 아주 짧게 쉼
                        if produced_total == 0 {
                            std::thread::park_timeout(std::time::Duration::from_micros(200));
                        }
                    }
                }
            }));
        }
        // 7) Self
        return Self {
            track: tk,
            producers,
            consumers,

            thread_worker: worker,
            thread_stop: stop,
            thread_wait: wait,

            real_time_params: params,
            track_run_time: rt,
            decod: decs,

            play_time_manager: playing,
            seek_epoch,
            seek_lock: seek_lock,
            pending_bpm: AtomicU32::new(f32::to_bits(60.0)),
            has_pending_bpm: AtomicBool::new(false),
            pad_sample: AtomicCell::new(None),
            sfx_state: Mutex::new(None),
        };
    }

    fn wake_workers(&self) {
        //워커 깨우기
        let (lock, cvar) = &*self.thread_wait;
        *lock.lock().unwrap() = false;
        cvar.notify_all();
    }

    fn pause_workers(&self) {
        //워커 대기
        let (lock, cvar) = &*self.thread_wait;
        *lock.lock().unwrap() = true;
        cvar.notify_all();
    }

    fn align_write_pos_to_transport(&self) {
        let pos = self.play_time_manager.pos_frames();
        for tr_mx in self.track_run_time.iter() {
            if let Ok(mut tr) = tr_mx.lock() {
                tr.write_pos_frames = pos;
            }
        }
    }

    fn flush_ringbuffers(&mut self) {
        for cons_mx in self.consumers.iter() {
            if let Ok(mut cons) = cons_mx.lock() {
                while cons.pop().is_ok() {}
            }
        }
    }

    fn prefill_rb1_blocking(&self, frames: usize) -> Result<(), String> {
        let sr = self.play_time_manager.sr();
        let tpos = self.play_time_manager.pos_frames();
        let n = self.track.len();
        for i in 0..n {
            let mut tr = match self.track_run_time[i].lock() {
                Ok(g) => g,
                Err(_) => continue,
            };
            let mut dec = match self.decod[i].lock() {
                Ok(g) => g,
                Err(_) => continue,
            };
            let mut prod = match self.producers[i].lock() {
                Ok(g) => g,
                Err(_) => continue,
            };
            let tempo_ratio = {
                let bpm_bits = self.real_time_params.bpm.load(Ordering::Relaxed);
                let bpm = f32::from_bits(bpm_bits);
                (bpm / BASE_BPM).clamp(0.25, 4.0)
            };
            let produced =
                fill_track_once(&mut tr, &mut dec, &mut prod, frames, sr, tempo_ratio, tpos)?;
        }
        Ok(())
    }

    fn rebuffer_current(&mut self) {
        let lock = std::sync::Arc::clone(&self.seek_lock);
        let _guard = lock.lock().unwrap();

        self.pause_workers();

        self.align_write_pos_to_transport();
        self.seek_epoch.fetch_add(1, Ordering::Release);

        if self.has_pending_bpm.swap(false, Ordering::AcqRel) {
            let b_bits = self.pending_bpm.load(Ordering::Acquire);
            self.real_time_params.bpm.store(b_bits, Ordering::Relaxed);
        }

        // ★★★ 여기서 디코더 리셋/시크
        let sr = self.play_time_manager.sr();
        let tpos = self.play_time_manager.pos_frames();
        let tempo_ratio = {
            let bpm_bits = self.real_time_params.bpm.load(Ordering::Relaxed);
            let bpm = f32::from_bits(bpm_bits);
            (bpm / BASE_BPM).clamp(0.25, 4.0)
        };
        for i in 0..self.track.len() {
            if let (Ok(tr), Ok(mut dec)) = (self.track_run_time[i].lock(), self.decod[i].lock()) {
                let _ = self.reset_decoder_to_tpos(&*tr, &mut *dec, tpos, sr, tempo_ratio);
            }
        }
        self.flush_ringbuffers();

        let _ = self.prefill_rb1_blocking(PREFILL_ON_SEEK * 2);
        self.wake_workers();
    }

    #[inline]
    fn with_seek_lock<R>(&mut self, f: impl FnOnce(&mut Self) -> R) -> R {
        // 1) 필드를 '읽는' 대여를 한 줄에서 끝내야 함
        let lock = std::sync::Arc::clone(&self.seek_lock);

        // 2) guard는 '복사된 Arc'에서 뽑음 (self에 대한 불변 대여 없음)
        let _guard = lock.lock().unwrap();

        // 3) guard가 살아있는 동안에도 &mut self 사용 가능
        f(self)
        // 4) 스코프 끝나며 _guard drop
    }

    fn reset_decoder_to_tpos(
        &self,
        tr: &TrackTimeline,
        dec: &mut Option<DecoderState>,
        tpos_frames: u64,
        out_sr: u32,
        tempo_ratio: f32,
    ) -> Result<(), String> {
        let Some(d) = dec.as_mut() else {
            return Ok(());
        };

        // tpos가 포함된 클립 찾기
        let active = tr
            .clips
            .range(..=tpos_frames)
            .next_back()
            .and_then(|(_, c)| {
                let end = c.tl_start.saturating_add(c.tl_len);
                if tpos_frames < end {
                    Some(c)
                } else {
                    None
                }
            });

        // 클립이 없으면 그냥 0으로 맞추고 종료(무음 구간은 fill에서 처리)
        let Some(clip) = active else {
            d.decoder.reset();
            d.sample_buf.clear();
            d.src_pos_samples = 0;
            return Ok(());
        };

        // 타임라인→소스 좌표 변환(템포 반영)
        let rel = (tpos_frames.saturating_sub(clip.tl_start)) as f64;
        let step = (d.src_sr as f64 / out_sr as f64) * (tempo_ratio as f64);
        let approx_src_samples = (rel * step).floor() as u64;

        // 정확 시크
        let time = Time::from(approx_src_samples as f64 / d.src_sr as f64);
        d.format
            .seek(
                SeekMode::Accurate,
                SeekTo::Time {
                    time,
                    track_id: None,
                },
            )
            .map_err(|e| format!("format.seek failed: {e}"))?;

        d.decoder.reset();
        d.sample_buf.clear();
        d.src_pos_samples = approx_src_samples;
        Ok(())
    }

    fn decode_head_1s_to_48k2ch_interleaved_arc(&self, path: &str) -> Option<Arc<[f32]>> {
        const OUT_SR: u32 = 48_000;
        const OUT_FRAMES: usize = 48_000;
        const OUT_SAMPLES: usize = OUT_FRAMES * 2;

        use std::fs::File;
        use std::path::Path;
        use symphonia::core::{
            audio::{SampleBuffer, SignalSpec},
            codecs::DecoderOptions,
            formats::FormatOptions,
            io::MediaSourceStream,
            meta::MetadataOptions,
            probe::Hint,
        };
        use symphonia::default::{get_codecs, get_probe};

        // 파일 열기 + 포맷/디코더 준비
        let file = File::open(Path::new(path)).ok()?;
        let mss = MediaSourceStream::new(Box::new(file), Default::default());
        let probed = get_probe()
            .format(
                &Hint::new(),
                mss,
                &FormatOptions::default(),
                &MetadataOptions::default(),
            )
            .ok()?;
        let mut format = probed.format;

        let track = format
            .tracks()
            .iter()
            .find(|t| t.codec_params.channels.is_some())?;
        let src_sr = track.codec_params.sample_rate?;
        let src_ch = track.codec_params.channels?.count().max(1);
        let mut decoder = get_codecs()
            .make(&track.codec_params, &DecoderOptions::default())
            .ok()?;

        // 디코드 버퍼
        let mut sample_buf = SampleBuffer::<f32>::new(
            0,
            SignalSpec {
                rate: src_sr,
                channels: track.codec_params.channels?,
            },
        );

        // 출력 버퍼 준비 (무음으로 채워 시작)
        let mut out: Vec<f32> = vec![0.0; OUT_SAMPLES];

        // 리샘플링 스텝(선형 보간 없이 최근접 샘플 픽업: 빠르고 클릭 없음)
        let step = src_sr as f64 / OUT_SR as f64;

        let mut src_samples: Vec<[f32; 2]> = Vec::new();
        src_samples.reserve((OUT_FRAMES as f64 * step + 4.0) as usize);

        // 소스에서 최소 1초 분량 만큼 뽑기
        'outer: loop {
            let pkt = match format.next_packet() {
                Ok(p) => p,
                Err(_) => break, // EOF
            };
            let decoded = match decoder.decode(&pkt) {
                Ok(x) => x,
                Err(_) => break, // 디코드 에러 -> 포기
            };
            if sample_buf.capacity() < decoded.capacity() {
                sample_buf = SampleBuffer::<f32>::new(decoded.capacity() as u64, *decoded.spec());
            }
            sample_buf.copy_interleaved_ref(decoded);
            let s = sample_buf.samples();

            // 채널 매핑: mono면 L=R, 그 외엔 앞 2채널만 사용
            let ch = src_ch.min(2);
            let mut i = 0;
            while i + ch <= s.len() {
                let l = s[i];
                let r = if ch >= 2 { s[i + 1] } else { l };
                src_samples.push([l, r]);
                i += ch;
                if src_samples.len() >= ((OUT_FRAMES as f64 * step).ceil() as usize + 2) {
                    break 'outer;
                }
            }
        }

        if src_samples.is_empty() {
            return None;
        }

        // 최근접 샘플 픽업으로 48k/2ch 1초 채우기
        for n in 0..OUT_FRAMES {
            let pos = (n as f64 * step).floor() as usize;
            let p = src_samples.get(pos).or_else(|| src_samples.last());
            if let Some([l, r]) = p {
                out[n * 2] = *l;
                out[n * 2 + 1] = *r;
            } else {
                break;
            }
        }

        Some(Arc::from(out))
    }
    
    pub fn project_end_frames(&self) -> u64 {
        let mut end : u64 = 0;
        for tr_mx in self.track_run_time.iter() {
            if let Ok(tr) = tr_mx.lock() {
                for c in tr.clips.values() {
                    let e = c.tl_start.saturating_add(c.tl_len);
                    if e > end {
                        end = e;
                    }
                }
            }
        }
        end
    }
    pub fn project_length_seconds(&self) -> f64 {
        let sr = self.play_time_manager.sr().max(1);
        self.project_end_frames() as f64 / sr as f64
    }
}

impl Drop for Engine {
    fn drop(&mut self) {

        self.thread_stop.store(true, Ordering::Relaxed);
        self.wake_workers();

        for h in self.thread_worker.drain(..) {
            let _ = h.join();
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_audio_track_new() -> *mut TrackConfig {
    let track = match TrackConfig::new() {
        Ok(data) => data,
        Err(_) => return std::ptr::null_mut(),
    };
    Box::into_raw(Box::new(track))
}

#[no_mangle]
pub extern "C" fn rust_audio_track_free(tk: *mut TrackConfig) {
    if tk.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(tk));
    }
}

#[no_mangle]
pub extern "C" fn rust_audio_engine_new(
    track0: *mut TrackConfig,
    track1: *mut TrackConfig,
    track2: *mut TrackConfig,
    track3: *mut TrackConfig,
) -> *mut Engine {
    if track0.is_null() || track1.is_null() || track2.is_null() || track3.is_null() {
        return std::ptr::null_mut();
    }
    let t0 = unsafe { *Box::from_raw(track0) };
    let t1 = unsafe { *Box::from_raw(track1) };
    let t2 = unsafe { *Box::from_raw(track2) };
    let t3 = unsafe { *Box::from_raw(track3) };
    let track: Vec<TrackConfig> = vec![t0, t1, t2, t3];
    let eng = Engine::new(track);
    Box::into_raw(Box::new(eng))
}

#[no_mangle]
pub extern "C" fn rust_audio_engine_free(eng: *mut Engine) {
    if eng.is_null() {
        return;
    }
    unsafe {
        drop(Box::from_raw(eng));
    }
}

#[no_mangle]
pub extern "C" fn rust_request_load_single_sample(
    engine: *mut Engine,
    path: *const c_char,
) -> bool {
    if engine.is_null() || path.is_null() {
        return false;
    }
    let eng = unsafe { &mut *engine };

    // C 문자열 → Rust &str
    let cstr = unsafe { CStr::from_ptr(path) };
    let path_str = match cstr.to_str() {
        Ok(s) => s,
        Err(_) => return false,
    };

    if let Some(buf) = eng.decode_head_1s_to_48k2ch_interleaved_arc(path_str) {
        // Sample에는 sr이 아니라 nframes가 있음
        let s = Arc::new(Sample {
            data: buf,
            nframes: 48_000,
        });
        eng.pad_sample.store(Some(s));
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn rust_pad_note_on(engine: *mut Engine) -> bool {
    if engine.is_null() { return false; }
    let eng = unsafe { &mut *engine };

    // pad_sample 스냅샷(Arc 복사만)
    let Some(sample_arc) = eng.pad_sample.take() else { return false; };
    let sample = sample_arc.clone();
    eng.pad_sample.store(Some(sample_arc)); // 재사용 가능하게 되돌리기

    if let Ok(mut st) = eng.sfx_state.lock() {
        // 리트리거: 처음부터 다시
        *st = Some(SfxState { sample, frame: 0 });
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn rust_pad_note_off(engine: *mut Engine) -> bool {
    if engine.is_null() { return false; }
    let eng = unsafe { &mut *engine };
    if let Ok(mut st) = eng.sfx_state.lock() {
        *st = None;
        true
    } else { false }
}
