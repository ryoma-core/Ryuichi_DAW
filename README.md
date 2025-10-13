<h1 align="center">Ryuichi DAW — JUCE × Rust (FFI)</h1>

<p align="center">
  <em>JUCE 기반 C++ UI + Rust 오디오 엔진(DLL) — 디코딩 · VST3 · 믹싱 · 출력(JUCE)</em><br/>
  <sub>Lock-free ring buffer(rtrb), Symphonia 디코더, JUCE 오디오 출력</sub>
</p>

<p align="center">
  <a href="https://www.rust-lang.org/"><img alt="Rust" src="https://img.shields.io/badge/Rust-stable-blue?logo=rust" /></a>
  <a href="https://juce.com/"><img alt="JUCE" src="https://img.shields.io/badge/JUCE-C%2B%2B-8A2BE2" /></a>
  <img alt="Platform" src="https://img.shields.io/badge/Platform-Windows%20x64-black" />
  <img alt="Audio" src="https://img.shields.io/badge/Audio-48kHz%20stereo-1abc9c" />
</p>

<hr/>

## ✨ 특징
- C++ ↔ Rust **직접 FFI** (`#[no_mangle] extern "C"`)
- **rtrb**(lock-free ring buffer)로 트랙별 파이프라인
- **symphonia**로 디코딩, **JUCE**로 출력
- 볼륨/뮤트/팬 파라미터, 타임라인/클립 구조
- 언더런 튜닝을 위한 **프레임 묶음 크기(FILL_FRAMES / CHUNK_FRAMES)** 및 **버퍼 용량(CAPACITY_SAMPLES)** 노출

---

## 🗂️ 폴더 구성
<br/>
Ryuichi_App/
<br/>
├── Source/                              # 메인 실행
<br/>
│   ├── Main/                            # 메인 파일
<br/>
│   ├── AudioEngine/                     # 오디오관련 관리
<br/>
│   ├── AssetsPath/                      # 에셋 경로 관리
<br/>
│   ├── ClipData/                        # Clip으로 오디오 파형그리기
<br/>
│   ├── soundData/                       # 파라메터 값들 저장
<br/>
│   └── TimeLineState/                   # UI 타임 핸들러
<br/>
├── GUI/                                 # GUI 관련
<br/>
│   ├── BackGround/                      # MainComponent mainWindow 관리
<br/>
│   ├── VST3Window/                      # VST3 Window 설정 
<br/>
│   ├── SoundSource/                     # 사운드 관련 에셋들 UI 처리 
<br/>
│   ├── Button/                          # 버튼 이벤트 UI 처리 
<br/>
│   ├── Track/                           # 트랙관련 UI 처리
<br/>
│   ├── Slider/                          # Track 볼륨 조절 처리
<br/>
│   ├── Mixer/                           # Mixer 관련 UI 처리
<br/>
│   ├── PlayBar/                         # PlayBar 재생,정지,리버브,BPM관리
<br/>
│   └── LookAndFeel/                     # 특정 JUCE 제공 이벤트를 커스텀GUI설정          
<br/>
├── Sound/                               # 오디오 콜백으로 사운드 출력 관리
<br/>
└── README.md                            # 프로젝트 설명 파일
---

## 🧰 사전 준비 (Windows)
- **Projucer** 설치
<br/>
설치후 Ryuichi.jucer 프로젝트 오픈
<br/>
오픈후 비쥬얼스튜디오 빌드 진행

```text
   실행 시 DLL 파일이 없다면 "정상" (Rust 엔진을 아직 안 붙였기 때문)
```

---

## ⚙️ Rust 엔진 빌드(DLL)
- **Rust(cargo)** 설치
<br/>
Ryuichi\RustSource\ryuichi를 vscode를 통하여 폴더 Open 터미널을 통하여 

```powershell
cargo build --release 빌드
```
<br/>

1. 빌드 완료 이후 Ryuichi\RustSource\ryuichi\target\release 폴더 안에 ryuichi.dll 파일 복사 
2. Ryuichi\Builds\VisualStudio2022\x64\Debug(아님 Release)\App 에 붙여 넣기
3. Projucer에 Exporters 설정이 안되어있다면 진행

```
Projucer에 Exporters 설정 Visual Studio 2022에 Debug , Release 둘다
Extra Library Search Paths -> Rust 릴리즈 빌드 하여 추출된 DLL 파일 경로를 입력 (예시:RustSource\ryuichi\target\release)
Configuration-specific Linker Flags -> Rust 릴리즈 빌드하여 생성된 DLL 파일 이름을 등록 (예시:ryuichi.dll.lib)
```

---
## 🔗 C++ ↔ Rust FFI 헤더

AudioEngine.h
```
#pragma once
#include <cstdint>

extern "C" {
    struct TrackConfig;
    struct Engine;

    TrackDatas* rust_audio_track_new(std::int32_t number);
    void rust_audio_track_free(TrackDatas* track);

    Engine* rust_audio_engine_new(TrackDatas* track0, TrackDatas* track1, TrackDatas* track2, TrackDatas* track3);
    void rust_audio_engine_free(Engine* engine);
}
```
Rust 쪽에는 동일 시그니처로 #[no_mangle] extern "C" 함수가 구현돼 있어야 합니다.

---