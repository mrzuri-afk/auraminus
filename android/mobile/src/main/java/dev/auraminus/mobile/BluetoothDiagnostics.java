package dev.auraminus.mobile;

import android.Manifest;
import android.bluetooth.*;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import java.util.*;

/** Read-only profile inspection. No discovery, hidden APIs, connect calls or MAC export. */
final class BluetoothDiagnostics implements AutoCloseable {
    private final Context context;
    private BluetoothAdapter adapter;
    private BluetoothA2dp profile;
    private boolean requested, closed;
    BluetoothDiagnostics(Context context) { this.context = context.getApplicationContext(); }
    private boolean permitted() {
        return Build.VERSION.SDK_INT < 31 || context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
    }
    private final BluetoothProfile.ServiceListener listener = new BluetoothProfile.ServiceListener() {
        @Override public void onServiceConnected(int type, BluetoothProfile proxy) {
            if (closed) { if (adapter != null) adapter.closeProfileProxy(type, proxy); return; }
            profile = (BluetoothA2dp) proxy; refresh();
        }
        @Override public void onServiceDisconnected(int type) { profile = null; }
    };
    void refresh() {
        if (closed) return;
        if (!permitted()) { publish("Bluetooth diagnostics need Nearby devices permission. Audio testing remains available."); return; }
        try {
            BluetoothManager manager = context.getSystemService(BluetoothManager.class);
            adapter = manager == null ? null : manager.getAdapter();
            if (adapter == null) { publish("No Bluetooth adapter"); return; }
            if (!adapter.isEnabled()) { publish("Bluetooth is off"); return; }
            if (!requested) {
                requested = adapter.getProfileProxy(context, listener, BluetoothProfile.A2DP);
            }
            if (profile == null) { publish("Waiting for Bluetooth A2DP service…"); return; }
            List<BluetoothDevice> connected = profile.getConnectedDevices();
            Set<BluetoothDevice> all = new HashSet<>(adapter.getBondedDevices());
            all.addAll(connected);
            List<String> lines = new ArrayList<>();
            for (BluetoothDevice device : all) {
                String name = device.getName() == null ? "Unnamed paired device" : device.getName();
                int state = profile.getConnectionState(device);
                String connection = state == BluetoothProfile.STATE_CONNECTED ? "A2DP connected"
                        : state == BluetoothProfile.STATE_CONNECTING ? "A2DP connecting"
                        : state == BluetoothProfile.STATE_DISCONNECTING ? "A2DP disconnecting" : "A2DP not connected";
                lines.add(name + " • " + connection + " • playing=" + profile.isA2dpPlaying(device));
            }
            Collections.sort(lines);
            publish("A2DP connected: " + connected.size() + " / paired devices: " + all.size() + "\n" + String.join("\n", lines));
        } catch (SecurityException e) { publish("Bluetooth diagnostic permission denied or revoked"); }
        catch (RuntimeException e) { publish("Bluetooth diagnostics unavailable: " + e.getClass().getSimpleName()); }
    }
    private void publish(String text) {
        if (!text.equals(Session.bluetooth)) { Session.bluetooth = text; Session.log(text); }
    }
    @Override public void close() {
        closed = true;
        if (adapter != null && profile != null) adapter.closeProfileProxy(BluetoothProfile.A2DP, profile);
        profile = null;
    }
}
