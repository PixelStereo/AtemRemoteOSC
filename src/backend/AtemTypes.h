#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <cstdint>

namespace Atem {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Failed
};

enum class TransitionStyle {
    Mix,
    Dip,
    Wipe,
    DVE,
    Stinger,
    Unknown
};

enum class KeyType {
    Luma,
    Chroma,
    Pattern,
    DVE,
    AdvancedChroma
};

enum class AudioMixOption {
    Off,
    On,
    AudioFollowVideo
};

enum class ClockMode {
    TimeOfDay,
    CountDown,
    CountUp
};

enum class MultiViewLayout {
    Default,
    Top,
    Bottom,
    Left,
    Right,
    Quadrant,
    OnePlusFive,
    TwoPlusSix,
    OnePlusSeven,
    TwoPlusEight,
    OnePlusNine
};

struct InputInfo {
    int64_t id = 0;
    QString shortName;
    QString longName;
    QString portType; // SDI, HDMI, Color, MediaPlayer, etc.
    bool isExternal = false;
    bool isProgram = false;
    bool isPreview = false;
};

struct MeState {
    int index = 1;
    int64_t programInput = 1;
    int64_t previewInput = 2;
    bool inTransition = false;
    float transitionPosition = 0.0f;
    uint32_t transitionFramesRemaining = 0;
    TransitionStyle transitionStyle = TransitionStyle::Mix;
    float transitionRate = 30.0f;
    bool previewTransition = false;
    bool ftbActive = false;
};

struct KeyerState {
    int meIndex = 1;
    int keyerIndex = 1;
    bool onAir = false;
    bool tie = false;
    KeyType type = KeyType::Luma;
    int64_t fillInput = 1;
    int64_t cutInput = 1;
    bool masked = false;
    float maskTop = 0.0f;
    float maskBottom = 0.0f;
    float maskLeft = 0.0f;
    float maskRight = 0.0f;
    // DVE Fly
    bool flyEnabled = false;
    float posX = 0.0f;
    float posY = 0.0f;
    float sizeX = 1.0f;
    float sizeY = 1.0f;
    float rotation = 0.0f;
    // Luma
    bool lumaPreMultiplied = false;
    float lumaClip = 0.0f;
    float lumaGain = 0.0f;
    bool lumaInverse = false;
    // Chroma
    float chromaHue = 0.0f;
    float chromaGain = 0.0f;
    float chromaYSuppress = 0.0f;
    float chromaLift = 0.0f;
    bool chromaNarrow = false;
};

struct DskState {
    int index = 1;
    bool onAir = false;
    bool tie = false;
    bool isAutoTransitioning = false;
    int rate = 30;
    int64_t fillInput = 0;
    int64_t cutInput = 0;
    float clip = 0.0f;
    float gain = 0.0f;
    bool preMultiplied = false;
    bool inverse = false;
    bool masked = false;
    float maskTop = 0.0f;
    float maskBottom = 0.0f;
    float maskLeft = 0.0f;
    float maskRight = 0.0f;
};

struct SuperSourceBoxState {
    int boxIndex = 1;
    bool enabled = false;
    int64_t inputSource = 0;
    float x = 0.0f;
    float y = 0.0f;
    float size = 0.5f;
    bool cropped = false;
    float cropTop = 0.0f;
    float cropBottom = 0.0f;
    float cropLeft = 0.0f;
    float cropRight = 0.0f;
};

struct AudioChannelState {
    int64_t channelId = 0;
    QString name;
    AudioMixOption mixOption = AudioMixOption::Off;
    float faderGainDb = 0.0f;
    float pan = 0.0f; // -1.0 to 1.0
    bool isFairlight = false;
    bool solo = false;
};

struct FairlightEqBandState {
    int64_t channelId = 0;
    int bandIndex = 0; // 0 to 5
    bool enabled = true;
    float frequency = 1000.0f; // Hz
    float gain = 0.0f; // dB
    float qFactor = 0.71f;
};

struct FairlightDynamicsState {
    int64_t channelId = 0;
    // Compressor
    bool compEnabled = false;
    float compThreshold = -20.0f; // dB
    float compRatio = 2.0f;
    float compAttack = 1.4f; // ms
    float compRelease = 100.0f; // ms
    // Limiter
    bool limEnabled = false;
    float limThreshold = -2.0f;
    // Expander / Gate
    bool expEnabled = false;
    float expThreshold = -40.0f;
    float expRange = 18.0f;
    float expRatio = 2.0f;
};

struct RecordState {
    bool isRecording = false;
    QString filename;
    uint32_t durationSeconds = 0;
    uint32_t remainingTimeSeconds = 0;
    int workingDiskIndex = 0;
    int totalDisks = 0;
    bool recordAllInputsIso = false;
};

struct StreamState {
    bool isStreaming = false;
    QString serviceName;
    uint32_t bitrate = 0;
    float cachePercentage = 0.0f;
    uint32_t durationSeconds = 0;
};

struct DisplayClockState {
    bool enabled = false;
    ClockMode mode = ClockMode::TimeOfDay;
    bool isRunning = false;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    float size = 1.0f;
    float x = 0.5f;
    float y = 0.5f;
    float opacity = 1.0f;
    bool autoHide = false;
};

struct MultiViewState {
    int multiViewIndex = 1;
    MultiViewLayout layout = MultiViewLayout::Default;
    QMap<int, int64_t> windowSources; // window 1..16 -> inputId
    bool safeAreaVisible = false;
    bool metersVisible = true;
};

struct MacroState {
    uint32_t index = 0;
    QString name;
    QString description;
    bool isValid = false;
    bool isRunning = false;
    bool isWaiting = false;
    bool loop = false;
};

struct CameraControlState {
    int cameraId = 1;
    float iris = 0.0f;
    float focus = 0.0f;
    float gain = 0.0f;
    float shutter = 0.0f;
    float whiteBalance = 5600.0f;
    // PTZ
    float pan = 0.0f;
    float tilt = 0.0f;
    float zoom = 0.0f;
};

} // namespace Atem
