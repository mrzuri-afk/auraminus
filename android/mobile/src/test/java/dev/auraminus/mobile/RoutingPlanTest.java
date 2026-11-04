package dev.auraminus.mobile;

import org.junit.Test;
import java.util.Arrays;
import static org.junit.Assert.*;

public final class RoutingPlanTest {
    private Session.Output output(int id, boolean selected, boolean available) {
        Session.Output result = new Session.Output(id, "Output " + id);
        result.selected = selected; result.available = available; return result;
    }
    @Test public void selectedOriginalIsNeverMirroredOntoItself() {
        Session.Output original = output(1, true, true), wired = output(2, true, true);
        RoutingPlan plan = new RoutingPlan(Arrays.asList(original, wired), true, 1);
        assertEquals(Arrays.asList(wired), plan.rendered);
        assertEquals(2, plan.required.size()); assertTrue(plan.required.contains(1));
    }
    @Test public void liveDeselectionRemovesOldRenderersFromThePlan() {
        Session.Output original = output(1, false, true), wired = output(2, true, true), bluetooth = output(3, true, true);
        assertEquals(2, new RoutingPlan(Arrays.asList(original, wired, bluetooth), true, 1).rendered.size());
        wired.selected = false;
        RoutingPlan updated = new RoutingPlan(Arrays.asList(original, wired, bluetooth), true, 1);
        assertEquals(Arrays.asList(bluetooth), updated.rendered); assertFalse(updated.required.contains(2));
        bluetooth.selected = false;
        assertTrue(new RoutingPlan(Arrays.asList(original, wired, bluetooth), true, 1).rendered.isEmpty());
    }
    @Test public void repeatedAndUnavailableEndpointsCannotCreateExtraCopies() {
        Session.Output original = output(1, false, true), wired = output(2, true, true);
        RoutingPlan plan = new RoutingPlan(Arrays.asList(original, wired, output(2, true, true), output(8, true, false)), true, 1);
        assertEquals(Arrays.asList(wired), plan.rendered);
    }
    @Test(expected = IllegalStateException.class) public void missingOriginalIsRejectedBeforeStartingCapture() {
        new RoutingPlan(Arrays.asList(output(1, true, false), output(2, true, true)), true, 1);
    }
    @Test public void pulseCanStillTestTheChosenOriginalOutput() {
        Session.Output original = output(1, true, true);
        assertEquals(Arrays.asList(original), new RoutingPlan(Arrays.asList(original), false, 1).rendered);
    }
}
