#pragma once
/**
 * Notification Client — WASAPI endpoint notification implementation.
 *
 * Implements IMMNotificationClient to receive real-time OS events when
 * audio devices are connected, disconnected, removed, or default endpoint changes.
 */

#ifndef AURAMINUS_NOTIFICATION_CLIENT_H
#define AURAMINUS_NOTIFICATION_CLIENT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <functional>
#include <string>
#include <atomic>

namespace auraminus {

enum class DeviceEventType {
    Added,
    Removed,
    StateChanged,
    DefaultChanged
};

using DeviceEventCallback = std::function<void(
    DeviceEventType type,
    const std::wstring& device_id,
    DWORD state_or_role
)>;

class NotificationClient : public IMMNotificationClient {
public:
    NotificationClient(DeviceEventCallback callback);
    virtual ~NotificationClient() = default;

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IMMNotificationClient methods
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) override;
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

private:
    std::atomic<ULONG> ref_count_{1};
    DeviceEventCallback callback_;
};

} // namespace auraminus

#endif // AURAMINUS_NOTIFICATION_CLIENT_H
