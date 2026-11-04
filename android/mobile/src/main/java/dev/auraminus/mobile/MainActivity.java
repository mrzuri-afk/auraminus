package dev.auraminus.mobile;

import android.Manifest;
import android.app.Activity;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.*;
import android.media.projection.MediaProjectionManager;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import java.util.*;

public final class MainActivity extends Activity {
    private final int ink = Color.rgb(237,245,239), muted = Color.rgb(145,168,157), mint = Color.rgb(166,229,185);
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Map<Integer, TextView> routes = new HashMap<>();
    private final List<View> lockedControls = new ArrayList<>();
    private AudioManager manager;
    private LinearLayout root, deviceList;
    private TextView status, metrics, count, bluetoothStatus;
    private CheckBox holdRoute;
    private BluetoothDiagnostics bluetooth;
    private long lastBluetoothRefresh;
    private Button tone, capture;
    private Spinner primary;
    private AudioDeviceInfo[] available = new AudioDeviceInfo[0];
    private boolean requesting;
    private MixStore mixStore;
    private final Runnable saveMix = () -> { if (mixStore != null) mixStore.save(); };
    private void scheduleSave() { handler.removeCallbacks(saveMix); handler.postDelayed(saveMix, 450); }
    private final Runnable refresh = new Runnable() {
        @Override public void run() {
            status.setText(Session.status);
            metrics.setText(String.format(Locale.US, "48 kHz / stereo    •    %.1f s processed    •    peak %.0f%%",
                    Session.frames / 48000.0, Session.peak * 100));
            boolean enabled = !Session.running && !Session.starting && !requesting;
            tone.setEnabled(enabled); capture.setEnabled(enabled);
            primary.setEnabled(enabled);
            holdRoute.setEnabled(enabled);
            if (bluetooth != null && SystemClock.elapsedRealtime() - lastBluetoothRefresh >= 1000) {
                bluetooth.refresh(); lastBluetoothRefresh = SystemClock.elapsedRealtime();
            }
            bluetoothStatus.setText(Session.bluetooth);
            for (View view : lockedControls) view.setEnabled(enabled);
            for (Map.Entry<Integer, TextView> item : routes.entrySet()) {
                Session.Output o = Session.outputs.get(item.getKey());
                if (o != null) item.getValue().setText(o.droppedBlocks > 0 ? getString(R.string.route_drops, o.route, o.droppedBlocks) : o.route);
            }
            handler.postDelayed(this, 400);
        }
    };
    private final AudioDeviceCallback deviceCallback = new AudioDeviceCallback() {
        @Override public void onAudioDevicesAdded(AudioDeviceInfo[] devices) { rebuildDevices(); }
        @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] devices) { rebuildDevices(); }
    };
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        manager = getSystemService(AudioManager.class);
        mixStore = new MixStore(this);
        requesting = saved != null && saved.getBoolean("capturePending", false);
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.rgb(16,25,25));
        root = column(); root.setPadding(dp(22), dp(24), dp(22), dp(24));
        scroll.addView(root); setContentView(scroll);
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            root.setPadding(dp(22), dp(24) + insets.getSystemWindowInsetTop(), dp(22), dp(24) + insets.getSystemWindowInsetBottom());
            return insets;
        });
        add(root, text("a∿   auraminus", 23, mint), 24);
        add(root, text("YOUR POCKET LISTENING ROOM", 10, muted), 8);
        TextView title = text("Your sound.\nA little closer.", 34, ink);
        title.setTypeface(null, Typeface.BOLD); add(root, title, 8);
        add(root, text("Choose your outputs. Shape the mix. Share the moment.", 14, muted), 24);

        LinearLayout session = card();
        add(session, text("SESSION", 10, mint), 8);
        status = text(Session.status, 18, ink); status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        add(session, status, 8);
        metrics = text("48 kHz · stereo", 11, muted); add(session, metrics, 18);
        tone = button("Test outputs", mint, Color.rgb(16,37,28));
        tone.setOnClickListener(v -> startTone()); add(session, tone, 10);
        capture = button("Share app audio", Color.rgb(41,64,57), ink);
        capture.setOnClickListener(v -> requestCapture()); add(session, capture, 10);
        Button stop = button("Stop session", Color.rgb(41,64,57), ink);
        stop.setOnClickListener(v -> {
            stopService(new Intent(this, AudioService.class));
            Session.status = "Session stopped";
        }); add(session, stop, 0); add(root, session, 24);

        add(root, text("Original audio source", 19, ink), 6);
        add(root, text("For sharing, choose the output already playing your app. Android controls its original sound; Auraminus sends a copy to the other selected outputs.", 13, muted), 8);
        primary = new Spinner(this); primary.setContentDescription("Original playback output"); add(root, primary, 22);
        count = text("OUTPUT DEVICES", 11, mint); add(root, count, 12);
        deviceList = column(); add(root, deviceList, 4);
        Button save = button("Save this mix", Color.rgb(41,64,57), ink);
        save.setOnClickListener(v -> { mixStore.save(); Toast.makeText(this, "Mix saved on this phone", Toast.LENGTH_SHORT).show(); });
        add(root, save, 8);
        add(root, text("Your mix saves automatically. Delay is added alignment time, not a latency measurement.", 12, muted), 20);

        LinearLayout advanced = column(); advanced.setVisibility(View.GONE);
        Button details = button("Device support & diagnostics  +", Color.rgb(24,38,34), ink);
        details.setOnClickListener(v -> {
            boolean show = advanced.getVisibility() != View.VISIBLE;
            advanced.setVisibility(show ? View.VISIBLE : View.GONE);
            details.setText(show ? "Device support & diagnostics  −" : "Device support & diagnostics  +");
        }); add(root, details, 12);
        LinearLayout support = card();
        add(support, text("ABOUT YOUR PHONE'S AUDIO", 11, mint), 8);
        add(support, text("Android decides which outputs can play together. Two paired earbuds may still share one active media route. Auraminus checks the actual route and mutes a redirected stream. A matched route is not proof of simultaneous sound.", 13, ink), 12);
        add(support, text("Sharing uses Android's consent screen and capture-eligible app audio. Protected audio and calls cannot be captured. This app does not record microphone input.", 12, muted), 12);
        Button settings = button("Open Bluetooth settings ↗", Color.rgb(41,64,57), ink);
        settings.setOnClickListener(v -> startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)));
        add(support, settings, 0); add(advanced, support, 12);

        LinearLayout diagnostics = card();
        add(diagnostics, text("CONNECTION SNAPSHOT", 11, mint), 8);
        bluetoothStatus = text(Session.bluetooth, 12, muted); add(diagnostics, bluetoothStatus, 12);
        Button inspect = button("Refresh connections", Color.rgb(41,64,57), ink);
        inspect.setOnClickListener(v -> {
            if (Build.VERSION.SDK_INT >= 31 && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, 10);
            else if (bluetooth != null) bluetooth.refresh();
        }); add(diagnostics, inspect, 12);
        holdRoute = new CheckBox(this); holdRoute.setText(R.string.keep_streams); holdRoute.setTextColor(ink);
        holdRoute.setChecked(Session.holdRouteTest);
        holdRoute.setOnCheckedChangeListener((button, checked) -> Session.holdRouteTest = checked);
        add(diagnostics, holdRoute, 6);
        add(diagnostics, text("Test pulse only. Keeps tracks open and tries newly exposed Bluetooth outputs. This cannot force a second active Bluetooth route.", 12, muted), 12);
        Button report = button("Copy route report", Color.rgb(41,64,57), ink);
        report.setOnClickListener(v -> copyReport()); add(diagnostics, report, 0); add(advanced, diagnostics, 12);
        add(advanced, text("Auraminus Android 1.0 · Routing approach inspired by Indal Singh's WaveSync (MIT).", 11, muted), 12);
        add(root, advanced, 0);
        add(root, text("LOCAL AUDIO. SHARED SPACE.", 10, muted), 0);
        rebuildDevices();
        // Notification permission is requested when the user starts a session.
    }
    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("capturePending", requesting); super.onSaveInstanceState(state);
    }
    @Override protected void onStart() {
        super.onStart(); bluetooth = new BluetoothDiagnostics(this);
        manager.registerAudioDeviceCallback(deviceCallback, handler); handler.post(refresh);
    }
    @Override protected void onStop() {
        mixStore.save(); handler.removeCallbacks(saveMix);
        handler.removeCallbacks(refresh); manager.unregisterAudioDeviceCallback(deviceCallback); super.onStop();
        if (bluetooth != null) { bluetooth.close(); bluetooth = null; }
    }
    private void rebuildDevices() {
        available = Arrays.stream(manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS))
                .filter(d -> OutputPolicy.selectable(d.getType())).toArray(AudioDeviceInfo[]::new);
        Set<Integer> ids = new HashSet<>();
        for (AudioDeviceInfo d : available) ids.add(d.getId());
        for (Session.Output o : Session.outputs.values()) o.available = ids.contains(o.id);
        deviceList.removeAllViews(); routes.clear(); lockedControls.clear();
        ArrayList<String> names = new ArrayList<>(); names.add("Choose original playback output…");
        int selected = 0;
        for (int index = 0; index < available.length; index++) {
            AudioDeviceInfo device = available[index];
            String name = device.getProductName() + " · " + type(device.getType());
            names.add(name);
            if (device.getId() == Session.primaryId) selected = index + 1;
            String key = MixStore.key(device);
            Session.Output output = Session.outputs.get(device.getId());
            if (output == null || (output.preferenceKey != null && !key.equals(output.preferenceKey))) {
                output = new Session.Output(device.getId(), name);
                mixStore.restore(key, output);
                Session.outputs.put(device.getId(), output);
            } else if (output.preferenceKey == null) output.preferenceKey = key;
            final Session.Output endpoint = output;
            if (Session.primaryId == -1 && mixStore.isPrimary(device)) selected = index + 1;
            output.available = true;
            LinearLayout row = card();
            CheckBox choice = new CheckBox(this); choice.setText(name); choice.setTextColor(ink); choice.setTextSize(16);
            choice.setChecked(output.selected); choice.setEnabled(!Session.running); lockedControls.add(choice);
            choice.setOnCheckedChangeListener((b, checked) -> { endpoint.selected = checked; scheduleSave(); }); add(row, choice, 6);
            TextView route = text(output.route, 12, muted); routes.put(output.id, route); add(row, route, 12);
            slider(row, "Volume", (int)(output.gain * 100), 100, "%", value -> { endpoint.gain = value / 100f; scheduleSave(); });
            slider(row, "Added delay", output.delayMs, 500, " ms", value -> { endpoint.delayMs = value; scheduleSave(); });
            add(deviceList, row, 12);
        }
        for (Session.Output output : Session.outputs.values()) if (!output.available) {
            LinearLayout row = card();
            add(row, text(output.name + " · no longer exposed (#" + output.id + ")", 14, muted), 6);
            TextView route = text(output.route, 12, muted); routes.put(output.id, route); add(row, route, 0);
            add(deviceList, row, 12);
        }
        if (available.length == 0) add(deviceList, text("No audio outputs found. Connect a speaker or headphones and return here.", 14, muted), 16);
        Session.primaryId = selected == 0 ? -1 : available[selected - 1].getId();
        primary.setOnItemSelectedListener(null);
        primary.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, names));
        primary.setSelection(selected);
        final int[] lastPosition = { selected };
        primary.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(AdapterView<?> p, View v, int position, long id) {
                Session.primaryId = position == 0 || position > available.length ? -1 : available[position - 1].getId();
                if (position != lastPosition[0]) {
                    mixStore.primary(Session.primaryId == -1 ? null : available[position - 1]);
                    lastPosition[0] = position;
                }
            }
            @Override public void onNothingSelected(AdapterView<?> p) { Session.primaryId = -1; }
        });
        long bt = Arrays.stream(available).filter(d -> d.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_A2DP ||
                (Build.VERSION.SDK_INT >= 31 && (d.getType() == AudioDeviceInfo.TYPE_BLE_HEADSET || d.getType() == AudioDeviceInfo.TYPE_BLE_SPEAKER))).count();
        count.setText(getResources().getQuantityString(R.plurals.output_count, available.length, available.length, bt));
    }
    private boolean validate(boolean captureMode) {
        long selected = Session.outputs.values().stream().filter(o -> o.available && o.selected && (!captureMode || o.id != Session.primaryId)).count();
        if (captureMode && Session.primaryId == -1) { Session.status = "Choose the original playback output first"; return false; }
        if (selected == 0) { Session.status = "Select " + (captureMode ? "an additional" : "at least one") + " output below"; return false; }
        return true;
    }
    private void startTone() {
        if (!validate(false)) return;
        beginSession(new Intent(this, AudioService.class).setAction(AudioService.START_TONE));
    }
    private void beginSession(Intent intent) {
        if (Session.starting || Session.running) return;
        Session.starting = true;
        tone.setEnabled(false); capture.setEnabled(false);
        try {
            startForegroundService(intent);
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 7);
        } catch (RuntimeException error) {
            Session.starting = false; Session.status = "Could not start audio: " + error.getMessage();
        }
    }
    private void requestCapture() {
        if (!validate(true)) return;
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 8); return;
        }
        requesting = true;
        startActivityForResult(getSystemService(MediaProjectionManager.class).createScreenCaptureIntent(), 9);
    }
    @Override public void onRequestPermissionsResult(int request, String[] permissions, int[] grants) {
        super.onRequestPermissionsResult(request, permissions, grants);
        if (request == 10 && bluetooth != null) bluetooth.refresh();
        if (request == 8) {
            if (grants.length > 0 && grants[0] == PackageManager.PERMISSION_GRANTED) requestCapture();
            else Session.status = "Audio permission was denied. The output test still works.";
        }
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != 9) return;
        requesting = false;
        if (result == RESULT_OK && data != null && validate(true))
            beginSession(new Intent(this, AudioService.class).setAction(AudioService.START_CAPTURE).putExtra("grant", data));
        else Session.status = "Playback capture was not started";
    }
    private void copyReport() {
        if (bluetooth != null) bluetooth.refresh();
        StringBuilder report = new StringBuilder("Auraminus Android 1.0\n").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append("\nAndroid ").append(Build.VERSION.RELEASE).append(" / API ").append(Build.VERSION.SDK_INT)
                .append("\n").append(Session.status).append("\nPrimary id: ").append(Session.primaryId).append('\n');
        report.append("Running: ").append(Session.running).append("\nRoute observations are last observed, not proof of simultaneous sound.\n");
        for (Session.Output output : Session.outputs.values()) report.append('#').append(output.id).append(' ').append(output.name)
                .append(" exposed=").append(output.available).append(" selected=").append(output.selected).append(" delay=").append(output.delayMs).append("ms gain=").append(output.gain)
                .append("\n  ").append(output.route).append(" dropped=").append(output.droppedBlocks).append('\n');
        report.append("\nBluetooth profile snapshot:\n").append(Session.bluetooth)
                .append("\n\nRecent events (seconds since phone boot):\n").append(Session.eventReport());
        getSystemService(ClipboardManager.class).setPrimaryClip(ClipData.newPlainText("Auraminus route report", report.toString()));
        Toast.makeText(this, "Route report copied", Toast.LENGTH_SHORT).show();
    }
    private static String type(int type) {
        switch (type) {
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP: return "Bluetooth";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER: return "Phone speaker";
            case AudioDeviceInfo.TYPE_USB_DEVICE: case AudioDeviceInfo.TYPE_USB_HEADSET: return "USB";
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES: case AudioDeviceInfo.TYPE_WIRED_HEADSET: return "Wired";
            case AudioDeviceInfo.TYPE_HDMI: return "HDMI";
            case AudioDeviceInfo.TYPE_BLE_HEADSET: case AudioDeviceInfo.TYPE_BLE_SPEAKER: return "Bluetooth LE";
            default: return "Audio type " + type;
        }
    }
    private void slider(LinearLayout parent, String label, int value, int max, String unit, java.util.function.IntConsumer changed) {
        TextView caption = text(getString(R.string.control_value, label, value, unit), 13, ink); add(parent, caption, 0);
        SeekBar slider = new SeekBar(this); slider.setMax(max); slider.setProgress(value); slider.setContentDescription(label);
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean user) {
                caption.setText(getString(R.string.control_value, label, progress, unit)); changed.accept(progress);
            }
            @Override public void onStartTrackingTouch(SeekBar bar) {}
            @Override public void onStopTrackingTouch(SeekBar bar) {}
        }); add(parent, slider, 6);
    }
    private LinearLayout column() { LinearLayout l = new LinearLayout(this); l.setOrientation(LinearLayout.VERTICAL); return l; }
    private LinearLayout card() {
        LinearLayout l = column(); l.setPadding(dp(16), dp(16), dp(16), dp(16));
        l.setBackground(background(Color.rgb(24,38,34))); return l;
    }
    private GradientDrawable background(int color) { GradientDrawable d = new GradientDrawable(); d.setColor(color); d.setCornerRadius(dp(16)); return d; }
    private TextView text(String value, int size, int color) {
        TextView t = new TextView(this); t.setText(value); t.setTextSize(size); t.setTextColor(color); t.setLineSpacing(dp(3), 1); return t;
    }
    private Button button(String label, int bg, int fg) {
        Button b = new Button(this); b.setText(label); b.setAllCaps(false); b.setTextColor(fg); b.setTextSize(14);
        b.setBackground(background(bg)); b.setPadding(dp(12), dp(12), dp(12), dp(12)); return b;
    }
    private void add(LinearLayout parent, View child, int bottom) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2); params.bottomMargin = dp(bottom); parent.addView(child, params);
    }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
