package dev.auraminus.mobile;

import android.app.*;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.*;
import android.media.projection.*;
import android.os.*;
import java.util.ArrayList;
import java.util.List;

public final class AudioService extends Service {
    public static final String START_TONE = "tone", START_CAPTURE = "capture", STOP = "stop";
    private final Handler main = new Handler(Looper.getMainLooper());
    private AudioEngine engine;
    private MediaProjection projection;
    private AudioManager manager;
    private BluetoothDiagnostics bluetooth;
    private boolean holdTest;
    private final Runnable inspectBluetooth = new Runnable() {
        @Override public void run() { bluetooth.refresh(); main.postDelayed(this, 1000); }
    };
    private final List<Integer> requiredIds = new ArrayList<>();
    private final MediaProjection.Callback projectionCallback = new MediaProjection.Callback() {
        @Override public void onStop() { finish("Capture permission ended. Start again to grant access."); }
    };
    private final AudioDeviceCallback devices = new AudioDeviceCallback() {
        @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] removed) {
            for (AudioDeviceInfo d : removed) Session.log("Output removed #" + d.getId() + " " + d.getProductName());
            if (holdTest) {
                Session.status = "Route experiment continues • removed tracks stay open; redirected audio is muted";
                return;
            }
            for (AudioDeviceInfo d : removed) if (requiredIds.contains(d.getId())) {
                finish("An active device disconnected. Check outputs and restart."); return;
            }
        }
        @Override public void onAudioDevicesAdded(AudioDeviceInfo[] added) {
            if (registering) { registering = false; return; }
            for (AudioDeviceInfo d : added) Session.log("Output added #" + d.getId() + " " + d.getProductName());
            if (holdTest && engine != null) {
                for (AudioDeviceInfo d : added) if (d.isSink() && d.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP) {
                    Session.Output output = Session.outputs.computeIfAbsent(d.getId(), id -> new Session.Output(id, d.getProductName() + " · Bluetooth"));
                    output.available = true;
                    output.selected = true;
                    try { engine.addTestOutput(d, output); }
                    catch (RuntimeException error) { output.route = "Could not open extra track: " + error.getMessage(); Session.log(output.route); }
                }
            }
            // Capture's original route may change when plugging in a device.
            if (projection != null && Session.running) finish("Audio devices changed. Recheck the original playback output and restart.");
        }
    };
    @Override public void onCreate() {
        super.onCreate();
        manager = getSystemService(AudioManager.class);
        bluetooth = new BluetoothDiagnostics(this);
        getSystemService(NotificationManager.class).createNotificationChannel(
                new NotificationChannel("audio", "Auraminus audio sessions", NotificationManager.IMPORTANCE_LOW));
    }
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        Session.starting = false;
        if (intent == null || STOP.equals(intent.getAction())) {
            finish("Session stopped"); return START_NOT_STICKY;
        }
        if (engine != null) return START_NOT_STICKY;
        boolean capture = START_CAPTURE.equals(intent.getAction());
        holdTest = !capture && Session.holdRouteTest;
        try {
            PendingIntent open = PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
            PendingIntent stop = PendingIntent.getService(this, 1, new Intent(this, AudioService.class).setAction(STOP), PendingIntent.FLAG_IMMUTABLE);
            Notification notification = new Notification.Builder(this, "audio")
                    .setSmallIcon(dev.auraminus.mobile.R.drawable.ic_audio)
                    .setContentTitle("Auraminus · " + (capture ? "Playback capture" : "Route test"))
                    .setContentText("Tap to inspect outputs. Stop ends this audio session.")
                    .setContentIntent(open).setOngoing(true)
                    .addAction(new Notification.Action.Builder(null, "Stop", stop).build()).build();
            startForeground(1, notification, capture ? ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION
                    : ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
            if (capture) {
                Intent grant = intent.getParcelableExtra("grant");
                if (grant == null) throw new IllegalStateException("Capture consent is missing");
                projection = getSystemService(MediaProjectionManager.class).getMediaProjection(Activity.RESULT_OK, grant);
                projection.registerCallback(projectionCallback, main);
            }
            ArrayList<Session.Output> selected = new ArrayList<>();
            for (Session.Output output : Session.outputs.values()) {
                if (output.available) output.route = "Not selected this session";
                if (output.selected && output.available) { selected.add(output); requiredIds.add(output.id); }
            }
            if (capture) requiredIds.add(Session.primaryId);
            engine = new AudioEngine(error -> main.post(() -> finish("Audio stopped: " + error)), holdTest);
            Session.frames = 0;
            engine.start(manager, selected, projection, Session.primaryId);
            Session.running = true;
            Session.log("Started " + (capture ? "capture" : holdTest ? "keep-stream-open experiment" : "tone test"));
            main.post(inspectBluetooth);
            Session.status = capture ? "Capturing permitted playback • inspect each route below"
                    : "Playing a soft pulse every second • listen on each earbud";
            // Registration delivers an initial device inventory: ignore that initial added callback.
            registering = true;
            manager.registerAudioDeviceCallback(devices, main);
        } catch (RuntimeException error) { finish("Could not start: " + error.getMessage()); }
        return START_NOT_STICKY;
    }
    private boolean registering;
    private void finish(String reason) {
        Session.status = reason;
        stopSelf();
        cleanup();
    }
    private void cleanup() {
        if (engine != null) Session.log("Session ended: " + Session.status);
        Session.running = false;
        Session.starting = false;
        manager.unregisterAudioDeviceCallback(devices);
        if (engine != null) { engine.stop(); engine = null; }
        main.removeCallbacksAndMessages(null);
        if (projection != null) {
            projection.unregisterCallback(projectionCallback);
            projection.stop(); projection = null;
        }
        requiredIds.clear();
        stopForeground(STOP_FOREGROUND_REMOVE);
    }
    @Override public void onDestroy() { cleanup(); bluetooth.close(); super.onDestroy(); }
    @Override public IBinder onBind(Intent intent) { return null; }
}
