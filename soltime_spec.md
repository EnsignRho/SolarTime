# SolTime — solar-anchored continuous civil time

**Author:** Rick C. Hodgin. **Revision:** 2026-10-05, evaluation draft 3.
**License:** public domain (CC0 1.0); see `LICENSE`.
**Status:** Draft specification with reference implementations; submitted for independent
evaluation. Author decisions dated July 11 and October 3 describe the design history,
not approval by a standards organization.

The combined [SolTime_overview.md](SolTime_overview.md) is the standalone evaluation
document. This file retains API and implementation detail. §12 identifies unresolved
standards requirements. SolTime defines a civil-clock mapping over a supplied time
reference; it does not redefine the SI second or replace UTC.


## §0 — The goal

Civil time should put daylight where people are: across the workday and the school day.
The proposed policy assigns a ceiling to modeled sunrise and puts the remaining
daylight later on the clock. The motivation is light for school travel and usable
evenings. Actual school schedules, terrain, weather and health effects are not modeled,
so this policy does not guarantee daylight at every bus stop or optimal social outcomes.

People use **one clock all year**, and the clock itself works out when each day starts
and stops. Days grow or shrink by seconds to minutes with the season, and one common
pattern (§9) says where in the day the seconds are lengthened or shortened, and by how
much: the clock owner’s basis night absorbs the change. At a fixed location outside the
adjustment windows, the rate equals the input rate. Positive-width zone ramps are
continuous away from the ±180° date boundary; hard edges, relocation and follow-mode
changes can produce steps. Fast motion can produce a backward-running clock.

Every app can show the same clock three ways: the raw SolTime field at your exact spot
(§1), the zone you are in (§8, a metro locked to one clock), or any other zone's clock.
A device's displayed clock follows the true clock, recomputing it every few minutes and
setting the length of its seconds to stay on it (§10).


## §1 — The one rule

```
offset(lat, lon, day) = ANCHOR − sunriseUTC(basisLat(lat, day), lon, day)
local = UTC + offset
```

Hold sunrise at the bus anchor (default **06:45**). `basisLat` is the place's own latitude
wherever the day and the night are both at least 4 hours long (§4): all of the inhabited
mid-latitudes, all year. For a fixed modeled daylight duration, this rule maximizes the clock reading at
sunset subject to the chosen sunrise ceiling:

- Constraint: modeled sunrise_local ≤ ANCHOR, on the raw field with its own basis.
- Objective: maximize modeled sunset_local for that day.
- Since sunset_local = sunrise_local + modeled daylight duration on the daily
  plateau, that objective drives sunrise_local to ANCHOR before smoothing.

This conditional optimization does not establish an optimal work/school schedule,
circadian outcome or amount of useful daylight for all people.

Consequences, automatically:
- **Winter** (short days): a modeled 9-hour day beginning at 06:45 ends at 15:45.
  The policy allocates more of that daylight to the morning.
- **Summer** (long days): sunrise still 06:45, so the entire seasonal surplus spills into
  the evening — a 15-hour June day at 40°N sets at ~21:48 local.
- **Drift**: sunrise moves ~0–2.5 min/day at mid-latitudes, so the clock slides by
  seconds-to-minutes per day by nature, all of it absorbed at night (§9). No cliffs, no
  spring-forward.


## §2 — Solar math

NOAA general solar position equations (declination + equation of time as Fourier series in
the fractional year, hour angle at zenith 90.833° = official sunrise incl. refraction).
These are approximate, sea-level solar times; sub-minute astronomical accuracy has not
been established, especially near polar sunrise/sunset. The equations are described in
[NOAA's solar calculation notes](https://gml.noaa.gov/grad/solcalc/solareqns.PDF).
Measured against the Meeus low-precision solar position at each event's own time, 2026
events differ by up to 128 s at sunrise and 153 s at sunset in Indianapolis, and up to
348 s in Anchorage; the overview's section 5.1 gives the figures, the two causes and
the measured upgrade path (`android/evaluation/SolarAccuracy.java`).

- At midday, fractional year γ = 2π·dayIndex/daysInYear, with a zero-based day index
  and 365 or 366 days according to the Gregorian calendar. There is no extra half-day
  term: NOAA's `(hour − 12)/24` is zero at midday. The legacy periodic API uses 366.
- Per-day solar params (decl, eqtime) are evaluated at that day's midday.
- `sunriseUTC = 720 − 4·(lonE + haDeg) − eqtime` minutes (lonE positive east).

Longitude enters the offset as a clean linear term: **+240 s per degree east**. Erosion
smoothing (§3) commutes with a constant shift, so the year table is computed once per
latitude at lon 0 and the longitude term is added after — this is also what makes travel
continuous: driving 1° west costs exactly 4 minutes, spread smoothly over the miles. (The
4 minutes are exact for each day's value; the night's adjustment window (§9) moves with
longitude too, so during the night two places 1° apart can differ by a few seconds more
or less for a moment.)

**The model date boundary.** At ±180° the two sides are on different local days, so the clock follows
two different days' schedules there:  besides the date, the time of day steps by that
day's slide (at most `max_slide`, 4 minutes; under 2 minutes at mid-latitudes). It is a
step in the raw field; a device crossing it shows "New Time Zone" (§10). This meridian
does not follow the political International Date Line. Hard-edged zones and device
relocations can also step. Longitude at an exact geographic pole is a coordinate
convention; a single location-independent pole clock remains a standards decision.


## §3 — Smoothing: the slope-limited lower envelope

At high latitudes the raw target can swing faster than civil life should. The published
offset is the **erosion** of the raw target by a cone of slope `max_slide_secs` per day
(default **240 s/day**). The legacy periodic API uses circular distance over 366 days:

```
smoothed[i] = min over j of ( target[j] + max_slide · circularDays(i, j) )
```

The real-calendar API uses the same minimum with `abs(i − j)` over actual Gregorian
dates, so both leap years and common years join without a skipped day. Its cached table
uses nine full neighboring years on each side. This includes both solar target types
in both directions, including the eight-year leap-day gap at a skipped century; a more
distant repetition cannot improve the envelope.

Three properties, all load-bearing:
1. **Slope-limited**: |smoothed[i+1] − smoothed[i]| ≤ max_slide. No cliffs, including
   across New Year (both calendar and periodic erosion preserve this bound).
2. **Never above target**: smoothed[i] ≤ target[i], with equality wherever the erosion limiter is inactive. Since
   sunrise_local = sunriseUTC + offset and target = ANCHOR − sunriseUTC:
   **For the raw field, modeled sunrise_local ≤ ANCHOR on every day a place keeps
   its own basis (§4).** This is a property of the model, not observed bus-stop lighting. Where the limiter binds, sunrise
   comes *earlier* than the anchor — the safe direction.
3. **Tightest such curve**: it is the maximum of all slope-limited curves below target,
   so no daylight is given up beyond what the no-cliffs rule forces.

Computed in O(n) by a two-pass sweep: forward `s[i] = min(s[i], s[i−1] + max_slide)`,
then backward `s[i] = min(s[i], s[i+1] + max_slide)`, two laps each around the circle.
Bit-identical to the direct min over j (verified Oct.03.2026).

The table gives one offset per day: the value in force through that day's daylight. How
the clock moves from one day's value to the next is §9.


## §4 — Polar regions: the basis latitude (ruled Oct.03.2026 RCH)

Where the sun never rises under the model’s zenith convention (polar night), never sets (midnight sun),
or does so only briefly, a place takes its timing from the nearest latitude toward the
equator where real day and night happen — its **basis latitude** for that day:

```
basisLat(lat, day) = lat                     if day ≥ M and night ≥ M at lat
                   = latitude where day = M   short-day side (winter)
                   = latitude where night = M long-day side (summer)
```

taking the root in the same hemisphere, between the equator and lat. **M =
`basis_min_secs`, default 4 hours.** Closed form: with hour angle H0 = M/2 (or 12 h − M/2)
and declination δ, `sin φ·sin δ + cos φ·cos δ·cos H0 = cos zenith` is
`a·cos φ + b·sin φ = c`, so `φ = atan2(b, a) ± acos(c / √(a² + b²))`. (Verified against
bisection to 4·10⁻¹⁴°.)

`basisLat` is a clamp, so it is continuous in latitude and in day, and so is the clock.

At the solstices the basis starts at **64.4N** (December, day = 4 h) and **62.5N** (June,
night = 4 h); every place equatorward keeps its own latitude. Local clock, lon 0:

| place | Dec 21 | Jun 21 |
|-------|--------|--------|
| 60N | sunrise 06:45, sunset 12:38 | sunrise 06:45, sunset 01:37 |
| 65N | sunrise 06:57, sunset 10:33 | sunrise 05:43, sunset 03:47 |
| 69.6N (Tromsø) | polar night; twilight peak (solar noon) 08:45 | midnight sun; solar noon 16:45 |
| 78N (Svalbard) | polar night; solar noon 08:45 | midnight sun; solar noon 16:40 |

**The price, stated plainly.** Continuity forces the places just short of polar night —
sun up for less than 4 hours — to share the borrowed clock, so their sunrise comes after
ANCHOR, by at most M/2 (measured worst: 113 min at 70.5N). In polar night the brightest
moment of the twilight lands at ANCHOR + M/2 = 08:45, inside the morning. The modeled sunrise ceiling of
§1/§3 applies to the raw field wherever a place keeps its own basis.

`soltime_sunTimesUtc` still reports the place's own raw sun (+1 polar day, −1 polar
night) so callers can render it honestly; `soltime_basisLatDeg` reports the basis.


## §5 — API (i/ii convention: no-prefix validates, iSolTime_* internal)

```c
struct SSolTime {                       // params; soltime_defaults() fills, NULL = defaults
    f64  anchor_secs;                   // sunrise anchor, secs after local midnight (24300 = 06:45)
    f64  max_slide_secs;                // erosion slope limit, secs per day (240)
    f64  zenith_deg;                    // sunrise zenith (90.833 = official, incl. refraction)
    f64  basis_min_secs;                // M:  min day AND night at the basis latitude (14400 = 4 h; §4)
    f64  night_start_secs;              // earliest start of the night's adjustment (72000 = 20:00; §9)
    f64  night_end_secs;                // latest end of the night's adjustment (14400 = 04:00; §9)
    f64  max_correction;                // display correction bound, +/- secs per sec (0.5 = 0.5x..1.5x; §10)
    f64  design_speed_kmh;              // zone ramps keep this speed inside max_correction (128.75 = 80 mph; §8)
    f64  ramp_min_secs_per_km;          // ramp slope floor where the budget leaves no room (2; §8)
    f64  check_tolerance_secs;          // auto-flex checkpoint tolerance (0.05; §10)
    f64  check_max_secs;                // longest checkpoint interval (300 = 5 min; §10)
};

void soltime_defaults      (SSolTime* p);
f64  soltime_offsetSecs    (SSolTime* p, f64 latDeg, f64 lonDegE, f64 day);          // NAN on invalid
f64  soltime_localFromUtc  (SSolTime* p, f64 latDeg, f64 lonDegE, f64 utcSecsOfYear); // NAN on invalid
s32  soltime_sunTimesUtc   (SSolTime* p, f64 latDeg, f64 lonDegE, f64 day, f64* riseSecs, f64* setSecs, f64* noonSecs);
                                        // the place's own sun at p's zenith:  0 normal, +1 polar day,
                                        // -1 polar night, -2 invalid
f64  soltime_basisLatDeg   (SSolTime* p, f64 latDeg, f64 day);                         // §4; NAN on invalid
s32  soltime_nightWindowUtc(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day, f64* startUtc, f64* endUtc);
                                        // §9:  tonight's adjustment window (after local day floor(day)),
                                        // UTC seconds of year;  0 ok, -2 invalid
f64  soltime_distanceKm    (f64 lat1, f64 lon1, f64 lat2, f64 lon2);                   // great circle, R = 6371

// Zones (§8)
f64           soltime_zoneRadiusKm        (SSolTime* p, f64 quantumSecs, f64 latDeg); // honest auto radius
f64           soltime_zoneQuantumForRadius(SSolTime* p, f64 radiusKm, f64 latDeg);
SSolRegistry* soltime_registryCreate       (SSolTime* p, const SSolZone* zones, s32 nZones); // NULL on invalid
void          soltime_registryDelete       (SSolRegistry* r);
f64           soltime_zoneLockKm          (SSolRegistry* r, s32 i);                   // zone i's disc radius
f64           soltime_zoneRampKm          (SSolRegistry* r, s32 i);                   // its merged zone's ramp width
f64           soltime_zoneJumpSecs        (SSolRegistry* r, s32 i);                   // its merged zone's J (§8)
s32           soltime_zoneLeader          (SSolRegistry* r, s32 i);                   // its merged zone's leader;  -2 invalid
s32           soltime_zonesAt             (SSolRegistry* r, f64 latDeg, f64 lonDegE, s32* idx, f64* weight, s32 capacity);
                                        // every merged zone covering the point (its leader), in layer
                                        // order (last = on top);  returns the total count, fills up to
                                        // capacity;  -2 invalid
f64           soltime_zoneOffsetSecs      (SSolRegistry* r, s32 i, f64 day);         // zone i's merged zone's clock, anywhere
f64           soltime_offsetSecsR          (SSolRegistry* r, f64 latDeg, f64 lonDegE, f64 day); // the layered clock here

// Device clock (§10)
SSolClock*    soltime_clockCreate          (SSolTime* p, SSolRegistry* r);            // r nullable = raw field only
void          soltime_clockDelete          (SSolClock* c);
s32           soltime_clockFix             (SSolClock* c, f64 utcSecsOfYear, f64 latDeg, f64 lonDegE, f64 speedKmh, f64 headingDeg);
                                        // feed each position fix, GPS or typed in (heading degrees clockwise
                                        // from north);  0 ok, -2 invalid
f64           soltime_clockShow            (SSolClock* c, f64 utcSecsOfYear, u32* flags);
                                        // the local seconds-of-year to display now;  flags (nullable):
                                        // SOLCLOCK_NEW_ZONE (jumped since the last call), SOLCLOCK_FAST
                                        // (moving faster than the correction can follow), SOLCLOCK_NO_FIX
                                        // (no position yet:  the call returns NAN)
void          soltime_clockExact           (SSolClock* c, s32 on);                     // exact mode:  the true value every call
s32           soltime_clockFollowZone      (SSolClock* c, s32 zone);                   // keep zone's clock wherever the device is;
                                        // -1 = the clock of where it is;  0 ok, -2 invalid (needs the registry)
s32           soltime_clockFollowing       (SSolClock* c);                             // the leader followed, or -1
f64           soltime_clockNextCheckUtc    (SSolClock* c);                             // when the next checkpoint is due
```

Conventions: `day` is a continuous day-of-year, `0.0` = Jan 1 00:00 UTC, valid `[0, 366)`;
the offset at a fractional day follows the seconds pattern (§9). Sun times are UTC seconds
of that day (raw — near the date line they legitimately fall outside [0, 86400)).
Longitude is degrees east, `[-180, +180]`; latitude `[-90, +90]`. C ABI (`extern "C"`),
ships as `libsoltime.so`. `SSolRegistry` and `SSolClock` are opaque; create/delete own
them. `soltime.h` is self-contained in C11 and C++: it defines the fixed-width aliases
and structure typedefs without requiring the repository's private headers. The
`c_public_api` CTest compiles an isolated copy of this header and links a C client to
the shared library.

The apps use the Gregorian API: `soltime_offsetUnixSecs`, `soltime_offsetUnixSecsR`,
`soltime_zoneOffsetUnixSecs`, `soltime_solarDateUnix`, `soltime_sunTimesUnix`, `soltime_basisLatUnix`,
`soltime_nightWindowUnix`, and `soltime_clockCreateUnix` (see `soltime.h`). These accept
Unix seconds for years 1600–9999. A calendar clock's fix, show and next-check values
also use Unix seconds; its shown value is UTC plus the local offset. Sun/night date
arguments select a UTC-labeled day and return absolute instants. For today's sun and
basis at the physical location, use `soltime_solarDateUnix(p, lat, lon, utcUnix)`;
it returns `floor((utcUnix + rawFieldOffset)/86400)*86400`. Render the resulting
events in the chosen clock afterward. A distant followed clock can be on another
date and must not select the physical location's sunrise. For the overnight adjustment,
select the date using the clock owner's coordinates instead. The Java method is
`solarDateUnix`. Do not reduce calendar timestamps to seconds-of-year. A followed
zone works before the first location fix.

Parameter validation (no-prefix layer): ANCHOR in [0, 86400); max_slide > 0; zenith in
(0, 180); 0 < M < 12 h; night_end < ANCHOR < night_start; **M − (ANCHOR − night_end)
> 2·max_slide**, and **86400 − night_start + night_end > 3·max_slide**, which keep
custom night windows long enough for the adjustment without running backward (§9);
0 < max_correction < 1; design_speed > 0; ramp_min > 0; check_tolerance > 0;
check_max ≥ 1 s.


## §6 — Calendar integration and remaining simplifications

- Both apps use real Gregorian dates and Unix/POSIX-style seconds. The APIs do not
  encode UTC leap seconds or normalize host clock smears; see §12. The periodic 366-day
  API remains available for synthetic simulations and registry design calculations.
- Calendar field tables are cached per thread by latitude, year and solar parameters;
  the zone registry (§8) does its heavy work once, at create. The publisher always
  recalculates derived jumps and ramps, including when its input was already published.
- No altitude/horizon-dip term; sea-level official sunrise.
- No schedule is modeled beyond the chosen sunrise ceiling. Maximizing sunset under
  that constraint (§1) is a policy objective; it does not prove maximal workday coverage
  or a health/safety benefit. An evening constraint would need a separately reviewed rule.
- The sunrise bound is a raw-field property where a place keeps its own basis. Sharing
  a city's clock, blending through a zone ramp, or following a distant zone can put
  the physical sunrise after 06:45. Zone sharing does not enforce a zone-wide bus floor.
- Scheduling from local time back to UTC, recurrence semantics, stable zone identifiers,
  registry version/update coordination, and Android widgets/alarms/background operation
  remain future work. Independent astronomical validation and physical-phone testing
  also remain open; see [the bug-fix plan](BUG_FIX_PLAN_2026-10-05.md).


## §7 — Tests (`soltime_tests`, unity-build over soltime.cpp, `/test:<name>` dispatch)

Run `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build
--output-on-failure`. CTest also runs `soltime_calendar_tests` (real New Year, century
rules, sunrise/night invariants, travel across the date line and following without GPS),
`soltime_review_tests` (motion transitions, distant-follow solar dates and custom ramps),
the standalone C client, the Java location/review tests when a JDK is available, and
publisher validation/recalculation when Python 3 is available. Publisher tests include
malformed/empty input, long rows, in-place publication and failed-write preservation.
`difftest/run_difftest.sh` compares C++ and Java for both
periodic and calendar APIs. Date-line travel applies the date jump at the crossing;
the servo never interpolates it as a velocity and emits one `NEW_ZONE` notification.

| test        | checks in the implemented scenarios |
|-------------|--------|
| `noaa`      | solar engine vs known ephemeris values (Boulder Jun 21 sunrise/solar noon, equator day length) |
| `erosion`   | the O(n) sweep is bit-identical to the direct min over j at the sampled latitudes |
| `anchor`    | the §3 theorem **on the running raw-field clock**: at the modeled sunrise instant, the clock reads ≤ ANCHOR wherever a place keeps its own basis; exactly ANCHOR where the limiter does not bind |
| `slide`     | \|O(k+1) − O(k)\| ≤ max_slide per day, all latitudes incl. 89°/90°, across New Year |
| `travel`    | exact +240 s/°E for each day's value; sampled continuity in longitude away from ±180°; the date boundary:  a whole day, plus that day's slide (≤ max_slide) |
| `basis`     | §4: closed form vs bisection; own latitude wherever day and night ≥ M; continuity in latitude and day; polar-night solar noon at ANCHOR + M/2; the sunrise slip ≤ M/2 |
| `polar`     | finite/continuous through polar transitions; correct ±1 status flags |
| `night`     | §9: the window lies inside the basis night and inside night_start..night_end; the offset is constant outside it (true seconds all day); the window's length and the peak stretch, by latitude band; where local midnight falls |
| `localtime` | stationary raw-field monotonicity sampled over a synthetic year at selected latitudes |
| `zone`     | §8: honest radius holds inside the disc (all bearings, all days) and is tight; capped-lead ramps:  no step in the sampled positive-ramp cases (around a merged zone's outline and through the notch where two discs meet, between two zones side by side), every disc reads exactly its leader's clock at every instant tried, 80 mph inside ±0.5 s/s through every ramp the budget fits, the 2 s/km floor where it does not, hard edges on request; merging (a town inside a city's disc, two discs side by side, a chain of three) and the most populous leader; a later ramp stops in the gap; given jumps and ramps reproduce the computed clock exactly; a far neighbor leaves a ramp alone; zonesAt lists and orders; registry validation |
| `clock`     | §10: display-to-model gaps in stationary (30 days) and 80 mph zone-edge scenarios; the 50 ms midpoint tolerance is not a global maximum (observed driving gap 69 ms); the interval grows to 5 min standing still; "New Time Zone" for a relocation, once, and not for a walk; fast motion (airliner with turns, jet straight over a zone) within a second or so; exact mode equals the true clock every call; dead reckoning without GPS; New Year without a jump; following a zone from far away (Denver on Indianapolis's clock), and back |
| `valid`     | out-of-range inputs → NAN / −2; parameter validation incl. the §9 window rule; NULL params = defaults |


## §8 — Zones (author decisions: July 11 and October 3, 2026)

A SolTime zone extends a city’s clock across a geometric area. Positive-width ramps
avoid a step at the edge; explicitly requested zero-width ramps are hard edges.

A **zone** is a region that locks to one city's SolTime — so a metro coordinates on a single
clock while the planet stays continuous. The city's time still slides daily (§1 and §9 apply at
the city); the zone just extends that one clock spatially.

Zero or more zones are at work at any location. Inside a zone the time is the same across its
whole area; it changes only when you move from zone to zone, or back out to the raw SolTime
field. A zone is a named clock, not a wall: apps know every zone they are in
(`soltime_zonesAt`), and can show any zone's clock (`soltime_zoneOffsetSecs`) or the raw field
(`soltime_offsetSecs`) at any time.

**One zone per city** (RCH Oct.03.2026). Every city of 100,000 people or more has a zone:  a
disc around the city reaching **1.5× the city's radius**. A city's radius is declared — a city
can set its own — with measured defaults (`data/make_zones.pl`):

- U.S.:  the larger of the city limits (U.S. Census Gazetteer land area) and the dense urban
  core (GHSL Urban Centre Database R2024A);
- elsewhere:  the GHSL dense urban core;
- neither:  estimated from the population at the country's median urban-core density.

The radius is √(area / π), an equivalent-area circle, not a bounding circle for
the municipal shape. Even a 1.5× disc need not contain an entire irregular city.
Indianapolis:  17.3 km (its Census city limits) → a 25.9 km zone
(16 mi). Anchorage:  its municipality's 1,700 square miles of land → a 56 km zone (35 mi).
5,854 cities qualify (GeoNames cities15000:  cities, not their neighborhoods and districts; the
radius came from the Census for 252, from GHSL for 3,259, by estimate for 2,343).

**Overlapping zones merge** (RCH Oct.03.2026). The center distance must be strictly
less than the sum of radii; merely touching discs do not merge. Overlaps merge transitively,
into one zone:  the union of their discs, on the clock of its **most populous city** (its
leader). There is no size cap. The 5,854 cities make 4,168 zones (3,729 of them a single city).
New York City's zone takes in 14 cities (the five boroughs, Newark, Jersey City, Yonkers,
Paterson, Elizabeth, Stamford, Edison) and reaches 68 km from Manhattan; Los Angeles's, 35 cities
and 104 km; the widest, Shanghai's, 39 cities and 176 km. Zones cross national borders where
cities do:  17 do (San Diego on Tijuana's clock, El Paso on Ciudad Juárez's, Windsor on
Detroit's, Hong Kong and Macau on Guangzhou's). Ruled (RCH Oct.03.2026):  zones extend across
borders — where people do business, they need to know the local clock. (The prototype's
choice; a jurisdiction could still stop a merge at its border.)

**Anyone may follow any zone** (RCH Oct.03.2026). A person or device anywhere can keep any
zone's clock instead of the clock of where it is (`soltime_clockFollowZone`, §10):  a family in
a suburb outside a zone, a business with offices in two cities, a traveler keeping home time.

```c
struct SSolZone { f64 latDeg, lonDegE;   // the city
                  f64 quantumSecs;       // only for an auto radius:  the accuracy class
                  f64 radiusKm;          // the disc;  <= 0 = auto (the honest radius of quantumSecs)
                  f64 rampKm;            // < 0 = auto, 0 = hard edge, > 0 = requested width, capped at earlier discs
                  f64 population;        // the leader of a merged zone is the most populous
                  f64 jumpSecs; };       // J;  <= 0 = compute, > 0 = as given (the published registry)
```

**Modeled deviation: J.** The publisher estimates the largest absolute difference
between the zone clock and the clock beneath its layer (the field plus earlier zones),
using daily plateau values in the synthetic 366-day design year. It samples member
rims every 5°, refines between samples, and examines interior rings affected by earlier
zones. This numerical estimate is not a certified maximum over every point, calendar
year and instant, nor necessarily a bound against each member’s raw field. During
night adjustment, the city and edge windows differ and the gap can exceed J. A formal
standard needs separate definitions and validation for these different error measures. Over the
4,168 zones:  median 0.3 min, 90% within 0.9 min, the worst 8.3 min (Shanghai's merged zone;
next, Anchorage, one city at 61N). Indianapolis:  1.4 min. New York City:  3.7 min.

**The honest radius — an auto disc.** Where no radius is given, a zone's disc is the honest
radius of an accuracy class:  the largest R such that, everywhere within R of the center and on
every day of the year, the zone’s daily clock values are estimated to be within quantum/2 of the raw field
there in the design year. The field
changes north–south as well as east–west (seasonally, and strongly at high latitude), so the
radius comes from the field itself — every bearing at 5° steps and refined between them, all 366
days — not from longitude alone (measured, spherical earth R = 6371 km):

| class | 0° | 30N | 40N | 50N | 60N | covers |
|-------|----|-----|-----|-----|-----|--------|
| 1 min (60 s) | 12.8 km | 10.6 km | 9.0 km | 6.9 km | 4.0 km | a town |
| 5 min (300 s) | 63.8 km | 53.1 km | 45.0 km | 34.3 km | 19.7 km | a large city |
| 10 min (600 s) | 127.5 km | 106.1 km | 89.8 km | 68.4 km | 39.1 km | city + surrounding counties |

`soltime_zoneQuantumForRadius` inverts it:  the smallest standard class (60/300/600/1200 s)
whose honest radius covers a radius at that latitude; beyond 1200 s, whole minutes. A given
radius (every city's zone) carries J as its error bar instead.

**The ramp: a capped lead, outside the discs.** Inside a zone’s discs the time of day agrees with its
leader modulo one day. Date agreement across ±180° needs a separate rule. Past a disc's rim, the zone's *lead* over the clock beneath it (the difference between
the two) is capped, and the cap falls to 0 at the ramp's end:

```
lead  = wrapDay(zone − beneath)
cap   = max over the member discs whose ramp holds the point of
            max(J, |wrapDay(zone − beneathAtRim)|) · (1 − (distance − R) / rampKm)
clock = beneath + clamp(lead, −cap, +cap)                        R ≤ distance ≤ R + rampKm
```

`wrapDay(d)` is the IEEE remainder of d by 86,400 (nearest integer quotient, ties
to even). The rim is on the query point’s bearing from that member’s center. Wrapping
can itself change branch at half-day differences; global continuity across that branch
or the date boundary is not asserted.

Where the lead is inside the cap the clock is still the zone's; where it is capped, the clock
moves with the clock beneath plus the shrinking cap. The ramp-sizing heuristic uses J / rampKm as its nominal additional spatial slope. (A linear blend toward the field counts the
field's slope twice, because the field keeps moving away across the ramp; measured, that overran
the drive budget from about 47N northward.)

Why the rim term:  at every disc's rim the cap is never below the lead there, so the ramp meets
the lock exactly — at every instant, not only on each day's value, and all the way around a
merged zone's outline, the notches where two discs meet included. The night glides (§9) of the
city and of the rim do not run in lockstep (their windows are minutes apart), so for a moment the
lead can exceed J by a few seconds; a cap of J alone would leave a small step at the rim then.
The rim term closes that mismatch. J still sets the nominal ramp width, but the
instantaneous cap also varies with the rim lead. A universal speed bound would need
to account for that variation; the existing speed results are numerical scenario checks.

**The ramp’s nominal slope comes from the drive budget.** Ordinary device correction
is bounded at ±0.5 s per input second (§10). Automatic ramps target that budget at
`design_speed` (**80 mph**); the floor, narrow gaps and instantaneous rim corrections
limit the scope of the claim:

```
slope  = (max_correction − night rate) / design_speed − steepest slope of the clock beneath
         (night rate:  §9's peak at the leader;  steepest:  over the ramp, all year)
slope  = max(slope, ramp_min_secs_per_km)             the floor, 2 s/km
rampKm = J / slope
```

"Over the ramp" is the ramp being sized, so the measurement starts from the shortest ramp the
budget could allow and grows until the ramp it implies fits inside the ring it was measured on —
the smallest consistent ramp. (Starting from a wide ring let a far neighbor's ramp into the
measurement:  Indianapolis's ramp came out 89 km beside Chicago's, against 29 km alone.)

Measured on a 10-minute class disc alone (`/test:zone`; the 80 mph column is the steepest
anywhere in the ramp, any bearing, any day, plus the night stretch):

| 10-min disc | radius | ramp slope | ramp | 80 mph needs |
|-------------|--------|------------|------|--------------|
| 30N | 106.1 km | 11.0 s/km | 27.3 km | 0.499 s/s |
| 40N | 89.8 km | 10.4 s/km | 28.9 km | 0.499 s/s |
| 50N | 68.4 km | 9.2 s/km | 32.5 km | 0.498 s/s |
| 55N | 55.2 km | 8.0 s/km | 37.4 km | 0.497 s/s |
| 60N | 39.1 km | 5.1 s/km | 58.4 km | 0.496 s/s |
| 64N | — | 2.0 s/km (floor) | 150 km | 0.541 s/s — keeps up to 73 mph |

The city zones:  Indianapolis's ramp is 8.2 km (5 mi), New York City's 21 km (13 mi),
Anchorage's 171 km (106 mi:  61N, where the field itself is steep).

North of about 62N the field alone uses most of the budget, so the floor applies. There a car
faster than the "keeps up to" speed is moving faster than the correction can follow, so its clock
switches to the fast-motion rule (§10) and shows the true time at least once a minute. Road speeds
that far north are mostly below it anyway.

**Side by side.** Zones whose discs overlap are one zone. Where zones' ramps would reach each
other, a later zone's ramp stops in the gap before any earlier zone's disc — steeper if the gap
is short:  two 10-minute discs at Kolkata and Dhaka, 22 km apart, leave an edge of about
30 s/km between them, so a car at 80 mph crosses it under the fast-motion rule.

`rampKm = 0` gives a hard edge if a jurisdiction wants one: crossing it is a step, and the device
shows "New Time Zone" (§10). A supplied positive `rampKm` is capped at the gap to
earlier groups' discs too; a requested ramp must not overwrite an earlier locked clock.
Nonconflicting requested widths are retained.

**Layers, not winners.** Zones apply in a fixed order — the most populous leader first; equal
populations in registry order — and each one caps its lead over the clock beneath it (the field
plus every zone applied before it):

```
clock = field
for each zone, in layer order:   clock = clock + clamp(wrapDay(zone − clock), −cap, +cap)
```

`cap` is unlimited inside the zone's discs, falls to 0 across the ramp, and is 0 beyond. Each
layer is continuous (a clamp of continuous values), and the order does not depend on where you
are, so positive-width layers preserve continuity where the underlying clocks and
wrapped lead remain continuous, away from the date boundary and half-day branch changes.
Hard edges are explicit exceptions. A changing choice of layer winner is avoided. Where a smaller zone's ramp reaches into a bigger zone's ramp, it ramps into
that clock, not the bare field.

**Registry.** Discs, merging, jumps and ramps are computed once, in `soltime_registryCreate`
(a latitude-sorted neighbor search keeps it near-linear); queries are then cheap.
`soltime_zonesAt` lists every merged zone covering a point (by its leader) with its weight, in
layer order (the last one is on top); `soltime_zoneLeader` names a city's leader.

**Published.** `data/zones.tsv` carries every zone's ramp and jump, computed once by
`soltime_zonetool` (21 s for the 5,854 cities), and each city's leader (the `zone` column:  the
leader's row). Given jumps and gap-capped ramps are reused, so a device builds the whole registry from the
file in 0.1 s — and implementations reuse the same published inputs. Cross-language results agree
within the measured tolerances (§11); bitwise equality across platforms is not established. Any edit to the rows means
running `soltime_zonetool` again (it is a fixed point:  run on its own output, it writes the same
file). The publisher accepts complete 11-column raw or 13-column published rows,
validates finite numbers and coordinate/radius ranges, and rejects an empty registry.
It recalculates derived values, checks a temporary output file, then atomically replaces
the destination. Invalid input or a failed write leaves an existing destination intact,
including when rebuilding in place; diagnostics identify malformed rows and columns.

Zone membership is **geometric and day-independent** (great-circle discs plus their ramp) —
a county is in the zone in June and December alike. Iso-offset membership was considered and
rejected: the offset field breathes seasonally, and civil identity must not.


## §9 — The seconds pattern (ruled Oct.03.2026 RCH)

Each day has one offset — §3's value for that day — and it is in force from the end of
last night's adjustment to the start of tonight's. That covers the basis daylight, so
**the stationary raw-field rate equals the input rate outside its windows**. A
shared or followed clock uses its leader’s window, which can differ from local darkness.
The claim excludes motion and external input-clock steps or smears. On the running raw-field clock, sunrise is at or before ANCHOR wherever the
place keeps its own basis, and exactly ANCHOR when the erosion limiter does not bind.

Between day k and day k + 1 the clock moves from O(k) to O(k+1) during the night:

```
window = [ max(sunset(k), 20:00 by O(k)),  min(sunrise(k+1), 04:00 by O(k+1)) ]
```

Sunset and sunrise are the basis latitude's (§4), so a midnight-sun place still has a
night to work in. The window is the night, kept inside 20:00–04:00 local
(`night_start_secs` / `night_end_secs`). Within it the change is spread as a smooth bell
(raised cosine). With x the position in the window, 0 to 1:

```
offset = O(k) + (O(k+1) − O(k)) · (x − sin(2πx) / 2π)
```

so the clock's rate eases in and out, with no kink at either edge, and peaks mid-window at
1 + 2·ΔO / window. Local midnight is where the clock reads 00:00 — usually inside the
window (8,331 of 9,801 nights in `/test:night`); where the night starts after midnight
(high-latitude summer), just before it.

Local date boundaries occur when the clock passes a multiple of 86,400 displayed
seconds. For two successive local midnights t0 and t1 at a fixed location:

```
t1 − t0 = 86400 − (offset(t1) − offset(t0))
```

The offsets here are the instantaneous smoothed values at midnight, not necessarily
the daily plateau values O(k) and O(k+1). One night’s plateau change is ΔO; a
midnight-to-midnight interval may include parts of two adjustments. Input seconds
match elapsed SI seconds only under the stated input-clock assumptions (§12).

**Reported default samples and rate formula:**
- The nominal shortest window is M − (ANCHOR − 04:00) = **1 h 15 m**. The existing
  test allows one daily slide below that value (71 minutes with defaults); this is
  not an exact 75-minute universal bound. Parameter validation in §5 is intended to
  keep windows long enough for a positive stationary clock rate.
- The table measures peak |clock rate − 1| = 2·|ΔO| / window, not the fractional
  change in the duration of a displayed second. That duration is the reciprocal rate.

| \|lat\| | shortest window | peak clock-rate deviation |
|---------|-----------------|---------------------|
| 0–45 | 5.6 h | 0.8 % |
| 45–55 | 3.9 h | 1.4 % |
| 55–60 | 2.4 h | 2.4 % |
| 60–89 | 1.25 h | 10.6 % |

North of 60° in summer the night is short and late, so the day's change is squeezed into
the part of the night before 04:00.


## §10 — The device clock (ruled Oct.03.2026 RCH)

The true clock (§1–§9) is a function anyone can evaluate for any place and moment. A
device shows a **displayed clock** that follows it.

**Checkpoints.** At each checkpoint the device recomputes the true clock where it will be
at the next checkpoint (its position carried forward by its GPS speed and heading), and
sets the length of its seconds so the display lands exactly on it. Between checkpoints the
display runs at that steady rate. With correct input time and predicted position, each device aims at the same true
function at its checkpoints. Different update times, location errors and motion can
produce different displayed values between or at checkpoints. Earlier prototype
desktop timings reported about 0.12 ms for an uncached evaluation and 0.005 ms for
a cached latitude. Those are workload/hardware examples, not benchmarks for every
Gregorian/zone query or a maximum runtime.

**Auto-flex interval.** At each checkpoint the device checks its own prediction halfway
through the coming interval. If the halfway point would be off by more than
`check_tolerance` (50 ms), it halves the interval; if it is well inside (under a quarter
of it), the next interval doubles, up to `check_max` (5 minutes). Standing still it
settles at 5 minutes; moving, the interval follows speed, zone ramps and the night
pattern by itself. A GPS fix that departs from the predicted path re-plans at once. The
shortest interval is 0.1 s. Checkpoints happen at their scheduled moments however seldom
the display asks:  late ones are replayed in order (up to a minute late; after a longer
silence, one checkpoint now). Measured (`/test:clock`):

| case | checkpoints | worst gap, display vs true | jumps |
|------|-------------|----------------------------|-------|
| standing still, 40N, 30 days | every 5 min (288 a day) | 0.008 s | 0 |
| standing still, 70N, summer, 30 days | about every 5 min | 0.038 s | 0 |
| 80 mph east out of Indy's 10-min zone, 3 h | about every 3 min | 0.069 s | 0 |
| 80 mph west, December, 3 h | | 0.069 s | 0 |

**Bounded correction.** The display's rate stays within 1 ± `max_correction` (0.5: from
0.5× to 1.5×). Outside the clock owner’s adjustment windows, at a fixed location,
the selected raw/followed/locked clock’s modeled rate is 1 relative to input time.
Blends, motion and input-clock corrections need their own analysis.

**"New Time Zone."** If correcting at up to ±0.5 s per second cannot land the display on
the true clock within the next minute — the display would have to jump more than 30 s —
the device recomputes and shows the accurate present time at once, and the display shows
a note such as **"New Time Zone"**. That covers a phone switched on after a trip, a first
GPS fix far from the last one, and crossing a hard-edged zone, moving or not. The small
corrections of fast motion (below) are not new time zones and show no note.

**Ground-speed design examples.** The sampled open-field gradients below are within
the ordinary correction budget at 80 mph through about 65° latitude. Zone ramps target
that budget (§8), with polar and narrow-gap exceptions; these are not all-path bounds:

| latitude | open field, per km | 0.5 s/s reached at |
|----------|--------------------|--------------------|
| 40N | 3.3 s | 332 mph |
| 50N | 4.3 s | 252 mph |
| 60N | 7.6 s | 142 mph |
| 65N | 11.0 s | 85 mph |
| 70N | 13.2 s | 67 mph |
| 82N | 31.0 s | 30 mph |

Near the poles the lines of longitude crowd together, so each mile is many more seconds
of sun.

**Fast motion — no flight mode.** When the device moves so fast that the true clock
itself runs outside 0.5×–1.5× (a plane; a car above the "keeps up to" speed on a
far-north ramp), the correction bound cannot follow it, so the rule changes: at each
checkpoint, at most a minute apart, the device **shows the true time**, then runs at
whatever rate lands on the true clock at the next checkpoint, with no bound. When the
heading or speed changes (a turn), it re-aims at once from the time it is showing — the
new direction changes the sine and cosine of the velocity, and so the rate — instead of
waiting for the minute. The halfway check still applies, so the interval shortens where
the clock bends (a zone's edge). The small corrections at checkpoints roll quickly
through the digits. Zones apply in the air as on the ground: over a city, the clock is
the city's. This turn shortcut requires an already-fast clock, a remaining checkpoint
within one minute (or the configured shorter maximum), and an error small enough to
correct within one minute. A stationary-to-fast transition or a larger relocation
runs the full planner immediately; a relocation beyond that correction budget snaps
and emits "New Time Zone" instead of carrying a stale five-minute checkpoint.

"Moving fast" is judged locally:  the true clock outruns the bound over the next second.
Inside a planned interval too:  if the true clock would outrun the bound somewhere in it
(a minute that runs from a ramp into a lock, at jet speed), the interval shrinks, down to
0.1 s, and the fast-motion rule takes over there. Measured, a 900 km/h airliner with 90°
and 180° turns at 1.5 and 3° per second, a fix every second:

| case | worst shown vs true | jumps |
|------|---------------------|-------|
| from 40N | 0.12 s | 0 |
| from 60N | 0.52 s | 0 |
| from 70N | 0.32 s | 0 |
| straight over Indy's 10-min zone | 0.02 s | 0 |

Without re-aiming on turns (prototype), the minute corrections reach 55 s at 40N and 144 s
at 60N.

The seconds run fast flying east and slow flying west; at high latitudes, flying west,
the clock runs backward — the plane is outrunning the sun. It is the accurate time, so
the display shows it. At 900 km/h:

| latitude | flying east | flying west | north / south, at most |
|----------|-------------|-------------|------------------------|
| 0° | 1.54× | 0.46× | 1 ± 0.23 |
| 40N | 1.70× | 0.30× | 1 ± 0.44 |
| 60N | 2.08× | −0.08× | 1 ± 1.55 |
| 70N | 2.58× | −0.58× | 1 ± 2.90 |

When the motion slows back inside the bound, the usual rules resume.

**Exact mode** (a user setting): evaluate the true clock on each display call, with
no checkpoint approximation. The Android UI requests updates about every 200 ms.
Measured prototype runtimes are hardware/workload examples, not guaranteed maximums.
Exact mode does not improve uncertain location, reference time or the solar model.

**No GPS.** The device keeps following the true clock at its last known position, which
stays correct as long as it has not moved; moving fast, it carries the last speed and
heading forward. When a fix returns, the usual rules apply.

**Following a zone** (RCH Oct.03.2026:  anyone may be on any zone). The device can keep a
chosen zone's clock instead of the clock of where it is (`soltime_clockFollowZone`):  the true
clock is then that zone's leader's, wherever the device goes, and the same checkpoints follow it
(standing still, it is flat by day, so the interval settles at 5 minutes). Choosing a zone far
away is a jump, so the display shows "New Time Zone"; following back (`-1`) is another. Measured,
a device in Denver following Indianapolis's zone:  "New Time Zone" once, then within 0.007 s of
Indianapolis's clock for two days.


## §11 — Apps (Oct.03.2026 RCH)

SolTime is just math, so an app can show the time anywhere on Earth:

- **Your clock**, from GPS (§10), or from coordinates typed in by hand.
- **Any zone's clock**, and the raw SolTime field (§8).
- **Search** by country, and by the cities in each country. A result shows that place's
  clock (its zone's, if it is in one), the raw field, and its sunrise and sunset.

**Data** (`data/` in the full project; an identical evaluation snapshot is bundled in
`android/data/` for standalone handoff):
- `places.tsv`, `countries.tsv`:  GeoNames cities15000 (34,152 places with 15,000 people or
  more, each with its GeoNames feature code; 252 countries), CC BY 4.0 — every app shows the
  credit line **"Place data:  GeoNames (geonames.org), CC BY 4.0"** (`data/ATTRIBUTION.md`;
  `make_places.pl` rebuilds the files).
- `zones.tsv`:  the published zone registry (§8) — every city of 100,000+ people (5,854), its
  city radius and where it came from (Census, GHSL, or estimated), its zone's disc (1.5×), its
  merged zone's ramp and jump, and its leader. Built by `make_zones.pl` (GeoNames + the GHSL
  Urban Centre Database GeoPackage, read directly, + the Census Gazetteer; it matches each GHSL
  urban centre by name near its centroid, leaves out GeoNames' sections of cities (feature
  PPLX:  neighborhoods, districts) and drops its duplicate rows), then
  `soltime_zonetool` (the registry computed once). The GHSL data is (c) European Union,
  CC BY 4.0; the Census Gazetteer is public domain — both credited in `data/ATTRIBUTION.md`.

**The desktop display** (`soltime_display`, on the TUI library in `../tui`):
- F1 Clock:  your clock (§10) in large digits, the raw field, your zone's clock (its leader,
  how many cities share it, and its accuracy J), UTC, the zone you follow, today's sun in the
  clock you see, the basis, tonight's adjustment window, other zones' clocks.
- F2 World:  a character map — blue where the sun is up, land traced by the places and
  coloured by the local SolTime hour, the zones led by cities of 1M+; a cursor reads out any
  spot.
- F3 Simulate:  six scenarios against the device clock — standing still at 600×, 80 mph
  through a zone's edge, an airliner with turns, a relocation ("New Time Zone"), Tromsø
  through the year, a jet outrunning the sun.
- F4 Find:  a city, a country (its largest places), or typed coordinates; Enter makes it
  your location; F5 puts your clock on its zone (u, on the other screens, stops following).
- `TUI_BACKEND=offscreen SOLTIME_SELFTEST=1 ./soltime_display` runs every screen headless
  and prints them as text.

**The Android app** (`android/`, plain Java, no dependencies):  the globe, your clock (§10)
fed by GPS position, speed and heading, or by a place or coordinates you choose (Find → *Use as
my location*); Find, Exact, GPS, Me and Follow buttons (Follow:  the zone under the reticle;
Find → *Follow its zone's clock*); zone rings (each disc and its ramp edge), dots on the zones
led by cities of 1M+; the credit lines. Gradle uses the shared `data/` when present and the bundled `android/data/` otherwise.
A release must refresh and verify the bundled snapshot so both copies agree. `SolTime.java` is the library's Java port.

**The differential test** (`difftest/run_difftest.sh`):  the same queries through the C
library and `SolTime.java` — the field, sun times and windows, honest radii, the published
`zones.tsv` registry (every city's disc, ramp, jump and leader), the layered clock and the zones
around 30 zones, the U.S. cities with their jumps and ramps computed from scratch on both sides
(merging, J, the drive budget, the gap rule), and device-clock runs (standing, 80 mph, a jet with
turns, a relocation, following a zone). Measured Oct.03.2026:  the field within 10⁻¹¹ s, the
layered clock within 4·10⁻¹¹ s, every leader identical, the computed jumps and ramps within
4·10⁻¹⁰, the clock runs within 2·10⁻⁹ s.


## §12 — Evaluation status and remaining standards decisions

This revision describes the candidate clock rules and the prototype behavior. It is
not an adopted standard or a declaration that all implementations conform. The
[combined overview](SolTime_overview.md) is the portable review document; it includes
the formulas, policy choices, evidence, package contents and requested evaluation.

**Input time and leap seconds.** Calendar APIs take Unix/POSIX-style seconds since
1970-01-01, mapped through Gregorian dates with 86,400 nominal seconds per day. They
have no representation for 23:59:60, no leap-second table and no leap-smear correction.
The `UTC` labels in prototype formulas refer to that input coordinate. A production
standard must define its UTC mapping, input clock synchronization and behavior during
leap events and wall-clock corrections. SI-rate and monotonicity claims assume a
steadily advancing input clock. See [NIST’s leap-second explanation](https://www.nist.gov/pml/time-and-frequency-division/leap-seconds-faqs).

**Interchange and scheduling.** A displayed SolTime date/time alone is insufficient to
identify an instant. A future format must identify the clock basis (coordinates or a
stable followed-zone ID), parameter profile, registry revision and time-reference
policy. Preserve the underlying instant separately. RFC 3339 numeric offsets have
whole-minute resolution, so do not round a SolTime offset into that format and imply
exact equivalence. See [RFC 3339 §4.2](https://www.rfc-editor.org/rfc/rfc3339.html#section-4.2).
Local-to-reference inversion, recurring events, registry changes affecting future
appointments, date-boundary cases and durations measured while moving remain open.
A recurring local 08:00 meeting is not a fixed daily UTC event.

**Candidate conformance structure.** Review the stationary raw field, zone registry,
and device display separately. A core implementation should reproduce the reference
formulas and declared parameters; a zone implementation additionally needs the same
registry bytes and layer order; a device implementation must declare its display and
location policies. Define mandatory rules separately from UI recommendations and
optional profiles before certification. Existing numerical test thresholds and the
50 ms planning heuristic are not a general conformance tolerance. Publish versioned
input/output vectors, error budgets, rounding rules and supported date/coordinate
ranges before calling independent implementations conformant.

**Governance and scope.** Agree on stable zone IDs, registry schema/release numbers,
activation instants, retained historical releases, update authority, rollback behavior,
source authentication, jurisdiction boundaries and an exact-pole convention. The
prototype's row indices are not durable public zone identifiers. The geographic
model is spherical (R = 6371 km), and the solar model uses an unobstructed sea-level
horizon. Different astronomical or geodetic models require an identified new profile.

**Evidence.** Automated checks establish agreement in their tested scenarios. They
do not establish astronomical accuracy, universal error bounds, human benefits or
physical-phone reliability. Independent ephemeris comparison, adversarial boundary
analysis and a pilot remain necessary. Report modeling error, registry deviation,
device tracking error and source-time/location uncertainty separately.

**Reproducibility and rights.** Supply a dated source/data snapshot and hashes, build
instructions and the verification record. The evaluation package carries the full
project: the C++ implementation, its tests and data tools, the differential test, and
the Android app with its bundled data, all as source; `VERIFICATION.txt` records its
checks. Third-party data attribution is supplied with the snapshot. The author
dedicates the SolTime software and documents to the public domain (CC0 1.0, `LICENSE`);
the third-party data keep their own terms. Standards-body submission and legal adoption
are separate processes from publishing this evaluation draft.
