pub const CHANNELS: usize = 2;
#[inline]
pub fn slots(frames: usize) -> usize {
    frames * CHANNELS
} // frames → samples

// 링버퍼·FIFO 용량 (전부 frames 단위)
pub const RB1_FRAMES: usize = 16_777_216; // 트랙(1차) 링버퍼 용량
pub const RB2_FRAMES: usize = 131_072; // 재생(2차) 링버퍼 용량
pub const FIFO_MAX_FRAMES: usize = RB2_FRAMES; // 복제 스레드 내부 FIFO 상한

// 디코드/복제 청크 (frames)
pub const CHUNK_DECODE: usize = 65_536; // 디코더 워커가 한 번에 밀어넣는 크기
pub const CHUNK_COPY: usize = 8_192; // 복제 스레드 보간 출력 단위

// 전역 워터마크 (frames) — 히스테리시스
pub const HIGH_FRAMES: usize = 12288;
pub const LOW_FRAMES: usize = 4096;

// 출력 페이드인 길이 (frames)
pub const RAMP_FRAMES: usize = 256;

// 복제 스레드 전용 기준 (frames)
pub const FIFO_HWM_FRAMES: usize = RB2_FRAMES * 3 / 4;
pub const FIFO_LWM_FRAMES: usize = RB2_FRAMES / 8;
pub const PULL_BURST_FRAMES: usize = RB2_FRAMES / 4;

// seek 직후 동기 예열 크기 (frames)
pub const PREFILL_ON_START: usize = RB1_FRAMES - 65_536;
pub const PREFILL_ON_SEEK: usize = RB1_FRAMES / 2; // ≈ 170ms

pub const PREFILL_RB2_FRAMES: usize = RB2_FRAMES - 4_096;

pub const MAX_BUDGET: usize = HIGH_FRAMES * 6;

pub const RESUME_RAMP_FRAMES: usize = 512;

pub const BASE_BPM: f32 = 60.0;

//swap 전용 기준
pub const SWAP_MIN_ACTIVE: usize = LOW_FRAMES / 2; // 액티브가 12.5% 이하일 때만 스왑 고려
pub const SWAP_MIN_STANDBY: usize = HIGH_FRAMES; // 최소 75%는 차 있어야 “스왑 후보”
pub const SWAP_TARGET_STANDBY: usize = RB2_FRAMES - 2048;
