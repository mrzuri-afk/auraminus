/**
 * Auraminus Audio Engine — C ABI Implementation
 *
 * This file implements the public C API defined in auraminus_engine.h.
 * It wraps the C++ Engine class behind a flat C interface.
 *
 * Rules:
 *   - No C++ exceptions escape these functions
 *   - All errors returned as AuraminusResult codes
 *   - Engine handle is an opaque void* to Engine*
 */

#include "public/auraminus_engine.h"
#include "runtime/session.h"

#include <new> // for nothrow

using namespace auraminus;

/* ============================================================
 * Lifecycle
 * ============================================================ */

AURAMINUS_API AuraminusResult auraminus_engine_create(AuraminusEngine* out_engine) {
    if (!out_engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        auto* engine = new (std::nothrow) Engine();
        if (!engine) return AURAMINUS_ERROR_UNKNOWN;

        AuraminusResult result = engine->initialize();
        if (result != AURAMINUS_OK) {
            delete engine;
            return result;
        }

        *out_engine = static_cast<AuraminusEngine>(engine);
        return AURAMINUS_OK;
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API void auraminus_engine_destroy(AuraminusEngine engine) {
    if (!engine) return;

    try {
        auto* eng = static_cast<Engine*>(engine);
        delete eng;
    } catch (...) {
        // Swallow — destructor should not throw
    }
}

AURAMINUS_API AuraminusResult auraminus_engine_start(AuraminusEngine engine) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->start();
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_engine_stop(AuraminusEngine engine) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->stop();
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

/* ============================================================
 * Device Management
 * ============================================================ */

AURAMINUS_API AuraminusResult auraminus_get_devices(
    AuraminusEngine engine,
    AuraminusDeviceInfo* devices,
    int32_t capacity,
    int32_t* out_count
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->get_devices(devices, capacity, out_count);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_refresh_devices(AuraminusEngine engine) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->refresh_devices();
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_add_output(
    AuraminusEngine engine,
    const char* device_id
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->add_output(device_id);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_remove_output(
    AuraminusEngine engine,
    const char* device_id
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->remove_output(device_id);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

/* ============================================================
 * Per-Device Controls
 * ============================================================ */

AURAMINUS_API AuraminusResult auraminus_set_volume(
    AuraminusEngine engine,
    const char* device_id,
    float volume
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->set_volume(device_id, volume);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_set_device_delay(
    AuraminusEngine engine,
    const char* device_id,
    uint32_t delay_ms
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->set_delay(device_id, delay_ms);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_get_device_delay(
    AuraminusEngine engine,
    const char* device_id,
    uint32_t* out_delay_ms
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->get_delay(device_id, out_delay_ms);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

/* ============================================================
 * Status & Diagnostics
 * ============================================================ */

AURAMINUS_API AuraminusResult auraminus_get_status(
    AuraminusEngine engine,
    AuraminusEngineStatus* out_status
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        return static_cast<Engine*>(engine)->get_status(out_status);
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}

AURAMINUS_API AuraminusResult auraminus_set_device_event_callback(
    AuraminusEngine engine,
    AuraminusDeviceEventCallback callback
) {
    if (!engine) return AURAMINUS_ERROR_INVALID_ARG;

    try {
        static_cast<Engine*>(engine)->set_device_event_callback(callback);
        return AURAMINUS_OK;
    } catch (...) {
        return AURAMINUS_ERROR_UNKNOWN;
    }
}
