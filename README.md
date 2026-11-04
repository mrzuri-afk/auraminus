# Auraminus

**One audio source. Multiple outputs.**

Auraminus lets you share what’s playing with someone using a second pair of headphones or a speaker. The Windows app mirrors system audio to additional outputs. The Android app shares playback from apps that allow audio capture.

## How it works

| | Windows | Android |
|---|---|---|
| Audio source | System audio from the default playback device | Playback from a capture-eligible app, with your consent |
| Original output | Keeps playing through the Windows default device | Keeps playing through the phone’s active media output |
| Additional outputs | Select playback devices in Auraminus | Select outputs exposed by the phone |
| Controls | Volume and 0–500 ms of added delay per copy | Phone media volume, plus gain and 0–500 ms of added delay per copy |

Both apps save your mix so you can reuse it the next time you connect your devices.

## Try it on Windows

1. Connect your headphones or speaker and start playing audio.
2. In Windows sound settings, choose the device that should play the original audio.
3. Open Auraminus, select an additional output, and start the session.
4. Adjust the copy’s volume. If it plays ahead of the original, add delay until the two sound aligned.

To build on Windows 10 or 11 x64, install the .NET 9 SDK, CMake 3.20 or later, and a C++20 toolchain. From the repository root, run:

```powershell
cd windows
.\tools\build.ps1
.\run.ps1
```

## Try it on Android

1. Connect your audio devices to a phone running Android 10 or later.
2. Open Auraminus and tap **Test outputs** to hear which devices the phone can route to.
3. Play audio in another app. In Auraminus, select the device already playing that audio as the **original output**, then select an additional output for the copy.
4. Tap **Share app audio** and approve Android’s capture prompt. Use **Phone media volume** for the original playback and **Auraminus copy gain** to adjust the copy. Start with added delay at **0 ms**.

To build the debug APK from the repository root using PowerShell, run:

```powershell
cd android
.\tools\bootstrap.ps1
.\tools\build.ps1
```

The build runs unit tests and Android lint, then writes the APK to `android/output/Auraminus-Android-debug.apk`.

## Device support

On Android, a successful output test confirms that the test tone can play. Sharing also requires the source app to permit playback capture and the phone to keep the selected routes active together. Pairing two Bluetooth devices does not guarantee simultaneous playback.

Capture and Bluetooth can introduce audible delay. The delay control can hold back an output that plays early. It cannot speed up an output that already plays late.

## Project guides

- [Windows setup and architecture](windows/README.md) — C# and WPF desktop app with a native C++ WASAPI engine.
- [Android setup and usage](android/README.md) — Java app using Android playback capture, `AudioRecord`, and `AudioTrack`.
- [Android device notes](android/docs/device-notes.md) — observations from handset testing.

## Credits

The Windows audio engine builds on [WaveSync by Indal Singh](https://github.com/Indal-Singh/WaveSync), licensed under MIT. Auraminus adds a redesigned desktop interface, saved mixes, and a separate Android implementation. See the [third-party notices](android/THIRD_PARTY_NOTICES.md) for attribution.
