#include "BmdCallbackMonitors.h"
#include "BmdSwitcherBackend.h"
#include <cstring>

#if defined(__APPLE__) || defined(_WIN32)

#if defined(_WIN32)
static REFIID unknownIid() { return IID_IUnknown; }
#else
static CFUUIDBytes unknownIid() { return CFUUIDGetUUIDBytes(IUnknownUUID); }
#endif

static bool sameIid(REFIID a, REFIID b) {
#if defined(_WIN32)
    return IsEqualIID(a, b);
#else
    return std::memcmp(&a, &b, sizeof(REFIID)) == 0;
#endif
}

// SwitcherMonitor
SwitcherMonitor::SwitcherMonitor(BmdSwitcherBackend *backend) : m_backend(backend) {}

HRESULT STDMETHODCALLTYPE SwitcherMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE SwitcherMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE SwitcherMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE SwitcherMonitor::Notify(BMDSwitcherEventType eventType, BMDSwitcherVideoMode /*coreVideoMode*/) {
    if (eventType == bmdSwitcherEventTypeDisconnected) {
        if (auto *b = m_backend.load()) b->onSwitcherDisconnected();
    }
    return S_OK;
}

// MixEffectBlockMonitor
MixEffectBlockMonitor::MixEffectBlockMonitor(BmdSwitcherBackend *backend, int meIndex)
    : m_backend(backend), m_meIndex(meIndex) {}

HRESULT STDMETHODCALLTYPE MixEffectBlockMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherMixEffectBlockCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MixEffectBlockMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE MixEffectBlockMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE MixEffectBlockMonitor::Notify(BMDSwitcherMixEffectBlockEventType eventType) {
    if (auto *b = m_backend.load()) {
        b->onMeUpdated(m_meIndex, eventType);
    }
    return S_OK;
}

// DownstreamKeyMonitor
DownstreamKeyMonitor::DownstreamKeyMonitor(BmdSwitcherBackend *backend, int dskIndex)
    : m_backend(backend), m_dskIndex(dskIndex) {}

HRESULT STDMETHODCALLTYPE DownstreamKeyMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherDownstreamKeyCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DownstreamKeyMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE DownstreamKeyMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE DownstreamKeyMonitor::Notify(BMDSwitcherDownstreamKeyEventType eventType) {
    if (auto *b = m_backend.load()) {
        b->onDskUpdated(m_dskIndex, eventType);
    }
    return S_OK;
}

// RecordMonitor
RecordMonitor::RecordMonitor(BmdSwitcherBackend *backend) : m_backend(backend) {}

HRESULT STDMETHODCALLTYPE RecordMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherRecordAVCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE RecordMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE RecordMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE RecordMonitor::Notify(BMDSwitcherRecordAVEventType eventType) {
    if (auto *b = m_backend.load()) {
        b->onRecordUpdated(eventType);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RecordMonitor::NotifyWorkingSetChange(uint32_t /*workingSetIndex*/, BMDSwitcherRecordDiskId /*diskId*/) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RecordMonitor::NotifyDiskAvailability(BMDSwitcherRecordDiskAvailabilityEventType /*eventType*/, BMDSwitcherRecordDiskId /*diskId*/) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE RecordMonitor::NotifyStatus(BMDSwitcherRecordAVState /*stateType*/, BMDSwitcherRecordAVError /*error*/) {
    if (auto *b = m_backend.load()) {
        b->onRecordUpdated(static_cast<BMDSwitcherRecordAVEventType>(0));
    }
    return S_OK;
}

// StreamMonitor
StreamMonitor::StreamMonitor(BmdSwitcherBackend *backend) : m_backend(backend) {}

HRESULT STDMETHODCALLTYPE StreamMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherStreamRTMPCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE StreamMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE StreamMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE StreamMonitor::Notify(BMDSwitcherStreamRTMPEventType eventType) {
    if (auto *b = m_backend.load()) {
        b->onStreamUpdated(eventType);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamMonitor::NotifyStatus(BMDSwitcherStreamRTMPState /*stateType*/, BMDSwitcherStreamRTMPError /*error*/) {
    if (auto *b = m_backend.load()) {
        b->onStreamUpdated(static_cast<BMDSwitcherStreamRTMPEventType>(0));
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE StreamMonitor::NotifyCloudStreamingDestination(BMDSwitcherCloudStreamDestinationConfigurationEventType /*eventType*/, uint32_t /*id*/) {
    return S_OK;
}

// DisplayClockMonitor
DisplayClockMonitor::DisplayClockMonitor(BmdSwitcherBackend *backend) : m_backend(backend) {}

HRESULT STDMETHODCALLTYPE DisplayClockMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherDisplayClockCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DisplayClockMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE DisplayClockMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE DisplayClockMonitor::Notify(BMDSwitcherDisplayClockEventType eventType) {
    if (auto *b = m_backend.load()) {
        b->onClockUpdated(eventType);
    }
    return S_OK;
}

// MultiViewMonitor
MultiViewMonitor::MultiViewMonitor(BmdSwitcherBackend *backend, int mvIndex)
    : m_backend(backend), m_mvIndex(mvIndex) {}

HRESULT STDMETHODCALLTYPE MultiViewMonitor::QueryInterface(REFIID iid, LPVOID *ppv) {
    if (!ppv) return E_POINTER;
    if (sameIid(iid, unknownIid()) || sameIid(iid, IID_IBMDSwitcherMultiViewCallback)) {
        *ppv = this;
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MultiViewMonitor::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE MultiViewMonitor::Release() {
    ULONG newRef = --m_refCount;
    if (newRef == 0) delete this;
    return newRef;
}

HRESULT STDMETHODCALLTYPE MultiViewMonitor::Notify(BMDSwitcherMultiViewEventType eventType, int32_t /*window*/) {
    if (auto *b = m_backend.load()) {
        b->onMultiViewUpdated(m_mvIndex, eventType);
    }
    return S_OK;
}

#endif
