// Measure the model's solar events against an independent reference, with a JDK only.
// Model:      SolTime.sunTimesUnix -- the approximate NOAA series, one declination and equation
//             of time per UTC date, evaluated at 12:00 UTC (overview section 5.1).
// Reference:  the Meeus low-precision solar position (J. Meeus, Astronomical Algorithms, 2nd ed.,
//             1998), re-evaluated at each event's own time;  the same 90.833 degree zenith.
// Also measured:  the same Meeus formulas evaluated once per date at 12:00 UTC, the upgrade path
// that keeps the model's one-value-per-date structure.  Every date of 2026.
// In package com.lebeli.soltime to read the model's own per-date series (SolarCalendar.solar).
// Output is UTF-8 with "\n" line ends on every platform.
package com.lebeli.soltime;

import java.io.BufferedOutputStream;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public final class SolarAccuracy {
    private static final double ZENITH = 90.833;

    // Meeus:  declination (radians) and equation of time (minutes) at Julian day jd.
    static double[] sun(double jd) {
        double T = (jd - 2451545.0) / 36525.0;
        double L0 = ((280.46646 + T * (36000.76983 + 0.0003032 * T)) % 360 + 360) % 360;
        double M = 357.52911 + T * (35999.05029 - 0.0001537 * T);
        double e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
        double C = Math.sin(Math.toRadians(M)) * (1.914602 - T * (0.004817 + 0.000014 * T))
                 + Math.sin(Math.toRadians(2 * M)) * (0.019993 - 0.000101 * T)
                 + Math.sin(Math.toRadians(3 * M)) * 0.000289;
        double lam = L0 + C - 0.00569 - 0.00478 * Math.sin(Math.toRadians(125.04 - 1934.136 * T));
        double eps0 = 23 + (26 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60) / 60;
        double eps = eps0 + 0.00256 * Math.cos(Math.toRadians(125.04 - 1934.136 * T));
        double decl = Math.asin(Math.sin(Math.toRadians(eps)) * Math.sin(Math.toRadians(lam)));
        double y = Math.pow(Math.tan(Math.toRadians(eps / 2)), 2);
        double l0 = Math.toRadians(L0), m = Math.toRadians(M);
        double eot = 4 * Math.toDegrees(y * Math.sin(2 * l0) - 2 * e * Math.sin(m)
                + 4 * e * y * Math.sin(m) * Math.cos(2 * l0)
                - 0.5 * y * y * Math.sin(4 * l0) - 1.25 * e * e * Math.sin(2 * m));
        return new double[] {decl, eot};
    }

    static double jd(double unix) { return unix / 86400.0 + 2440587.5; }

    // Reference event (Unix seconds), iterated from the model's event t0;  sign -1 rise, +1 set.
    static double event(double lat, double lon, double t0, int sign) {
        double t = t0;
        for (int i = 0; i < 6; ++i) {
            double[] s = sun(jd(t));
            double c = Math.cos(Math.toRadians(ZENITH)) / (Math.cos(Math.toRadians(lat)) * Math.cos(s[0]))
                     - Math.tan(Math.toRadians(lat)) * Math.tan(s[0]);
            if (Math.abs(c) > 1) return Double.NaN;
            double ha = Math.toDegrees(Math.acos(c));
            double day = Math.floor((t + lon * 240) / 86400.0);
            double noon = day * 86400.0 - lon * 240 + (720 - s[1]) * 60;
            t = noon + sign * ha * 240;
        }
        return t;
    }

    // The same formulas, one value per date at 12:00 UTC:  {rise, set}.
    static double[] perDate(double lat, double lon, long day) {
        double[] s = sun(jd(day * 86400.0 + 43200));
        double q = (Math.cos(Math.toRadians(ZENITH)) - Math.sin(Math.toRadians(lat)) * Math.sin(s[0]))
                 / (Math.cos(Math.toRadians(lat)) * Math.cos(s[0]));
        double ha = Math.toDegrees(Math.acos(q));
        double noon = day * 86400.0 + (720 - 4 * lon - s[1]) * 60;
        return new double[] {noon - ha * 240, noon + ha * 240};
    }

    static String summary(List<double[]> diffs, int k, List<LocalDate> dates) {
        double[] a = new double[diffs.size()];
        int worst = 0;
        for (int i = 0; i < a.length; ++i) {
            a[i] = Math.abs(diffs.get(i)[k]);
            if (a[i] > a[worst]) worst = i;
        }
        double w = a[worst];
        Arrays.sort(a);
        double median = (a.length % 2 == 1) ? a[a.length / 2] : (a[a.length / 2 - 1] + a[a.length / 2]) / 2;
        return String.format(Locale.ROOT, "%3.0f s / %3.0f s (%s)", median, w, dates.get(worst).toString().substring(5));
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("Usage: SolarAccuracy path/to/places.tsv");
        PrintStream out = new PrintStream(new BufferedOutputStream(new FileOutputStream(FileDescriptor.out)),
            false, StandardCharsets.UTF_8.name());
        Map<String, double[]> places = new LinkedHashMap<>();
        for (String c : new String[] {"Miami", "Indianapolis", "Seattle", "Anchorage"}) places.put(c, null);
        for (String row : Files.readAllLines(Paths.get(args[0]))) {
            if (row.isEmpty() || row.startsWith("#")) continue;
            String[] f = row.split("\t", -1);
            if (f.length > 6 && f[2].equals("US") && places.containsKey(f[0]) && Long.parseLong(f[6]) > 100000)
                places.put(f[0], new double[] {Double.parseDouble(f[4]), Double.parseDouble(f[5])});
        }
        places.put("0N 0E", new double[] {0, 0});

        double equinox = Instant.parse("2026-03-20T14:46:00Z").getEpochSecond();
        out.printf(Locale.ROOT, "Reference check:  declination at the March equinox, 2026-03-20 14:46 UTC:  %.4f deg\n",
            Math.toDegrees(sun(jd(equinox))[0]));

        // The model's per-date series against the reference at the same instant (12:00 UTC).
        double cross = Double.NaN;
        for (int year : new int[] {2026, 2050, 2096}) {
            double worstDecl = 0, worstEot = 0, prevT = 0, prevD = Double.NaN;
            for (LocalDate d = LocalDate.of(year, 1, 1); d.getYear() == year; d = d.plusDays(1)) {
                long day = d.toEpochDay();
                double t = day * 86400.0 + 43200;
                double[] model = SolarCalendar.solar(day), ref = sun(jd(t));
                worstDecl = Math.max(worstDecl, Math.abs(Math.toDegrees(model[0] - ref[0])));
                worstEot = Math.max(worstEot, Math.abs(model[1] - ref[1]) * 60);
                if (year == 2026 && prevD < 0 && model[0] >= 0 && Double.isNaN(cross))
                    cross = prevT + (t - prevT) * -prevD / (model[0] - prevD);
                prevT = t; prevD = model[0];
            }
            out.printf(Locale.ROOT, "Model series vs reference, %d, at 12:00 UTC:  declination worst %.2f deg, equation of time worst %.0f s\n",
                year, worstDecl, worstEot);
        }
        out.printf(Locale.ROOT, "Model declination crosses zero (northward) at %s UTC, %.1f h after the equinox\n",
            Instant.ofEpochSecond((long) Math.floor(cross)).atOffset(ZoneOffset.UTC).toString().substring(0, 16).replace('T', ' '),
            (cross - equinox) / 3600);

        out.print("\n|model - reference|, every date of 2026:  typical (median) / worst (date)\n");
        out.printf(Locale.ROOT, "%-14s %-26s %-26s %-26s %-26s\n", "Place", "Model sunrise", "Model sunset",
            "Per-date Meeus sunrise", "Per-date Meeus sunset");
        SolTime model = new SolTime();
        double[] ev = new double[3];
        for (Map.Entry<String, double[]> p : places.entrySet()) {
            if (p.getValue() == null) throw new IllegalStateException("Missing place: " + p.getKey());
            double lat = p.getValue()[0], lon = p.getValue()[1];
            List<double[]> diffs = new ArrayList<>();
            List<LocalDate> dates = new ArrayList<>();
            for (LocalDate d = LocalDate.of(2026, 1, 1); d.getYear() == 2026; d = d.plusDays(1)) {
                long day = d.toEpochDay();
                if (model.sunTimesUnix(lat, lon, day * 86400.0, ev) != 0)
                    throw new IllegalStateException("No ordinary rise/set: " + p.getKey() + " " + d);
                double r = event(lat, lon, ev[0], -1), s = event(lat, lon, ev[1], +1);
                double[] pd = perDate(lat, lon, day);
                diffs.add(new double[] {ev[0] - r, ev[1] - s, pd[0] - r, pd[1] - s});
                dates.add(d);
            }
            out.printf(Locale.ROOT, "%-14s %-26s %-26s %-26s %-26s\n", p.getKey(), summary(diffs, 0, dates),
                summary(diffs, 1, dates), summary(diffs, 2, dates), summary(diffs, 3, dates));
        }
        out.flush();
    }
}
