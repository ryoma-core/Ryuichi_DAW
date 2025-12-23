<p align="center">
  <a href="https://www.youtube.com/watch?v=Q7ig6vn-y4M">
    <img src="UI_Image/64px-YouTube_full-color_icon_(2017).svg.png" alt="유튜브" width="64"/>
  </a>
</p>
<h1 align="center">Ryuichi DAW(샘플 작곡 소프트웨어) — JUCE × Rust (FFI)</h1>

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

| 경로(Path)                            | 설명                         |
|--------------------------------------|------------------------------|
| `Ryuichi_App/`                       | 루트 디렉터리                |
| `Ryuichi_App/Source/`                | 메인 실행 소스               |
| `Ryuichi_App/Source/Main/`           | 메인 엔트리/부트스트랩 파일  |
| `Ryuichi_App/Source/AudioEngine/`    | 오디오 엔진 관리, I/O, 콜백 연동 |
| `Ryuichi_App/Source/AssetsPath/`     | 에셋 경로 유틸 및 로더       |
| `Ryuichi_App/Source/ClipData/`       | 오디오 파형 그리기, 클립 데이터 |
| `Ryuichi_App/Source/soundData/`      | 파라미터 저장소(볼륨/팬/뮤트 등) |
| `Ryuichi_App/Source/TimeLineState/`  | UI 타임라인 상태/핸들러      |
| `Ryuichi_App/GUI/`                   | GUI 전반                     |
| `Ryuichi_App/GUI/BackGround/`        | 메인 윈도우 배경/레이아웃    |
| `Ryuichi_App/GUI/VST3Window/`        | VST3 윈도우 관리             |
| `Ryuichi_App/GUI/SoundSource/`       | 사운드 에셋 브라우저/뷰      |
| `Ryuichi_App/GUI/Button/`            | 버튼 위젯 및 이벤트 처리     |
| `Ryuichi_App/GUI/Track/`             | 트랙 UI 컴포넌트             |
| `Ryuichi_App/GUI/Slider/`            | 트랙 볼륨/슬라이더 컨트롤    |
| `Ryuichi_App/GUI/Mixer/`             | 믹서 UI(채널, 버스 등)       |
| `Ryuichi_App/GUI/PlayBar/`           | 재생/정지/리버브/BPM 컨트롤  |
| `Ryuichi_App/GUI/LookAndFeel/`       | 커스텀 Look&Feel 테마        |
| `Ryuichi_App/Sound/`                 | 오디오 콜백/출력 관리        |
| `Ryuichi_App/README.md`              | 프로젝트 설명                |

---

## 🧰 사전 준비 (Windows)
- **Projucer** 설치
- 'Ryuichi.jucer' 프로젝트 오픈
- 비주얼 스튜디오 빌드 진행

> 실행 시 DLL 파일이 없다면 "정상" (Rust 엔진을 아직 안 붙였기 때문)
<br/>
필수 에셋 :
<br/>
[Assets 다운로드](https://drive.google.com/file/d/1m9ydxmQDN2TVKN6PVAy9Syy_I6pJ7Srv/view?usp=sharing)

```text
Assets.zip 파일 다운로드 다운하여 C:\Ryuichi\Builds\VisualStudio2022\x64\Debug(아님 Release)\App 에 압축을 해제하여 디렉토리 형태로 넣어준다.
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
