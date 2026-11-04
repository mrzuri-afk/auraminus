package dev.auraminus.mobile;

import android.media.*;
import android.media.projection.MediaProjection;
import android.os.Process;
import android.os.SystemClock;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/** Public-API routing experiment. Device preferences are requests, never guarantees. */
public final class AudioEngine {
    static final int RATE = 48000, SAMPLES = 960; // 10 ms, stereo PCM16
    private final List<Renderer> outputs = new CopyOnWriteArrayList<>();
    private final AtomicBoolean active = new AtomicBoolean();
    private final Consumer<String> failure;
    private final boolean isolateOutputFailures;
    private AudioRecord recorder;
    private Thread producer;
    private boolean mirrorCapture;
    public AudioEngine(Consumer<String> failure, boolean isolateOutputFailures) {
        this.failure = failure;
        this.isolateOutputFailures = isolateOutputFailures;
    }

    public void start(AudioManager manager, List<Session.Output> selected,
                      MediaProjection projection, int primary) {
        mirrorCapture = projection != null;
        try {
            for (Session.Output state : selected) {
                if (projection != null && state.id == primary) {
                    state.route = "Original audio • controlled by Android"; continue;
                }
                AudioDeviceInfo device = null;
                for (AudioDeviceInfo d : manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS))
                    if (d.getId() == state.id && OutputPolicy.selectable(d.getType())) device = d;
                if (device == null) throw new IllegalStateException("Output disconnected: " + state.name);
                outputs.add(new Renderer(device, state));
            }
            if (outputs.isEmpty()) throw new IllegalStateException("Select at least one additional output");
            if (projection != null) {
                AudioPlaybackCaptureConfiguration config = new AudioPlaybackCaptureConfiguration.Builder(projection)
                        .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                        .addMatchingUsage(AudioAttributes.USAGE_GAME)
                        .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                        .excludeUid(Process.myUid()).build();
                int min = AudioRecord.getMinBufferSize(RATE, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT);
                if (min <= 0) throw new IllegalStateException("48 kHz stereo capture is unavailable");
                recorder = new AudioRecord.Builder().setAudioPlaybackCaptureConfig(config)
                        .setAudioFormat(new AudioFormat.Builder().setSampleRate(RATE)
                                .setChannelMask(AudioFormat.CHANNEL_IN_STEREO).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                        .setBufferSizeInBytes(Math.max(min, SAMPLES * 2)).build();
                if (recorder.getState() != AudioRecord.STATE_INITIALIZED) throw new IllegalStateException("Capture initialization failed");
                recorder.startRecording();
            }
            active.set(true);
            for (Renderer r : outputs) r.start();
            producer = new Thread(this::produce, "Auraminus-source");
            producer.start();
        } catch (SecurityException e) {
            stop(); throw new IllegalStateException("Audio capture permission was denied or revoked", e);
        } catch (RuntimeException e) { stop(); throw e; }
    }

    private void produce() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
        short[] block = new short[SAMPLES];
        long frame = 0, deadline = System.nanoTime();
        try {
            while (active.get()) {
                int count;
                if (recorder != null) {
                    count = recorder.read(block, 0, block.length, AudioRecord.READ_BLOCKING);
                    if (count < 0) throw new IllegalStateException("Capture read failed: " + count);
                    if (count == 0) continue;
                    if (count % 2 != 0) throw new IllegalStateException("Incomplete stereo capture frame");
                } else {
                    count = block.length;
                    for (int i = 0; i < count; i += 2, frame++) {
                        // A soft 880 Hz pulse every second makes inter-device echo audible.
                        int position = (int)(frame % RATE);
                        double envelope = position < 4800 ? Math.pow(Math.sin(Math.PI * position / 4800.0), 2) : 0;
                        short sample = (short)(Math.sin(2 * Math.PI * 880 * frame / RATE) * 5000 * envelope);
                        block[i] = block[i + 1] = sample;
                    }
                    deadline += 10_000_000;
                    long wait = deadline - System.nanoTime();
                    if (wait > 0) SystemClock.sleep(Math.max(1, wait / 1_000_000));
                    else if (wait < -100_000_000) deadline = System.nanoTime();
                }
                int peak = 0;
                for (int i = 0; i < count; i++) peak = Math.max(peak, Math.abs((int)block[i]));
                Session.peak = peak / 32768f;
                Session.frames += count / 2;
                for (Renderer output : outputs)
                    if (output.healthy && output.queue.offer(block, count)) output.state.droppedBlocks++;
            }
        } catch (RuntimeException e) { if (active.get()) failure.accept(e.getMessage()); }
    }

    public void stop() {
        active.set(false);
        if (recorder != null) { try { recorder.stop(); } catch (IllegalStateException ignored) {} }
        join(producer);
        for (Renderer output : outputs) output.stop();
        outputs.clear();
        if (recorder != null) { recorder.release(); recorder = null; }
        Session.peak = 0;
    }
    /** Main-thread-only experimental addition while the tone producer continues. */
    public void addTestOutput(AudioDeviceInfo device, Session.Output state) {
        if (!active.get() || recorder != null) return;
        for (Renderer r : outputs) if (r.state.id == state.id) return;
        Renderer renderer = new Renderer(device, state);
        outputs.add(renderer);
        renderer.start();
        Session.log("Opened extra test track for #" + state.id + " " + state.name);
    }
    /** Reconcile requested mirrors on the service main thread; the producer keeps its capture token. */
    public void updateOutputs(AudioManager manager, List<Session.Output> selected, int primary) {
        for (Renderer renderer : outputs) {
            boolean wanted = false;
            for (Session.Output state : selected)
                if (state.id == renderer.state.id && (!mirrorCapture || state.id != primary)) wanted = true;
            if (!wanted) {
                outputs.remove(renderer); renderer.stop(); renderer.state.route = "Not selected";
            }
        }
        for (Session.Output state : selected) {
            if (mirrorCapture && state.id == primary) continue;
            boolean present = false;
            for (Renderer renderer : outputs) if (renderer.state.id == state.id) present = true;
            if (present) continue;
            try {
                AudioDeviceInfo device = null;
                for (AudioDeviceInfo candidate : manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS))
                    if (candidate.getId() == state.id && OutputPolicy.selectable(candidate.getType())) device = candidate;
                if (device == null) throw new IllegalStateException("Output is no longer exposed by Android");
                Renderer renderer = new Renderer(device, state);
                outputs.add(renderer); renderer.start();
            } catch (RuntimeException error) {
                state.route = "Could not enable output: " + error.getMessage();
                Session.log(state.route);
                Session.status = "An output could not be enabled; inspect its route below";
            }
        }
    }
    private static void join(Thread thread) {
        if (thread == null || thread == Thread.currentThread()) return;
        try { thread.join(2000); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }

    private final class Renderer {
        final Session.Output state;
        final AudioTrack track;
        final PcmQueue queue = new PcmQueue(mirrorCapture ? 3 : 12, SAMPLES);
        final PcmDelay delay = new PcmDelay(RATE);
        Thread worker;
        int lastRouteId = Integer.MIN_VALUE;
        volatile boolean healthy = true;
        Renderer(AudioDeviceInfo device, Session.Output state) {
            this.state = state;
            int min = AudioTrack.getMinBufferSize(RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
            if (min <= 0) throw new IllegalStateException("48 kHz stereo playback is unavailable");
            track = new AudioTrack.Builder()
                    .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .setAllowedCapturePolicy(AudioAttributes.ALLOW_CAPTURE_BY_NONE).build())
                    .setAudioFormat(new AudioFormat.Builder().setSampleRate(RATE)
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
                    .setBufferSizeInBytes(Math.max(min, SAMPLES * (mirrorCapture ? 2 : 4)))
                    .setPerformanceMode(mirrorCapture ? AudioTrack.PERFORMANCE_MODE_LOW_LATENCY : AudioTrack.PERFORMANCE_MODE_NONE)
                    .setTransferMode(AudioTrack.MODE_STREAM).build();
            if (track.getState() != AudioTrack.STATE_INITIALIZED || !track.setPreferredDevice(device)) {
                track.release(); throw new IllegalStateException("Android rejected output: " + state.name);
            }
            track.setVolume(0); // Route must be observed before making sound.
        }
        void start() {
            state.droppedBlocks = 0;
            worker = new Thread(() -> {
                Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);
                short[] input = new short[SAMPLES], rendered = new short[SAMPLES];
                try {
                    track.play();
                    while (active.get() && healthy) {
                        int count = queue.take(input);
                        if (count < 0) break;
                        AudioDeviceInfo actual = track.getRoutedDevice();
                        boolean matched = actual != null && actual.getId() == state.id;
                        int actualId = actual == null ? -1 : actual.getId();
                        if (actualId != lastRouteId) {
                            lastRouteId = actualId;
                            Session.log("Track requested #" + state.id + " → actual #" + actualId + (matched ? " matched" : " muted"));
                        }
                        state.route = actual == null ? "Waiting for Android route…"
                                : matched ? "Route matched • " + actual.getProductName() + " (#" + actual.getId() + ")"
                                : "MUTED • Android routed to " + actual.getProductName() + " (#" + actual.getId() + ")";
                        if (track.setVolume(matched ? state.gain : 0) != AudioTrack.SUCCESS)
                            throw new IllegalStateException("Android rejected stream volume");
                        delay.process(input, rendered, count, state.delayMs);
                        int offset = 0;
                        while (offset < count && active.get() && healthy) {
                            int written = track.write(rendered, offset, count - offset, AudioTrack.WRITE_BLOCKING);
                            if (written <= 0) throw new IllegalStateException(written == AudioTrack.ERROR_DEAD_OBJECT
                                    ? "AudioTrack invalidated (ERROR_DEAD_OBJECT, -6); restart to recreate this track"
                                    : "Output write failed: " + written);
                            offset += written;
                        }
                    }
                } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
                catch (RuntimeException e) {
                    boolean reportFailure = active.get() && healthy;
                    healthy = false;
                    queue.close();
                    try { track.pause(); track.flush(); } catch (IllegalStateException ignored) {}
                    if (reportFailure) {
                        String error = "Output #" + state.id + " " + state.name + ": " + e.getMessage();
                        state.route = "FAILED • " + error;
                        Session.log(error);
                        if (isolateOutputFailures) {
                            boolean othersHealthy = false;
                            for (Renderer other : outputs) if (other != this && other.healthy) othersHealthy = true;
                            Session.status = othersHealthy
                                    ? "An output failed • other test tracks continue; inspect the failed output below"
                                    : "No working test tracks • stop/restart or connect another output";
                        } else failure.accept(error);
                    }
                }
            }, "Auraminus-output-" + state.id);
            worker.start();
        }
        void stop() {
            healthy = false;
            queue.close();
            try { track.pause(); track.flush(); } catch (IllegalStateException ignored) {}
            join(worker);
            track.release();
        }
    }
}
