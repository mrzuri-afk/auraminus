package dev.auraminus.mobile;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioDeviceInfo;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/** Keeps mix preferences across process restarts without persisting ephemeral output IDs. */
final class MixStore {
    private final SharedPreferences preferences;
    MixStore(Context context) { preferences = context.getSharedPreferences("auraminus.mix", Context.MODE_PRIVATE); }
    static String key(AudioDeviceInfo device) {
        String address;
        try { address = device.getAddress(); } catch (SecurityException ignored) { address = ""; }
        String identity = device.getType() + ":" + device.getProductName() + ":" + address;
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(identity.getBytes(StandardCharsets.UTF_8));
            StringBuilder key = new StringBuilder();
            for (byte b : digest) key.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
            return key.toString();
        } catch (NoSuchAlgorithmException impossible) { throw new AssertionError(impossible); }
    }
    void restore(String key, Session.Output output) {
        output.preferenceKey = key;
        MixProfile profile = MixProfile.decode(preferences.getString("output." + key, null));
        output.selected = profile.selected; output.gain = profile.gain; output.delayMs = profile.delay;
    }
    void save() {
        SharedPreferences.Editor edit = preferences.edit();
        for (Session.Output output : Session.outputs.values()) if (output.preferenceKey != null)
            edit.putString("output." + output.preferenceKey, new MixProfile(output.selected, output.gain, output.delayMs).encode());
        edit.apply();
    }
    boolean isPrimary(AudioDeviceInfo device) { return key(device).equals(preferences.getString("source", "")); }
    void primary(AudioDeviceInfo device) {
        preferences.edit().putString("source", device == null ? "" : key(device)).apply();
    }
}
