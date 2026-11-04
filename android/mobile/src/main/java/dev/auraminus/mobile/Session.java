package dev.auraminus.mobile;

import java.util.LinkedHashMap;
import java.util.Map;

/** Main-thread control state; audio threads only read volatile fields. */
public final class Session {
    public static final Map<Integer, Output> outputs = new LinkedHashMap<>();
    public static volatile boolean running;
    public static boolean starting;
    public static volatile String status = "Ready · choose your outputs";
    public static volatile float peak;
    public static volatile long frames;
    public static int primaryId = -1;
    public static boolean holdRouteTest;
    public static volatile String bluetooth = "Bluetooth diagnostics not started";
    private static final java.util.ArrayDeque<String> events = new java.util.ArrayDeque<>();
    public static synchronized void log(String event) {
        if (events.size() == 80) events.removeFirst();
        events.addLast(android.os.SystemClock.elapsedRealtime() / 1000 + "s • " + event);
    }
    public static synchronized String eventReport() { return String.join("\n", events); }

    public static final class Output {
        public final int id;
        public final String name;
        public String preferenceKey;
        public boolean selected;
        public boolean available = true;
        public volatile float gain = 0.5f;
        public volatile int delayMs;
        public volatile String route = "Not tested";
        public volatile long droppedBlocks;
        public Output(int id, String name) { this.id = id; this.name = name; }
    }
    private Session() {}
}
