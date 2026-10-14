//////////
//
// soltime.cpp -- SolTime implementation (see soltime_spec.md).
//
// The rule is §1, the erosion §3, the basis latitude §4, zones §8, the seconds pattern §9,
// the device clock §10.  Solar engine:  NOAA general solar position equations -- declination
// and equation of time as Fourier series in the fractional year, hour angle at the sunrise
// zenith.  BUG_FIX_PLAN_2026-10-05.md FIX-07 — corrected 2026-10-05 16:54:43 EDT (UTC-04:00).
// Approximate sea-level times; sub-minute astronomical accuracy has not been established.
//
//////
	#include "soltime.h"
	#include <math.h>
	#include <algorithm>
	#include <vector>
	// BUG_FIX_PLAN_2026-10-05.md FIX-05 — fixed 2026-10-05 16:48:37 EDT (UTC-04:00).
	// Calendar indexing is private; the public header needs only its ABI types.
	typedef int64_t s64;


//////////
// Constants
//////
	static const s32	_SOLTIME_YEAR_DAYS		= 366;							// v1 periodic year (spec §2/§6)
	static const f64	_SOLTIME_DAY_SECS		= 86400.0;
	static const f64	_SOLTIME_YEAR_SECS		= 366.0 * 86400.0;
	static const f64	_SOLTIME_DEG2RAD		= M_PI / 180.0;
	static const f64	_SOLTIME_RAD2DEG		= 180.0 / M_PI;
	static const f64	_SOLTIME_EARTH_KM		= 6371.0;						// Spherical earth, everywhere
	static const f64	_SOLTIME_KM_PER_DEG		= 6371.0 * M_PI / 180.0;		// 111.195 km per degree of arc
	static const f64	_SOLTIME_SECS_PER_DEG	= 240.0;						// One degree east = +4 minutes
	static const f64	_SOLTIME_RIM_STEP_DEG	= 5.0;							// Bearing step around a zone (§8)
	static const f64	_SOLTIME_CHECK_MIN_SECS	= 0.1;							// Shortest checkpoint interval (§10)
	static const f64	_SOLTIME_CATCHUP_SECS	= 60.0;							// Late checkpoints replayed in order up to this


//////////
// Internal:  small helpers
//////
	// Circular day index for a local day number that may run past the year's edges
	static inline s32 iSolTime_dayIndex(s64 k)
	{
		s64	m = k % _SOLTIME_YEAR_DAYS;
		return (s32)((m < 0) ? (m + _SOLTIME_YEAR_DAYS) : m);
	}

	// A clock difference as a time of day, in [-43200, +43200]:  zones lock the time of day (§8)
	static inline f64 iSolTime_wrapDay(f64 secs)
	{
		return remainder(secs, _SOLTIME_DAY_SECS);
	}

	static inline f64 iSolTime_clamp(f64 v, f64 lo, f64 hi)
	{
		return ((v < lo) ? lo : ((v > hi) ? hi : v));
	}

	// Great-circle distance (haversine:  exact down to meters)
	static f64 iSolTime_distanceKm(f64 lat1, f64 lon1, f64 lat2, f64 lon2)
	{
		f64	p1	= lat1 * _SOLTIME_DEG2RAD;
		f64	p2	= lat2 * _SOLTIME_DEG2RAD;
		f64	sp	= sin((p2 - p1) * 0.5);
		f64	sl	= sin((lon2 - lon1) * _SOLTIME_DEG2RAD * 0.5);
		f64	h	= sp * sp + cos(p1) * cos(p2) * sl * sl;
		return 2.0 * _SOLTIME_EARTH_KM * asin(sqrt(iSolTime_clamp(h, 0.0, 1.0)));
	}

	// The point km away along a great circle from (lat, lon) at bearingDeg (clockwise from north)
	static void iSolTime_destination(f64 latDeg, f64 lonDegE, f64 km, f64 bearingDeg, f64* lat2, f64* lon2)
	{
		f64	d	= km / _SOLTIME_EARTH_KM;
		f64	p1	= latDeg * _SOLTIME_DEG2RAD;
		f64	th	= bearingDeg * _SOLTIME_DEG2RAD;
		f64	sp2	= iSolTime_clamp(sin(p1) * cos(d) + cos(p1) * sin(d) * cos(th), -1.0, 1.0);
		f64	p2	= asin(sp2);
		f64	l2	= lonDegE * _SOLTIME_DEG2RAD + atan2(sin(th) * sin(d) * cos(p1), cos(d) - sin(p1) * sp2);

		*lat2 = p2 * _SOLTIME_RAD2DEG;
		*lon2 = remainder(l2 * _SOLTIME_RAD2DEG, 360.0);
	}


//////////
// Internal:  NOAA declination (radians) and equation of time (minutes) for a fractional year
// angle taken at the given day's MIDDAY (spec §2).
//////
	static void iSolTime_solar(f64 day, f64* declRad, f64* eqtimeMin, s32 days = _SOLTIME_YEAR_DAYS)
	{
		// NOAA's day_of_year - 1 + (hour - 12)/24 is the zero-based day at noon.
		f64	g = 2.0 * M_PI * floor(day) / (f64)days;

		*declRad	= 0.006918
					- 0.399912 * cos(g)       + 0.070257 * sin(g)
					- 0.006758 * cos(2.0 * g) + 0.000907 * sin(2.0 * g)
					- 0.002697 * cos(3.0 * g) + 0.001480 * sin(3.0 * g);

		*eqtimeMin	= 229.18 * (0.000075
					+ 0.001868 * cos(g)       - 0.032077 * sin(g)
					- 0.014615 * cos(2.0 * g) - 0.040849 * sin(2.0 * g));
	}


//////////
// Internal:  the year's solar parameters, one entry per day, built once (C++11 thread-safe
// static) and shared by every table, window and sun-time evaluation
//////
	struct SSolYear
	{
		f64		decl[_SOLTIME_YEAR_DAYS];			// Radians
		f64		eqtimeMin[_SOLTIME_YEAR_DAYS];		// Minutes
	};

	static SSolYear iSolTime_yearBuild(s32 days = _SOLTIME_YEAR_DAYS)
	{
		SSolYear	y;
		for (s32 i = 0; i < _SOLTIME_YEAR_DAYS; ++i)
			iSolTime_solar((f64)i, &y.decl[i], &y.eqtimeMin[i], days);
		return y;
	}

	static const SSolYear& iSolTime_year(s32 days = _SOLTIME_YEAR_DAYS)
	{
		static const SSolYear	y = iSolTime_yearBuild();
		static const SSolYear y365 = iSolTime_yearBuild(365);
		return days == 365 ? y365 : y;
	}


//////////
// Internal:  the half-day hour angle (degrees) at this latitude.  The cosine is CLAMPED to +/-1
// (spec §4):  polar night collapses to 0, polar day to 180.  *state (nullable):  0 normal,
// +1 polar day, -1 polar night.
//////
	static f64 iSolTime_hourAngle(f64 latDeg, f64 declRad, f64 zenithDeg, s32* state)
	{
		f64	lat		= latDeg * _SOLTIME_DEG2RAD;
		f64	cosHa	= cos(zenithDeg * _SOLTIME_DEG2RAD) / (cos(lat) * cos(declRad)) - tan(lat) * tan(declRad);
		s32	st		= 0;
		if (cosHa >  1.0)	{ cosHa =  1.0;  st = -1; }		// Sun never reaches the zenith:  polar night
		if (cosHa < -1.0)	{ cosHa = -1.0;  st = +1; }		// Sun never drops below it:     polar day
		if (state)	*state = st;
		return acos(cosHa) * _SOLTIME_RAD2DEG;
	}

	// The place's own raw sun on day index m, UTC seconds of that day
	static s32 iSolTime_sunTimes(f64 latDeg, f64 lonDegE, s32 m, f64 zenithDeg, f64* riseSecs, f64* setSecs, f64* noonSecs)
	{
		const SSolYear&	y	= iSolTime_year();
		s32				st;
		f64				ha	= iSolTime_hourAngle(latDeg, y.decl[m], zenithDeg, &st);

		*riseSecs	= (720.0 - 4.0 * (lonDegE + ha) - y.eqtimeMin[m]) * 60.0;
		*setSecs	= (720.0 - 4.0 * (lonDegE - ha) - y.eqtimeMin[m]) * 60.0;
		*noonSecs	= (720.0 - 4.0 *  lonDegE       - y.eqtimeMin[m]) * 60.0;
		return st;
	}


//////////
// Internal:  the basis latitude (spec §4).  latForDayLen solves, in closed form, for the latitude
// (same hemisphere as latDeg, between the equator and it) whose day is daySecs long:
//     sin(phi) sin(d) + cos(phi) cos(d) cos(H0) = cos(z)   ->   a cos(phi) + b sin(phi) = c
//     phi = atan2(b, a) +/- acos(c / sqrt(a^2 + b^2))
//////
	static f64 iSolTime_latForDayLen(f64 latDeg, f64 declRad, f64 zenithDeg, f64 daySecs)
	{
		f64	H0	= daySecs / _SOLTIME_DAY_SECS * M_PI;			// Half the day as an hour angle, radians
		f64	a	= cos(declRad) * cos(H0);
		f64	b	= sin(declRad);
		f64	c	= cos(zenithDeg * _SOLTIME_DEG2RAD);
		f64	ab	= sqrt(a * a + b * b);
		f64	psi	= atan2(b, a);
		f64	q	= acos(iSolTime_clamp(c / ab, -1.0, 1.0));
		f64	r1	= remainder((psi + q) * _SOLTIME_RAD2DEG, 360.0);	// Normalized:  near the equinox a root can
		f64	r2	= remainder((psi - q) * _SOLTIME_RAD2DEG, 360.0);	// come out as -270.6 for +89.4
		f64	lo	= fmin(0.0, latDeg);
		f64	hi	= fmax(0.0, latDeg);

		// The root between the equator and latDeg (rounding can nudge it a hair outside)
		if (r1 >= lo && r1 <= hi)		return r1;
		if (r2 >= lo && r2 <= hi)		return r2;
		f64	d1	= (r1 < lo) ? (lo - r1) : (r1 - hi);
		f64	d2	= (r2 < lo) ? (lo - r2) : (r2 - hi);
		return iSolTime_clamp((d1 <= d2) ? r1 : r2, lo, hi);
	}

	// The hour angle (degrees) at the BASIS latitude on day index m; *basisLat (nullable) gets it
	static f64 iSolTime_basisHa(SSolTime* p, f64 latDeg, s32 m, f64* basisLat, s32 days = _SOLTIME_YEAR_DAYS)
	{
		const SSolYear&	y	= iSolTime_year(days);
		f64				ha	= iSolTime_hourAngle(latDeg, y.decl[m], p->zenith_deg, NULL);
		f64				D	= 480.0 * ha;			// Day length:  one degree of hour angle is 240 s, on both sides of noon
		f64				bl	= latDeg;

		if (D < p->basis_min_secs)
		{
			// Short-day side:  the latitude where the day is exactly M long
			bl = iSolTime_latForDayLen(latDeg, y.decl[m], p->zenith_deg, p->basis_min_secs);
			ha = p->basis_min_secs / 480.0;

		} else if (_SOLTIME_DAY_SECS - D < p->basis_min_secs) {
			// Long-day side:  the latitude where the night is exactly M long
			bl = iSolTime_latForDayLen(latDeg, y.decl[m], p->zenith_deg, _SOLTIME_DAY_SECS - p->basis_min_secs);
			ha = (_SOLTIME_DAY_SECS - p->basis_min_secs) / 480.0;
		}

		if (basisLat)	*basisLat = bl;
		return ha;
	}


//////////
// Internal:  the year's offset table for this latitude at lon 0 -- raw target
// (ANCHOR - sunriseUTC at the basis latitude) eroded circularly by a cone of slope
// max_slide_secs/day (spec §3).  Longitude is a constant +240 s/degE shift and commutes with
// the erosion, so callers add it per day (iSolTime_dayOffset).
//////
	// O(n) circular erosion:  forward then backward sweeps, two laps each around the circle
	static void iSolTime_erode(f64* s, f64 slide)
	{
		for (s32 lap = 0; lap < 2; ++lap)
		{
			for (s32 i = 0; i < _SOLTIME_YEAR_DAYS; ++i)
			{
				f64	v = s[(i + _SOLTIME_YEAR_DAYS - 1) % _SOLTIME_YEAR_DAYS] + slide;
				if (v < s[i])	s[i] = v;
			}
		}
		for (s32 lap = 0; lap < 2; ++lap)
		{
			for (s32 i = _SOLTIME_YEAR_DAYS - 1; i >= 0; --i)
			{
				f64	v = s[(i + 1) % _SOLTIME_YEAR_DAYS] + slide;
				if (v < s[i])	s[i] = v;
			}
		}
	}

	// The raw target before erosion (also used by the tests)
	static void iSolTime_targetTable(SSolTime* p, f64 latDeg, f64* target)
	{
		const SSolYear&	y = iSolTime_year();
		for (s32 i = 0; i < _SOLTIME_YEAR_DAYS; ++i)
			target[i] = p->anchor_secs - (720.0 - 4.0 * iSolTime_basisHa(p, latDeg, i, NULL) - y.eqtimeMin[i]) * 60.0;
	}

	static void iSolTime_yearTable(SSolTime* p, f64 latDeg, f64* sm)
	{
		iSolTime_targetTable(p, latDeg, sm);
		iSolTime_erode(sm, p->max_slide_secs);
	}

	// Local day k's offset (the value in force through its daylight)
	static inline f64 iSolTime_dayOffset(const f64* sm, f64 lonDegE, s64 k)
	{
		return sm[iSolTime_dayIndex(k)] + _SOLTIME_SECS_PER_DEG * lonDegE;
	}


//////////
// Internal:  the night window between local day k and k + 1 (spec §9), absolute UTC seconds
// (k may run past the year's edges; the year is periodic):  the basis sunset of day k to the
// basis sunrise of day k + 1, kept inside night_start (by O(k)) .. night_end (by O(k + 1)).
//////
	static void iSolTime_window(SSolTime* p, const f64* sm, f64 latDeg, f64 lonDegE, s64 k, f64* ws, f64* we)
	{
		const SSolYear&	y		= iSolTime_year();
		s32				m0		= iSolTime_dayIndex(k);
		s32				m1		= iSolTime_dayIndex(k + 1);
		f64				ha0		= iSolTime_basisHa(p, latDeg, m0, NULL);
		f64				ha1		= iSolTime_basisHa(p, latDeg, m1, NULL);
		f64				sunset	= (f64)k       * _SOLTIME_DAY_SECS + (720.0 - 4.0 * (lonDegE - ha0) - y.eqtimeMin[m0]) * 60.0;
		f64				sunrise	= (f64)(k + 1) * _SOLTIME_DAY_SECS + (720.0 - 4.0 * (lonDegE + ha1) - y.eqtimeMin[m1]) * 60.0;
		f64				t20		= (f64)k       * _SOLTIME_DAY_SECS + p->night_start_secs - iSolTime_dayOffset(sm, lonDegE, k);
		f64				t04		= (f64)(k + 1) * _SOLTIME_DAY_SECS + p->night_end_secs   - iSolTime_dayOffset(sm, lonDegE, k + 1);

		*ws = fmax(sunset,  t20);
		*we = fmin(sunrise, t04);
		if (*we - *ws < 1.0)
		{
			// Defensive only:  the §5 window rule keeps validated parameters from ever reaching here
			*ws = t20;
			*we = t04;
		}
	}


//////////
// Internal:  the offset at UTC instant t (seconds of the periodic year, unwrapped is fine) for a
// place whose year table is sm (spec §9):  the day's value through the day, the raised-cosine
// glide from one day's value to the next inside the night window.
//////
	static f64 iSolTime_offsetAt(SSolTime* p, const f64* sm, f64 latDeg, f64 lonDegE, f64 t)
	{
		s64	kU	= (s64)floor(t / _SOLTIME_DAY_SECS);
		s64	k0	= (s64)floor((t + iSolTime_dayOffset(sm, lonDegE, kU)) / _SOLTIME_DAY_SECS);		// The local day, give or take one
		f64	ws[5], we[5];

		for (s32 j = 0; j < 5; ++j)
			iSolTime_window(p, sm, latDeg, lonDegE, k0 - 2 + j, &ws[j], &we[j]);

		// Inside a night window:  the glide
		for (s32 j = 0; j < 5; ++j)
		{
			if (t >= ws[j] && t < we[j])
			{
				s64	k	= k0 - 2 + j;
				f64	a	= iSolTime_dayOffset(sm, lonDegE, k);
				f64	b	= iSolTime_dayOffset(sm, lonDegE, k + 1);
				f64	x	= (t - ws[j]) / (we[j] - ws[j]);
				return a + (b - a) * (x - sin(2.0 * M_PI * x) / (2.0 * M_PI));
			}
		}

		// Between windows:  that day's value -- true seconds
		for (s32 j = 1; j < 5; ++j)
		{
			if (t >= we[j - 1] && t < ws[j])
				return iSolTime_dayOffset(sm, lonDegE, k0 - 2 + j);
		}
		return NAN;		// Unreachable:  the windows and the days between them tile the timeline
	}

	// The raw field at a point and instant
	static f64 iSolTime_fieldAt(SSolTime* p, f64 latDeg, f64 lonDegE, f64 t)
	{
		f64	sm[_SOLTIME_YEAR_DAYS];
		iSolTime_yearTable(p, latDeg, sm);
		return iSolTime_offsetAt(p, sm, latDeg, lonDegE, t);
	}


//////////
// Internal:  parameter resolution + validation helpers
//////
	static SSolTime* iSolTime_params(SSolTime* p, SSolTime* def)
	{
		if (p)		return p;
		soltime_defaults(def);
		return def;
	}

	static bool iSolTime_validParams(SSolTime* p)
	{
		return (isfinite(p->anchor_secs)          && p->anchor_secs          >= 0.0 && p->anchor_secs   < _SOLTIME_DAY_SECS
			 && isfinite(p->max_slide_secs)       && p->max_slide_secs       >  0.0
			 && isfinite(p->zenith_deg)           && p->zenith_deg           >  0.0 && p->zenith_deg    < 180.0
			 && isfinite(p->basis_min_secs)       && p->basis_min_secs       >  0.0 && p->basis_min_secs < _SOLTIME_DAY_SECS * 0.5
			 && isfinite(p->night_start_secs)     && isfinite(p->night_end_secs)
			 && p->night_end_secs >= 0.0          && p->night_end_secs < p->anchor_secs
			 && p->night_start_secs > p->anchor_secs && p->night_start_secs < _SOLTIME_DAY_SECS
			 && p->basis_min_secs - (p->anchor_secs - p->night_end_secs) > 2.0 * p->max_slide_secs		// §9:  the window can never get too short
			 && _SOLTIME_DAY_SECS - p->night_start_secs + p->night_end_secs > 3.0 * p->max_slide_secs
			 && isfinite(p->max_correction)       && p->max_correction       >  0.0 && p->max_correction < 1.0
			 && isfinite(p->design_speed_kmh)     && p->design_speed_kmh     >  0.0
			 && isfinite(p->ramp_min_secs_per_km) && p->ramp_min_secs_per_km >  0.0
			 && isfinite(p->check_tolerance_secs) && p->check_tolerance_secs >  0.0
			 && isfinite(p->check_max_secs)       && p->check_max_secs       >= 1.0);
	}

	static bool iSolTime_validLatLon(f64 latDeg, f64 lonDegE)
	{
		return (isfinite(latDeg)  && latDeg  >=  -90.0 && latDeg  <=  90.0
			 && isfinite(lonDegE) && lonDegE >= -180.0 && lonDegE <= 180.0);
	}

	static bool iSolTime_validDay(f64 day)
	{
		return (isfinite(day) && day >= 0.0 && day < (f64)_SOLTIME_YEAR_DAYS);
	}

	#include "soltime_calendar.inc"


//////////
// API -- the field
//////
	void soltime_defaults(SSolTime* p)
	{
		if (!p)		return;
		p->anchor_secs			= 24300.0;		// 06:45 -- light before the bus
		p->max_slide_secs		= 240.0;		// 4 min/day cap (binds only at high latitude)
		p->zenith_deg			= 90.833;		// Official sunrise, incl. refraction
		p->basis_min_secs		= 14400.0;		// 4 h of day AND night at the basis latitude
		p->night_start_secs		= 72000.0;		// 20:00
		p->night_end_secs		= 14400.0;		// 04:00
		p->max_correction		= 0.5;			// The display runs 0.5x .. 1.5x at most
		p->design_speed_kmh		= 128.75;		// 80 mph
		p->ramp_min_secs_per_km	= 2.0;			// Ramp slope floor (far north)
		p->check_tolerance_secs	= 0.05;			// Auto-flex:  the halfway check's tolerance
		p->check_max_secs		= 300.0;		// Auto-flex:  5 minutes standing still
	}

	f64 soltime_offsetSecs(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day)
	{
		SSolTime	def;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || !iSolTime_validLatLon(latDeg, lonDegE) || !iSolTime_validDay(day))
			return NAN;
		return iSolTime_fieldAt(p, latDeg, lonDegE, day * _SOLTIME_DAY_SECS);
	}

	f64 soltime_localFromUtc(SSolTime* p, f64 latDeg, f64 lonDegE, f64 utcSecsOfYear)
	{
		if (!isfinite(utcSecsOfYear) || utcSecsOfYear < 0.0 || utcSecsOfYear >= _SOLTIME_YEAR_SECS)
			return NAN;

		// The result is a RAW continuous local seconds-of-year:  near the date line it can fall
		// outside [0, yearSecs) -- a different local date.  The caller owns the wrap.
		return utcSecsOfYear + soltime_offsetSecs(p, latDeg, lonDegE, utcSecsOfYear / _SOLTIME_DAY_SECS);
	}

	s32 soltime_sunTimesUtc(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day, f64* riseSecs, f64* setSecs, f64* noonSecs)
	{
		SSolTime	def;
		f64			rise, set, noon;
		if (!riseSecs)	riseSecs = &rise;
		if (!setSecs)	setSecs  = &set;
		if (!noonSecs)	noonSecs = &noon;

		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || !iSolTime_validLatLon(latDeg, lonDegE) || !iSolTime_validDay(day))
		{
			*riseSecs = *setSecs = *noonSecs = NAN;
			return -2;
		}
		return iSolTime_sunTimes(latDeg, lonDegE, (s32)floor(day), p->zenith_deg, riseSecs, setSecs, noonSecs);
	}

	f64 soltime_basisLatDeg(SSolTime* p, f64 latDeg, f64 day)
	{
		SSolTime	def;
		f64			bl;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || !iSolTime_validLatLon(latDeg, 0.0) || !iSolTime_validDay(day))
			return NAN;
		iSolTime_basisHa(p, latDeg, (s32)floor(day), &bl);
		return bl;
	}

	s32 soltime_nightWindowUtc(SSolTime* p, f64 latDeg, f64 lonDegE, f64 day, f64* startUtc, f64* endUtc)
	{
		SSolTime	def;
		f64			sm[_SOLTIME_YEAR_DAYS], ws, we;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || !iSolTime_validLatLon(latDeg, lonDegE) || !iSolTime_validDay(day))
		{
			if (startUtc)	*startUtc = NAN;
			if (endUtc)		*endUtc   = NAN;
			return -2;
		}
		iSolTime_yearTable(p, latDeg, sm);
		iSolTime_window(p, sm, latDeg, lonDegE, (s64)floor(day), &ws, &we);
		if (startUtc)	*startUtc = ws;
		if (endUtc)		*endUtc   = we;
		return 0;
	}

	f64 soltime_distanceKm(f64 lat1, f64 lon1, f64 lat2, f64 lon2)
	{
		if (!isfinite(lat1) || !isfinite(lon1) || !isfinite(lat2) || !isfinite(lon2))
			return NAN;
		return iSolTime_distanceKm(lat1, lon1, lat2, lon2);
	}


//////////
// Internal:  the honest radius (spec §8).  circleError is the largest gap, over every bearing
// and every day, between a clock locked to the center (table smc) and the field on the circle
// of radius km -- daily values, as a time of day.  Bearings b and 360 - b share a latitude and
// mirror the longitude, so one table serves both.  sm2 is scratch.
//////
	// Golden-section search for the largest f(bearing) on [a, b] -- the refinement between sampled
	// bearings, so a guarantee measured on a grid also holds between its points
	static f64 iSolTime_goldenMax(f64 a, f64 b, f64 (*f)(void* ctx, f64 brg), void* ctx)
	{
		const f64	gr	= 0.6180339887498949;
		f64			x1	= b - gr * (b - a), x2 = a + gr * (b - a);
		f64			f1	= f(ctx, x1),       f2 = f(ctx, x2);
		f64			best = fmax(f1, f2);
		for (s32 it = 0; it < 24; ++it)
		{
			if (f1 >= f2)	{ b = x2;  x2 = x1;  f2 = f1;  x1 = b - gr * (b - a);  f1 = f(ctx, x1);  best = fmax(best, f1); }
			else			{ a = x1;  x1 = x2;  f1 = f2;  x2 = a + gr * (b - a);  f2 = f(ctx, x2);  best = fmax(best, f2); }
		}
		return best;
	}

	struct SSolCircleCtx
	{
		SSolTime*		p;
		f64				latDeg;
		const f64*		smc;
		f64				km;
		f64*			sm2;
	};

	// The gap on one bearing (lon 0 center), worst day
	static f64 iSolTime_circleErrorAt(void* ctx, f64 brg)
	{
		SSolCircleCtx*	cx = (SSolCircleCtx*)ctx;
		f64				lat2, lon2, worst = 0.0;
		iSolTime_destination(cx->latDeg, 0.0, cx->km, brg, &lat2, &lon2);
		iSolTime_yearTable(cx->p, lat2, cx->sm2);
		for (s32 k = 0; k < _SOLTIME_YEAR_DAYS; ++k)
			worst = fmax(worst, fabs(iSolTime_wrapDay(cx->smc[k] - cx->sm2[k] - _SOLTIME_SECS_PER_DEG * lon2)));
		return worst;
	}

	static f64 iSolTime_circleError(SSolTime* p, f64 latDeg, const f64* smc, f64 km, f64* sm2)
	{
		f64	worst	= 0.0, bestBrg = 0.0;
		s32	steps	= (s32)(180.0 / _SOLTIME_RIM_STEP_DEG);

		for (s32 a = 0; a <= steps; ++a)
		{
			f64	lat2, lon2, e1 = 0.0, e2 = 0.0;
			iSolTime_destination(latDeg, 0.0, km, (f64)a * _SOLTIME_RIM_STEP_DEG, &lat2, &lon2);
			iSolTime_yearTable(p, lat2, sm2);
			for (s32 k = 0; k < _SOLTIME_YEAR_DAYS; ++k)
			{
				f64	d = smc[k] - sm2[k];
				e1 = fmax(e1, fabs(iSolTime_wrapDay(d - _SOLTIME_SECS_PER_DEG * lon2)));		// Bearing a
				e2 = fmax(e2, fabs(iSolTime_wrapDay(d + _SOLTIME_SECS_PER_DEG * lon2)));		// Its mirror, 360 - a
			}
			if (e1 > worst)		{ worst = e1;  bestBrg = (f64)a * _SOLTIME_RIM_STEP_DEG; }
			if (e2 > worst)		{ worst = e2;  bestBrg = 360.0 - (f64)a * _SOLTIME_RIM_STEP_DEG; }
		}

		// Refine between the samples around the worst bearing
		SSolCircleCtx	cx = { p, latDeg, smc, km, sm2 };
		return fmax(worst, iSolTime_goldenMax(bestBrg - _SOLTIME_RIM_STEP_DEG, bestBrg + _SOLTIME_RIM_STEP_DEG, &iSolTime_circleErrorAt, &cx));
	}

	static f64 iSolTime_honestRadius(SSolTime* p, f64 latDeg, f64 quantumSecs)
	{
		f64	smc[_SOLTIME_YEAR_DAYS], sm2[_SOLTIME_YEAR_DAYS];
		f64	half	= quantumSecs * 0.5;
		f64	maxKm	= M_PI * _SOLTIME_EARTH_KM;			// Half the circumference

		iSolTime_yearTable(p, latDeg, smc);

		// Bracket:  start from the east-west answer, grow until the circle exceeds the bar
		f64	lo	= 0.0, flo = -half;
		f64	hi	= fmax(1.0e-3, (half / _SOLTIME_SECS_PER_DEG) * _SOLTIME_KM_PER_DEG * cos(latDeg * _SOLTIME_DEG2RAD) * 1.05);
		f64	fhi	= iSolTime_circleError(p, latDeg, smc, hi, sm2) - half;
		while (fhi <= 0.0)
		{
			lo	= hi;
			flo	= fhi;
			if (hi >= maxKm)	return maxKm;
			hi	= fmin(maxKm, hi * 1.5);
			fhi	= iSolTime_circleError(p, latDeg, smc, hi, sm2) - half;
		}

		// Illinois regula falsi to 0.1 m; keep the side that meets the bar
		s32	side = 0;
		for (s32 it = 0; it < 100 && hi - lo > 1.0e-4; ++it)
		{
			f64	r = (lo * fhi - hi * flo) / (fhi - flo);
			if (!(r > lo && r < hi))	r = 0.5 * (lo + hi);
			f64	fr = iSolTime_circleError(p, latDeg, smc, r, sm2) - half;
			if (fr <= 0.0)
			{
				lo = r;  flo = fr;
				if (side == -1)		fhi *= 0.5;
				side = -1;

			} else {
				hi = r;  fhi = fr;
				if (side == +1)		flo *= 0.5;
				side = +1;
			}
		}
		return lo;
	}


//////////
// Internal:  the registry (spec §8).  Zones whose discs overlap merge, transitively, into one
// group:  the union of their discs, on the clock of the group's most populous zone (its leader).
// Groups apply in layer order -- the most populous leader first -- each capping its lead over the
// clock beneath it (the field plus every group before it).
//////
	struct SSolRegistry
	{
		SSolTime		p;
		s32				n;
		SSolZone*		zones;			// As given
		f64*			lockKm;			// Per zone:  its disc's radius (resolved)
		s32*			leader;			// Per zone:  its group's leader
		s32*			layerOf;		// Per zone:  its group's layer position
		s32				nGroups;
		s32*			order;			// Per layer position:  the group's leader
		s32*			memberStart;	// Per layer position:  its first entry in members[]  (nGroups + 1 entries)
		s32*			members;		// Zone indices, grouped by layer position
		f64*			rampKm;			// Per leader:  the group's ramp (0 = hard edge)
		f64*			jump;			// Per leader:  J, the group's largest lead over the clock beneath
		f64*			steep;			// Per leader:  the steepest slope inside its ramp (NAN until needed)
		f64**			tables;			// Per leader:  the year table at its center (the group's clock); NULL otherwise
		s32*			latOrder;		// Zone indices by latitude (the neighbor search)
		f64				maxLock;		// Largest disc radius
		f64				maxReach;		// Largest disc radius + its group's ramp, over the groups sized so far
		f64*			gradCache;		// The field's steepest slope per 0.1 degree of latitude (build only; NAN = not yet)
	};

	static void iSolTime_registryFree(SSolRegistry* r)
	{
		if (!r)		return;
		if (r->tables)
		{
			for (s32 i = 0; i < r->n; ++i)
				delete[] r->tables[i];
		}
		delete[] r->tables;
		delete[] r->zones;
		delete[] r->lockKm;
		delete[] r->leader;
		delete[] r->layerOf;
		delete[] r->order;
		delete[] r->memberStart;
		delete[] r->members;
		delete[] r->rampKm;
		delete[] r->jump;
		delete[] r->steep;
		delete[] r->latOrder;
		delete[] r->gradCache;
		delete r;
	}

	// The initial great-circle bearing from point 1 to point 2, degrees clockwise from north
	static f64 iSolTime_bearing(f64 lat1, f64 lon1, f64 lat2, f64 lon2)
	{
		f64	p1	= lat1 * _SOLTIME_DEG2RAD;
		f64	p2	= lat2 * _SOLTIME_DEG2RAD;
		f64	dl	= (lon2 - lon1) * _SOLTIME_DEG2RAD;
		return atan2(sin(dl) * cos(p2), cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl)) * _SOLTIME_RAD2DEG;
	}

	// Zones whose centers lie within reachKm of a latitude (a window over latOrder; the caller
	// still checks the real distance).  A new[] array of zone indices -- the caller deletes it.
	static s32* iSolTime_near(SSolRegistry* r, f64 latDeg, f64 reachKm, s32* count)
	{
		f64	dLat	= reachKm / _SOLTIME_KM_PER_DEG + 1.0e-9;
		s32	lo		= 0, hi = r->n;
		while (lo < hi)
		{
			s32	mid = (lo + hi) / 2;
			if (r->zones[r->latOrder[mid]].latDeg < latDeg - dLat)		lo = mid + 1;
			else														hi = mid;
		}
		s32	end = lo;
		while (end < r->n && r->zones[r->latOrder[end]].latDeg <= latDeg + dLat)
			++end;

		*count = end - lo;
		s32*	out = new s32[(*count > 0) ? *count : 1];
		for (s32 j = lo; j < end; ++j)
			out[j - lo] = r->latOrder[j];
		return out;
	}


//////////
// Internal:  evaluating the layered clock.  A point is PREPARED once against the first nLayers
// groups -- its field table, and for every group touching it, the touch:  in the lock (inside any
// member's disc), or in the ramp of one or more members, each with its fraction across the ramp
// and the rim point on the same bearing prepared beneath that group -- and then evaluated for any
// day (daily values) or instant.
//
// In a ramp the cap on the lead falls linearly from max(J, |the lead at that member's rim on this
// bearing, right now|) to 0, and a group's cap is the largest over its members:  at every member's
// rim the cap is never below the lead there, so the ramp meets the lock exactly, at every instant
// and around the whole outline of a merged zone (concave notches between discs included); the
// largest of the members' caps is no steeper than the steepest of them, so the drive budget holds.
//////
	struct SSolPrep;

	struct SSolPrepStep
	{
		s32				leader;					// The group
		s32				nMembers;				// 0 = in the lock
		s32*			member;					// Ramp:  the members whose ramps cover the point
		f64*			frac;					// Their fractions across the ramp (0 at the rim)
		SSolPrep**		rim;					// Their rim points, prepared beneath this group
	};

	struct SSolPrep
	{
		f64				lat, lon;
		f64				sm[_SOLTIME_YEAR_DAYS];	// The point's field table
		s32				nSteps;
		SSolPrepStep*	steps;					// In layer order
	};

	struct SSolEval
	{
		bool			daily;					// true:  day k's values;  false:  instant t
		s64				k;
		f64				t;
		bool            calendar;
	};

	struct SSolHit
	{
		s32				layer;
		s32				zone;
		f64				frac;					// < 0 = in the lock
	};

	static SSolPrep* iSolTime_prepare(SSolRegistry* r, s32 nLayers, f64 latDeg, f64 lonDegE, const f64* smKnown)
	{
		SSolPrep*	pp	= new SSolPrep;
		pp->lat			= latDeg;
		pp->lon			= lonDegE;
		pp->nSteps		= 0;
		pp->steps		= NULL;
		if (smKnown)	{ for (s32 k = 0; k < _SOLTIME_YEAR_DAYS; ++k)	pp->sm[k] = smKnown[k]; }
		else			iSolTime_yearTable(&r->p, latDeg, pp->sm);
		if (nLayers <= 0 || r->n <= 0)
			return pp;

		// The touches, from the zones near enough
		s32		nc;
		s32*	cand	= iSolTime_near(r, latDeg, r->maxReach, &nc);
		SSolHit*	hit	= new SSolHit[(nc > 0) ? nc : 1];
		s32		nh		= 0;
		for (s32 j = 0; j < nc; ++j)
		{
			s32	e = cand[j];
			if (r->layerOf[e] >= nLayers)	continue;
			f64	ramp	= r->rampKm[r->leader[e]];
			f64	d		= iSolTime_distanceKm(latDeg, lonDegE, r->zones[e].latDeg, r->zones[e].lonDegE);
			if (d <= r->lockKm[e])							{ hit[nh].layer = r->layerOf[e];  hit[nh].zone = e;  hit[nh].frac = -1.0;  ++nh; }
			else if (ramp > 0.0 && d < r->lockKm[e] + ramp)	{ hit[nh].layer = r->layerOf[e];  hit[nh].zone = e;  hit[nh].frac = (d - r->lockKm[e]) / ramp;  ++nh; }
		}
		delete[] cand;
		std::sort(hit, hit + nh, [](const SSolHit& a, const SSolHit& b) { return (a.layer != b.layer) ? (a.layer < b.layer) : (a.zone < b.zone); });

		// One step per group touched:  the lock if any member's disc holds the point, else the ramp
		s32	groups = 0;
		for (s32 j = 0; j < nh; ++j)
			if (j == 0 || hit[j].layer != hit[j - 1].layer)		++groups;
		pp->steps = new SSolPrepStep[(groups > 0) ? groups : 1];
		for (s32 j = 0; j < nh; )
		{
			s32		end		= j;
			bool	locked	= false;
			while (end < nh && hit[end].layer == hit[j].layer)
			{
				if (hit[end].frac < 0.0)	locked = true;
				++end;
			}

			SSolPrepStep*	st	= &pp->steps[pp->nSteps++];
			st->leader			= r->order[hit[j].layer];
			st->nMembers		= 0;
			st->member			= NULL;
			st->frac			= NULL;
			st->rim				= NULL;
			if (!locked)
			{
				st->nMembers	= end - j;
				st->member		= new s32[st->nMembers];
				st->frac		= new f64[st->nMembers];
				st->rim			= new SSolPrep*[st->nMembers];
				for (s32 m = 0; m < st->nMembers; ++m)
				{
					const SSolZone*	z = &r->zones[hit[j + m].zone];
					f64				rLat, rLon;
					iSolTime_destination(z->latDeg, z->lonDegE, r->lockKm[hit[j + m].zone], iSolTime_bearing(z->latDeg, z->lonDegE, latDeg, lonDegE), &rLat, &rLon);
					st->member[m]	= hit[j + m].zone;
					st->frac[m]		= hit[j + m].frac;
					st->rim[m]		= iSolTime_prepare(r, hit[j].layer, rLat, rLon, NULL);
				}
			}
			j = end;
		}
		delete[] hit;
		return pp;
	}

	static void iSolTime_prepFree(SSolPrep* pp)
	{
		if (!pp)	return;
		for (s32 s = 0; s < pp->nSteps; ++s)
		{
			SSolPrepStep* st = &pp->steps[s];
			for (s32 m = 0; m < st->nMembers; ++m)
				iSolTime_prepFree(st->rim[m]);
			delete[] st->member;
			delete[] st->frac;
			delete[] st->rim;
		}
		delete[] pp->steps;
		delete pp;
	}

	// A group's own clock (its leader's center) for this evaluation
	static f64 iSolTime_groupValue(SSolRegistry* r, s32 L, SSolEval* ev)
	{
		const SSolZone* z = &r->zones[L];
		if (ev->calendar) return iSolTime_calendarFieldAt(&r->p, z->latDeg, z->lonDegE, ev->t);
		return ev->daily ? iSolTime_dayOffset(r->tables[L], z->lonDegE, ev->k)
						 : iSolTime_offsetAt(&r->p, r->tables[L], z->latDeg, z->lonDegE, ev->t);
	}

	// The layered clock at a prepared point
	static f64 iSolTime_prepValue(SSolRegistry* r, SSolPrep* pp, SSolEval* ev)
	{
		f64	clock = ev->calendar ? iSolTime_calendarFieldAt(&r->p, pp->lat, pp->lon, ev->t)
		                         : ev->daily ? iSolTime_dayOffset(pp->sm, pp->lon, ev->k)
							  : iSolTime_offsetAt(&r->p, pp->sm, pp->lat, pp->lon, ev->t);
		for (s32 s = 0; s < pp->nSteps; ++s)
		{
			SSolPrepStep*	st		= &pp->steps[s];
			f64				Oe		= iSolTime_groupValue(r, st->leader, ev);
			f64				lead	= iSolTime_wrapDay(Oe - clock);			// Zones lock the time of day (§8)
			if (st->nMembers == 0)
			{
				clock += lead;												// In the lock:  exactly the group's clock
				continue;
			}
			f64	cap = 0.0;
			for (s32 m = 0; m < st->nMembers; ++m)
			{
				f64	rimLead = fabs(iSolTime_wrapDay(Oe - iSolTime_prepValue(r, st->rim[m], ev)));
				cap = fmax(cap, fmax(r->jump[st->leader], rimLead) * (1.0 - st->frac[m]));
			}
			clock += iSolTime_clamp(lead, -cap, cap);
		}
		return clock;
	}

	// The layered clock at a point and instant (sm:  the point's field table, nullable)
	static f64 iSolTime_layeredAt(SSolRegistry* r, const f64* sm, f64 latDeg, f64 lonDegE, f64 t, bool calendar = false)
	{
		SSolEval	ev		= { false, 0, t, calendar };
		SSolPrep*	pp		= iSolTime_prepare(r, r->nGroups, latDeg, lonDegE, sm);
		f64			clock	= iSolTime_prepValue(r, pp, &ev);
		iSolTime_prepFree(pp);
		return clock;
	}


//////////
// Internal:  J for the group at layer position pos -- its largest lead over the clock beneath
// anywhere in its discs, all year.  Every member's rim, every 5 degrees, refined between samples
// around the worst bearing; rings inside a disc too where an earlier group reaches into it (with
// only the field beneath, the lead is largest at the rim).
//////
	struct SSolJumpCtx
	{
		SSolRegistry*	r;
		s32				pos;
		s32				member;
		f64				km;
	};

	static f64 iSolTime_leadAt(void* ctx, f64 brg)
	{
		SSolJumpCtx*		cx	= (SSolJumpCtx*)ctx;
		SSolRegistry*		r	= cx->r;
		s32					L	= r->order[cx->pos];
		const SSolZone*		m	= &r->zones[cx->member];
		f64					lat2, lon2, worst = 0.0;

		iSolTime_destination(m->latDeg, m->lonDegE, cx->km, brg, &lat2, &lon2);
		SSolPrep*	pp = iSolTime_prepare(r, cx->pos, lat2, lon2, NULL);
		SSolEval	ev = { true, 0, 0.0 };
		for (ev.k = 0; ev.k < _SOLTIME_YEAR_DAYS; ++ev.k)
			worst = fmax(worst, fabs(iSolTime_wrapDay(iSolTime_dayOffset(r->tables[L], r->zones[L].lonDegE, ev.k) - iSolTime_prepValue(r, pp, &ev))));
		iSolTime_prepFree(pp);
		return worst;
	}

	// Does any earlier group's zone (disc + ramp) reach into member m's disc?
	static bool iSolTime_reachedBy(SSolRegistry* r, s32 pos, s32 m)
	{
		s32		nc;
		s32*	cand	= iSolTime_near(r, r->zones[m].latDeg, r->lockKm[m] + r->maxReach, &nc);
		bool	hit		= false;
		for (s32 j = 0; j < nc && !hit; ++j)
		{
			s32	e = cand[j];
			if (r->layerOf[e] >= pos)	continue;
			f64	d = iSolTime_distanceKm(r->zones[m].latDeg, r->zones[m].lonDegE, r->zones[e].latDeg, r->zones[e].lonDegE);
			hit = (d < r->lockKm[m] + r->lockKm[e] + r->rampKm[r->leader[e]]);
		}
		delete[] cand;
		return hit;
	}

	static f64 iSolTime_jump(SSolRegistry* r, s32 pos)
	{
		static const f64	rings[] = { 1.0, 0.75, 0.5, 0.25, 0.0 };
		s32					steps	= (s32)(360.0 / _SOLTIME_RIM_STEP_DEG);
		f64					J		= 0.0;
		SSolJumpCtx			best	= { r, pos, -1, 0.0 };
		f64					bestBrg	= 0.0;

		for (s32 j = r->memberStart[pos]; j < r->memberStart[pos + 1]; ++j)
		{
			s32			m		= r->members[j];
			s32			nRings	= iSolTime_reachedBy(r, pos, m) ? (s32)(sizeof(rings) / sizeof(rings[0])) : 1;
			SSolJumpCtx	cx		= { r, pos, m, 0.0 };
			for (s32 g = 0; g < nRings; ++g)
			{
				cx.km = r->lockKm[m] * rings[g];
				for (s32 a = 0; a < ((rings[g] > 0.0) ? steps : 1); ++a)
				{
					f64	v = iSolTime_leadAt(&cx, (f64)a * _SOLTIME_RIM_STEP_DEG);
					if (v > J)	{ J = v;  best = cx;  bestBrg = (f64)a * _SOLTIME_RIM_STEP_DEG; }
				}
			}
		}
		if (best.member < 0)	return J;
		return fmax(J, iSolTime_goldenMax(bestBrg - _SOLTIME_RIM_STEP_DEG, bestBrg + _SOLTIME_RIM_STEP_DEG, &iSolTime_leadAt, &best));
	}


//////////
// Internal:  ramp sizing (the drive budget, spec §8)
//////
	// The field's steepest slope (s/km), any direction, any day, at grid latitude k (-90 + 0.1 k)
	static f64 iSolTime_gradAt(SSolRegistry* r, s32 k)
	{
		if (!isnan(r->gradCache[k]))	return r->gradCache[k];

		const f64	h	= 0.01;
		f64			lat	= -90.0 + 0.1 * (f64)k;
		f64			smA[_SOLTIME_YEAR_DAYS], smB[_SOLTIME_YEAR_DAYS];
		f64			gE	= _SOLTIME_SECS_PER_DEG / (_SOLTIME_KM_PER_DEG * cos(lat * _SOLTIME_DEG2RAD));
		f64			g	= 0.0;
		iSolTime_yearTable(&r->p, lat,     smA);
		iSolTime_yearTable(&r->p, lat + h, smB);
		for (s32 d = 0; d < _SOLTIME_YEAR_DAYS; ++d)
		{
			f64	gN = (smB[d] - smA[d]) / (h * _SOLTIME_KM_PER_DEG);
			g = fmax(g, sqrt(gE * gE + gN * gN));
		}
		r->gradCache[k] = g;
		return g;
	}

	// The steepest slope of the raw field over a band of latitudes (on the 0.1 degree grid,
	// widened outward to whole grid steps)
	static f64 iSolTime_fieldSteepest(SSolRegistry* r, f64 lat0, f64 lat1)
	{
		s32	k0	= (s32)floor((lat0 + 90.0) / 0.1);
		s32	k1	= (s32)ceil ((lat1 + 90.0) / 0.1);
		if (k0 < 1)		k0 = 1;
		if (k1 > 1799)	k1 = 1799;
		f64	g = 0.0;
		for (s32 k = k0; k <= k1; ++k)
			g = fmax(g, iSolTime_gradAt(r, k));
		return g;
	}

	static f64 iSolTime_steepOf(SSolRegistry* r, s32 L);

	// The steepest slope beneath the group at layer position pos, over its members' rings
	// [R, R + ramp]:  the field's, or an earlier group's ramp reaching in, whichever is steeper
	static f64 iSolTime_beneathSteepest(SSolRegistry* r, s32 pos, f64 ramp)
	{
		f64	lat0 = 90.0, lat1 = -90.0, g = 0.0;
		for (s32 j = r->memberStart[pos]; j < r->memberStart[pos + 1]; ++j)
		{
			s32	m		= r->members[j];
			f64	span	= (r->lockKm[m] + ramp) / _SOLTIME_KM_PER_DEG;
			lat0 = fmin(lat0, r->zones[m].latDeg - span);
			lat1 = fmax(lat1, r->zones[m].latDeg + span);

			s32		nc;
			s32*	cand = iSolTime_near(r, r->zones[m].latDeg, r->lockKm[m] + ramp + r->maxReach, &nc);
			for (s32 c = 0; c < nc; ++c)
			{
				s32	e = cand[c];
				if (r->layerOf[e] >= pos)	continue;
				s32	Le	= r->leader[e];
				f64	d	= iSolTime_distanceKm(r->zones[m].latDeg, r->zones[m].lonDegE, r->zones[e].latDeg, r->zones[e].lonDegE);
				if (r->rampKm[Le] > 0.0 && d < r->lockKm[m] + ramp + r->lockKm[e] + r->rampKm[Le])
					g = fmax(g, iSolTime_steepOf(r, Le));
			}
			delete[] cand;
		}
		return fmax(g, iSolTime_fieldSteepest(r, lat0, lat1));
	}

	// An earlier group's steepest ramp slope, worked out when first needed (a given ramp skips it)
	static f64 iSolTime_steepOf(SSolRegistry* r, s32 L)
	{
		if (isnan(r->steep[L]))
			r->steep[L] = (r->rampKm[L] > 0.0) ? (iSolTime_beneathSteepest(r, r->layerOf[L], r->rampKm[L]) + r->jump[L] / r->rampKm[L]) : 0.0;
		return r->steep[L];
	}

	// The worst night-stretch rate at a place (§9's peak):  2 * |dO| / window, any day
	static f64 iSolTime_nightRate(SSolTime* p, const f64* sm, f64 latDeg, f64 lonDegE)
	{
		f64	worst = 0.0;
		for (s64 k = 0; k < _SOLTIME_YEAR_DAYS; ++k)
		{
			f64	ws, we;
			iSolTime_window(p, sm, latDeg, lonDegE, k, &ws, &we);
			worst = fmax(worst, 2.0 * fabs(iSolTime_dayOffset(sm, lonDegE, k + 1) - iSolTime_dayOffset(sm, lonDegE, k)) / (we - ws));
		}
		return worst;
	}

	// The shortest gap from the group at pos to any earlier group's disc (a ramp stops there)
	static f64 iSolTime_gapToEarlier(SSolRegistry* r, s32 pos, f64 ramp)
	{
		f64	gap = ramp;
		for (s32 j = r->memberStart[pos]; j < r->memberStart[pos + 1]; ++j)
		{
			s32		m = r->members[j];
			s32		nc;
			s32*	cand = iSolTime_near(r, r->zones[m].latDeg, r->lockKm[m] + ramp + r->maxLock, &nc);
			for (s32 c = 0; c < nc; ++c)
			{
				s32	e = cand[c];
				if (r->layerOf[e] >= pos)	continue;
				f64	g = iSolTime_distanceKm(r->zones[m].latDeg, r->zones[m].lonDegE, r->zones[e].latDeg, r->zones[e].lonDegE) - r->lockKm[m] - r->lockKm[e];
				gap = fmin(gap, fmax(g, 0.0));
			}
			delete[] cand;
		}
		return gap;
	}

	// The ramp of the group at layer position pos
	static void iSolTime_sizeRamp(SSolRegistry* r, s32 pos)
	{
		s32					L	= r->order[pos];
		const SSolZone*		z	= &r->zones[L];
		f64					J	= r->jump[L];

		if (z->rampKm == 0.0 || J <= 0.0)	{ r->rampKm[L] = 0.0;			r->steep[L] = 0.0;	return; }		// Hard edge, or nothing to absorb
		// BUG_FIX_PLAN_2026-10-05.md FIX-03 — fixed 2026-10-05 16:43:53 EDT (UTC-04:00).
		// Supplied widths must respect the same neighboring locks as automatic ramps.
		if (z->rampKm > 0.0) { r->rampKm[L] = iSolTime_gapToEarlier(r, pos, z->rampKm); r->steep[L] = NAN; return; }

		// Auto:  from the shortest ramp the budget could allow, grow the ring until the slope it
		// implies fits inside the ring it was measured on -- the smallest consistent ramp
		f64	budget	= (r->p.max_correction - iSolTime_nightRate(&r->p, r->tables[L], z->latDeg, z->lonDegE)) / (r->p.design_speed_kmh / 3600.0);
		f64	span	= J / fmax(budget, r->p.ramp_min_secs_per_km);
		for (s32 it = 0; it < 16; ++it)
		{
			f64	gb		= iSolTime_beneathSteepest(r, pos, span);
			f64	slope	= fmax(budget - gb, r->p.ramp_min_secs_per_km);
			f64	ramp	= J / slope;
			if (ramp <= span || it == 15)
			{
				ramp = (ramp <= span) ? ramp : (J / r->p.ramp_min_secs_per_km);
				ramp = iSolTime_gapToEarlier(r, pos, ramp);			// Never into an earlier group's lock
				r->rampKm[L]	= ramp;
				r->steep[L]		= (ramp > 0.0) ? (gb + J / ramp) : gb;
				return;
			}
			span = ramp;
		}
	}


//////////
// API -- zones (§8)
//////
	f64 soltime_zoneRadiusKm(SSolTime* p, f64 quantumSecs, f64 latDeg)
	{
		SSolTime	def;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || !isfinite(quantumSecs) || quantumSecs <= 0.0 || !iSolTime_validLatLon(latDeg, 0.0))
			return NAN;
		return iSolTime_honestRadius(p, latDeg, quantumSecs);
	}

	f64 soltime_zoneQuantumForRadius(SSolTime* p, f64 radiusKm, f64 latDeg)
	{
		SSolTime	def;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || !isfinite(radiusKm) || radiusKm <= 0.0 || !isfinite(latDeg) || fabs(latDeg) > 89.5)
			return NAN;

		static const f64	std_[] = { 60.0, 300.0, 600.0, 1200.0 };
		for (u32 j = 0; j < sizeof(std_) / sizeof(std_[0]); ++j)
		{
			if (iSolTime_honestRadius(p, latDeg, std_[j]) >= radiusKm)
				return std_[j];
		}

		// Beyond the classes:  whole minutes.  Find a covering upper bound, then bisect on minutes.
		s64	lo = 20, hi = 40;			// Minutes:  20 does not cover
		while (iSolTime_honestRadius(p, latDeg, (f64)hi * 60.0) < radiusKm)
		{
			lo = hi;
			hi *= 2;
			if (hi > 1440)		return NAN;			// A whole day of time does not cover it
		}
		while (hi - lo > 1)
		{
			s64	mid = (lo + hi) / 2;
			if (iSolTime_honestRadius(p, latDeg, (f64)mid * 60.0) >= radiusKm)		hi = mid;
			else																	lo = mid;
		}
		return (f64)hi * 60.0;
	}

	static s32 iSolTime_find(s32* parent, s32 x)
	{
		while (parent[x] != x)
		{
			parent[x] = parent[parent[x]];
			x = parent[x];
		}
		return x;
	}

	SSolRegistry* soltime_registryCreate(SSolTime* p, const SSolZone* zones, s32 nZones)
	{
		SSolTime	def;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p) || nZones < 0 || (nZones > 0 && !zones))
			return NULL;
		for (s32 i = 0; i < nZones; ++i)
		{
			const SSolZone* z = &zones[i];
			if (!iSolTime_validLatLon(z->latDeg, z->lonDegE)
			 || !isfinite(z->radiusKm)    || (z->radiusKm <= 0.0 && (!isfinite(z->quantumSecs) || z->quantumSecs <= 0.0))
			 || !isfinite(z->rampKm)      || !isfinite(z->population) || z->population < 0.0 || !isfinite(z->jumpSecs))
				return NULL;
		}

		s32				n	= nZones;
		s32				na	= (n > 0) ? n : 1;
		SSolRegistry*	r	= new SSolRegistry;
		r->p			= *p;
		r->n			= n;
		r->zones		= new SSolZone[na];
		r->lockKm		= new f64[na];
		r->leader		= new s32[na];
		r->layerOf		= new s32[na];
		r->order		= new s32[na];
		r->memberStart	= new s32[na + 1];
		r->members		= new s32[na];
		r->rampKm		= new f64[na];
		r->jump			= new f64[na];
		r->steep		= new f64[na];
		r->tables		= new f64*[na];
		r->latOrder		= new s32[na];
		r->gradCache	= new f64[1801];
		r->nGroups		= 0;
		r->maxLock		= 0.0;
		r->maxReach		= 0.0;
		for (s32 k = 0; k < 1801; ++k)		r->gradCache[k] = NAN;

		// Discs (a given radius, or the honest radius of the class)
		for (s32 i = 0; i < n; ++i)
		{
			r->zones[i]		= zones[i];
			r->lockKm[i]	= (zones[i].radiusKm > 0.0) ? zones[i].radiusKm : iSolTime_honestRadius(&r->p, zones[i].latDeg, zones[i].quantumSecs);
			r->latOrder[i]	= i;
			r->rampKm[i]	= 0.0;
			r->jump[i]		= 0.0;
			r->steep[i]		= NAN;
			r->tables[i]	= NULL;
			r->maxLock		= fmax(r->maxLock, r->lockKm[i]);
		}
		std::stable_sort(r->latOrder, r->latOrder + n, [r](s32 a, s32 b) { return r->zones[a].latDeg < r->zones[b].latDeg; });

		// Merge:  discs that overlap join one group (transitively)
		s32* parent = new s32[na];
		for (s32 i = 0; i < n; ++i)		parent[i] = i;
		for (s32 a = 0; a < n; ++a)
		{
			s32	i = r->latOrder[a];
			for (s32 b = a + 1; b < n; ++b)
			{
				s32	j = r->latOrder[b];
				if ((r->zones[j].latDeg - r->zones[i].latDeg) * _SOLTIME_KM_PER_DEG > r->lockKm[i] + r->maxLock)
					break;
				if (iSolTime_distanceKm(r->zones[i].latDeg, r->zones[i].lonDegE, r->zones[j].latDeg, r->zones[j].lonDegE) < r->lockKm[i] + r->lockKm[j])
				{
					s32	x = iSolTime_find(parent, i), y = iSolTime_find(parent, j);
					if (x != y)		parent[x] = y;
				}
			}
		}

		// Each group's leader:  its most populous zone (ties:  registry order)
		for (s32 i = 0; i < n; ++i)		r->leader[i] = -1;
		for (s32 i = 0; i < n; ++i)
		{
			s32	root = iSolTime_find(parent, i);
			s32	L	 = r->leader[root];
			if (L < 0 || r->zones[i].population > r->zones[L].population)
				r->leader[root] = i;
		}
		for (s32 i = 0; i < n; ++i)
			r->leader[i] = r->leader[iSolTime_find(parent, i)];
		delete[] parent;

		// Layer order:  the most populous leader first (ties:  registry order);  members by group
		for (s32 i = 0; i < n; ++i)
			if (r->leader[i] == i)		r->order[r->nGroups++] = i;
		std::stable_sort(r->order, r->order + r->nGroups, [r](s32 a, s32 b) { return r->zones[a].population > r->zones[b].population; });
		for (s32 pos = 0; pos < r->nGroups; ++pos)
			r->layerOf[r->order[pos]] = pos;
		for (s32 i = 0; i < n; ++i)
			r->layerOf[i] = r->layerOf[r->leader[i]];
		s32* count = new s32[r->nGroups + 1];
		for (s32 pos = 0; pos <= r->nGroups; ++pos)		count[pos] = 0;
		for (s32 i = 0; i < n; ++i)						++count[r->layerOf[i] + 1];
		r->memberStart[0] = 0;
		for (s32 pos = 0; pos < r->nGroups; ++pos)		r->memberStart[pos + 1] = r->memberStart[pos] + count[pos + 1];
		for (s32 pos = 0; pos < r->nGroups; ++pos)		count[pos] = r->memberStart[pos];
		for (s32 i = 0; i < n; ++i)						r->members[count[r->layerOf[i]]++] = i;
		delete[] count;

		// Each group's clock:  the year table at its leader
		for (s32 pos = 0; pos < r->nGroups; ++pos)
		{
			s32	L = r->order[pos];
			r->tables[L] = new f64[_SOLTIME_YEAR_DAYS];
			iSolTime_yearTable(&r->p, r->zones[L].latDeg, r->tables[L]);
		}

		// Jumps and ramps, in layer order (each measured against the clock beneath it); a published
		// registry carries them, and given values are used as they are
		for (s32 pos = 0; pos < r->nGroups; ++pos)
		{
			s32	L = r->order[pos];
			r->jump[L] = (r->zones[L].jumpSecs > 0.0) ? r->zones[L].jumpSecs : iSolTime_jump(r, pos);
			iSolTime_sizeRamp(r, pos);
			for (s32 j = r->memberStart[pos]; j < r->memberStart[pos + 1]; ++j)
				r->maxReach = fmax(r->maxReach, r->lockKm[r->members[j]] + r->rampKm[L]);
		}
		return r;
	}

	void soltime_registryDelete(SSolRegistry* r)
	{
		iSolTime_registryFree(r);
	}

	f64 soltime_zoneLockKm(SSolRegistry* r, s32 i)
	{
		return (r && i >= 0 && i < r->n) ? r->lockKm[i] : NAN;
	}

	f64 soltime_zoneRampKm(SSolRegistry* r, s32 i)
	{
		return (r && i >= 0 && i < r->n) ? r->rampKm[r->leader[i]] : NAN;
	}

	f64 soltime_zoneJumpSecs(SSolRegistry* r, s32 i)
	{
		return (r && i >= 0 && i < r->n) ? r->jump[r->leader[i]] : NAN;
	}

	s32 soltime_zoneLeader(SSolRegistry* r, s32 i)
	{
		return (r && i >= 0 && i < r->n) ? r->leader[i] : -2;
	}

	s32 soltime_zonesAt(SSolRegistry* r, f64 latDeg, f64 lonDegE, s32* idx, f64* weight, s32 capacity)
	{
		if (!r || !iSolTime_validLatLon(latDeg, lonDegE) || capacity < 0 || (capacity > 0 && !idx))
			return -2;

		s32			nc;
		s32*		cand	= iSolTime_near(r, latDeg, r->maxReach, &nc);
		SSolHit*	hit		= new SSolHit[(nc > 0) ? nc : 1];
		s32			nh		= 0;
		for (s32 j = 0; j < nc; ++j)
		{
			s32	e		= cand[j];
			f64	ramp	= r->rampKm[r->leader[e]];
			f64	d		= iSolTime_distanceKm(latDeg, lonDegE, r->zones[e].latDeg, r->zones[e].lonDegE);
			if (d <= r->lockKm[e])							{ hit[nh].layer = r->layerOf[e];  hit[nh].zone = e;  hit[nh].frac = 0.0;  ++nh; }
			else if (ramp > 0.0 && d < r->lockKm[e] + ramp)	{ hit[nh].layer = r->layerOf[e];  hit[nh].zone = e;  hit[nh].frac = (d - r->lockKm[e]) / ramp;  ++nh; }
		}
		delete[] cand;
		std::sort(hit, hit + nh, [](const SSolHit& a, const SSolHit& b) { return (a.layer != b.layer) ? (a.layer < b.layer) : (a.frac < b.frac); });

		// One entry per group, in layer order:  the weight of its deepest member (1 = locked)
		s32	count = 0;
		for (s32 j = 0; j < nh; ++j)
		{
			if (j > 0 && hit[j].layer == hit[j - 1].layer)		continue;
			if (count < capacity)
			{
				idx[count] = r->order[hit[j].layer];
				if (weight)		weight[count] = 1.0 - hit[j].frac;
			}
			++count;
		}
		delete[] hit;
		return count;
	}

	f64 soltime_zoneOffsetSecs(SSolRegistry* r, s32 i, f64 day)
	{
		if (!r || i < 0 || i >= r->n || !iSolTime_validDay(day))
			return NAN;
		s32	L = r->leader[i];
		return iSolTime_offsetAt(&r->p, r->tables[L], r->zones[L].latDeg, r->zones[L].lonDegE, day * _SOLTIME_DAY_SECS);
	}

	f64 soltime_offsetSecsR(SSolRegistry* r, f64 latDeg, f64 lonDegE, f64 day)
	{
		if (!r || !iSolTime_validLatLon(latDeg, lonDegE) || !iSolTime_validDay(day))
			return NAN;

		f64	sm[_SOLTIME_YEAR_DAYS];
		iSolTime_yearTable(&r->p, latDeg, sm);
		return iSolTime_layeredAt(r, sm, latDeg, lonDegE, day * _SOLTIME_DAY_SECS);
	}


//////////
// Internal:  the device clock (spec §10).  Internally the timeline is unwrapped across New Year
// (lap counts the wraps) so the display never jumps a year; values handed back are re-wrapped.
//////
	f64 soltime_offsetUnixSecsR(SSolRegistry* r, f64 lat, f64 lon, f64 utc)
	{
		if (!r || !iSolTime_validLatLon(lat, lon) || !iSolTime_validUnix(utc)) return NAN;
		return iSolTime_layeredAt(r, NULL, lat, lon, utc, true);
	}

	f64 soltime_zoneOffsetUnixSecs(SSolRegistry* r, s32 i, f64 utc)
	{
		if (!r || i < 0 || i >= r->n || !iSolTime_validUnix(utc)) return NAN;
		const SSolZone& z = r->zones[r->leader[i]];
		return iSolTime_calendarFieldAt(&r->p, z.latDeg, z.lonDegE, utc);
	}

	struct SSolClock
	{
		SSolTime		p;							// Servo parameters (and the field's, when there is no registry)
		SSolRegistry*	r;							// Nullable, not owned
		bool			exact;
		s32				follow;						// A zone's leader to follow wherever the device is; -1 = by position
		s64				lap;						// New Year wraps seen
		f64				lastSeen;					// Latest unwrapped time seen
		bool            unixTime;
		bool            hasShowLon, boundaryExact;
		f64             lastShowLon;

		bool			hasFix;
		f64				fixUtc, fixLat, fixLon, fixSpeed, fixHeading;			// The latest fix (unwrapped time)
		f64				planUtc, planLat, planLon, planSpeed, planHeading;		// The fix the current plan came from

		bool			shown;
		f64				dispUtc0, disp0, rate;		// display = disp0 + rate * (t - dispUtc0)
		f64				nextCheck, interval;
		bool			replan;
		bool			fast;
		u32				latched;					// Flags waiting for the next show

		bool			cacheValid;					// One-latitude year-table cache (standing still, east-west motion)
		f64				cacheLat;
		f64				cacheTable[_SOLTIME_YEAR_DAYS];
	};

	static f64 iSolTime_clockUnwrap(SSolClock* c, f64 utc)
	{
		if (c->unixTime) return utc;
		f64	t = utc + (f64)c->lap * _SOLTIME_YEAR_SECS;
		if (t < c->lastSeen - _SOLTIME_YEAR_SECS * 0.5)
		{
			++c->lap;
			t += _SOLTIME_YEAR_SECS;
		}
		if (t > c->lastSeen)	c->lastSeen = t;
		return t;
	}

	// Where the device will be at time t:  dead reckoning from the latest fix
	static void iSolTime_clockPos(SSolClock* c, f64 t, f64* lat, f64* lon)
	{
		f64	km = c->fixSpeed * (t - c->fixUtc) / 3600.0;
		if (km == 0.0)
		{
			*lat = c->fixLat;
			*lon = c->fixLon;
			return;
		}
		iSolTime_destination(c->fixLat, c->fixLon, km, c->fixHeading, lat, lon);
	}

	// The true clock (raw local seconds, unwrapped) at time t, where the device will be -- or the
	// clock of the zone it follows, wherever it is
	static f64 iSolTime_clockTruth(SSolClock* c, f64 t)
	{
		SSolTime*	q	= c->r ? &c->r->p : &c->p;
		f64			tw	= t - floor(t / _SOLTIME_YEAR_SECS) * _SOLTIME_YEAR_SECS;		// The year is periodic
		if (c->follow >= 0)
		{
			const SSolZone* z = &c->r->zones[c->follow];
			if (c->unixTime) return t + iSolTime_calendarFieldAt(q, z->latDeg, z->lonDegE, t);
			return t + iSolTime_offsetAt(q, c->r->tables[c->follow], z->latDeg, z->lonDegE, tw);
		}

		f64	lat, lon;
		iSolTime_clockPos(c, t, &lat, &lon);
		if (c->unixTime)
			return t + (c->r ? iSolTime_layeredAt(c->r, NULL, lat, lon, t, true)
			                 : iSolTime_calendarFieldAt(q, lat, lon, t));

		if (!c->cacheValid || c->cacheLat != lat)
		{
			iSolTime_yearTable(q, lat, c->cacheTable);
			c->cacheLat		= lat;
			c->cacheValid	= true;
		}

		f64	off	= c->r ? iSolTime_layeredAt(c->r, c->cacheTable, lat, lon, tw) : iSolTime_offsetAt(q, c->cacheTable, lat, lon, tw);
		return t + off;
	}

	static void iSolTime_clockPlanFix(SSolClock* c)
	{
		c->planUtc		= c->fixUtc;
		c->planLat		= c->fixLat;
		c->planLon		= c->fixLon;
		c->planSpeed	= c->fixSpeed;
		c->planHeading	= c->fixHeading;
		c->replan		= false;
	}

	// Outside the correction bound?
	static inline bool iSolTime_outside(f64 rate, f64 mc)
	{
		return (rate < 1.0 - mc || rate > 1.0 + mc);
	}

	// Show the true time now (silent for a small correction; beyond a minute's correction it is a
	// new time zone)
	static f64 iSolTime_clockSnap(SSolClock* c, f64 T, f64 Dnow)
	{
		if (fabs(T - Dnow) > c->p.max_correction * 60.0)
			c->latched |= SOLCLOCK_NEW_ZONE;
		return T;
	}

	// A checkpoint at time t.  why:  0 = first show (or re-sync), 1 = scheduled, 2 = the path changed.
	// "Moving fast" is local:  the true clock outruns 1 +/- max_correction right now (over the next
	// second), not on average over a minute that may run from a ramp into a lock.
	static void iSolTime_clockCheckpoint(SSolClock* c, f64 t, s32 why)
	{
		SSolTime*	q		= &c->p;
		f64			mc		= q->max_correction;
		f64			T		= iSolTime_clockTruth(c, t);
		f64 oneSecond = iSolTime_clockTruth(c, t + 1.0) - T;
		bool fast = fabs(oneSecond - 1.0) < 43200.0 && iSolTime_outside(oneSecond, mc);
		c->boundaryExact = false;
		f64			Dnow	= (why == 0) ? T : (c->disp0 + c->rate * (t - c->dispUtc0));

		// BUG_FIX_PLAN_2026-10-05.md FIX-01 — fixed 2026-10-05 16:43:53 EDT (UTC-04:00).
		// Only an ordinary turn in an existing flight may keep its current deadline.
		// A relocation must snap now; a newly fast clock must shorten its old plan.
		if (why == 2 && fast && c->fast && fabs(T - Dnow) <= mc * 60.0
		    && c->nextCheck - t >= 1.0 && c->nextCheck - t <= fmin(60.0, q->check_max_secs)
		    && fabs(iSolTime_clockTruth(c, c->nextCheck) - T - (c->nextCheck - t)) < 43200.0)
		{
			// A turn while moving fast:  re-aim from the time showing at the checkpoint already due -- no jump
			c->rate		= (iSolTime_clockTruth(c, c->nextCheck) - Dnow) / (c->nextCheck - t);
			c->disp0	= Dnow;
			c->dispUtc0	= t;
			c->fast		= true;
			iSolTime_clockPlanFix(c);
			return;
		}

		if (why != 0)
		{
			if (fast)
			{
				// Moving fast:  every checkpoint shows the true time
				Dnow = iSolTime_clockSnap(c, T, Dnow);

			} else {
				// A gap the bounded correction cannot close within the next minute is shown at once
				// (unless it is the truth itself that will outrun the bound in that minute -- motion,
				// which the planning below handles)
				f64	T60 = iSolTime_clockTruth(c, t + 60.0);
				if (iSolTime_outside((T60 - Dnow) / 60.0, mc) && !iSolTime_outside((T60 - T) / 60.0, mc))
					Dnow = iSolTime_clockSnap(c, T, Dnow);
			}
		}

		// Plan the next interval (auto-flex):  aim at the true clock at t + h, check halfway
		f64	hMax	= fast ? fmin(60.0, q->check_max_secs) : q->check_max_secs;
		f64	h		= fmin(c->interval, hMax);
		f64	aim, err;
		for (;;)
		{
			f64	Th	= iSolTime_clockTruth(c, t + h);
			// A date-line step is not a velocity. Never interpolate the 24-hour jump.
			if (fabs(Th - T - h) > 43200.0) {
				if (h > _SOLTIME_CHECK_MIN_SECS) { h = fmax(_SOLTIME_CHECK_MIN_SECS, h * 0.5); continue; }
				c->boundaryExact = true;
				Dnow = T; aim = 1.0; err = q->check_tolerance_secs; break;
			}
			f64	raw	= (Th - Dnow) / h;
			if (!fast)
			{
				if (iSolTime_outside((Th - T) / h, mc))
				{
					// The truth itself outruns the bound somewhere in here:  look closer; if it does so
					// right at the shortest interval, this is fast motion
					if (h > _SOLTIME_CHECK_MIN_SECS)	{ h = fmax(_SOLTIME_CHECK_MIN_SECS, h * 0.5);  continue; }
					fast	= true;
					hMax	= fmin(60.0, q->check_max_secs);
					Dnow	= iSolTime_clockSnap(c, T, Dnow);
					raw		= (Th - Dnow) / h;

				} else if (iSolTime_outside(raw, mc)) {
					// A gap to glide off within the bound:  check again within the minute
					if (h > 60.0)	{ h = 60.0;  continue; }
					aim	= iSolTime_clamp(raw, 1.0 - mc, 1.0 + mc);
					err	= 0.0;
					break;
				}
			}
			aim	= raw;
			err	= fabs(Dnow + aim * h * 0.5 - iSolTime_clockTruth(c, t + h * 0.5));
			if (err > q->check_tolerance_secs && h > _SOLTIME_CHECK_MIN_SECS)
			{
				h = fmax(_SOLTIME_CHECK_MIN_SECS, h * 0.5);
				continue;
			}
			break;
		}

		c->fast			= fast;
		c->interval		= (err < q->check_tolerance_secs * 0.25) ? fmin(hMax, h * 2.0) : h;
		c->disp0		= Dnow;
		c->dispUtc0		= t;
		c->rate			= aim;
		c->nextCheck	= t + h;
		c->shown		= true;
		iSolTime_clockPlanFix(c);
	}


//////////
// API -- the device clock (§10)
//////
	SSolClock* soltime_clockCreate(SSolTime* p, SSolRegistry* r)
	{
		SSolTime	def;
		p = iSolTime_params(p, &def);
		if (!iSolTime_validParams(p))
			return NULL;

		SSolClock*	c	= new SSolClock;
		c->p			= *p;
		c->r			= r;
		c->exact		= false;
		c->follow		= -1;
		c->lap			= 0;
		c->lastSeen		= -INFINITY;
		c->unixTime = false;
		c->hasShowLon = c->boundaryExact = false;
		c->lastShowLon = 0.0;
		c->hasFix		= false;
		c->fixUtc		= c->fixLat = c->fixLon = c->fixSpeed = c->fixHeading = 0.0;
		c->planUtc		= c->planLat = c->planLon = c->planSpeed = c->planHeading = 0.0;
		c->shown		= false;
		c->dispUtc0		= c->disp0 = 0.0;
		c->rate			= 1.0;
		c->nextCheck	= 0.0;
		c->interval		= p->check_max_secs;
		c->replan		= false;
		c->fast			= false;
		c->latched		= 0;
		c->cacheValid	= false;
		c->cacheLat		= 0.0;
		return c;
	}

	void soltime_clockDelete(SSolClock* c)
	{
		delete c;
	}

	SSolClock* soltime_clockCreateUnix(SSolTime* p, SSolRegistry* r)
	{
		SSolClock* c = soltime_clockCreate(p, r);
		if (c) c->unixTime = true;
		return c;
	}

	static bool iSolTime_clockValidUtc(SSolClock* c, f64 utc)
	{
		return c && (c->unixTime ? iSolTime_validUnix(utc)
		                        : (isfinite(utc) && utc >= 0.0 && utc < _SOLTIME_YEAR_SECS));
	}

	s32 soltime_clockFix(SSolClock* c, f64 utcSecsOfYear, f64 latDeg, f64 lonDegE, f64 speedKmh, f64 headingDeg)
	{
		if (!iSolTime_clockValidUtc(c, utcSecsOfYear)
		 || !iSolTime_validLatLon(latDeg, lonDegE) || !isfinite(speedKmh) || speedKmh < 0.0 || !isfinite(headingDeg))
			return -2;

		f64	t		= iSolTime_clockUnwrap(c, utcSecsOfYear);
		c->fixUtc		= t;
		c->fixLat		= latDeg;
		c->fixLon		= lonDegE;
		c->fixSpeed		= speedKmh;
		c->fixHeading	= remainder(headingDeg, 360.0);
		c->hasFix		= true;

		if (c->shown)
		{
			// A departure from the plan -- off the planned path, a change of speed, or a turn -- re-plans
			f64	pLat = c->planLat, pLon = c->planLon;
			f64	km	= c->planSpeed * (t - c->planUtc) / 3600.0;
			if (km != 0.0)
				iSolTime_destination(c->planLat, c->planLon, km, c->planHeading, &pLat, &pLon);

			f64	off	= iSolTime_distanceKm(pLat, pLon, latDeg, lonDegE);
			f64	dh	= fabs(remainder(c->fixHeading - c->planHeading, 360.0));
			if (off > 0.05 || fabs(speedKmh - c->planSpeed) > 1.0 || (speedKmh > 1.0 && dh > 2.0))
				c->replan = true;
		}
		return 0;
	}

	f64 soltime_clockShow(SSolClock* c, f64 utcSecsOfYear, u32* flags)
	{
		u32	sink;
		if (!flags)		flags = &sink;
		*flags = 0;
		if (!iSolTime_clockValidUtc(c, utcSecsOfYear))
			return NAN;
		if (!c->hasFix && c->follow < 0)
		{
			*flags = SOLCLOCK_NO_FIX;
			return NAN;
		}

		f64	t		= iSolTime_clockUnwrap(c, utcSecsOfYear);
		f64	wrapTo	= c->unixTime ? 0.0 : (f64)c->lap * _SOLTIME_YEAR_SECS;
		if (c->follow < 0) {
			f64 lat, lon; iSolTime_clockPos(c, t, &lat, &lon);
			if (c->hasShowLon && fabs(lon - c->lastShowLon) > 180.0) {
				c->latched |= SOLCLOCK_NEW_ZONE;
				c->shown = false; // Restart on this side of the date line, at the actual instant.
			}
			c->lastShowLon = lon; c->hasShowLon = true;
		}

		if (c->exact)
		{
			// Exact mode:  the true value, every call
			f64	T = iSolTime_clockTruth(c, t);
			if (c->shown && fabs((T - c->disp0) - (t - c->dispUtc0)) > c->p.max_correction * (60.0 + fabs(t - c->dispUtc0)))
				c->latched |= SOLCLOCK_NEW_ZONE;
			c->shown		= true;
			c->disp0		= T;
			c->dispUtc0		= t;
			c->rate			= 1.0;
			c->nextCheck	= t;
			c->replan		= false;
			*flags			= c->latched;
			c->latched		= 0;
			return T - wrapTo;
		}

		if (!c->shown || t < c->dispUtc0)
		{
			// First show, or the device's own clock went back
			iSolTime_clockCheckpoint(c, t, 0);

		} else {
			// Checkpoints happen at their scheduled moments, however seldom the display asks:  catch up
			// in order (a long silence gets one checkpoint, now)
			if (t - c->nextCheck > _SOLTIME_CATCHUP_SECS)		iSolTime_clockCheckpoint(c, t, 1);
			else while (t >= c->nextCheck)						iSolTime_clockCheckpoint(c, c->nextCheck, 1);
			if (c->replan)										iSolTime_clockCheckpoint(c, t, 2);
		}

		*flags		= c->latched | (c->fast ? SOLCLOCK_FAST : 0);
		c->latched	= 0;
		return (c->boundaryExact ? iSolTime_clockTruth(c, t)
		                        : c->disp0 + c->rate * (t - c->dispUtc0)) - wrapTo;
	}

	void soltime_clockExact(SSolClock* c, s32 on)
	{
		if (c)	c->exact = (on != 0);
	}

	s32 soltime_clockFollowZone(SSolClock* c, s32 zone)
	{
		if (!c)		return -2;
		c->hasShowLon = false;
		if (zone < 0)
		{
			c->follow	= -1;
			c->replan	= true;
			return 0;
		}
		if (!c->r || zone >= c->r->n)
			return -2;
		c->follow	= c->r->leader[zone];
		c->replan	= true;
		return 0;
	}

	s32 soltime_clockFollowing(SSolClock* c)
	{
		return c ? c->follow : -2;
	}

	f64 soltime_clockNextCheckUtc(SSolClock* c)
	{
		if (!c || !c->shown)	return NAN;
		return c->nextCheck - (f64)c->lap * _SOLTIME_YEAR_SECS;
	}
