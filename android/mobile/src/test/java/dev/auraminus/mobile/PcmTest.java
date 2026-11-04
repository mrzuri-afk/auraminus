package dev.auraminus.mobile;

import org.junit.Test;
import static org.junit.Assert.*;

public final class PcmTest {
    @Test public void zeroDelayPreservesStereo() {
        short[] input = {1, -1, 2, -2, 3, -3}; short[] output = new short[6];
        new PcmDelay(48000).process(input, output, 6, 0);
        assertArrayEquals(input, output);
    }
    @Test public void delayPersistsAcrossBlocksAndKeepsChannelsAligned() {
        PcmDelay delay = new PcmDelay(1000);
        short[] output = new short[4];
        delay.process(new short[]{11, 12, 21, 22}, output, 4, 2);
        assertArrayEquals(new short[4], output);
        delay.process(new short[]{31, 32, 41, 42}, output, 4, 2);
        assertArrayEquals(new short[]{11, 12, 21, 22}, output);
    }
    @Test public void maximumDelaySurvivesRingWrap() {
        PcmDelay delay = new PcmDelay(1000);
        short[] input = new short[2400], output = new short[2400];
        for (int i = 0; i < input.length; i++) input[i] = (short)(i + 1);
        delay.process(input, output, input.length, 500);
        for (int i = 0; i < 1000; i++) assertEquals(0, output[i]);
        for (int i = 1000; i < output.length; i++) assertEquals(input[i - 1000], output[i]);
    }
    @Test public void delayChangesUseHistoryWithoutBreakingStereo() {
        PcmDelay delay = new PcmDelay(1000); short[] output = new short[4];
        delay.process(new short[]{1, 2, 3, 4}, output, 4, 0);
        delay.process(new short[]{5, 6, 7, 8}, output, 4, 1);
        assertArrayEquals(new short[]{3, 4, 5, 6}, output);
    }
    @Test(expected = IllegalArgumentException.class) public void rejectsPartialStereoFrame() {
        new PcmDelay(48000).process(new short[3], new short[3], 3, 0);
    }
    @Test public void queueDropsOldestAndPreservesInputOwnership() throws Exception {
        PcmQueue queue = new PcmQueue(2, 2); short[] input = {1, 2}, output = new short[2];
        assertFalse(queue.offer(input, 2)); input[0] = 9;
        queue.take(output); assertArrayEquals(new short[]{1, 2}, output);
        queue.offer(new short[]{3, 4}, 2); queue.offer(new short[]{5, 6}, 2);
        assertTrue(queue.offer(new short[]{7, 8}, 2));
        queue.take(output); assertArrayEquals(new short[]{5, 6}, output);
        queue.take(output); assertArrayEquals(new short[]{7, 8}, output);
    }
    @Test public void closingQueueWakesWaitingConsumer() throws Exception {
        PcmQueue queue = new PcmQueue(2, 2);
        int[] result = {0};
        Thread worker = new Thread(() -> { try { result[0] = queue.take(new short[2]); } catch (InterruptedException e) { throw new AssertionError(e); } });
        worker.start(); queue.close(); worker.join(1000);
        assertFalse(worker.isAlive()); assertEquals(-1, result[0]);
    }
}
