pub mod unit;
pub use unit::*;
mod waveform_generation_module;
pub use waveform_generation_module::*;
mod sound_track_update;
pub use sound_track_update::*;
mod sound_play;
pub use sound_play::*;
mod logging;
pub use logging::rlog_send;
pub use logging::*;

use std::ffi::CStr;
use std::os::raw::c_char;

use crossbeam_utils::atomic::AtomicCell;
use std::collections::BTreeMap;
use std::sync::Condvar;
use std::time::{Duration, Instant};

pub static CTR_CB: AtomicUsize = AtomicUsize::new(0);
pub static CTR_COPY: AtomicUsize = AtomicUsize::new(0);

fn bump_priority_copy_thread() {
    unsafe {
        let h = GetCurrentThread();
        let _ = SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL);
    }
}

fn bump_priority_worker_thread() {
    unsafe {
        let h = GetCurrentThread();
        let _ = SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL);
    }
}

fn pin_to_core(core_id: usize) {
    if let Some(core) = core_affinity::get_core_ids().and_then(|v| v.get(core_id).cloned()) {
        let _ = core_affinity::set_for_current(core);
    }
}

enum TrackNumber {
    Zero,
    One,
    Two,
    Three,
}
pub struct CircularBuffer {
    producer: Option<Producer<f32>>, //디코더/프로듀서가 push할 핸들
    consumer: Option<Consumer<f32>>, //소비(믹서/출력)가 pop할 핸들
}

pub struct TrackConfig {
    track_number: TrackNumber,
    volume: f32,
    muted: bool,
    pan: f32,
    reverb: bool,
    delay: bool,
    circularbuffer: CircularBuffer,
}
impl TrackConfig {
    fn new(number: i32) -> Result<Self, String> {
        let track_num = match number {
            0 => TrackNumber::Zero,
            1 => TrackNumber::One,
            2 => TrackNumber::Two,
            3 => TrackNumber::Three,
            _ => return Err("not a valid track number".to_string()),
        };
        let (tx, rx) = RingBuffer::<f32>::new(slots(RB1_FRAMES));
        //f32 타입에 배열을 생성 [CAPACITY_SAMPLES] 만큼
        let circularbuffer = CircularBuffer {
            producer: Some(tx),
            consumer: Some(rx),
        };
        Ok(Self {
            track_number: track_num,
            volume: 0.5,
            muted: false,
            pan: 0.0,
            reverb: false,
            delay: false,
            circularbuffer: circularbuffer,
        })
    }
}

pub struct Parameters {
    volume: Vec<AtomicU32>,
    pan: Vec<AtomicU32>,
    muted: Vec<AtomicBool>,
    bpm: AtomicU32,
    pitch_semitones: AtomicU32,
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
        let pitch_semitones = AtomicU32::new((0.0f32).to_bits());
        Self {
            volume,
            pan,
            muted,
            bpm,
            pitch_semitones,
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

struct Budget {
    frames: AtomicUsize,
}
impl Budget {
    pub fn new() -> Self {
        Self {
            frames: AtomicUsize::new(0),
        }
    }
    #[inline]
    pub fn add(&self, n: usize) {
        if n == 0 {
            return;
        }
        // 포화 덧셈
        let mut cur = self.frames.load(Ordering::Acquire);
        loop {
            let next = (cur.saturating_add(n)).min(MAX_BUDGET);
            match self
                .frames
                .compare_exchange(cur, next, Ordering::AcqRel, Ordering::Acquire)
            {
                Ok(_) => break,
                Err(v) => cur = v,
            }
        }
    }
    #[inline]
    pub fn sub(&self, n: usize) {
        if n == 0 {
            return;
        }
        // compare_exchange 루프로 "0 이하로는 안내려가게" 포화 감소
        let mut current = self.frames.load(Ordering::Acquire);
        loop {
            let next = current.saturating_sub(n);
            match self
                .frames
                .compare_exchange(current, next, Ordering::AcqRel, Ordering::Acquire)
            {
                Ok(_) => break,
                Err(v) => current = v,
            }
        }
    }
    #[inline]
    pub fn frames(&self) -> usize {
        self.frames.load(Ordering::Acquire)
    }
    #[inline]
    pub fn reset(&self) {
        self.frames.store(0, Ordering::Release);
    }
    #[inline]
    pub fn decay(&self, n: usize) {
        if n == 0 {
            return;
        }
        self.sub(n);
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
    playout_producers: Vec<Option<Producer<[f32; 2]>>>,
    playout_consumers: Vec<Option<Consumer<[f32; 2]>>>,
    playout_producers_bank: [Vec<Option<Producer<[f32; 2]>>>; 2],
    playout_consumers_bank: [Vec<Option<Consumer<[f32; 2]>>>; 2],
    active_idx: Arc<AtomicUsize>,
    rb2_level_sb: Arc<Vec<AtomicUsize>>,

    thread_worker: Vec<JoinHandle<()>>,
    copythread_worker: Option<JoinHandle<()>>, // ★ 복제 스레드 핸들 저장

    thread_stop: Arc<AtomicBool>,             //스레드 종료
    copythread_stop: Arc<AtomicBool>,         //스레드 종료
    thread_wait: Arc<(Mutex<bool>, Condvar)>, //전체 대기
    flush_flag: Arc<AtomicBool>,

    real_time_params: Arc<Parameters>,
    track_run_time: Arc<Vec<Mutex<TrackTimeline>>>,
    decod: Arc<Vec<Mutex<Option<DecoderState>>>>,

    play_time_manager: Arc<Transport>,
    seek_epoch: Arc<AtomicU64>,
    sound_output: Option<cpal::Stream>,
    track: Vec<TrackConfig>,
    budget: Arc<Budget>,
    seek_lock: Arc<Mutex<()>>,
    rb2_level: Arc<Vec<AtomicUsize>>,
    underrun_streak_callbacks: usize,
    last_auto_rebuffer_at: Instant,
    pending_bpm: AtomicU32,
    has_pending_bpm: AtomicBool,
    last_bank_swap_at: Instant,
    swap_qualify_streak: usize,
    precharge_req: Arc<AtomicBool>,
    last_swap_epoch: Arc<AtomicU64>, // 스왑 카운터
    swap_epoch_at_call: u64,         // (렌더 스냅샷)
    pad_sample: AtomicCell<Option<Arc<Sample>>>,
    sfx_state: Mutex<Option<SfxState>>, 
}
impl Engine {
    fn new(mut tk: Vec<TrackConfig>) -> Self {
        logging::init_file_log("C:\\temp\\rust_audio.log");
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

        // 2) 2차(pl*): 복제 스레드 → cpal 로 가는 링버퍼
        let mut playout_producers = Vec::with_capacity(tk.len());
        let mut playout_consumers = Vec::with_capacity(tk.len());
        for _ in 0..tk.len() {
            let (tx, rx) = RingBuffer::<[f32; 2]>::new(RB2_FRAMES);
            playout_producers.push(Some(tx));
            playout_consumers.push(Some(rx));
        }
        let ntracks = tk.len();
        let mut bank_prod0 = Vec::with_capacity(ntracks);
        let mut bank_cons0 = Vec::with_capacity(ntracks);
        let mut bank_prod1 = Vec::with_capacity(ntracks);
        let mut bank_cons1 = Vec::with_capacity(ntracks);
        for _ in 0..ntracks {
            let (tx0, rx0) = RingBuffer::<[f32; 2]>::new(RB2_FRAMES);
            let (tx1, rx1) = RingBuffer::<[f32; 2]>::new(RB2_FRAMES);
            bank_prod0.push(Some(tx0));
            bank_cons0.push(Some(rx0));
            bank_prod1.push(Some(tx1));
            bank_cons1.push(Some(rx1));
        }
        let playout_producers_bank = [bank_prod0, bank_prod1];
        let playout_consumers_bank = [bank_cons0, bank_cons1];

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
        let repl_stop = Arc::new(AtomicBool::new(false));
        let flush_flag = Arc::new(AtomicBool::new(false));
        let seek_epoch = Arc::new(AtomicU64::new(0));
        let budget = Arc::new(Budget::new());
        let rb2_level = Arc::new(
            (0..tk.len())
                .map(|_| AtomicUsize::new(0))
                .collect::<Vec<_>>(),
        );
        let rb2_level_sb = Arc::new((0..ntracks).map(|_| AtomicUsize::new(0)).collect());
        let precharge_req = Arc::new(AtomicBool::new(false));
        let seek_lock = Arc::new(Mutex::new(()));
        let (sfx_tx, sfx_rx) = RingBuffer::<[f32; 2]>::new(48_000 * 2);

        // 4) 디코딩 스레드 포인터 클론
        let decoding_workers: usize = 6;
        let mut worker = Vec::with_capacity(decoding_workers + 1);
        for worker_id in 0..decoding_workers {
            let rt_c = Arc::clone(&rt);
            let prod_c = Arc::clone(&producers);
            let stop_c = Arc::clone(&stop);
            let wait_c = Arc::clone(&wait);
            let dec_c = Arc::clone(&decs);
            let playing_c = Arc::clone(&playing);
            let budget_c = Arc::clone(&budget);
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

                    let ntracks = rt_c.len();
                    if ntracks == 0 {
                        std::thread::yield_now();
                        continue;
                    }

                    // 라운드로빈 시작점만 한 칸씩 밀기
                    let start = rr;
                    rr = rr.wrapping_add(1);

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
                            let (lock, cvar) = &*wait_c;
                            let mut guard = lock.lock().unwrap();
                            guard = cvar.wait_while(guard, |waiting| *waiting).unwrap();
                            if stop_c.load(Ordering::Relaxed) {
                                return;
                            }
                        }

                        // 로컬 포화면 짧게 대기
                        {
                            let (mx, cv) = &*wait_c;
                            let mut g = mx.lock().unwrap();
                            while {
                                if let Ok(p) = prod_c[track_idx].lock() {
                                    p.is_full()
                                } else {
                                    true
                                }
                            } && !stop_c.load(Ordering::Acquire)
                            {
                                g = cv
                                    .wait_timeout(g, std::time::Duration::from_millis(1))
                                    .unwrap()
                                    .0;
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
                            budget_c.add(n); // 예산은 기록만(스로틀에 사용 안 함)
                        }

                        // 트랙 단위로 아무 것도 못했으면 아주 짧게 쉼
                        if produced_total == 0 {
                            budget_c.decay(CHUNK_COPY / 2);
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
            playout_producers,
            playout_consumers,
            playout_producers_bank,
            playout_consumers_bank,
            active_idx: Arc::new(AtomicUsize::new(0)),
            rb2_level_sb,

            thread_worker: worker,
            copythread_worker: None,
            thread_stop: stop,
            copythread_stop: repl_stop,
            thread_wait: wait,
            flush_flag: flush_flag,

            real_time_params: params,
            track_run_time: rt,
            decod: decs,

            play_time_manager: playing,
            seek_epoch,
            sound_output: None,
            budget: budget,
            seek_lock: seek_lock,
            rb2_level: rb2_level.clone(),
            underrun_streak_callbacks: 0,
            last_auto_rebuffer_at: Instant::now(),
            pending_bpm: AtomicU32::new(f32::to_bits(60.0)),
            has_pending_bpm: AtomicBool::new(false),
            last_bank_swap_at: Instant::now(),
            swap_qualify_streak: 0,
            precharge_req: precharge_req,
            last_swap_epoch: Arc::new(AtomicU64::new(0)),
            swap_epoch_at_call: 0,
            pad_sample: AtomicCell::new(None),
            sfx_state: Mutex::new(None),
        };
    }

    #[allow(dead_code)]
    fn spawn_copy_thread(&mut self) {
        if self.copythread_worker.is_some() {
            return;
        }

        // 두 뱅크 모두 move. 스레드 안에서 active_idx 보고 standby만 채움.
        let mut outs0: Vec<Producer<[f32; 2]>> =
            Vec::with_capacity(self.playout_producers_bank[0].len());
        let mut outs1: Vec<Producer<[f32; 2]>> =
            Vec::with_capacity(self.playout_producers_bank[1].len());
        for p in &mut self.playout_producers_bank[0] {
            if let Some(tx) = p.take() {
                outs0.push(tx);
            }
        }
        for p in &mut self.playout_producers_bank[1] {
            if let Some(tx) = p.take() {
                outs1.push(tx);
            }
        }

        self.copythread_stop.store(false, Ordering::Relaxed);

        // 공유 상태 캡처
        let cons_c = Arc::clone(&self.consumers); // RB1: f32 (L,R,L,R,...)
        let wait_c = Arc::clone(&self.thread_wait);
        let repl_stop_c = Arc::clone(&self.copythread_stop);
        let seek_epoch_c = Arc::clone(&self.seek_epoch);
        let budget_c = Arc::clone(&self.budget);
        let rb2_level_c = Arc::clone(&self.rb2_level);
        let rb2_level_sb_c = Arc::clone(&self.rb2_level_sb);
        let active_idx_c = Arc::clone(&self.active_idx);
        let precharge_req_c = Arc::clone(&self.precharge_req);
        let handle = std::thread::spawn(move || {
            bump_priority_copy_thread();
            pin_to_core(0);
            let mut _last_epoch = seek_epoch_c.load(Ordering::Acquire);

            loop {
                if repl_stop_c.load(Ordering::Relaxed) {
                    break;
                }

                // 전역 일시정지(시킹/플러시) 게이트
                {
                    let (lock, cvar) = &*wait_c;
                    let mut waiting = lock.lock().unwrap();
                    while *waiting {
                        waiting = cvar.wait(waiting).unwrap();
                        if repl_stop_c.load(Ordering::Relaxed) {
                            return;
                        }
                    }
                }

                // seek epoch 변동 감지(필요시 로컬 상태 초기화 지점)
                {
                    let cur = seek_epoch_c.load(Ordering::Acquire);
                    if cur != _last_epoch {
                        _last_epoch = cur;
                        // 로컬 상태 없으니 지금은 noop
                    }
                }

                // 지금 시점의 standby 선택
                let a = active_idx_c.load(Ordering::Acquire);
                let standby = a ^ 1;
                let outs = if standby == 0 { &mut outs0 } else { &mut outs1 };
                let standby_levels = if standby == 0 {
                    &rb2_level_c
                } else {
                    &rb2_level_sb_c
                };
                let ntracks = outs.len();
                if ntracks == 0 {
                    std::thread::yield_now();
                    continue;
                }
                // ★ "더 비어있는 트랙부터"도 standby 레벨 기준으로 정렬
                let mut order: Vec<usize> = (0..ntracks).collect();
                order.sort_by_key(|&i| standby_levels[i].load(Ordering::Relaxed));

                let mut did_anything = false;
                for ti in order {
                    let pp = &mut outs[ti];
                    if pp.is_full() {
                        continue;
                    }

                    // 현재 RB2 레벨
                    let mut level = standby_levels[ti].load(Ordering::Relaxed);

                    // ★ 충전 모드 여부 확인
                    let precharge = precharge_req_c.load(Ordering::Acquire);

                    // ★ 목표치: 충전 모드면 거의 만땅까지, 아니면 기존 HIGH_FRAMES까지만
                    let goal = if precharge {
                        SWAP_TARGET_STANDBY
                    } else {
                        HIGH_FRAMES
                    };

                    // 핵심: RB2가 HIGH_FRAMES에 닿을 때까지 퍼붓는 루프
                    while level < goal {
                        if pp.is_full() {
                            break;
                        }

                        // 이번 사이클에서 채울 목표량
                        let want = goal - level;
                        let urgent = level <= (LOW_FRAMES / 2);

                        // ★ 충전 모드일 때는 더 큰 벌크로 끌어온다
                        let burst = if precharge {
                            CHUNK_COPY * 3
                        } else if urgent {
                            CHUNK_COPY * 2
                        } else {
                            CHUNK_COPY
                        };
                        let mut todo = core::cmp::min(want, burst);

                        // RB1 -> RB2 벌크 이동
                        let mut moved = 0usize;
                        if let Ok(mut rc) = cons_c[ti].lock() {
                            while moved < todo && !pp.is_full() {
                                match (rc.pop(), rc.pop()) {
                                    (Ok(l), Ok(r)) => {
                                        if pp.push([l, r]).is_err() {
                                            break;
                                        }
                                        moved += 1;
                                    }
                                    _ => {
                                        // RB1 말랐음. 바깥에서 채우고 다시 시도.
                                        break;
                                    }
                                }
                            }
                        }

                        if moved == 0 {
                            // RB1이 말랐음 → 워커 깨우고 이 트랙은 패스
                            let (_, cv) = &*wait_c;
                            cv.notify_all();
                            break;
                        } else {
                            // 이동 성공
                            level += moved;
                            standby_levels[ti].fetch_add(moved, Ordering::Relaxed);
                            did_anything = true;

                            // 생산 예산 감소(“엔진→디바이스로 옮긴 만큼”)
                            budget_c.sub(moved);

                            // 아직 HIGH에 못 닿았으면 루프 계속 돌아서 더 채운다
                        }
                    }
                }

                if !did_anything {
                    let (lock, cv) = &*wait_c;
                    // waiting 플래그는 재생 중 항상 false이므로, 단순 타임아웃 wait만 쓴다
                    let guard = lock.lock().unwrap();
                    let _ = cv.wait_timeout(guard, std::time::Duration::from_micros(200));
                }
            }
        });

        self.copythread_worker = Some(handle);
    }
    #[allow(dead_code)]
    fn stop_copy_thread(&mut self) {
        if self.copythread_worker.is_none() {
            return;
        }
        self.copythread_stop.store(true, Ordering::Relaxed);
        self.wake_workers(); // condvar 깨워서 루프 탈출
        if let Some(h) = self.copythread_worker.take() {
            let _ = h.join(); // 여기서 outs(2차 Producer) drop
        }
        self.copythread_stop.store(false, Ordering::Relaxed);

        // 스레드가 2차 P를 drop했으니, 다시 쓸 수 있도록 재생성
        self.rebuild_rb2_bank_ringbuffers();
    }
    #[allow(dead_code)]
    fn rebuild_rb2_ringbuffers(&mut self) {
        self.playout_producers.clear();
        self.playout_consumers.clear();
        for _ in 0..self.track.len() {
            let (tx, rx) = RingBuffer::<[f32; 2]>::new(RB2_FRAMES);
            self.playout_producers.push(Some(tx));
            self.playout_consumers.push(Some(rx));
        }
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

    fn rebuild_all_ringbuffers(&mut self) {
        for i in 0..self.track.len() {
            let (tx, rx) = RingBuffer::<f32>::new(slots(RB1_FRAMES));

            if let Ok(mut p) = self.producers[i].lock() {
                *p = tx;
            }
            if let Ok(mut c) = self.consumers[i].lock() {
                *c = rx;
            }

            // (선택) 트랙 안의 사본도 끊어버리거나 맞춰준다.
            if let Some(tr) = self.track.get_mut(i) {
                tr.circularbuffer.producer = None; // ← 트랙 사본 비사용화 (추천)
                tr.circularbuffer.consumer = None; //  or 필요하면 여기도 새 tx/rx로 교체
            }
        }

        // // playout_*도 쓰면 동일하게 재생성
        // for i in 0..self.playout_producers.len() {
        //     let (tx, rx) = RingBuffer::<f32>::new(CAPACITY_SAMPLES);
        //     self.playout_producers[i] = Some(tx);
        //     self.playout_consumers[i] = Some(rx);
        // }
    }
    #[allow(dead_code)]
    fn start_output_from_ringbuffer(&mut self) -> anyhow::Result<cpal::Stream> {
        let host = cpal::default_host(); //기본 오디오 호스트 (Windows: WASAPI, Linux: ALSA/PulseAudio 등)
        let device = host
            .default_output_device()
            .ok_or_else(|| anyhow::anyhow!("no output device"))?; //기본 장비 조회

        // 장치 기본(Windows 믹스) 포맷 사용: WASAPI에서 가장 안정적
        let supported = device.default_output_config()?;

        // 지금 콜백이 f32 전용이므로, 기본 포맷이 f32가 아니면 bail
        if supported.sample_format() != SampleFormat::F32 {
            anyhow::bail!("default output format is not f32; adjust callback or add match");
        }
        let mut config = supported.config(); //기본 포맷 설정
        config.buffer_size = cpal::BufferSize::Fixed(2048); // ★ 추가 (4096도 시험)
        let sr = config.sample_rate.0; //샘플링 레이트
        self.play_time_manager.set_sr(sr); //트랜스포트에 샘플링 레이트 설정

        let channels = config.channels as usize; //채널 수

        if channels != CHANNELS {
            anyhow::bail!("not stereo output");
        } //2채널이 아니면 종료

        let err_fn = |e| eprintln!("[cpal] stream error: {e}"); //에러 콜백

        //콜백에 넘길 핸들/파라미터 스냅샷
        let mut play_cons: Vec<Consumer<[f32; 2]>> =
            Vec::with_capacity(self.playout_consumers.len());
        for slot in self.playout_consumers.iter_mut() {
            let c = slot.take().expect("playout consumer already moved");
            play_cons.push(c);
        }

        //활성화 트랙 저장
        let active_idxs: Arc<Vec<usize>> = Arc::new((0..play_cons.len()).collect());
        let flush_flag = Arc::clone(&self.flush_flag);
        let params = Arc::clone(&self.real_time_params); //실시간 파라미터 핸들
        let active = active_idxs.clone(); //활성화 트랙 인덱스
        let transport_c = Arc::clone(&self.play_time_manager); //트랜스포트 핸들
        let budget_c = Arc::clone(&self.budget);
        let wait_gate = Arc::clone(&self.thread_wait);
        #[derive(Clone, Copy)]
        struct Resamp {
            //선형보간 용 구조체
            frac: f32,
            s0_l: f32,
            s0_r: f32,
            s1_l: f32,
            s1_r: f32,
        }

        let mut last: Vec<[f32; 2]> = vec![[0.0, 0.0]; active_idxs.len()]; // 마지막 정상 L/R
        let mut ramp_pos: usize = 0; // 페이드인 램프
        let mut mix_l_buf: Vec<f32> = Vec::new();
        let mut mix_r_buf: Vec<f32> = Vec::new();
        let mut underrun_len: Vec<usize> = vec![0; active_idxs.len()];
        let mut resume_ramp: Vec<usize> = vec![0; active_idxs.len()];
        let stream = device.build_output_stream(
            &config, //출력 스트림 생성
            move |data: &mut [f32], _| {
                //출력 콜백 기본 구조 FnMut(&mut [T], &cpal::OutputCallbackInfo) 기본구조에 맞추어 data 버퍼와 콜백정보를 받음
                //폴리슁
                if flush_flag.swap(false, std::sync::atomic::Ordering::AcqRel) {
                    for (slot, &idx) in active_idxs.iter().enumerate() {
                        let c = &mut play_cons[idx];
                        // 프레임 단위로 싹 비우기
                        while c.pop().is_ok() {}
                        last[idx] = [0.0, 0.0]; // ← 튜플이 아니라 배열
                        underrun_len[slot] = 0;
                        resume_ramp[slot] = 0;
                    }
                    ramp_pos = 0;
                }

                //예외처리 - 스테레오가 아니거나 활성화 트랙이 없으면 무음
                if channels != 2 || active.is_empty() {
                    //스테레오가 아니거나 활성화 트랙이 없으면 무음
                    for sample in data.iter_mut() {
                        //data 버퍼 순환
                        *sample = 0.0; //0.0으로 채움
                    }
                    return;
                }

                // 이번 콜백에서 필요한 프레임 수
                let nframes = data.len() / 2;
                // 상한선 관리
                let mut popped_frames_total = 0usize;

                if mix_l_buf.len() != nframes {
                    mix_l_buf.resize(nframes, 0.0);
                    mix_r_buf.resize(nframes, 0.0);
                } else {
                    for v in &mut mix_l_buf[..] {
                        *v = 0.0;
                    }
                    for v in &mut mix_r_buf[..] {
                        *v = 0.0;
                    }
                }

                if !transport_c.in_playing() {
                    for s in data.iter_mut() {
                        *s = 0.0;
                    }
                    ramp_pos = 0;
                    return;
                }

                // playing일 때만 playhead 진행
                transport_c.advance_frames((data.len() / channels) as u64);

                // // 믹스 누적 버퍼(한 번에 모아서 씀)
                // let mut mix_l_buf = vec![0.0f32; nframes];
                // let mut mix_r_buf = vec![0.0f32; nframes];

                // 트랙 단위로 한 번만 lock 해서 nframes 만큼 pop → 누적
                for (slot, &idx) in active_idxs.iter().enumerate() {
                    // 파라미터 범위 체크
                    if idx >= params.volume.len()
                        || idx >= params.pan.len()
                        || idx >= params.muted.len()
                    {
                        continue;
                    }

                    // 볼륨/팬/뮤트
                    let muted = params.muted[idx].load(Ordering::Relaxed);
                    let vol =
                        f32::from_bits(params.volume[idx].load(Ordering::Relaxed)).clamp(0.0, 1.0);
                    let pan =
                        f32::from_bits(params.pan[idx].load(Ordering::Relaxed)).clamp(-1.0, 1.0);
                    let m = if muted { 0.0 } else { 1.0 };
                    let gl = m * vol * (1.0 - pan) * 0.5;
                    let gr = m * vol * (1.0 + pan) * 0.5;

                    let c = &mut play_cons[idx];
                    let mut popped_this = 0usize;

                    for f in 0..nframes {
                        // 1) RB2용 프레임 pop (+ 언더런 처리)
                        let got = c.pop();
                        let fr = if let Ok(v) = got {
                            // 정상 수급 → 이전에 언더런이 있던 트랙이면 복구 램프 시작
                            if underrun_len[slot] > 0 && resume_ramp[slot] == 0 {
                                resume_ramp[slot] = RESUME_RAMP_FRAMES; // 0→정상으로 페이드 인
                            }
                            underrun_len[slot] = 0;
                            popped_this += 1;
                            v
                        } else {
                            // 언더런: 마지막 샘플에서 0으로 짧게 페이드 아웃(클릭 방지)
                            const UNDER_FADE: usize = 256;
                            let u = {
                                let u = underrun_len[slot].saturating_add(1);
                                underrun_len[slot] = u;
                                u
                            };
                            let k = (u.min(UNDER_FADE) as f32) / (UNDER_FADE as f32); // 0..1
                            let lf = last[idx][0] * (1.0 - k);
                            let rf = last[idx][1] * (1.0 - k);

                            // 디코더/워커를 깨워 보충 유도
                            let (_, cv) = &*wait_gate;
                            cv.notify_all();

                            [lf, rf]
                        };

                        // 2) 복구 램프(0→정상): pop 재개 직후에 클릭을 없애려고 천천히 볼륨을 올림
                        let (mut l, mut r) = (fr[0], fr[1]);
                        if resume_ramp[slot] > 0 {
                            let k = 1.0 - (resume_ramp[slot] as f32 / RESUME_RAMP_FRAMES as f32); // 0→1
                            l *= k;
                            r *= k;
                            resume_ramp[slot] -= 1;
                        }

                        // 3) 믹스 누적 + last 갱신(항상 램프 적용 후 값으로)
                        last[idx] = [l, r];
                        mix_l_buf[f] += l * gl;
                        mix_r_buf[f] += r * gr;
                    }

                    popped_frames_total += popped_this;
                }

                // if popped_frames_total > 0 {
                //     let before = budget_c.frames();
                //     let after = budget_c.frames();
                //     let crossed_high = before > HIGH_FRAMES && after <= HIGH_FRAMES;
                //     let crossed_low = before >= LOW_FRAMES && after < LOW_FRAMES;
                //     if crossed_high || crossed_low {
                //         let (mx, cv) = &*wait_gate;
                //         let _g = mx.lock().unwrap();
                //         cv.notify_all();
                //     }
                // }

                if popped_frames_total > 0 {
                    let (_, cv) = &*wait_gate;
                    cv.notify_all();
                }

                // 램프 게인 곱해서 한 번에 출력
                for (f, frame) in data.chunks_mut(2).enumerate() {
                    let m = if ramp_pos < RAMP_FRAMES {
                        (ramp_pos as f32) / (RAMP_FRAMES as f32)
                    } else {
                        1.0
                    };
                    ramp_pos = ramp_pos.saturating_add(1);

                    frame[0] = (mix_l_buf[f] * m).clamp(-1.0, 1.0);
                    frame[1] = (mix_r_buf[f] * m).clamp(-1.0, 1.0);
                }
                let n = CTR_CB.fetch_add(1, Ordering::Relaxed);
                if n % 128 == 0 {
                    rlog!(
                        "rb2_pop={}, budget={}, low={}, high={}",
                        popped_frames_total,
                        budget_c.frames(),
                        LOW_FRAMES,
                        HIGH_FRAMES
                    );
                }
                // rlog!("cpal output: sr={} ch={}", sr, channels);
            },
            err_fn,
            None,
        )?;
        stream.play()?;
        Ok(stream)
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
            self.budget.add(produced);
        }
        Ok(())
    }

    #[allow(dead_code)]
    pub fn prefill_rb2_blocking(&mut self, target_frames: usize) -> Result<(), String> {
        if self.copythread_worker.is_some() {
            return Err("RB2 prefill must run before copy thread starts".into());
        }

        if self.sound_output.is_some() {
            return Err("RB2 prefill must run before output stream starts".into());
        }
        let sr = self.play_time_manager.sr();
        let ntracks = self.track.len();
        let active = self.active_idx.load(Ordering::Relaxed);
        let tpos = self.play_time_manager.pos_frames();
        // 두 뱅크를 순차로 채운다: bank = 0, 1
        for bank in 0..2 {
            let mut filled: Vec<usize> = vec![0; ntracks];
            'outer_each_bank: loop {
                let mut all_ok = true;

                for i in 0..ntracks {
                    if filled[i] >= target_frames {
                        continue;
                    }
                    all_ok = false;

                    let Some(rb2) = self.playout_producers_bank[bank][i].as_mut() else {
                        return Err(format!("RB2 bank{bank} producer[{i}] not available"));
                    };
                    if rb2.is_full() {
                        std::thread::yield_now();
                        continue;
                    }

                    // RB1에서 L/R 1프레임 뽑기
                    let mut l_opt = None;
                    let mut r_opt = None;
                    if let Ok(mut rc) = self.consumers[i].lock() {
                        l_opt = rc.pop().ok();
                        r_opt = rc.pop().ok();
                    }

                    // 부족하면 RB1 보충
                    if l_opt.is_none() || r_opt.is_none() {
                        {
                            let mut tr = self.track_run_time[i].lock().map_err(|_| "rt lock")?;
                            let mut dec = self.decod[i].lock().map_err(|_| "dec lock")?;
                            let mut prod = self.producers[i].lock().map_err(|_| "prod lock")?;
                            let tempo_ratio = {
                                let bpm_bits = self.real_time_params.bpm.load(Ordering::Relaxed);
                                let bpm = f32::from_bits(bpm_bits);
                                (bpm / BASE_BPM).clamp(0.25, 4.0)
                            };
                            let n = fill_track_once(
                                &mut *tr,
                                &mut *dec,
                                &mut *prod,
                                CHUNK_DECODE,
                                sr,
                                tempo_ratio,
                                tpos,
                            )
                            .map_err(|e| format!("fill_track_once: {e}"))?;
                            self.budget.add(n);
                        }
                        if let Ok(mut rc) = self.consumers[i].lock() {
                            if l_opt.is_none() {
                                l_opt = rc.pop().ok();
                            }
                            if r_opt.is_none() {
                                r_opt = rc.pop().ok();
                            }
                        }
                    }

                    let (Some(l), Some(r)) = (l_opt, r_opt) else {
                        std::thread::sleep(std::time::Duration::from_micros(200));
                        continue;
                    };

                    if rb2.push([l, r]).is_ok() {
                        filled[i] += 1;
                        self.budget.sub(1);
                        if bank == active {
                            self.rb2_level[i].fetch_add(1, Ordering::Relaxed);
                        } else {
                            self.rb2_level_sb[i].fetch_add(1, Ordering::Relaxed);
                        }
                    } else {
                        std::thread::yield_now();
                    }
                }

                if all_ok {
                    break 'outer_each_bank;
                }
            }
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
        self.budget.reset();

        let _ = self.prefill_rb1_blocking(PREFILL_ON_SEEK * 2);
        self.wake_workers();
        self.last_auto_rebuffer_at = Instant::now();
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

    #[allow(dead_code)]
    fn try_bank_swap(&mut self) {
        const DWELL_MS: u64 = 2000; // 스왑 최소 간격
        const DEBOUNCE_N: usize = 32; // 스왑 판정 안정화

        let now = Instant::now();
        if now.duration_since(self.last_bank_swap_at) < Duration::from_millis(DWELL_MS) {
            return;
        }

        // 현재 active/standby 레벨 배열
        let a = self.active_idx.load(Ordering::Acquire);
        let (lvl_active, lvl_standby) = if a == 0 {
            (&self.rb2_level, &self.rb2_level_sb)
        } else {
            (&self.rb2_level_sb, &self.rb2_level)
        };

        let min_active = lvl_active
            .iter()
            .map(|x| x.load(Ordering::Relaxed))
            .min()
            .unwrap_or(0);
        let min_standby = lvl_standby
            .iter()
            .map(|x| x.load(Ordering::Relaxed))
            .min()
            .unwrap_or(0);

        // 🔒 트랙별 최소치도 확인 (전 트랙이 SWAP_TARGET_STANDBY 이상일 때만)
        let standby_ok_all_tracks = lvl_standby
            .iter()
            .all(|lv| lv.load(Ordering::Relaxed) >= SWAP_TARGET_STANDBY);

        // 액티브가 충분히 낮아야 스왑 고려
        if min_active <= SWAP_MIN_ACTIVE {
            // 스탠바이가 모자라면: 스왑 금지, 충전만 요청
            if !(min_standby >= SWAP_MIN_STANDBY && standby_ok_all_tracks) {
                self.precharge_req.store(true, Ordering::Release); // ★ 스탠바이 강제 충전 ON
                self.swap_qualify_streak = 0;
                self.wake_workers();
                return;
            }

            // 여기까지 오면 “거의 만땅” → 디바운스 후 스왑
            self.swap_qualify_streak += 1;
            if self.swap_qualify_streak >= DEBOUNCE_N {
                self.active_idx.store(a ^ 1, Ordering::Release);
                self.last_bank_swap_at = now;
                self.swap_qualify_streak = 0;
                self.precharge_req.store(false, Ordering::Release); // 스왑 직후 충전요청 해제
                self.last_swap_epoch.fetch_add(1, Ordering::Release);
            }
        } else {
            self.swap_qualify_streak = 0;
            // 상황에 따라 여기서 선충전 켤 수도 있음(원하면 주석 해제)
            self.precharge_req.store(true, Ordering::Release);
        }
    }
    #[allow(dead_code)]
    fn rebuild_rb2_bank_ringbuffers(&mut self) {
        let n = self.track.len();

        // 새로 모두 만든다 (두 뱅크 모두)
        let mut bank_prod0 = Vec::with_capacity(n);
        let mut bank_cons0 = Vec::with_capacity(n);
        let mut bank_prod1 = Vec::with_capacity(n);
        let mut bank_cons1 = Vec::with_capacity(n);

        for _ in 0..n {
            let (tx0, rx0) = RingBuffer::<[f32; 2]>::new(RB2_FRAMES);
            let (tx1, rx1) = RingBuffer::<[f32; 2]>::new(RB2_FRAMES);
            bank_prod0.push(Some(tx0));
            bank_cons0.push(Some(rx0));
            bank_prod1.push(Some(tx1));
            bank_cons1.push(Some(rx1));
        }

        self.playout_producers_bank = [bank_prod0, bank_prod1];
        self.playout_consumers_bank = [bank_cons0, bank_cons1];

        // 레벨/상태 초기화
        for lvl in self.rb2_level.iter() {
            lvl.store(0, Ordering::Relaxed);
        }
        for lvl in self.rb2_level_sb.iter() {
            lvl.store(0, Ordering::Relaxed);
        }
        self.active_idx.store(0, Ordering::Relaxed);
        self.last_bank_swap_at = Instant::now();
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
        // 복제 스레드부터 정리 (2차 P drop + 재생성 안 해도 됨: 어차피 drop 중)
        if self.copythread_worker.is_some() {
            self.stop_copy_thread();
        }

        self.thread_stop.store(true, Ordering::Relaxed);
        self.wake_workers();

        if let Some(stream) = self.sound_output.take() {
            let _ = stream.pause();
        }
        for h in self.thread_worker.drain(..) {
            let _ = h.join();
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_audio_track_new(number: i32) -> *mut TrackConfig {
    let track = match TrackConfig::new(number) {
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
pub extern "C" fn rust_audio_start_output(engine: *mut Engine) -> bool {
    if engine.is_null() {
        return false;
    }
    let eng = unsafe { &mut *engine };

    // 이미 시작했다면 패스
    if eng.sound_output.is_some() {
        eng.play_time_manager.start();
        eng.wake_workers();
        return true;
    }

    match eng.start_output_from_ringbuffer() {
        Ok(stream) => {
            eng.sound_output = Some(stream);
            eng.play_time_manager.start();
            eng.wake_workers();
            true
        }
        Err(_) => false,
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
