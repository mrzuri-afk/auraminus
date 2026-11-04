package dev.auraminus.mobile;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** A fresh route plan for each selection update. The source is never copied back onto itself. */
final class RoutingPlan {
    final List<Session.Output> rendered;
    final Set<Integer> required;
    RoutingPlan(Iterable<Session.Output> outputs, boolean capture, int primaryId) {
        LinkedHashMap<Integer, Session.Output> selected = new LinkedHashMap<>();
        boolean sourceAvailable = false;
        for (Session.Output output : outputs) {
            if (!output.available) continue;
            if (output.id == primaryId) sourceAvailable = true;
            if (output.selected && (!capture || output.id != primaryId)) selected.putIfAbsent(output.id, output);
        }
        if (capture && (primaryId < 0 || !sourceAvailable))
            throw new IllegalStateException("Choose the output currently playing the original app audio");
        rendered = Collections.unmodifiableList(new ArrayList<>(selected.values()));
        LinkedHashSet<Integer> ids = new LinkedHashSet<>(selected.keySet());
        if (capture) ids.add(primaryId);
        required = Collections.unmodifiableSet(ids);
    }
}
