package dev.auraminus.mobile;

/** Bounded, preallocated block queue. Drops oldest queued audio on overload.
 * Short synchronized copies are a PoC tradeoff, not a hard realtime guarantee. */
public final class PcmQueue {
    private final short[][] blocks;
    private final int[] lengths;
    private int head, size;
    private boolean closed;
    public PcmQueue(int capacity, int blockSize) {
        blocks = new short[capacity][blockSize]; lengths = new int[capacity];
    }
    public synchronized boolean offer(short[] data, int length) {
        if (closed) return false;
        boolean dropped = size == blocks.length;
        if (dropped) { head = (head + 1) % blocks.length; size--; }
        int tail = (head + size) % blocks.length;
        System.arraycopy(data, 0, blocks[tail], 0, length);
        lengths[tail] = length; size++; notifyAll();
        return dropped;
    }
    public synchronized int take(short[] destination) throws InterruptedException {
        while (size == 0 && !closed) wait();
        if (closed) return -1;
        int length = lengths[head];
        System.arraycopy(blocks[head], 0, destination, 0, length);
        head = (head + 1) % blocks.length; size--;
        return length;
    }
    public synchronized void close() { closed = true; notifyAll(); }
}
