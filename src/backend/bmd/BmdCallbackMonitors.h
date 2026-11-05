#pragma once

#include "BmdInclude.h"
#include <atomic>

#if defined(ATEM_HAVE_BMD_SDK)

class BmdSwitcherBackend;

class SwitcherMonitor : public IBMDSwitcherCallback {
public:
    explicit SwitcherMonitor(BmdSwitcherBackend *backend);
    virtual ~SwitcherMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherEventType eventType, BMDSwitcherVideoMode coreVideoMode) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
};

class MixEffectBlockMonitor : public IBMDSwitcherMixEffectBlockCallback {
public:
    MixEffectBlockMonitor(BmdSwitcherBackend *backend, int meIndex);
    virtual ~MixEffectBlockMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherMixEffectBlockEventType eventType) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
    int m_meIndex = 1;
};

class DownstreamKeyMonitor : public IBMDSwitcherDownstreamKeyCallback {
public:
    DownstreamKeyMonitor(BmdSwitcherBackend *backend, int dskIndex);
    virtual ~DownstreamKeyMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherDownstreamKeyEventType eventType) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
    int m_dskIndex = 1;
};

class RecordMonitor : public IBMDSwitcherRecordAVCallback {
public:
    explicit RecordMonitor(BmdSwitcherBackend *backend);
    virtual ~RecordMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherRecordAVEventType eventType) override;
    HRESULT STDMETHODCALLTYPE NotifyWorkingSetChange(uint32_t workingSetIndex, BMDSwitcherRecordDiskId diskId) override;
    HRESULT STDMETHODCALLTYPE NotifyDiskAvailability(BMDSwitcherRecordDiskAvailabilityEventType eventType, BMDSwitcherRecordDiskId diskId) override;
    HRESULT STDMETHODCALLTYPE NotifyStatus(BMDSwitcherRecordAVState stateType, BMDSwitcherRecordAVError error) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
};

class StreamMonitor : public IBMDSwitcherStreamRTMPCallback {
public:
    explicit StreamMonitor(BmdSwitcherBackend *backend);
    virtual ~StreamMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherStreamRTMPEventType eventType) override;
    HRESULT STDMETHODCALLTYPE NotifyStatus(BMDSwitcherStreamRTMPState stateType, BMDSwitcherStreamRTMPError error) override;
    HRESULT STDMETHODCALLTYPE NotifyCloudStreamingDestination(BMDSwitcherCloudStreamDestinationConfigurationEventType eventType, uint32_t id) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
};

class DisplayClockMonitor : public IBMDSwitcherDisplayClockCallback {
public:
    explicit DisplayClockMonitor(BmdSwitcherBackend *backend);
    virtual ~DisplayClockMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherDisplayClockEventType eventType) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
};

class MultiViewMonitor : public IBMDSwitcherMultiViewCallback {
public:
    MultiViewMonitor(BmdSwitcherBackend *backend, int mvIndex);
    virtual ~MultiViewMonitor() = default;
    void detach() { m_backend = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Notify(BMDSwitcherMultiViewEventType eventType, int32_t window) override;

private:
    std::atomic<ULONG> m_refCount{1};
    std::atomic<BmdSwitcherBackend*> m_backend{nullptr};
    int m_mvIndex = 1;
};

#endif
