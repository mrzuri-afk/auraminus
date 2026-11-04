/**
 * Device Manager — WASAPI device enumeration implementation.
 */

#include "device_manager.h"
#include <combaseapi.h>
#include <string>

// PKEY_Device_FriendlyName may not be defined in MinGW headers
// Define it if missing
#ifndef PKEY_Device_FriendlyName_DEFINED
#include <initguid.h>
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName_WS,
    0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
#endif

namespace auraminus {

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager() {
    shutdown();
}

HRESULT DeviceManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return S_OK;

    // Initialize COM for this thread (MTA for audio)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        // RPC_E_CHANGED_MODE means COM is already initialized with a different
        // threading model — that's OK, we can still use it
        return hr;
    }

    // Create the device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator_)
    );

    if (FAILED(hr)) {
        return hr;
    }

    // Register for device hotplug / removal / state notifications
    notification_client_ = new NotificationClient(
        [this](DeviceEventType type, const std::wstring& id, DWORD val) {
            on_notification(type, id, val);
        }
    );
    enumerator_->RegisterEndpointNotificationCallback(notification_client_);

    initialized_ = true;

    // Do initial enumeration
    return refresh_devices_locked();
}

void DeviceManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (enumerator_) {
        if (notification_client_) {
            enumerator_->UnregisterEndpointNotificationCallback(notification_client_);
            notification_client_->Release();
            notification_client_ = nullptr;
        }
        enumerator_->Release();
        enumerator_ = nullptr;
    }

    devices_.clear();
    initialized_ = false;
}

void DeviceManager::on_notification(DeviceEventType type, const std::wstring& id, DWORD /*val*/) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) {
            refresh_devices_locked();
        }
    }
    if (event_callback_) {
        event_callback_(type, id);
    }
}

HRESULT DeviceManager::refresh_devices() {
    std::lock_guard<std::mutex> lock(mutex_);
    return refresh_devices_locked();
}

HRESULT DeviceManager::refresh_devices_locked() {
    if (!initialized_ || !enumerator_) {
        return E_FAIL;
    }

    devices_.clear();
    default_device_id_.clear();

    // Get default render device ID
    IMMDevice* default_device = nullptr;
    HRESULT hr = enumerator_->GetDefaultAudioEndpoint(
        eRender, eMultimedia, &default_device
    );
    if (SUCCEEDED(hr) && default_device) {
        LPWSTR default_id = nullptr;
        if (SUCCEEDED(default_device->GetId(&default_id))) {
            default_device_id_ = default_id;
            CoTaskMemFree(default_id);
        }
        default_device->Release();
    }

    // Enumerate all active render devices
    IMMDeviceCollection* collection = nullptr;
    hr = enumerator_->EnumAudioEndpoints(
        eRender,
        DEVICE_STATE_ACTIVE,
        &collection
    );
    if (FAILED(hr)) return hr;

    UINT count = 0;
    hr = collection->GetCount(&count);
    if (FAILED(hr)) {
        collection->Release();
        return hr;
    }

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        hr = collection->Item(i, &device);
        if (FAILED(hr) || !device) continue;

        DeviceInfo info;

        // Get device ID
        LPWSTR device_id = nullptr;
        if (SUCCEEDED(device->GetId(&device_id))) {
            info.id = device_id;
            CoTaskMemFree(device_id);
        }

        // Get friendly name from property store
        IPropertyStore* props = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT name_var;
            PropVariantInit(&name_var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &name_var))) {
                if (name_var.vt == VT_LPWSTR && name_var.pwszVal) {
                    info.name = name_var.pwszVal;
                }
            }
            PropVariantClear(&name_var);
            props->Release();
        }

        // Fallback name
        if (info.name.empty()) {
            info.name = L"Unknown Device";
        }

        // Check state
        DWORD state = 0;
        if (SUCCEEDED(device->GetState(&state))) {
            info.is_active = (state == DEVICE_STATE_ACTIVE);
        }

        info.is_default = (info.id == default_device_id_);
        info.supports_render = true;

        devices_.push_back(std::move(info));
        device->Release();
    }

    collection->Release();
    return S_OK;
}

std::vector<DeviceInfo> DeviceManager::get_devices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

std::wstring DeviceManager::get_default_device_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return default_device_id_;
}

bool DeviceManager::is_device_available(const std::wstring& device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& dev : devices_) {
        if (dev.id == device_id && dev.is_active) {
            return true;
        }
    }
    return false;
}

HRESULT DeviceManager::get_device(const std::wstring& device_id, IMMDevice** out_device) {
    if (!out_device) return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !enumerator_) return E_FAIL;

    return enumerator_->GetDevice(device_id.c_str(), out_device);
}

HRESULT DeviceManager::get_default_device(IMMDevice** out_device) {
    if (!out_device) return E_INVALIDARG;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !enumerator_) return E_FAIL;

    return enumerator_->GetDefaultAudioEndpoint(
        eRender, eMultimedia, out_device
    );
}

} // namespace auraminus
