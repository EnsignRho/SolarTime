# SolTime:  One Clock, All Year

### A proposal for solar-anchored civil time — 2026

**Proposed by:** Rick C. Hodgin
**Date:** October 2026
**Status:** Evaluation draft; reference software built and tested as reported in §9.
Independent astronomical validation, interoperability rules and field testing remain open.
**Document revision:** 2026-10-05, evaluation draft 1.
**Combined handoff:** [SolTime_overview.md](SolTime_overview.md) consolidates the policy,
mathematics, implementation behavior, evidence and open decisions in one document.

---

## Summary

**The problem.** Our clocks change twice a year, and even when they hold still they fit the sun
poorly. A conventional time zone shares one clock across places whose sunrise times differ.
It coordinates schedules, but does not align every location with the sun. Every few years the debate returns:  permanent daylight saving time
gives dark winter mornings, and permanent standard time gives summer sunrises before 5 a.m.

**The proposal.** Replace both with a clock that follows the sun every day, with no switch:

- **The local solar clock targets sunrise at 6:45 a.m.** Where a place keeps its own
  solar basis, smoothing can make sunrise earlier, never later. Sharing a city clock,
  following a distant zone, and the far-north rule have exceptions described below.
- **Everything else follows from that.** The extra daylight of the longer seasons goes to the
  evening, when people can use it.
- **Seasonal adjustments are gradual.** The stationary raw clock adjusts overnight, between 8 p.m. and 4 a.m., by no
  more than about two minutes in the lower 48 states. At a fixed location outside its adjustment windows, the modeled clock runs at
  the input clock’s rate.
- **Every city keeps one clock.** Each city of 100,000 or more has a SolTime zone reaching 1.5
  times the city's radius (§5); cities whose zones overlap share the largest city's clock, and
  anyone outside a zone may keep its clock. Positive-width zone edges blend smoothly;
  the prototype also permits explicitly requested hard edges (§6).
- **Phones and computers do the work.** They already know where they are, and they compute
  the time.
- **The global reference doesn't change.** Coordinated Universal Time (UTC) stays the legal
  and technical anchor for aviation, finance and computing.

**What it asks of lawmakers:**
1. Recognize the published SolTime standard as an optional legal time.
2. Authorize a pilot in one state or metro area.
3. Adopt the city zone rule and assign authority over the zone registry.
4. Fix the handful of policy parameters (§8).

A working reference implementation already exists.

---

## 1. Why the clocks we have don't fit the sun

**Time zones coordinate a region on one clock.** U.S. standard time dates to the
Standard Time Act of 1918, and the Uniform Time Act of 1966 established a national
daylight saving framework. See [U.S. DOT’s account](https://www.transportation.gov/regulations/time-act). A zone gives one clock to places
whose sunrise differs by an hour or more:

- **Same zone, different mornings.** Atlanta and New York are both on Eastern Time, but
  Atlanta's December sunrise comes 22 minutes later than New York's (7:38 vs. 7:16 a.m.).
- **Worse at the western edge of a zone.** Indianapolis, also on Eastern Time, sees the sun
  at 8:02 a.m. in December.

**Daylight saving time adds seasonal clock changes.** Where observed in the U.S.,
clocks move by an hour in spring and fall. SolTime proposes a gradual daily adjustment.
Its effects on sleep, safety and public acceptance require independent study.

**The two permanent options emphasize different trade-offs:**

| Option | Winter | Summer |
|--------|--------|--------|
| Permanent daylight saving time | Dark mornings:  sunrise after 8 a.m. in much of the country, after 9 a.m. in some places | Late evenings |
| Permanent standard time | Sunrise reads one hour earlier than under permanent DST | Near the June solstice, sunrise around 4:15 a.m. in Chicago and 4:24 a.m. in New York; sunset reads an hour earlier than under DST |

The policy question is how to allocate limited winter daylight and longer summer
daylight while preserving workable schedules. The comparisons here illustrate clock
readings; they do not establish health, safety or economic benefits.

**SolTime takes neither side.** It is not a time zone choice. The clock simply follows the sun
day by day.

---

## 2. The proposal in one rule

> **Anchor the local solar clock to sunrise at 6:45 a.m.**

This rule defines the raw solar field. The overnight smoothing keeps sunrise at or
before the anchor wherever a place uses its own solar basis. City zones share their
leader's clock (§5), so another neighborhood's sunrise may be later; voluntarily
following a distant zone has no local sunrise guarantee. Far-north places sometimes
borrow a basis latitude (§7). These are limits of the existing policy.

That one rule produces everything else:

- **Mornings:** the raw-field clock places sunrise by the anchor wherever the location
  keeps its own basis. The policy aims to provide light before school buses arrive.
- **Winter:** the short day is placed so it begins when people walk out the door.
- **Summer:** the long day's extra light spills into the evening, as daylight saving time
  intends, but in exact proportion to the season instead of a one-hour jump.
- **Every day in between:** sunrise moves a little each day, so the clock moves a little each
  day too. On the busiest days of the year that's about 1 to 2 minutes at U.S. latitudes
  (just over 3 in Anchorage), adjusted overnight (§4).

Why 6:45? It is a proposed policy choice intended to provide morning light before
school travel. Actual bus schedules, terrain, weather and shared-zone effects vary;
the rule cannot guarantee daylight at every bus stop. Earlier anchors favor mornings
and later anchors favor evenings. The anchor is configurable (§8), and its suitability
needs local evidence.

---

## 3. What people would see

The figures below are reference-software estimates recomputed for this draft,
not independently validated astronomical predictions. "Today" means legal time in 2026:
standard time in winter, and daylight saving time from March 8 to November 1 where it applies.
The SolTime figures describe each listed location's own solar clock, not every
neighborhood sharing a metro clock or a voluntarily followed distant zone.

**December 21 (shortest day) and June 21 (longest day):**

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

All times use the 24-hour clock, rounded to the nearest minute; `+1 day` means
sunset after local midnight. Recomputed October 5 with the current Java Gregorian
implementation, bundled GeoNames coordinates and the JDK’s IANA tzdb 2026a rules.
These are raw-field examples at each listed city coordinate, without shared zones.
`android/evaluation/OverviewExamples.java` and its recorded output reproduce them.

**Over the 365 days of 2026:** strict comparisons use unrounded event times.

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

### The trade-off, stated plainly

- **Mornings:** the listed raw-field clocks bring sunrise to or before 6:45 a.m., where
  today many have one after 7:00 a.m. for 100 to 244 days a year. Shared zones and the
  far-north basis can give later local sunrises.
- **Summer sunset readings:** they become later than under today’s daylight saving
  time, by 15 minutes in Miami, about 1 hour 20 minutes in New York and 1 hour 34 minutes
  in Seattle. The physical amount of daylight does not change; its relation to fixed
  clock-time schedules changes.
- **Winter evenings:** in the northern half of the country they get **earlier**. In December the
  sun sets about 30 minutes earlier than today in New York and Chicago, and about an hour earlier
  in Minneapolis and Seattle. The winter day is short there, and the rule spends it on the morning
  so that it starts when people walk out the door.

A legislature that prefers more winter evening light can choose a later anchor (§8). That shifts the modeled daylight later; its effect on school travel depends on
local schedules and conditions. The math works the same for any anchor.

**Alaska** shows the far end of the trade. In December the sun is up only about 5½ hours in
Anchorage. SolTime places that daylight from 6:45 a.m. to 12:12 p.m., where today it runs from
10:14 a.m. to 3:41 p.m. Section 7 describes how the system handles places with very short days,
and places where the sun doesn't rise at all.

---

## 4. How the clock changes, day to day

- **There is no "spring forward" and no "fall back."** The clock never jumps (except at the
  model date boundary at ±180° longitude, §6; devices and hard edges have other exceptions).
- **At a fixed location, the modeled clock runs at the input rate outside adjustment
  windows.** A shared or followed clock uses its leader’s window. Moving devices can
  have a different rate, and leap seconds/input-clock corrections need a separate policy.
- **The day's change happens overnight.** Between 8 p.m. and 4 a.m., inside the night, the
  clock's seconds run very slightly long or short, smoothly easing in and out, until the clock
  matches tomorrow's sunrise.
- **The changes are tiny.**
  - At U.S. latitudes the overnight change is at most about 1 to 2 minutes:  56 seconds in
    Honolulu, 97 in Indianapolis, 122 in Seattle, and 191 (just over 3 minutes) in Anchorage.
  - The reported peak modeled clock-rate deviation below about 45° latitude is 0.8%.
    This is a rate measurement, not a demonstrated limit on human perception.
- **The stationary raw-field clock is designed to increase continuously.** The reported
  tests sample every minute of a synthetic year at ten locations. Fast westward travel
  can produce a backward-running local clock (§6).
- **A modeled calendar day spans 86,400 displayed seconds.** Its elapsed input-clock
  duration is 86,400 minus the offset change between its two local midnights. That change
  need not equal one night’s full adjustment. Leap-second behavior is not yet specified.

In winter the clock is set further behind UTC, and in summer closer to it. Indianapolis, for
example, runs about UTC − 6:17 in December and UTC − 3:31 in June, sliding gradually between
the two. Phones and computers store and exchange UTC as they do today, and SolTime is the time
they display.

---

## 5. Every city keeps one clock:  SolTime zones

Strict solar time differs by 4 minutes for every degree of longitude, which is about 45 to 65
miles east or west across the U.S. (the distance shrinks toward the north). A city needs one
shared clock for schools, businesses, transit and broadcast schedules. SolTime provides this with
**SolTime zones**.

**A SolTime zone extends its leader’s clock across a set of city discs.** Typical
metro zones extend tens of miles; a merged group can be larger. Positive-width ramps
blend the clock at the edge. The implementation locks time of day modulo one day;
date agreement across the ±180° boundary remains a standards issue.

Sharing a clock means sharing the leader's timing, not the instant of sunrise. A
neighborhood's sunrise can fall after 6:45 on that shared clock; the published zone
deviation J estimates the difference from the clock beneath that zone’s layer. The software does not
shift the whole zone to guarantee the anchor at every point.

- **Every city of 100,000 people or more has one.** The zone reaches **1.5 times the city's
  equivalent-area radius**. A circular disc is an approximation; it does not guarantee
  coverage of every part of an irregular municipal boundary.
  - **A city's size is declared, with measured defaults.** A city may declare its own radius.
    By default, in the U.S. it is the larger of the city limits (U.S. Census) and the city's
    dense built-up core (the European Commission's Global Human Settlement Layer, measured from
    satellite imagery). Elsewhere it is the built-up core.
  - Indianapolis:  its city limits give a radius of 10.7 miles, so its zone reaches **16
    miles** from downtown.
- **Where zones overlap, they become one zone, on the clock of its largest city.** New York
  City's zone takes in 14 cities (the five boroughs, Newark, Jersey City, Yonkers, Paterson,
  Elizabeth, Stamford and Edison). Los Angeles's takes in 35. The 328 U.S. cities of 100,000
  or more make 174 zones.
- **Every zone publishes a modeled deviation, J:** a numerical estimate of its largest
  daily difference from the clock beneath its layer. This is not astronomical accuracy
  or a certified bound against each resident’s raw clock; overnight differences may be
  larger. The reported U.S. median is about 45 seconds, with several large metros at
  roughly 2 to 5 minutes.

| Zone | Cities sharing its clock | Reaches (from the center) | Edge blend | Accuracy |
|------|------|------|------|------|
| New York City | 14 | 42 miles | 13 miles | 3.7 minutes |
| Los Angeles | 35 | 65 miles | 17 miles | 4.8 minutes |
| San Jose (the Bay Area) | 13 | 63 miles | 18 miles | 5.1 minutes |
| Dallas–Fort Worth | 14 | 46 miles | 12 miles | 3.5 minutes |
| Chicago | 3 | 43 miles | 13 miles | 3.7 minutes |
| Miami–Fort Lauderdale | 9 | 36 miles | 7 miles | 2.0 minutes |
| Phoenix | 10 | 30 miles | 8 miles | 2.3 minutes |
| Houston | 4 | 30 miles | 7 miles | 2.2 minutes |
| Denver | 7 | 19 miles | 6 miles | 1.6 minutes |
| Atlanta | 2 | 17 miles | 3 miles | 0.9 minutes |
| Indianapolis | 1 | 16 miles | 5 miles | 1.4 minutes |
| Anchorage | 1 | 35 miles | 106 miles (far-north rule, §7) | 8.3 minutes |

- **Anyone can be on any zone.** A household, school, business or whole town outside a zone
  can keep that zone's clock, and any phone or clock can be set to follow any zone wherever it
  is. A family in a nearby town can keep the city's time; a traveler can keep home time.
- **No lines on the map.** Leaving a zone, the clock blends back to the local solar time over a
  few miles of road. A car's clock changes smoothly as it drives. The default ramps target a correction rate
  of at most half a second per second at 80 mph; far-north and closely spaced zones
  can require the fast-motion rule. Explicit hard edges and the model date boundary
  are exceptions to smooth travel.
- **Example:  central Indiana.** On June 21:

| Town | Miles from Indianapolis | Its clock | Its own solar time vs. Indianapolis |
|------|------|------|------|
| Greenwood | 11 | Indianapolis's, exactly | 18 seconds behind |
| Carmel | 15 | Indianapolis's, exactly | 51 seconds ahead |
| Fishers | 15 | Indianapolis's, exactly | 71 seconds ahead |
| Noblesville | 21 | on the edge blend | 1½ minutes ahead |
| Anderson | 34 | its own (or Indianapolis's, if it chooses) | 3 minutes ahead |
| Columbus | 41 | its own (or Indianapolis's) | 53 seconds behind |
| Bloomington | 46 | its own (or Indianapolis's) | 3½ minutes behind |
| Muncie | 50 | its own (or Indianapolis's) | 4½ minutes ahead |

  Fort Wayne, 108 miles away, has a zone of its own, about 8½ minutes ahead of Indianapolis.
- **Who decides:** cities declare their size (or accept the measured default), and the zones are
  published as an official registry that every device reads (§8).

---

## 6. Devices and everyday life

**Phones, computers, cars and networked clocks compute SolTime themselves** from their position.
The draft defines a prototype display planner that follows the modeled SolTime:

- **Checkpoints:** the device checks the true time every few minutes (every 5 minutes when it
  is standing still, more often when it moves). Between checks it adjusts the length of its
  seconds to target the next checkpoint. Time/location errors and changed motion
  can prevent an exact arrival.
  - Measured:  a clock standing still for 30 days stayed within 0.008 seconds of the true
    SolTime.
  - A car driving out of a zone at 80 mph stayed within 0.07 seconds.
- **"New Time Zone":** when a device turns on after a trip, or is moved somewhere new, it shows
  the correct time at once with a note such as "New Time Zone".
- **Air travel:** in a plane the clock shows the true time of the ground below, checked at
  least once a minute and re-aimed on every turn.
  - Measured:  within about half a second.
  - Flying east the seconds run fast, flying west slow. At far-northern latitudes a jet flying
    west can outrun the sun, and the accurate clock then runs backward, as solar time really
    does.
- **The model date boundary is ±180° longitude:** crossing it changes the date and can
  also shift the time of day by up to one daily adjustment. It does not trace the
  political International Date Line. A device crossing it shows "New Time Zone".
  Explicit hard edges, relocation and changing a followed zone can also produce steps.
- **No connection, no problem:** without GPS or network, a device keeps following the true clock
  for its last known position. SolTime is pure math, and it needs no server.
- **Keeping another zone's clock:** any device can follow any zone's clock wherever it is (§5).
  Measured:  a device in Denver following Indianapolis's zone stayed within 0.007 seconds of
  Indianapolis's clock for two days.

**Traditional clocks** must be reset now and then. A clock that is never reset drifts behind or
ahead by up to a minute or two per day in spring and autumn, so over a week it can be off by
several minutes. A synchronized clock needs SolTime-aware firmware or software to make these
adjustments; receiving conventional radio/network time alone is insufficient. Replacing
or adapting legacy clocks is a real transition cost (§10).

**Schedules and appointments** have the following intended semantics; conversion
from a requested local time to UTC and recurring-event support remain to be implemented:
- **Local events** (school start, store hours, a doctor's appointment) are set in local SolTime.
- **Events across regions** are stored in UTC and shown in each participant's local SolTime,
  with SolTime-aware conversion software. Existing calendar support is not automatic.
- **Transit timetables and broadcast schedules** are published in the zone's clock.

**Searching any place on Earth:** because SolTime is just math, anyone can look up the current
time anywhere. The reference apps include a search of 34,152 cities and every country.

---

## 7. The far north

Above about 64° latitude the sun can stay down all day in winter, or up all night in summer. A
sunrise rule cannot work where there is no sunrise.

**Borrowing a nearby day.** Such a place takes its timing from the nearest latitude
toward the equator that has at least **4 hours of daylight and 4 hours of darkness** that day.

- **During polar night:** the default winter-solstice examples place modeled solar
  noon near 8:45 a.m.; smoothing and the date affect the exact value.
- **Under the midnight sun:** midday falls around 4:45 p.m.
- **The basis changes continuously** through the borrowing threshold. The ±180°
  boundary and the longitude convention at the geographic poles remain separate issues.
- **The cost:** in the few places just short of polar night, sunrise comes after 6:45 a.m. by
  up to 2 hours.

**Anchorage** (61°N) keeps its own sunrise all year (6:45 a.m.). Utqiaġvik (71°N) borrows during
its polar night and midnight sun.

**Gentler edges up north.** In the far north the edge of a zone blends over a longer distance
(Anchorage's, 106 miles), because the sun's timing changes faster per mile there. Where that
still isn't enough at full highway speed, devices simply show the true time at least once a
minute.

---

## 8. What a law would need to say

The following are **model provisions for discussion**, not legal text.

**1. The standard.** SolTime is defined by a published specification, a reference implementation,
and its test suite, under a version number. All three are kept public, and any change requires a
new version.

**2. The policy parameters.** These are set in statute or by rule:

| Parameter | Proposed value | What it decides |
|------|------|------|
| Sunrise anchor | 6:45 a.m. | Modeled raw-field sunrise target; chosen for morning-light policy |
| Daily change limit | 4 minutes | Fastest the clock may drift per day; it only matters in the far north |
| Overnight adjustment window | 8 p.m. – 4 a.m. | Basis-night adjustment; stationary rate equals input rate outside the owner’s windows |
| Far-north basis | 4 hours of day and night | When a place borrows a nearby day (§7) |
| Device correction limit | ±0.5 seconds per second | Ordinary tracking range; fast motion and resets have separate rules |
| Design speed for zone edges | 80 mph | Target for automatic ramps; polar and narrow-gap exceptions use the fast-motion rule |
| Zone threshold | Cities of 100,000 people | Which cities have a zone by default |
| Zone size | 1.5 × the city's radius | How far a city's clock reaches |
| Merging | Overlapping zones share the largest city's clock | One clock per connected metro |

**3. Official time.**
- **Instants:** legal deadlines, timestamps, contracts and records are fixed in UTC, as most
  systems already store them.
- **Local obligations** (business hours, school schedules, local ordinances) refer to local
  SolTime:  the SolTime of the zone the place is in, or the place's own SolTime outside any zone.

**4. Zone registry.**
- **Every city of 100,000 or more has a zone.** A city may declare its own radius; otherwise
  the measured default applies (§5). States may add zones for smaller places on petition.
- **Rules:** a zone reaches 1.5 times its city's radius; overlapping zones merge on the most
  populous city's clock; every zone’s modeled deviation J is computed and published with it.
- **Anyone may follow any zone:** a person, household, business or device may keep any zone's
  clock.
- **Publication:** a national registry published as open data, updated on a fixed schedule so
  devices can rely on it. It carries every zone's computed values, so every device agrees on
  every zone under the same parameters and numerical rules. Stable identifiers,
  release versions and coordinated activation dates still need to be specified.

**5. Transition.**
- **A pilot:** one state or metro area, with SolTime displayed alongside conventional time for a
  defined period. It should measure public acceptance, school and commute effects, safety data,
  and device readiness.
- **A sunset clause** and a required report to the legislature.

**6. Federal coordination.**
- **Federal coordination:** U.S. DOT describes the existing standard-time/DST framework
  and state exemptions. This draft proposes a different civil clock; legal adoption and
  any official-time pilot need a separately reviewed legislative pathway. Displaying
  an experimental clock alongside legal time does not itself establish legal time.
  See [U.S. DOT](https://www.transportation.gov/regulations/time-act).
- **What stays the same:** UTC remains the reference for aviation, maritime, finance, and
  international coordination. SolTime integration may still require changes to local
  displays, schedules and interfaces in those systems.

---

## 9. Where the work stands (October 2026)

The core clock design has a reference implementation and reported automated checks.
The draft is ready for independent evaluation; the unresolved decisions in §10 prevent
a claim of complete standardization or deployment readiness:

- **Specification** (`soltime_spec.md`):  the rule, the smoothing, the far-north basis, the
  overnight adjustment, zones, the device clock, and the apps.
- **Reference library** (C, `libsoltime.so`) and a corresponding **Java version** for Android.
  - A cross-check runs both versions on thousands of the same questions.
  - The earlier periodic comparison reported differences of a few billionths of a
    second and identical leaders. Gregorian checks were added later. These results
    concern implementation agreement, not astronomical accuracy or universal precision.
- **Automated tests:** the original 12 named checks plus calendar, review, C interface,
  Java and publisher regressions. They cover:
  - sunrise at or before 6:45 on the raw-field clock where the place keeps its own basis
  - no daily change over 4 minutes
  - the stationary raw-field clock increasing in the tested cases
  - input-rate seconds outside the clock owner’s adjustment windows
  - zone accuracy, merging, and smooth edges
  - device behavior standing still, driving, flying, relocating and across New Year

  The October 5 verification record reports all six CTest suites passing, plus
  differential and targeted address/undefined-behavior sanitizer checks. These sampled
  checks are not proof of all-input correctness or absence of memory defects.
- **The zone registry:** all 5,854 cities of 100,000 or more worldwide, with their measured
  sizes, merged into 4,168 zones, each with its modeled deviation J. Built by a published script from open
  data.
- **Desktop display:** a world map, a live clock, simulations of driving and flying, a search of
  world cities, and following any zone's clock.
- **Android app:** a 3D globe, your SolTime clock from GPS, a search of 34,152 cities and every
  country, zone boundaries, following any zone's clock, and an exact-time mode. Not yet run on a
  phone.
- **Data:** GeoNames (geonames.org), CC BY 4.0; the Global Human Settlement Layer's Urban Centre
  Database, © European Union, CC BY 4.0; U.S. Census Bureau Gazetteer files.

---

## 10. Open questions and known limits

- **Winter evenings in the north** are earlier than today (§3). This is the deliberate cost of
  light at the bus stop. The sunrise anchor is the lever if a legislature weighs it differently.
- **Calendar:** both apps use Gregorian dates and Unix/POSIX-style seconds, including
  leap years and century rules. The solar equations remain an approximation; sub-minute
  astronomical accuracy has not been established, especially at high latitudes.
- **Legacy clocks:** clocks that are not network- or radio-synchronized need to be reset. Some
  existing date-and-time formats assume offsets from UTC in whole minutes. SolTime offsets are
  not whole minutes, so systems should store UTC, as best practice already holds.
- **Date boundary:** the prototype uses the ±180° meridian, with a date step plus a
  possible daily-offset step (§6). Alignment with political date boundaries is unresolved.
- **The far north** borrows its timing (§7), so sunrise can come late there just short of polar
  night.
- **Shared and followed clocks:** the 6:45 sunrise bound applies to a location's own
  raw-field clock, with its own basis. It is not a guarantee for every point sharing a
  city clock, in a blend, or following a distant zone.
- **Product work remains:** local-to-UTC scheduling and recurring events; stable zone
  identifiers, registry versions and coordinated update dates; Android widgets, alarms
  and background operation; independent astronomical validation and physical-phone
  accessibility, location and lifecycle testing. The October 5 corrections and their
  verification are tracked in [the bug-fix plan](BUG_FIX_PLAN_2026-10-05.md).
- **Zones that cross a border.** The merge rule follows cities, not borders, on purpose:  where
  people do business across a border, they need to know the local clock, and one metro keeps
  one. Where a U.S. city's zone overlaps a larger city's across the border, the merged zone
  keeps the larger city's clock:  San Diego, Chula Vista and El Cajon keep Tijuana's clock; El
  Paso, Ciudad Juárez's; Laredo, McAllen and Brownsville, their Mexican neighbors'. Detroit's
  zone takes in Windsor, Ontario. The clocks involved differ by only seconds. A legislature that
  prefers otherwise could stop merging at the national border.
- **City data.** The measured city sizes come from open data. That data contains errors (some
  cities listed twice), which the build catches where it can and reports. A city's own declared
  radius always takes precedence.
- **Public acceptance and real-world effects** (sleep, commuting, safety, energy, commerce)
  need independent study and a pilot. The sunrise rule is a modeled clock constraint,
  not evidence of better health, safety or school outcomes.
- **Time representation and conformance:** leap seconds, input-clock steps/smears,
  timestamp interchange, supported coordinate conventions, numerical tolerances and
  compliance criteria remain standards work. RFC 3339 offsets have whole-minute
  resolution; storing UTC alone does not define recurring local appointments.
- **Release and rights:** the evaluation package needs an identified source/data
  snapshot and explicit software/document licensing terms from the author. Data
  licenses are recorded separately and do not license the SolTime software.

The combined [overview](SolTime_overview.md) sets out the review questions and the
Android handoff. It distinguishes defined prototype behavior from proposed policy
and unresolved standards requirements.

---

## Appendix A — Technical summary (for staff and engineers)

**The rule.** `offset = 6:45 − sunrise(UTC)`, and local time = UTC + offset. Sunrise is computed
with the approximate NOAA general solar equations and a fixed 90.833° zenith
convention. Sub-minute astronomical accuracy has not been established.
Each degree of longitude east adds exactly 4 minutes to daily plateau offsets;
instantaneous night-window differences can vary slightly.

**Smoothing.**
- **The slope limit:** the daily offset is the tightest curve that never exceeds the raw rule
  and never changes by more than 4 minutes per day. Real-calendar calculations use
  actual Gregorian dates across neighboring years; only the legacy simulation API
  uses a circular 366-day year.
- **Why that preserves the modeled sunrise ceiling:** the curve never exceeds the raw rule, so sunrise can only
  come *earlier* than 6:45, never later, wherever a place keeps its own basis. That is a
  property of the solar model, not a guarantee of real bus-stop lighting.
- **Where it binds:** the limit is active only at high latitudes.

**Far-north basis.** A place uses its own latitude whenever its day and its night are both at
least 4 hours long. Otherwise it uses the latitude, toward the equator, where the day (or the
night) is exactly 4 hours. That latitude has a closed-form solution, and the result is continuous
in both place and season.

**The overnight adjustment.**
- **The window:** each day's offset applies from the end of one night's window to the start of
  the next. The window runs from the basis sunset to the next basis sunrise, clipped to
  8 p.m.–4 a.m. local time.
- **The shape:** inside the window the change follows a raised cosine, so the clock's rate eases
  in and out with no corners.
- **How short it gets:** approximately 1 hour 15 minutes in the reported default
  samples. A conservative allowance subtracts one daily slide (4 minutes) from this
  nominal value; the precise window is calculated for each night.

| Latitude band | Shortest window | Largest modeled clock-rate deviation |
|------|------|------|
| 0–45° | 5.6 h | 0.8% |
| 45–55° | 3.9 h | 1.4% |
| 55–60° | 2.4 h | 2.4% |
| 60–89° | 1.25 h | 10.6% |

**Zones.**
- **The disc:** every city of 100,000 or more has a disc of 1.5 times its radius. The radius is
  declared, or measured:  in the U.S. the larger of the Census city limits and the GHSL built-up
  core; elsewhere the core; failing both, an estimate from population at the country's median
  core density. Radius = √(area / π).
- **Merging:** discs that overlap merge, transitively, into one zone:  the union of the discs,
  on the clock of the most populous city in it. 5,854 cities make 4,168 zones.
- **Modeled deviation (J):** numerically estimated from daily plateau values against
  the clock beneath the zone’s layer, using disc-rim and interior samples with refinement.
  It is not a certified continuous-time bound or necessarily a bound against the raw
  field after earlier layers are applied. Nighttime differences can exceed J. Reported
  median: 0.3 minutes worldwide; 0.7 minutes in the U.S.
- **The edge ramp:** outside the discs, the zone's lead over the clock beneath it is capped,
  and the cap shrinks to zero across the ramp. The ramp's slope is set by the 80 mph budget,
  with a floor of 2 seconds per km (3.2 s per mile) in the far north. This is a
  numerical sizing target, with narrow-gap and instantaneous-rim exceptions. The cap starts from the
  larger of J and the lead at the nearest disc's rim, so the ramp meets its disc lock,
  including the notches where two discs meet, for positive-width ramps.
- **Layering:** zones apply in a fixed order, the most populous city first, and each is blended
  over the clock beneath it. Positive-width ramps avoid steps at their boundaries;
  hard edges and the model date boundary are exceptions.
- **The published registry:** every zone’s modeled deviation and ramp are computed once (21 seconds for
  the whole world) and published with the registry; a device loads it in a tenth of a second.

**The device clock.**
- **Checkpoints:** the device computes the true time and plans each interval with a halfway
  self-check. The tolerance is 0.05 seconds, the shortest interval 0.1 seconds, and the longest
  5 minutes when standing still.
- **Ordinary correction:** within ±0.5 seconds per input second. Errors too large to
  recover within one minute cause a reset and "New Time Zone"; fast motion has a
  separate unbounded-rate rule.
- **Fast motion** (a plane, judged over the next second): the device shows the true time at every
  checkpoint, at most one minute apart, and re-aims on every turn.
- **Exact mode:** the device shows the true value on every tick.

**Reported checks and their scope (automated tests).**

Numerical samples and C++/Java agreement test the implementation, not astronomical
truth or universal correctness. Detailed scenarios appear in the specification.

| Property | Reported result / scope |
|------|------|
| Modeled sunrise on the stationary raw field | At or before 6:45 where the location keeps its own basis; equality where the limiter is inactive |
| Daily change | At most 4 minutes, at every latitude, across New Year |
| Stationary monotonicity | Sampled every minute of a synthetic year at 10 locations; moving clocks can run backward |
| Clock rate | Equals input rate between the clock owner’s overnight windows at a fixed location |
| Zone deviation | Daily sampled/refined estimate J relative to the underlying layer; nighttime differences can be larger |
| Zone edges | Positive-width boundary continuity checked; 80 mph target has polar and narrow-gap exceptions |
| Device clock, standing still 30 days | Within 0.008 s |
| Device clock, 80 mph through a zone's edge | Within 0.07 s |
| Device clock, airliner with turns | Worst reported example 0.52 s |
| Following a zone | Within 0.007 s of the followed zone's clock |
| C and Java versions | Earlier periodic samples within about 0.000000002 s; leaders identical; not a universal calendar tolerance |

---

## Appendix B — Glossary

- **Basis latitude:** the latitude whose daylight timing a far-north place borrows (§7).
- **Zone:** a city's clock shared across 1.5 times the city's radius; overlapping zones share
  the most populous city's clock (§5).
- **Modeled zone deviation (J):** a sampled/refined daily difference against the
  clock beneath the zone’s layer; not an absolute astronomical accuracy guarantee.
- **Edge ramp:** the band around a zone where its clock blends back into the surrounding time.
- **Overnight adjustment:** the 8 p.m.–4 a.m. window in which the day's small change is absorbed.
- **SolTime:** the local civil time this proposal defines.
- **UTC:** Coordinated Universal Time, the world's reference clock. It is unchanged by this
  proposal.

## Appendix C — References and data

- **Evaluation handoff:** [SolTime_overview.md](SolTime_overview.md) plus the complete
  `android/` folder, including its bundled `data/` snapshot and attribution. The
  overview can be read independently of these two original documents.
- **Full implementation review:** provide a versioned archive or repository snapshot
  of this project, rather than a path on the author’s computer. The full project includes:
  - `soltime_spec.md`, the library, and its tests
  - the desktop display
  - the Android app (`android/`)
  - the cross-check (`difftest/`)
- **Solar equations:** [NOAA general solar equations](https://gml.noaa.gov/grad/solcalc/solareqns.PDF),
  using the 90.833° sunrise zenith convention.
- **Interchange:** [RFC 3339 §4.2](https://www.rfc-editor.org/rfc/rfc3339.html#section-4.2).
- **U.S. time policy:** [U.S. DOT, Uniform Time](https://www.transportation.gov/regulations/time-act).
- **Standards development:** [ISO’s process overview](https://www.iso.org/developing-standards.html);
  cited as process background, not an endorsement or selected submission route.
- **Place data:** GeoNames (geonames.org), CC BY 4.0. Cities with a population of 15,000 or
  more, and country names.
- **City sizes:** the Global Human Settlement Layer Urban Centre Database R2024A (European
  Commission, Joint Research Centre), © European Union, CC BY 4.0; the U.S. Census Bureau's 2023
  Gazetteer files (land area of incorporated places), public domain.
