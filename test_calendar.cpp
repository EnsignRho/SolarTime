// Real-calendar integration and regressions from the October 2026 review.
#include <cstdio>
#include <cstdlib>
#include "soltime.cpp"

static int failures = 0;
#define CHECK(c, label) do { if (!(c)) { std::fprintf(stderr, "FAIL: %s (line %d)\n", label, __LINE__); ++failures; } } while (0)

static void calendarChecks()
{
    f64 decl, eq; iSolTime_solar(0, &decl, &eq);
    CHECK(fabs(decl - (.006918 - .399912 - .006758 - .002697)) < 1e-14, "NOAA Jan 1 midday phase");
    CHECK(fabs(eq - 229.18 * (.000075 + .001868 - .014615)) < 1e-12, "NOAA Jan 1 midday equation of time");
    CHECK(iSolTime_jan1(1970) == 0, "Unix epoch");
    CHECK(iSolTime_jan1(2101) - iSolTime_jan1(2100) == 365, "2100 is not leap");
    CHECK(iSolTime_jan1(2401) - iSolTime_jan1(2400) == 366, "2400 is leap");
    SSolTime p; soltime_defaults(&p);
    CHECK(isnan(soltime_offsetUnixSecs(NULL, 40, 0, NAN)), "reject nonfinite calendar time");
    CHECK(isfinite(soltime_offsetUnixSecs(NULL, 40, 179.9, iSolTime_jan1(1600) * 86400.0)), "lower calendar boundary");
    CHECK(isfinite(soltime_offsetUnixSecs(NULL, 40, -179.9, iSolTime_jan1(10000) * 86400.0 - 1)), "upper calendar boundary");
    for (double slide : {.0001, 1., 240.}) {
        SSolTime custom = p; custom.max_slide_secs = slide;
        for (int year : {2027, 2028, 2100, 2104}) {
            const SSolCalendar& before = iSolTime_calendar(&custom, 70, year - 1);
            const SSolCalendar& after = iSolTime_calendar(&custom, 70, year);
            for (s64 k = iSolTime_jan1(year) - 7; k < iSolTime_jan1(year) + 7; ++k)
                CHECK(fabs(iSolTime_calendarDay(before, 0, k) - iSolTime_calendarDay(after, 0, k)) < 1e-7,
                      "calendar cache agrees across years even with very slow drift");
        }
    }
    for (int year : {2026, 2028, 2100, 2400}) {
        for (double lat : {-70., -40., 0., 40., 70., 89.}) {
            for (double lon : {-179.9, -86., 0., 179.9}) {
                // The real New Year, sampled with and without a fresh position fix.
                double start = iSolTime_jan1(year + 1) * 86400.0 - 120;
                for (double speed : {0., 100.}) {
                    SSolClock* c = soltime_clockCreateUnix(NULL, NULL);
                    soltime_clockFix(c, start, lat, lon, speed, 90);
                    double worst = 0; unsigned jumps = 0;
                    for (int j = 0; j <= 1200; ++j) {
                        double t = start + j * .2, la, lo;
                        iSolTime_destination(lat, lon, speed * (t - start) / 3600, 90, &la, &lo);
                        u32 flags; double d = soltime_clockShow(c, t, &flags);
                        double truth = t + soltime_offsetUnixSecs(NULL, la, lo, t);
                        worst = fmax(worst, fabs(d - truth));
                        if (flags & SOLCLOCK_NEW_ZONE) ++jumps;
                    }
                    CHECK(worst < .12, "real New Year stays on the moving or stationary clock");
                    // A polar eastward path may cross the geographic date line; New Year itself must not signal.
                    if (speed == 0 || fabs(lon) < 170) CHECK(jumps == 0, "no artificial New Year zone change");
                    soltime_clockDelete(c);
                }
            }
        }
    }
    unsigned bad = 0;
    for (int year : {2026, 2028}) for (double lat : {-60., -40., 0., 40., 60.}) {
        for (s64 k = iSolTime_jan1(year); k < iSolTime_jan1(year + 1); ++k) {
            double date = k * 86400.0, rise, set, noon;
            soltime_sunTimesUnix(NULL, lat, -86, date, &rise, &set, &noon);
            double localRise = rise + soltime_offsetUnixSecs(NULL, lat, -86, rise) - date;
            if (!isfinite(localRise) || localRise > p.anchor_secs + 1e-5) { if (bad < 10) std::fprintf(stderr, "anchor y%d lat%g k%lld rise%.9f\n", year, lat, (long long)k, localRise); ++bad; }
            double ws, we;
            soltime_nightWindowUnix(NULL, lat, -86, date, &ws, &we);
            if (!(we > ws && ws >= set - 1e-6)) { if (bad < 10) std::fprintf(stderr, "window y%d lat%g k%lld length%g sunset delta%.9f\n", year, lat, (long long)k, we-ws,ws-set); ++bad; }
            double rate = (soltime_offsetUnixSecs(NULL, lat, -86, (ws + we)/2 + 1)
                          - soltime_offsetUnixSecs(NULL, lat, -86, (ws + we)/2)) + 1;
            if (!(rate > 0 && rate < 1.5)) { if (bad < 10) std::fprintf(stderr, "rate y%d lat%g k%lld rate%g\n", year, lat, (long long)k,rate); ++bad; }
            // In daylight a clock second remains a real second.
            if (fabs(soltime_offsetUnixSecs(NULL, lat, -86, noon + 1)
                   - soltime_offsetUnixSecs(NULL, lat, -86, noon)) > 1e-9) { if (bad < 10) std::fprintf(stderr, "day y%d lat%g k%lld\n", year, lat, (long long)k); ++bad; }
        }
    }
    CHECK(bad == 0, "actual common/leap dates preserve sunrise anchor, night windows and daytime seconds");
    p.anchor_secs = 1; p.night_start_secs = 86399; p.night_end_secs = 0;
    CHECK(isnan(soltime_offsetSecs(&p, 40, 0, 4)), "reject a night window too short for the daily adjustment");
}

static void dateLineChecks()
{
    for (bool calendar : {false, true}) for (bool exact : {false, true}) {
        for (double lat : {0., 40., 70.}) for (double speed : {128.75, 900.}) for (double heading : {90., 270.}) {
            SSolClock* c = calendar ? soltime_clockCreateUnix(NULL, NULL) : soltime_clockCreate(NULL, NULL);
            soltime_clockExact(c, exact);
            double lon = heading == 90 ? 179.99 : -179.99;
            double start = 80.5 * 86400 + (calendar ? iSolTime_jan1(2026) * 86400.0 : 0);
            unsigned jumps = 0; double worst = 0;
            for (int j = 0; j <= 300; ++j) {
                double t = start + j * .2, la, lo;
                iSolTime_destination(lat, lon, speed * j * .2 / 3600, heading, &la, &lo);
                if (j % 5 == 0) {
                    double nextLat, nextLon;
                    iSolTime_destination(lat, lon, speed * (j * .2 + 1) / 3600, heading, &nextLat, &nextLon);
                    soltime_clockFix(c, t, la, lo, speed, iSolTime_bearing(la, lo, nextLat, nextLon));
                }
                u32 flags; double d = soltime_clockShow(c, t, &flags);
                double off = calendar ? soltime_offsetUnixSecs(NULL, la, lo, t) : soltime_offsetSecs(NULL, la, lo, t/86400);
                worst = fmax(worst, fabs(d - (t + off)));
                if (flags & SOLCLOCK_NEW_ZONE) ++jumps;
            }
            if (worst >= .1) std::fprintf(stderr, "dateline calendar%d exact%d lat%g speed%g heading%g worst%.9f\n", calendar,exact,lat,speed,heading,worst);
            CHECK(worst < .1, "date-line crossing never interpolates the 24-hour jump");
            CHECK(jumps == 1, "date-line crossing signals exactly once");
            soltime_clockDelete(c);
        }
    }
}

static void followingChecks()
{
    SSolZone z = {40, -86, 0, 10, 4, 100000, 40};
    SSolRegistry* r = soltime_registryCreate(NULL, &z, 1);
    for (bool calendar : {false, true}) {
        SSolClock* c = calendar ? soltime_clockCreateUnix(NULL, r) : soltime_clockCreate(NULL, r);
        double t = calendar ? iSolTime_jan1(2026) * 86400.0 : 0;
        soltime_clockFollowZone(c, 0);
        u32 flags;
        double d = soltime_clockShow(c, t, &flags);
        double off = calendar ? soltime_zoneOffsetUnixSecs(r, 0, t) : soltime_zoneOffsetSecs(r, 0, 0);
        CHECK(isfinite(d) && fabs(d - (t + off)) < 1e-6 && !(flags & SOLCLOCK_NO_FIX), "following works without GPS");
        soltime_clockFollowZone(c, -1);
        CHECK(isnan(soltime_clockShow(c, t + 1, &flags)) && (flags & SOLCLOCK_NO_FIX), "unfollow without GPS reports missing location");
        soltime_clockDelete(c);
    }
    soltime_registryDelete(r);
}

int main()
{
    calendarChecks(); dateLineChecks(); followingChecks();
    std::printf("Calendar/travel regressions: %d failures\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
