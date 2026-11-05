#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include "AtemTypes.h"

namespace Atem {

class IAtemBackend : public QObject {
    Q_OBJECT
public:
    explicit IAtemBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~IAtemBackend() override = default;

    virtual QString backendName() const = 0;
    virtual ConnectionState connectionState() const = 0;
    virtual QString switcherName() const = 0;
    virtual QString ipAddress() const = 0;
    // True when a failed connection cannot succeed by retrying (the vendor SDK is missing on this computer)
    virtual bool permanentFailure() const { return false; }

    // Connection
    virtual void connectTo(const QString &ip) = 0;
    virtual void disconnectFromSwitcher() = 0;

    // Status fetch
    virtual void requestStatus() = 0;

    // ME / Program / Preview / Transitions
    virtual void setProgramInput(int me, int64_t inputId) = 0;
    virtual void setPreviewInput(int me, int64_t inputId) = 0;
    virtual void performCut(int me) = 0;
    virtual void performAutoTransition(int me) = 0;
    virtual void performFadeToBlack(int me) = 0;
    virtual void setTransitionPosition(int me, float position) = 0;
    virtual void setTransitionStyle(int me, TransitionStyle style) = 0;
    virtual void setTransitionRate(int me, TransitionStyle style, float rateFrames) = 0;
    virtual void setPreviewTransition(int me, bool preview) = 0;

    // USK (Upstream Keyers)
    virtual void setUskOnAir(int me, int keyer, bool onAir) = 0;
    virtual void setUskTie(int me, int keyer, bool tie) = 0;
    virtual void setUskType(int me, int keyer, KeyType type) = 0;
    virtual void setUskFillSource(int me, int keyer, int64_t inputId) = 0;
    virtual void setUskCutSource(int me, int keyer, int64_t inputId) = 0;
    virtual void setUskLumaParams(int me, int keyer, bool preMult, float clip, float gain, bool inverse) = 0;
    virtual void setUskChromaParams(int me, int keyer, float hue, float gain, float ySuppress, float lift, bool narrow) = 0;
    virtual void setUskAdvancedChromaSample(int me, int keyer, float x, float y) = 0;
    virtual void setUskFlyEnabled(int me, int keyer, bool enabled) = 0;
    virtual void setUskFlyPosition(int me, int keyer, float x, float y) = 0;
    virtual void setUskFlySize(int me, int keyer, float sizeX, float sizeY) = 0;
    virtual void setUskFlyRotation(int me, int keyer, float rotation) = 0;
    virtual void resetUskFly(int me, int keyer) = 0;
    virtual void setUskMask(int me, int keyer, bool enabled, float top, float bottom, float left, float right) = 0;
    virtual void resetUskMask(int me, int keyer) = 0;

    // DSK (Downstream Keyers)
    virtual void setDskOnAir(int keyer, bool onAir) = 0;
    virtual void setDskTie(int keyer, bool tie) = 0;
    virtual void performDskAutoTransition(int keyer) = 0;
    virtual void setDskRate(int keyer, int rateFrames) = 0;
    virtual void setDskFillSource(int keyer, int64_t inputId) = 0;
    virtual void setDskCutSource(int keyer, int64_t inputId) = 0;
    virtual void setDskLumaParams(int keyer, bool preMult, float clip, float gain, bool inverse) = 0;
    virtual void setDskMask(int keyer, bool enabled, float top, float bottom, float left, float right) = 0;
    virtual void resetDskMask(int keyer) = 0;

    // SuperSource
    virtual void setSuperSourceBoxEnabled(int box, bool enabled) = 0;
    virtual void setSuperSourceBoxSource(int box, int64_t inputId) = 0;
    virtual void setSuperSourceBoxPosition(int box, float x, float y) = 0;
    virtual void setSuperSourceBoxSize(int box, float size) = 0;
    virtual void setSuperSourceBoxCrop(int box, bool cropped, float top, float bottom, float left, float right) = 0;
    virtual void resetSuperSourceBoxCrop(int box) = 0;
    virtual void setSuperSourceBorder(bool enabled, float widthIn, float widthOut, float softnessIn, float softnessOut,
                                       float hue, float sat, float luma) = 0;

    // Audio & Fairlight Audio
    virtual void setAudioInputGain(int64_t inputId, float gainDb) = 0;
    virtual void setAudioInputPan(int64_t inputId, float pan) = 0;
    virtual void setAudioInputMixOption(int64_t inputId, AudioMixOption option) = 0;
    virtual void setAudioMasterGain(float gainDb) = 0;
    virtual void setAudioSolo(int64_t inputId, bool solo) = 0;
    virtual void setFairlightEqBand(int64_t inputId, int band, bool enabled, float freq, float gainDb, float q) = 0;
    virtual void resetFairlightEq(int64_t inputId) = 0;
    virtual void setFairlightCompressor(int64_t inputId, bool enabled, float thresholdDb, float ratio, float attackMs, float releaseMs) = 0;
    virtual void setFairlightLimiter(int64_t inputId, bool enabled, float thresholdDb, float attackMs, float releaseMs) = 0;
    virtual void setFairlightExpander(int64_t inputId, bool enabled, float thresholdDb, float rangeDb, float ratio) = 0;
    virtual void setAnalogInputPreamp(int64_t inputId, float gainDb, bool micLevel, bool phantom48V, bool lowCut) = 0;

    // MultiView
    virtual void setMultiViewLayout(int mvIndex, MultiViewLayout layout) = 0;
    virtual void setMultiViewWindowSource(int mvIndex, int window, int64_t inputId) = 0;
    virtual void setMultiViewSafeAreaVisible(int mvIndex, bool visible) = 0;
    virtual void setMultiViewMetersVisible(int mvIndex, bool visible) = 0;

    // Display Clock
    virtual void setDisplayClockEnabled(bool enabled) = 0;
    virtual void setDisplayClockMode(ClockMode mode) = 0;
    virtual void startDisplayClock() = 0;
    virtual void stopDisplayClock() = 0;
    virtual void resetDisplayClock() = 0;
    virtual void setDisplayClockTime(int hours, int minutes, int seconds) = 0;
    virtual void setDisplayClockLayout(float size, float x, float y, float opacity) = 0;

    // Recording & Streaming
    virtual void startRecording() = 0;
    virtual void stopRecording() = 0;
    virtual void switchRecordingDisk() = 0;
    virtual void setRecordingFilename(const QString &filename) = 0;
    virtual void setRecordAllInputsIso(bool recordIso) = 0;
    virtual void startStreaming() = 0;
    virtual void stopStreaming() = 0;
    virtual void setStreamingService(const QString &serviceName, const QString &url, const QString &key) = 0;

    // Macros
    virtual void runMacro(uint32_t index) = 0;
    virtual void stopMacro() = 0;
    virtual void setMacroLoop(bool loop) = 0;
    virtual uint32_t maxMacros() const = 0;

    // HyperDecks
    virtual void hyperDeckPlay(int deck) = 0;
    virtual void hyperDeckStop(int deck) = 0;
    virtual void hyperDeckRecord(int deck) = 0;
    virtual void hyperDeckShuttle(int deck, int speedPercent) = 0;
    virtual void hyperDeckJog(int deck, int frameCount) = 0;
    virtual void hyperDeckSelectClip(int deck, int clipIndex) = 0;
    virtual void hyperDeckSetClipTime(int deck, const QString &timecode) = 0;
    virtual void hyperDeckSetTimelineTime(int deck, const QString &timecode) = 0;
    virtual void hyperDeckSetLooped(int deck, bool loop) = 0;
    virtual void hyperDeckSetSingleClip(int deck, bool single) = 0;

    // Camera Control & PTZ
    virtual void setCameraIris(int camera, float value) = 0;
    virtual void setCameraFocus(int camera, float value) = 0;
    virtual void triggerCameraAutoFocus(int camera) = 0;
    virtual void setCameraGain(int camera, float db) = 0;
    virtual void setCameraShutter(int camera, float speed) = 0;
    virtual void setCameraWhiteBalance(int camera, float kelvin) = 0;
    virtual void setCameraPtz(int camera, float pan, float tilt, float zoom) = 0;
    virtual void recallCameraPtzPreset(int camera, int presetIndex) = 0;

    // Media Players & Stills
    virtual void selectMediaPlayerStill(int player, int stillIndex) = 0;
    virtual void selectMediaPlayerClip(int player, int clipIndex) = 0;
    virtual void captureStill(int64_t sourceInputId) = 0;

    // Aux & Converted Video Mode
    virtual void setAuxSource(int auxIndex, int64_t inputId) = 0;
    virtual void setConvertedVideoMode(int auxIndex, uint32_t videoMode) = 0;

    // Getters for current model state
    virtual const QMap<int64_t, InputInfo>& inputs() const = 0;
    virtual const QMap<int, MeState>& meBlocks() const = 0;
    virtual int meCount() const = 0;
    virtual int dskCount() const = 0;
    virtual int auxCount() const = 0;
    virtual int hyperdeckCount() const = 0;

signals:
    void connectionStateChanged(ConnectionState state);
    void switcherNameChanged(const QString &name);
    void inputListChanged();
    void inputChanged(const InputInfo &info);
    void meStateChanged(const MeState &state);
    void uskStateChanged(const KeyerState &state);
    void dskStateChanged(const DskState &state);
    void superSourceBoxChanged(const SuperSourceBoxState &state);
    void audioChannelChanged(const AudioChannelState &state);
    void audioMasterGainChanged(float gainDb);
    void fairlightEqBandChanged(const FairlightEqBandState &state);
    void fairlightDynamicsChanged(const FairlightDynamicsState &state);
    void multiViewChanged(const MultiViewState &state);
    void displayClockChanged(const DisplayClockState &state);
    void recordStateChanged(const RecordState &state);
    void streamStateChanged(const StreamState &state);
    void macroStateChanged(const MacroState &state);
    void cameraControlChanged(const CameraControlState &state);
    void auxSourceChanged(int auxIndex, int64_t inputId);
    void logMessage(const QString &message);
};

} // namespace Atem
