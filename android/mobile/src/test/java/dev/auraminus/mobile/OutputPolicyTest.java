package dev.auraminus.mobile;

import android.media.AudioDeviceInfo;
import org.junit.Test;
import static org.junit.Assert.*;

public final class OutputPolicyTest {
    @Test public void excludesReportedRemoteSubmixAndInternalOutputs() {
        // Redmi 6 Pro report exposed a virtual remote-submix as audio type 25.
        assertFalse(OutputPolicy.selectable(25));
        assertFalse(OutputPolicy.selectable(AudioDeviceInfo.TYPE_BUS));
        assertFalse(OutputPolicy.selectable(AudioDeviceInfo.TYPE_TELEPHONY));
        assertFalse(OutputPolicy.selectable(AudioDeviceInfo.TYPE_BLUETOOTH_SCO));
        assertFalse(OutputPolicy.selectable(AudioDeviceInfo.TYPE_UNKNOWN));
    }
    @Test public void retainsPhysicalMediaOutputFamilies() {
        int[] types = {AudioDeviceInfo.TYPE_BUILTIN_SPEAKER, AudioDeviceInfo.TYPE_BLUETOOTH_A2DP,
                AudioDeviceInfo.TYPE_USB_HEADSET, AudioDeviceInfo.TYPE_WIRED_HEADPHONES,
                AudioDeviceInfo.TYPE_HDMI, AudioDeviceInfo.TYPE_BLE_HEADSET};
        for (int type : types) assertTrue(OutputPolicy.selectable(type));
    }
}
