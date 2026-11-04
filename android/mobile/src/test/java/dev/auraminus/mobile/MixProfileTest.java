package dev.auraminus.mobile;

import org.junit.Test;
import static org.junit.Assert.*;

public final class MixProfileTest {
    @Test public void settingsRoundTrip() {
        MixProfile profile = MixProfile.decode(new MixProfile(true, .63f, 81).encode());
        assertTrue(profile.selected); assertEquals(.63f, profile.gain, .0001f); assertEquals(81, profile.delay);
    }
    @Test public void malformedDataUsesSafeDefaults() {
        for (String value : new String[]{null, "", "broken", "true,invalid,20", "maybe,1,2"}) {
            MixProfile profile = MixProfile.decode(value);
            assertFalse(profile.selected); assertEquals(.5f, profile.gain, 0); assertEquals(0, profile.delay);
        }
    }
    @Test public void savedValuesAreBounded() {
        MixProfile profile = MixProfile.decode("true,9,900");
        assertEquals(1f, profile.gain, 0); assertEquals(500, profile.delay);
        profile = MixProfile.decode("true,NaN,-12");
        assertEquals(.5f, profile.gain, 0); assertEquals(0, profile.delay);
    }
}
