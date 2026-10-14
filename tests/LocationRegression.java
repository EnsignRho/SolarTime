package com.lebeli.soltime;

// Pure Java tests for the Android location policy and no-location clock behavior.
public final class LocationRegression {
    private static void check(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
    private static LocationFixes.Fix fix(double time, double accuracy) {
        return new LocationFixes.Fix(40, -86, accuracy, time, 100, 90);
    }
    public static void main(String[] args) {
        LocationFixes f = new LocationFixes();
        check(!f.accept(fix(39, 10), 100), "reject cached fixes older than a minute");
        check(!f.accept(fix(101, 10), 100), "reject future fixes");
        check(f.accept(fix(98, 10), 100), "accept recent cached position");
        check(f.age(100) == 2 && f.speed(100) == 100, "retain measurement age and fresh speed");
        check(!f.accept(fix(97, 5), 100), "reject out-of-order position");
        check(!f.accept(fix(99, 1000), 100), "coarse result must not replace fresh precise position");
        check(f.speed(103) == 0, "stop extrapolating after five seconds");
        check(f.accept(fix(104, 1000), 104), "approximate-only location remains usable");
        check(!f.accept(new LocationFixes.Fix(Double.NaN, 0, 5, 105, 0, 0), 105), "reject invalid coordinates");
        check(!f.accept(fix(105, Double.NaN), 105), "reject missing accuracy");
        check(f.accept(new LocationFixes.Fix(40, -86, 10, 106, 100, Double.NaN), 106), "accept fix without bearing");
        check(f.speed(106) == 0, "do not predict without bearing");

        double[] ll = new double[2];
        check(Cities.parseCoords("39.77N 86.16W", ll) && ll[0] == 39.77 && ll[1] == -86.16, "hemisphere coordinates");
        check(Cities.parseCoords("39.77, -86.16", ll), "signed coordinates");
        for (String s : new String[]{"40E 80N", "-40N 80E", "NaN 0", "0 Infinity", "91 0"})
            check(!Cities.parseCoords(s, ll), "reject invalid coordinate: " + s);

        SolTime p = new SolTime();
        SolTime.Registry r = p.registryCreate(new SolTime.Zone[]{new SolTime.Zone(40, -86, 0, 10, 4, 100000, 40)});
        SolTime.Clock c = p.clockCreateUnix(r);
        int[] flags = new int[1];
        double t = SolarCalendar.jan1(2027) * 86400.0;
        check(c.followZone(0) == 0, "select followed zone before GPS");
        check(Math.abs(c.show(t, flags) - t - r.zoneOffsetUnixSecs(0, t)) < 1e-6
                && (flags[0] & SolTime.Clock.NO_FIX) == 0, "followed clock works without GPS");
        c.followZone(-1);
        check(Double.isNaN(c.show(t + 1, flags)) && (flags[0] & SolTime.Clock.NO_FIX) != 0, "unfollow returns to waiting for GPS");
        check(SolarCalendar.jan1(2101) - SolarCalendar.jan1(2100) == 365, "Gregorian century rule");
        System.out.println("Location, coordinates and no-GPS following: passed");
    }
}
