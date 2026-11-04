#pragma once
/**
 * Device Manager — WASAPI device enumeration and monitoring.
 *
 * Wraps IMMDeviceEnumerator to provide:
 *   - Enumerate audio render endpoints
 *   - Get device info (ID, name, state, default)
 *   - Detect device arrival/removal (future: IMMNotificationClient)
 */

#ifndef AURAMINUS_DEVICE_MANAGER_H
#define AURAMINUS_DEVICE_MANAGER_H

#include <string>
#include <vector>
#include <mutex>

// Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <functional>
#include "notification_client.h"

namespace auraminus {

/**
 * Internal device info structure (C++ side).
 */
struct DeviceInfo {
    std::wstring id;
    std::wstring name;
    bool is_default = false;
    bool is_active = false;
    bool supports_render = true;
};

/**
 * Manages audio device enumeration via Windows Core Audio.
 * Thread-safe for enumeration queries.
 */
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // Non-copyable
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    /**
     * Initialize COM and create device enumerator.
     * Must be called before any other method.
     */
    HRESULT initialize();

    /**
     * Shutdown and release COM resources.
     */
    void shutdown();

    /**
     * Enumerate all active audio render devices.
     * Updates the internal device list.
     */
    HRESULT refresh_devices();

    /**
     * Get the current list of devices.
     * Thread-safe (locks internally).
     */
    std::vector<DeviceInfo> get_devices() const;

    /**
     * Get the default audio render endpoint device ID.
     */
    std::wstring get_default_device_id() const;

    /**
     * Check if a device ID exists and is active.
     */
    bool is_device_available(const std::wstring& device_id) const;

    /**
     * Get an IMMDevice pointer for a given device ID.
     * Caller must Release() the returned pointer.
     */
    HRESULT get_device(const std::wstring& device_id, IMMDevice** out_device);

    /**
     * Get the default render endpoint.
     * Caller must Release() the returned pointer.
     */
    HRESULT get_default_device(IMMDevice** out_device);

    /**
     * Set callback for real-time device hot-plug/removal/state events.
     */
    void set_event_callback(std::function<void(DeviceEventType, const std::wstring&)> cb) {
        event_callback_ = std::move(cb);
    }

private:
    HRESULT refresh_devices_locked();
    void on_notification(DeviceEventType type, const std::wstring& id, DWORD val);

    NotificationClient* notification_client_ = nullptr;
    std::function<void(DeviceEventType, const std::wstring&)> event_callback_;

    IMMDeviceEnumerator* enumerator_ = nullptr;
    std::vector<DeviceInfo> devices_;
    std::wstring default_device_id_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace auraminus

#endif // AURAMINUS_DEVICE_MANAGER_H
