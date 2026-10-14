// Gregorian calendar field. Included by soltime.cpp after the shared solar/validation helpers.
// Unix seconds measure elapsed time; dates only select the daylight schedule. The legacy
// periodic API remains available for synthetic-year simulations.

static s64 iSolTime_jan1(s32 year)
{
    s64 y = year - 1;
    return 365 * y + y / 4 - y / 100 + y / 400 - 719162;
}

static s32 iSolTime_yearOf(s64 day)
{
    s32 lo = 1, hi = 10001;
    while (hi - lo > 1) {
        s32 mid = (lo + hi) / 2;
        if (iSolTime_jan1(mid) <= day) lo = mid; else hi = mid;
    }
    return lo;
}

static bool iSolTime_validUnix(f64 t)
{
    return isfinite(t) && t >= (f64)iSolTime_jan1(1600) * 86400.0
                         && t < (f64)iSolTime_jan1(10000) * 86400.0;
}

static s32 iSolTime_calendarIndex(s64 day, s32* days)
{
    s32 year = iSolTime_yearOf(day);
    *days = (s32)(iSolTime_jan1(year + 1) - iSolTime_jan1(year));
    return (s32)(day - iSolTime_jan1(year));
}

struct SSolCalendar
{
    bool valid = false;
    s32 year;
    f64 lat, anchor, slide, zenith, basis;
    s64 first;
    f64 sm[382]; // Current year plus eight real days on either side.
};

static const SSolCalendar& iSolTime_calendar(SSolTime* p, f64 lat, s32 year)
{
    // Thread-local because the desktop map and UI share registries.
    static thread_local SSolCalendar cache[64];
    static thread_local u32 next = 0;
    for (const SSolCalendar& c : cache)
        if (c.valid && c.year == year && c.lat == lat && c.anchor == p->anchor_secs
            && c.slide == p->max_slide_secs && c.zenith == p->zenith_deg && c.basis == p->basis_min_secs)
            return c;

    SSolCalendar& c = cache[next++ % 64];
    c.valid = true; c.year = year; c.lat = lat;
    c.anchor = p->anchor_secs; c.slide = p->max_slide_secs;
    c.zenith = p->zenith_deg; c.basis = p->basis_min_secs;
    c.first = iSolTime_jan1(year) - 8;

    f64 target[2][366];
    for (s32 days = 365; days <= 366; ++days) {
        const SSolYear& solar = iSolTime_year(days);
        for (s32 k = 0; k < days; ++k)
            target[days - 365][k] = p->anchor_secs -
                (720.0 - 4.0 * iSolTime_basisHa(p, lat, k, NULL, days) - solar.eqtimeMin[k]) * 60.0;
    }

    // This solar approximation has only two kinds of annual target (365/366). Include
    // both in each direction, even across a skipped century leap day (an eight-year gap).
    // A farther copy of either target can never lower the distance-penalized envelope.
    s64 start = iSolTime_jan1(year - 9), end = iSolTime_jan1(year + 10);
    std::vector<f64> sm((size_t)(end - start));
    for (s32 y = year - 9; y < year + 10; ++y) {
        s64 first = iSolTime_jan1(y);
        s32 days = (s32)(iSolTime_jan1(y + 1) - first);
        for (s32 k = 0; k < days; ++k) sm[(size_t)(first - start + k)] = target[days - 365][k];
    }
    for (size_t k = 1; k < sm.size(); ++k) sm[k] = fmin(sm[k], sm[k - 1] + p->max_slide_secs);
    for (size_t k = sm.size() - 1; k > 0; --k) sm[k - 1] = fmin(sm[k - 1], sm[k] + p->max_slide_secs);
    s32 count = (s32)(iSolTime_jan1(year + 1) - iSolTime_jan1(year)) + 16;
    for (s32 k = 0; k < count; ++k) c.sm[k] = sm[(size_t)(c.first - start + k)];
    return c;
}

static f64 iSolTime_calendarDay(const SSolCalendar& c, f64 lon, s64 k)
{
    return c.sm[k - c.first] + 240.0 * lon;
}

static void iSolTime_calendarWindow(SSolTime* p, const SSolCalendar& c, f64 lat, f64 lon,
                                    s64 k, f64* ws, f64* we)
{
    s32 d0, d1, m0 = iSolTime_calendarIndex(k, &d0), m1 = iSolTime_calendarIndex(k + 1, &d1);
    f64 sunset = k * 86400.0 + (720.0 - 4.0 * (lon - iSolTime_basisHa(p, lat, m0, NULL, d0))
                                  - iSolTime_year(d0).eqtimeMin[m0]) * 60.0;
    f64 sunrise = (k + 1) * 86400.0 + (720.0 - 4.0 * (lon + iSolTime_basisHa(p, lat, m1, NULL, d1))
                                      - iSolTime_year(d1).eqtimeMin[m1]) * 60.0;
    *ws = fmax(sunset, k * 86400.0 + p->night_start_secs - iSolTime_calendarDay(c, lon, k));
    *we = fmin(sunrise, (k + 1) * 86400.0 + p->night_end_secs - iSolTime_calendarDay(c, lon, k + 1));
}

static f64 iSolTime_calendarFieldAt(SSolTime* p, f64 lat, f64 lon, f64 t)
{
    if (!iSolTime_validUnix(t)) return NAN;
    s64 ku = (s64)floor(t / 86400.0);
    const SSolCalendar& c = iSolTime_calendar(p, lat, iSolTime_yearOf(ku));
    s64 k0 = (s64)floor((t + iSolTime_calendarDay(c, lon, ku)) / 86400.0);
    f64 ws[5], we[5];
    for (s32 j = 0; j < 5; ++j) iSolTime_calendarWindow(p, c, lat, lon, k0 - 2 + j, &ws[j], &we[j]);
    for (s32 j = 0; j < 5; ++j) {
        if (t >= ws[j] && t < we[j]) {
            s64 k = k0 - 2 + j;
            f64 a = iSolTime_calendarDay(c, lon, k), b = iSolTime_calendarDay(c, lon, k + 1);
            f64 x = (t - ws[j]) / (we[j] - ws[j]);
            return a + (b - a) * (x - sin(2.0 * M_PI * x) / (2.0 * M_PI));
        }
    }
    for (s32 j = 1; j < 5; ++j)
        if (t >= we[j - 1] && t < ws[j]) return iSolTime_calendarDay(c, lon, k0 - 2 + j);
    return NAN;
}

f64 soltime_offsetUnixSecs(SSolTime* p, f64 lat, f64 lon, f64 utc)
{
    SSolTime def; p = iSolTime_params(p, &def);
    if (!iSolTime_validParams(p) || !iSolTime_validLatLon(lat, lon) || !iSolTime_validUnix(utc)) return NAN;
    return iSolTime_calendarFieldAt(p, lat, lon, utc);
}

// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
f64 soltime_solarDateUnix(SSolTime* p, f64 lat, f64 lon, f64 utc)
{
    f64 date = floor((utc + soltime_offsetUnixSecs(p, lat, lon, utc)) / 86400.0) * 86400.0;
    return iSolTime_validUnix(date) ? date : NAN;
}

s32 soltime_sunTimesUnix(SSolTime* p, f64 lat, f64 lon, f64 date, f64* rise, f64* set, f64* noon)
{
    SSolTime def; p = iSolTime_params(p, &def);
    if (!iSolTime_validParams(p) || !iSolTime_validLatLon(lat, lon) || !iSolTime_validUnix(date)) {
        if (rise) *rise = NAN; if (set) *set = NAN; if (noon) *noon = NAN;
        return -2;
    }
    s64 k = (s64)floor(date / 86400.0);
    s32 days, m = iSolTime_calendarIndex(k, &days), state;
    f64 ha = iSolTime_hourAngle(lat, iSolTime_year(days).decl[m], p->zenith_deg, &state);
    f64 n = k * 86400.0 + (720.0 - 4.0 * lon - iSolTime_year(days).eqtimeMin[m]) * 60.0;
    if (rise) *rise = n - ha * 240.0; if (set) *set = n + ha * 240.0; if (noon) *noon = n;
    return state;
}

f64 soltime_basisLatUnix(SSolTime* p, f64 lat, f64 date)
{
    SSolTime def; p = iSolTime_params(p, &def);
    if (!iSolTime_validParams(p) || !iSolTime_validLatLon(lat, 0) || !iSolTime_validUnix(date)) return NAN;
    s32 days, m = iSolTime_calendarIndex((s64)floor(date / 86400.0), &days);
    f64 basis; iSolTime_basisHa(p, lat, m, &basis, days); return basis;
}

s32 soltime_nightWindowUnix(SSolTime* p, f64 lat, f64 lon, f64 date, f64* ws, f64* we)
{
    SSolTime def; p = iSolTime_params(p, &def);
    if (!iSolTime_validParams(p) || !iSolTime_validLatLon(lat, lon) || !iSolTime_validUnix(date)) {
        if (ws) *ws = NAN; if (we) *we = NAN; return -2;
    }
    s64 k = (s64)floor(date / 86400.0);
    const SSolCalendar& c = iSolTime_calendar(p, lat, iSolTime_yearOf(k));
    f64 a, b; iSolTime_calendarWindow(p, c, lat, lon, k, &a, &b);
    if (ws) *ws = a; if (we) *we = b;
    return 0;
}
