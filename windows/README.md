# Auraminus

Sound, in good company.

A Windows desktop audio studio that shares system sound across multiple outputs using WASAPI. Select speakers, adjust secondary-output volume and alignment delay, and save the mix for the next session.

## Project layout

```text
desktop/              WPF application
  Shell/              Window and system tray
  Mix/                Endpoint state and saved mixes
  Audio/              Managed engine services and native bindings
  Resources/          Product icon and vector mark
engine/               Native C++ audio pipeline
  runtime/            Session orchestration and C exports
  platform/windows/   WASAPI capture, rendering and endpoint discovery
  memory/             Per-output SPSC buffering
  public/             C API header
checks/               Buffer, settings and interop verification
tools/                Build, packaging and Windows installer
docs/                 Architecture reference
output/               Generated builds and release packages (ignored)
```

## Build and launch

Requires Windows 10/11 x64, .NET 9 SDK, CMake 3.20+, and a C++20 compiler (MinGW-w64 or Visual Studio 2022 with Desktop development with C++).

```powershell
.\tools\build.ps1
.\run.ps1
```

For portable toolchains:

```powershell
.\tools\build.ps1 -ToolchainDirectory C:\tools\mingw\bin -DotnetDirectory C:\tools\dotnet
```

Use `-CppOnly` or `-CSharpOnly` to build one layer. Build the native engine first. `-Clean` clears the native build cache when switching compilers. Native output is in `output/native`; managed builds and intermediate files are also kept under `output`.

## Listen

1. Play audio in any Windows app.
2. Select secondary speakers in Auraminus, then start the session. Windows plays its default output directly.
3. Adjust secondary volume and delay (0–500 ms) to align devices by ear.
4. Mix changes save automatically to `%LOCALAPPDATA%\Auraminus\routing.json`; **Save mix** saves immediately. Disconnected devices retain their settings.
5. Closing the window keeps playback in the tray. Choose **Exit** in the tray menu to stop and quit.

Windows controls the default output's volume. Auraminus cannot mute or delay that direct playback. Bluetooth latency varies; sample-accurate alignment and zero drift are not guaranteed.

## Verification

```powershell
ctest --test-dir output/native -C Release --output-on-failure
dotnet run --project checks/settings -c Release
dotnet run --project checks/interop -c Release -- --smoke
```

The smoke check enumerates endpoints without starting playback. Omit `--smoke` for the interactive audio console. Before a demo, check physical-device alignment, disconnect/reconnect, default-output changes and tray restoration on your hardware.

## Packaging

```powershell
.\tools\package.ps1 -PublishOnly
```

The self-contained Windows app is published to `output/portable`. Keep its accompanying DLLs beside `Auraminus.exe`. Both build and packaging commands accept portable toolchain paths. To create an installer, install Inno Setup 6, put `ISCC.exe` on PATH, and omit `-PublishOnly`; installers are written to `output/installers`. Close a running build before replacing its files.

See [the architecture reference](docs/architecture.md) for component details.

## Origin and license

Based on [WaveSync by Indal Singh](https://github.com/Indal-Singh/WaveSync), baseline `2a3faea1daa4860b9e746a4b08e20cc08b794fbb`, under the [MIT license](LICENSE).

The WASAPI capture/render pipeline, per-device SPSC buffering, routing worker and endpoint notifications are retained. Auraminus adds the rewritten desktop screen, persistent mixes, validation, revised project layout and packaging. Native algorithms were not reimplemented from scratch. Original Git history and copyright notices are preserved.
