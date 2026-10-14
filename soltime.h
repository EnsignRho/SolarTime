//////////
//
// soltime.h -- SolTime:  solar-anchored continuous civil time (no timezones, no DST).
//
// The one rule (soltime_spec.md §1):  offset = ANCHOR - sunriseUTC(basisLat).  Sunrise is held
// at the bus anchor (default 06:45); winter light lands exactly when kids walk out, summer
// surplus spills into the evening.  Each day's offset holds through its daylight -- true
// seconds -- and the day-to-day change is absorbed at night (§9).  A circular slope-limited
// erosion (§3) caps the drift and keeps raw-field sunrise_local <= ANCHOR wherever a place keeps
// its own basis; polar places borrow the nearest latitude with real day and night (§4). Zones lock a
// region to one clock (§8); the device clock follows the true clock (§10).
// BUG_FIX_PLAN_2026-10-05.md FIX-07 — clarified 2026-10-05 16:54:43 EDT (UTC-04:00).
// Shared or followed zone clocks do not guarantee this sunrise bound at every location.
//
// Conventions:  day = continuous day-of-year, 0.0 = Jan 1 00:00 UTC, valid [0, 366).
// Longitude degrees EAST [-180, +180], latitude [-90, +90].  C ABI, libsoltime.so.
// Validation lives at the no-prefix API layer; iSolTime_* internals trust their inputs.
//
//////
#ifndef SOLTIME_H
#define SOLTIME_H

	// BUG_FIX_PLAN_2026-10-05.md FIX-05 — fixed 2026-10-05 16:48:37 EDT (UTC-04:00).
	// Standalone C11/C++ header; retain the public aliases and ABI without sibling headers.
	#include <stdint.h>
	typedef double f64;
	typedef int32_t s32;
	typedef uint32_t u32;
	typedef struct SSolTime SSolTime;
	typedef struct SSolZone SSolZone;
	typedef struct SSolRegistry SSolRegistry;
	typedef struct SSolClock SSolClock;

	#ifdef __cplusplus
	extern "C" {
	#endif


	//////////
	// Parameters.  Pass NULL anywhere an SSolTime* is taken to use the defaults.
	//////
		struct SSolTime
		{
			f64		anchor_secs;			// Sunrise anchor, secs after local midnight (default 24300 = 06:45)
			f64		max_slide_secs;			// Erosion slope limit, secs of offset change per day (default 240)
			f64		zenith_deg;				// Sunrise zenith (default 90.833 = official, incl. refraction)
			f64		basis_min_secs;			// M:  min day AND night at the basis latitude (default 14400 = 4 h; §4)
			f64		night_start_secs;		// Earliest start of the night's adjustment (default 72000 = 20:00; §9)
			f64		night_end_secs;			// Latest end of the night's adjustment (default 14400 = 04:00; §9)
			f64		max_correction;			// Display correction bound, +/- secs per sec (default 0.5; §10)
			f64		design_speed_kmh;		// Zone ramps keep this speed inside max_correction (default 128.75 = 80 mph; §8)
			f64		ramp_min_secs_per_km;	// Ramp slope floor where the budget leaves no room (default 2; §8)
			f64		check_tolerance_secs;	// Auto-flex checkpoint tolerance (default 0.05; §10)
			f64		check_max_secs;			// Longest checkpoint interval (default 300 = 5 min; §10)
		};


	//////////
	// Zone:  a city's disc -- 1.5x the city's radius (spec §8) -- locked to SolTime.  Zones whose
	// discs overlap merge into one area on the clock of their most populous city.  The fields
	// after rampKm were added later:  older five-field initializers still mean the same thing.
	//////
		struct SSolZone
		{
			f64		latDeg;					// Center (the city)
			f64		lonDegE;
			f64		quantumSecs;			// Only for an auto radius:  the accuracy class in seconds of time
			f64		radiusKm;				// The disc's radius; <= 0 = auto (the honest radius of quantumSecs)
			// BUG_FIX_PLAN_2026-10-05.md FIX-03 — contract updated 2026-10-05 16:54:43 EDT (UTC-04:00).
			f64		rampKm;					// < 0 = auto, 0 = hard edge, > 0 = requested width, capped at earlier discs
			f64		population;				// Who leads a merged zone:  the most populous (ties:  registry order)
			f64		jumpSecs;				// J;  <= 0 = compute, > 0 = as given (a published registry carries it)
		};

		// Opaque:  create/delete own them
		struct SSolRegistry;
		struct SSolClock;

		// soltime_clockShow() flags
		#define SOLCLOCK_NEW_ZONE	0x01	// The display jumped since the last call (more than a minute's correction)
		#define SOLCLOCK_FAST		0x02	// Moving faster than the correction can follow:  minute checkpoints, true time
		#define SOLCLOCK_NO_FIX		0x04	// No position yet (the call returns NAN)


	//////////
	// API -- the field
	//////
			// Fill p with the defaults above
			void			soltime_defaults				(SSolTime* p);

			// Gregorian calendar API (years 1600..9999). Inputs/outputs use Unix seconds,
			// never seconds-of-year. A date argument selects the UTC-labeled calendar day;
			// for today's local sun/night, select the physical/owner date with solarDateUnix below.
			f64 soltime_offsetUnixSecs(SSolTime* p, f64 lat, f64 lon, f64 utcUnix);
			// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
			// UTC-labeled date of the raw field at this physical location (independent of
			// any followed zone). Use for its sun/basis; use the clock owner's location for night.
			f64 soltime_solarDateUnix(SSolTime* p, f64 lat, f64 lon, f64 utcUnix);
			s32 soltime_sunTimesUnix(SSolTime* p, f64 lat, f64 lon, f64 dateUnix, f64* riseUnix, f64* setUnix, f64* noonUnix);
			f64 soltime_basisLatUnix(SSolTime* p, f64 lat, f64 dateUnix);
			s32 soltime_nightWindowUnix(SSolTime* p, f64 lat, f64 lon, f64 dateUnix, f64* startUnix, f64* endUnix);

			// UTC -> local offset in seconds for this place at this moment (day may be fractional;
			// the seconds pattern of §9 applies).  NAN on invalid input.
			f64				soltime_offsetSecs				(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day);

			// Local seconds-of-year from UTC seconds-of-year ([0, 366*86400)).  The result is RAW:
			// near the date line it can fall outside [0, yearSecs) -- a different local date.
			f64				soltime_localFromUtc			(SSolTime* p, f64 latDeg, f64 lonDegE, f64 utcSecsOfYear);

			// The place's own raw sun for day floor(day), UTC seconds of that day (near the date
			// line these legitimately fall outside [0, 86400)), at p's zenith.  Returns 0 = normal,
			// +1 = polar day, -1 = polar night (rise/set degenerate toward solar noon/midnight),
			// -2 = invalid input.  Outs are nullable.
			s32				soltime_sunTimesUtc				(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day, f64* riseSecs, f64* setSecs, f64* noonSecs);

			// The latitude whose daylight this place uses on day floor(day) (§4).  NAN on invalid.
			f64				soltime_basisLatDeg				(SSolTime* p, f64 latDeg, f64 day);

			// Tonight's adjustment window -- after local day floor(day) -- in UTC seconds of year
			// (raw:  it may run past the year's edges).  0 = ok, -2 = invalid input.
			s32				soltime_nightWindowUtc			(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day, f64* startUtc, f64* endUtc);

			// Great-circle distance, km (spherical earth, R = 6371).  NAN on invalid.
			f64				soltime_distanceKm				(f64 lat1, f64 lon1, f64 lat2, f64 lon2);


	//////////
	// API -- zones (§8)
	//////
			// The honest auto radius:  the largest lock radius whose disc stays within
			// quantumSecs/2 of the field, every bearing, every day.  NAN on invalid.
			f64				soltime_zoneRadiusKm			(SSolTime* p, f64 quantumSecs, f64 latDeg);

			// The smallest standard class (60/300/600/1200 s) whose honest radius covers radiusKm
			// at this latitude; beyond 1200 s, whole minutes.  NAN on invalid (or nothing covers it).
			f64				soltime_zoneQuantumForRadius	(SSolTime* p, f64 radiusKm, f64 latDeg);

			// Build a registry: overlapping discs merged, jumps and ramps computed once. Given jumps
			// are reused; positive ramps are capped at the gap to earlier discs (FIX-03 above). NULL on invalid.
			SSolRegistry*	soltime_registryCreate			(SSolTime* p, const SSolZone* zones, s32 nZones);
			void			soltime_registryDelete			(SSolRegistry* r);

			// Zone i's disc radius;  its merged zone's ramp width and jump (J -- the zone's accuracy:
			// nobody in it is more than J seconds from their own SolTime, on each day's value).  NAN on invalid.
			f64				soltime_zoneLockKm				(SSolRegistry* r, s32 i);
			f64				soltime_zoneRampKm				(SSolRegistry* r, s32 i);
			f64				soltime_zoneJumpSecs			(SSolRegistry* r, s32 i);

			// The leader of zone i's merged zone (whose clock it keeps).  -2 on invalid.
			s32				soltime_zoneLeader				(SSolRegistry* r, s32 i);

			// Every merged zone covering the point (its leader), in layer order (the last one is on
			// top).  Returns the total count and fills up to capacity entries (weight:  1 = locked,
			// falling to 0 across the ramp; nullable).  -2 on invalid input.
			s32				soltime_zonesAt				(SSolRegistry* r, f64 latDeg, f64 lonDegE, s32* idx, f64* weight, s32 capacity);

			// The clock of zone i's merged zone -- the same anywhere -- as an offset.  NAN on invalid.
			f64				soltime_zoneOffsetSecs			(SSolRegistry* r, s32 i, f64 day);

			// The layered clock at a point (field + every zone, capped leads).  NAN on invalid.
			f64				soltime_offsetSecsR				(SSolRegistry* r, f64 latDeg, f64 lonDegE, f64 day);
			f64 soltime_offsetUnixSecsR(SSolRegistry* r, f64 lat, f64 lon, f64 utcUnix);
			f64 soltime_zoneOffsetUnixSecs(SSolRegistry* r, s32 i, f64 utcUnix);


	//////////
	// API -- the device clock (§10)
	//////
			// r is nullable (raw field only); it must outlive the clock.  With r, the true clock is
			// evaluated with the registry's parameters; p sets the correction (max_correction, check_*).
			SSolClock*		soltime_clockCreate				(SSolTime* p, SSolRegistry* r);
			// Real-calendar clock: fix/show/nextCheck use Unix seconds on this instance.
			// Legacy clockCreate retains the periodic 366-day seconds-of-year contract.
			SSolClock* soltime_clockCreateUnix(SSolTime* p, SSolRegistry* r);
			void			soltime_clockDelete				(SSolClock* c);

			// Feed each position fix (GPS or typed in):  speed km/h, heading degrees clockwise from north
			s32				soltime_clockFix				(SSolClock* c, f64 utcSecsOfYear, f64 latDeg, f64 lonDegE, f64 speedKmh, f64 headingDeg);

			// The local seconds-of-year to display now (raw, like soltime_localFromUtc).  flags
			// (nullable) get SOLCLOCK_*. NAN before the first fix unless following a zone.
			f64				soltime_clockShow				(SSolClock* c, f64 utcSecsOfYear, u32* flags);

			// Exact mode:  the true value on every call, no increments in between
			void			soltime_clockExact				(SSolClock* c, s32 on);

			// Follow zone i's clock wherever the device is (anyone may be on any zone);  -1 = back
			// to the clock of where the device is.  0 ok, -2 invalid (a zone needs the registry).
			// soltime_clockFollowing:  the leader followed, or -1.
			s32				soltime_clockFollowZone			(SSolClock* c, s32 zone);
			s32				soltime_clockFollowing			(SSolClock* c);

			// When the next checkpoint is due (UTC seconds of year); NAN before the first show
			f64				soltime_clockNextCheckUtc		(SSolClock* c);


	#ifdef __cplusplus
	}
	#endif

#endif	// SOLTIME_H
