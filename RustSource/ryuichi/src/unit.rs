pub const CHANNELS: usize = 2;
#[inline]
pub fn slots(frames: usize) -> usize {
    frames * CHANNELS
} // frames → samples

// 링버퍼·FIFO 용량 (전부 frames 단위)
pub const RB1_FRAMES: usize = 16_777_216; // 트랙(1차) 링버퍼 용량

// 디코드/복제 청크 (frames)
pub const CHUNK_DECODE: usize = 65_536; // 디코더 워커가 한 번에 밀어넣는 크기
pub const CHUNK_COPY: usize = 8_192; // 복제 스레드 보간 출력 단위

// 전역 워터마크 (frames) — 히스테리시스
pub const HIGH_FRAMES: usize = 12288;
pub const LOW_FRAMES: usize = 4096;


// seek 직후 동기 예열 크기 (frames)
pub const PREFILL_ON_START: usize = RB1_FRAMES - 65_536;
pub const PREFILL_ON_SEEK: usize = RB1_FRAMES / 2; // ≈ 170ms

pub const MAX_BUDGET: usize = HIGH_FRAMES * 6;
pub const BASE_BPM: f32 = 60.0;
