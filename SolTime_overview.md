# SolTime — a proposed solar-anchored civil-time standard

*Rick C. Hodgin — October 5, 2026*

## In brief

**The idea.** A phone already knows where it is and what time it is. SolTime uses that
to set the clock by the sun: sunrise comes at about 6:45 in the morning on every day of
the year, except in the far north and far south. The clock moves a small amount each
night as the seasons change, so there is no spring-forward or fall-back day, and nobody
resets a phone, computer or other device that runs it.

**What one person would see.** Indianapolis, Indiana, in 2026:

| Indianapolis, 2026 | Today's clocks | SolTime |
| --- | --- | --- |
| Sunrise, December 21 | 8:02 a.m. | 6:45 a.m. |
| Sunset, December 21 | 5:23 p.m. | 4:06 p.m. |
| Sunrise, June 21 | 6:16 a.m. | 6:45 a.m. |
| Sunset, June 21 | 9:16 p.m. | 9:44 p.m. |
| Days with sunrise after 7:00 a.m. | 244 | 0 |
| Days with sunset before 5:00 p.m. | 0 | 87 |
| Days with sunset after 9:00 p.m. | 67 | 81 |

**How much the clock moves.** In Indianapolis the clock shifts by about a minute each
night: 56 seconds on a typical night, and 98 seconds at most, in late March. The change
is spread across the night, between 8 p.m. and 4 a.m.; outside those hours the clock runs
normally. The rule limits the change to at most 4 minutes in a night. Between its
winter and summer extremes, the Indianapolis clock moves 2 hours 50 minutes, a minute or
so at a time.

**Shared city time.** Everyone in and around a city reads the same clock. Each city of
100,000 people or more has a shared area, and where neighboring cities' areas overlap,
they all keep the most populous city's clock. At the edge of an area, its clock blends
gradually into the clocks around it. Anyone may also choose to follow a particular
city's clock wherever they are.

**The trade-off.** SolTime does not create daylight; it moves where daylight falls on
the clock. In winter, light moves from the evening to the morning: in Indianapolis the
December sunrise comes 1 hour 17 minutes earlier (6:45 instead of 8:02 a.m.), and so
does the sunset (4:06 instead of 5:23 p.m.). In summer it moves the other way: the June
sunrise and sunset each come about half an hour later (sunset at 9:44 instead of
9:16 p.m.).

**Clocks that are not phones.** Phones, computers and other connected devices can run
SolTime in software. Ordinary wall clocks and watches cannot follow it on their own;
they would need resetting by hand or replacing, and clocks that set themselves by radio
or network signals would need SolTime-aware software or hardware.

**What exists today.** A working reference implementation of the clock, in C++ and in
Java, with automated tests that check the two against each other, and an Android
demonstration app. All of it is supplied as source code, to inspect and build. SolTime
has not been adopted anywhere and has not been field-tested. The author dedicates all of
it to the public domain: anyone may use it, change it or build on it.

**What is asked.** Only consideration. This is offered to the country as a new kind of
local clock that an always-connected age makes possible. If it is of interest, the rest
of this document gives a technical team what it needs to evaluate the idea, reproduce
the results, and adapt it to the needs of different governments. In the author's words:
nothing may ever come of it, but until someone hears an idea, they might not know it is
possible.

## Common questions

### Won't winter evenings be darker?

Yes. Every city in the section 2 table has an earlier December sunset under SolTime,
including Miami and Honolulu. In Indianapolis the December sunset moves from 5:23 to
4:06 p.m.; in Seattle, from 4:20 to 3:11 p.m.

The author's reason is children going to school. School buses pick children up as early
as 7:00 a.m., so children are heading out to the bus before then. A 6:45 sunrise puts
that walk in morning light. In Indianapolis, 244 days of 2026 have sunrise after
7:00 a.m. on today's clocks; under SolTime, none do.

The 6:45 anchor is a setting, not a fixed part of the design. A later anchor moves both
sunrise and sunset later.

### What about clocks that are not phones?

A wall clock, watch or appliance clock that is not connected to anything cannot follow
SolTime by itself. It needs resetting by hand, or replacing with a SolTime-aware model.
A clock that sets itself from a radio or network time signal does not implement SolTime
just because it receives today's time; it needs SolTime-aware software or hardware.
Keeping UTC as the reference for instants still leaves integration work for local
scheduling, transportation displays, broadcasts, contracts and record presentation.

### What about the far north and far south?

The rule has trouble there because of how the sun moves at those latitudes. In
Anchorage, Alaska, SolTime's December sunset would come at 12:12 p.m., after a 6:45 a.m.
sunrise, and its June sunset at 2:08 a.m. the next morning. Closer to the poles, where
for part of the year the sun does not rise or does not set, the design borrows the
sunrise times of a latitude nearer the equator (section 5.2).

The author's position: the rule gets the bulk of the world right. For the extreme north
and south there could be adaptations, options or versions that center the clock on the
midday sun instead of a fixed sunrise time. This draft is a first proposal, offered so
that people who know and care about time-zone policy can take the idea and make it work
for the needs of different governments.

In practice, the far north and far south would extend the model by consensus or by a
nation's own rule. The base model would always remain available to everyone, to use
unilaterally for consistency, even where a nation prefers an altered model for
expediency.

## About this document

**Author:** Rick C. Hodgin  
**Document identifier:** SolTime evaluation draft 3, 2026-10-05  
**Status:** Draft for independent technical and policy evaluation  
**License:** public domain, by the author's CC0 1.0 dedication (see `LICENSE`); the
third-party data keep their own terms (section 13)  
**Companion material:** the full SolTime source in this package: the C++ reference
implementation and its tests, and the `android/` example app with its bundled data

This document combines the proposal, mathematical design, zone policy, device behavior,
implementation evidence and remaining decisions into one evaluation specification.
It can be read on its own; `soltime_spec.md` in the package is the implementation's API
reference. The Android project provides an inspectable example of the design and can
be built independently.

The Android app is provided in source-code form for evaluation. No prebuilt APK is
included, by the author's choice: evaluators can inspect the source, scan it, and build
the app themselves (section 11.2).

The core clock has working reference implementations and automated checks, rerun for
this draft (section 10.3). SolTime has not been adopted by a standards body,
independently validated against a broad astronomical reference, or field-tested as civil
time. Some interoperability rules and registry governance remain open. The author
dedicates this document and the SolTime software to the public domain; the document
does not claim endorsement by the sources it cites.

**Requested evaluation:** assess the mathematical rules, the usefulness and consequences
of the policy, reproducibility, interoperability, and the work needed for a controlled
pilot or formal standards proposal. Findings should distinguish a defect in the
implementation, a gap in the specification, and a disagreement with the policy choices.

**For a policy reader:** nothing is requested beyond consideration. The package is
offered so that the people responsible for time policy know this kind of clock is now
possible, with enough detail that a technical team can evaluate it without the author.
If it is of interest, the natural first step is the independent review described in
section 12.1.

## Contents

1. [Purpose and intended use](#1-purpose-and-intended-use)
2. [What people would see](#2-what-people-would-see)
3. [Definitions, units and time reference](#3-definitions-units-and-time-reference)
4. [Default policy and validation](#4-default-policy-and-validation)
5. [Solar model and polar basis](#5-solar-model-and-polar-basis)
6. [Daily smoothing and overnight adjustment](#6-daily-smoothing-and-overnight-adjustment)
7. [Shared city clocks and zone registry](#7-shared-city-clocks-and-zone-registry)
8. [Device display and movement](#8-device-display-and-movement)
9. [Scheduling, interchange and conformance](#9-scheduling-interchange-and-conformance)
10. [Implementation and evidence](#10-implementation-and-evidence)
11. [Package contents and reproduction](#11-package-contents-and-reproduction)
12. [Pilot, governance and unresolved decisions](#12-pilot-governance-and-unresolved-decisions)
13. [References and data provenance](#13-references-and-data-provenance)

## 1. Purpose and intended use

SolTime proposes a local civil clock whose offset changes gradually with the season.
Its raw solar rule places modeled sunrise at 06:45. A daily-change limiter can move
that sunrise earlier. Changes are spread through a nighttime window, so a stationary
clock runs at the input reference rate outside its adjustment windows.

Cities share a common clock through geographic zones. Overlapping city discs merge
under the most populous city's clock; positive-width edge ramps blend into surrounding
clocks. People and devices may instead choose to follow a named city's zone anywhere.
This provides three useful views: the raw clock at a coordinate, the clock supplied
by the local zone system, and a deliberately followed zone's clock.

The proposal changes the mapping from a reference instant to local date and clock
reading. It does not change the physical amount of daylight; it changes how daylight
falls relative to schedules expressed on that clock. UTC remains the intended reference for instants, and the SI second retains
its existing meaning. A SolTime displayed second can take more or less than one input
second during an adjustment or movement. Elapsed-time instruments should therefore
use an appropriate monotonic reference, not subtract moving local-clock readings.

### 1.1 The policy choice

The proposed 06:45 anchor favors morning light for school travel. The author's reason is
the school bus: pickups begin as early as 07:00, so children leave home before then
(see Common questions). Given a fixed modeled
daylight duration, maximizing the clock reading at sunset subject to sunrise being no
later than that anchor places sunrise at the anchor. This is the mathematical objective
of the raw rule. It does not establish an optimal school schedule, health outcome or
allocation of daylight for every household.

The trade is explicit: December sunsets are earlier in every city in the section 2
table, from Miami and Honolulu to Anchorage, while June sunsets are later. A later anchor shifts both ends of daylight later. The
default anchor, four-hour polar basis and other settings are policy proposals, not
astronomical constants. Work schedules, actual bus times, terrain, weather and sleep
patterns require separate evaluation.

### 1.2 Intended scope

The candidate design covers local date/time display, stationary and moving devices,
shared city clocks, Gregorian dates, and behavior near polar day/night. Its current
prototype implements forward conversion from a supplied reference instant.

Production scheduling, recurring appointments, timestamp interchange, registry updates,
leap-second handling and formal conformance are incomplete. Section 9 states those
gaps explicitly. The Android app is a demonstration of the display and conversion
model; it does not change the phone's system clock or create legal time.

## 2. What people would see

These examples were recomputed on October 5, 2026 with the supplied Java calendar
implementation, default parameters, and city coordinates in `android/data/places.tsv`.
Conventional local time uses the JDK's IANA tzdb **2026a** rules. SolTime uses each
city coordinate's **raw field**, without the shared-zone layers. Values are model
outputs, not independent observations of sunrise or sunset.

All event times below use the 24-hour clock, rounded to the nearest minute. `+1 day`
means a sunset after local midnight. December and June dates are December 21 and
June 21, 2026. The reproduction program and full output are included in
`android/evaluation/`.

| City | Dec sunrise, conventional → SolTime | Dec sunset, conventional → SolTime | Jun sunrise, conventional → SolTime | Jun sunset, conventional → SolTime |
| --- | --- | --- | --- | --- |
| Miami | 07:03 → 06:45 | 17:34 → 17:17 | 06:30 → 06:45 | 20:15 → 20:30 |
| Honolulu | 07:04 → 06:45 | 17:54 → 17:35 | 05:50 → 06:45 | 19:16 → 20:11 |
| Houston | 07:12 → 06:45 | 17:26 → 16:59 | 06:21 → 06:45 | 20:25 → 20:49 |
| Phoenix | 07:28 → 06:45 | 17:24 → 16:41 | 05:18 → 06:45 | 19:41 → 21:07 |
| Los Angeles | 06:54 → 06:45 | 16:47 → 16:38 | 05:42 → 06:45 | 20:07 → 21:11 |
| Atlanta | 07:38 → 06:45 | 17:33 → 16:40 | 06:27 → 06:45 | 20:51 → 21:09 |
| Denver | 07:17 → 06:45 | 16:38 → 16:06 | 05:32 → 06:45 | 20:31 → 21:44 |
| Indianapolis | 08:02 → 06:45 | 17:23 → 16:06 | 06:16 → 06:45 | 21:16 → 21:44 |
| New York City | 07:16 → 06:45 | 16:31 → 16:00 | 05:24 → 06:45 | 20:30 → 21:51 |
| Chicago | 07:14 → 06:45 | 16:22 → 15:53 | 05:15 → 06:45 | 20:29 → 21:59 |
| Minneapolis | 07:48 → 06:45 | 16:34 → 15:31 | 05:26 → 06:45 | 21:03 → 22:22 |
| Seattle | 07:54 → 06:45 | 16:20 → 15:11 | 05:11 → 06:45 | 21:10 → 22:44 |
| Anchorage | 10:14 → 06:45 | 15:41 → 12:12 | 04:20 → 06:45 | 23:42 → 02:08 (+1 day) |

The annual comparisons count all 365 dates in 2026, using unrounded event times and
strict thresholds. These eight cities have no after-midnight sunset ambiguity in
the listed comparisons.

| City | Sunrises after 07:00, conventional → SolTime | Sunsets before 17:00, conventional → SolTime | Sunsets after 21:00, conventional → SolTime |
| --- | --- | --- | --- |
| Miami | 156 → 0 | 0 → 0 | 0 → 0 |
| Houston | 169 → 0 | 0 → 15 | 0 → 0 |
| Atlanta | 243 → 0 | 0 → 59 | 0 → 39 |
| Indianapolis | 244 → 0 | 0 → 87 | 67 → 81 |
| New York City | 104 → 0 | 83 → 91 | 0 → 86 |
| Chicago | 92 → 0 | 90 → 95 | 0 → 91 |
| Minneapolis | 169 → 0 | 76 → 104 | 28 → 103 |
| Seattle | 165 → 0 | 87 → 111 | 46 → 111 |

Indianapolis illustrates the seasonal shift: its raw clock has about UTC−06:17 in
December and UTC−03:31 in June. Seattle's modeled winter daylight runs from 06:45 to
15:11; Anchorage's runs from 06:45 to 12:12. The same policy places Anchorage's June
sunset at about 02:08 the next day. These are consequences to evaluate, not hidden
exceptions to the proposal. The author's position on the far north and far south,
including possible midday-centered variants there, is under Common questions.

### 2.1 Limits of the sunrise statement

The ceiling is **modeled sunrise at or before 06:45 on the raw field where the place
retains its own basis latitude**. It is not a guarantee of actual daylight at every
bus stop. Sharing a city's clock, blending in its ramp, following a distant zone,
or borrowing a polar basis can show a later physical-location sunrise. Atmospheric
conditions and a non-level horizon add further differences from the solar model.

## 3. Definitions, units and time reference

| Term | Meaning in this draft |
| --- | --- |
| Reference input `t` | Unix/POSIX-style seconds since 1970-01-01 00:00:00; the prototype's input coordinate |
| Raw field `F(x,t)` | SolTime offset in seconds at location `x`, before city-zone layers |
| Daily offset `O(k)` | Smoothed offset assigned to the daylight plateau of calendar date `k` |
| Local reading `L` | Reference input plus the selected instantaneous offset, decoded as a Gregorian date/time |
| Basis latitude | The place's latitude, or the nearest latitude toward the equator with sufficient modeled day and night |
| Zone leader | The most populous city in a connected group of overlapping discs |
| Locked disc | The region around a member city that displays its merged zone leader's raw clock |
| Ramp | The region outside a locked disc where a zone's influence decreases |
| `J` | Published numerical estimate of a zone's daily deviation from the clock beneath its layer |
| True clock | Direct evaluation of the selected mathematical clock; this term does not imply observational accuracy |
| Displayed clock | A device's approximation to that true clock between updates |
| Clock owner | The location whose raw clock supplies a selected raw/followed/locked clock |

Coordinates are latitude in degrees north and longitude in degrees east. Accepted
numeric ranges are −90° to +90° and −180° to +180°. Trigonometric arguments are in
radians unless explicitly marked as degrees. Geography uses a sphere with radius
6371 km and great-circle distances. A future standard must identify its geodetic
input convention and tolerances; spherical distance is the prototype convention.

The calendar is Gregorian, including common years, leap years and century exceptions.
Calendar APIs accept years 1600–9999; this is an input range, not an astronomical
accuracy claim over those centuries. A separate legacy API repeats a synthetic
366-day year for simulations and registry design. It must not replace the Gregorian
API in real-date applications.

### 3.1 UTC, leap seconds and input-clock corrections

The intended real-world reference is UTC, but the prototype uses an ordinary Unix
input coordinate with 86,400 nominal seconds per date. It has no leap-second table,
no `23:59:60` representation, and no correction for a host's leap smear. The Android
app reads the device wall clock. Its location timestamps also use monotonic elapsed
time to assess fix age; that does not supply a separate UTC synchronization service.

A production standard must define the mapping from UTC and the treatment of leap
events, wall-clock steps, smears and source uncertainty. Statements about SI-rate
seconds and monotonicity assume a steadily advancing, correctly synchronized input
outside such events. UTC leap-second behavior is background to this open decision;
see [NIST's explanation](https://www.nist.gov/pml/time-and-frequency-division/leap-seconds-faqs).

### 3.2 Dates, boundaries and displayed seconds

For a selected offset `o(t)`, the encoded local reading is `L(t) = t + o(t)`. Split
it using floor division by 86,400 to obtain the local date and seconds within it.
The encoded local value is not itself a UTC timestamp.

The raw model wraps longitude at ±180°. Crossing that meridian produces a date step
and potentially a time-of-day difference of up to the configured daily slide. This
is not the political International Date Line. Shared/followed clocks use their own
clock owner's date behavior. At an exact geographic pole, arbitrary longitude still
affects the input formula; a unique physical pole-clock convention remains unresolved.

For successive stationary local midnights `t0` and `t1`:

```
t1 − t0 = 86400 − (o(t1) − o(t0))
```

Midnights can lie inside adjustment windows, so this offset difference is not in
general the full plateau change of a single night. During motion, local time can
reverse or skip dates, and this stationary-day description does not apply.

## 4. Default policy and validation

| Parameter | Default | Purpose |
| --- | --- | --- |
| Sunrise anchor `A` | 24,300 s = 06:45 | Target reading for modeled sunrise |
| Daily slide `D` | 240 s/day | Maximum difference between successive daily offsets |
| Solar zenith `z` | 90.833° | Fixed sunrise/sunset convention, including nominal refraction |
| Minimum basis day/night `M` | 14,400 s = 4 h | Polar borrowing threshold |
| Earliest adjustment start `Ns` | 72,000 s = 20:00 | Evening clipping limit |
| Latest adjustment end `Ne` | 14,400 s = 04:00 | Morning clipping limit |
| Ordinary correction `c` | 0.5 s/s | Display rate range 0.5×–1.5× |
| Ramp design speed `v` | 128.75 km/h, approximately 80 mph | Automatic ramp sizing target |
| Ramp slope floor | 2 s/km | Limits how broad far-north ramps become |
| Checkpoint tolerance | 0.05 s | Midpoint planning heuristic, not a global display-error bound |
| Maximum checkpoint interval | 300 s | Five minutes under ordinary conditions |
| Minimum checkpoint interval | 0.1 s | Prototype planner limit |
| Fast-motion checkpoint maximum | 60 s | More frequent true-clock updates |
| Default city threshold | 100,000 people | Eligibility in the supplied registry |
| Default zone radius | 1.5 × equivalent-area city radius | Locked-disc size |
| Earth radius | 6371 km | Spherical geography |

All numeric parameters must be finite. The implemented validation requires:

```
0 ≤ Ne < A < Ns < 86400
D > 0; 0 < z < 180 degrees; 0 < M < 43200
M − (A − Ne) > 2D
86400 − Ns + Ne > 3D
0 < c < 1
design speed > 0; ramp slope floor > 0
checkpoint tolerance > 0; maximum checkpoint interval ≥ 1 second
```

These checks reject invalid parameter combinations; they do not certify every custom
profile. Changing the solar/clock parameters can invalidate published `J` and ramp
values. A release must identify the parameter profile used to build its registry.

## 5. Solar model and polar basis

### 5.1 Solar calculations

The current model uses the approximate
[NOAA general solar equations](https://gml.noaa.gov/grad/solcalc/solareqns.PDF).
For zero-based day-of-year `i` and year length `N` (365 or 366), evaluated at midday:

```
γ = 2πi/N
δ = 0.006918 − 0.399912 cos γ + 0.070257 sin γ
    − 0.006758 cos 2γ + 0.000907 sin 2γ
    − 0.002697 cos 3γ + 0.001480 sin 3γ
E = 229.18 × (0.000075 + 0.001868 cos γ − 0.032077 sin γ
             − 0.014615 cos 2γ − 0.040849 sin 2γ)
q = (cos z − sin φ sin δ) / (cos φ cos δ)
H = acos(q), expressed in degrees
sunriseMinutes = 720 − 4(λ + H) − E
sunsetMinutes  = 720 − 4(λ − H) − E
solarNoonMinutes = 720 − 4λ − E
```

Here `δ` is declination in radians, `E` is equation of time in minutes, `φ` is
latitude, and `λ` is east longitude in degrees. Event values are minutes relative
to the selected UTC-labeled date and may lie outside 0–1440 near the date boundary.
Multiply by 60 and add that date's reference midnight to obtain event instants.

For `q > 1` the modeled sun does not rise; for `q < −1` it does not set. Report those
statuses instead of displaying an invented physical sunrise. The prototype treats
degenerate polar geometry separately and provides finite basis-clock calculations.
Grazing events and exact-pole conventions need explicit numerical acceptance rules.

The fixed zenith is a convention, not a prediction of actual atmospheric refraction.
Altitude, mountains, buildings, horizon dip and variable weather are omitted.

**Measured accuracy.** This is a first-draft solar model. It was measured against an
independent reference: the Meeus low-precision solar position (J. Meeus, *Astronomical
Algorithms*, 2nd ed., 1998), re-evaluated at each event's own time with the same
90.833° zenith. That reference is itself an approximation, but a much closer one; it
gives a declination of 0.0023° at the published March 2026 equinox, 14:46 UTC on
March 20. For every date of 2026 the model's events differ from it by:

| Place | Sunrise: typical / worst | Sunset: typical / worst |
| --- | --- | --- |
| Miami | 31 s / 92 s | 62 s / 100 s |
| Indianapolis | 57 s / 128 s | 101 s / 153 s |
| Seattle | 85 s / 167 s | 143 s / 205 s |
| Anchorage | 181 s / 262 s | 273 s / 348 s |
| 0°N 0°E | 18 s / 42 s | 20 s / 53 s |

"Typical" is the median absolute difference. Two causes contribute. First, the
approximate series runs late: in 2026 its declination differs by up to 0.44° and
crosses zero 25.2 hours after the March equinox, and the difference grows to 0.52° in
2050 and 0.75° in 2096; its equation of time differs by up to 47 seconds. Second, one
value per UTC date, taken at 12:00 UTC, serves all of that date's events, while
sunsets in the section 2 cities fall roughly 9 to 20 hours after that instant. Near
the solstices the two agree within about half a minute, so the section 2 December and
June tables hold to the minute. The section 2 annual counts come from the same model
and carry the same differences.

The same Meeus formulas, evaluated once per date at 12:00 UTC, would keep the model's
one-value-per-date structure and reduce the differences in Indianapolis to at most 4 s
at sunrise and 49 s at sunset (41 s and 135 s in Anchorage). That is the measured
upgrade path. Adopting it would change the daily targets, the registry and every
published example. It would also make each date's target depend on the actual year,
so the nine-year window of section 6.1 would need a new exactness argument.
`android/evaluation/SolarAccuracy.java` reproduces these figures (section 11.3). A
full comparison against an ephemeris across dates, latitudes and polar transitions
has not been done.

### 5.2 Borrowed basis at high latitude

If modeled day and night at the physical latitude each last at least `M`, use that
latitude. Otherwise use the nearest latitude toward the equator, in the same
hemisphere, where the short day or short night lasts `M`.

For the required hour angle, choose `H0 = M/480` degrees on the short-day side, or
`H0 = 180 − M/480` degrees on the short-night side. Solve:

```
sin φb sin δ + cos φb cos δ cos H0 = cos z
a = cos δ cos H0; b = sin δ; c0 = cos z
φb = atan2(b,a) ± acos(c0 / sqrt(a²+b²))
```

Convert angular units consistently and choose the root between the equator and the
physical latitude. The reference implementation handles root selection and limiting
cases. Report the physical place's solar-event status separately from its borrowed
basis events.

With defaults, the borrowing threshold is approximately 64.4°N around the December
solstice and 62.5°N around the June solstice. Anchorage retains its own basis through
the year. More northerly places borrow seasonally; the corresponding behavior occurs
in the southern hemisphere with reversed seasons.

Winter-solstice polar examples place modeled solar noon near 08:45; summer examples
place it near 16:45, with variation from smoothing. Just outside polar night, a place
with less than four hours of daylight can see its actual-location modeled sunrise
after the anchor. The design analysis gives a slip of up to `M/2`; reported samples
reached about 113 minutes. The raw-field sunrise ceiling does not apply there.

The author's position: practical use in the far north and far south would need a
consensus or a nation's own rule for extending the model there. The base model would
remain available to everyone, to use unilaterally for consistency, even where a nation
prefers an altered model for expediency (see Common questions).

## 6. Daily smoothing and overnight adjustment

### 6.1 The raw target and lower envelope

For each Gregorian date `k`, compute sunrise using its basis latitude and define:

```
T(k, φ, λ) = A − basisSunriseSeconds(k, φ, λ)
O(k) = min over dates j of [T(j) + D × |k − j|]
```

The event seconds in `T` are relative to that date, not an absolute timestamp. This
lower envelope is the greatest sequence no larger than `T` whose adjacent values
differ by at most `D`. Thus `O(k) ≤ T(k)` and `|O(k+1) − O(k)| ≤ D`. Equality with
the raw target holds where the limiter is inactive.

Longitude adds exactly 240 seconds per degree east to each daily plateau. The table
can therefore be built once at longitude zero for a latitude and shifted afterward.
Instantaneous nighttime differences need not be exactly 240 seconds per degree,
because the adjustment windows also move with longitude.

The Gregorian implementation evaluates nine complete neighboring years on each side
of the requested year and performs forward and backward minimum sweeps. That span gives
the exact envelope, not an approximation, for every positive `D`. In this model `T`
depends only on the year's length (365 or 366 days) and the day's number within the
year. A date outside the span therefore has a nearer date inside it with the same year
length and day number, hence the same `T`, and the nearer copy always gives the smaller
value of `T(j) + D × |k − j|`. Nine years guarantees both kinds of year in each direction
for every date in the computed table (the requested year plus eight days at each end),
including where a skipped century leap day leaves seven consecutive common years, as in
1897–1903. Separately, only dates within `(max T − min T) / D` days of `k` can affect
`O(k)`; at the defaults that is about 8 days at the equator, 43 at Indianapolis and 124
at 70°N. The legacy simulation uses circular distance over 366 days instead. These are
different APIs, not interchangeable calendars.

### 6.2 The night window

Let `Bk = 86400k` denote the selected date label's reference midnight. Between date
`k` and `k+1`, define absolute reference endpoints:

```
ws = max(basisSunsetInstant(k), Bk + Ns − O(k))
we = min(basisSunriseInstant(k+1), B(k+1) + Ne − O(k+1))
W = we − ws
```

The daily offset is constant between one window's end and the next window's start.
Inside `[ws,we)`, put `x = (t − ws)/W` and interpolate:

```
o(t) = O(k) + (O(k+1) − O(k)) × (x − sin(2πx)/(2π))
dL/dt = 1 + (O(k+1) − O(k))/W × (1 − cos(2πx))
```

The rate meets the adjacent constant-rate periods smoothly. Its peak absolute
deviation from 1 is `2|O(k+1) − O(k)|/W`. One displayed second's input duration is
the reciprocal of this rate, so rate deviation and second-length deviation must not
be confused. A positive rate requires the window to be sufficiently long for a
negative offset change.

Reported default samples give these approximate values:

| Absolute latitude band | Shortest sampled window | Peak clock-rate deviation |
| --- | --- | --- |
| 0–45° | 5.6 h | 0.8% |
| 45–55° | 3.9 h | 1.4% |
| 55–60° | 2.4 h | 2.4% |
| 60–89° | 1.25 h | 10.6% |

The nominal minimum `M − (A − Ne)` is 75 minutes. The existing test allows one daily
slide below it, or 71 minutes with defaults; 75 minutes is not an established exact
universal lower bound. These are sampled results from the synthetic design year.

### 6.3 Selecting the correct date

The reference algorithm estimates a local date from the input and a daily offset,
examines neighboring windows, selects the containing window if any, and otherwise
uses the intervening plateau. `SolarCalendar.offset` implements that selection.

For physical-location solar events, the app selects the date using the physical
location's raw field, then renders event instants on the user's selected clock. A
distant followed zone must not choose the physical location's solar date. For the
adjustment window, use the clock owner's coordinates and date instead.

## 7. Shared city clocks and zone registry

### 7.1 City discs and merging

The default registry selects qualifying city records with at least 100,000 people.
A city may declare a radius. Otherwise the preparation process uses the larger of
the U.S. Census land area and the GHSL urban-core area for U.S. cities, the GHSL area
elsewhere, or an estimate from population and a country's median urban-core density
when measured data is unavailable. The equivalent-area radius is `sqrt(area/π)`.

Taking the larger area favors wide city limits. Of the 328 U.S. city rows in this
snapshot, 252 use the Census area. Of the 161 U.S. cities matched in both sources, 65
have a Census land area at least twice their GHSL urban-core area, 36 at least three
times and 11 at least six times. Two causes contribute. Some city limits take in large
undeveloped areas: Anchorage's municipal land area is 37 times its GHSL core, and
Jacksonville (6.7×), Oklahoma City (5.0×) and Indianapolis (3.7×) are further examples.
Elsewhere GHSL assigns a shared metropolitan core to the larger city: the Dallas–Fort
Worth core matched Dallas, leaving Fort Worth a 60 km² core against 907 km² of city land
(15×). Anchorage's 35-mile reach, 8.3-minute `J` and 106-mile ramp in the table below
follow from its municipal boundary. The author keeps this rule for this draft. Changing
it would change merging, leaders, `J` and ramps, and would need a new registry release.

Each default locked disc has radius `1.5 × cityRadius`. This is not a claim that the
disc contains every part of an irregular city boundary. Municipal declarations and
geographic coverage need review before adoption.

Two discs merge when their great-circle center distance is **strictly less** than
the sum of their radii. Merging is transitive, with no size cap. A group's area is
the union of its member discs, not a larger enclosing circle. The most populous
member is its leader; equal populations are resolved by original registry order.
Groups apply in descending leader population, again breaking ties by registry order.
Exactly touching discs do not merge under the strict comparison; zero-gap behavior
needs boundary testing and an explicit standards decision.

The snapshot contains 5,854 city rows and 4,168 merged groups. These are counts in a
particular dataset, not a guarantee of a complete and error-free world census. The
prototype permits groups across national borders. Whether legal jurisdictions accept
those groups is a policy decision separate from the geometry.

In this snapshot, 17 groups cross a national border. Those with U.S. cities: Tijuana
leads San Diego, Chula Vista and El Cajon; Ciudad Juárez leads El Paso; Reynosa leads
McAllen; Heroica Matamoros leads Brownsville; Nuevo Laredo leads Laredo; and Detroit
leads Windsor, Canada. Elsewhere, Guangzhou leads Hong Kong and Macau, and Singapore
leads Johor Bahru in Malaysia and Batam in Indonesia, among others. These groups follow
from the prototype's simple merging rule and are not essential to the concept.

Representative geometry checked against the supplied registry, rounded for display:

| Leader / area | Member cities | Approximate reach from leader | Ramp width | Reported J |
| --- | --- | --- | --- | --- |
| New York City | 14 | 42 mi | 13 mi | 3.7 min |
| Los Angeles | 35 | 65 mi | 17 mi | 4.8 min |
| San Jose / Bay Area | 13 | 63 mi | 18 mi | 5.1 min |
| Dallas–Fort Worth | 14 | 46 mi | 12 mi | 3.5 min |
| Chicago | 3 | 43 mi | 13 mi | 3.7 min |
| Miami–Fort Lauderdale | 9 | 36 mi | 7 mi | 2.0 min |
| Phoenix | 10 | 30 mi | 8 mi | 2.3 min |
| Houston | 4 | 30 mi | 7 mi | 2.2 min |
| Denver | 7 | 19 mi | 6 mi | 1.6 min |
| Atlanta | 2 | 17 mi | 3 mi | 0.9 min |
| Indianapolis | 1 | 16 mi | 5 mi | 1.4 min |
| Anchorage | 1 | 35 mi | 106 mi | 8.3 min |

Reach describes the merged outline; it is not an additional circular lock radius.
Exact per-member geometry is in `android/data/zones.tsv`. Population order, data
corrections or declared radii can change both merging and leaders in a later release.

### 7.2 Layered offsets and edge ramps

Begin at the raw-field offset. Apply groups in their fixed order. For a group with
leader offset `Z`, let `B` be the offset already produced by earlier layers. The
prototype compares time-of-day differences using:

```
wrapDay(d) = IEEE remainder(d, 86400)
lead = wrapDay(Z − B)
```

IEEE remainder selects the nearest integral multiple of 86,400, with even-integer
tie breaking. This preserves the prototype's behavior at half-day ties. It also means
a lock agrees with the leader modulo a day; agreement on the full date across the
model date boundary is a separate issue. Following a zone directly uses the leader's
full raw clock. Normal metro examples away from that boundary share the same date.

Inside any member disc, add the full `lead`. Outside the discs, for each member whose
ramp contains the query point, compute its great-circle distance `d`, radius `R`,
group ramp width `w`, and the rim point on the same bearing from that member's center.
Evaluate earlier layers at the rim, giving `B_rim`. Then:

```
memberCap = max(J, abs(wrapDay(Z − B_rim))) × (1 − (d − R)/w)
cap = maximum memberCap among ramps containing the point, or 0 if none
newOffset = B + clamp(lead, −cap, +cap)
```

The rim term allows the cap to meet the current lock value even when the leader and
rim have different night windows. A cap based only on daily `J` can be too small at
some instants. Later layers blend into the clock already beneath them, not a freshly
selected winner or just the bare raw field.

Positive ramp widths preserve boundary continuity where the underlying clocks and
wrapped differences remain continuous. Hard edges, the date boundary and half-day
wrapping transitions are exceptions requiring explicit treatment. Global continuity
over every possible custom registry is not established.

### 7.3 What J measures

The publisher estimates the maximum absolute wrapped difference between a group's
daily plateau clock and the layer beneath it inside its discs. It evaluates all 366
days of the synthetic design year, samples disc rims every 5° of bearing, examines
interior rings where earlier groups intrude, and refines near the largest sample.

`J` is a useful modeled deviation estimate. It is not independently established as
an exact maximum over every point, all real-calendar years and every instant. It is
also not necessarily the difference from the original raw field when earlier layers
already affect that field. Night-window differences can exceed the daily value.
Astronomical error, zone deviation and device tracking error are different quantities.

### 7.4 Ramp sizing and optional radius classes

Automatic ramp sizing uses a numerical estimate of the gradient beneath the ramp
and the leader's peak night-rate deviation `n`:

```
v = designSpeedKmh / 3600                 # km per input second
budget = (maxCorrection − n) / v         # seconds per km
slope = max(budget − underlyingGradient, rampSlopeFloor)
w = J / slope
```

The implementation starts with the shortest candidate span and grows the sampled
ring until the implied width fits. It allows up to 16 iterations, with a floor-based
fallback. The result is capped at the gap to earlier groups' locked discs, so later
ramps do not overwrite those locks. Supplied positive widths are gap-capped too;
zero requests a hard edge; a negative width requests automatic sizing.

These are design calculations and tested scenarios, not an all-path 80 mph guarantee.
The slope floor, a narrow gap, and the changing instantaneous rim term can require
faster tracking. The device then uses the fast-motion behavior in section 8.

The API also supports an optional radius from a requested accuracy class: a numerical
search for the largest disc whose modeled daily raw-field difference is within half
the class. Typical classes are 60, 300, 600 and 1200 seconds. That design aid uses
all bearings and days through numerical sampling/refinement. The supplied city
registry instead uses declared/measured radii and publishes `J` afterward.

### 7.5 Published format and release behavior

`zones.tsv` is UTF-8, tab-separated, with `#` comment lines. Published rows contain:

| Column | Meaning |
| --- | --- |
| `name`, `country`, `admin1` | City label, country code and administrative region |
| `lat`, `lon` | City coordinates in degrees |
| `population` | Population used in leader selection |
| `city_radius_km` | Declared or derived equivalent-area radius |
| `radius_source` | Provenance label such as Census, GHSL or estimate |
| `zone_radius_km` | Locked-disc radius |
| `ramp_km` | Published group ramp width |
| `jump_s` | Published group deviation estimate J |
| `zone` | Zero-based row of the leader, counting city rows only |
| `zone_name` | Human-readable leader label |

The full-project publisher accepts 11-column raw and 13-column published input,
recomputes derived values, validates its temporary output and replaces the destination
atomically. Its malformed-input/write-failure protections are reported tested. The
Android reader consumes the supplied snapshot; it is not the publisher or a secure
registry update service.

Row indices are not stable public identifiers. A production registry still needs
stable IDs, a schema/profile version, release numbers, activation times, authentication,
retained historical releases, migration and rollback rules. Devices should not silently
mix data from different releases or interpret a row number from another snapshot.

## 8. Device display and movement

### 8.1 Ordinary checkpoint tracking

The true selected clock can be evaluated for any accepted location and reference
instant. The ordinary device display follows it by planning straight segments:

1. Obtain the current reference time, location and available speed/heading.
2. Predict position at the next checkpoint and evaluate the true clock there.
3. Choose a constant displayed rate intended to land on that value.
4. Check the predicted midpoint. Halve the interval when error exceeds 50 ms; grow
   the next interval when error is below one quarter of that tolerance, up to 300 s.
5. Replan on a material position-fix departure. The minimum interval is 0.1 s.

The midpoint check is a planning heuristic. It is not a proven 50 ms maximum at all
points or under uncertain motion. Ordinary display correction is limited to rates
between 0.5× and 1.5× the input clock. A large error that cannot be recovered within
60 seconds causes an immediate true-time reset and a **New Time Zone** indication.
Examples include startup after travel, a distant fix, a hard edge or a follow-mode
change. This label describes a device event, not necessarily a legal zone crossing.

Scheduled checkpoints are replayed when the display is polled late, up to a minute
of lateness; after a longer silence the planner makes a current checkpoint. Successful
tracking assumes sufficiently accurate time and location. Devices with different
fixes, update schedules or predictions need not display identical intermediate values.

### 8.2 Fast motion

When the selected true clock's local rate exceeds the ordinary correction range,
the planner uses checkpoints at most one minute apart, shows the true value there,
and uses an unbounded intervening rate. Midpoint refinement continues. A turn can
replan immediately if the clock is already in fast mode, its next checkpoint is near
enough, and the error is small. Stationary-to-fast transitions and larger relocations
run the full planner; a large relocation triggers the reset indication.

Flying east accelerates local solar time; flying west slows it. At high latitude a
westbound aircraft can outrun the local solar clock, so the correct moving display
can run backward. This is permitted by the current design. Zones still apply overhead;
there is no separate flight mode that removes them. Such displays need human-factors
evaluation before operational use.

### 8.3 Exact mode, following and missing location

**Exact mode** shows direct evaluation on each display update. It removes the
checkpoint approximation; it does not improve the input clock, location or solar
model. The Android UI updates roughly every 200 ms, subject to platform scheduling.

**Follow a zone** selects its leader's raw clock independent of the device location.
Local physical sunrise/sunset remain distinct and are rendered on that chosen clock.
Changing the followed zone or returning to location-based time may produce a step.
Following can work before the first location fix.

**Missing location:** the engine can use a last position or continue prediction from
the last velocity. The Android adapter is more conservative: it accepts cached fixes
up to 60 seconds old, rejects out-of-order fixes and certain coarse replacements of
fresh precise fixes, and stops extrapolating motion after five seconds without a new
fix. It displays location age/accuracy and allows manual location selection. Offline
calculation works from the included data and a time/location input; offline operation
does not guarantee that those inputs remain correct.

## 9. Scheduling, interchange and conformance

### 9.1 Intended scheduling semantics

An appointment at a fixed local reading needs both a date/time and an identified
clock: a fixed coordinate or a followed zone, a parameter profile, registry release
and reference-time policy. Solving `L(t) = requestedLocalReading` produces its reference
instant. The current applications do not provide a general scheduling/inversion API.

For a fixed, strictly increasing raw clock, numerical inversion is a plausible
implementation method. It still requires explicit error tolerances and boundary
rules. Moving clocks, registry changes, hard edges and date changes need separate
semantics; they may produce repeated or skipped readings.

A recurring 08:00 local meeting must be resolved separately for each occurrence.
It is not equivalent to adding 86,400 reference seconds each day. A future scheduling
standard must decide whether registry updates change future events or whether those
events remain tied to their original release. Timers and elapsed durations should
use a monotonic time source independent of SolTime display adjustments.

### 9.2 Interchange

Preserve the underlying reference instant separately from the SolTime display label.
A future interchange record needs the clock selector, coordinates or stable zone ID,
parameter profile, registry identity and time-reference convention, with precision
and rounding rules. These are required design topics, not an implemented wire format.

RFC 3339 numeric offsets use whole minutes. SolTime offsets can include seconds and
fractions, so rounding a SolTime offset into that format loses information. An ordinary
UTC timestamp can identify an instant, but does not by itself encode a recurring local
schedule or reproduce a particular SolTime display. See
[RFC 3339 §4.2](https://www.rfc-editor.org/rfc/rfc3339.html#section-4.2).

### 9.3 Proposed conformance structure

The following separates what independent implementations would need to demonstrate.
It is a review framework, not an existing certification program:

| Profile | Proposed subject of conformance |
| --- | --- |
| Core calendar clock | Input/calendar rules, solar/basis formulas, smoothing, night windows, defaults and invalid-input behavior |
| Shared-zone clock | Core behavior plus the declared registry bytes/profile, merging, ties, wrapping, layer order and ramps |
| Device display | A declared clock profile plus checkpoint/reset/fast-motion behavior, input uncertainty and location-loss policy |
| Scheduling/interchange | Inverse conversion, recurrence, metadata, version changes and boundary resolution; presently unfinished |

Separate requirements from optional UI choices. An application can display the
exact mathematical clock without implementing the prototype's predictive display
planner. Conversely, passing display tests cannot establish astronomical accuracy.

Before claiming formal conformance, publish versioned input/output vectors, numerical
error budgets by date range, rounding and exceptional-value rules, and a complete
acceptance suite. Binary64 arithmetic at large absolute timestamps has different
resolution from small offsets. The current differential-test differences are measured
results, not adopted tolerance requirements or a promise of bitwise platform identity.

## 10. Implementation and evidence

### 10.1 What exists

The full project contains a C++ implementation exposed through a C ABI, a corresponding
pure Java implementation, an offline zone publisher, automated regression tests,
cross-language comparisons, a desktop display and the Android app. The desktop includes
world and clock views, place search and simulated standing, driving, flight and
relocation scenarios. This package includes the full project except the desktop
display, which depends on a separate TUI project.

The Android package’s `versionName` of `1.0` is application metadata, not an adopted
SolTime standard version. The portable example contains the Java clock/calendar, place lookup, location adapter,
OpenGL globe, Android UI, data snapshot, Gradle wrapper and worked-example generator.
The app includes raw/local/followed clocks, location input or manual coordinates,
modeled sun events, zone rings and exact mode. Alarms, widgets, background scheduling
and calendar integration remain product work.

### 10.2 What the existing tests establish

The earlier October 5 verification record (`BUG_FIX_PLAN_2026-10-05.md`) reports six
CTest suites passing: core clock checks, Gregorian/calendar travel, review regressions,
isolated C interface, Java location/review tests, and registry publisher
validation/recalculation. It also reports C++/Java differential agreement, targeted
address/undefined-behavior sanitizer checks, desktop offscreen rendering, and an
identical rebuilt registry. LeakSanitizer was unavailable under that sandbox's tracing.
The six suites, the differential test and the registry rebuild were rerun for drafts 2
and 3 (section 10.3); the sanitizer and desktop checks were not.

Earlier numerical scenarios reported:

| Scenario | Reported maximum display difference from modeled true clock |
| --- | --- |
| Stationary, 40°N, 30 days | 0.008 s |
| Stationary, 70°N summer, 30 days | 0.038 s |
| 80 mph through an Indianapolis test zone, three hours | 0.069 s |
| 900 km/h aircraft with turns from 40°N | 0.12 s |
| Same flight scenario from 60°N | 0.52 s |
| Same flight scenario from 70°N | 0.32 s |
| Denver device following Indianapolis for two days | 0.007 s |

The test zones and trajectories are defined in the full test sources; a named city
in a scenario does not necessarily mean the current published metro geometry was
used. These figures do not include real GPS errors or field observations. Earlier
periodic cross-language runs reported clock differences around 2 × 10⁻⁹ s and exact
agreement on leaders; calendar checks were subsequently added. None of that establishes
nanosecond astronomical accuracy or universal all-input correctness.

### 10.3 Checks performed for this draft

For draft 3 the package was assembled and checked on October 5, 2026, from a fresh copy
outside the author's repository, with Clang 18.1.3, CMake 3.28.3, OpenJDK 21.0.11,
Python 3.12.3 and Perl 5.38.2. `VERIFICATION.txt` has the full record.

- The CMake Release build completed with no compiler warnings, and all six CTest
  suites passed. With an unrelated `tui` folder placed beside the package, CMake still
  left out the desktop display.
- The C++/Java differential test agreed in all 20 comparison groups, also when run
  without `ionice`. The largest clock difference was 1.9 × 10⁻⁹ s; the largest radius
  difference was 3.0 × 10⁻⁸ km.
- The worked-example and solar-accuracy programs, compiled for Java 17, reproduced
  `reference-output.txt` and `solar-accuracy-output.txt` byte for byte, under both a
  UTF-8 locale and the plain `C` locale. The examples used tzdb 2026a.
- `places.tsv`, `countries.tsv` and `zones.tsv` were rebuilt from the October 3 source
  downloads with the package's own scripts and publisher, and matched the supplied
  files byte for byte. The `data/` and `android/data/` copies are identical. The
  registry has 5,854 city rows and 4,168 leaders.
- Android, with Android Gradle Plugin 9.0.1 and Android SDK 36. Starting from an empty
  Gradle home, the wrapper downloaded Gradle 9.1.0 and verified it against Gradle's
  published SHA-256. With that home and network access (no `--offline`), the package's
  `:app:assembleDebug` and `:app:lintDebug` downloaded their dependencies and
  succeeded. Lint reported **0 errors and 5 warnings**; the fifth says a newer Gradle
  (9.8.0) is available, which lint can check only with network access. A copy of
  `android/` alone then built offline from the same, now filled, Gradle home, with
  0 errors and the other 4 warnings. The data tables and Earth texture inside each
  built APK matched the source bytes. Those APKs were built only as a check and are
  not included.

The section 2 examples and the offset values below were generated from the current
Java implementation. These checks did not establish physical-phone behavior or
independent astronomical accuracy, and did not repeat the earlier sanitizer and
desktop checks.

### 10.4 Small reproducible reference examples

Defaults, Gregorian API, raw field, no registry or device smoothing. At input
`2026-06-21T12:00:00Z`, represented here as Unix seconds `1782043200`:

| Latitude | East longitude | Offset returned, seconds | Basis latitude for that date |
| --- | --- | --- | --- |
| 0.00000 | 0.00000 | 2838.228993166 | 0.000000000 |
| 39.76838 | −86.15804 | −12672.927951636 | 39.768380000 |
| 69.64920 | 18.95530 | 21569.577791987 | 62.454920186 |
| −33.86880 | 151.20930 | 35120.456047827 | −33.868800000 |

These are reference-program outputs for comparison, not independently measured
sunrise times or formal certification vectors. Reproduction commands are in section 11.

### 10.5 Independent evidence still needed

Validate the solar model against an independent ephemeris across dates and latitudes,
especially near polar transitions. A first comparison, against the Meeus formulas for
five places through 2026, is in section 5.1; it measured differences of up to 128 s at
sunrise and 153 s at sunset in Indianapolis, and up to 348 s in Anchorage. Audit the
Gregorian envelope, local-midnight
selection, wrapped zone differences, touching discs, ramps near the date boundary,
extreme/custom parameters, large merged regions, and real-calendar deviation bounds.
Measure reference-time and location uncertainty separately from the mathematical
display's tracking error.

Device evaluation should cover permissions, stale/coarse location, indoor use,
background/resume behavior, clock corrections, battery cost, accessibility and small
screens. Field and human-factors studies are needed for sleep, school travel,
commuting, winter evenings, commerce and public understanding.

## 11. Package contents and reproduction

### 11.1 Package contents

The package is `SolTime_evaluation_2026_10_05.zip`. Its root is the SolTime project
root: this document, the license, the C++ reference implementation with its tests and
data tools, and the `android/` example app, all as source code, with a `SHA256SUMS`
manifest. It excludes local SDK configuration, caches, build directories and signing
credentials.

The Android app is provided in source-code form for evaluation. No prebuilt APK is
included, by the author's choice: an evaluator can inspect the source, scan it, and
build the app with their own tools (section 11.2). The desktop display is also left
out, because it needs a separate TUI project.

```
SolTime_overview.md         this document
LICENSE                     public domain dedication (CC0 1.0) and third-party terms
SHA256SUMS                  SHA-256 of every other file
VERIFICATION.txt            the checks run for this draft
soltime_spec.md             API and implementation reference
BUG_FIX_PLAN_2026-10-05.md  the October 5 corrections and their checks
CMakeLists.txt
soltime.h, soltime.cpp, soltime_calendar.inc    C++ reference implementation, C ABI
test_soltime.cpp, test_calendar.cpp             core and calendar test suites
zonetool.cpp                                    zone registry publisher
tests/                      review, C interface, Java and publisher tests
difftest/                   C++/Java differential test
data/
    places.tsv, countries.tsv, zones.tsv, ATTRIBUTION.md
    make_places.pl, make_zones.pl               rebuild the tables from their sources
android/
    README_android.md
    build.gradle, settings.gradle
    gradlew, gradlew.bat, gradle/wrapper/...
    data/                   the same four data files as data/ above
    evaluation/
        OverviewExamples.java
        reference-output.txt
        SolarAccuracy.java
        solar-accuracy-output.txt
        verification.txt
    app/
        build.gradle
        src/main/AndroidManifest.xml
        src/main/assets/earth.jpg
        src/main/java/com/lebeli/soltime/...
```

The Android build prefers the shared `data/` folder at the package root and otherwise
uses `android/data/`, so the `android/` folder can also be copied out and built alone.
Both copies were checked identical for this draft. Refresh both, and the manifest, for
any later release. To check a download, run `sha256sum -c SHA256SUMS` in the package
root. SHA-256 identifies the supplied bytes; the manifest is not a digital signature or
a standards-registry versioning system.

### 11.2 Build and inspect the application

The snapshot requires JDK 17 or newer and uses Gradle 9.1.0, Android Gradle Plugin 9.0.1 and Android SDK 36.
It targets SDK 36, with minimum Android API 26 and OpenGL ES 2.0. Open `android/`
directly in Android Studio, sync Gradle and run the `app` configuration. Alternatively:

```sh
cd android
nice -n 19 env ANDROID_HOME=/path/to/Android/Sdk ./gradlew --no-daemon :app:assembleDebug :app:lintDebug
```

The first build needs network access to obtain the build tools/dependencies. An
untracked `local.properties` with `sdk.dir` can supply the SDK location instead.
The output is `app/build/outputs/apk/debug/app-debug.apk`. To run it on a phone, use
Android Studio's run button with the phone connected, or `adb install -r` with that
file; a debug build installs only on a phone set to allow it. The application uses
platform Activity, LocationManager and OpenGL rather than AndroidX or Play Services.

For a quick review, select **Find → Indianapolis → Use as my location** without
requiring GPS. Inspect the clock, offset, local zone and sun events. Repeat with
Seattle and Anchorage. Follow a distant city's zone to see the separation between
the physical location and the chosen clock. Toggle **Exact** to compare direct
evaluation with the checkpoint display; use **GPS** or **Unfollow** to return.

The app's top panel shows date, zone, displayed rate, next checkpoint, sunrise/sunset
and the adjustment window. The globe shows modeled daylight, zone discs/ramps and
city markers. **Find** accepts place names, country names or coordinates. **Me**
recenters the globe; **Follow** selects the reticle's zone. Settings for exact mode,
manual location and a followed zone persist across restarts. A stale-location label
helps distinguish calculation from uncertainty in the input position.

The app displays current-time behavior and has no seasonal date-simulation control.
Use the next subsection to reproduce seasonal examples. Detailed application controls
and implementation responsibilities are in
[android/README_android.md](android/README_android.md).

### 11.3 Reproduce the tables without Android

The reference clock/calendar, the example generator and the solar-accuracy measurement
require only a JDK, version 17 or newer. From `android/`:

```sh
nice -n 19 mkdir -p build/evaluation-classes
nice -n 19 javac -d build/evaluation-classes \
  app/src/main/java/com/lebeli/soltime/SolTime.java \
  app/src/main/java/com/lebeli/soltime/SolarCalendar.java \
  evaluation/OverviewExamples.java evaluation/SolarAccuracy.java
nice -n 19 java -cp build/evaluation-classes OverviewExamples data/places.tsv
nice -n 19 java -cp build/evaluation-classes com.lebeli.soltime.SolarAccuracy data/places.tsv
```

Compare the first program's output with `evaluation/reference-output.txt`, and the
second's with `evaluation/solar-accuracy-output.txt`. Both programs write UTF-8 with
`\n` line ends whatever the locale or platform, so the files can be compared byte for
byte. The first line of the examples names the JDK's tzdb version and can differ on
another JDK; all lines after it should match. A changed conventional-time database can
alter the conventional comparison; the SolTime rules themselves do not depend on that
database. The city coordinates
come from the supplied place records. Sunrise/sunset come from `sunTimesUnix`, and
their SolTime readings come from adding `offsetUnixSecs` at each event instant.
Annual counts use unrounded local readings; after-midnight sunsets require attention
to the calendar date rather than treating a morning reading as an ordinary evening.

### 11.4 Full implementation checks

The package includes the full SolTime project, so the C++ checks, the registry
publisher and the cross-language results can be reproduced. From the package root:

```sh
nice -n 19 cmake -S . -B build-review -DCMAKE_BUILD_TYPE=Release
nice -n 19 cmake --build build-review -j2
nice -n 19 ctest --test-dir build-review --output-on-failure
nice -n 19 bash difftest/run_difftest.sh /tmp/soltime-review-difftest
```

CMake selects Clang. A JDK and Python 3 are needed for the Java location tests and the
publisher test; CMake skips those tests when it cannot find them. The differential test
needs `clang++`, a JDK and Perl. The desktop display is not included, and CMake skips
it when the TUI project is absent. `VERIFICATION.txt` records the results of these
commands for this draft, and `SHA256SUMS` identifies every file.

The data tables can be rebuilt from their public sources with `data/make_places.pl`,
`data/make_zones.pl` and `soltime_zonetool`. Each script's header gives its download
URLs and command line.

## 12. Pilot, governance and unresolved decisions

The author asks only that this proposal be considered. It is offered in the public
domain so that the people responsible for time policy know this kind of clock is now
possible; any government may adopt it, change it or set it aside. If it is of
interest, the sequence below is the suggested path.

### 12.1 Proposed adoption sequence

First obtain independent mathematical, software and human-factors review. Resolve the
mandatory clock/input/interchange rules and publish an identified candidate profile
with reproducible vectors. Then consider a voluntary pilot displaying SolTime beside
conventional time in a defined metro area, with a fixed registry snapshot, support
arrangements, a defined end date and published evaluation criteria.

A pilot should measure comprehension, missed appointments, clock disagreement,
location availability, device battery cost and compatibility with school/work schedules.
Assess winter-evening and summer-evening effects over a sufficient seasonal period.
Health, safety, energy and economic outcomes need an appropriate study design; the
mathematical model alone cannot establish them. Include accessibility and users with
ordinary clocks or no continuously located device.

The effect on clocks that are not phones (unconnected wall clocks and watches, and
radio- or network-set clocks) is described under Common questions at the start of
this document.

### 12.2 Standardization and legal adoption

The evaluator should help identify the appropriate standards forum, stakeholders and
sponsor. An ISO-style process involves expert work, stakeholder participation and
consensus; publishing this draft does not complete that process. See
[ISO's standards-development description](https://www.iso.org/developing-standards.html).
No standards organization has been selected or claimed as an endorser here.

Legal civil time is a separate policy matter. The U.S. currently has a federal
standard-time/DST framework administered by the Department of Transportation, with
specified exemptions. A proposal for official SolTime needs a separately reviewed
legislative pathway; an experimental display alone does not alter legal time. See
[U.S. DOT's Uniform Time account](https://www.transportation.gov/regulations/time-act).
Other jurisdictions need their own legal review.

Model policy provisions would identify the adopted specification/profile; the authority
to maintain it; default and permitted parameters; which clock governs a local obligation;
how instants are recorded; and who publishes the zone registry. Following another zone
as a display preference must not silently change the clock governing an obligation at
the physical location. Cross-border merging and exceptions need explicit decisions.

### 12.3 Decisions still required

| Topic | Present behavior / evidence | Decision or work still needed |
| --- | --- | --- |
| Scope and maturity | Core clock and examples implemented | Agree which profiles belong in an initial standard |
| Time reference | Unix-style input; host wall clock in Android | UTC mapping, leap events, clock steps/smears and uncertainty |
| Solar accuracy | First-draft approximate series, one value per date, fixed zenith, sea level; against the Meeus formulas, up to 128 s at sunrise and 153 s at sunset in Indianapolis, 348 s in Anchorage (section 5.1) | Choice of solar model (the per-date Meeus upgrade is measured), a full ephemeris comparison and an accepted accuracy range |
| Anchor policy | 06:45 default; configurable | Evidence for selected schedules and seasonal trade-offs |
| Polar/date behavior | Borrowed basis; ±180° meridian; longitude-dependent poles | Political date boundaries, exact poles, wrapped date/zone semantics; extensions for extreme latitudes by consensus or a nation's own rule, such as the author's suggested midday-centered variants, with the base model kept available to all |
| City coverage | Equivalent-area discs and population-led merging | Declared boundaries/radii, the U.S. Census/GHSL radius rule (section 7.1), population updates, border policy and touching discs |
| Zone error and speed | Daily sampled J and automatic ramp heuristic | Continuous-time/raw-field error definitions and verified bounds |
| Registry | Ordered TSV snapshot, row-based leader references | Stable IDs, profile/schema versions, activation, history, authentication and rollback |
| Scheduling | Forward conversion and displays | Local-time inversion, recurrence, update handling and repeated/skipped readings |
| Interchange | No published SolTime wire format | Lossless representation, clock identity, precision and rounding |
| Conformance | Tests and small reference examples | Normative requirements, error budgets and independent acceptance suite |
| Devices | Build/lint and simulated behavior | Phone/emulator validation, accessibility, battery and operational behavior |
| Governance and rights | Author-directed draft; software and documents in the public domain (CC0 1.0); attributed third-party data | Maintenance/change process |
| Adoption evidence | No civil-time field pilot | Pilot design, community participation and measured outcomes |

These open items are part of the evaluation request. They do not prevent review of
the proposed rules, but they prevent representing this package as an adopted or
deployment-ready civil-time standard.

## 13. References and data provenance

The following sources explain inputs and relevant existing conventions. The specific
SolTime choices, algorithms and policy judgments remain the author's proposal.

- [NOAA general solar equations](https://gml.noaa.gov/grad/solcalc/solareqns.PDF):
  approximate declination, equation of time and hour-angle formulas used by the model.
- [RFC 3339, Date and Time on the Internet: Timestamps](https://www.rfc-editor.org/rfc/rfc3339.html):
  relevant existing timestamp syntax and its whole-minute numeric offset limitation.
- [NIST leap-second FAQs](https://www.nist.gov/pml/time-and-frequency-division/leap-seconds-faqs):
  background for defining the reference-time interface.
- [U.S. DOT, Uniform Time](https://www.transportation.gov/regulations/time-act):
  background on U.S. standard time and daylight saving policy.
- [ISO, Developing standards](https://www.iso.org/developing-standards.html):
  standards-process background, not a commitment to a particular submission route.
- [GeoNames](https://www.geonames.org/) and its
  [download service](https://download.geonames.org/export/dump/): place/country data,
  recorded as downloaded October 3, 2026. Columns are reduced and sorted for the app.
  The snapshot has 34,152 places and 252 country/territory records. CC BY 4.0.
- [European Commission Global Human Settlement Layer](https://human-settlement.emergency.copernicus.eu/):
  Urban Centre Database R2024A, general characteristics V1-2, © European Union,
  CC BY 4.0. Names, country, area and centroid are used to match urban cores to places
  and derive equivalent-area radii. The source download is recorded October 3, 2026.
- [U.S. Census Gazetteer files](https://www.census.gov/geographies/reference-files/time-series/geo/gazetteer-files.html):
  2023 national places, land area and location; public-domain source as recorded in
  the project attribution. Used with GHSL for U.S. city radii.
- [Bundled attribution and transformations](data/ATTRIBUTION.md): details
  accompanying the TSV files, including the credit lines the app displays. These
  data terms are distinct from the public domain dedication that covers the SolTime
  software and documents.
- [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/): the public
  domain dedication the author uses for the SolTime software and documents. The full
  text, and the files it does not cover, are in `LICENSE`.

The Earth texture is identified in the project as NASA Blue Marble
`land_ocean_ice`, 2048 × 1024. It is a visual aid and has no role in the clock's
mathematical calculations.

**Draft history:** the author's original direction was recorded July 11, 2026, with
clock/zone/device decisions on October 3. October 5 implementation fixes addressed
motion replanning, event-date selection, supplied ramps, publisher input handling,
the C interface and Android layout. Draft 1 of this evaluation document consolidated
the design, qualified the claims, updated the numerical examples, and made the Android
source buildable apart from the rest of the repository. No clock algorithm was changed
in that documentation and packaging revision.

Draft 2, the evening of October 5, adds the plain-language summary and common
questions at the start, the author's reasons for the morning anchor and position on
extreme latitudes, the public domain dedication, and the author's request for a policy
reader. The package now carries the full C++ project with a rerun of its checks, and
the Android app as source code only, with no prebuilt APK. Sections 1.1, 6.1 and 7.1
were clarified. No clock algorithm, parameter or data table changed in draft 2.

Draft 3, later that evening, corrects the package after an outside review. The Gradle
wrapper's distribution checksum carried three extra characters, so the Android build
failed on any machine that had to download Gradle; it is corrected and checked with an
empty Gradle home. The worked examples now write UTF-8 in any locale; the differential
test no longer requires the Linux-only `ionice`; source comments no longer name the
author's folders; and the build skips the desktop display unless its source is present.
Section 5.1 now states the solar model's measured accuracy, with
`SolarAccuracy.java` to reproduce it. The far-north position, the cross-border groups
(section 7.1) and a tidied `BUG_FIX_PLAN_2026-10-05.md` were added. No clock algorithm,
parameter or data table changed in draft 3.
