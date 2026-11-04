# Redmi 6 Pro findings · 2026-11-04

Evidence: user-supplied v0.2 report from Xiaomi Redmi 6 Pro, Android 11 / API 30. Times are seconds since phone boot, not wall-clock dates.

| Time | Observation |
|---|---|
| 30598–30599 | Keep-stream-open experiment began. Stone Beam's track #408 matched; both Stone Beam and trüke were A2DP-connected, only Stone Beam reported playing. |
| 30631 | Track #408 changed to actual #2 (speaker), then actual #428 (trüke). Both mismatches were muted. Stone Beam's audio endpoint disappeared. |
| 30631 | A new track for exposed trüke endpoint #428 opened and matched. |
| 30632 | Both Bluetooth profiles remained connected; only trüke reported playing. A trüke-named track returned write error -6 and v0.2 stopped the session. |
| 30647–30648 | Restart succeeded on #428 and the speaker. Both Bluetooth devices remained connected, only trüke reported playing. |

The old and new trüke IDs share a product name. v0.2's error text did not include the requested ID, so the error cannot confidently be attributed to the new #428 renderer rather than an older track from this report alone. v0.3 includes IDs in renderer failures.

Conclusion: two Bluetooth profile connections are possible on this device. The attempted open-track preservation did not retain Stone Beam's independent audio route when the active device changed. There is no evidence of simultaneous sound from both devices; no acoustic observation was supplied with the report. This is consistent with the inspected LineageOS single-active-peer implementation, not proof of all Android vendor behavior.

`-6` is [AudioTrack.ERROR_DEAD_OBJECT](https://developer.android.com/reference/android/media/AudioTrack#ERROR_DEAD_OBJECT): the track is invalid and needs recreation. The error does not identify why it was invalidated. It is separate from the already-observed routing transition.

The report also exposed a virtual remote-submix (type 25) being selected for rendering and old diagnostic states being overwritten at restart. v0.3 filters selectable outputs, preserves unavailable-output history, and isolates failed renderers during the tone experiment. Restart is required to recreate a failed track. Normal playback/capture failure behavior is unchanged. These changes improve diagnostic reliability, not Bluetooth route availability.
