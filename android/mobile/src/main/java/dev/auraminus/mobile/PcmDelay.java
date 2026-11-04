package dev.auraminus.mobile;

/** A stereo, frame-aligned delay. Only the output worker owns this object. */
public final class PcmDelay {
    private final short[] history;
    private final int sampleRate;
    private int cursor;
    public PcmDelay(int sampleRate) {
        this.sampleRate = sampleRate;
        history = new short[(sampleRate / 2 + 1) * 2];
    }
    public void process(short[] input, short[] output, int count, int delayMs) {
        if (count < 0 || count % 2 != 0 || count > input.length || count > output.length)
            throw new IllegalArgumentException("Expected complete stereo frames");
        int offset = sampleRate * Math.max(0, Math.min(500, delayMs)) / 1000 * 2;
        for (int i = 0; i < count; i++) {
            history[cursor] = input[i];
            output[i] = history[(cursor - offset + history.length) % history.length];
            cursor = (cursor + 1) % history.length;
        }
    }
}
