package com.lebeli.soltime;

// Gregorian counterpart of soltime_calendar.inc. No Android dependencies.
final class SolarCalendar {
    static long jan1(int year) {
        long y = year - 1;
        return 365 * y + y / 4 - y / 100 + y / 400 - 719162;
    }
    static int yearOf(long day) {
        int lo = 1, hi = 10001;
        while (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            if (jan1(mid) <= day) lo = mid; else hi = mid;
        }
        return lo;
    }
    static boolean validTime(double t) {
        return Double.isFinite(t) && t >= jan1(1600) * 86400.0 && t < jan1(10000) * 86400.0;
    }
    private static final double[][][] SOLAR = new double[2][366][2];
    static {
        for (int days = 365; days <= 366; ++days) for (int k = 0; k < days; ++k) {
            double g = 2 * Math.PI * k / days;
            double[] s = SOLAR[days - 365][k];
            s[0] = .006918 - .399912 * Math.cos(g) + .070257 * Math.sin(g)
                    - .006758 * Math.cos(2*g) + .000907 * Math.sin(2*g)
                    - .002697 * Math.cos(3*g) + .001480 * Math.sin(3*g);
            s[1] = 229.18 * (.000075 + .001868 * Math.cos(g) - .032077 * Math.sin(g)
                    - .014615 * Math.cos(2*g) - .040849 * Math.sin(2*g));
        }
    }
    static double[] solar(long day) {
        int year = yearOf(day);
        int days = (int)(jan1(year + 1) - jan1(year));
        return SOLAR[days - 365][(int)(day - jan1(year))];
    }
    static final class Table {
        int year;
        long first;
        double lat, anchor, slide, zenith, basis;
        double[] sm;
        double day(double lon, long k) { return sm[(int)(k - first)] + 240 * lon; }
    }
    private static final class Cache {
        Table[] entries = new Table[64];
        int next;
    }
    private static final ThreadLocal<Cache> CACHE = ThreadLocal.withInitial(Cache::new);

    static Table table(SolTime p, double lat, int year) {
        Cache cache = CACHE.get();
        for (Table c : cache.entries)
            if (c != null && c.year == year && c.lat == lat && c.anchor == p.anchorSecs
                    && c.slide == p.maxSlideSecs && c.zenith == p.zenithDeg && c.basis == p.basisMinSecs) return c;
        Table c = new Table();
        c.year = year; c.lat = lat; c.anchor = p.anchorSecs; c.slide = p.maxSlideSecs;
        c.zenith = p.zenithDeg; c.basis = p.basisMinSecs; c.first = jan1(year) - 8;
        double[][] target = new double[2][366];
        for (int days = 365; days <= 366; ++days) for (int k = 0; k < days; ++k) {
            double[] s = SOLAR[days - 365][k];
            target[days - 365][k] = p.anchorSecs - (720 - 4 * p.basisHaDecl(lat, s[0], null) - s[1]) * 60;
        }
        // Nine full years in either direction cover both annual target types, including
        // the eight-year leap-day gap at a non-leap century. More distant copies cannot win.
        long start = jan1(year - 9);
        double[] sm = new double[(int)(jan1(year + 10) - start)];
        for (int y = year - 9; y < year + 10; ++y) {
            long first = jan1(y);
            int days = (int)(jan1(y + 1) - first);
            System.arraycopy(target[days - 365], 0, sm, (int)(first - start), days);
        }
        for (int k = 1; k < sm.length; ++k) sm[k] = Math.min(sm[k], sm[k-1] + p.maxSlideSecs);
        for (int k = sm.length - 1; k > 0; --k) sm[k-1] = Math.min(sm[k-1], sm[k] + p.maxSlideSecs);
        c.sm = new double[(int)(jan1(year + 1) - jan1(year)) + 16];
        System.arraycopy(sm, (int)(c.first - start), c.sm, 0, c.sm.length);
        cache.entries[cache.next++ & 63] = c;
        return c;
    }

    static void window(SolTime p, Table c, double lat, double lon, long k, double[] out) {
        double[] s0 = solar(k), s1 = solar(k + 1);
        double sunset = k * 86400.0 + (720 - 4 * (lon - p.basisHaDecl(lat, s0[0], null)) - s0[1]) * 60;
        double sunrise = (k + 1) * 86400.0 + (720 - 4 * (lon + p.basisHaDecl(lat, s1[0], null)) - s1[1]) * 60;
        if (out != null) {
            out[0] = Math.max(sunset, k * 86400.0 + p.nightStartSecs - c.day(lon, k));
            out[1] = Math.min(sunrise, (k + 1) * 86400.0 + p.nightEndSecs - c.day(lon, k + 1));
        }
    }
    static double offset(SolTime p, double lat, double lon, double t) {
        if (!validTime(t)) return Double.NaN;
        long ku = (long)Math.floor(t / 86400);
        Table c = table(p, lat, yearOf(ku));
        long k0 = (long)Math.floor((t + c.day(lon, ku)) / 86400);
        double[] ws = new double[5], we = new double[5], w = new double[2];
        for (int j = 0; j < 5; ++j) {
            window(p, c, lat, lon, k0 - 2 + j, w); ws[j] = w[0]; we[j] = w[1];
        }
        for (int j = 0; j < 5; ++j) if (t >= ws[j] && t < we[j]) {
            long k = k0 - 2 + j;
            double a = c.day(lon, k), b = c.day(lon, k+1), x = (t - ws[j]) / (we[j] - ws[j]);
            return a + (b - a) * (x - Math.sin(2*Math.PI*x) / (2*Math.PI));
        }
        for (int j = 1; j < 5; ++j) if (t >= we[j-1] && t < ws[j]) return c.day(lon, k0 - 2 + j);
        return Double.NaN;
    }
}
