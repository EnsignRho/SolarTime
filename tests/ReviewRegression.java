package com.lebeli.soltime;

// BUG_FIX_PLAN_2026-10-05.md FIX-01 and FIX-03: independent checks of the Java port.
// FIX-01/FIX-03 added 2026-10-05 16:43:53 EDT; FIX-02 added 16:44:47 EDT (UTC-04:00).
public final class ReviewRegression {
    private static int failures;
    private static void check(boolean ok, String message) {
        if (!ok) { ++failures; System.err.println("FAIL: " + message); }
    }
    public static void main(String[] args) {
        SolTime p = new SolTime();
        for (boolean calendar : new boolean[]{false, true}) for (double initialSpeed : new double[]{0, 900}) {
            for (boolean relocate : new boolean[]{false, true}) {
                double t = 80.5 * 86400 + (calendar ? SolarCalendar.jan1(2026) * 86400.0 : 0);
                SolTime.Clock c = calendar ? p.clockCreateUnix(null) : p.clockCreate(null);
                int[] flags = new int[1];
                c.fix(t, 40, -86, initialSpeed, 90); c.show(t, flags);
                // A repeated coordinate one second later is within 1 s of a moving fix.
                double lon = relocate ? 0 : -86;
                c.fix(t + 1, 40, lon, 900, 80);
                double shown = c.show(t + 1, flags);
                double offset = calendar ? p.offsetUnixSecs(40, lon, t + 1) : p.offsetSecs(40, lon, (t + 1) / 86400);
                check(Double.isFinite(shown) && Math.abs(shown - (t + 1 + offset)) < 1, "fresh fast fix shows current time");
                check((flags[0] & SolTime.Clock.FAST) != 0, "fast motion recognized");
                check(((flags[0] & SolTime.Clock.NEW_ZONE) != 0) == relocate, "only relocation signals");
                check(c.nextCheckUtc() > t + 1 && c.nextCheckUtc() <= t + 61, "fast checkpoint within one minute");
                c.show(t + 2, flags);
                check((flags[0] & SolTime.Clock.NEW_ZONE) == 0, "notice occurs once");
            }
        }
        SolTime.Registry r = p.registryCreate(new SolTime.Zone[]{
            new SolTime.Zone(40, 0, 0, 10, 100, 200000, 40), new SolTime.Zone(40, 1, 0, 10, 100, 100000, 40)});
        check(r.rampKm(0) == 100, "unobstructed supplied width retained");
        check(r.rampKm(1) <= SolTime.distanceKm(40, 0, 40, 1) - 20 + 1e-9, "later supplied ramp clipped");
        for (int i = 0; i < 2; ++i) for (double day : new double[]{.1, 80.5, 172, 355.9}) {
            check(Math.abs(r.offsetSecsR(40, i, day) - r.zoneOffsetSecs(i, day)) < 1e-7, "city center stays locked");
            double t = SolarCalendar.jan1(2026) * 86400.0 + day * 86400;
            check(Math.abs(r.offsetUnixSecs(40, i, t) - r.zoneOffsetUnixSecs(i, t)) < 1e-7, "calendar city center stays locked");
        }
        // FIX-02: physical solar day and followed clock's adjustment day stay independent.
        for (double t : new double[]{SolarCalendar.jan1(2026) * 86400.0,
                (SolarCalendar.jan1(2026) + 79) * 86400.0, (SolarCalendar.jan1(2028) + 59) * 86400.0}) {
            double date = p.solarDateUnix(35.68, 139.69, t);
            check(date == t, "Tokyo solar date through New Year and leap day");
            check(p.solarDateUnix(37.77, -122.42, t) == t - 86400, "followed San Francisco date differs");
            double[] sun = new double[3]; p.sunTimesUnix(35.68, 139.69, date, sun);
            check(sun[0] > t - 12 * 3600 && sun[0] < t, "sunrise is this morning");
        }
        check(Double.isNaN(p.solarDateUnix(91, 0, 0)), "invalid location rejected");
        if (failures != 0) throw new AssertionError(failures + " review regression failures");
        System.out.println("Java review regressions: passed");
    }
}
