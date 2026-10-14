// Regression scenarios for BUG_FIX_PLAN_2026-10-05.md FIX-01 through FIX-03.
// FIX-01/FIX-03 added 2026-10-05 16:43:53 EDT; FIX-02 added 16:44:47 EDT (UTC-04:00).
#include <cstdio>
#include "../soltime.cpp"

static int failures;
#define CHECK(test, message) do { if (!(test)) { ++failures; std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, message); } } while (0)

static void clockReplanning()
{
    for (bool calendar : {false, true}) for (double initialSpeed : {0., 900.}) {
        for (bool relocate : {false, true}) {
            double t = 80.5 * 86400 + (calendar ? iSolTime_jan1(2026) * 86400.0 : 0);
            auto c = calendar ? soltime_clockCreateUnix(nullptr, nullptr) : soltime_clockCreate(nullptr, nullptr);
            soltime_clockFix(c, t, 40, -86, initialSpeed, 90);
            u32 flags; soltime_clockShow(c, t, &flags);
            double lat = 40, lon = -86;
            if (initialSpeed) iSolTime_destination(lat, lon, initialSpeed / 3600, 90, &lat, &lon);
            if (relocate) { lat = 40; lon = 0; }
            soltime_clockFix(c, t + 1, lat, lon, 900, 80);
            double shown = soltime_clockShow(c, t + 1, &flags);
            double offset = calendar ? soltime_offsetUnixSecs(nullptr, lat, lon, t + 1)
                                     : soltime_offsetSecs(nullptr, lat, lon, (t + 1) / 86400);
            CHECK(std::isfinite(shown) && std::fabs(shown - (t + 1 + offset)) < .1, "new fast fix shows the current local time");
            CHECK((flags & SOLCLOCK_FAST) != 0, "fast motion recognized");
            CHECK(((flags & SOLCLOCK_NEW_ZONE) != 0) == relocate, "only relocation signals a zone change");
            CHECK(soltime_clockNextCheckUtc(c) > t + 1 && soltime_clockNextCheckUtc(c) <= t + 61,
                  "fast deadline is at most one minute away");
            for (int j = 2; j <= 65; ++j) {
                shown = soltime_clockShow(c, t + j, &flags);
                CHECK(std::isfinite(shown) && std::fabs(shown - iSolTime_clockTruth(c, t + j)) < .2,
                      "new flight stays on time between and beyond checkpoints");
                CHECK(!(flags & SOLCLOCK_NEW_ZONE), "relocation notice occurs once");
            }
            soltime_clockDelete(c);
        }
    }
}

static void suppliedRamps()
{
    SSolZone zones[] = {{40, 0, 0, 10, 100, 200000, 40}, {40, 1, 0, 10, 100, 100000, 40}};
    auto r = soltime_registryCreate(nullptr, zones, 2);
    CHECK(r != nullptr, "custom registry accepted");
    if (!r) return;
    CHECK(soltime_zoneRampKm(r, 0) == 100, "unobstructed requested ramp retained");
    double gap = soltime_distanceKm(40, 0, 40, 1) - 20;
    CHECK(soltime_zoneRampKm(r, 1) <= gap + 1e-9, "later supplied ramp stops at the earlier lock");
    for (int i = 0; i < 2; ++i) for (double radius : {0., 5., 9.999}) {
        for (int bearing = 0; bearing < 360; bearing += 45) for (double day : {0.1, 80.5, 172.0, 355.9}) {
            double lat, lon; iSolTime_destination(40, i, radius, bearing, &lat, &lon);
            CHECK(std::fabs(soltime_offsetSecsR(r, lat, lon, day) - soltime_zoneOffsetSecs(r, i, day)) < 1e-7,
                  "every sampled point in each disc keeps its own leader's clock");
            double t = iSolTime_jan1(2026) * 86400.0 + day * 86400;
            CHECK(std::fabs(soltime_offsetUnixSecsR(r, lat, lon, t) - soltime_zoneOffsetUnixSecs(r, i, t)) < 1e-7,
                  "Gregorian locked interiors agree too");
        }
    }
    double bearing = iSolTime_bearing(40, 0, 40, 1), a, b, c, d;
    iSolTime_destination(40, 0, 10 - .00001, bearing, &a, &b);
    iSolTime_destination(40, 0, 10 + .00001, bearing, &c, &d);
    CHECK(std::fabs(soltime_offsetSecsR(r, a, b, 80.5) - soltime_offsetSecsR(r, c, d, 80.5)) < .001,
          "capped supplied ramp remains continuous at the earlier disc rim");
    soltime_registryDelete(r);
    zones[1].rampKm = 0;
    r = soltime_registryCreate(nullptr, zones, 2);
    CHECK(r && soltime_zoneRampKm(r, 1) == 0, "explicit hard edge retained");
    soltime_registryDelete(r);
}

static void solarDates()
{
    for (double t : {iSolTime_jan1(2026) * 86400.0, (iSolTime_jan1(2026) + 79) * 86400.0,
                     (iSolTime_jan1(2028) + 59) * 86400.0}) {
        double physicalDate = soltime_solarDateUnix(nullptr, 35.68, 139.69, t);
        double followedDate = soltime_solarDateUnix(nullptr, 37.77, -122.42, t);
        CHECK(physicalDate == t, "Tokyo physical day is the current UTC date at 00Z");
        CHECK(followedDate == t - 86400, "San Francisco clock owner is on the previous date at 00Z");
        double rise, set, noon;
        soltime_sunTimesUnix(nullptr, 35.68, 139.69, physicalDate, &rise, &set, &noon);
        CHECK(rise > t - 12 * 3600 && rise < t, "Tokyo sunrise belongs to this morning, not yesterday");
        double wrongRise;
        soltime_sunTimesUnix(nullptr, 35.68, 139.69, followedDate, &wrongRise, nullptr, nullptr);
        CHECK(rise - wrongRise > 23 * 3600, "regression distinguishes followed-date sunrise");
        double ws, we;
        CHECK(soltime_nightWindowUnix(nullptr, 37.77, -122.42, followedDate, &ws, &we) == 0
              && we > ws && we - t < 24 * 3600, "night adjustment uses its owner's date");
    }
    CHECK(std::isnan(soltime_solarDateUnix(nullptr, 91, 0, 0)), "solar date validates location");
    CHECK(std::isnan(soltime_solarDateUnix(nullptr, 0, 0, NAN)), "solar date validates instant");
}

int main()
{
    clockReplanning(); suppliedRamps(); solarDates();
    std::printf("Review regressions: %d failures\n", failures);
    return failures ? 1 : 0;
}
