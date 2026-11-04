/**
 * Notification Client — Implementation
 */

#include "notification_client.h"

namespace auraminus {

NotificationClient::NotificationClient(DeviceEventCallback callback)
    : callback_(std::move(callback))
{
}

STDMETHODIMP NotificationClient::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (riid == __uuidof(IUnknown)) {
        *ppv = static_cast<IUnknown*>(this);
    } else if (riid == __uuidof(IMMNotificationClient)) {
        *ppv = static_cast<IMMNotificationClient*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) NotificationClient::AddRef() {
    return ref_count_.fetch_add(1, std::memory_order_relaxed) + 1;
}

STDMETHODIMP_(ULONG) NotificationClient::Release() {
    ULONG count = ref_count_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP NotificationClient::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
    if (callback_ && pwstrDeviceId) {
        callback_(DeviceEventType::StateChanged, pwstrDeviceId, dwNewState);
    }
    return S_OK;
}

STDMETHODIMP NotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
    if (callback_ && pwstrDeviceId) {
        callback_(DeviceEventType::Added, pwstrDeviceId, 0);
    }
    return S_OK;
}

STDMETHODIMP NotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
    if (callback_ && pwstrDeviceId) {
        callback_(DeviceEventType::Removed, pwstrDeviceId, 0);
    }
    return S_OK;
}

STDMETHODIMP NotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) {
    // Only care about rendering endpoints for multimedia/console role
    if (flow == eRender && (role == eMultimedia || role == eConsole)) {
        if (callback_) {
            std::wstring id = pwstrDefaultDeviceId ? pwstrDefaultDeviceId : L"";
            callback_(DeviceEventType::DefaultChanged, id, static_cast<DWORD>(role));
        }
    }
    return S_OK;
}

STDMETHODIMP NotificationClient::OnPropertyValueChanged(LPCWSTR /*pwstrDeviceId*/, const PROPERTYKEY /*key*/) {
    return S_OK;
}

} // namespace auraminus
