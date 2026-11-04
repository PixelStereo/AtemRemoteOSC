#pragma once

#include "../IAtemBackend.h"

// Backend that controls nothing. It is used where the Blackmagic SDK does not exist (Linux), so that the
// OSC engine, the interface and the tests still build and run; connecting always fails with an explanation.
class NullAtemBackend : public Atem::IAtemBackend {
    Q_OBJECT
public:
    explicit NullAtemBackend(QObject *parent = nullptr) : Atem::IAtemBackend(parent) {}

    QString backendName() const override { return QStringLiteral("None (the Blackmagic SDK is not available on this platform)"); }
    Atem::ConnectionState connectionState() const override { return m_state; }
    QString switcherName() const override { return QString(); }
    QString ipAddress() const override { return m_ip; }
    bool permanentFailure() const override { return true; }

    void connectTo(const QString &ip) override {
        m_ip = ip;
        m_state = Atem::ConnectionState::Failed;
        emit logMessage(QStringLiteral("Cannot connect: the Blackmagic ATEM SDK is only available on macOS and Windows."));
        emit connectionStateChanged(m_state);
    }
    void disconnectFromSwitcher() override {
        if (m_state != Atem::ConnectionState::Disconnected) {
            m_state = Atem::ConnectionState::Disconnected;
            emit connectionStateChanged(m_state);
        }
    }
    void requestStatus() override {}

    void setSuperSourceBorder(bool enabled, float widthIn, float widthOut, float softnessIn, float softnessOut, float hue, float sat, float luma) override {}
    void setProgramInput(int me, int64_t inputId) override {}
    void setPreviewInput(int me, int64_t inputId) override {}
    void performCut(int me) override {}
    void performAutoTransition(int me) override {}
    void performFadeToBlack(int me) override {}
    void setTransitionPosition(int me, float position) override {}
    void setTransitionStyle(int me, Atem::TransitionStyle style) override {}
    void setTransitionRate(int me, Atem::TransitionStyle style, float rateFrames) override {}
    void setPreviewTransition(int me, bool preview) override {}
    void setUskOnAir(int me, int keyer, bool onAir) override {}
    void setUskTie(int me, int keyer, bool tie) override {}
    void setUskType(int me, int keyer, Atem::KeyType type) override {}
    void setUskFillSource(int me, int keyer, int64_t inputId) override {}
    void setUskCutSource(int me, int keyer, int64_t inputId) override {}
    void setUskLumaParams(int me, int keyer, bool preMult, float clip, float gain, bool inverse) override {}
    void setUskChromaParams(int me, int keyer, float hue, float gain, float ySuppress, float lift, bool narrow) override {}
    void setUskAdvancedChromaSample(int me, int keyer, float x, float y) override {}
    void setUskFlyEnabled(int me, int keyer, bool enabled) override {}
    void setUskFlyPosition(int me, int keyer, float x, float y) override {}
    void setUskFlySize(int me, int keyer, float sizeX, float sizeY) override {}
    void setUskFlyRotation(int me, int keyer, float rotation) override {}
    void resetUskFly(int me, int keyer) override {}
    void setUskMask(int me, int keyer, bool enabled, float top, float bottom, float left, float right) override {}
    void resetUskMask(int me, int keyer) override {}
    void setDskOnAir(int keyer, bool onAir) override {}
    void setDskTie(int keyer, bool tie) override {}
    void performDskAutoTransition(int keyer) override {}
    void setDskRate(int keyer, int rateFrames) override {}
    void setDskFillSource(int keyer, int64_t inputId) override {}
    void setDskCutSource(int keyer, int64_t inputId) override {}
    void setDskLumaParams(int keyer, bool preMult, float clip, float gain, bool inverse) override {}
    void setDskMask(int keyer, bool enabled, float top, float bottom, float left, float right) override {}
    void resetDskMask(int keyer) override {}
    void setSuperSourceBoxEnabled(int box, bool enabled) override {}
    void setSuperSourceBoxSource(int box, int64_t inputId) override {}
    void setSuperSourceBoxPosition(int box, float x, float y) override {}
    void setSuperSourceBoxSize(int box, float size) override {}
    void setSuperSourceBoxCrop(int box, bool cropped, float top, float bottom, float left, float right) override {}
    void resetSuperSourceBoxCrop(int box) override {}
    void setAudioInputGain(int64_t inputId, float gainDb) override {}
    void setAudioInputPan(int64_t inputId, float pan) override {}
    void setAudioInputMixOption(int64_t inputId, Atem::AudioMixOption option) override {}
    void setAudioMasterGain(float gainDb) override {}
    void setAudioSolo(int64_t inputId, bool solo) override {}
    void setFairlightEqBand(int64_t inputId, int band, bool enabled, float freq, float gainDb, float q) override {}
    void resetFairlightEq(int64_t inputId) override {}
    void setFairlightCompressor(int64_t inputId, bool enabled, float thresholdDb, float ratio, float attackMs, float releaseMs) override {}
    void setFairlightLimiter(int64_t inputId, bool enabled, float thresholdDb, float attackMs, float releaseMs) override {}
    void setFairlightExpander(int64_t inputId, bool enabled, float thresholdDb, float rangeDb, float ratio) override {}
    void setAnalogInputPreamp(int64_t inputId, float gainDb, bool micLevel, bool phantom48V, bool lowCut) override {}
    void setMultiViewLayout(int mvIndex, Atem::MultiViewLayout layout) override {}
    void setMultiViewWindowSource(int mvIndex, int window, int64_t inputId) override {}
    void setMultiViewSafeAreaVisible(int mvIndex, bool visible) override {}
    void setMultiViewMetersVisible(int mvIndex, bool visible) override {}
    void setDisplayClockEnabled(bool enabled) override {}
    void setDisplayClockMode(Atem::ClockMode mode) override {}
    void startDisplayClock() override {}
    void stopDisplayClock() override {}
    void resetDisplayClock() override {}
    void setDisplayClockTime(int hours, int minutes, int seconds) override {}
    void setDisplayClockLayout(float size, float x, float y, float opacity) override {}
    void startRecording() override {}
    void stopRecording() override {}
    void switchRecordingDisk() override {}
    void setRecordingFilename(const QString &filename) override {}
    void setRecordAllInputsIso(bool recordIso) override {}
    void startStreaming() override {}
    void stopStreaming() override {}
    void setStreamingService(const QString &serviceName, const QString &url, const QString &key) override {}
    void runMacro(uint32_t index) override {}
    void stopMacro() override {}
    void setMacroLoop(bool loop) override {}
    uint32_t maxMacros() const override { return 100; }
    void hyperDeckPlay(int deck) override {}
    void hyperDeckStop(int deck) override {}
    void hyperDeckRecord(int deck) override {}
    void hyperDeckShuttle(int deck, int speedPercent) override {}
    void hyperDeckJog(int deck, int frameCount) override {}
    void hyperDeckSelectClip(int deck, int clipIndex) override {}
    void hyperDeckSetClipTime(int deck, const QString &timecode) override {}
    void hyperDeckSetTimelineTime(int deck, const QString &timecode) override {}
    void hyperDeckSetLooped(int deck, bool loop) override {}
    void hyperDeckSetSingleClip(int deck, bool single) override {}
    void setCameraIris(int camera, float value) override {}
    void setCameraFocus(int camera, float value) override {}
    void triggerCameraAutoFocus(int camera) override {}
    void setCameraGain(int camera, float db) override {}
    void setCameraShutter(int camera, float speed) override {}
    void setCameraWhiteBalance(int camera, float kelvin) override {}
    void setCameraPtz(int camera, float pan, float tilt, float zoom) override {}
    void recallCameraPtzPreset(int camera, int presetIndex) override {}
    void selectMediaPlayerStill(int player, int stillIndex) override {}
    void selectMediaPlayerClip(int player, int clipIndex) override {}
    void captureStill(int64_t sourceInputId) override {}
    void setAuxSource(int auxIndex, int64_t inputId) override {}
    void setConvertedVideoMode(int auxIndex, uint32_t videoMode) override {}

    const QMap<int64_t, Atem::InputInfo>& inputs() const override { return m_inputs; }
    const QMap<int, Atem::MeState>& meBlocks() const override { return m_me; }
    int meCount() const override { return 0; }
    int dskCount() const override { return 0; }
    int auxCount() const override { return 0; }
    int hyperdeckCount() const override { return 0; }

private:
    Atem::ConnectionState m_state = Atem::ConnectionState::Disconnected;
    QString m_ip;
    QMap<int64_t, Atem::InputInfo> m_inputs;
    QMap<int, Atem::MeState> m_me;
};
