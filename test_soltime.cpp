//////////
//
// test_soltime.cpp -- SolTime test harness (soltime_spec.md §7).
//
// UNITY BUILD:  #includes soltime.cpp directly so the iSolTime_* internals are visible.
// Dispatch:  `soltime_tests` runs every test; `soltime_tests /test:<name>` runs one.
// Each test returns the number of failed checks (0 = pass).
//
//////
	#include <stdio.h>
	#include <string.h>
	#include "soltime.cpp"


//////////
// Tiny check harness (per-test failure counter, single-threaded)
//////
	static u32	gtFails;
	static char	gtMsg[256];

	#define TCHECK(cond, msg)												\
		do {																\
			if (cond)	printf("  ok:    %s\n", (msg));						\
			else		{ printf("  FAIL:  %s\n", (msg));  ++gtFails; }		\
		} while (0)

	// Quiet variant for grid sweeps -- reports once at the end via TCHECK
	#define TQUIET(cond, failCounter)		do { if (!(cond))  ++(failCounter); } while (0)

	// A formatted message for TCHECK
	#define TMSG(...)						(snprintf(gtMsg, sizeof(gtMsg), __VA_ARGS__), gtMsg)

	static const f64	tMph80 = 128.75;		// km/h


//////////
// Shared helpers
//////
	// The layered clock's daily value at a point, from the registry's internals (sm unused:  kept
	// for the call sites' shape)
	static f64 tLayeredDay(SSolRegistry* r, f64 lat, f64 lon, s64 k, f64* sm)
	{
		(void)sm;
		SSolEval	ev	= { true, k, 0.0 };
		SSolPrep*	pp	= iSolTime_prepare(r, r->nGroups, lat, lon, NULL);
		f64			v	= iSolTime_prepValue(r, pp, &ev);
		iSolTime_prepFree(pp);
		return v;
	}

	// The layered daily values of one point, every 5th day (one preparation per point)
	static void tLayeredDays(SSolRegistry* r, f64 lat, f64 lon, f64* out)
	{
		SSolEval	ev	= { true, 0, 0.0 };
		SSolPrep*	pp	= iSolTime_prepare(r, r->nGroups, lat, lon, NULL);
		for (ev.k = 0; ev.k < 366; ev.k += 5)
			out[ev.k / 5] = iSolTime_prepValue(r, pp, &ev);
		iSolTime_prepFree(pp);
	}

	// Steepest slope (s/km) of the layered clock anywhere in zone i's ramp (bearings every 15
	// degrees, every half km, every 5th day)
	static f64 tSteepestInRamp(SSolRegistry* r, s32 i)
	{
		const SSolZone*	b		= &r->zones[i];
		f64					R		= r->lockKm[i];
		f64					ramp	= r->rampKm[r->leader[i]];
		f64					g		= 0.0;
		f64					o[74], oE[74], oN[74];

		for (s32 a = 0; a < 360; a += 15)
		{
			for (f64 km = fmax(0.0, R - 1.0); km <= R + ramp + 1.0; km += 0.5)
			{
				f64	la, lo, laE, loE, laN, loN;
				iSolTime_destination(b->latDeg, b->lonDegE, km, (f64)a, &la, &lo);
				iSolTime_destination(la, lo, 0.25,  90.0, &laE, &loE);
				iSolTime_destination(la, lo, 0.25,   0.0, &laN, &loN);
				tLayeredDays(r, la,  lo,  o);
				tLayeredDays(r, laE, loE, oE);
				tLayeredDays(r, laN, loN, oN);
				for (s32 j = 0; j < 74; ++j)
				{
					f64	gE	= iSolTime_wrapDay(oE[j] - o[j]) / 0.25;
					f64	gN	= iSolTime_wrapDay(oN[j] - o[j]) / 0.25;
					g = fmax(g, sqrt(gE * gE + gN * gN));
				}
			}
		}
		return g;
	}

	// Walk a straight line (great circle) between two points:  the biggest change of the layered
	// clock between samples `step` km apart, on a given day's value; *where gets its distance
	static f64 tWalkStep(SSolRegistry* r, f64 la0, f64 lo0, f64 la1, f64 lo1, s64 k, f64 step, f64* where)
	{
		f64	sm[366];
		f64	L		= iSolTime_distanceKm(la0, lo0, la1, lo1);
		f64	brg		= atan2(sin((lo1 - lo0) * _SOLTIME_DEG2RAD) * cos(la1 * _SOLTIME_DEG2RAD),
							cos(la0 * _SOLTIME_DEG2RAD) * sin(la1 * _SOLTIME_DEG2RAD)
							- sin(la0 * _SOLTIME_DEG2RAD) * cos(la1 * _SOLTIME_DEG2RAD) * cos((lo1 - lo0) * _SOLTIME_DEG2RAD)) * _SOLTIME_RAD2DEG;
		f64	prev	= tLayeredDay(r, la0, lo0, k, sm);
		f64	big		= 0.0;
		for (f64 km = step; km <= L; km += step)
		{
			f64	la, lo;
			iSolTime_destination(la0, lo0, km, brg, &la, &lo);
			f64	o = tLayeredDay(r, la, lo, k, sm);
			f64	d = fabs(iSolTime_wrapDay(o - prev));
			if (d > big)	{ big = d;  if (where) *where = km; }
			prev = o;
		}
		return big;
	}

	// No step anywhere on the walk:  around the steepest 25 m, 2.5 m samples change ~10x less
	// (a step would not shrink).  Returns the steepest slope, s/km; *stepped counts failures.
	static f64 tWalkNoStep(SSolRegistry* r, f64 la0, f64 lo0, f64 la1, f64 lo1, u32* stepped)
	{
		f64	worst = 0.0;
		s64	days[] = { 171, 354, 80 };
		for (u32 j = 0; j < 3; ++j)
		{
			f64	where = 0.0;
			f64	big = tWalkStep(r, la0, lo0, la1, lo1, days[j], 0.025, &where);
			worst = fmax(worst, big / 0.025);

			// Zoom in on the steepest stretch with 2.5 m samples
			f64	L	= iSolTime_distanceKm(la0, lo0, la1, lo1);
			f64	t0	= fmax(0.0, where - 0.05) / L, t1 = fmin(L, where + 0.025) / L;
			f64	a0 = la0 + (la1 - la0) * t0, b0 = lo0 + (lo1 - lo0) * t0;
			f64	a1 = la0 + (la1 - la0) * t1, b1 = lo0 + (lo1 - lo0) * t1;
			f64	fine = tWalkStep(r, a0, b0, a1, b1, days[j], 0.0025, NULL);
			if (!(fine <= 0.2 * big + 1.0e-3))		++*stepped;
		}
		return worst;
	}

	// Inside zone i's lock (many points, several instants) the layered clock reads exactly zone i's
	// -- wherever zone i is on top (a finer zone nested inside takes over its own zone)
	static u32 tLockExact(SSolRegistry* r, s32 i)
	{
		u32	off = 0;
		s32	idx[16];
		for (s32 brg = 0; brg < 360; brg += 20)
		{
			f64	fr[] = { 0.0, 0.3, 0.7, 0.95, 0.999 };
			for (u32 j = 0; j < 5; ++j)
			{
				f64	la, lo;
				iSolTime_destination(r->zones[i].latDeg, r->zones[i].lonDegE, r->lockKm[i] * fr[j], (f64)brg, &la, &lo);
				s32	n = soltime_zonesAt(r, la, lo, idx, NULL, 16);
				if (n < 1 || n > 16 || idx[n - 1] != r->leader[i])	continue;
				for (f64 day = 10.37; day < 366.0; day += 61.0)
					TQUIET(fabs(iSolTime_wrapDay(soltime_offsetSecsR(r, la, lo, day) - soltime_zoneOffsetSecs(r, i, day))) < 1.0e-6, off);
			}
		}
		return off;
	}

	// The true clock (raw local seconds) at a place and UTC instant, with or without a registry
	static f64 tTruth(SSolRegistry* r, f64 lat, f64 lon, f64 utc)
	{
		f64	tw = utc - floor(utc / _SOLTIME_YEAR_SECS) * _SOLTIME_YEAR_SECS;
		return utc + (r ? soltime_offsetSecsR(r, lat, lon, tw / 86400.0) : soltime_offsetSecs(NULL, lat, lon, tw / 86400.0));
	}


//////////
// Test:  /test:noaa -- the solar engine against known ephemeris values.  Boulder-ish
// (40N 105W) Jun 21:  sunrise ~11:31Z, solar noon ~19:02Z.  Equator:  day length is
// ~12h07 (refraction) every day of the year.
//////
	static u32 test_noaa(void)
	{
		gtFails = 0;

		f64	rise, set, noon;
		s32	st = soltime_sunTimesUtc(NULL, 40.0, -105.0, 171.0, &rise, &set, &noon);
		TCHECK(st == 0,												"40N 105W Jun 21:  normal rise/set day");
		TCHECK(fabs(rise - 41478.0) < 180.0,						"sunrise ~11:31 UTC (+/-3 min)");
		TCHECK(fabs(noon - 68502.0) < 180.0,						"solar noon ~19:02 UTC (+/-3 min)");
		TCHECK(set > noon && noon > rise,							"rise < noon < set");

		u32	bad = 0;
		for (s32 d = 0; d < 366; ++d)
		{
			soltime_sunTimesUtc(NULL, 0.0, 0.0, (f64)d, &rise, &set, &noon);
			f64	len = set - rise;
			TQUIET(len > 12.0 * 3600.0 + 240.0 && len < 12.0 * 3600.0 + 720.0, bad);
		}
		TCHECK(bad == 0,											"equator day length ~12h07 all 366 days");

		return gtFails;
	}


//////////
// Test:  /test:erosion -- the O(n) two-pass sweep (spec §3) against the direct min over j,
// and the envelope's properties
//////
	static void tErodeDirect(const f64* target, f64 slide, f64* out)
	{
		for (s32 i = 0; i < 366; ++i)
		{
			f64	best = target[i];
			for (s32 j = 0; j < 366; ++j)
			{
				s32	d = (i >= j) ? (i - j) : (j - i);
				if (d > 366 - d)	d = 366 - d;
				f64	v = target[j] + slide * (f64)d;
				if (v < best)	best = v;
			}
			out[i] = best;
		}
	}

	static u32 test_erosion(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		u32	differ = 0, above = 0, steep = 0;
		f64	worst = 0.0;
		for (f64 lat = -90.0; lat <= 90.0; lat += 0.5)
		{
			f64	target[366], a[366], b[366];
			iSolTime_targetTable(&p, lat, target);
			memcpy(a, target, sizeof(a));
			iSolTime_erode(a, p.max_slide_secs);
			tErodeDirect(target, p.max_slide_secs, b);
			for (s32 i = 0; i < 366; ++i)
			{
				if (a[i] != b[i])	{ ++differ;  worst = fmax(worst, fabs(a[i] - b[i])); }
				TQUIET(a[i] <= target[i], above);
				TQUIET(fabs(a[(i + 1) % 366] - a[i]) <= p.max_slide_secs + 1.0e-9, steep);
			}
		}
		TCHECK(differ == 0,											TMSG("two-pass sweep bit-identical to the direct min, every half degree (%u differ, worst %.3g s)", differ, worst));
		TCHECK(above == 0,											"never above the raw target");
		TCHECK(steep == 0,											"slope-limited:  <= max_slide between any two days, New Year included");

		return gtFails;
	}


//////////
// Test:  /test:anchor -- the §3 theorem ON THE RUNNING CLOCK:  at the real sunrise instant the
// clock reads <= ANCHOR wherever a place keeps its own basis, and exactly ANCHOR where the
// limiter does not bind.
//////
	static u32 test_anchor(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		f64	lons[]   = { 0.0, -105.0, -75.0, 151.2, 100.0 };
		u32	over     = 0;			// Sunrise after the anchor (bus violated) -- must be 0
		u32	unpinned = 0;			// Limiter not binding, but sunrise != anchor -- must be 0
		u32	checked  = 0;

		for (f64 lat = -65.0; lat <= 65.0; lat += 5.0)
		{
			f64	target[366], sm[366];
			iSolTime_targetTable(&p, lat, target);
			memcpy(sm, target, sizeof(sm));
			iSolTime_erode(sm, p.max_slide_secs);

			for (u32 li = 0; li < sizeof(lons) / sizeof(lons[0]); ++li)
			{
				for (s32 k = 2; k < 364; ++k)
				{
					f64	bl, rise, set, noon;
					iSolTime_basisHa(&p, lat, k, &bl);
					if (bl != lat)																	continue;		// Borrowed basis:  §4's slip, tested in /test:basis
					if (iSolTime_sunTimes(lat, lons[li], k, p.zenith_deg, &rise, &set, &noon) != 0)	continue;

					f64	t	= (f64)k * 86400.0 + rise;
					f64	tod	= soltime_localFromUtc(&p, lat, lons[li], t) - (f64)k * 86400.0;
					TQUIET(tod <= p.anchor_secs + 1.0e-6, over);
					if (sm[k] == target[k])
						TQUIET(fabs(tod - p.anchor_secs) < 1.0e-6, unpinned);
					++checked;
				}
			}
		}
		TCHECK(checked > 40000,										TMSG("%u real sunrises checked on the running clock", checked));
		TCHECK(over == 0,											"<= 06:45 at every real sunrise where a place keeps its own basis (the bus theorem)");
		TCHECK(unpinned == 0,										"exactly 06:45 wherever the limiter does not bind");

		// The summer-evening story at 40N:  Jun 21 sunset lands in the late evening
		f64	rise, set, noon;
		soltime_sunTimesUtc(&p, 40.0, -105.0, 171.0, &rise, &set, &noon);
		f64	setLocal = soltime_localFromUtc(&p, 40.0, -105.0, 171.0 * 86400.0 + set) - 171.0 * 86400.0;
		TCHECK(setLocal > 21.5 * 3600.0 && setLocal < 22.5 * 3600.0,	"40N Jun 21:  sunset ~21:45 local (surplus spills to evening)");

		return gtFails;
	}


//////////
// Test:  /test:slide -- no cliffs:  |O(k+1) - O(k)| <= max_slide at every latitude including the
// poles, circularly; and the public offset in the daytime is exactly the day's value.
//////
	static u32 test_slide(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		u32	cliffs = 0, nans = 0;
		for (f64 lat = -90.0; lat <= 90.0; lat += 1.0)
		{
			f64	sm[366];
			iSolTime_yearTable(&p, lat, sm);
			for (s32 k = 0; k < 366; ++k)
			{
				TQUIET(isfinite(sm[k]), nans);
				TQUIET(fabs(sm[(k + 1) % 366] - sm[k]) <= p.max_slide_secs + 1.0e-9, cliffs);
			}
		}
		TCHECK(cliffs == 0 && nans == 0,							"every latitude -90..90:  finite, <= 240 s/day across all 366 seams (incl. New Year)");

		// Local noon sits in the day's flat part:  the public offset IS the day's value there
		u32	off = 0;
		f64	sm[366];
		iSolTime_yearTable(&p, 40.0, sm);
		for (s32 k = 1; k < 366; k += 7)
		{
			f64	t = (f64)k * 86400.0 + 43200.0 - (sm[k] + 240.0 * -86.16);
			TQUIET(soltime_offsetSecs(&p, 40.0, -86.16, t / 86400.0) == sm[k] + 240.0 * -86.16, off);
		}
		TCHECK(off == 0,											"at local noon the offset is exactly that day's value (true seconds all day)");

		return gtFails;
	}


//////////
// Test:  /test:travel -- +240 s per degree east, exact for each day's value; continuity in
// longitude at every instant (night windows included) away from the date line; and the date
// line:  a whole day, plus that day's slide.
//////
	static u32 test_travel(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		f64	sm[366];
		iSolTime_yearTable(&p, 40.0, sm);

		u32	bad = 0;
		for (f64 lon = -179.0; lon <= 178.0; lon += 13.0)
		{
			for (s32 k = 1; k < 366; k += 30)
			{
				f64	t = (f64)k * 86400.0 + 43200.0 - (sm[k] + 240.0 * lon);		// Local noon at lon (lon + 1 reads 12:04, also flat)
				f64	a = soltime_offsetSecs(&p, 40.0, lon,       t / 86400.0);
				f64	b = soltime_offsetSecs(&p, 40.0, lon + 1.0, t / 86400.0);
				TQUIET(fabs((b - a) - 240.0) < 1.0e-6, bad);
			}
		}
		TCHECK(bad == 0,											"one degree east = exactly +240 s for each day's value, every lon, every month");

		// Continuity at every instant, night windows included:  0.001 degree is 0.24 s
		u32	jumps = 0;
		f64	worst = 0.0;
		for (f64 t = 100.0 * 86400.0; t < 102.0 * 86400.0; t += 1800.0)
		{
			for (f64 lon = -179.0; lon <= 178.0; lon += 2.0)
			{
				f64	d = fabs(soltime_offsetSecs(&p, 40.0, lon + 0.001, t / 86400.0) - soltime_offsetSecs(&p, 40.0, lon, t / 86400.0));
				worst = fmax(worst, d);
				TQUIET(d < 1.0, jumps);
			}
		}
		TCHECK(jumps == 0,											TMSG("continuous in longitude at every instant, night windows included (worst %.3f s per 0.001 deg)", worst));

		// The date line:  a whole day, plus that day's slide (the two sides are on different days' schedules)
		u32	dl = 0;
		for (s32 k = 1; k < 366; k += 5)
		{
			f64	t = (f64)k * 86400.0 + 12.0 * 3600.0;
			f64	d = soltime_offsetSecs(&p, 40.0, 180.0, t / 86400.0) - soltime_offsetSecs(&p, 40.0, -180.0, t / 86400.0);
			TQUIET(fabs(d - 86400.0) <= p.max_slide_secs + 1.0e-6, dl);
		}
		TCHECK(dl == 0,												"date line:  +180 vs -180 differ by a day, give or take that day's slide (<= 240 s)");

		return gtFails;
	}


//////////
// Test:  /test:basis -- §4:  the closed form against bisection; a place's own latitude wherever
// day and night are both >= M; continuity in latitude and day; polar-night solar noon at
// ANCHOR + M/2; the sunrise slip at most M/2.
//////
	static f64 tBasisBisect(SSolTime* p, f64 lat, s32 m)
	{
		const SSolYear&	y	= iSolTime_year();
		f64				a	= fabs(lat);
		f64				sg	= (lat < 0.0) ? -1.0 : 1.0;
		auto			ok	= [&](f64 x) { f64 D = 480.0 * iSolTime_hourAngle(sg * x, y.decl[m], p->zenith_deg, NULL);
										   return (D >= p->basis_min_secs && 86400.0 - D >= p->basis_min_secs); };
		if (ok(a))		return lat;
		f64	lo = 0.0, hi = a;
		for (s32 i = 0; i < 80; ++i)
		{
			f64	mid = 0.5 * (lo + hi);
			if (ok(mid))	lo = mid;
			else			hi = mid;
		}
		return sg * lo;
	}

	static u32 test_basis(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		u32	far = 0;
		f64	worst = 0.0;
		for (f64 lat = -89.5; lat <= 89.5; lat += 0.5)
		{
			for (s32 m = 0; m < 366; m += 3)
			{
				f64	bl;
				iSolTime_basisHa(&p, lat, m, &bl);
				f64	d = fabs(bl - tBasisBisect(&p, lat, m));
				worst = fmax(worst, d);
				TQUIET(d < 1.0e-9, far);
			}
		}
		TCHECK(far == 0,											TMSG("closed form == bisection, every half degree, every 3rd day (worst %.2g deg)", worst));

		// Own latitude wherever day and night are both >= M; continuity in latitude and day
		u32	own = 0, jumpLat = 0, jumpDay = 0;
		const SSolYear&	y = iSolTime_year();
		for (f64 lat = -89.0; lat <= 89.0; lat += 0.25)
		{
			for (s32 m = 0; m < 366; ++m)
			{
				f64	bl, bl2, bl3;
				iSolTime_basisHa(&p, lat, m, &bl);
				f64	D = 480.0 * iSolTime_hourAngle(lat, y.decl[m], p.zenith_deg, NULL);
				if (D >= p.basis_min_secs && 86400.0 - D >= p.basis_min_secs)
					TQUIET(bl == lat, own);
				iSolTime_basisHa(&p, lat + 0.001, m, &bl2);
				TQUIET(fabs(bl2 - bl) <= 0.001 + 1.0e-9, jumpLat);
				iSolTime_basisHa(&p, lat, (m + 1) % 366, &bl3);
				TQUIET(fabs(bl3 - bl) < 0.75, jumpDay);
			}
		}
		TCHECK(own == 0,											"a place keeps its own latitude wherever day and night are both >= 4 h");
		TCHECK(jumpLat == 0,										"continuous in latitude (a clamp:  never moves faster than the place does)");
		TCHECK(jumpDay == 0,										"continuous in day (< 0.75 deg/day, New Year included)");

		// Polar night at 78N and 69.6N, Dec 21:  the twilight peak (solar noon) at ANCHOR + M/2 = 08:45
		f64	rise, set, noon;
		f64	targets[] = { 78.0, 69.6 };
		for (u32 j = 0; j < 2; ++j)
		{
			s32	st			= soltime_sunTimesUtc(&p, targets[j], 0.0, 354.0, &rise, &set, &noon);
			f64	noonLocal	= soltime_localFromUtc(&p, targets[j], 0.0, 354.0 * 86400.0 + noon) - 354.0 * 86400.0;
			TCHECK(st == -1 && fabs(noonLocal - (p.anchor_secs + 0.5 * p.basis_min_secs)) < 1.0,
																	TMSG("%.1fN Dec 21:  polar night, solar noon at 08:45 local (%.1f s off)", targets[j], noonLocal - (p.anchor_secs + 0.5 * p.basis_min_secs)));
		}

		// The slip:  wherever the sun rises, sunrise comes at most M/2 after the anchor
		u32	slip = 0;
		f64	most = 0.0;
		for (f64 lat = -89.5; lat <= 89.5; lat += 0.5)
		{
			f64	sm[366];
			iSolTime_yearTable(&p, lat, sm);
			for (s32 m = 0; m < 366; ++m)
			{
				if (iSolTime_sunTimes(lat, 0.0, m, p.zenith_deg, &rise, &set, &noon) != 0)		continue;
				f64	late = rise + sm[m] - p.anchor_secs;
				most = fmax(most, late);
				TQUIET(late <= 0.5 * p.basis_min_secs + 1.0e-6, slip);
			}
		}
		TCHECK(slip == 0,											TMSG("sunrise slip <= M/2 = 2 h everywhere the sun rises (worst %.0f min)", most / 60.0));

		return gtFails;
	}


//////////
// Test:  /test:polar -- above the circles:  correct +1/-1 state flags, and the offset stays
// finite and slope-limited straight through the polar-day/night transitions.
//////
	static u32 test_polar(void)
	{
		gtFails = 0;

		f64	rise, set, noon;
		TCHECK(soltime_sunTimesUtc(NULL,  80.0, 0.0, 171.0, &rise, &set, &noon) == +1,	"80N Jun 21:  polar day (+1)");
		TCHECK(soltime_sunTimesUtc(NULL,  80.0, 0.0, 354.0, &rise, &set, &noon) == -1,	"80N Dec 21:  polar night (-1)");
		TCHECK(soltime_sunTimesUtc(NULL,  80.0, 0.0,  80.0, &rise, &set, &noon) ==  0,	"80N late Mar:  normal day (0)");
		TCHECK(soltime_sunTimesUtc(NULL, -80.0, 0.0, 171.0, &rise, &set, &noon) == -1,	"80S Jun 21:  polar night (-1)");

		SSolTime	p;
		soltime_defaults(&p);
		u32	bad = 0;
		f64	lats[] = { 89.0, 90.0, -90.0, 75.0, -70.0 };
		for (u32 j = 0; j < sizeof(lats) / sizeof(lats[0]); ++j)
			for (s32 d = 0; d < 366; ++d)
				TQUIET(isfinite(soltime_offsetSecs(&p, lats[j], 0.0, (f64)d + 0.3)), bad);
		TCHECK(bad == 0,											"89N, the poles, 75N, 70S:  offset finite all year");

		return gtFails;
	}


//////////
// Test:  /test:night -- §9:  the window lies inside the basis night and inside 20:00..04:00; the
// offset is constant outside it (true seconds all day); the window's length and the peak
// stretch; local midnight.
//////
	static u32 test_night(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		f64	lats[] = { 0.0, 40.0, -40.0, 55.0, 62.0, 65.0, 70.0, 80.0, -75.0 };
		f64	lons[] = { 0.0, -105.0, 151.2 };
		u32	outClip = 0, outNight = 0, notFlat = 0, shortWin = 0, midInside = 0, midTotal = 0;
		f64	shortest = 1.0e9;

		for (u32 a = 0; a < sizeof(lats) / sizeof(lats[0]); ++a)
		{
			f64	sm[366];
			iSolTime_yearTable(&p, lats[a], sm);
			for (u32 b = 0; b < sizeof(lons) / sizeof(lons[0]); ++b)
			{
				f64	lon = lons[b];
				for (s64 k = 1; k < 364; ++k)
				{
					f64	ws, we, ws2, we2;
					soltime_nightWindowUtc(&p, lats[a], lon, (f64)k + 0.5, &ws, &we);
					iSolTime_window(&p, sm, lats[a], lon, k + 1, &ws2, &we2);

					// Inside 20:00 (by O(k)) .. 04:00 (by O(k+1))
					f64	O0 = iSolTime_dayOffset(sm, lon, k), O1 = iSolTime_dayOffset(sm, lon, k + 1);
					TQUIET(ws >= (f64)k * 86400.0 + p.night_start_secs - O0 - 1.0e-6, outClip);
					TQUIET(we <= (f64)(k + 1) * 86400.0 + p.night_end_secs - O1 + 1.0e-6, outClip);

					// Inside the basis night
					const SSolYear&	y	= iSolTime_year();
					s32	m0	= iSolTime_dayIndex(k), m1 = iSolTime_dayIndex(k + 1);
					f64	ss	= (f64)k * 86400.0       + (720.0 - 4.0 * (lon - iSolTime_basisHa(&p, lats[a], m0, NULL)) - y.eqtimeMin[m0]) * 60.0;
					f64	sr	= (f64)(k + 1) * 86400.0 + (720.0 - 4.0 * (lon + iSolTime_basisHa(&p, lats[a], m1, NULL)) - y.eqtimeMin[m1]) * 60.0;
					TQUIET(ws >= ss - 1.0e-6 && we <= sr + 1.0e-6, outNight);

					// Length
					shortest = fmin(shortest, we - ws);
					TQUIET(we - ws >= p.basis_min_secs - (p.anchor_secs - p.night_end_secs) - p.max_slide_secs, shortWin);

					// Flat from this window's end to the next one's start:  O(k+1) exactly
					for (s32 s = 0; s <= 4; ++s)
					{
						f64	t = we + (ws2 - we) * (0.02 + 0.24 * s);
						TQUIET(iSolTime_offsetAt(&p, sm, lats[a], lon, t) == O1, notFlat);
					}

					// Local midnight (the clock reads 00:00 of day k+1):  inside the window, or not
					f64	lo = ws - 12.0 * 3600.0, hi = we + 12.0 * 3600.0, mid = (f64)(k + 1) * 86400.0;
					for (s32 it = 0; it < 60; ++it)
					{
						f64	t = 0.5 * (lo + hi);
						if (t + iSolTime_offsetAt(&p, sm, lats[a], lon, t) < mid)	lo = t;
						else														hi = t;
					}
					++midTotal;
					if (lo >= ws && lo < we)	++midInside;
				}
			}
		}
		TCHECK(outClip == 0,										"every window inside 20:00 .. 04:00 local");
		TCHECK(outNight == 0,										"every window inside the basis night (sunset .. sunrise)");
		TCHECK(notFlat == 0,										"the offset is constant between windows:  true seconds through the day");
		TCHECK(shortWin == 0,										TMSG("no window shorter than M - (ANCHOR - 04:00) = 1 h 15 m (less one day's slide); shortest %.2f h", shortest / 3600.0));
		printf("  info:  local midnight inside the window on %u of %u nights (outside:  sunset after midnight)\n", midInside, midTotal);

		// Peak second-stretch by latitude band (spec §9 table)
		f64	bands[][3] = { { 0.0, 45.0, 0.0080 }, { 45.0, 55.0, 0.0138 }, { 55.0, 60.0, 0.0241 }, { 60.0, 89.0, 0.1061 } };
		for (u32 j = 0; j < 4; ++j)
		{
			f64	peak = 0.0;
			for (f64 lat = bands[j][0]; lat <= bands[j][1]; lat += 1.0)
			{
				for (s32 sg = -1; sg <= 1; sg += 2)
				{
					f64	sm[366];
					iSolTime_yearTable(&p, sg * lat, sm);
					for (s64 k = 1; k < 365; ++k)
					{
						f64	ws, we;
						iSolTime_window(&p, sm, sg * lat, 0.0, k, &ws, &we);
						peak = fmax(peak, 2.0 * fabs(sm[k + 1] - sm[k]) / (we - ws));
					}
				}
			}
			TCHECK(peak <= bands[j][2] + 0.0005,					TMSG("|lat| %2.0f-%2.0f:  peak second-stretch %.2f%% (spec table %.1f%%)", bands[j][0], bands[j][1], peak * 100.0, bands[j][2] * 100.0));
		}

		return gtFails;
	}


//////////
// Test:  /test:localtime -- the civil guarantee:  at a fixed spot the local clock NEVER runs
// backward.  Every minute of the whole year, at every latitude band, New Year included.
//////
	static u32 test_localtime(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		f64	places[][2] = { { 40.0, -105.0 }, { -40.0, 151.2 }, { 0.0, 0.0 }, { 55.0, 37.6 }, { 65.0, 20.0 },
							{ 70.0, -30.0 }, { 80.0, 0.0 }, { 89.0, 0.0 }, { -89.0, 0.0 }, { -70.0, 120.0 } };
		for (u32 j = 0; j < sizeof(places) / sizeof(places[0]); ++j)
		{
			f64	sm[366];
			f64	lat = places[j][0], lon = places[j][1];
			iSolTime_yearTable(&p, lat, sm);

			u32	backward = 0;
			f64	prev = iSolTime_offsetAt(&p, sm, lat, lon, 0.0);
			for (f64 t = 60.0; t < _SOLTIME_YEAR_SECS + 3600.0; t += 60.0)
			{
				f64	loc = t + iSolTime_offsetAt(&p, sm, lat, lon, t);
				TQUIET(loc > prev, backward);
				prev = loc;
			}
			TCHECK(backward == 0,									TMSG("%+5.1f %+6.1f:  strictly forward, every minute of the year, across New Year", lat, lon));
		}

		return gtFails;
	}


//////////
// Test:  /test:zone -- §8
//////
	static u32 test_zone(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);
		f64			sm[366], smc[366];

		// Honest radii (spec table) and the guarantee itself, inside the disc too
		f64	lats[] = { 0.0, 30.0, 40.0, 50.0, 60.0 };
		f64	qs[]   = { 60.0, 300.0, 600.0 };
		for (u32 a = 0; a < sizeof(lats) / sizeof(lats[0]); ++a)
		{
			printf("  info:  honest radius %4.0fN:", lats[a]);
			for (u32 b = 0; b < 3; ++b)
				printf("  %3.0f s %6.1f km", qs[b], soltime_zoneRadiusKm(&p, qs[b], lats[a]));
			printf("\n");
		}

		u32	broke = 0, loose = 0;
		for (u32 a = 0; a < sizeof(lats) / sizeof(lats[0]); ++a)
		{
			f64	R = soltime_zoneRadiusKm(&p, 600.0, lats[a]);
			iSolTime_yearTable(&p, lats[a], smc);
			f64	worstIn = 0.0, worstOut = 0.0;
			for (s32 brg = 0; brg < 360; brg += 10)
			{
				f64	fr[] = { 0.25, 0.5, 0.75, 1.0, 1.02 };
				for (u32 j = 0; j < 5; ++j)
				{
					f64	la, lo;
					iSolTime_destination(lats[a], 0.0, R * fr[j], (f64)brg, &la, &lo);
					iSolTime_yearTable(&p, la, sm);
					for (s32 k = 0; k < 366; ++k)
					{
						f64	e = fabs(iSolTime_wrapDay(smc[k] - (sm[k] + 240.0 * lo)));
						if (j < 4)	worstIn  = fmax(worstIn, e);
						else		worstOut = fmax(worstOut, e);
					}
				}
			}
			TQUIET(worstIn <= 300.0 + 1.0e-6, broke);
			TQUIET(worstOut > 300.0, loose);
		}
		TCHECK(broke == 0,											"10-min zones 0..60N:  every point inside the lock, every day, within 5 min of its true SolTime");
		TCHECK(loose == 0,											"and the radius is tight:  2% further out, some point on some day is not");
		TCHECK(soltime_zoneQuantumForRadius(&p, 50.0, 40.0) == 600.0,	"radius 50 km at 40N -> 10-min class (5-min covers ~45 km)");
		TCHECK(soltime_zoneQuantumForRadius(&p, 40.0, 40.0) == 300.0,	"radius 40 km at 40N -> 5-min class");
		f64	q300 = soltime_zoneQuantumForRadius(&p, 300.0, 40.0);
		TCHECK(q300 > 1200.0 && fmod(q300, 60.0) == 0.0 && soltime_zoneRadiusKm(&p, q300, 40.0) >= 300.0 && soltime_zoneRadiusKm(&p, q300 - 60.0, 40.0) < 300.0,
																	TMSG("radius 300 km at 40N -> beyond the classes, the smallest whole minute that covers it (%.0f min)", q300 / 60.0));

		// Indy alone:  the jump is the half-class, the ramp keeps 80 mph inside 0.5 s/s
		SSolZone	indy = { 39.77, -86.16, 600.0, 0.0, -1.0, 887642.0, 0.0 };
		SSolRegistry*	r = soltime_registryCreate(&p, &indy, 1);
		TCHECK(r != NULL,											"Indianapolis 10-min registry");
		if (r)
		{
			f64	J = soltime_zoneJumpSecs(r, 0), R = soltime_zoneLockKm(r, 0), ramp = soltime_zoneRampKm(r, 0);
			TCHECK(J > 299.0 && J <= 300.0 + 1.0e-6,				TMSG("jump %.2f s = the half-class (lock %.1f km)", J, R));
			TCHECK(ramp > 20.0 && ramp < 40.0,						TMSG("auto ramp %.1f km (slope %.1f s/km)", ramp, J / ramp));

			// A far neighbor (Chicago, 265 km away, more populous:  first in the layers) must not lengthen it
			SSolZone	pair[2] = { { 41.88, -87.63, 600.0, 0.0, -1.0, 2664452.0, 0.0 }, indy };
			SSolRegistry* r2 = soltime_registryCreate(&p, pair, 2);
			TCHECK(r2 != NULL && r2->nGroups == 2 && fabs(soltime_zoneRampKm(r2, 1) - ramp) < 0.01,
																	TMSG("with Chicago in the registry, Indy's ramp is still %.1f km", r2 ? soltime_zoneRampKm(r2, 1) : NAN));
			soltime_registryDelete(r2);
			soltime_registryDelete(r);
		}

		// The drive budget:  80 mph inside 0.5 s/s through every auto ramp the budget fits; the floor beyond
		f64	budLats[] = { 30.0, 40.0, 50.0, 55.0, 60.0, 64.0 };
		for (u32 a = 0; a < sizeof(budLats) / sizeof(budLats[0]); ++a)
		{
			SSolZone		b	= { budLats[a], 10.0, 600.0, 0.0, -1.0, 0.0, 0.0 };
			SSolRegistry*	rb	= soltime_registryCreate(&p, &b, 1);
			if (!rb)	{ TCHECK(false, "registry");  continue; }

			f64	g		= tSteepestInRamp(rb, 0);
			f64	night	= iSolTime_nightRate(&rb->p, rb->tables[0], b.latDeg, b.lonDegE);
			f64	need	= g * tMph80 / 3600.0 + night;
			f64	slope	= rb->jump[0] / rb->rampKm[0];
			bool	floor_	= (fabs(slope - p.ramp_min_secs_per_km) < 1.0e-9);
			if (floor_)
				TCHECK(true,										TMSG("%2.0fN:  floor slope 2 s/km, ramp %.0f km; 80 mph needs %.3f s/s (keeps up to %.0f mph)", budLats[a], rb->rampKm[0], need, (0.5 - night) / g * 3600.0 / 1.609));
			else
				TCHECK(need <= 0.5 + 1.0e-3,						TMSG("%2.0fN:  lock %.1f km, ramp %.1f km (%.1f s/km):  80 mph needs %.3f s/s", budLats[a], rb->lockKm[0], rb->rampKm[0], slope, need));
			soltime_registryDelete(rb);
		}

		// Merging (RCH Oct.03):  a town whose disc lies inside Indy's merges into Indy's zone -- the
		// more populous city's clock, across the union
		f64	k40 = _SOLTIME_KM_PER_DEG * cos(39.77 * _SOLTIME_DEG2RAD);
		SSolZone	town[2] = { { 39.77, -86.16, 600.0, 0.0, -1.0, 887642.0, 0.0 }, { 39.77, -86.16 + 40.0 / k40, 0.0, 9.0, -1.0, 50000.0, 0.0 } };
		r = soltime_registryCreate(&p, town, 2);
		TCHECK(r != NULL && r->nGroups == 1 && soltime_zoneLeader(r, 1) == 0,	"a town 40 km east, inside Indy's disc:  merged, led by Indianapolis");
		if (r)
		{
			s32	idx[2];
			f64	w[2];
			s32	n = soltime_zonesAt(r, 39.77, -86.16 + 40.0 / k40, idx, w, 2);
			TCHECK(n == 1 && idx[0] == 0 && w[0] == 1.0,			"zonesAt in the town:  one zone, Indianapolis, locked");
			TCHECK(soltime_zonesAt(r, 0.0, 0.0, idx, w, 2) == 0,	"far away:  no zones");
			TCHECK(soltime_zonesAt(r, 91.0, 0.0, idx, w, 2) == -2,	"invalid point:  -2");
			TCHECK(soltime_offsetSecsR(r, 39.77, -86.16 + 40.0 / k40, 171.6) == soltime_zoneOffsetSecs(r, 0, 171.6)
				&& soltime_zoneOffsetSecs(r, 1, 171.6) == soltime_zoneOffsetSecs(r, 0, 171.6),
																	"the town reads Indianapolis's clock;  the town's own zone clock is Indy's");
			TCHECK(tLockExact(r, 0) == 0 && tLockExact(r, 1) == 0,	"inside both discs, at every instant tried, the clock is exactly Indy's");
			soltime_registryDelete(r);
		}

		// Two discs that overlap side by side:  one zone over the union;  no step anywhere around its
		// outline, the concave notch where the discs meet included;  the notch keeps the budget
		SSolZone	two[2] = { { 40.0, -86.0, 0.0, 30.0, -1.0, 400000.0, 0.0 }, { 40.0, -86.0 + 50.0 / k40, 0.0, 30.0, -1.0, 300000.0, 0.0 } };
		r = soltime_registryCreate(&p, two, 2);
		TCHECK(r != NULL && r->nGroups == 1 && soltime_zoneLeader(r, 1) == 0,	"two 30 km discs 50 km apart:  one zone, led by the more populous");
		if (r)
		{
			f64	midLon	= -86.0 + 25.0 / k40;
			u32	stepped	= 0;
			f64	s1 = tWalkNoStep(r, 40.0, midLon, 40.0 + 90.0 / 111.195, midLon, &stepped);			// North out through the notch
			f64	s2 = tWalkNoStep(r, 40.0, -86.0 - 80.0 / k40, 40.0, -86.0 + 130.0 / k40, &stepped);	// East through both discs and out
			TCHECK(stepped == 0,									TMSG("walks out through the notch and through both discs:  no step anywhere (steepest %.1f / %.1f s/km)", s1, s2));
			TCHECK(s1 <= 14.0 + 0.5 && s2 <= 14.0 + 0.5,			"and inside the 80 mph budget (14 s/km) -- the larger of the members' caps never adds slopes");
			TCHECK(tLockExact(r, 0) == 0 && tLockExact(r, 1) == 0,	"both discs read the leader's clock exactly");
			soltime_registryDelete(r);
		}

		// A chain:  A touches B, B touches C, A does not touch C -- one zone, led by the most populous
		SSolZone	chain[3] = { { 40.0, -90.0, 0.0, 30.0, -1.0, 100000.0, 0.0 }, { 40.0, -90.0 + 55.0 / k40, 0.0, 30.0, -1.0, 900000.0, 0.0 },
								 { 40.0, -90.0 + 110.0 / k40, 0.0, 30.0, -1.0, 200000.0, 0.0 } };
		r = soltime_registryCreate(&p, chain, 3);
		TCHECK(r != NULL && r->nGroups == 1 && soltime_zoneLeader(r, 0) == 1 && soltime_zoneLeader(r, 2) == 1,
																	"a chain of three:  one zone, led by the middle (most populous) city");
		if (r)
		{
			TCHECK(tLockExact(r, 0) == 0 && tLockExact(r, 2) == 0,	"the far ends read the leader's clock exactly");
			soltime_registryDelete(r);
		}

		// Side by side but apart (locks 22 km apart):  two zones;  each lock exact;  no step between them
		SSolZone	kd[2] = { { 22.57, 88.36, 600.0, 0.0, -1.0, 4631392.0, 0.0 }, { 23.81, 90.41, 600.0, 0.0, -1.0, 10356500.0, 0.0 } };
		r = soltime_registryCreate(&p, kd, 2);
		TCHECK(r != NULL && r->nGroups == 2,						"Kolkata + Dhaka 10-min (discs apart, ramps would meet):  two zones");
		if (r)
		{
			u32	stepped = 0;
			f64	gap		= iSolTime_distanceKm(22.57, 88.36, 23.81, 90.41) - r->lockKm[0] - r->lockKm[1];
			f64	steepest = tWalkNoStep(r, 22.57, 88.36, 23.81, 90.41, &stepped);
			TCHECK(stepped == 0,									TMSG("drive Kolkata -> Dhaka:  no step anywhere (steepest %.1f s/km in the %.0f km gap)", steepest, gap));
			TCHECK(soltime_zoneRampKm(r, 0) <= gap + 1.0e-9,		TMSG("Kolkata's ramp (the later layer) stops in the gap (%.1f km), short of Dhaka's disc", soltime_zoneRampKm(r, 0)));
			TCHECK(tLockExact(r, 0) == 0 && tLockExact(r, 1) == 0,	"Kolkata's disc reads Kolkata's clock, Dhaka's reads Dhaka's -- everywhere, every instant tried");
			soltime_registryDelete(r);
		}

		// Denver + Boulder as two 10-min zones (40 km apart):  merged, on Denver's clock
		SSolZone	db[2] = { { 39.74, -104.99, 600.0, 0.0, -1.0, 715522.0, 0.0 }, { 40.01, -105.27, 600.0, 0.0, -1.0, 108250.0, 0.0 } };
		r = soltime_registryCreate(&p, db, 2);
		TCHECK(r != NULL && r->nGroups == 1 && soltime_zoneLeader(r, 1) == 0 && tLockExact(r, 1) == 0,
																	"Denver + Boulder:  one zone;  Boulder's disc reads Denver's clock");
		soltime_registryDelete(r);

		// A published registry:  given jumps and ramps reproduce the computed one exactly
		SSolZone	pub[3] = { chain[0], chain[1], chain[2] };
		r = soltime_registryCreate(&p, pub, 3);
		if (r)
		{
			for (s32 i = 0; i < 3; ++i)		{ pub[i].rampKm = soltime_zoneRampKm(r, i);  pub[i].jumpSecs = soltime_zoneJumpSecs(r, i); }
			SSolRegistry*	r2	= soltime_registryCreate(&p, pub, 3);
			u32				off	= 0;
			for (f64 km = 0.0; km < 200.0; km += 7.3)
				for (f64 day = 3.3; day < 366.0; day += 73.0)
					TQUIET(soltime_offsetSecsR(r, 40.2, -90.5 + km / k40, day) == soltime_offsetSecsR(r2, 40.2, -90.5 + km / k40, day), off);
			TCHECK(r2 != NULL && off == 0,							"given jumps and ramps:  the same clock everywhere as computing them");
			soltime_registryDelete(r2);
			soltime_registryDelete(r);
		}

		// Hard edge:  the clock steps at the rim
		SSolZone	hard = { 39.77, -86.16, 600.0, 0.0, 0.0 };
		r = soltime_registryCreate(&p, &hard, 1);
		TCHECK(r != NULL && r->rampKm[0] == 0.0,					"rampKm 0:  a hard edge");
		if (r)
		{
			f64	la, lo, la2, lo2;
			iSolTime_destination(39.77, -86.16, r->lockKm[0] - 0.01,  90.0, &la, &lo);
			iSolTime_destination(39.77, -86.16, r->lockKm[0] + 0.01,  90.0, &la2, &lo2);
			f64	step = fabs(tLayeredDay(r, la2, lo2, 171, sm) - tLayeredDay(r, la, lo, 171, sm));
			TCHECK(step > 100.0,									TMSG("crossing it east on Jun 21:  a %.0f s step", step));
			soltime_registryDelete(r);
		}

		// Validation and the empty registry
		SSolZone	bad = { 39.77, -86.16, 0.0, 0.0, -1.0, 0.0, 0.0 };
		TCHECK(soltime_registryCreate(&p, &bad, 1) == NULL,			"quantum 0 -> refused");
		bad.quantumSecs = 600.0;  bad.radiusKm = NAN;
		TCHECK(soltime_registryCreate(&p, &bad, 1) == NULL,			"radius NAN -> refused");
		bad.radiusKm = 0.0;  bad.population = -1.0;
		TCHECK(soltime_registryCreate(&p, &bad, 1) == NULL,			"population -1 -> refused");
		bad.population = 0.0;  bad.jumpSecs = NAN;
		TCHECK(soltime_registryCreate(&p, &bad, 1) == NULL,			"jump NAN -> refused");
		TCHECK(soltime_registryCreate(&p, NULL, 1) == NULL,			"NULL zones with a count -> refused");
		TCHECK(soltime_registryCreate(&p, NULL, -1) == NULL,		"negative count -> refused");
		r = soltime_registryCreate(&p, NULL, 0);
		TCHECK(r != NULL && soltime_offsetSecsR(r, 40.0, -86.0, 171.6) == soltime_offsetSecs(&p, 40.0, -86.0, 171.6),
																	"empty registry == the plain field");
		TCHECK(isnan(soltime_zoneLockKm(r, 0)) && isnan(soltime_zoneOffsetSecs(r, 0, 1.0)),	"no zone 0 in an empty registry -> NAN");
		soltime_registryDelete(r);

		return gtFails;
	}


//////////
// Test:  /test:clock -- §10
//////
	struct STrack
	{
		f64		gap;			// Worst |display - truth|
		u32		zones;			// NEW_ZONE flags seen
		u32		fastSeen;		// Shows with SOLCLOCK_FAST
		u32		checks;			// Checkpoints (changes of the next-check time)
	};

	// Standing still:  shows every 10 s, truth compared every 97 s
	static STrack tStill(SSolRegistry* r, f64 lat, f64 lon, f64 utc0, f64 secs)
	{
		STrack		tr = { 0.0, 0, 0, 0 };
		SSolClock*	c = soltime_clockCreate(NULL, r);
		soltime_clockFix(c, utc0, lat, lon, 0.0, 0.0);
		f64	lastNext = NAN;
		for (f64 t = utc0; t < utc0 + secs; t += 10.0)
		{
			f64	u = fmod(t, _SOLTIME_YEAR_SECS);
			u32	fl;
			f64	D = soltime_clockShow(c, u, &fl);
			if (fl & SOLCLOCK_NEW_ZONE)		++tr.zones;
			if (fl & SOLCLOCK_FAST)			++tr.fastSeen;
			f64	nx = soltime_clockNextCheckUtc(c);
			if (nx != lastNext)		{ ++tr.checks;  lastNext = nx; }
			if (fmod(t - utc0, 970.0) == 0.0)
				tr.gap = fmax(tr.gap, fabs(iSolTime_wrapDay(D - tTruth(r, lat, lon, u))));
		}
		soltime_clockDelete(c);
		return tr;
	}

	// A drive or flight:  legs of { seconds, turn deg/s }, a fix every second, compared every second
	struct SLeg { f64 secs, turn; };
	static STrack tTrip(SSolRegistry* r, f64 lat, f64 lon, f64 heading, f64 kmh, const SLeg* legs, s32 nLegs, f64 utc0)
	{
		STrack		tr = { 0.0, 0, 0, 0 };
		SSolClock*	c = soltime_clockCreate(NULL, r);
		f64			t = utc0, lastNext = NAN;
		for (s32 j = 0; j < nLegs; ++j)
		{
			for (s32 s = 0; s < (s32)legs[j].secs; ++s)
			{
				soltime_clockFix(c, t, lat, lon, kmh, heading);
				u32	fl;
				f64	D = soltime_clockShow(c, t, &fl);
				if (fl & SOLCLOCK_NEW_ZONE)		++tr.zones;
				if (fl & SOLCLOCK_FAST)			++tr.fastSeen;
				f64	nx = soltime_clockNextCheckUtc(c);
				if (nx != lastNext)		{ ++tr.checks;  lastNext = nx; }
				tr.gap = fmax(tr.gap, fabs(iSolTime_wrapDay(D - tTruth(r, lat, lon, t))));

				// Move one second along the current heading, then turn
				iSolTime_destination(lat, lon, kmh / 3600.0, heading, &lat, &lon);
				heading += legs[j].turn;
				t += 1.0;
			}
		}
		soltime_clockDelete(c);
		return tr;
	}

	static u32 test_clock(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		// Before any position
		SSolClock*	c = soltime_clockCreate(NULL, NULL);
		u32			fl;
		TCHECK(isnan(soltime_clockShow(c, 1000.0, &fl)) && fl == SOLCLOCK_NO_FIX,	"no position yet:  NAN + NO_FIX");
		soltime_clockDelete(c);

		// Standing still
		STrack	tr = tStill(NULL, 39.77, -86.16, 100.0 * 86400.0, 30.0 * 86400.0);
		TCHECK(tr.gap <= 2.0 * p.check_tolerance_secs && tr.zones == 0 && tr.fastSeen == 0,
																	TMSG("standing still 40N, 30 days:  within %.3f s of true, no jumps", tr.gap));
		TCHECK(tr.checks >= 280 * 30 && tr.checks <= 300 * 30,		TMSG("  checking about every 5 minutes (%.0f per day)", tr.checks / 30.0));
		tr = tStill(NULL, 70.0, 20.0, 180.0 * 86400.0, 30.0 * 86400.0);
		TCHECK(tr.gap <= 2.0 * p.check_tolerance_secs && tr.zones == 0,
																	TMSG("standing still 70N in summer, 30 days:  within %.3f s (%.0f checks per day)", tr.gap, tr.checks / 30.0));

		// 80 mph out of Indy's zone, east and west
		SSolZone		indy	= { 39.77, -86.16, 600.0, 0.0, -1.0 };
		SSolRegistry*	r		= soltime_registryCreate(&p, &indy, 1);
		SLeg			drive[]	= { { 3.0 * 3600.0, 0.0 } };
		tr = tTrip(r, 39.77, -86.16, 90.0, tMph80, drive, 1, 171.0 * 86400.0 + 14.0 * 3600.0);
		TCHECK(tr.gap <= 0.1 && tr.zones == 0,						TMSG("80 mph east out of Indy's zone, 3 h:  within %.3f s, no jumps (%.0f checks/h)", tr.gap, tr.checks / 3.0));
		tr = tTrip(r, 39.77, -86.16, 270.0, tMph80, drive, 1, 354.0 * 86400.0 + 14.0 * 3600.0);
		TCHECK(tr.gap <= 0.1 && tr.zones == 0,						TMSG("80 mph west, December:  within %.3f s, no jumps", tr.gap));

		// An airliner with turns (no zones):  fast-motion rule, re-aim on turns
		SLeg	route[] = { { 900.0, 0.0 }, { 60.0, -1.5 }, { 900.0, 0.0 }, { 30.0, -3.0 }, { 900.0, 0.0 }, { 120.0, 1.5 }, { 900.0, 0.0 } };
		f64		fl0[][2] = { { 39.77, -100.0 }, { 59.9, -30.0 }, { 70.0, -30.0 } };
		for (u32 j = 0; j < 3; ++j)
		{
			tr = tTrip(NULL, fl0[j][0], fl0[j][1], 90.0, 900.0, route, 7, 171.0 * 86400.0 + 12.0 * 3600.0);
			TCHECK(tr.gap <= 2.0 && tr.zones == 0 && tr.fastSeen > 0,
																	TMSG("900 km/h with 90/180-degree turns from %.0fN:  within %.2f s, no jumps, fast-motion rule on", fl0[j][0], tr.gap));
		}
		tr = tTrip(r, 39.77, -89.5, 90.0, 900.0, drive, 1, 171.0 * 86400.0 + 12.0 * 3600.0);
		TCHECK(tr.gap <= 6.0 && tr.zones == 0,						TMSG("900 km/h straight over Indy's zone:  within %.2f s", tr.gap));

		// A relocation:  a fix 1,000 km away is a new time zone; a short walk is not
		c = soltime_clockCreate(NULL, r);
		f64	t0 = 200.0 * 86400.0 + 10.0 * 3600.0;
		soltime_clockFix(c, t0, 39.77, -86.16, 0.0, 0.0);
		soltime_clockShow(c, t0, &fl);
		soltime_clockFix(c, t0 + 10.0, 39.771, -86.16, 0.0, 0.0);
		soltime_clockShow(c, t0 + 10.0, &fl);
		TCHECK(!(fl & SOLCLOCK_NEW_ZONE),							"a 100 m walk:  no note");
		soltime_clockFix(c, t0 + 20.0, 39.74, -104.99, 0.0, 0.0);
		f64	D = soltime_clockShow(c, t0 + 20.0, &fl);
		TCHECK((fl & SOLCLOCK_NEW_ZONE) && fabs(D - tTruth(r, 39.74, -104.99, t0 + 20.0)) < 1.0e-6,
																	"a fix in Denver:  'New Time Zone', showing Denver's true time at once");
		soltime_clockShow(c, t0 + 30.0, &fl);
		TCHECK(!(fl & SOLCLOCK_NEW_ZONE),							"the note shows once");

		// Exact mode:  the true value every call
		soltime_clockExact(c, 1);
		u32	off = 0;
		for (f64 t = t0 + 40.0; t < t0 + 4000.0; t += 37.0)
			TQUIET(fabs(soltime_clockShow(c, t, &fl) - tTruth(r, 39.74, -104.99, t)) < 1.0e-6, off);
		TCHECK(off == 0,											"exact mode:  the true value on every call");
		soltime_clockDelete(c);

		// No GPS in flight:  dead reckoning carries on; the fix that returns on the path changes nothing
		c = soltime_clockCreate(NULL, NULL);
		f64	la = 39.77, lo = -100.0, t = 171.0 * 86400.0 + 12.0 * 3600.0;
		soltime_clockFix(c, t, la, lo, 900.0, 90.0);
		f64	worst = 0.0;
		for (s32 s = 0; s < 1800; ++s)
		{
			f64	Ds = soltime_clockShow(c, t, &fl);
			worst = fmax(worst, fabs(Ds - tTruth(NULL, la, lo, t)));
			iSolTime_destination(39.77, -100.0, 900.0 * (t + 1.0 - (171.0 * 86400.0 + 12.0 * 3600.0)) / 3600.0, 90.0, &la, &lo);
			t += 1.0;
		}
		soltime_clockFix(c, t, la, lo, 900.0, 90.0);
		soltime_clockShow(c, t, &fl);
		TCHECK(worst < 1.0 && !(fl & SOLCLOCK_NEW_ZONE),			TMSG("30 min with no GPS at 900 km/h:  dead reckoning within %.3f s; the returning fix is no jump", worst));
		soltime_clockDelete(c);

		// New Year:  the display carries straight across the year's wrap
		c = soltime_clockCreate(NULL, NULL);
		soltime_clockFix(c, _SOLTIME_YEAR_SECS - 600.0, 39.77, -86.16, 0.0, 0.0);
		f64	before	= soltime_clockShow(c, _SOLTIME_YEAR_SECS - 30.0, &fl);
		f64	after	= soltime_clockShow(c, 30.0, &fl);
		TCHECK(fabs(iSolTime_wrapDay(after - before - 60.0)) < 0.5 && !(fl & SOLCLOCK_NEW_ZONE),
																	"New Year:  60 s later reads 60 s later, no jump");
		soltime_clockDelete(c);

		// Anyone may be on any zone:  a device in Denver follows Indianapolis's clock, and back
		SSolZone		two[2]	= { { 39.77, -86.16, 600.0, 0.0, -1.0, 887642.0, 0.0 }, { 39.74, -104.99, 600.0, 0.0, -1.0, 715522.0, 0.0 } };
		SSolRegistry*	r2		= soltime_registryCreate(&p, two, 2);
		c = soltime_clockCreate(NULL, r2);
		t0 = 220.0 * 86400.0 + 16.0 * 3600.0;
		soltime_clockFix(c, t0, 39.74, -104.99, 0.0, 0.0);
		D = soltime_clockShow(c, t0, &fl);
		TCHECK(fabs(D - (t0 + soltime_zoneOffsetSecs(r2, 1, t0 / 86400.0))) < 1.0e-6,	"a device in Denver shows Denver's clock");
		TCHECK(soltime_clockFollowZone(c, 0) == 0 && soltime_clockFollowing(c) == 0,	"it follows the Indianapolis zone");
		D = soltime_clockShow(c, t0 + 10.0, &fl);
		TCHECK((fl & SOLCLOCK_NEW_ZONE) && fabs(D - (t0 + 10.0 + soltime_zoneOffsetSecs(r2, 0, (t0 + 10.0) / 86400.0))) < 1.0e-6,
																	"  'New Time Zone', and Indianapolis's time at once, from Denver");
		worst = 0.0;
		for (f64 t = t0 + 20.0; t < t0 + 2.0 * 86400.0; t += 97.0)
			worst = fmax(worst, fabs(soltime_clockShow(c, t, &fl) - (t + soltime_zoneOffsetSecs(r2, 0, t / 86400.0))));
		TCHECK(worst < 2.0 * p.check_tolerance_secs,				TMSG("  two days on Indianapolis's clock from Denver:  within %.3f s", worst));
		TCHECK(soltime_clockFollowZone(c, -1) == 0 && soltime_clockFollowing(c) == -1,	"back to the clock of where it is");
		D = soltime_clockShow(c, t0 + 2.0 * 86400.0 + 10.0, &fl);
		TCHECK((fl & SOLCLOCK_NEW_ZONE) && fabs(D - tTruth(r2, 39.74, -104.99, t0 + 2.0 * 86400.0 + 10.0)) < 1.0e-6,
																	"  'New Time Zone', and Denver's time again");
		TCHECK(soltime_clockFollowZone(c, 5) == -2,					"following a zone that does not exist:  -2");
		soltime_clockDelete(c);
		soltime_registryDelete(r2);

		soltime_registryDelete(r);
		return gtFails;
	}


//////////
// Test:  /test:valid -- the no-prefix API layer validates (i/ii convention):  out-of-range
// inputs come back NAN / -2, never garbage; NULL params mean the defaults.
//////
	static u32 test_valid(void)
	{
		gtFails = 0;

		SSolTime	p;
		soltime_defaults(&p);

		TCHECK(isnan(soltime_offsetSecs(&p,  91.0,    0.0,   0.0)),	"lat 91 -> NAN");
		TCHECK(isnan(soltime_offsetSecs(&p,  40.0,  200.0,   0.0)),	"lon 200 -> NAN");
		TCHECK(isnan(soltime_offsetSecs(&p,  40.0,    0.0,  -1.0)),	"day -1 -> NAN");
		TCHECK(isnan(soltime_offsetSecs(&p,  40.0,    0.0, 366.0)),	"day 366 -> NAN");
		TCHECK(isnan(soltime_offsetSecs(&p,  NAN,     0.0,   0.0)),	"lat NAN -> NAN");

		SSolTime	bad = p;
		bad.max_slide_secs = 0.0;
		TCHECK(isnan(soltime_offsetSecs(&bad, 40.0, 0.0, 0.0)),		"max_slide 0 -> NAN");
		bad = p;  bad.anchor_secs = -1.0;
		TCHECK(isnan(soltime_offsetSecs(&bad, 40.0, 0.0, 0.0)),		"anchor -1 -> NAN");
		bad = p;  bad.zenith_deg = 200.0;
		TCHECK(isnan(soltime_offsetSecs(&bad, 40.0, 0.0, 0.0)),		"zenith 200 -> NAN");
		bad = p;  bad.basis_min_secs = 2.0 * 3600.0;
		TCHECK(isnan(soltime_offsetSecs(&bad, 40.0, 0.0, 0.0)),		"M = 2 h with 04:00 / 06:45 -> NAN (the §9 window rule)");
		bad = p;  bad.night_end_secs = 30000.0;
		TCHECK(isnan(soltime_offsetSecs(&bad, 40.0, 0.0, 0.0)),		"night end after the anchor -> NAN");
		bad = p;  bad.max_correction = 1.0;
		TCHECK(isnan(soltime_offsetSecs(&bad, 40.0, 0.0, 0.0)),		"max_correction 1 -> NAN");
		bad = p;  bad.check_max_secs = 0.5;
		TCHECK(soltime_clockCreate(&bad, NULL) == NULL,				"check_max under a second -> no clock");

		f64	rise, set, noon;
		TCHECK(soltime_sunTimesUtc(&p, 91.0, 0.0, 0.0, &rise, &set, &noon) == -2 && isnan(rise),
																	"sunTimesUtc lat 91 -> -2, NAN outs");
		TCHECK(isnan(soltime_localFromUtc(&p, 40.0, 0.0, -1.0)),	"localFromUtc utc -1 -> NAN");
		TCHECK(isnan(soltime_localFromUtc(&p, 40.0, 0.0, 366.0 * 86400.0)),
																	"localFromUtc utc past year end -> NAN");
		TCHECK(isnan(soltime_basisLatDeg(&p, 95.0, 1.0)),			"basisLatDeg lat 95 -> NAN");
		TCHECK(soltime_nightWindowUtc(&p, 40.0, 0.0, 400.0, NULL, NULL) == -2,	"nightWindowUtc day 400 -> -2");
		TCHECK(isnan(soltime_zoneRadiusKm(&p, 0.0, 40.0)),			"zoneRadiusKm quantum 0 -> NAN");
		TCHECK(isnan(soltime_zoneQuantumForRadius(&p, 10.0, 89.9)),	"zoneQuantumForRadius at 89.9N -> NAN");
		TCHECK(isfinite(soltime_offsetSecs(NULL, 40.0, 0.0, 0.0)),	"NULL params -> defaults, finite result");

		SSolClock*	c = soltime_clockCreate(NULL, NULL);
		TCHECK(soltime_clockFix(c, 10.0, 95.0, 0.0, 0.0, 0.0) == -2,	"clockFix lat 95 -> -2");
		TCHECK(soltime_clockFix(c, 10.0, 40.0, 0.0, -5.0, 0.0) == -2,	"clockFix negative speed -> -2");
		TCHECK(soltime_clockFix(NULL, 10.0, 40.0, 0.0, 0.0, 0.0) == -2,	"clockFix NULL clock -> -2");
		TCHECK(isnan(soltime_clockShow(c, -1.0, NULL)),				"clockShow utc -1 -> NAN");
		TCHECK(isnan(soltime_clockNextCheckUtc(c)),					"next check before the first show -> NAN");
		soltime_clockDelete(c);

		return gtFails;
	}


//////////
// Dispatch
//////
	// BUG_FIX_PLAN_2026-10-05.md FIX-05 — fixed 2026-10-05 16:48:37 EDT (UTC-04:00).
	struct STest
	{
		const char*	name;
		u32		(*fn)(void);
	};

	static STest gTests[] =
	{
		{ "noaa",		test_noaa      },
		{ "erosion",	test_erosion   },
		{ "anchor",		test_anchor    },
		{ "slide",		test_slide     },
		{ "travel",		test_travel    },
		{ "basis",		test_basis     },
		{ "polar",		test_polar     },
		{ "night",		test_night     },
		{ "localtime",	test_localtime },
		{ "zone",		test_zone     },
		{ "clock",		test_clock     },
		{ "valid",		test_valid     },
	};

	int main(int argc, char* argv[])
	{
		setvbuf(stdout, NULL, _IOLBF, 0);

		const char*	only = NULL;
		if (argc >= 2 && strncmp(argv[1], "/test:", 6) == 0)
			only = argv[1] + 6;

		u32	total = 0, ran = 0;
		for (u32 i = 0; i < sizeof(gTests) / sizeof(gTests[0]); ++i)
		{
			if (only && strcmp(only, gTests[i].name) != 0)
				continue;
			printf("[%s]\n", gTests[i].name);
			total += gTests[i].fn();
			++ran;
		}

		if (only && ran == 0)
		{
			printf("unknown test:  %s\n", only);
			return 1;
		}
		printf("\n%u test%s run, %u failure%s\n", ran, (ran == 1) ? "" : "s", total, (total == 1) ? "" : "s");
		return (total == 0) ? 0 : 1;
	}
