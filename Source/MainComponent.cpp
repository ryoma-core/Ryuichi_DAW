#include "MainComponent.h"
#include "AudioEngine.h"

//==============================================================================
MainComponent::MainComponent()
{
#pragma region Setting
    formatManager.addDefaultFormats();
    setSize (1200, 600);
    setFramesPerSecond(60);
    addAndMakeVisible(soundBrowser);
    addAndMakeVisible(mainTrack);
    mainTrack.setInterceptsMouseClicks(true, true);
    addAndMakeVisible(mixers);
    addAndMakeVisible(playBar);
    soundBrowser.addMouseListener(this, true);
    audioEngine->audioTrack_0 = mainTrack_0;
    audioEngine->audioTrack_1 = mainTrack_1;
    audioEngine->audioTrack_2 = mainTrack_2;
    audioEngine->audioTrack_3 = mainTrack_3;
    juce::File fileDragFile(FILEDRAG_DIR_PATH);
    if (fileDragFile.existsAsFile())
    {
        fileDragIcon = juce::ImageFileFormat::loadFrom(fileDragFile);
    }
    addMouseListener(this, true);
    juce::File vstR(DefaultVSTReverb);
    if (vstR.existsAsFile()) {
        const double sr = static_cast<double>(audioEngine->rust_get_out_sr());
        const int    bs = static_cast<int>(audioEngine->rust_get_out_bs());
        DefaultVST3FromFile(reverb,vstR.getFullPathName(), sr, bs);
    }
    playBar.reverbToggleButton.onClick = [this]() {
        if (!(reverb && reverb->instance)) { DBG("reverb not ready"); return; }
        reverbEnabled = playBar.reverbToggleButton.getToggleState();
        DBG(juce::String("REVERB_BUTTON_STATE=") << (reverbEnabled ? "On" : "Off"));
        reverb->instance->suspendProcessing(!reverbEnabled);
        if (audioEngine && audioEngine->host_)
            audioEngine->host_->setBypassed(reverb->instance.get(), !reverbEnabled);
        };
    soundBrowser.sourcePanel.soundFile->sample_path = [this](const char* path) {
        audioEngine->rust_sample_add(path);
        };
#pragma endregion
#pragma region FileDrepped callBack
    mainTrack.onDropIntoSubTrack = [this](int track, const juce::File& file, float laneX)
        {
            const double s = timeline.xToSamples(laneX);
            const uint64_t startProj = (uint64_t)timeline.snapSamples(s, 4);
            addClipToTrack(track, file, startProj);
        };
#pragma endregion
#pragma region SubTrackImg reference
    mainTrack.subTrack_0->soundTrackImg = &(mainTrack_0->soundWaveForm);
    mainTrack.subTrack_1->soundTrackImg = &(mainTrack_1->soundWaveForm);
    mainTrack.subTrack_2->soundTrackImg = &(mainTrack_2->soundWaveForm);
    mainTrack.subTrack_3->soundTrackImg = &(mainTrack_3->soundWaveForm);

    mainTrack.subTrack_0->playheadSamples = &subTrackTime;
    mainTrack.subTrack_1->playheadSamples = &subTrackTime;
    mainTrack.subTrack_2->playheadSamples = &subTrackTime;
    mainTrack.subTrack_3->playheadSamples = &subTrackTime;
#pragma endregion
#pragma region TrackClear
    mainTrack.handleMenuSelection = [this](int selectedId) 
        {
            switch (selectedId)
            {
            case 1:
                DBG("0 Delete");
                mainTrack_0->fileNames.clear();
                mainTrack_0->filePaths.clear();
                mainTrack_0->soundWaveForm.clear();
                repaint();
                if (audioEngine) {
                    DBG("[Rust]-file_all_delete");
                }
                break;
            case 2:
                DBG("1 Delete");
                mainTrack_1->fileNames.clear();
                mainTrack_1->filePaths.clear();
                mainTrack_1->soundWaveForm.clear();
                repaint();
                if (audioEngine) {
                    DBG("[Rust]-file_all_delete");
                }
                break;
            case 3:
                DBG("2 Delete");
                mainTrack_2->fileNames.clear();
                mainTrack_2->filePaths.clear();
                mainTrack_2->soundWaveForm.clear();
                repaint();
                if (audioEngine) {
                    DBG("[Rust]-file_all_delete");
                }
                break;
            case 4:
                DBG("3 Delete");
                mainTrack_3->fileNames.clear();
                mainTrack_3->filePaths.clear();
                mainTrack_3->soundWaveForm.clear();
                repaint();
                if (audioEngine) {
                    DBG("[Rust]-file_all_delete");
                }
                break;
            default:
                break;
            }
        };
#pragma endregion
#pragma region SubTrackController_Volume_and_Mut_and_BPM
    mainTrack.subTrackController_0->slider.onValueChange = [this]() {
        mainTrack_0->volume = mainTrack.subTrackController_0->slider.getValue();
        if (audioEngine->rust_volume_update(mainTrack_0->volume, 0)) {
            DBG("[volume_Update]-Ok");
        }
        else {
            DBG("[volume_Update]-Error");
        }
        };
    mainTrack.subTrackController_1->slider.onValueChange = [this]() {
        mainTrack_1->volume = mainTrack.subTrackController_1->slider.getValue();
        if(audioEngine->rust_volume_update(mainTrack_1->volume, 1)) {
            DBG("[volume_Update]-Ok");
        }
        else {
            DBG("[volume_Update]-Error");
        }
        };
    mainTrack.subTrackController_2->slider.onValueChange = [this]() {
        mainTrack_2->volume = mainTrack.subTrackController_2->slider.getValue();
        if (audioEngine->rust_volume_update(mainTrack_2->volume, 2)) {
            DBG("[volume_Update]-Ok");
        }
        else {
            DBG("[volume_Update]-Error");
        }
        };
    mainTrack.subTrackController_3->slider.onValueChange = [this]() {
        mainTrack_3->volume = mainTrack.subTrackController_3->slider.getValue();
        if(audioEngine->rust_volume_update(mainTrack_3->volume, 3)) {
            DBG("[volume_Update]-Ok");
        }
        else {
            DBG("[volume_Update]-Error");
        }
        };


    mainTrack.subTrackController_0->muteToggleButton.onClick = [this]() {
        mainTrack_0->isMuted = mainTrack.subTrackController_0->muteToggleButton.getToggleState();
        if (audioEngine->rust_mute_update(mainTrack_0->isMuted,0)) {
            DBG("[Mute_Update]-Ok");
        }
        else {
            DBG("[Mute_Update]-Error");
        }
        };
    mainTrack.subTrackController_1->muteToggleButton.onClick = [this]() {
        mainTrack_1->isMuted = mainTrack.subTrackController_1->muteToggleButton.getToggleState();
        if (audioEngine->rust_mute_update(mainTrack_1->isMuted, 1)) {
            DBG("[Mute_Update]-Ok");
        }
        else {
            DBG("[Mute_Update]-Error");
        }
        };
    mainTrack.subTrackController_2->muteToggleButton.onClick = [this]() {
        mainTrack_2->isMuted = mainTrack.subTrackController_2->muteToggleButton.getToggleState();
        if (audioEngine->rust_mute_update(mainTrack_2->isMuted, 2)) {
            DBG("[Mute_Update]-Ok");
        }
        else {
            DBG("[Mute_Update]-Error");
        }
        };
    mainTrack.subTrackController_3->muteToggleButton.onClick = [this]() {
        mainTrack_3->isMuted = mainTrack.subTrackController_3->muteToggleButton.getToggleState();
        if (audioEngine->rust_mute_update(mainTrack_3->isMuted, 3)) {
            DBG("[Mute_Update]-Ok");
        }
        else {
            DBG("[Mute_Update]-Error");
        }
        };
    playBar.bpm.bpmEditor.onTextChange = [this]() {

        const float newBpm = playBar.bpm.bpmEditor.getText().getFloatValue();
        if (newBpm > 0.0f && audioEngine->rust_bpm_update(newBpm)) {
            timeline.bpm = newBpm;
            const double outSR = (double)audioEngine->rust_get_out_sr();
            for (int t = 0; t < 4; ++t) {
                for (auto* c : clips[t]) c->recalcProjectFrames(outSR, newBpm);
                repaintTrack(t);

            }
            playBar.repaint();
            DBG("[BPM_Update]-Ok");
        }
        else {
            DBG("[BPM_Update]-Error");
        }
        };
#pragma endregion
#pragma region MixerController_Pan_R_D
    mixers.trackMixer_0->volumeKnob.onValueChange = [this]() {
        mainTrack_0->pan = mixers.trackMixer_0->volumeKnob.getValue();
        if (audioEngine->rust_pan_update(mainTrack_0->pan, 0)) {
            DBG("[Pan_Update]-Ok");
        }
        else {
            DBG("[Pan_Update]-Error");
        }
        };
    mixers.trackMixer_1->volumeKnob.onValueChange = [this]() {
        mainTrack_1->pan = mixers.trackMixer_1->volumeKnob.getValue();
        if (audioEngine->rust_pan_update(mainTrack_1->pan, 1)) {
            DBG("[Pan_Update]-Ok");
        }
        else {
            DBG("[Pan_Update]-Error");
        }
        };
    mixers.trackMixer_2->volumeKnob.onValueChange = [this]() {
        mainTrack_2->pan = mixers.trackMixer_2->volumeKnob.getValue();
        if (audioEngine->rust_pan_update(mainTrack_2->pan, 2)) {
            DBG("[Pan_Update]-Ok");
        }
        else {
            DBG("[Pan_Update]-Error");
        }
        };
    mixers.trackMixer_3->volumeKnob.onValueChange = [this]() {
        mainTrack_3->pan = mixers.trackMixer_3->volumeKnob.getValue();
        if (audioEngine->rust_pan_update(mainTrack_3->pan, 3)) {
            DBG("[Pan_Update]-Ok");
        }
        else {
            DBG("[Pan_Update]-Error");
        }
        };
#pragma endregion
#pragma region bind_Data
    mainTrack.subTrack_0->bindTimeline(&timeline);
    mainTrack.subTrack_1->bindTimeline(&timeline);
    mainTrack.subTrack_2->bindTimeline(&timeline);
    mainTrack.subTrack_3->bindTimeline(&timeline);

    mainTrack.subTrack_0->bindClips(&clips[0]);
    mainTrack.subTrack_1->bindClips(&clips[1]);
    mainTrack.subTrack_2->bindClips(&clips[2]);
    mainTrack.subTrack_3->bindClips(&clips[3]);
#pragma endregion
#pragma region Timeline
    mainTrack.playhead.onDragStart = [this]() {
        timeHandler->stopTimer();
        wasPlayingWhileDrag = audioEngine->rust_get_is_playing();
        //if (wasPlayingWhileDrag) audioEngine->rust_start_sound(false); // STOP
        };
    mainTrack.playhead.onDragEnd = [this]() {
        const uint64_t s = static_cast<uint64_t>(mainTrack.playhead.getValue());
        audioEngine->rust_set_play_time(s); // SEEK
        //if (wasPlayingWhileDrag) audioEngine->rust_start_sound(true); // 재개
        timeHandler->startTimerHz(60);
        };
#pragma endregion
#pragma region VST3
    soundBrowser.sourcePanel.vstFile->vstPanel.get()->onDoubleClick = [this](const juce::File& f) {
        
        const double sr = static_cast<double>(audioEngine->rust_get_out_sr());
        const int    bs = static_cast<int>(audioEngine->rust_get_out_bs());
        loadVST3FromFile(f.getFullPathName() , sr, bs);
        /*audioEngine->rust_vst3_execution(s.toRawUTF8());*/
    };
#pragma endregion
#pragma region SAVE
    mainTrack.onExportWav = [this]() {
        audioEngine->rust_save_wav();
        };
#pragma endregion 
}
MainComponent::~MainComponent()
{
    if (audioEngine && audioEngine->host_)
        audioEngine->host_->clearPlugins();

    for (auto& s : pluginSlots) {
        if (s.instance) {
            s.instance->suspendProcessing(true);
            s.instance->releaseResources();
        }
        s.window.reset();
    }
    pluginSlots.clear();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    mainTrack.subTrackController_0->muteToggleButton.setLookAndFeel(nullptr);
    mainTrack.subTrackController_1->muteToggleButton.setLookAndFeel(nullptr);
    mainTrack.subTrackController_2->muteToggleButton.setLookAndFeel(nullptr);
    mainTrack.subTrackController_3->muteToggleButton.setLookAndFeel(nullptr);

    mixers.trackMixer_0->volumeKnob.setLookAndFeel(nullptr);
    mixers.trackMixer_1->volumeKnob.setLookAndFeel(nullptr);
    mixers.trackMixer_2->volumeKnob.setLookAndFeel(nullptr);
    mixers.trackMixer_3->volumeKnob.setLookAndFeel(nullptr);

    //mixers.trackMixer_0->delayToggleButton.setLookAndFeel(nullptr);
    //mixers.trackMixer_1->delayToggleButton.setLookAndFeel(nullptr);
    //mixers.trackMixer_2->delayToggleButton.setLookAndFeel(nullptr);
    //mixers.trackMixer_3->delayToggleButton.setLookAndFeel(nullptr);

    playBar.reverbToggleButton.setLookAndFeel(nullptr);

    playBar.playToggleButton.setLookAndFeel(nullptr);
    playBar.stopToggleButton.setLookAndFeel(nullptr);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
//    g.fillAll (juce::Colour::fromString("#2B2B2B"));
//    juce::Font John("Segoe UI", 35.0f, juce::Font::italic);
//    g.setFont (John);
//    g.setColour (juce::Colours::black);
//    g.drawText (backGroundName,getLocalBounds(), juce::Justification::centred, true);
//
//#pragma region Animated
//    g.setColour(juce::Colours::grey);
//
//    float radiusX = 150.0f;
//    float radiusY = 100.0f;
//    float t = (float)getFrameCounter() * 0.06f;
//
//    float x = getWidth() / 2.0f + radiusX * std::sin(t);
//    float y = getHeight() / 2.0f + radiusY * std::sin(2 * t);
//
//    float tPrev = t - 0.06f;
//    float prevX = getWidth() / 2.0f + radiusX * std::sin(tPrev);
//    float prevY = getHeight() / 2.0f + radiusY * std::sin(2 * tPrev);
//
//    g.drawLine(prevX, prevY, x, y, 5.0f);
//#pragma endregion
}
void MainComponent::update()
{

}
void MainComponent::resized()
{
    soundBrowser.setBounds(10, 10, 300, 1100);
    mainTrack.setBounds(600, 200, 1200, 640);
    mixers.setBounds(600, 850, 1200, 240);
    playBar.setBounds(1000, 0, 300, 80);
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto* listBox = &soundBrowser.sourcePanel.soundFile->soundListBox;

    // 마우스 위치를 listBox 기준 좌표로 변환
    auto relativePos = e.getEventRelativeTo(listBox).position.toInt();

    // 마우스가 listBox 내부에 있을 때만 드래그 시작
    if (listBox->getLocalBounds().contains(relativePos))
    {

        auto dragDescription = soundBrowser.sourcePanel.soundFile->soundPanel->getDragSourceDescription(
            listBox->getSelectedRows());
        startDragging(dragDescription, listBox, fileDragIcon, true);
        
    }

    if (!isDraggingClip || !selectedClip) return;
    int hitTrack;
    float localX;
    if (!hitWhichTrackAndLocalX(e, hitTrack, localX)) return;

    const double sNow = timeline.xToSamples(localX);
    double newStart = sNow - dragGrabOffsetS;
    newStart = timeline.snapSamples(newStart, 4);
    newStart = std::max(0.0, newStart);
    if (hitTrack != selectedTrack)
    {
        const int oldTrack = selectedTrack;
        const int oldIdx = clips[selectedTrack].indexOf(selectedClip);
        if (oldIdx >= 0) {
            auto* p = clips[selectedTrack].removeAndReturn(oldIdx);
            clips[hitTrack].add(p);
            selectedTrack = hitTrack;
            repaintTrack(oldTrack);
        }
    }
    selectedClip->startProjFrames = (uint64_t)std::llround(newStart);
    repaintTrack(selectedTrack);

    dragNewTrack = selectedTrack;
    dragNewStart = selectedClip->startProjFrames;
}
void MainComponent::mouseUp(const juce::MouseEvent&)
{
    if (isDraggingClip && selectedClip) {
        const int newTrack = dragNewTrack;
        const uint64_t newStart = dragNewStart;

        if (!(dragOrigTrack == newTrack && dragOrigStart == newStart)) {
            const bool ok = audioEngine && audioEngine->rust_file_move(dragOrigTrack, dragOrigStart, dragNewTrack, dragNewStart);
            if (!ok) {
                DBG("[Rust]-File-Move : Err");
                const int newTrackWas = selectedTrack;
                int curIdx = clips[newTrackWas].indexOf(selectedClip);
                ClipData* p = (curIdx >= 0) ? clips[newTrackWas].removeAndReturn(curIdx)
                    : selectedClip;
                p->startProjFrames = dragOrigStart;
                clips[dragOrigTrack].add(p);

                selectedClip = p;
                selectedTrack = dragOrigTrack;

                repaintTrack(dragOrigTrack);
                if (newTrackWas != dragOrigTrack) repaintTrack(newTrackWas);
            }
            else {
                DBG("[Rust]-File-Move : Ok");
                repaintTrack(dragOrigTrack);
                if (newTrack != dragOrigTrack) repaintTrack(newTrack);
            }
        }
    }
    isDraggingClip = false;
    dragOrigTrack = dragNewTrack = -1;
    dragOrigStart = dragNewStart = 0;
}
void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    int hitTrack; float localX;
    if (!hitWhichTrackAndLocalX(e, hitTrack, localX)) return;

    const double s = timeline.xToSamples(localX);
    const uint64_t sClamped = (uint64_t)std::max(0.0, s);

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) //mouseRight Delete
    {
        const int idx = findClipIndexAtSample(hitTrack, sClamped);
        if (idx >= 0)
        {
            ClipData* victim = clips[hitTrack][idx];
            const uint64_t victimStart = victim->startProjFrames;

            if (audioEngine && audioEngine->rust_file_delet(hitTrack, victimStart)) {

                clips[hitTrack].remove(idx);              // OwnedArray is RAW delete
                if (selectedClip == victim) {             // Selecteding the delet
                    selectedClip = nullptr; selectedTrack = -1; isDraggingClip = false;
                }
                repaintTrack(hitTrack); //track 
            }
            else {
                DBG("[Rust]_sound_delete_clip_by_start failed");
            }
            return; // 우클릭은 여기서 끝
        }

    }

    const int idx = findClipIndexAtSample(hitTrack, sClamped);
    if (idx < 0) {
        // 빈 공간 클릭: 선택 해제
        selectedClip = nullptr; selectedTrack = -1; isDraggingClip = false;
        return;
    }

    // 이 지점에 있는 클립을 집는다
    selectedClip = clips[hitTrack][idx];
    selectedTrack = hitTrack;
    isDraggingClip = true;

    dragOrigTrack = hitTrack;
    dragOrigStart = selectedClip->startProjFrames;

    // 드래그 중 최신값(초기엔 원본과 동일)
    dragNewTrack = hitTrack;
    dragNewStart = selectedClip->startProjFrames;

    // 클릭 지점이 클립 시작으로부터 얼마나 떨어져 있는지(샘플) 저장
    dragGrabOffsetS = (double)sClamped - (double)selectedClip->startProjFrames;

}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'q' || key.getTextCharacter() == 'w' ||
        key.getTextCharacter() == 'e' || key.getTextCharacter() == 'r') { audioEngine->rust_sample_play(); }
    if (key.getTextCharacter() == '+') timeline.pxPerBeat = juce::jmin(800.0, timeline.pxPerBeat * 1.2);
    if (key.getTextCharacter() == '-') timeline.pxPerBeat = juce::jmax(10.0, timeline.pxPerBeat / 1.2);

    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        const bool newPlayState = !playBar.playToggleButton.getToggleState();
        playBar.playToggleButton.setToggleState(newPlayState, juce::sendNotification);

        const bool newStopState = !playBar.stopToggleButton.getToggleState();
        playBar.stopToggleButton.setToggleState(newStopState, juce::sendNotification);

        if (playBar.playToggleButton.getToggleState())
        {
            audioEngine->rust_start_sound(true);
            return true;
        }
        else
        {
            audioEngine->rust_start_sound(false);
            return false;
        }
    }

    mainTrack.subTrack_0->repaint();
    mainTrack.subTrack_1->repaint();
    mainTrack.subTrack_2->repaint();
    mainTrack.subTrack_3->repaint();
    return true;
}


void MainComponent::addClipToTrack(int track, const juce::File& file, uint64_t startSamples)
{
    if (track < 0 || track >= 4) return;
    if (!file.existsAsFile())     return;

    std::unique_ptr<juce::AudioFormatReader> r(audioShared.fm.createReaderFor(file));
    if (!r) return;

    const double   srcSRd = r->sampleRate;         if (srcSRd <= 0.0) return;
    const uint32_t srcSR = (uint32_t)std::llround(srcSRd);
    const uint64_t srcLenS = (uint64_t)r->lengthInSamples;

    // 1) 소스 좌표로 Clip 생성
    auto* c = new ClipData(audioShared.fm, audioShared.cache, file,
        /*startSrcSamples*/ 0,
        /*lenSrcSamples*/   srcLenS);

    // 2) 프로젝트 좌표로 환산(BPM, 출력 SR 반영)
    const double outSR = (double)audioEngine->rust_get_out_sr();
    const double bpm = timeline.bpm;
    c->recalcProjectFrames(outSR, bpm);

    // 3) 타임라인 배치(드랍 위치는 '프로젝트 프레임' 단위)
    c->startProjFrames = startSamples;

    clips[track].add(c);

    // 4) 엔진에도 프로젝트 좌표로 전달
    if (audioEngine) {
        const juce::String pathStr = file.getFullPathName();
        const char* path = pathStr.toRawUTF8();
        audioEngine->rust_file_update(
            track,
            path,
            /*startProjectFrames*/ c->startProjFrames,
            /*lenProjectFrames*/   c->lenProjFrames,
            /*srcSR*/              srcSR
        );
    }

    repaintTrack(track);
}

void MainComponent::repaintTrack(int track)
{
    if (track == 0 && mainTrack.subTrack_0) mainTrack.subTrack_0->repaint();
    else if (track == 1 && mainTrack.subTrack_1) mainTrack.subTrack_1->repaint();
    else if (track == 2 && mainTrack.subTrack_2) mainTrack.subTrack_2->repaint();
    else if (track == 3 && mainTrack.subTrack_3) mainTrack.subTrack_3->repaint();
}


void MainComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown()) {
     
        timeline.pxPerBeat = juce::jlimit(1.0, 2000.0, timeline.pxPerBeat * (1.0 + w.deltaY * 0.2));
    }
    if (e.mods.isShiftDown()) {
        // 가로 스크롤(원하는 감도값으로 조절)
        const double panPixels = -(w.deltaY != 0 ? w.deltaY : w.deltaX) * 120.0;
        timeline.scrollSamples = juce::jmax(0.0, timeline.scrollSamples + panPixels * timeline.samplesPerPixel());
    }
    mainTrack.subTrack_0->repaint(); 
    mainTrack.subTrack_1->repaint();
    mainTrack.subTrack_2->repaint(); 
    mainTrack.subTrack_3->repaint();
}
bool MainComponent::hitWhichTrackAndLocalX(const juce::MouseEvent& e, int& outTrack, float& outLocalX)
{
    auto p = e.getEventRelativeTo(&mainTrack).position.toInt();

    struct Lane { juce::Component* comp; int index; } lanes[] = {
       { mainTrack.subTrack_0.get(), 0 },
       { mainTrack.subTrack_1.get(), 1 },
       { mainTrack.subTrack_2.get(), 2 },
       { mainTrack.subTrack_3.get(), 3 },
    };

    for (auto& L : lanes) {
        if (!L.comp) continue;
        auto local = L.comp->getLocalPoint(&mainTrack, p);
        if (L.comp->getLocalBounds().contains(local)) {
            outTrack = L.index;
            outLocalX = (float)local.x;
            return true;
        }
    }
    return false;
}

int MainComponent::findClipIndexAtSample(int track, uint64_t s) const
{
    const auto& arr = clips[track];
    for (int i = 0; i < arr.size(); ++i) {
        if (auto* c = arr[i]) {
            if (s >= c->startProjFrames && s < c->startProjFrames + c->lenProjFrames)
            {
                return i;
            }
        }
    }
    return -1;
}

bool MainComponent::loadVST3FromFile(const juce::String& path, double sampleRate, int blockSize)
{
    for (auto* fmt : formatManager.getFormats()) //Pluginhost Check My project just one Pluginhost (VST3)
    {
        if (!fmt->fileMightContainThisPluginType(path)) continue; // this is click file in the pluginhost formats same check

        juce::OwnedArray<juce::PluginDescription> types; //PluginDescription
        fmt->findAllTypesForFile(types, path);
        if (types.isEmpty()) continue;                 //find?

        fmt->createPluginInstanceAsync(*types[0], sampleRate, blockSize, //find just 1 file [0]  sr and bs is sound play sr and plugin input data size
            [this, path, sampleRate, blockSize](std::unique_ptr<juce::AudioPluginInstance> inst, //Lambda
                const juce::String& err)
            {
                if (!inst) { DBG("VST load failed: " + err); return; } //inst error

                auto it = pluginSlots.emplace(pluginSlots.end()); //list add and get iterator
                it->path = path;                                  //set list
                it->instance = std::move(inst);
                it->instance->prepareToPlay(sampleRate, blockSize);

                // 호스트 체인에 등록 (소유권은 그대로 여기 유지)
                if (audioEngine && audioEngine->host_)
                    audioEngine->host_->addPlugin(it->instance.get(),false);

                if (it->instance->hasEditor())                   //instance plugin is GUI true?? false??
                {
                    auto* editor = it->instance->createEditorIfNeeded();         //GUI create Component
                    it->window = std::make_unique<PluginWindow>(it->instance->getName()); //class create 
                    it->window->setContentOwned(editor, true); //GUI content Class
                    it->window->centreWithSize(editor->getWidth(), editor->getHeight()); //size free size 
                    it->window->setVisible(true);                                        //see true

                    // 이 창만 닫을 때, 이 슬롯만 정리
                    it->window->onClose = [this, it]() mutable {                       //mutable is  compare Rust is mut ok? mut Lambda
                        if (!it->instance) { it->window.reset(); pluginSlots.erase(it); return; }
                        if (audioEngine && audioEngine->host_)
                            audioEngine->host_->removePlugin(it->instance.get());
                        if (it->instance) {                                            //instance Ok?
                            it->instance->suspendProcessing(true);                     //suspendProcessing is here plugin not now and wait
                            it->instance->releaseResources();                          //it->instance->prepareToPlay(sampleRate, blockSize);  is clear
                        }                                                              //and Class closeButtonPressed start?
                        it->window.reset();                                            // class clear
                        pluginSlots.erase(it);                                         // list clear(it)
                        };
                }

                DBG("Loaded: " + it->instance->getName());
            });

        return true;
    }
    DBG("Not a VST3: " + path);
    return false;
}

bool MainComponent::DefaultVST3FromFile(std::optional<DefaultPlugin> &plugin, const juce::String& path, double sampleRate, int blockSize)
{
    for (auto* fmt : formatManager.getFormats()) //Pluginhost Check My project just one Pluginhost (VST3)
    {
        if (!fmt->fileMightContainThisPluginType(path)) continue; // this is click file in the pluginhost formats same check

        juce::OwnedArray<juce::PluginDescription> types; //PluginDescription
        fmt->findAllTypesForFile(types, path);
        if (types.isEmpty()) continue;                 //find?

        fmt->createPluginInstanceAsync(*types[0], sampleRate, blockSize, //find just 1 file [0]  sr and bs is sound play sr and plugin input data size
            [this, &plugin, path, sampleRate, blockSize](std::unique_ptr<juce::AudioPluginInstance> inst, //Lambda
                const juce::String& err)
            {
                if (!inst) { DBG("VST load failed: " + err); return; } //inst error

                // 1) 기존 슬롯 깨끗이 제거(그래프에서 먼저 빼고 리소스 해제)
                if (plugin && plugin->instance)
                {
                    if (audioEngine && audioEngine->host_) {
                        audioEngine->host_->removePlugin(plugin->instance.get());
                        plugin->instance->suspendProcessing(true);
                        plugin->instance->releaseResources();
                        plugin->window.reset();
                        plugin.reset();
                    }
                }

                // 3) 슬롯에 소유권 이전
                plugin.emplace();
                plugin->instance = std::move(inst);

                // 2) 새 인스턴스 준비
                plugin->instance->prepareToPlay(sampleRate, blockSize);
                
                if (auto* bp = plugin->instance->getBypassParameter())
                    bp->setValueNotifyingHost(1.0f); // 1.0 = Bypass 활성(효과 OFF)

                // 4) 시작은 OFF(진짜 멈춤 시그널)
                plugin->instance->suspendProcessing(true);
                

                // 호스트 체인에 등록 (소유권은 그대로 여기 유지)
                if (audioEngine && audioEngine->host_)
                    audioEngine->host_->addPlugin(plugin->instance.get(), true);
                playBar.reverbToggleButton.setToggleState(false, juce::dontSendNotification);

                DBG("Loaded: " + plugin->instance->getName());
            });
        return true;
    }
    DBG("Not a VST3: " + path);
    return false;
}
