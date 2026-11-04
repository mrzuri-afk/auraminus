# Auraminus Audio Engine — C++ WASAPI Library
cmake_minimum_required(VERSION 3.20)
project(AuraminusEngine VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Sources ─────────────────────────────────────────────────
set(ENGINE_SOURCES
    runtime/exports.cpp
    runtime/session.cpp
    platform/windows/device_manager.cpp
    platform/windows/notification_client.cpp
    platform/windows/loopback_capture.cpp
    platform/windows/output_device.cpp
    memory/ring_buffer.cpp
)

# ── Shared Library ──────────────────────────────────────────
add_library(AuraminusEngine SHARED ${ENGINE_SOURCES})

target_include_directories(AuraminusEngine
    PUBLIC public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(AuraminusEngine PRIVATE AURAMINUS_EXPORTS)

# ── Windows Libraries (WASAPI / Core Audio) ─────────────────
target_link_libraries(AuraminusEngine PRIVATE
    ole32           # COM runtime
    uuid            # COM interface IIDs
    winmm           # Multimedia timers
    avrt            # Real-time thread priority
    ksuser          # Audio format GUIDs
)

# ── Compiler Warnings ───────────────────────────────────────
if(MSVC)
    target_compile_options(AuraminusEngine PRIVATE /W4 /WX-)
else()
    target_compile_options(AuraminusEngine PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(AuraminusEngine PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ── Output ──────────────────────────────────────────────────
set_target_properties(AuraminusEngine PROPERTIES
    PREFIX ""
    OUTPUT_NAME "AuraminusEngine"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# ── Install ─────────────────────────────────────────────────
install(TARGETS AuraminusEngine
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES public/auraminus_engine.h DESTINATION include)

# ── Unit tests ──────────────────────────────────────────────
enable_testing()
add_executable(AuraminusRingBufferTests
    ../checks/buffering/ring_buffer.cpp
    memory/ring_buffer.cpp
)
target_include_directories(AuraminusRingBufferTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} public)
add_test(NAME RingBufferTests COMMAND AuraminusRingBufferTests)
set_target_properties(AuraminusRingBufferTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
