package dev.auraminus.mobile;

/** Portable, validated representation of a saved endpoint mix. */
final class MixProfile {
    final boolean selected;
    final float gain;
    final int delay;
    MixProfile(boolean selected, float gain, int delay) {
        this.selected = selected;
        this.gain = Float.isFinite(gain) ? Math.max(0f, Math.min(1f, gain)) : 0.5f;
        this.delay = Math.max(0, Math.min(500, delay));
    }
    String encode() { return selected + "," + gain + "," + delay; }
    static MixProfile decode(String value) {
        if (value != null) try {
            String[] parts = value.split(",");
            if (parts.length == 3 && (parts[0].equals("true") || parts[0].equals("false")))
                return new MixProfile(Boolean.parseBoolean(parts[0]), Float.parseFloat(parts[1]), Integer.parseInt(parts[2]));
        } catch (NumberFormatException ignored) { }
        return new MixProfile(false, 0.5f, 0);
    }
}
