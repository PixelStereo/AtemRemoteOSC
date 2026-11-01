#include "BmdSwitcherBackend.h"
#include "BmdCallbackMonitors.h"
#include <QMetaObject>
#include <QDebug>

#if defined(__APPLE__) || defined(_WIN32)

static QString cfOrBstrToQString(
#if defined(__APPLE__)
    CFStringRef str
#elif defined(_WIN32)
    BSTR str
#endif
) {
    if (!str) return QString();
#if defined(__APPLE__)
    CFIndex length = CFStringGetLength(str);
    if (length == 0) return QString();
    QVector<QChar> buffer(length);
    CFStringGetCharacters(str, CFRangeMake(0, length), reinterpret_cast<UniChar*>(buffer.data()));
    return QString(buffer.data(), length);
#elif defined(_WIN32)
    return QString::fromWCharArray(str);
#endif
}

BmdSwitcherBackend::BmdSwitcherBackend(QObject *parent)
    : Atem::IAtemBackend(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(100);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() { pollState(false); });
}

BmdSwitcherBackend::~BmdSwitcherBackend() {
    cleanup();
}

void BmdSwitcherBackend::cleanup() {
    if (m_pollTimer) m_pollTimer->stop();
    m_snapshot.clear();
    m_cameraFlushInit = false;
    for (size_t i = 0; i < m_meMonitors.size(); ++i) {
        if (!m_meMonitors[i]) continue;
        m_meMonitors[i]->detach();
        if (i < m_meBlocks.size() && m_meBlocks[i]) m_meBlocks[i]->RemoveCallback(m_meMonitors[i]);
        m_meMonitors[i]->Release();
    }
    m_meMonitors.clear();

    for (auto *me : m_meBlocks) {
        if (me) me->Release();
    }
    m_meBlocks.clear();

    for (size_t i = 0; i < m_dskMonitors.size(); ++i) {
        if (!m_dskMonitors[i]) continue;
        m_dskMonitors[i]->detach();
        if (i < m_dsks.size() && m_dsks[i]) m_dsks[i]->RemoveCallback(m_dskMonitors[i]);
        m_dskMonitors[i]->Release();
    }
    m_dskMonitors.clear();

    for (auto *d : m_dsks) {
        if (d) d->Release();
    }
    m_dsks.clear();

    for (auto *a : m_auxInputs) {
        if (a) a->Release();
    }
    m_auxInputs.clear();

    for (auto *h : m_hyperdecks) {
        if (h) h->Release();
    }
    m_hyperdecks.clear();

    for (size_t i = 0; i < m_mvMonitors.size(); ++i) {
        if (!m_mvMonitors[i]) continue;
        m_mvMonitors[i]->detach();
        if (i < m_multiviews.size() && m_multiviews[i]) m_multiviews[i]->RemoveCallback(m_mvMonitors[i]);
        m_mvMonitors[i]->Release();
    }
    m_mvMonitors.clear();

    for (auto *mv : m_multiviews) {
        if (mv) mv->Release();
    }
    m_multiviews.clear();

    for (int i = 0; i < 4; ++i) {
        if (m_superSourceBoxes[i]) {
            m_superSourceBoxes[i]->Release();
            m_superSourceBoxes[i] = nullptr;
        }
    }

    if (m_superSource) { m_superSource->Release(); m_superSource = nullptr; }
    if (m_recordMonitor) {
        m_recordMonitor->detach();
        if (m_record) m_record->RemoveCallback(m_recordMonitor);
        m_recordMonitor->Release();
        m_recordMonitor = nullptr;
    }
    if (m_record) { m_record->Release(); m_record = nullptr; }
    if (m_streamMonitor) {
        m_streamMonitor->detach();
        if (m_stream) m_stream->RemoveCallback(m_streamMonitor);
        m_streamMonitor->Release();
        m_streamMonitor = nullptr;
    }
    if (m_stream) { m_stream->Release(); m_stream = nullptr; }
    if (m_clockMonitor) {
        m_clockMonitor->detach();
        if (m_clock) m_clock->RemoveCallback(m_clockMonitor);
        m_clockMonitor->Release();
        m_clockMonitor = nullptr;
    }
    if (m_clock) { m_clock->Release(); m_clock = nullptr; }
    if (m_cameraControl) { m_cameraControl->Release(); m_cameraControl = nullptr; }
    if (m_fairlightMixer) { m_fairlightMixer->Release(); m_fairlightMixer = nullptr; }
    if (m_classicMixer) { m_classicMixer->Release(); m_classicMixer = nullptr; }
    if (m_macroPool) { m_macroPool->Release(); m_macroPool = nullptr; }
    if (m_macroControl) { m_macroControl->Release(); m_macroControl = nullptr; }
    if (m_mediaPool) { m_mediaPool->Release(); m_mediaPool = nullptr; }

    if (m_switcherMonitor) {
        m_switcherMonitor->detach();
        if (m_switcher) m_switcher->RemoveCallback(m_switcherMonitor);
        m_switcherMonitor->Release();
        m_switcherMonitor = nullptr;
    }

    if (m_switcher) {
        m_switcher->Release();
        m_switcher = nullptr;
    }

    if (m_discovery) {
        m_discovery->Release();
        m_discovery = nullptr;
    }

    m_inputsMap.clear();
    m_meMap.clear();
}

void BmdSwitcherBackend::connectTo(const QString &ip) {
    disconnectFromSwitcher();
    m_ip = ip;
    m_connectionState = Atem::ConnectionState::Connecting;
    emit connectionStateChanged(m_connectionState);
    emit logMessage(QStringLiteral("Connecting to ATEM switcher at %1 (SDK 11.0)...").arg(ip));

#if defined(__APPLE__)
    m_discovery = CreateBMDSwitcherDiscoveryInstance();
    if (!m_discovery) {
        m_connectionState = Atem::ConnectionState::Failed;
        emit connectionStateChanged(m_connectionState);
        emit logMessage(QStringLiteral("Failed to create BMDSwitcherDiscovery instance."));
        return;
    }

    CFStringRef cfIp = ip.toCFString();
    BMDSwitcherConnectToFailure failureReason;
    HRESULT hr = m_discovery->ConnectTo(cfIp, &m_switcher, &failureReason);
    CFRelease(cfIp);
#elif defined(_WIN32)
    CoInitialize(NULL);
    HRESULT hr = CoCreateInstance(CLSID_CBMDSwitcherDiscovery, NULL, CLSCTX_ALL, IID_IBMDSwitcherDiscovery, (void**)&m_discovery);
    if (FAILED(hr) || !m_discovery) {
        m_connectionState = Atem::ConnectionState::Failed;
        emit connectionStateChanged(m_connectionState);
        emit logMessage(QStringLiteral("Failed to create BMDSwitcherDiscovery instance (COM CoCreateInstance)."));
        return;
    }

    BSTR bstrIp = SysAllocString((const OLECHAR*)ip.utf16());
    BMDSwitcherConnectToFailure failureReason;
    hr = m_discovery->ConnectTo(bstrIp, &m_switcher, &failureReason);
    SysFreeString(bstrIp);
#endif

    if (FAILED(hr) || !m_switcher) {
        m_connectionState = Atem::ConnectionState::Failed;
        emit connectionStateChanged(m_connectionState);
        QString reasonStr = QStringLiteral("Unknown error");
        switch (failureReason) {
            case bmdSwitcherConnectToFailureNoResponse: reasonStr = QStringLiteral("No response from switcher"); break;
            case bmdSwitcherConnectToFailureIncompatibleFirmware: reasonStr = QStringLiteral("Incompatible firmware version"); break;
            case bmdSwitcherConnectToFailureCorruptData: reasonStr = QStringLiteral("Corrupt data received"); break;
            case bmdSwitcherConnectToFailureStateSyncTimedOut: reasonStr = QStringLiteral("State synchronization timed out"); break;
            default: break;
        }
        emit logMessage(QStringLiteral("Connection failed: %1 (HRESULT: 0x%2)").arg(reasonStr).arg(hr, 8, 16, QChar('0')));
        cleanup();
        return;
    }

    m_switcherMonitor = new SwitcherMonitor(this);
    m_switcher->AddCallback(m_switcherMonitor);

#if defined(__APPLE__)
    CFStringRef cfName = NULL;
    if (SUCCEEDED(m_switcher->GetProductName(&cfName)) && cfName) {
        m_switcherName = cfOrBstrToQString(cfName);
        CFRelease(cfName);
    }
#elif defined(_WIN32)
    BSTR bstrName = NULL;
    if (SUCCEEDED(m_switcher->GetProductName(&bstrName)) && bstrName) {
        m_switcherName = cfOrBstrToQString(bstrName);
        SysFreeString(bstrName);
    }
#endif

    if (m_switcherName.isEmpty()) {
        m_switcherName = QStringLiteral("ATEM Switcher");
    }
    emit switcherNameChanged(m_switcherName);

    queryCapabilities();

    m_connectionState = Atem::ConnectionState::Connected;
    emit connectionStateChanged(m_connectionState);
    emit logMessage(QStringLiteral("Successfully connected to %1 (%2)").arg(m_switcherName, m_ip));

    requestStatus();
    m_pollTick = 0;
    m_pollTimer->start();
}

void BmdSwitcherBackend::disconnectFromSwitcher() {
    if (m_connectionState != Atem::ConnectionState::Disconnected) {
        cleanup();
        m_connectionState = Atem::ConnectionState::Disconnected;
        emit connectionStateChanged(m_connectionState);
        emit logMessage(QStringLiteral("Disconnected from switcher."));
    }
}

void BmdSwitcherBackend::queryCapabilities() {
    if (!m_switcher) return;

    // 1. ME blocks
    IBMDSwitcherMixEffectBlockIterator *meIt = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherMixEffectBlockIterator, (void**)&meIt)) && meIt) {
        IBMDSwitcherMixEffectBlock *me = nullptr;
        int meIdx = 1;
        while (meIt->Next(&me) == S_OK && me) {
            m_meBlocks.push_back(me);
            MixEffectBlockMonitor *mon = new MixEffectBlockMonitor(this, meIdx);
            me->AddCallback(mon);
            m_meMonitors.push_back(mon);
            meIdx++;
        }
        meIt->Release();
    }

    // 2. Inputs
    IBMDSwitcherInputIterator *inputIt = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherInputIterator, (void**)&inputIt)) && inputIt) {
        IBMDSwitcherInput *input = nullptr;
        while (inputIt->Next(&input) == S_OK && input) {
            BMDSwitcherInputId id;
            input->GetInputId(&id);

            Atem::InputInfo info;
            info.id = id;

#if defined(__APPLE__)
            CFStringRef cfShort = NULL, cfLong = NULL;
            if (SUCCEEDED(input->GetShortName(&cfShort)) && cfShort) {
                info.shortName = cfOrBstrToQString(cfShort);
                CFRelease(cfShort);
            }
            if (SUCCEEDED(input->GetLongName(&cfLong)) && cfLong) {
                info.longName = cfOrBstrToQString(cfLong);
                CFRelease(cfLong);
            }
#elif defined(_WIN32)
            BSTR bstrShort = NULL, bstrLong = NULL;
            if (SUCCEEDED(input->GetShortName(&bstrShort)) && bstrShort) {
                info.shortName = cfOrBstrToQString(bstrShort);
                SysFreeString(bstrShort);
            }
            if (SUCCEEDED(input->GetLongName(&bstrLong)) && bstrLong) {
                info.longName = cfOrBstrToQString(bstrLong);
                SysFreeString(bstrLong);
            }
#endif
            BMDSwitcherPortType portType;
            if (SUCCEEDED(input->GetPortType(&portType))) {
                info.isExternal = (portType == bmdSwitcherPortTypeExternal);
            }

            // Check if Aux input
            IBMDSwitcherInputAux *aux = nullptr;
            if (SUCCEEDED(input->QueryInterface(IID_IBMDSwitcherInputAux, (void**)&aux)) && aux) {
                input->AddRef();
                m_auxInputs.push_back(input);
                aux->Release();
            }

            m_inputsMap[id] = info;
            input->Release();
        }
        inputIt->Release();
        emit inputListChanged();
    }

    // 3. Downstream Keyers
    IBMDSwitcherDownstreamKeyIterator *dskIt = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherDownstreamKeyIterator, (void**)&dskIt)) && dskIt) {
        IBMDSwitcherDownstreamKey *dsk = nullptr;
        int dskIdx = 1;
        while (dskIt->Next(&dsk) == S_OK && dsk) {
            m_dsks.push_back(dsk);
            DownstreamKeyMonitor *mon = new DownstreamKeyMonitor(this, dskIdx);
            dsk->AddCallback(mon);
            m_dskMonitors.push_back(mon);
            dskIdx++;
        }
        dskIt->Release();
    }

    // 4. Fairlight & Classic Audio
    m_switcher->QueryInterface(IID_IBMDSwitcherFairlightAudioMixer, (void**)&m_fairlightMixer);
    m_switcher->QueryInterface(IID_IBMDSwitcherAudioMixer, (void**)&m_classicMixer);

    // 5. Record & Stream
    if (SUCCEEDED(m_switcher->QueryInterface(IID_IBMDSwitcherRecordAV, (void**)&m_record)) && m_record) {
        m_recordMonitor = new RecordMonitor(this);
        m_record->AddCallback(m_recordMonitor);
    }
    if (SUCCEEDED(m_switcher->QueryInterface(IID_IBMDSwitcherStreamRTMP, (void**)&m_stream)) && m_stream) {
        m_streamMonitor = new StreamMonitor(this);
        m_stream->AddCallback(m_streamMonitor);
    }

    // 6. Display Clock (SDK 11.0)
    if (SUCCEEDED(m_switcher->QueryInterface(IID_IBMDSwitcherDisplayClock, (void**)&m_clock)) && m_clock) {
        m_clockMonitor = new DisplayClockMonitor(this);
        m_clock->AddCallback(m_clockMonitor);
    }

    // 7. MultiViews
    IBMDSwitcherMultiViewIterator *mvIt = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherMultiViewIterator, (void**)&mvIt)) && mvIt) {
        IBMDSwitcherMultiView *mv = nullptr;
        int mvIdx = 1;
        while (mvIt->Next(&mv) == S_OK && mv) {
            m_multiviews.push_back(mv);
            MultiViewMonitor *mon = new MultiViewMonitor(this, mvIdx);
            mv->AddCallback(mon);
            m_mvMonitors.push_back(mon);
            mvIdx++;
        }
        mvIt->Release();
    }

    // 8. SuperSource
    if (SUCCEEDED(m_switcher->QueryInterface(IID_IBMDSwitcherInputSuperSource, (void**)&m_superSource)) && m_superSource) {
        IBMDSwitcherSuperSourceBoxIterator *boxIt = nullptr;
        if (SUCCEEDED(m_superSource->CreateIterator(IID_IBMDSwitcherSuperSourceBoxIterator, (void**)&boxIt)) && boxIt) {
            IBMDSwitcherSuperSourceBox *box = nullptr;
            int bIdx = 0;
            while (boxIt->Next(&box) == S_OK && box && bIdx < 4) {
                m_superSourceBoxes[bIdx++] = box;
            }
            boxIt->Release();
        }
    }

    // 9. Camera Control, Macros, MediaPool
    m_switcher->QueryInterface(IID_IBMDSwitcherCameraControl, (void**)&m_cameraControl);
    m_switcher->QueryInterface(IID_IBMDSwitcherMacroPool, (void**)&m_macroPool);
    m_switcher->QueryInterface(IID_IBMDSwitcherMacroControl, (void**)&m_macroControl);
    m_switcher->QueryInterface(IID_IBMDSwitcherMediaPool, (void**)&m_mediaPool);
}

void BmdSwitcherBackend::requestStatus() {
    if (!m_switcher) return;

    // Refresh M/E blocks
    for (size_t i = 0; i < m_meBlocks.size(); ++i) {
        auto *me = m_meBlocks[i];
        Atem::MeState state;
        state.index = static_cast<int>(i + 1);

        BMDSwitcherInputId pId, prvId;
        if (SUCCEEDED(me->GetProgramInput(&pId))) state.programInput = pId;
        if (SUCCEEDED(me->GetPreviewInput(&prvId))) state.previewInput = prvId;

        double pos;
        if (SUCCEEDED(me->GetTransitionPosition(&pos))) state.transitionPosition = static_cast<float>(pos);

        bool ftb;
        if (SUCCEEDED(me->GetFadeToBlackFullyBlack(&ftb))) state.ftbActive = ftb;

        m_meMap[state.index] = state;
        emit meStateChanged(state);
    }

    // Refresh Record status
    if (m_record) {
        Atem::RecordState rState;
        BMDSwitcherRecordAVState recordAvState;
        BMDSwitcherRecordAVError recordAvError;
        if (SUCCEEDED(m_record->GetStatus(&recordAvState, &recordAvError))) {
            rState.isRecording = (recordAvState == bmdSwitcherRecordAVStateRecording);
        }
        emit recordStateChanged(rState);
    }

    // Refresh Stream status
    if (m_stream) {
        Atem::StreamState sState;
        BMDSwitcherStreamRTMPState streamState;
        if (SUCCEEDED(m_stream->GetStatus(&streamState, NULL))) {
            sState.isStreaming = (streamState == bmdSwitcherStreamRTMPStateStreaming);
        }
        emit streamStateChanged(sState);
    }

    pollState(true);
}

namespace CamCtl {
constexpr uint32_t Lens = 0, Video = 1, Ptz = 11;
constexpr uint32_t LensFocus = 0, LensAutoFocus = 1, LensApertureNormalised = 3, LensContinuousZoom = 9;
constexpr uint32_t VideoManualWhiteBalance = 2, VideoShutterSpeed = 12, VideoGain = 13;
constexpr uint32_t PtzPanTiltVelocity = 0, PtzMemoryPreset = 1;
constexpr int MaxCameras = 8;
}

#if defined(__APPLE__)
using NativeString = CFStringRef;
static void freeNativeString(NativeString s) { if (s) CFRelease(s); }
#else
using NativeString = BSTR;
static void freeNativeString(NativeString s) { if (s) SysFreeString(s); }
#endif

// Reads the switcher state and emits what changed since the last call (everything when `force`).
void BmdSwitcherBackend::pollState(bool force) {
    if (!m_switcher || m_connectionState != Atem::ConnectionState::Connected) return;

    if (m_cameraControl && !m_cameraFlushInit) {
        m_cameraFlushInit = true;
        for (int cam = 1; cam <= CamCtl::MaxCameras; ++cam) {
            const uint32_t params[][2] = { { CamCtl::Lens, CamCtl::LensApertureNormalised }, { CamCtl::Lens, CamCtl::LensFocus },
                { CamCtl::Video, CamCtl::VideoGain }, { CamCtl::Video, CamCtl::VideoShutterSpeed },
                { CamCtl::Video, CamCtl::VideoManualWhiteBalance } };
            for (const auto &pr : params) m_cameraControl->SetParameterPeriodicFlushEnabled(cam, pr[0], pr[1], true);
        }
    }

    const bool slowTick = force || (m_pollTick++ % 20 == 0); // names, macros: every ~2 s
    auto changed = [this, force](const QString &key, const QVector<double> &values) {
        auto it = m_snapshot.find(key);
        if (!force && it != m_snapshot.end() && it.value() == values) return false;
        m_snapshot.insert(key, values);
        return true;
    };
    auto d = [](bool b) { return b ? 1.0 : 0.0; };

    // M/E transition settings
    for (size_t m = 0; m < m_meBlocks.size(); ++m) {
        const int meIdx = static_cast<int>(m) + 1;
        auto *me = m_meBlocks[m];
        Atem::MeState s = m_meMap.value(meIdx);
        s.index = meIdx;

        bool b = false; double pos = 0; BMDSwitcherInputId in = 0; uint32_t frames = 0;
        if (SUCCEEDED(me->GetProgramInput(&in))) s.programInput = in;
        if (SUCCEEDED(me->GetPreviewInput(&in))) s.previewInput = in;
        if (SUCCEEDED(me->GetTransitionPosition(&pos))) s.transitionPosition = static_cast<float>(pos);
        if (SUCCEEDED(me->GetFadeToBlackFullyBlack(&b))) s.ftbActive = b;
        if (SUCCEEDED(me->GetPreviewTransition(&b))) s.previewTransition = b;
        if (SUCCEEDED(me->GetInTransition(&b))) s.inTransition = b;

        IBMDSwitcherTransitionParameters *tp = nullptr;
        if (SUCCEEDED(me->QueryInterface(IID_IBMDSwitcherTransitionParameters, (void**)&tp)) && tp) {
            BMDSwitcherTransitionStyle style = bmdSwitcherTransitionStyleMix;
            if (SUCCEEDED(tp->GetNextTransitionStyle(&style))) {
                s.transitionStyle = style == bmdSwitcherTransitionStyleDip ? Atem::TransitionStyle::Dip
                                  : style == bmdSwitcherTransitionStyleWipe ? Atem::TransitionStyle::Wipe
                                  : style == bmdSwitcherTransitionStyleDVE ? Atem::TransitionStyle::DVE
                                  : style == bmdSwitcherTransitionStyleStinger ? Atem::TransitionStyle::Stinger
                                  : Atem::TransitionStyle::Mix;
            }
            tp->Release();
        }
        switch (s.transitionStyle) {
            case Atem::TransitionStyle::Dip: {
                IBMDSwitcherTransitionDipParameters *r = nullptr;
                if (SUCCEEDED(me->QueryInterface(IID_IBMDSwitcherTransitionDipParameters, (void**)&r)) && r) { if (SUCCEEDED(r->GetRate(&frames))) s.transitionRate = float(frames); r->Release(); }
                break;
            }
            case Atem::TransitionStyle::Wipe: {
                IBMDSwitcherTransitionWipeParameters *r = nullptr;
                if (SUCCEEDED(me->QueryInterface(IID_IBMDSwitcherTransitionWipeParameters, (void**)&r)) && r) { if (SUCCEEDED(r->GetRate(&frames))) s.transitionRate = float(frames); r->Release(); }
                break;
            }
            case Atem::TransitionStyle::DVE: {
                IBMDSwitcherTransitionDVEParameters *r = nullptr;
                if (SUCCEEDED(me->QueryInterface(IID_IBMDSwitcherTransitionDVEParameters, (void**)&r)) && r) { if (SUCCEEDED(r->GetRate(&frames))) s.transitionRate = float(frames); r->Release(); }
                break;
            }
            case Atem::TransitionStyle::Stinger:
                break;
            default: {
                IBMDSwitcherTransitionMixParameters *r = nullptr;
                if (SUCCEEDED(me->QueryInterface(IID_IBMDSwitcherTransitionMixParameters, (void**)&r)) && r) { if (SUCCEEDED(r->GetRate(&frames))) s.transitionRate = float(frames); r->Release(); }
                break;
            }
        }

        const QVector<double> snap = { double(s.programInput), double(s.previewInput), s.transitionPosition, d(s.ftbActive),
            d(s.previewTransition), d(s.inTransition), double(int(s.transitionStyle)), s.transitionRate };
        if (changed(QStringLiteral("me/%1").arg(meIdx), snap)) {
            m_meMap[meIdx] = s;
            emit meStateChanged(s);
        }
    }

    // Display clock
    if (m_clock) {
        Atem::DisplayClockState s;
        bool b = false; uint16_t u = 0; double v = 0; uint8_t h = 0, mi = 0, se = 0, fr = 0;
        BMDSwitcherDisplayClockMode mode = bmdSwitcherDisplayClockModeTimeOfDay;
        BMDSwitcherDisplayClockState cs = bmdSwitcherDisplayClockStateStopped;
        if (SUCCEEDED(m_clock->GetEnabled(&b))) s.enabled = b;
        if (SUCCEEDED(m_clock->GetClockMode(&mode))) {
            s.mode = mode == bmdSwitcherDisplayClockModeCountdown ? Atem::ClockMode::CountDown
                   : mode == bmdSwitcherDisplayClockModeCountup ? Atem::ClockMode::CountUp : Atem::ClockMode::TimeOfDay;
        }
        if (SUCCEEDED(m_clock->GetClockState(&cs))) s.isRunning = (cs == bmdSwitcherDisplayClockStateRunning);
        if (SUCCEEDED(m_clock->GetClockTime(&h, &mi, &se, &fr))) { s.hours = h; s.minutes = mi; s.seconds = se; s.frames = fr; }
        if (SUCCEEDED(m_clock->GetSize(&u))) s.size = u / 100.0f;
        if (SUCCEEDED(m_clock->GetOpacity(&u))) s.opacity = u / 100.0f;
        if (SUCCEEDED(m_clock->GetPositionX(&v))) s.x = static_cast<float>(v);
        if (SUCCEEDED(m_clock->GetPositionY(&v))) s.y = static_cast<float>(v);
        if (SUCCEEDED(m_clock->GetAutoHide(&b))) s.autoHide = b;
        const QVector<double> snap = { d(s.enabled), double(int(s.mode)), d(s.isRunning), double(s.hours), double(s.minutes),
                                       double(s.seconds), double(s.frames), s.size, s.opacity, s.x, s.y, d(s.autoHide) };
        if (changed(QStringLiteral("clock"), snap)) emit displayClockChanged(s);
    }

    // Recording
    if (m_record) {
        Atem::RecordState s;
        BMDSwitcherRecordAVState st; BMDSwitcherRecordAVError err; bool b = false; uint32_t u = 0;
        if (SUCCEEDED(m_record->GetStatus(&st, &err))) s.isRecording = (st == bmdSwitcherRecordAVStateRecording);
        if (SUCCEEDED(m_record->GetRecordAllISOInputs(&b))) s.recordAllInputsIso = b;
        if (SUCCEEDED(m_record->GetTotalRecordingTimeAvailable(&u))) s.remainingTimeSeconds = u;
        if (SUCCEEDED(m_record->GetActiveDiskIndex(&u))) s.workingDiskIndex = static_cast<int>(u);
        NativeString fn = nullptr;
        if (SUCCEEDED(m_record->GetFilename(&fn)) && fn) s.filename = cfOrBstrToQString(fn);
        freeNativeString(fn);
        const QVector<double> snap = { d(s.isRecording), d(s.recordAllInputsIso), double(s.remainingTimeSeconds),
                                       double(s.workingDiskIndex), double(qHash(s.filename)) };
        if (changed(QStringLiteral("record"), snap)) emit recordStateChanged(s);
    }

    // Streaming
    if (m_stream) {
        Atem::StreamState s;
        BMDSwitcherStreamRTMPState st; uint32_t u = 0; double v = 0;
        if (SUCCEEDED(m_stream->GetStatus(&st, NULL))) s.isStreaming = (st == bmdSwitcherStreamRTMPStateStreaming);
        if (SUCCEEDED(m_stream->GetEncodingBitrate(&u))) s.bitrate = u;
        if (SUCCEEDED(m_stream->GetCacheUsed(&v))) s.cachePercentage = static_cast<float>(v);
        NativeString sn = nullptr;
        if (SUCCEEDED(m_stream->GetServiceName(&sn)) && sn) s.serviceName = cfOrBstrToQString(sn);
        freeNativeString(sn);
        const QVector<double> snap = { d(s.isStreaming), double(s.bitrate), s.cachePercentage, double(qHash(s.serviceName)) };
        if (changed(QStringLiteral("stream"), snap)) emit streamStateChanged(s);
    }

    // Camera control
    if (m_cameraControl) {
        for (int cam = 1; cam <= CamCtl::MaxCameras; ++cam) {
            Atem::CameraControlState s;
            s.cameraId = cam;
            bool any = false;
            uint32_t n = 1;
            double f = 0;
            int8_t i8 = 0;
            int32_t i32 = 0;
            int16_t wb[2] = { 0, 0 };

            n = 1;
            if (SUCCEEDED(m_cameraControl->GetFloats(cam, CamCtl::Lens, CamCtl::LensApertureNormalised, &n, &f))) { s.iris = static_cast<float>(f); any = true; }
            n = 1;
            if (SUCCEEDED(m_cameraControl->GetFloats(cam, CamCtl::Lens, CamCtl::LensFocus, &n, &f))) { s.focus = static_cast<float>(f); any = true; }
            n = 1;
            if (SUCCEEDED(m_cameraControl->GetInt8s(cam, CamCtl::Video, CamCtl::VideoGain, &n, &i8))) { s.gain = i8; any = true; }
            n = 1;
            if (SUCCEEDED(m_cameraControl->GetInt32s(cam, CamCtl::Video, CamCtl::VideoShutterSpeed, &n, &i32))) { s.shutter = static_cast<float>(i32); any = true; }
            n = 2;
            if (SUCCEEDED(m_cameraControl->GetInt16s(cam, CamCtl::Video, CamCtl::VideoManualWhiteBalance, &n, wb))) { s.whiteBalance = wb[0]; any = true; }

            if (!any) continue;
            const QVector<double> snap = { s.iris, s.focus, s.gain, s.shutter, s.whiteBalance };
            if (changed(QStringLiteral("camera/%1").arg(cam), snap)) emit cameraControlChanged(s);
        }
    }

    // Upstream keyers
    for (size_t m = 0; m < m_meBlocks.size(); ++m) {
        const int meIdx = static_cast<int>(m) + 1;

        uint32_t nextSelection = 0;
        IBMDSwitcherTransitionParameters *tp = nullptr;
        if (SUCCEEDED(m_meBlocks[m]->QueryInterface(IID_IBMDSwitcherTransitionParameters, (void**)&tp)) && tp) {
            BMDSwitcherTransitionSelection sel = bmdSwitcherTransitionSelectionBackground;
            if (SUCCEEDED(tp->GetNextTransitionSelection(&sel))) nextSelection = static_cast<uint32_t>(sel);
            tp->Release();
        }

        IBMDSwitcherKeyIterator *kIt = nullptr;
        if (FAILED(m_meBlocks[m]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) || !kIt) continue;
        IBMDSwitcherKey *key = nullptr;
        int keyIdx = 1;
        while (kIt->Next(&key) == S_OK && key) {
            Atem::KeyerState s;
            s.meIndex = meIdx;
            s.keyerIndex = keyIdx;

            bool b = false; double v = 0; BMDSwitcherInputId in = 0; BMDSwitcherKeyType type = bmdSwitcherKeyTypeLuma;
            if (SUCCEEDED(key->GetOnAir(&b))) s.onAir = b;
            s.tie = (nextSelection & (1u << keyIdx)) != 0;
            if (SUCCEEDED(key->GetType(&type))) {
                s.type = type == bmdSwitcherKeyTypeChroma ? Atem::KeyType::Chroma
                       : type == bmdSwitcherKeyTypePattern ? Atem::KeyType::Pattern
                       : type == bmdSwitcherKeyTypeDVE ? Atem::KeyType::DVE : Atem::KeyType::Luma;
            }
            if (SUCCEEDED(key->GetInputFill(&in))) s.fillInput = in;
            if (SUCCEEDED(key->GetInputCut(&in))) s.cutInput = in;
            if (SUCCEEDED(key->GetMasked(&b))) s.masked = b;
            if (SUCCEEDED(key->GetMaskTop(&v))) s.maskTop = static_cast<float>(v);
            if (SUCCEEDED(key->GetMaskBottom(&v))) s.maskBottom = static_cast<float>(v);
            if (SUCCEEDED(key->GetMaskLeft(&v))) s.maskLeft = static_cast<float>(v);
            if (SUCCEEDED(key->GetMaskRight(&v))) s.maskRight = static_cast<float>(v);

            IBMDSwitcherKeyLumaParameters *luma = nullptr;
            if (SUCCEEDED(key->QueryInterface(IID_IBMDSwitcherKeyLumaParameters, (void**)&luma)) && luma) {
                if (SUCCEEDED(luma->GetPreMultiplied(&b))) s.lumaPreMultiplied = b;
                if (SUCCEEDED(luma->GetClip(&v))) s.lumaClip = static_cast<float>(v);
                if (SUCCEEDED(luma->GetGain(&v))) s.lumaGain = static_cast<float>(v);
                if (SUCCEEDED(luma->GetInverse(&b))) s.lumaInverse = b;
                luma->Release();
            }
            IBMDSwitcherKeyChromaParameters *chroma = nullptr;
            if (SUCCEEDED(key->QueryInterface(IID_IBMDSwitcherKeyChromaParameters, (void**)&chroma)) && chroma) {
                if (SUCCEEDED(chroma->GetHue(&v))) s.chromaHue = static_cast<float>(v);
                if (SUCCEEDED(chroma->GetGain(&v))) s.chromaGain = static_cast<float>(v);
                if (SUCCEEDED(chroma->GetYSuppress(&v))) s.chromaYSuppress = static_cast<float>(v);
                if (SUCCEEDED(chroma->GetLift(&v))) s.chromaLift = static_cast<float>(v);
                if (SUCCEEDED(chroma->GetNarrow(&b))) s.chromaNarrow = b;
                chroma->Release();
            }
            IBMDSwitcherKeyFlyParameters *fly = nullptr;
            if (SUCCEEDED(key->QueryInterface(IID_IBMDSwitcherKeyFlyParameters, (void**)&fly)) && fly) {
                if (SUCCEEDED(fly->GetFly(&b))) s.flyEnabled = b;
                if (SUCCEEDED(fly->GetPositionX(&v))) s.posX = static_cast<float>(v);
                if (SUCCEEDED(fly->GetPositionY(&v))) s.posY = static_cast<float>(v);
                if (SUCCEEDED(fly->GetSizeX(&v))) s.sizeX = static_cast<float>(v);
                if (SUCCEEDED(fly->GetSizeY(&v))) s.sizeY = static_cast<float>(v);
                if (SUCCEEDED(fly->GetRotation(&v))) s.rotation = static_cast<float>(v);
                fly->Release();
            }

            const QVector<double> snap = { d(s.onAir), d(s.tie), double(int(s.type)), double(s.fillInput), double(s.cutInput),
                d(s.masked), s.maskTop, s.maskBottom, s.maskLeft, s.maskRight, d(s.lumaPreMultiplied), s.lumaClip, s.lumaGain,
                d(s.lumaInverse), s.chromaHue, s.chromaGain, s.chromaYSuppress, s.chromaLift, d(s.chromaNarrow),
                d(s.flyEnabled), s.posX, s.posY, s.sizeX, s.sizeY, s.rotation };
            if (changed(QStringLiteral("usk/%1/%2").arg(meIdx).arg(keyIdx), snap)) emit uskStateChanged(s);

            key->Release();
            ++keyIdx;
        }
        kIt->Release();
    }

    // Downstream keyers
    for (size_t i = 0; i < m_dsks.size(); ++i) {
        auto *dsk = m_dsks[i];
        Atem::DskState s;
        s.index = static_cast<int>(i) + 1;
        bool b = false; double v = 0; uint32_t u = 0; BMDSwitcherInputId in = 0;
        if (SUCCEEDED(dsk->GetOnAir(&b))) s.onAir = b;
        if (SUCCEEDED(dsk->GetTie(&b))) s.tie = b;
        if (SUCCEEDED(dsk->IsAutoTransitioning(&b))) s.isAutoTransitioning = b;
        if (SUCCEEDED(dsk->GetRate(&u))) s.rate = static_cast<int>(u);
        if (SUCCEEDED(dsk->GetInputFill(&in))) s.fillInput = in;
        if (SUCCEEDED(dsk->GetInputCut(&in))) s.cutInput = in;
        if (SUCCEEDED(dsk->GetClip(&v))) s.clip = static_cast<float>(v);
        if (SUCCEEDED(dsk->GetGain(&v))) s.gain = static_cast<float>(v);
        if (SUCCEEDED(dsk->GetPreMultiplied(&b))) s.preMultiplied = b;
        if (SUCCEEDED(dsk->GetInverse(&b))) s.inverse = b;
        if (SUCCEEDED(dsk->GetMasked(&b))) s.masked = b;
        if (SUCCEEDED(dsk->GetMaskTop(&v))) s.maskTop = static_cast<float>(v);
        if (SUCCEEDED(dsk->GetMaskBottom(&v))) s.maskBottom = static_cast<float>(v);
        if (SUCCEEDED(dsk->GetMaskLeft(&v))) s.maskLeft = static_cast<float>(v);
        if (SUCCEEDED(dsk->GetMaskRight(&v))) s.maskRight = static_cast<float>(v);
        const QVector<double> snap = { d(s.onAir), d(s.tie), d(s.isAutoTransitioning), double(s.rate), double(s.fillInput),
            double(s.cutInput), s.clip, s.gain, d(s.preMultiplied), d(s.inverse), d(s.masked), s.maskTop, s.maskBottom,
            s.maskLeft, s.maskRight };
        if (changed(QStringLiteral("dsk/%1").arg(s.index), snap)) emit dskStateChanged(s);
    }

    // Aux outputs
    for (size_t i = 0; i < m_auxInputs.size(); ++i) {
        IBMDSwitcherInputAux *aux = nullptr;
        if (SUCCEEDED(m_auxInputs[i]->QueryInterface(IID_IBMDSwitcherInputAux, (void**)&aux)) && aux) {
            BMDSwitcherInputId src = 0;
            if (SUCCEEDED(aux->GetInputSource(&src)) && changed(QStringLiteral("aux/%1").arg(i + 1), { double(src) })) {
                emit auxSourceChanged(static_cast<int>(i) + 1, src);
            }
            aux->Release();
        }
    }

    // SuperSource boxes
    for (int i = 0; i < 4; ++i) {
        auto *box = m_superSourceBoxes[i];
        if (!box) continue;
        Atem::SuperSourceBoxState s;
        s.boxIndex = i + 1;
        bool b = false; double v = 0; BMDSwitcherInputId in = 0;
        if (SUCCEEDED(box->GetEnabled(&b))) s.enabled = b;
        if (SUCCEEDED(box->GetInputSource(&in))) s.inputSource = in;
        if (SUCCEEDED(box->GetPositionX(&v))) s.x = static_cast<float>(v);
        if (SUCCEEDED(box->GetPositionY(&v))) s.y = static_cast<float>(v);
        if (SUCCEEDED(box->GetSize(&v))) s.size = static_cast<float>(v);
        if (SUCCEEDED(box->GetCropped(&b))) s.cropped = b;
        if (SUCCEEDED(box->GetCropTop(&v))) s.cropTop = static_cast<float>(v);
        if (SUCCEEDED(box->GetCropBottom(&v))) s.cropBottom = static_cast<float>(v);
        if (SUCCEEDED(box->GetCropLeft(&v))) s.cropLeft = static_cast<float>(v);
        if (SUCCEEDED(box->GetCropRight(&v))) s.cropRight = static_cast<float>(v);
        const QVector<double> snap = { d(s.enabled), double(s.inputSource), s.x, s.y, s.size, d(s.cropped),
                                       s.cropTop, s.cropBottom, s.cropLeft, s.cropRight };
        if (changed(QStringLiteral("ssrc/%1").arg(s.boxIndex), snap)) emit superSourceBoxChanged(s);
    }

    // Classic audio mixer
    if (m_classicMixer) {
        double master = 0;
        if (SUCCEEDED(m_classicMixer->GetProgramOutGain(&master)) && changed(QStringLiteral("audio/master"), { master })) {
            emit audioMasterGainChanged(static_cast<float>(master));
        }
        IBMDSwitcherAudioInputIterator *aIt = nullptr;
        if (SUCCEEDED(m_classicMixer->CreateIterator(IID_IBMDSwitcherAudioInputIterator, (void**)&aIt)) && aIt) {
            IBMDSwitcherAudioInput *ain = nullptr;
            while (aIt->Next(&ain) == S_OK && ain) {
                Atem::AudioChannelState s;
                BMDSwitcherAudioInputId id = 0; double v = 0; BMDSwitcherAudioMixOption mix = bmdSwitcherAudioMixOptionOff;
                ain->GetAudioInputId(&id);
                s.channelId = id;
                if (SUCCEEDED(ain->GetGain(&v))) s.faderGainDb = static_cast<float>(v);
                if (SUCCEEDED(ain->GetBalance(&v))) s.pan = static_cast<float>(v);
                if (SUCCEEDED(ain->GetMixOption(&mix))) {
                    s.mixOption = mix == bmdSwitcherAudioMixOptionOn ? Atem::AudioMixOption::On
                                : mix == bmdSwitcherAudioMixOptionAudioFollowVideo ? Atem::AudioMixOption::AudioFollowVideo
                                : Atem::AudioMixOption::Off;
                }
                const QVector<double> snap = { s.faderGainDb, s.pan, double(int(s.mixOption)) };
                if (changed(QStringLiteral("audio/%1").arg(s.channelId), snap)) emit audioChannelChanged(s);
                ain->Release();
            }
            aIt->Release();
        }
    }

    // MultiViews
    for (size_t i = 0; i < m_multiviews.size(); ++i) {
        auto *mv = m_multiviews[i];
        Atem::MultiViewState s;
        s.multiViewIndex = static_cast<int>(i) + 1;
        BMDSwitcherMultiViewLayout layout = bmdSwitcherMultiViewLayoutProgramBottom;
        if (SUCCEEDED(mv->GetLayout(&layout))) {
            s.layout = layout == bmdSwitcherMultiViewLayoutProgramTop ? Atem::MultiViewLayout::Top
                     : layout == bmdSwitcherMultiViewLayoutProgramLeft ? Atem::MultiViewLayout::Left
                     : layout == bmdSwitcherMultiViewLayoutProgramRight ? Atem::MultiViewLayout::Right
                     : Atem::MultiViewLayout::Bottom;
        }
        QVector<double> snap = { double(int(s.layout)) };
        uint32_t windows = 0;
        mv->GetWindowCount(&windows);
        for (uint32_t w = 0; w < windows; ++w) {
            BMDSwitcherInputId in = 0;
            if (SUCCEEDED(mv->GetWindowInput(w, &in))) {
                s.windowSources[static_cast<int>(w) + 1] = in;
                snap.append(double(in));
            }
        }
        bool b = false;
        if (SUCCEEDED(mv->GetSafeAreaEnabled(0, &b))) s.safeAreaVisible = b;
        snap.append(b ? 1.0 : 0.0);
        b = false;
        if (SUCCEEDED(mv->GetVuMeterEnabled(0, &b))) s.metersVisible = b;
        snap.append(b ? 1.0 : 0.0);
        if (changed(QStringLiteral("mv/%1").arg(s.multiViewIndex), snap)) emit multiViewChanged(s);
    }

    if (!slowTick) return;

    // Input names
    IBMDSwitcherInputIterator *inputIt = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherInputIterator, (void**)&inputIt)) && inputIt) {
        IBMDSwitcherInput *input = nullptr;
        while (inputIt->Next(&input) == S_OK && input) {
            BMDSwitcherInputId id = 0;
            input->GetInputId(&id);
            Atem::InputInfo info = m_inputsMap.value(id);
            info.id = id;
            NativeString sh = nullptr, lg = nullptr;
            if (SUCCEEDED(input->GetShortName(&sh)) && sh) info.shortName = cfOrBstrToQString(sh);
            if (SUCCEEDED(input->GetLongName(&lg)) && lg) info.longName = cfOrBstrToQString(lg);
            freeNativeString(sh);
            freeNativeString(lg);
            m_inputsMap[id] = info;
            if (changed(QStringLiteral("input/%1").arg(id), { double(qHash(info.shortName)), double(qHash(info.longName)) })) {
                emit inputChanged(info);
            }
            input->Release();
        }
        inputIt->Release();
    }

    // Macros
    if (m_macroPool) {
        uint32_t maxCount = 0;
        m_macroPool->GetMaxCount(&maxCount);
        for (uint32_t i = 0; i < maxCount; ++i) {
            Atem::MacroState s;
            s.index = i;
            bool valid = false;
            if (SUCCEEDED(m_macroPool->IsValid(i, &valid))) s.isValid = valid;
            if (valid) {
                NativeString name = nullptr, desc = nullptr;
                if (SUCCEEDED(m_macroPool->GetName(i, &name)) && name) s.name = cfOrBstrToQString(name);
                if (SUCCEEDED(m_macroPool->GetDescription(i, &desc)) && desc) s.description = cfOrBstrToQString(desc);
                freeNativeString(name);
                freeNativeString(desc);
            }
            if (changed(QStringLiteral("macro/%1").arg(i), { d(s.isValid), double(qHash(s.name)), double(qHash(s.description)) })) {
                emit macroStateChanged(s);
            }
        }
    }
}

// ME operations
void BmdSwitcherBackend::setProgramInput(int me, int64_t inputId) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->SetProgramInput(inputId);
    }
}

void BmdSwitcherBackend::setPreviewInput(int me, int64_t inputId) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->SetPreviewInput(inputId);
    }
}

void BmdSwitcherBackend::performCut(int me) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->PerformCut();
    }
}

void BmdSwitcherBackend::performAutoTransition(int me) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->PerformAutoTransition();
    }
}

void BmdSwitcherBackend::performFadeToBlack(int me) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->PerformFadeToBlack();
    }
}

void BmdSwitcherBackend::setTransitionPosition(int me, float position) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->SetTransitionPosition(position);
    }
}

void BmdSwitcherBackend::setTransitionStyle(int me, Atem::TransitionStyle style) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        IBMDSwitcherTransitionParameters *params = nullptr;
        if (SUCCEEDED(m_meBlocks[me - 1]->QueryInterface(IID_IBMDSwitcherTransitionParameters, (void**)&params)) && params) {
            BMDSwitcherTransitionStyle bmdStyle = bmdSwitcherTransitionStyleMix;
            switch (style) {
                case Atem::TransitionStyle::Dip: bmdStyle = bmdSwitcherTransitionStyleDip; break;
                case Atem::TransitionStyle::Wipe: bmdStyle = bmdSwitcherTransitionStyleWipe; break;
                case Atem::TransitionStyle::DVE: bmdStyle = bmdSwitcherTransitionStyleDVE; break;
                case Atem::TransitionStyle::Stinger: bmdStyle = bmdSwitcherTransitionStyleStinger; break;
                default: break;
            }
            params->SetNextTransitionStyle(bmdStyle);
            params->Release();
        }
    }
}

void BmdSwitcherBackend::setTransitionRate(int me, Atem::TransitionStyle style, float rateFrames) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    auto *meBlock = m_meBlocks[me - 1];

    if (style == Atem::TransitionStyle::Mix) {
        IBMDSwitcherTransitionMixParameters *mix = nullptr;
        if (SUCCEEDED(meBlock->QueryInterface(IID_IBMDSwitcherTransitionMixParameters, (void**)&mix)) && mix) {
            mix->SetRate(static_cast<uint32_t>(rateFrames));
            mix->Release();
        }
    } else if (style == Atem::TransitionStyle::Dip) {
        IBMDSwitcherTransitionDipParameters *dip = nullptr;
        if (SUCCEEDED(meBlock->QueryInterface(IID_IBMDSwitcherTransitionDipParameters, (void**)&dip)) && dip) {
            dip->SetRate(static_cast<uint32_t>(rateFrames));
            dip->Release();
        }
    } else if (style == Atem::TransitionStyle::Wipe) {
        IBMDSwitcherTransitionWipeParameters *wipe = nullptr;
        if (SUCCEEDED(meBlock->QueryInterface(IID_IBMDSwitcherTransitionWipeParameters, (void**)&wipe)) && wipe) {
            wipe->SetRate(static_cast<uint32_t>(rateFrames));
            wipe->Release();
        }
    } else if (style == Atem::TransitionStyle::DVE) {
        IBMDSwitcherTransitionDVEParameters *dve = nullptr;
        if (SUCCEEDED(meBlock->QueryInterface(IID_IBMDSwitcherTransitionDVEParameters, (void**)&dve)) && dve) {
            dve->SetRate(static_cast<uint32_t>(rateFrames));
            dve->Release();
        }
    }
}

void BmdSwitcherBackend::setPreviewTransition(int me, bool preview) {
    if (me > 0 && me <= static_cast<int>(m_meBlocks.size())) {
        m_meBlocks[me - 1]->SetPreviewTransition(preview);
    }
}

// USK implementations
void BmdSwitcherBackend::setUskOnAir(int me, int keyer, bool onAir) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                k->SetOnAir(onAir);
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskTie(int me, int keyer, bool tie) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherTransitionParameters *params = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->QueryInterface(IID_IBMDSwitcherTransitionParameters, (void**)&params)) && params) {
        BMDSwitcherTransitionSelection sel = bmdSwitcherTransitionSelectionBackground;
        params->GetNextTransitionSelection(&sel);
        uint32_t keyBit = (1 << keyer); // bmdSwitcherTransitionSelectionKey1 is bit 1
        uint32_t raw = static_cast<uint32_t>(sel);
        if (tie) raw |= keyBit;
        else raw &= ~keyBit;
        params->SetNextTransitionSelection(static_cast<BMDSwitcherTransitionSelection>(raw));
        params->Release();
    }
}

void BmdSwitcherBackend::setUskType(int me, int keyer, Atem::KeyType type) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                BMDSwitcherKeyType bmdType = bmdSwitcherKeyTypeLuma;
                switch (type) {
                    case Atem::KeyType::Chroma: bmdType = bmdSwitcherKeyTypeChroma; break;
                    case Atem::KeyType::Pattern: bmdType = bmdSwitcherKeyTypePattern; break;
                    case Atem::KeyType::DVE: bmdType = bmdSwitcherKeyTypeDVE; break;
                    default: break;
                }
                k->SetType(bmdType);
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskFillSource(int me, int keyer, int64_t inputId) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                k->SetInputFill(inputId);
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskCutSource(int me, int keyer, int64_t inputId) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                k->SetInputCut(inputId);
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskLumaParams(int me, int keyer, bool preMult, float clip, float gain, bool inverse) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyLumaParameters *luma = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyLumaParameters, (void**)&luma)) && luma) {
                    luma->SetPreMultiplied(preMult);
                    luma->SetClip(clip);
                    luma->SetGain(gain);
                    luma->SetInverse(inverse);
                    luma->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskChromaParams(int me, int keyer, float hue, float gain, float ySuppress, float lift, bool narrow) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyChromaParameters *chr = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyChromaParameters, (void**)&chr)) && chr) {
                    chr->SetHue(hue);
                    chr->SetGain(gain);
                    chr->SetYSuppress(ySuppress);
                    chr->SetLift(lift);
                    chr->SetNarrow(narrow);
                    chr->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskAdvancedChromaSample(int me, int keyer, float x, float y) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyAdvancedChromaParameters *adv = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyAdvancedChromaParameters, (void**)&adv)) && adv) {
                    adv->SetCursorXPosition(x);
                    adv->SetCursorYPosition(y);
                    adv->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskFlyEnabled(int me, int keyer, bool enabled) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyFlyParameters *fly = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyFlyParameters, (void**)&fly)) && fly) {
                    fly->SetFly(enabled);
                    fly->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskFlyPosition(int me, int keyer, float x, float y) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyFlyParameters *fly = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyFlyParameters, (void**)&fly)) && fly) {
                    fly->SetPositionX(x);
                    fly->SetPositionY(y);
                    fly->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskFlySize(int me, int keyer, float sizeX, float sizeY) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyFlyParameters *fly = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyFlyParameters, (void**)&fly)) && fly) {
                    fly->SetSizeX(sizeX);
                    fly->SetSizeY(sizeY);
                    fly->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskFlyRotation(int me, int keyer, float rotation) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyFlyParameters *fly = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyFlyParameters, (void**)&fly)) && fly) {
                    fly->SetRotation(rotation);
                    fly->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::resetUskFly(int me, int keyer) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                IBMDSwitcherKeyFlyParameters *fly = nullptr;
                if (SUCCEEDED(k->QueryInterface(IID_IBMDSwitcherKeyFlyParameters, (void**)&fly)) && fly) {
                    fly->ResetDVE();
                    fly->Release();
                }
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::setUskMask(int me, int keyer, bool enabled, float top, float bottom, float left, float right) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                k->SetMasked(enabled);
                k->SetMaskTop(top);
                k->SetMaskBottom(bottom);
                k->SetMaskLeft(left);
                k->SetMaskRight(right);
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

void BmdSwitcherBackend::resetUskMask(int me, int keyer) {
    if (me <= 0 || me > static_cast<int>(m_meBlocks.size())) return;
    IBMDSwitcherKeyIterator *kIt = nullptr;
    if (SUCCEEDED(m_meBlocks[me - 1]->CreateIterator(IID_IBMDSwitcherKeyIterator, (void**)&kIt)) && kIt) {
        IBMDSwitcherKey *k = nullptr;
        int idx = 1;
        while (kIt->Next(&k) == S_OK && k) {
            if (idx == keyer) {
                k->ResetMask();
                k->Release();
                break;
            }
            k->Release();
            idx++;
        }
        kIt->Release();
    }
}

// DSK operations
void BmdSwitcherBackend::setDskOnAir(int keyer, bool onAir) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->SetOnAir(onAir);
    }
}

void BmdSwitcherBackend::setDskTie(int keyer, bool tie) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->SetTie(tie);
    }
}

void BmdSwitcherBackend::performDskAutoTransition(int keyer) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->PerformAutoTransition();
    }
}

void BmdSwitcherBackend::setDskRate(int keyer, int rateFrames) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->SetRate(rateFrames);
    }
}

void BmdSwitcherBackend::setDskFillSource(int keyer, int64_t inputId) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->SetInputFill(inputId);
    }
}

void BmdSwitcherBackend::setDskCutSource(int keyer, int64_t inputId) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->SetInputCut(inputId);
    }
}

void BmdSwitcherBackend::setDskLumaParams(int keyer, bool preMult, float clip, float gain, bool inverse) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        auto *dsk = m_dsks[keyer - 1];
        dsk->SetPreMultiplied(preMult);
        dsk->SetClip(clip);
        dsk->SetGain(gain);
        dsk->SetInverse(inverse);
    }
}

void BmdSwitcherBackend::setDskMask(int keyer, bool enabled, float top, float bottom, float left, float right) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        auto *dsk = m_dsks[keyer - 1];
        dsk->SetMasked(enabled);
        dsk->SetMaskTop(top);
        dsk->SetMaskBottom(bottom);
        dsk->SetMaskLeft(left);
        dsk->SetMaskRight(right);
    }
}

void BmdSwitcherBackend::resetDskMask(int keyer) {
    if (keyer > 0 && keyer <= static_cast<int>(m_dsks.size())) {
        m_dsks[keyer - 1]->ResetMask();
    }
}

// SuperSource
void BmdSwitcherBackend::setSuperSourceBoxEnabled(int box, bool enabled) {
    if (box >= 1 && box <= 4 && m_superSourceBoxes[box - 1]) {
        m_superSourceBoxes[box - 1]->SetEnabled(enabled);
    }
}

void BmdSwitcherBackend::setSuperSourceBoxSource(int box, int64_t inputId) {
    if (box >= 1 && box <= 4 && m_superSourceBoxes[box - 1]) {
        m_superSourceBoxes[box - 1]->SetInputSource(inputId);
    }
}

void BmdSwitcherBackend::setSuperSourceBoxPosition(int box, float x, float y) {
    if (box >= 1 && box <= 4 && m_superSourceBoxes[box - 1]) {
        m_superSourceBoxes[box - 1]->SetPositionX(x);
        m_superSourceBoxes[box - 1]->SetPositionY(y);
    }
}

void BmdSwitcherBackend::setSuperSourceBoxSize(int box, float size) {
    if (box >= 1 && box <= 4 && m_superSourceBoxes[box - 1]) {
        m_superSourceBoxes[box - 1]->SetSize(size);
    }
}

void BmdSwitcherBackend::setSuperSourceBoxCrop(int box, bool cropped, float top, float bottom, float left, float right) {
    if (box >= 1 && box <= 4 && m_superSourceBoxes[box - 1]) {
        auto *b = m_superSourceBoxes[box - 1];
        b->SetCropped(cropped);
        b->SetCropTop(top);
        b->SetCropBottom(bottom);
        b->SetCropLeft(left);
        b->SetCropRight(right);
    }
}

void BmdSwitcherBackend::resetSuperSourceBoxCrop(int box) {
    if (box >= 1 && box <= 4 && m_superSourceBoxes[box - 1]) {
        m_superSourceBoxes[box - 1]->ResetCrop();
    }
}

void BmdSwitcherBackend::setSuperSourceBorder(bool enabled, float widthIn, float widthOut, float softnessIn, float softnessOut,
                                              float hue, float sat, float luma) {
    if (!m_superSource) return;
    IBMDSwitcherSuperSourceBorder *border = nullptr;
    if (SUCCEEDED(m_superSource->QueryInterface(IID_IBMDSwitcherSuperSourceBorder, (void**)&border)) && border) {
        border->SetBorderEnabled(enabled);
        border->SetBorderWidthIn(widthIn);
        border->SetBorderWidthOut(widthOut);
        border->SetBorderSoftnessIn(softnessIn);
        border->SetBorderSoftnessOut(softnessOut);
        border->SetBorderHue(hue);
        border->SetBorderSaturation(sat);
        border->SetBorderLuma(luma);
        border->Release();
    }
}

// Audio
void BmdSwitcherBackend::setAudioInputGain(int64_t inputId, float gainDb) {
    if (m_fairlightMixer) {
        IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
        if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
            IBMDSwitcherFairlightAudioInput *input = nullptr;
            while (it->Next(&input) == S_OK && input) {
                BMDSwitcherAudioInputId id;
                input->GetId(&id);
                if (id == inputId) {
                    IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                    if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                        IBMDSwitcherFairlightAudioSource *source = nullptr;
                        while (sIt->Next(&source) == S_OK && source) {
                            source->SetFaderGain(gainDb);
                            source->Release();
                        }
                        sIt->Release();
                    }
                    input->Release();
                    break;
                }
                input->Release();
            }
            it->Release();
        }
    } else if (m_classicMixer) {
        IBMDSwitcherAudioInputIterator *it = nullptr;
        if (SUCCEEDED(m_classicMixer->CreateIterator(IID_IBMDSwitcherAudioInputIterator, (void**)&it)) && it) {
            IBMDSwitcherAudioInput *input = nullptr;
            while (it->Next(&input) == S_OK && input) {
                BMDSwitcherAudioInputId id;
                input->GetAudioInputId(&id);
                if (id == inputId) {
                    input->SetGain(gainDb);
                    input->Release();
                    break;
                }
                input->Release();
            }
            it->Release();
        }
    }
}

void BmdSwitcherBackend::setAudioInputPan(int64_t inputId, float pan) {
    if (m_fairlightMixer) {
        IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
        if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
            IBMDSwitcherFairlightAudioInput *input = nullptr;
            while (it->Next(&input) == S_OK && input) {
                BMDSwitcherAudioInputId id;
                input->GetId(&id);
                if (id == inputId) {
                    IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                    if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                        IBMDSwitcherFairlightAudioSource *source = nullptr;
                        while (sIt->Next(&source) == S_OK && source) {
                            source->SetPan(pan);
                            source->Release();
                        }
                        sIt->Release();
                    }
                    input->Release();
                    break;
                }
                input->Release();
            }
            it->Release();
        }
    } else if (m_classicMixer) {
        IBMDSwitcherAudioInputIterator *it = nullptr;
        if (SUCCEEDED(m_classicMixer->CreateIterator(IID_IBMDSwitcherAudioInputIterator, (void**)&it)) && it) {
            IBMDSwitcherAudioInput *input = nullptr;
            while (it->Next(&input) == S_OK && input) {
                BMDSwitcherAudioInputId id;
                input->GetAudioInputId(&id);
                if (id == inputId) {
                    input->SetBalance(pan);
                    input->Release();
                    break;
                }
                input->Release();
            }
            it->Release();
        }
    }
}

void BmdSwitcherBackend::setAudioInputMixOption(int64_t inputId, Atem::AudioMixOption option) {
    if (m_fairlightMixer) {
        BMDSwitcherFairlightAudioMixOption mo = bmdSwitcherFairlightAudioMixOptionOff;
        if (option == Atem::AudioMixOption::On) mo = bmdSwitcherFairlightAudioMixOptionOn;
        else if (option == Atem::AudioMixOption::AudioFollowVideo) mo = bmdSwitcherFairlightAudioMixOptionAudioFollowVideo;

        IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
        if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
            IBMDSwitcherFairlightAudioInput *input = nullptr;
            while (it->Next(&input) == S_OK && input) {
                BMDSwitcherAudioInputId id;
                input->GetId(&id);
                if (id == inputId) {
                    IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                    if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                        IBMDSwitcherFairlightAudioSource *source = nullptr;
                        while (sIt->Next(&source) == S_OK && source) {
                            source->SetMixOption(mo);
                            source->Release();
                        }
                        sIt->Release();
                    }
                    input->Release();
                    break;
                }
                input->Release();
            }
            it->Release();
        }
    }
}

void BmdSwitcherBackend::setAudioMasterGain(float gainDb) {
    if (m_fairlightMixer) {
        m_fairlightMixer->SetMasterOutFaderGain(gainDb);
    } else if (m_classicMixer) {
        m_classicMixer->SetProgramOutGain(gainDb);
    }
}

void BmdSwitcherBackend::setAudioSolo(int64_t inputId, bool solo) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioSolo *soloIntf = nullptr;
    if (SUCCEEDED(m_fairlightMixer->QueryInterface(IID_IBMDSwitcherFairlightAudioSolo, (void**)&soloIntf)) && soloIntf) {
        soloIntf->SetSoloInput(inputId, 0);
        soloIntf->SetSolo(solo);
        soloIntf->Release();
    }
}

void BmdSwitcherBackend::setFairlightEqBand(int64_t inputId, int band, bool enabled, float freq, float gainDb, float q) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
    if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
        IBMDSwitcherFairlightAudioInput *input = nullptr;
        while (it->Next(&input) == S_OK && input) {
            BMDSwitcherAudioInputId id;
            input->GetId(&id);
            if (id == inputId) {
                IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                    IBMDSwitcherFairlightAudioSource *source = nullptr;
                    while (sIt->Next(&source) == S_OK && source) {
                        IBMDSwitcherFairlightAudioEqualizer *eq = nullptr;
                        if (SUCCEEDED(source->QueryInterface(IID_IBMDSwitcherFairlightAudioEqualizer, (void**)&eq)) && eq) {
                            IBMDSwitcherFairlightAudioEqualizerBandIterator *bIt = nullptr;
                            if (SUCCEEDED(eq->CreateIterator(IID_IBMDSwitcherFairlightAudioEqualizerBandIterator, (void**)&bIt)) && bIt) {
                                IBMDSwitcherFairlightAudioEqualizerBand *b = nullptr;
                                int bIdx = 0;
                                while (bIt->Next(&b) == S_OK && b) {
                                    if (bIdx == band) {
                                        b->SetEnabled(enabled);
                                        b->SetFrequency(freq);
                                        b->SetGain(gainDb);
                                        b->SetQFactor(q);
                                        b->Release();
                                        break;
                                    }
                                    b->Release();
                                    bIdx++;
                                }
                                bIt->Release();
                            }
                            eq->Release();
                        }
                        source->Release();
                    }
                    sIt->Release();
                }
                input->Release();
                break;
            }
            input->Release();
        }
        it->Release();
    }
}

void BmdSwitcherBackend::resetFairlightEq(int64_t inputId) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
    if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
        IBMDSwitcherFairlightAudioInput *input = nullptr;
        while (it->Next(&input) == S_OK && input) {
            BMDSwitcherAudioInputId id;
            input->GetId(&id);
            if (id == inputId) {
                IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                    IBMDSwitcherFairlightAudioSource *source = nullptr;
                    while (sIt->Next(&source) == S_OK && source) {
                        IBMDSwitcherFairlightAudioEqualizer *eq = nullptr;
                        if (SUCCEEDED(source->QueryInterface(IID_IBMDSwitcherFairlightAudioEqualizer, (void**)&eq)) && eq) {
                            eq->Reset();
                            eq->Release();
                        }
                        source->Release();
                    }
                    sIt->Release();
                }
                input->Release();
                break;
            }
            input->Release();
        }
        it->Release();
    }
}

void BmdSwitcherBackend::setFairlightCompressor(int64_t inputId, bool enabled, float thresholdDb, float ratio, float attackMs, float releaseMs) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
    if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
        IBMDSwitcherFairlightAudioInput *input = nullptr;
        while (it->Next(&input) == S_OK && input) {
            BMDSwitcherAudioInputId id;
            input->GetId(&id);
            if (id == inputId) {
                IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                    IBMDSwitcherFairlightAudioSource *source = nullptr;
                    while (sIt->Next(&source) == S_OK && source) {
                        IBMDSwitcherFairlightAudioDynamicsProcessor *dyn = nullptr;
                        if (SUCCEEDED(source->QueryInterface(IID_IBMDSwitcherFairlightAudioDynamicsProcessor, (void**)&dyn)) && dyn) {
                            IBMDSwitcherFairlightAudioCompressor *comp = nullptr;
                            if (SUCCEEDED(dyn->QueryInterface(IID_IBMDSwitcherFairlightAudioCompressor, (void**)&comp)) && comp) {
                                comp->SetEnabled(enabled);
                                comp->SetThreshold(thresholdDb);
                                comp->SetRatio(ratio);
                                comp->SetAttack(attackMs);
                                comp->SetRelease(releaseMs);
                                comp->Release();
                            }
                            dyn->Release();
                        }
                        source->Release();
                    }
                    sIt->Release();
                }
                input->Release();
                break;
            }
            input->Release();
        }
        it->Release();
    }
}

void BmdSwitcherBackend::setFairlightLimiter(int64_t inputId, bool enabled, float thresholdDb, float attackMs, float releaseMs) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
    if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
        IBMDSwitcherFairlightAudioInput *input = nullptr;
        while (it->Next(&input) == S_OK && input) {
            BMDSwitcherAudioInputId id;
            input->GetId(&id);
            if (id == inputId) {
                IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                    IBMDSwitcherFairlightAudioSource *source = nullptr;
                    while (sIt->Next(&source) == S_OK && source) {
                        IBMDSwitcherFairlightAudioDynamicsProcessor *dyn = nullptr;
                        if (SUCCEEDED(source->QueryInterface(IID_IBMDSwitcherFairlightAudioDynamicsProcessor, (void**)&dyn)) && dyn) {
                            IBMDSwitcherFairlightAudioLimiter *lim = nullptr;
                            if (SUCCEEDED(dyn->QueryInterface(IID_IBMDSwitcherFairlightAudioLimiter, (void**)&lim)) && lim) {
                                lim->SetEnabled(enabled);
                                lim->SetThreshold(thresholdDb);
                                lim->SetAttack(attackMs);
                                lim->SetRelease(releaseMs);
                                lim->Release();
                            }
                            dyn->Release();
                        }
                        source->Release();
                    }
                    sIt->Release();
                }
                input->Release();
                break;
            }
            input->Release();
        }
        it->Release();
    }
}

void BmdSwitcherBackend::setFairlightExpander(int64_t inputId, bool enabled, float thresholdDb, float rangeDb, float ratio) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
    if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
        IBMDSwitcherFairlightAudioInput *input = nullptr;
        while (it->Next(&input) == S_OK && input) {
            BMDSwitcherAudioInputId id;
            input->GetId(&id);
            if (id == inputId) {
                IBMDSwitcherFairlightAudioSourceIterator *sIt = nullptr;
                if (SUCCEEDED(input->CreateIterator(IID_IBMDSwitcherFairlightAudioSourceIterator, (void**)&sIt)) && sIt) {
                    IBMDSwitcherFairlightAudioSource *source = nullptr;
                    while (sIt->Next(&source) == S_OK && source) {
                        IBMDSwitcherFairlightAudioDynamicsProcessor *dyn = nullptr;
                        if (SUCCEEDED(source->QueryInterface(IID_IBMDSwitcherFairlightAudioDynamicsProcessor, (void**)&dyn)) && dyn) {
                            IBMDSwitcherFairlightAudioExpander *exp = nullptr;
                            if (SUCCEEDED(dyn->QueryInterface(IID_IBMDSwitcherFairlightAudioExpander, (void**)&exp)) && exp) {
                                exp->SetEnabled(enabled);
                                exp->SetThreshold(thresholdDb);
                                exp->SetRange(rangeDb);
                                exp->SetRatio(ratio);
                                exp->Release();
                            }
                            dyn->Release();
                        }
                        source->Release();
                    }
                    sIt->Release();
                }
                input->Release();
                break;
            }
            input->Release();
        }
        it->Release();
    }
}

void BmdSwitcherBackend::setAnalogInputPreamp(int64_t inputId, float /*gainDb*/, bool micLevel, bool phantom48V, bool /*lowCut*/) {
    if (!m_fairlightMixer) return;
    IBMDSwitcherFairlightAudioInputIterator *it = nullptr;
    if (SUCCEEDED(m_fairlightMixer->CreateIterator(IID_IBMDSwitcherFairlightAudioInputIterator, (void**)&it)) && it) {
        IBMDSwitcherFairlightAudioInput *input = nullptr;
        while (it->Next(&input) == S_OK && input) {
            BMDSwitcherAudioInputId id;
            input->GetId(&id);
            if (id == inputId) {
                IBMDSwitcherFairlightAnalogAudioInput *analog = nullptr;
                if (SUCCEEDED(input->QueryInterface(IID_IBMDSwitcherFairlightAnalogAudioInput, (void**)&analog)) && analog) {
                    analog->SetInputLevel(micLevel ? bmdSwitcherFairlightAudioAnalogInputLevelMicrophone : bmdSwitcherFairlightAudioAnalogInputLevelProLine);
                    analog->SetMicPowerMode(phantom48V ? bmdSwitcherFairlightAudioAnalogInputMicPowerModePlugInPower : bmdSwitcherFairlightAudioAnalogInputMicPowerModeNoPower);
                    analog->Release();
                }
                input->Release();
                break;
            }
            input->Release();
        }
        it->Release();
    }
}

// MultiView
void BmdSwitcherBackend::setMultiViewLayout(int mvIndex, Atem::MultiViewLayout layout) {
    if (mvIndex > 0 && mvIndex <= static_cast<int>(m_multiviews.size())) {
        BMDSwitcherMultiViewLayout bmdLayout;
        switch (layout) {
            case Atem::MultiViewLayout::Top:    bmdLayout = bmdSwitcherMultiViewLayoutProgramTop; break;
            case Atem::MultiViewLayout::Bottom: bmdLayout = bmdSwitcherMultiViewLayoutProgramBottom; break;
            case Atem::MultiViewLayout::Left:   bmdLayout = bmdSwitcherMultiViewLayoutProgramLeft; break;
            case Atem::MultiViewLayout::Right:  bmdLayout = bmdSwitcherMultiViewLayoutProgramRight; break;
            default:
                emit logMessage(QStringLiteral("MultiView layout not supported by the SDK on this switcher (use top, bottom, left or right)."));
                return;
        }
        m_multiviews[mvIndex - 1]->SetLayout(bmdLayout);
    }
}

void BmdSwitcherBackend::setMultiViewWindowSource(int mvIndex, int window, int64_t inputId) {
    if (mvIndex > 0 && mvIndex <= static_cast<int>(m_multiviews.size())) {
        m_multiviews[mvIndex - 1]->SetWindowInput(window - 1, inputId);
    }
}

void BmdSwitcherBackend::setMultiViewSafeAreaVisible(int mvIndex, bool visible) {
    if (mvIndex > 0 && mvIndex <= static_cast<int>(m_multiviews.size())) {
        m_multiviews[mvIndex - 1]->SetSafeAreaEnabled(0, visible);
    }
}

void BmdSwitcherBackend::setMultiViewMetersVisible(int mvIndex, bool visible) {
    if (mvIndex > 0 && mvIndex <= static_cast<int>(m_multiviews.size())) {
        m_multiviews[mvIndex - 1]->SetVuMeterEnabled(0, visible);
    }
}

// Display Clock (SDK 11.0)
void BmdSwitcherBackend::setDisplayClockEnabled(bool enabled) {
    if (m_clock) m_clock->SetEnabled(enabled);
}

void BmdSwitcherBackend::setDisplayClockMode(Atem::ClockMode mode) {
    if (!m_clock) return;
    BMDSwitcherDisplayClockMode m = bmdSwitcherDisplayClockModeTimeOfDay;
    if (mode == Atem::ClockMode::CountDown) m = bmdSwitcherDisplayClockModeCountdown;
    else if (mode == Atem::ClockMode::CountUp) m = bmdSwitcherDisplayClockModeCountup;
    m_clock->SetClockMode(m);
}

void BmdSwitcherBackend::startDisplayClock() {
    if (m_clock) m_clock->Start();
}

void BmdSwitcherBackend::stopDisplayClock() {
    if (m_clock) m_clock->Stop();
}

void BmdSwitcherBackend::resetDisplayClock() {
    if (m_clock) m_clock->Reset();
}

void BmdSwitcherBackend::setDisplayClockTime(int hours, int minutes, int seconds) {
    if (m_clock) m_clock->SetStartFrom(hours, minutes, seconds, 0);
}

void BmdSwitcherBackend::setDisplayClockLayout(float size, float x, float y, float opacity) {
    if (m_clock) {
        m_clock->SetSize(static_cast<uint16_t>(size * 100));
        m_clock->SetPositionX(x);
        m_clock->SetPositionY(y);
        m_clock->SetOpacity(static_cast<uint16_t>(opacity * 100));
    }
}

// Recording & Streaming
void BmdSwitcherBackend::startRecording() {
    if (m_record) m_record->StartRecording();
}

void BmdSwitcherBackend::stopRecording() {
    if (m_record) m_record->StopRecording();
}

void BmdSwitcherBackend::switchRecordingDisk() {
    if (m_record) m_record->SwitchDisk();
}

void BmdSwitcherBackend::setRecordingFilename(const QString &filename) {
    if (!m_record) return;
#if defined(__APPLE__)
    CFStringRef cf = filename.toCFString();
    m_record->SetFilename(cf);
    CFRelease(cf);
#elif defined(_WIN32)
    BSTR b = SysAllocString((const OLECHAR*)filename.utf16());
    m_record->SetFilename(b);
    SysFreeString(b);
#endif
}

void BmdSwitcherBackend::setRecordAllInputsIso(bool recordIso) {
    if (m_record) m_record->SetRecordAllISOInputs(recordIso);
}

void BmdSwitcherBackend::startStreaming() {
    if (m_stream) m_stream->StartStreaming();
}

void BmdSwitcherBackend::stopStreaming() {
    if (m_stream) m_stream->StopStreaming();
}

void BmdSwitcherBackend::setStreamingService(const QString &serviceName, const QString &url, const QString &key) {
    if (!m_stream) return;
#if defined(__APPLE__)
    CFStringRef cfS = serviceName.toCFString();
    CFStringRef cfU = url.toCFString();
    CFStringRef cfK = key.toCFString();
    m_stream->SetServiceName(cfS);
    m_stream->SetUrl(cfU);
    m_stream->SetKey(cfK);
    CFRelease(cfS); CFRelease(cfU); CFRelease(cfK);
#elif defined(_WIN32)
    BSTR bS = SysAllocString((const OLECHAR*)serviceName.utf16());
    BSTR bU = SysAllocString((const OLECHAR*)url.utf16());
    BSTR bK = SysAllocString((const OLECHAR*)key.utf16());
    m_stream->SetServiceName(bS);
    m_stream->SetUrl(bU);
    m_stream->SetKey(bK);
    SysFreeString(bS); SysFreeString(bU); SysFreeString(bK);
#endif
}

// Macros
void BmdSwitcherBackend::runMacro(uint32_t index) {
    if (m_macroControl) m_macroControl->Run(index);
}

void BmdSwitcherBackend::stopMacro() {
    if (m_macroControl) m_macroControl->StopRunning();
}

void BmdSwitcherBackend::setMacroLoop(bool loop) {
    if (m_macroControl) m_macroControl->SetLoop(loop);
}

uint32_t BmdSwitcherBackend::maxMacros() const {
    uint32_t count = 0;
    if (m_macroPool) m_macroPool->GetMaxCount(&count);
    return count;
}

// HyperDeck
void BmdSwitcherBackend::hyperDeckPlay(int deck) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->Play();
    }
}

void BmdSwitcherBackend::hyperDeckStop(int deck) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->Stop();
    }
}

void BmdSwitcherBackend::hyperDeckRecord(int deck) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->Record();
    }
}

void BmdSwitcherBackend::hyperDeckShuttle(int deck, int speedPercent) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->Shuttle(speedPercent);
    }
}

void BmdSwitcherBackend::hyperDeckJog(int deck, int frameCount) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->Jog(frameCount);
    }
}

void BmdSwitcherBackend::hyperDeckSelectClip(int deck, int clipIndex) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->SetCurrentClip(clipIndex - 1);
    }
}

void BmdSwitcherBackend::hyperDeckSetClipTime(int deck, const QString &timecode) {
    // cue timecode
}

void BmdSwitcherBackend::hyperDeckSetTimelineTime(int deck, const QString &timecode) {
    // cue timecode
}

void BmdSwitcherBackend::hyperDeckSetLooped(int deck, bool loop) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->SetLoopedPlayback(loop);
    }
}

void BmdSwitcherBackend::hyperDeckSetSingleClip(int deck, bool single) {
    if (deck > 0 && deck <= static_cast<int>(m_hyperdecks.size())) {
        m_hyperdecks[deck - 1]->SetSingleClipPlayback(single);
    }
}

// Camera Control & PTZ
// Camera control (Blackmagic Camera Control protocol, via IBMDSwitcherCameraControl).
// destinationDevice is the camera number as configured on the switcher.
void BmdSwitcherBackend::setCameraIris(int camera, float value) {
    if (!m_cameraControl) return;
    const double v = qBound(0.0f, value, 1.0f);
    m_cameraControl->SetFloats(camera, CamCtl::Lens, CamCtl::LensApertureNormalised, 1, &v);
}

void BmdSwitcherBackend::setCameraFocus(int camera, float value) {
    if (!m_cameraControl) return;
    const double v = qBound(0.0f, value, 1.0f);
    m_cameraControl->SetFloats(camera, CamCtl::Lens, CamCtl::LensFocus, 1, &v);
}

void BmdSwitcherBackend::triggerCameraAutoFocus(int camera) {
    if (!m_cameraControl) return;
    const bool trigger = true;
    m_cameraControl->SetFlags(camera, CamCtl::Lens, CamCtl::LensAutoFocus, 1, &trigger);
}

void BmdSwitcherBackend::setCameraGain(int camera, float db) {
    if (!m_cameraControl) return;
    const int8_t v = static_cast<int8_t>(qBound(-128, qRound(db), 127));
    m_cameraControl->SetInt8s(camera, CamCtl::Video, CamCtl::VideoGain, 1, &v);
}

void BmdSwitcherBackend::setCameraShutter(int camera, float speed) {
    if (!m_cameraControl) return;
    // speed is the denominator of the shutter speed (60 = 1/60 s)
    const int32_t v = static_cast<int32_t>(qMax(1, qRound(speed)));
    m_cameraControl->SetInt32s(camera, CamCtl::Video, CamCtl::VideoShutterSpeed, 1, &v);
}

void BmdSwitcherBackend::setCameraWhiteBalance(int camera, float kelvin) {
    if (!m_cameraControl) return;
    int16_t v[2] = { static_cast<int16_t>(qBound(2500, qRound(kelvin), 10000)), 0 }; // temperature, tint
    uint32_t count = 2;
    int16_t current[2] = { 0, 0 };
    if (SUCCEEDED(m_cameraControl->GetInt16s(camera, CamCtl::Video, CamCtl::VideoManualWhiteBalance, &count, current)) && count == 2) {
        v[1] = current[1]; // keep the existing tint
    }
    m_cameraControl->SetInt16s(camera, CamCtl::Video, CamCtl::VideoManualWhiteBalance, 2, v);
}

// pan / tilt / zoom are speeds from -1.0 to 1.0 (0 = stop)
void BmdSwitcherBackend::setCameraPtz(int camera, float pan, float tilt, float zoom) {
    if (!m_cameraControl) return;
    const double panTilt[2] = { qBound(-1.0f, pan, 1.0f), qBound(-1.0f, tilt, 1.0f) };
    m_cameraControl->SetFloats(camera, CamCtl::Ptz, CamCtl::PtzPanTiltVelocity, 2, panTilt);
    const double z = qBound(-1.0f, zoom, 1.0f);
    m_cameraControl->SetFloats(camera, CamCtl::Lens, CamCtl::LensContinuousZoom, 1, &z);
}

void BmdSwitcherBackend::recallCameraPtzPreset(int camera, int presetIndex) {
    if (!m_cameraControl) return;
    const int8_t cmd[2] = { 1 /* recall */, static_cast<int8_t>(qBound(0, presetIndex, 127)) };
    m_cameraControl->SetInt8s(camera, CamCtl::Ptz, CamCtl::PtzMemoryPreset, 2, cmd);
}

// Media Players
void BmdSwitcherBackend::selectMediaPlayerStill(int player, int stillIndex) {
    if (!m_mediaPool) return;
    IBMDSwitcherMediaPlayerIterator *it = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherMediaPlayerIterator, (void**)&it)) && it) {
        IBMDSwitcherMediaPlayer *mp = nullptr;
        int idx = 1;
        while (it->Next(&mp) == S_OK && mp) {
            if (idx == player) {
                mp->SetSource(bmdSwitcherMediaPlayerSourceTypeStill, stillIndex - 1);
                mp->Release();
                break;
            }
            mp->Release();
            idx++;
        }
        it->Release();
    }
}

void BmdSwitcherBackend::selectMediaPlayerClip(int player, int clipIndex) {
    if (!m_mediaPool) return;
    IBMDSwitcherMediaPlayerIterator *it = nullptr;
    if (SUCCEEDED(m_switcher->CreateIterator(IID_IBMDSwitcherMediaPlayerIterator, (void**)&it)) && it) {
        IBMDSwitcherMediaPlayer *mp = nullptr;
        int idx = 1;
        while (it->Next(&mp) == S_OK && mp) {
            if (idx == player) {
                mp->SetSource(bmdSwitcherMediaPlayerSourceTypeClip, clipIndex - 1);
                mp->Release();
                break;
            }
            mp->Release();
            idx++;
        }
        it->Release();
    }
}

void BmdSwitcherBackend::captureStill(int64_t sourceInputId) {
    if (!m_mediaPool) return;
    IBMDSwitcherStillCapture *cap = nullptr;
    if (SUCCEEDED(m_mediaPool->QueryInterface(IID_IBMDSwitcherStillCapture, (void**)&cap)) && cap) {
        cap->CaptureStill();
        cap->Release();
    }
}

// Aux & Converted Video Mode
void BmdSwitcherBackend::setAuxSource(int auxIndex, int64_t inputId) {
    if (auxIndex > 0 && auxIndex <= static_cast<int>(m_auxInputs.size())) {
        IBMDSwitcherInputAux *aux = nullptr;
        if (SUCCEEDED(m_auxInputs[auxIndex - 1]->QueryInterface(IID_IBMDSwitcherInputAux, (void**)&aux)) && aux) {
            aux->SetInputSource(inputId);
            aux->Release();
        }
    }
}

void BmdSwitcherBackend::setConvertedVideoMode(int auxIndex, uint32_t videoMode) {
    if (auxIndex > 0 && auxIndex <= static_cast<int>(m_auxInputs.size())) {
        IBMDSwitcherConvertedVideoModeOutput *conv = nullptr;
        if (SUCCEEDED(m_auxInputs[auxIndex - 1]->QueryInterface(IID_IBMDSwitcherConvertedVideoModeOutput, (void**)&conv)) && conv) {
            conv->SetConvertedVideoMode(bmdSwitcherVideoModeUnknown, static_cast<BMDSwitcherVideoMode>(videoMode));
            conv->Release();
        }
    }
}

// Callback dispatchers
void BmdSwitcherBackend::onSwitcherDisconnected() {
    QMetaObject::invokeMethod(this, [this]() {
        cleanup();
        m_connectionState = Atem::ConnectionState::Disconnected;
        emit connectionStateChanged(m_connectionState);
        emit logMessage(QStringLiteral("Disconnected event received from switcher."));
    }, Qt::QueuedConnection);
}

void BmdSwitcherBackend::onMeUpdated(int meIndex, BMDSwitcherMixEffectBlockEventType eventType) {
    QMetaObject::invokeMethod(this, [this, meIndex, eventType]() {
        if (meIndex <= 0 || meIndex > static_cast<int>(m_meBlocks.size())) return;
        auto *me = m_meBlocks[meIndex - 1];
        auto &state = m_meMap[meIndex];
        state.index = meIndex;

        if (eventType == bmdSwitcherMixEffectBlockEventTypeProgramInputChanged) {
            BMDSwitcherInputId pId;
            if (SUCCEEDED(me->GetProgramInput(&pId))) state.programInput = pId;
        } else if (eventType == bmdSwitcherMixEffectBlockEventTypePreviewInputChanged) {
            BMDSwitcherInputId prvId;
            if (SUCCEEDED(me->GetPreviewInput(&prvId))) state.previewInput = prvId;
        } else if (eventType == bmdSwitcherMixEffectBlockEventTypeTransitionPositionChanged) {
            double pos;
            if (SUCCEEDED(me->GetTransitionPosition(&pos))) state.transitionPosition = static_cast<float>(pos);
        } else if (eventType == bmdSwitcherMixEffectBlockEventTypeFadeToBlackFullyBlackChanged) {
            bool ftb;
            if (SUCCEEDED(me->GetFadeToBlackFullyBlack(&ftb))) state.ftbActive = ftb;
        }

        emit meStateChanged(state);
    }, Qt::QueuedConnection);
}

void BmdSwitcherBackend::onDskUpdated(int dskIndex, BMDSwitcherDownstreamKeyEventType eventType) {
    Q_UNUSED(dskIndex);
    Q_UNUSED(eventType);
    QMetaObject::invokeMethod(this, [this]() { pollState(false); }, Qt::QueuedConnection);
}

void BmdSwitcherBackend::onRecordUpdated(BMDSwitcherRecordAVEventType eventType) {
    Q_UNUSED(eventType);
    QMetaObject::invokeMethod(this, [this]() { pollState(false); }, Qt::QueuedConnection);
}

void BmdSwitcherBackend::onStreamUpdated(BMDSwitcherStreamRTMPEventType eventType) {
    Q_UNUSED(eventType);
    QMetaObject::invokeMethod(this, [this]() { pollState(false); }, Qt::QueuedConnection);
}

void BmdSwitcherBackend::onClockUpdated(BMDSwitcherDisplayClockEventType eventType) {
    Q_UNUSED(eventType);
    QMetaObject::invokeMethod(this, [this]() { pollState(false); }, Qt::QueuedConnection);
}

void BmdSwitcherBackend::onMultiViewUpdated(int mvIndex, BMDSwitcherMultiViewEventType eventType) {
    Q_UNUSED(mvIndex);
    Q_UNUSED(eventType);
    QMetaObject::invokeMethod(this, [this]() { pollState(false); }, Qt::QueuedConnection);
}

#endif
