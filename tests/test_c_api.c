/* BUG_FIX_PLAN_2026-10-05.md FIX-05 — added 2026-10-05 16:48:37 EDT (UTC-04:00).
   Include the isolated public header and link the C ABI. */
#include <soltime.h>
#include <assert.h>
#include <math.h>
#include <stddef.h>

int main(void)
{
    SSolTime p;
    soltime_defaults(&p);
    assert(p.anchor_secs == 24300);
    const double t = 1774094400.0;
    double off = soltime_offsetUnixSecs(&p, 40, -86, t);
    assert(isfinite(off));
    double date = soltime_solarDateUnix(&p, 40, -86, t), rise, set, noon;
    assert(isfinite(date) && fmod(date, 86400) == 0);
    assert(soltime_sunTimesUnix(&p, 40, -86, date, &rise, &set, &noon) == 0);
    assert(rise < noon && noon < set);
    assert(isnan(soltime_offsetUnixSecs(&p, 100, 0, t)));

    SSolZone z = {40, -86, 0, 10, 4, 100000, 40};
    SSolRegistry* r = soltime_registryCreate(&p, &z, 1);
    assert(r && soltime_zoneLeader(r, 0) == 0);
    SSolClock* c = soltime_clockCreateUnix(&p, r);
    assert(c);
    u32 flags = 0;
    assert(isnan(soltime_clockShow(c, t, &flags)) && (flags & SOLCLOCK_NO_FIX));
    assert(soltime_clockFollowZone(c, 0) == 0);
    assert(fabs(soltime_clockShow(c, t, &flags) - (t + off)) < 1e-6);
    assert(soltime_clockFollowZone(c, -1) == 0);
    assert(soltime_clockFix(c, t + 1, 40, -86, 0, 0) == 0);
    assert(isfinite(soltime_clockShow(c, t + 1, &flags)));
    soltime_clockDelete(c);
    soltime_registryDelete(r);
    return 0;
}
