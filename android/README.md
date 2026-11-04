# Auraminus for Android

Sound, in good company.

A native Android listening room for testing exposed audio outputs and sharing capture-eligible app playback. Built with Java 17, platform Views, AudioRecord and AudioTrack. Android 10 or later is required.

## What's included

- Auraminus interface, launcher mark and foreground-session notification.
- Test pulse and consent-based app-audio sharing.
- Independent bounded PCM queues, volume and 0–500 ms added delay per requested output.
- Saved selection, gain, delay and original playback source across app restarts.
- Actual-route checks that mute redirected streams, with copyable diagnostics.
- Optional keep-stream-open experiment under **Device support & diagnostics**.

Android controls active media routing. Two paired Bluetooth devices are not necessarily two simultaneous outputs. Auraminus cannot unlock a hidden second route, force OEM Dual Audio, or provide Auracast on unsupported hardware. A matched route is an Android observation, not proof of acoustic alignment.

## Sharing update 1.0.1

Phone media volume now controls Android media, including original playback. Per-output gain controls only the Auraminus stream. The original output's copy controls are hidden during sharing because it is not rendered a second time.

Existing exposed outputs can be checked or unchecked while sharing; removing the last copy ends the session. Refreshing the device list preserves the renderer's live gain state. Input-only connection events no longer stop sharing. A physical output change still ends capture so the original playback output can be confirmed before restarting.

Capture queues and requested buffers are smaller to reduce avoidable lag. Use **Reset added delays to 0 ms** before comparing normal playback with sharing. Capture and Bluetooth latency remain: this update does not establish echo-free sound or guarantee that Android will activate multiple Bluetooth routes.

## Use

1. Connect speakers or headphones in Android settings.
2. Select the outputs exposed by your phone, then use **Test outputs**. Listen on each device.
3. For **Share app audio**, select the original output already playing your source app and select at least one additional output. Grant Android's audio and capture consent.
4. Use **Phone media volume** for the original sound and **Auraminus copy gain** for each copied stream. Start with added delay at 0 ms. Android controls the source app's original sound. Protected audio, calls and apps that disallow capture are outside scope; the app does not capture microphone input.
5. Mix changes save automatically. **Save this mix** saves immediately. Closing the screen keeps an active foreground session running; use **Stop session** or the notification's Stop action to end it.

Saved mixes use output type, product name and an address when available, hashed into a local key. If Android exposes identical names without stable addresses, devices may share a saved profile. Android output IDs themselves are not persisted. Settings remain on this phone and the renamed app uses its own storage; older WaveSync Android installs are separate.

## Build

Use Android Studio with JDK 17 and Android SDK 35, or run:

```powershell
.\gradlew.bat :mobile:testDebugUnitTest :mobile:lintDebug :mobile:assembleDebug
```

With the workspace-local toolchain:

```powershell
.\tools\bootstrap.ps1
.\tools\build.ps1
```

Bootstrap downloads tools under ignored `.tools/` and leaves Android SDK license acceptance to the interactive installer. The build script runs tests and lint and copies the debug-signed APK to `output/Auraminus-Android-debug.apk`. This APK is for development/testing; publishing requires your own release signing setup.

## Layout

```text
mobile/       Android app module and unit tests
  src/main/   Activity, foreground service, engine, mix storage and resources
  src/test/   PCM buffering/delay, physical-output policy and saved-profile checks
tools/        Toolchain bootstrap and verified APK build
gradle/       Standard Gradle wrapper
docs/         Architecture and recorded handset findings
output/       Generated deliverables (ignored)
```

`.tools`, `.gradle`, `local.properties` and build outputs stay out of Git. No nested upstream repository or old APK bundle is needed.

## Hardware verification

Before a demo, test the pulse on both physical outputs, gain/delay changes, capture consent cancellation and revocation, notification Stop, app rotation, process restart with saved mixes, and disconnect/reconnect. Normal sessions stop when a required output disappears or capture devices change. The opt-in pulse experiment retains tracks and tries newly exposed Bluetooth outputs; it does not force connections. Failed tracks require a restart. Manual delay is not drift correction.

See [architecture](docs/architecture.md) and [recorded handset findings](docs/device-notes.md). A fresh phone test is required for this version; automated tests do not establish simultaneous Bluetooth sound.

## Origin

The routing approach is inspired by [Indal Singh's WaveSync](https://github.com/Indal-Singh/WaveSync). The Android engine is a Java implementation of Android APIs, not a Windows WASAPI port. Original attribution and the upstream MIT notice remain in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
