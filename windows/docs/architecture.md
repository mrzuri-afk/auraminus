# Architecture

Auraminus separates the desktop experience from the native audio pipeline.

## Desktop

`desktop/Shell` owns the window, session controls, telemetry and system tray. `desktop/Mix` contains observable endpoint state and JSON persistence; mix writes debounce for 600 ms and disconnected endpoints remain saved. `desktop/Audio` contains the managed engine interface, error translation, native structs and P/Invoke declarations. Device callbacks dispatch to the UI thread. Status polls every 500 ms while running; playback never starts automatically on launch.

WPF starts `Shell/MainWindow.xaml`. Icons are embedded from `Resources` and copied beside the executable for the tray. The only shipped app project is `desktop/Auraminus.csproj`.

## Native pipeline

```text
Windows app audio
  -> default playback endpoint (direct output)
  -> WASAPI loopback capture
  -> independent SPSC buffer per secondary endpoint
  -> independent WASAPI render thread per secondary endpoint
```

`engine/runtime` owns engine lifetime, selected outputs, routing refreshes and the C boundary. `engine/platform/windows` owns WASAPI capture/render and endpoint notifications. `engine/memory` owns the lock-free ring buffers. The native capture/render algorithms and threading approach are retained from WaveSync, as documented in the README.

The default endpoint bypasses secondary rendering to prevent feedback, so Auraminus gain and alignment controls apply only to secondary outputs.

`engine/public/auraminus_engine.h` defines the `auraminus_*` C API. Managed declarations load `AuraminusEngine.dll`. Both layers must be rebuilt together; WaveSync DLLs use different entry points.

## Build and checks

CMake builds the native library and the buffering check under `output/native`. `Directory.Build.props` directs managed outputs and intermediates to `output/managed` and `output/obj`. The desktop project copies the freshly built native DLL. `checks/settings` verifies saved mixes; `checks/interop` exercises the managed/native boundary.

`tools/package.ps1` publishes the desktop app under `output/portable`, includes README and LICENSE, and optionally calls `tools/windows/setup.iss`. Installer identity is independent from the upstream application.
