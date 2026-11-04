/**
 * Auraminus Audio Engine — Public C API
 *
 * This header defines the stable C ABI interface between the C++ audio engine
 * and the C# WinUI 3 application. All types are C-compatible.
 *
 * Rules:
 *   - No C++ types (std::string, std::vector, etc.) across this boundary
 *   - No C++ exceptions across this boundary
 *   - All functions return AuraminusResult error codes
 *   - Strings are null-terminated UTF-8 char*
 */

#ifndef AURAMINUS_ENGINE_H
#define AURAMINUS_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AURAMINUS_EXPORTS
    #define AURAMINUS_API __declspec(dllexport)
#else
    #define AURAMINUS_API __declspec(dllimport)
#endif

#include <stdint.h>

/* ============================================================
 * Error Codes
 * ============================================================ */

typedef enum AuraminusResult {
    AURAMINUS_OK                  = 0,
    AURAMINUS_ERROR_INVALID_ARG   = 1,
    AURAMINUS_ERROR_NOT_FOUND     = 2,
    AURAMINUS_ERROR_DISCONNECTED  = 3,
    AURAMINUS_ERROR_FORMAT        = 4,
    AURAMINUS_ERROR_AUDIO_CLIENT  = 5,
    AURAMINUS_ERROR_BUFFER        = 6,
    AURAMINUS_ERROR_ALREADY_RUNNING = 7,
    AURAMINUS_ERROR_NOT_RUNNING   = 8,
    AURAMINUS_ERROR_COM           = 9,
    AURAMINUS_ERROR_UNKNOWN       = 99
} AuraminusResult;

/* ============================================================
 * Data Structures
 * ============================================================ */

/**
 * Audio device information.
 * All strings are null-terminated UTF-8.
 */
typedef struct AuraminusDeviceInfo {
    char id[256];           /* Windows endpoint device ID */
    char name[256];         /* Friendly display name */
    int32_t is_default;     /* 1 if default device, 0 otherwise */
    int32_t is_active;      /* 1 if device state is active */
    int32_t supports_render; /* 1 if device supports audio output */
} AuraminusDeviceInfo;

/**
 * Engine status information.
 */
typedef struct AuraminusEngineStatus {
    int32_t is_running;
    int32_t capture_sample_rate;
    int32_t capture_channels;
    int32_t output_count;
    uint64_t frames_captured;
    uint64_t buffer_underruns;
    uint64_t buffer_overruns;
} AuraminusEngineStatus;

/**
 * Opaque engine handle.
 */
typedef void* AuraminusEngine;

/* ============================================================
 * Lifecycle
 * ============================================================ */

/**
 * Create a new audio engine instance.
 * @param out_engine  Receives the engine handle on success.
 * @return AURAMINUS_OK on success.
 */
AURAMINUS_API AuraminusResult auraminus_engine_create(AuraminusEngine* out_engine);

/**
 * Destroy an engine instance and release all resources.
 * Stops playback if running.
 * @param engine  Engine handle (may be NULL).
 */
AURAMINUS_API void auraminus_engine_destroy(AuraminusEngine engine);

/**
 * Start audio capture and playback on all added output devices.
 */
AURAMINUS_API AuraminusResult auraminus_engine_start(AuraminusEngine engine);

/**
 * Stop audio capture and playback.
 */
AURAMINUS_API AuraminusResult auraminus_engine_stop(AuraminusEngine engine);

/* ============================================================
 * Device Management
 * ============================================================ */

/**
 * Get available audio output devices.
 * @param engine    Engine handle.
 * @param devices   Caller-allocated array to receive device info.
 * @param capacity  Size of the devices array.
 * @param out_count Receives the number of devices written.
 * @return AURAMINUS_OK on success.
 */
AURAMINUS_API AuraminusResult auraminus_get_devices(
    AuraminusEngine engine,
    AuraminusDeviceInfo* devices,
    int32_t capacity,
    int32_t* out_count
);

/**
 * Refresh the device list (re-enumerate).
 */
AURAMINUS_API AuraminusResult auraminus_refresh_devices(AuraminusEngine engine);

/**
 * Add an output device by its ID.
 */
AURAMINUS_API AuraminusResult auraminus_add_output(
    AuraminusEngine engine,
    const char* device_id
);

/**
 * Remove an output device by its ID.
 */
AURAMINUS_API AuraminusResult auraminus_remove_output(
    AuraminusEngine engine,
    const char* device_id
);

/* ============================================================
 * Per-Device Controls
 * ============================================================ */

/**
 * Set volume for a specific output device.
 * @param volume  0.0 (mute) to 1.0 (full volume).
 */
AURAMINUS_API AuraminusResult auraminus_set_volume(
    AuraminusEngine engine,
    const char* device_id,
    float volume
);

/**
 * Set synchronization delay for a specific output device (0 to 500 ms).
 * @param delay_ms  Delay in milliseconds (0 = no extra delay).
 */
AURAMINUS_API AuraminusResult auraminus_set_device_delay(
    AuraminusEngine engine,
    const char* device_id,
    uint32_t delay_ms
);

/**
 * Get current synchronization delay for a specific output device.
 */
AURAMINUS_API AuraminusResult auraminus_get_device_delay(
    AuraminusEngine engine,
    const char* device_id,
    uint32_t* out_delay_ms
);

/* ============================================================
 * Status & Diagnostics
 * ============================================================ */

/**
 * Get current engine status.
 */
AURAMINUS_API AuraminusResult auraminus_get_status(
    AuraminusEngine engine,
    AuraminusEngineStatus* out_status
);
/**
 * Device event callback type for hotplug/removal notifications.
 * @param device_id   UTF-8 device identifier.
 * @param event_type  0 = Added, 1 = Removed, 2 = StateChanged, 3 = DefaultChanged.
 */
typedef void (*AuraminusDeviceEventCallback)(const char* device_id, int32_t event_type);

/**
 * Register a callback for real-time device hotplug / disconnect / reconnect events.
 */
AURAMINUS_API AuraminusResult auraminus_set_device_event_callback(
    AuraminusEngine engine,
    AuraminusDeviceEventCallback callback
);

#ifdef __cplusplus
}
#endif

#endif /* AURAMINUS_ENGINE_H */
