# Android architecture

`MainActivity` builds the listening-room interface and enumerates selectable physical media outputs. Session state lives in `Session`; transient endpoint IDs and diagnostics stay in memory. `MixStore` saves hashed endpoint identities and bounded `MixProfile` values in app-local preferences. Writes debounce for 450 ms and flush when the screen stops. The source selection is restored when its endpoint is available again.

`AudioService` owns the foreground notification, projection consent token, device callbacks and `AudioEngine`. Starting is guarded against duplicate requests. Capture revocation and normal required-device removal stop the session. The service is not automatically restarted with a stale projection token.

```text
Test pulse OR permitted AudioRecord playback capture
  -> independent bounded PCM queue per output
  -> stereo delay history
  -> AudioTrack with requested output
  -> actual-route observation; mismatch muted
```

`PcmQueue` drops the oldest block on overflow and records the drop. `PcmDelay` applies a bounded stereo offset. The engine checks the observed route before unmuting, excludes its own UID from capture and disallows capture of its renderer tracks to reduce feedback risk. Actual device clocks and Bluetooth routing remain outside the application's control.

The optional keep-stream-open pulse experiment adds newly exposed Bluetooth outputs and isolates failed renderers. It is not used for playback capture. It preserves diagnostic evidence rather than reporting simulated success.

Playback capture follows [Android's capture requirements](https://developer.android.com/media/platform/av-capture), including permission, user consent, compatible source usage and capture policy. Foreground service types follow [Android's service requirements](https://developer.android.com/develop/background-work/services/fgs/service-types). No root, hidden API, pairing automation, microphone recording or audio file export is used.

The `mobile` module keeps Java sources and matching tests under `dev.auraminus.mobile`. `tools/build.ps1` runs unit tests, Android lint and APK assembly, then creates the named debug APK under ignored `output/`.
