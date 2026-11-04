#pragma once

#include "../IAtemBackend.h"
#include "BmdInclude.h"
#include <vector>
#include <QHash>
#include <QPointer>
#include <QTimer>

#if defined(ATEM_HAVE_BMD_SDK)

class SwitcherMonitor;
class MixEffectBlockMonitor;
class DownstreamKeyMonitor;
class RecordMonitor;
class StreamMonitor;
class DisplayClockMonitor;
class MultiViewMonitor;

// Result of one connection attempt, computed off the GUI thread
struct BmdConnectAttempt {
    HRESULT hr = E_FAIL;
    BMDSwitcherConnectToFailure failure = bmdSwitcherConnectToFailureNoResponse;
    IBMDSwitcherDiscovery *discovery = nullptr;
    IBMDSwitcher *switcher = nullptr;
    bool legacy = false;     // the switcher object comes from the older library generation (10.2 - 10.4)
    bool sdkMissing = false; // no usable Blackmagic library on this computer
};

class BmdSwitcherBackend : public Atem::IAtemBackend {
    Q_OBJECT
public:
    explicit BmdSwitcherBackend(QObject *parent = nullptr);
    ~BmdSwitcherBackend() override;

    QString backendName() const override { return QStringLiteral("Blackmagic Official SDK 11.0"); }
    Atem::ConnectionState connectionState() const override { return m_connectionState; }
    QString switcherName() const override { return m_switcherName; }
    QString ipAddress() const override { return m_ip; }

    // Connection
    bool permanentFailure() const override { return m_sdkUnavailable; }
    void connectTo(const QString &ip) override;
    void disconnectFromSwitcher() override;

    // Status
    void requestStatus() override;

    // ME / Transitions
    void setProgramInput(int me, int64_t inputId) override;
    void setPreviewInput(int me, int64_t inputId) override;
    void performCut(int me) override;
    void performAutoTransition(int me) override;
    void performFadeToBlack(int me) override;
    void setTransitionPosition(int me, float position) override;
    void setTransitionStyle(int me, Atem::TransitionStyle style) override;
    void setTransitionRate(int me, Atem::TransitionStyle style, float rateFrames) override;
    void setPreviewTransition(int me, bool preview) override;

    // USK
    void setUskOnAir(int me, int keyer, bool onAir) override;
    void setUskTie(int me, int keyer, bool tie) override;
    void setUskType(int me, int keyer, Atem::KeyType type) override;
    void setUskFillSource(int me, int keyer, int64_t inputId) override;
    void setUskCutSource(int me, int keyer, int64_t inputId) override;
    void setUskLumaParams(int me, int keyer, bool preMult, float clip, float gain, bool inverse) override;
    void setUskChromaParams(int me, int keyer, float hue, float gain, float ySuppress, float lift, bool narrow) override;
    void setUskAdvancedChromaSample(int me, int keyer, float x, float y) override;
    void setUskFlyEnabled(int me, int keyer, bool enabled) override;
    void setUskFlyPosition(int me, int keyer, float x, float y) override;
    void setUskFlySize(int me, int keyer, float sizeX, float sizeY) override;
    void setUskFlyRotation(int me, int keyer, float rotation) override;
    void resetUskFly(int me, int keyer) override;
    void setUskMask(int me, int keyer, bool enabled, float top, float bottom, float left, float right) override;
    void resetUskMask(int me, int keyer) override;

    // DSK
    void setDskOnAir(int keyer, bool onAir) override;
    void setDskTie(int keyer, bool tie) override;
    void performDskAutoTransition(int keyer) override;
    void setDskRate(int keyer, int rateFrames) override;
    void setDskFillSource(int keyer, int64_t inputId) override;
    void setDskCutSource(int keyer, int64_t inputId) override;
    void setDskLumaParams(int keyer, bool preMult, float clip, float gain, bool inverse) override;
    void setDskMask(int keyer, bool enabled, float top, float bottom, float left, float right) override;
    void resetDskMask(int keyer) override;

    // SuperSource
    void setSuperSourceBoxEnabled(int box, bool enabled) override;
    void setSuperSourceBoxSource(int box, int64_t inputId) override;
    void setSuperSourceBoxPosition(int box, float x, float y) override;
    void setSuperSourceBoxSize(int box, float size) override;
    void setSuperSourceBoxCrop(int box, bool cropped, float top, float bottom, float left, float right) override;
    void resetSuperSourceBoxCrop(int box) override;
    void setSuperSourceBorder(bool enabled, float widthIn, float widthOut, float softnessIn, float softnessOut,
                              float hue, float sat, float luma) override;

    // Audio
    void setAudioInputGain(int64_t inputId, float gainDb) override;
    void setAudioInputPan(int64_t inputId, float pan) override;
    void setAudioInputMixOption(int64_t inputId, Atem::AudioMixOption option) override;
    void setAudioMasterGain(float gainDb) override;
    void setAudioSolo(int64_t inputId, bool solo) override;
    void setFairlightEqBand(int64_t inputId, int band, bool enabled, float freq, float gainDb, float q) override;
    void resetFairlightEq(int64_t inputId) override;
    void setFairlightCompressor(int64_t inputId, bool enabled, float thresholdDb, float ratio, float attackMs, float releaseMs) override;
    void setFairlightLimiter(int64_t inputId, bool enabled, float thresholdDb, float attackMs, float releaseMs) override;
    void setFairlightExpander(int64_t inputId, bool enabled, float thresholdDb, float rangeDb, float ratio) override;
    void setAnalogInputPreamp(int64_t inputId, float gainDb, bool micLevel, bool phantom48V, bool lowCut) override;

    // MultiView
    void setMultiViewLayout(int mvIndex, Atem::MultiViewLayout layout) override;
    void setMultiViewWindowSource(int mvIndex, int window, int64_t inputId) override;
    void setMultiViewSafeAreaVisible(int mvIndex, bool visible) override;
    void setMultiViewMetersVisible(int mvIndex, bool visible) override;

    // Display Clock
    void setDisplayClockEnabled(bool enabled) override;
    void setDisplayClockMode(Atem::ClockMode mode) override;
    void startDisplayClock() override;
    void stopDisplayClock() override;
    void resetDisplayClock() override;
    void setDisplayClockTime(int hours, int minutes, int seconds) override;
    void setDisplayClockLayout(float size, float x, float y, float opacity) override;

    // Recording & Streaming
    void startRecording() override;
    void stopRecording() override;
    void switchRecordingDisk() override;
    void setRecordingFilename(const QString &filename) override;
    void setRecordAllInputsIso(bool recordIso) override;
    void startStreaming() override;
    void stopStreaming() override;
    void setStreamingService(const QString &serviceName, const QString &url, const QString &key) override;

    // Macros
    void runMacro(uint32_t index) override;
    void stopMacro() override;
    void setMacroLoop(bool loop) override;
    uint32_t maxMacros() const override;

    // HyperDecks
    void hyperDeckPlay(int deck) override;
    void hyperDeckStop(int deck) override;
    void hyperDeckRecord(int deck) override;
    void hyperDeckShuttle(int deck, int speedPercent) override;
    void hyperDeckJog(int deck, int frameCount) override;
    void hyperDeckSelectClip(int deck, int clipIndex) override;
    void hyperDeckSetClipTime(int deck, const QString &timecode) override;
    void hyperDeckSetTimelineTime(int deck, const QString &timecode) override;
    void hyperDeckSetLooped(int deck, bool loop) override;
    void hyperDeckSetSingleClip(int deck, bool single) override;

    // Camera Control & PTZ
    void setCameraIris(int camera, float value) override;
    void setCameraFocus(int camera, float value) override;
    void triggerCameraAutoFocus(int camera) override;
    void setCameraGain(int camera, float db) override;
    void setCameraShutter(int camera, float speed) override;
    void setCameraWhiteBalance(int camera, float kelvin) override;
    void setCameraPtz(int camera, float pan, float tilt, float zoom) override;
    void recallCameraPtzPreset(int camera, int presetIndex) override;

    // Media Players
    void selectMediaPlayerStill(int player, int stillIndex) override;
    void selectMediaPlayerClip(int player, int clipIndex) override;
    void captureStill(int64_t sourceInputId) override;

    // Aux & Converted Video Mode
    void setAuxSource(int auxIndex, int64_t inputId) override;
    void setConvertedVideoMode(int auxIndex, uint32_t videoMode) override;

    // Getters
    const QMap<int64_t, Atem::InputInfo>& inputs() const override { return m_inputsMap; }
    const QMap<int, Atem::MeState>& meBlocks() const override { return m_meMap; }
    int meCount() const override { return static_cast<int>(m_meBlocks.size()); }
    int dskCount() const override { return static_cast<int>(m_dsks.size()); }
    int auxCount() const override { return static_cast<int>(m_auxInputs.size()); }
    int hyperdeckCount() const override { return static_cast<int>(m_hyperdecks.size()); }

    // Internal notification methods called by monitors
    void onSwitcherDisconnected();
    void onMeUpdated(int meIndex, BMDSwitcherMixEffectBlockEventType eventType);
    void onDskUpdated(int dskIndex, BMDSwitcherDownstreamKeyEventType eventType);
    void onRecordUpdated(BMDSwitcherRecordAVEventType eventType);
    void onStreamUpdated(BMDSwitcherStreamRTMPEventType eventType);
    void onClockUpdated(BMDSwitcherDisplayClockEventType eventType);
    void onMultiViewUpdated(int mvIndex, BMDSwitcherMultiViewEventType eventType);

private:
    void cleanup();
    void startConnectAttempt();
    void onConnectAttemptDone(int generation, BmdConnectAttempt result);
    void finishConnection();

    // With the older library generation some interfaces have a different layout: the calls that sit after the
    // methods added since then must go through the matching legacy interface (see Legacy/BMDSwitcherAPI_v*.h)
    HRESULT switcherCreateIterator(REFIID iid, LPVOID *ppv);
    HRESULT switcherAddCallback(IBMDSwitcherCallback *cb);
    HRESULT switcherRemoveCallback(IBMDSwitcherCallback *cb);
    HRESULT meCreateKeyIterator(IBMDSwitcherMixEffectBlock *me, LPVOID *ppv);
    HRESULT meAddCallback(IBMDSwitcherMixEffectBlock *me, IBMDSwitcherMixEffectBlockCallback *cb);
    HRESULT meRemoveCallback(IBMDSwitcherMixEffectBlock *me, IBMDSwitcherMixEffectBlockCallback *cb);
    HRESULT multiViewAddCallback(IBMDSwitcherMultiView *mv, IBMDSwitcherMultiViewCallback *cb);
    HRESULT multiViewRemoveCallback(IBMDSwitcherMultiView *mv, IBMDSwitcherMultiViewCallback *cb);
    QString sdkDiagnostic() const;
    void queryCapabilities();
    void pollState(bool force);

    Atem::ConnectionState m_connectionState = Atem::ConnectionState::Disconnected;
    QString m_switcherName;
    QString m_ip;

    IBMDSwitcherDiscovery *m_discovery = nullptr;
    IBMDSwitcher *m_switcher = nullptr;

    SwitcherMonitor *m_switcherMonitor = nullptr;
    std::vector<IBMDSwitcherMixEffectBlock*> m_meBlocks;
    std::vector<MixEffectBlockMonitor*> m_meMonitors;
    std::vector<IBMDSwitcherDownstreamKey*> m_dsks;
    std::vector<DownstreamKeyMonitor*> m_dskMonitors;
    std::vector<IBMDSwitcherInput*> m_auxInputs;
    std::vector<IBMDSwitcherHyperDeck*> m_hyperdecks;
    std::vector<IBMDSwitcherMultiView*> m_multiviews;
    std::vector<MultiViewMonitor*> m_mvMonitors;

    IBMDSwitcherFairlightAudioMixer *m_fairlightMixer = nullptr;
    IBMDSwitcherAudioMixer *m_classicMixer = nullptr;
    IBMDSwitcherRecordAV *m_record = nullptr;
    RecordMonitor *m_recordMonitor = nullptr;
    IBMDSwitcherStreamRTMP *m_stream = nullptr;
    StreamMonitor *m_streamMonitor = nullptr;
    IBMDSwitcherDisplayClock *m_clock = nullptr;
    DisplayClockMonitor *m_clockMonitor = nullptr;
    IBMDSwitcherCameraControl *m_cameraControl = nullptr;
    IBMDSwitcherInputSuperSource *m_superSource = nullptr;
    IBMDSwitcherSuperSourceBox *m_superSourceBoxes[4] = {nullptr};
    IBMDSwitcherMacroPool *m_macroPool = nullptr;
    IBMDSwitcherMacroControl *m_macroControl = nullptr;
    IBMDSwitcherMediaPool *m_mediaPool = nullptr;

    QMap<int64_t, Atem::InputInfo> m_inputsMap;
    QMap<int, Atem::MeState> m_meMap;

    // State is also polled (and diffed) so that every value reaches the OSC feedback quickly,
    // not only the ones the SDK sends callbacks for.
    QTimer *m_pollTimer = nullptr;
    int m_pollTick = 0;
    bool m_cameraFlushInit = false;
    bool m_sdkUnavailable = false;
    bool m_legacy = false;
    int m_generation = 0;        // bumped on every connect / disconnect: results of older attempts are dropped
    int m_attemptsRemaining = 0;
    QHash<QString, QVector<double>> m_snapshot;
};

#endif
