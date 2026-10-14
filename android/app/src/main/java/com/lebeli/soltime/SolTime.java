//////////
//
// SolTime.java -- Java port of soltime.cpp at the project root (see soltime_spec.md).  Pure Java (no
// Android imports), so the same file runs on the desktop for the C <-> Java differential test
// (difftest/run_difftest.sh at the project root).
//
// The one rule (§1):  offset = ANCHOR - sunriseUTC(basisLat).  Sunrise is held at the bus anchor
// (06:45); each day's offset holds through its daylight -- true seconds -- and the day-to-day
// change is absorbed at night (§9).  A circular slope-limited erosion (§3) caps the drift; polar
// places borrow the nearest latitude with real day and night (§4).  Zones lock a region to one
// clock (§8, Registry); the device clock follows the true clock (§10, Clock).
//
// Mirrors the C operation for operation (same expressions, same order) so the two agree to the
// last few ulps; the differential harness holds them to it.
//
//////
package com.lebeli.soltime;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Collections;
import java.util.Comparator;
import java.util.Locale;
import java.util.TimeZone;

public final class SolTime
{
	//////////
	// Constants
	//////
		public static final int		YEAR_DAYS		= 366;							// v1 periodic year (spec §2/§6)
		public static final double	DAY_SECS		= 86400.0;
		public static final double	YEAR_SECS		= 366.0 * 86400.0;
		private static final double	DEG2RAD			= Math.PI / 180.0;
		private static final double	RAD2DEG			= 180.0 / Math.PI;
		private static final double	EARTH_KM		= 6371.0;
		private static final double	KM_PER_DEG		= 6371.0 * Math.PI / 180.0;
		private static final double	SECS_PER_DEG	= 240.0;
		private static final double	RIM_STEP_DEG	= 5.0;
		private static final double	CHECK_MIN_SECS	= 0.1;
		private static final double	CATCHUP_SECS	= 60.0;


	//////////
	// Parameters (spec §5) -- the defaults
	//////
		public double	anchorSecs			= 24300.0;		// 06:45 -- light before the bus
		public double	maxSlideSecs		= 240.0;		// 4 min/day cap
		public double	zenithDeg			= 90.833;		// Official sunrise, incl. refraction
		public double	basisMinSecs		= 14400.0;		// 4 h of day AND night at the basis latitude
		public double	nightStartSecs		= 72000.0;		// 20:00
		public double	nightEndSecs		= 14400.0;		// 04:00
		public double	maxCorrection		= 0.5;			// The display runs 0.5x .. 1.5x at most
		public double	designSpeedKmh		= 128.75;		// 80 mph
		public double	rampMinSecsPerKm	= 2.0;			// Ramp slope floor
		public double	checkToleranceSecs	= 0.05;			// Auto-flex tolerance
		public double	checkMaxSecs		= 300.0;		// Auto-flex:  5 minutes standing still

		public SolTime copy()
		{
			SolTime c = new SolTime();
			c.anchorSecs			= anchorSecs;
			c.maxSlideSecs			= maxSlideSecs;
			c.zenithDeg				= zenithDeg;
			c.basisMinSecs			= basisMinSecs;
			c.nightStartSecs		= nightStartSecs;
			c.nightEndSecs			= nightEndSecs;
			c.maxCorrection			= maxCorrection;
			c.designSpeedKmh		= designSpeedKmh;
			c.rampMinSecsPerKm		= rampMinSecsPerKm;
			c.checkToleranceSecs	= checkToleranceSecs;
			c.checkMaxSecs			= checkMaxSecs;
			return c;
		}

		public boolean valid()
		{
			return (fin(anchorSecs)         && anchorSecs         >= 0.0 && anchorSecs   < DAY_SECS
				 && fin(maxSlideSecs)       && maxSlideSecs       >  0.0
				 && fin(zenithDeg)          && zenithDeg          >  0.0 && zenithDeg    < 180.0
				 && fin(basisMinSecs)       && basisMinSecs       >  0.0 && basisMinSecs < DAY_SECS * 0.5
				 && fin(nightStartSecs)     && fin(nightEndSecs)
				 && nightEndSecs >= 0.0     && nightEndSecs < anchorSecs
				 && nightStartSecs > anchorSecs && nightStartSecs < DAY_SECS
				 && basisMinSecs - (anchorSecs - nightEndSecs) > 2.0 * maxSlideSecs
				 && DAY_SECS - nightStartSecs + nightEndSecs > 3.0 * maxSlideSecs
				 && fin(maxCorrection)      && maxCorrection      >  0.0 && maxCorrection < 1.0
				 && fin(designSpeedKmh)     && designSpeedKmh     >  0.0
				 && fin(rampMinSecsPerKm)   && rampMinSecsPerKm   >  0.0
				 && fin(checkToleranceSecs) && checkToleranceSecs >  0.0
				 && fin(checkMaxSecs)       && checkMaxSecs       >= 1.0);
		}


	//////////
	// Small helpers
	//////
		private static boolean fin(double v)							{ return !Double.isNaN(v) && !Double.isInfinite(v); }
		private static int dayIndex(long k)								{ long m = k % YEAR_DAYS;  return (int)((m < 0) ? (m + YEAR_DAYS) : m); }
		private static double wrapDay(double secs)						{ return Math.IEEEremainder(secs, DAY_SECS); }
		private static double clamp(double v, double lo, double hi)		{ return (v < lo) ? lo : ((v > hi) ? hi : v); }

		private static boolean validLatLon(double lat, double lon)
		{
			return (fin(lat) && lat >= -90.0 && lat <= 90.0 && fin(lon) && lon >= -180.0 && lon <= 180.0);
		}

		private static boolean validDay(double day)
		{
			return (fin(day) && day >= 0.0 && day < (double)YEAR_DAYS);
		}

		// Great-circle distance (haversine)
		private static double iDistanceKm(double lat1, double lon1, double lat2, double lon2)
		{
			double p1 = lat1 * DEG2RAD;
			double p2 = lat2 * DEG2RAD;
			double sp = Math.sin((p2 - p1) * 0.5);
			double sl = Math.sin((lon2 - lon1) * DEG2RAD * 0.5);
			double h  = sp * sp + Math.cos(p1) * Math.cos(p2) * sl * sl;
			return 2.0 * EARTH_KM * Math.asin(Math.sqrt(clamp(h, 0.0, 1.0)));
		}

		// The point km away along a great circle at bearingDeg; out = { lat, lon }
		private static void destination(double latDeg, double lonDegE, double km, double bearingDeg, double[] out)
		{
			double d   = km / EARTH_KM;
			double p1  = latDeg * DEG2RAD;
			double th  = bearingDeg * DEG2RAD;
			double sp2 = clamp(Math.sin(p1) * Math.cos(d) + Math.cos(p1) * Math.sin(d) * Math.cos(th), -1.0, 1.0);
			double p2  = Math.asin(sp2);
			double l2  = lonDegE * DEG2RAD + Math.atan2(Math.sin(th) * Math.sin(d) * Math.cos(p1), Math.cos(d) - Math.sin(p1) * sp2);
			out[0] = p2 * RAD2DEG;
			out[1] = Math.IEEEremainder(l2 * RAD2DEG, 360.0);
		}

		private static double bearing(double lat1, double lon1, double lat2, double lon2)
		{
			double p1 = lat1 * DEG2RAD;
			double p2 = lat2 * DEG2RAD;
			double dl = (lon2 - lon1) * DEG2RAD;
			return Math.atan2(Math.sin(dl) * Math.cos(p2), Math.cos(p1) * Math.sin(p2) - Math.sin(p1) * Math.cos(p2) * Math.cos(dl)) * RAD2DEG;
		}


	//////////
	// NOAA solar engine (spec §2):  declination (radians) and equation of time (minutes), each
	// day's value taken at its midday, built once
	//////
		private static final double[]	DECL	= new double[YEAR_DAYS];
		private static final double[]	EQTIME	= new double[YEAR_DAYS];
		static
		{
			for (int i = 0; i < YEAR_DAYS; ++i)
			{
					double g = 2.0 * Math.PI * i / (double)YEAR_DAYS;
				DECL[i]		= 0.006918
							- 0.399912 * Math.cos(g)       + 0.070257 * Math.sin(g)
							- 0.006758 * Math.cos(2.0 * g) + 0.000907 * Math.sin(2.0 * g)
							- 0.002697 * Math.cos(3.0 * g) + 0.001480 * Math.sin(3.0 * g);
				EQTIME[i]	= 229.18 * (0.000075
							+ 0.001868 * Math.cos(g)       - 0.032077 * Math.sin(g)
							- 0.014615 * Math.cos(2.0 * g) - 0.040849 * Math.sin(2.0 * g));
			}
		}

		// The half-day hour angle (degrees), cosine clamped (§4); state[0] (nullable):  0 / +1 polar day / -1 polar night
		private static double hourAngle(double latDeg, double declRad, double zenithDeg, int[] state)
		{
			double lat   = latDeg * DEG2RAD;
			double cosHa = Math.cos(zenithDeg * DEG2RAD) / (Math.cos(lat) * Math.cos(declRad)) - Math.tan(lat) * Math.tan(declRad);
			int    st    = 0;
			if (cosHa >  1.0)	{ cosHa =  1.0;  st = -1; }
			if (cosHa < -1.0)	{ cosHa = -1.0;  st = +1; }
			if (state != null)	state[0] = st;
			return Math.acos(cosHa) * RAD2DEG;
		}


	//////////
	// The basis latitude (§4):  closed form for the latitude whose day is daySecs long
	//////
		private static double latForDayLen(double latDeg, double declRad, double zenithDeg, double daySecs)
		{
			double H0  = daySecs / DAY_SECS * Math.PI;
			double a   = Math.cos(declRad) * Math.cos(H0);
			double b   = Math.sin(declRad);
			double c   = Math.cos(zenithDeg * DEG2RAD);
			double ab  = Math.sqrt(a * a + b * b);
			double psi = Math.atan2(b, a);
			double q   = Math.acos(clamp(c / ab, -1.0, 1.0));
			double r1  = Math.IEEEremainder((psi + q) * RAD2DEG, 360.0);
			double r2  = Math.IEEEremainder((psi - q) * RAD2DEG, 360.0);
			double lo  = Math.min(0.0, latDeg);
			double hi  = Math.max(0.0, latDeg);
			if (r1 >= lo && r1 <= hi)		return r1;
			if (r2 >= lo && r2 <= hi)		return r2;
			double d1 = (r1 < lo) ? (lo - r1) : (r1 - hi);
			double d2 = (r2 < lo) ? (lo - r2) : (r2 - hi);
			return clamp((d1 <= d2) ? r1 : r2, lo, hi);
		}

		// The hour angle at the BASIS latitude on day index m; basisLat[0] (nullable) gets it
		private double basisHa(double latDeg, int m, double[] basisLat)
		{
			return basisHaDecl(latDeg, DECL[m], basisLat);
		}

		double basisHaDecl(double latDeg, double decl, double[] basisLat)
		{
			double ha = hourAngle(latDeg, decl, zenithDeg, null);
			double D  = 480.0 * ha;
			double bl = latDeg;
			if (D < basisMinSecs)
			{
				bl = latForDayLen(latDeg, decl, zenithDeg, basisMinSecs);
				ha = basisMinSecs / 480.0;

			} else if (DAY_SECS - D < basisMinSecs) {
				bl = latForDayLen(latDeg, decl, zenithDeg, DAY_SECS - basisMinSecs);
				ha = (DAY_SECS - basisMinSecs) / 480.0;
			}
			if (basisLat != null)	basisLat[0] = bl;
			return ha;
		}


	//////////
	// The year's offset table at lon 0 (§3):  target eroded by the slope-limited cone, O(n)
	//////
		private static void erode(double[] s, double slide)
		{
			for (int lap = 0; lap < 2; ++lap)
				for (int i = 0; i < YEAR_DAYS; ++i)
				{
					double v = s[(i + YEAR_DAYS - 1) % YEAR_DAYS] + slide;
					if (v < s[i])	s[i] = v;
				}
			for (int lap = 0; lap < 2; ++lap)
				for (int i = YEAR_DAYS - 1; i >= 0; --i)
				{
					double v = s[(i + 1) % YEAR_DAYS] + slide;
					if (v < s[i])	s[i] = v;
				}
		}

		private void yearTable(double latDeg, double[] sm)
		{
			for (int i = 0; i < YEAR_DAYS; ++i)
				sm[i] = anchorSecs - (720.0 - 4.0 * basisHa(latDeg, i, null) - EQTIME[i]) * 60.0;
			erode(sm, maxSlideSecs);
		}

		private static double dayOffset(double[] sm, double lonDegE, long k)
		{
			return sm[dayIndex(k)] + SECS_PER_DEG * lonDegE;
		}


	//////////
	// The night window between local day k and k + 1 (§9), absolute UTC seconds; out = { ws, we }
	//////
		private void window(double[] sm, double latDeg, double lonDegE, long k, double[] out)
		{
			int    m0      = dayIndex(k);
			int    m1      = dayIndex(k + 1);
			double ha0     = basisHa(latDeg, m0, null);
			double ha1     = basisHa(latDeg, m1, null);
			double sunset  = (double)k       * DAY_SECS + (720.0 - 4.0 * (lonDegE - ha0) - EQTIME[m0]) * 60.0;
			double sunrise = (double)(k + 1) * DAY_SECS + (720.0 - 4.0 * (lonDegE + ha1) - EQTIME[m1]) * 60.0;
			double t20     = (double)k       * DAY_SECS + nightStartSecs - dayOffset(sm, lonDegE, k);
			double t04     = (double)(k + 1) * DAY_SECS + nightEndSecs   - dayOffset(sm, lonDegE, k + 1);
			double ws      = Math.max(sunset,  t20);
			double we      = Math.min(sunrise, t04);
			if (we - ws < 1.0)	{ ws = t20;  we = t04; }
			out[0] = ws;
			out[1] = we;
		}

		// The offset at UTC instant t for a place whose year table is sm (§9)
		private double offsetAt(double[] sm, double latDeg, double lonDegE, double t)
		{
			long     kU = (long)Math.floor(t / DAY_SECS);
			long     k0 = (long)Math.floor((t + dayOffset(sm, lonDegE, kU)) / DAY_SECS);
			double[] ws = new double[5], we = new double[5], w2 = new double[2];
			for (int j = 0; j < 5; ++j)
			{
				window(sm, latDeg, lonDegE, k0 - 2 + j, w2);
				ws[j] = w2[0];
				we[j] = w2[1];
			}
			for (int j = 0; j < 5; ++j)
			{
				if (t >= ws[j] && t < we[j])
				{
					long   k = k0 - 2 + j;
					double a = dayOffset(sm, lonDegE, k);
					double b = dayOffset(sm, lonDegE, k + 1);
					double x = (t - ws[j]) / (we[j] - ws[j]);
					return a + (b - a) * (x - Math.sin(2.0 * Math.PI * x) / (2.0 * Math.PI));
				}
			}
			for (int j = 1; j < 5; ++j)
				if (t >= we[j - 1] && t < ws[j])
					return dayOffset(sm, lonDegE, k0 - 2 + j);
			return Double.NaN;
		}

		private double fieldAt(double latDeg, double lonDegE, double t)
		{
			double[] sm = new double[YEAR_DAYS];
			yearTable(latDeg, sm);
			return offsetAt(sm, latDeg, lonDegE, t);
		}


	//////////
	// API -- the field
	//////
		// UTC -> local offset in seconds for this place at this moment.  NaN on invalid input.
		public double offsetSecs(double latDeg, double lonDegE, double day)
		{
			if (!valid() || !validLatLon(latDeg, lonDegE) || !validDay(day))		return Double.NaN;
			return fieldAt(latDeg, lonDegE, day * DAY_SECS);
		}

		// Local seconds-of-year from UTC seconds-of-year (raw:  near the date line it can leave [0, YEAR_SECS))
		public double localFromUtc(double latDeg, double lonDegE, double utcSecsOfYear)
		{
			if (!fin(utcSecsOfYear) || utcSecsOfYear < 0.0 || utcSecsOfYear >= YEAR_SECS)	return Double.NaN;
			return utcSecsOfYear + offsetSecs(latDeg, lonDegE, utcSecsOfYear / DAY_SECS);
		}

		// The place's own raw sun, UTC seconds of day floor(day); out = { rise, set, noon }.
		// 0 normal, +1 polar day, -1 polar night, -2 invalid (outs NaN).
		public int sunTimesUtc(double latDeg, double lonDegE, double day, double[] out)
		{
			if (!valid() || !validLatLon(latDeg, lonDegE) || !validDay(day))
			{
				out[0] = out[1] = out[2] = Double.NaN;
				return -2;
			}
			int    m  = (int)Math.floor(day);
			int[]  st = new int[1];
			double ha = hourAngle(latDeg, DECL[m], zenithDeg, st);
			out[0] = (720.0 - 4.0 * (lonDegE + ha) - EQTIME[m]) * 60.0;
			out[1] = (720.0 - 4.0 * (lonDegE - ha) - EQTIME[m]) * 60.0;
			out[2] = (720.0 - 4.0 *  lonDegE       - EQTIME[m]) * 60.0;
			return st[0];
		}

		// The latitude whose daylight this place uses on day floor(day) (§4).  NaN on invalid.
		public double basisLatDeg(double latDeg, double day)
		{
			if (!valid() || !validLatLon(latDeg, 0.0) || !validDay(day))	return Double.NaN;
			double[] bl = new double[1];
			basisHa(latDeg, (int)Math.floor(day), bl);
			return bl[0];
		}

		// Tonight's adjustment window after local day floor(day), UTC seconds of year; out = { start, end }.  0 ok, -2 invalid.
		public int nightWindowUtc(double latDeg, double lonDegE, double day, double[] out)
		{
			if (!valid() || !validLatLon(latDeg, lonDegE) || !validDay(day))
			{
				out[0] = out[1] = Double.NaN;
				return -2;
			}
			double[] sm = new double[YEAR_DAYS];
			yearTable(latDeg, sm);
			window(sm, latDeg, lonDegE, (long)Math.floor(day), out);
			return 0;
		}

		public static double distanceKm(double lat1, double lon1, double lat2, double lon2)
		{
			if (!fin(lat1) || !fin(lon1) || !fin(lat2) || !fin(lon2))	return Double.NaN;
			return iDistanceKm(lat1, lon1, lat2, lon2);
		}


	//////////
	// The honest radius (§8):  golden-section refinement between sampled bearings
	//////
		private interface BearingFn		{ double at(double brg); }

		private static double goldenMax(double a, double b, BearingFn f)
		{
			final double gr = 0.6180339887498949;
			double x1 = b - gr * (b - a), x2 = a + gr * (b - a);
			double f1 = f.at(x1),         f2 = f.at(x2);
			double best = Math.max(f1, f2);
			for (int it = 0; it < 24; ++it)
			{
				if (f1 >= f2)	{ b = x2;  x2 = x1;  f2 = f1;  x1 = b - gr * (b - a);  f1 = f.at(x1);  best = Math.max(best, f1); }
				else			{ a = x1;  x1 = x2;  f1 = f2;  x2 = a + gr * (b - a);  f2 = f.at(x2);  best = Math.max(best, f2); }
			}
			return best;
		}

		private double circleError(final double latDeg, final double[] smc, final double km, final double[] sm2)
		{
			double   worst = 0.0, bestBrg = 0.0;
			int      steps = (int)(180.0 / RIM_STEP_DEG);
			double[] ll    = new double[2];
			for (int a = 0; a <= steps; ++a)
			{
				double e1 = 0.0, e2 = 0.0;
				destination(latDeg, 0.0, km, (double)a * RIM_STEP_DEG, ll);
				yearTable(ll[0], sm2);
				for (int k = 0; k < YEAR_DAYS; ++k)
				{
					double d = smc[k] - sm2[k];
					e1 = Math.max(e1, Math.abs(wrapDay(d - SECS_PER_DEG * ll[1])));
					e2 = Math.max(e2, Math.abs(wrapDay(d + SECS_PER_DEG * ll[1])));
				}
				if (e1 > worst)		{ worst = e1;  bestBrg = (double)a * RIM_STEP_DEG; }
				if (e2 > worst)		{ worst = e2;  bestBrg = 360.0 - (double)a * RIM_STEP_DEG; }
			}

			BearingFn f = new BearingFn()
			{
				@Override public double at(double brg)
				{
					double[] p = new double[2];
					double   w = 0.0;
					destination(latDeg, 0.0, km, brg, p);
					yearTable(p[0], sm2);
					for (int k = 0; k < YEAR_DAYS; ++k)
						w = Math.max(w, Math.abs(wrapDay(smc[k] - sm2[k] - SECS_PER_DEG * p[1])));
					return w;
				}
			};
			return Math.max(worst, goldenMax(bestBrg - RIM_STEP_DEG, bestBrg + RIM_STEP_DEG, f));
		}

		private double honestRadius(double latDeg, double quantumSecs)
		{
			double[] smc   = new double[YEAR_DAYS], sm2 = new double[YEAR_DAYS];
			double   half  = quantumSecs * 0.5;
			double   maxKm = Math.PI * EARTH_KM;
			yearTable(latDeg, smc);

			double lo = 0.0, flo = -half;
			double hi = Math.max(1.0e-3, (half / SECS_PER_DEG) * KM_PER_DEG * Math.cos(latDeg * DEG2RAD) * 1.05);
			double fhi = circleError(latDeg, smc, hi, sm2) - half;
			while (fhi <= 0.0)
			{
				lo  = hi;
				flo = fhi;
				if (hi >= maxKm)	return maxKm;
				hi  = Math.min(maxKm, hi * 1.5);
				fhi = circleError(latDeg, smc, hi, sm2) - half;
			}

			int side = 0;
			for (int it = 0; it < 100 && hi - lo > 1.0e-4; ++it)
			{
				double r = (lo * fhi - hi * flo) / (fhi - flo);
				if (!(r > lo && r < hi))	r = 0.5 * (lo + hi);
				double fr = circleError(latDeg, smc, r, sm2) - half;
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

		// The honest auto radius for a width class at a latitude.  NaN on invalid.
		public double zoneRadiusKm(double quantumSecs, double latDeg)
		{
			if (!valid() || !fin(quantumSecs) || quantumSecs <= 0.0 || !validLatLon(latDeg, 0.0))	return Double.NaN;
			return honestRadius(latDeg, quantumSecs);
		}

		// The smallest standard class whose honest radius covers radiusKm; beyond 1200 s, whole minutes.  NaN on invalid.
		public double zoneQuantumForRadius(double radiusKm, double latDeg)
		{
			if (!valid() || !fin(radiusKm) || radiusKm <= 0.0 || !fin(latDeg) || Math.abs(latDeg) > 89.5)	return Double.NaN;
			final double[] std = { 60.0, 300.0, 600.0, 1200.0 };
			for (double q : std)
				if (honestRadius(latDeg, q) >= radiusKm)	return q;

			long lo = 20, hi = 40;
			while (honestRadius(latDeg, (double)hi * 60.0) < radiusKm)
			{
				lo = hi;
				hi *= 2;
				if (hi > 1440)	return Double.NaN;
			}
			while (hi - lo > 1)
			{
				long mid = (lo + hi) / 2;
				if (honestRadius(latDeg, (double)mid * 60.0) >= radiusKm)	hi = mid;
				else														lo = mid;
			}
			return (double)hi * 60.0;
		}


	//////////
	// Zone (§8):  a city's disc -- 1.5x the city's radius -- locked to SolTime.  Zones whose discs
	// overlap merge into one area on the clock of their most populous city.
	//////
		public static final class Zone
		{
			public final double latDeg, lonDegE, quantumSecs, radiusKm, rampKm, population, jumpSecs;

			// BUG_FIX_PLAN_2026-10-05.md FIX-03 — contract updated 2026-10-05 16:54:43 EDT (UTC-04:00).
			// radiusKm <= 0 = auto (the honest radius of quantumSecs); rampKm < 0 = auto, 0 = hard edge,
			// > 0 = requested width, capped at earlier discs; population: the most populous leads (ties: registry
			// order);  jumpSecs <= 0 = compute, > 0 = as given (a published registry carries it)
			public Zone(double latDeg, double lonDegE, double quantumSecs, double radiusKm, double rampKm, double population, double jumpSecs)
			{
				this.latDeg      = latDeg;
				this.lonDegE     = lonDegE;
				this.quantumSecs = quantumSecs;
				this.radiusKm    = radiusKm;
				this.rampKm      = rampKm;
				this.population  = population;
				this.jumpSecs    = jumpSecs;
			}

			public Zone(double latDeg, double lonDegE, double quantumSecs, double radiusKm, double rampKm)
			{
				this(latDeg, lonDegE, quantumSecs, radiusKm, rampKm, 0.0, 0.0);
			}
		}

		// Build a registry: overlapping discs merged, jumps and ramps computed once. Given jumps are reused;
		// positive ramps are capped at the gap to earlier discs (FIX-03 above). null on invalid input.
		public Registry registryCreate(Zone[] zones)
		{
			if (!valid() || zones == null)		return null;
			for (Zone z : zones)
			{
				if (z == null || !validLatLon(z.latDeg, z.lonDegE)
				 || !fin(z.radiusKm)  || (z.radiusKm <= 0.0 && (!fin(z.quantumSecs) || z.quantumSecs <= 0.0))
				 || !fin(z.rampKm)    || !fin(z.population) || z.population < 0.0 || !fin(z.jumpSecs))
					return null;
			}
			Registry r = new Registry(copy(), zones);
			r.iBuild();
			return r;
		}


	//////////
	// The registry (§8).  Zones whose discs overlap merge, transitively, into one group:  the union of
	// their discs, on the clock of the group's most populous zone (its leader).  Groups apply in layer
	// order -- the most populous leader first -- each capping its lead over the clock beneath it (the
	// field plus every group before it).
	//////
		public static final class Registry
		{
			final SolTime		p;
			final int			n;
			final Zone[]		zones;
			final double[]		lockKm;			// Per zone:  its disc's radius (resolved)
			final int[]			leader;			// Per zone:  its group's leader
			final int[]			layerOf;		// Per zone:  its group's layer position
			int					nGroups;
			final int[]			order;			// Per layer position:  the group's leader
			final int[]			memberStart;	// Per layer position:  its first entry in members[]  (nGroups + 1 entries)
			final int[]			members;		// Zone indices, grouped by layer position
			final double[]		rampKm, jump, steep;	// Per leader (steep:  NaN until needed)
			final double[][]	tables;			// Per leader:  the year table at its center (the group's clock)
			final int[]			latOrder;		// Zone indices by latitude (the neighbor search)
			double				maxLock, maxReach;
			final double[]		gradCache;		// The field's steepest slope per 0.1 degree of latitude (NaN = not yet)

			Registry(SolTime p, Zone[] src)
			{
				this.p				= p;
				this.n				= src.length;
				this.zones			= src.clone();
				this.lockKm			= new double[n];
				this.leader			= new int[n];
				this.layerOf		= new int[n];
				this.order			= new int[n];
				this.memberStart	= new int[n + 1];
				this.members		= new int[n];
				this.rampKm			= new double[n];
				this.jump			= new double[n];
				this.steep			= new double[n];
				this.tables			= new double[n][];
				this.latOrder		= new int[n];
				this.gradCache		= new double[1801];
				Arrays.fill(gradCache, Double.NaN);
			}

			private static int iFind(int[] parent, int x)
			{
				while (parent[x] != x)
				{
					parent[x] = parent[parent[x]];
					x = parent[x];
				}
				return x;
			}

			private void iBuild()
			{
				// Discs (a given radius, or the honest radius of the class)
				for (int i = 0; i < n; ++i)
				{
					lockKm[i]	= (zones[i].radiusKm > 0.0) ? zones[i].radiusKm : p.honestRadius(zones[i].latDeg, zones[i].quantumSecs);
					steep[i]	= Double.NaN;
					maxLock		= Math.max(maxLock, lockKm[i]);
				}
				Integer[] lo = new Integer[n];
				for (int i = 0; i < n; ++i)		lo[i] = i;
				Arrays.sort(lo, new Comparator<Integer>()			// Stable
				{
					@Override public int compare(Integer a, Integer b)	{ return (zones[a].latDeg < zones[b].latDeg) ? -1 : ((zones[a].latDeg > zones[b].latDeg) ? 1 : 0); }
				});
				for (int i = 0; i < n; ++i)		latOrder[i] = lo[i];

				// Merge:  discs that overlap join one group (transitively)
				int[] parent = new int[n];
				for (int i = 0; i < n; ++i)		parent[i] = i;
				for (int a = 0; a < n; ++a)
				{
					int i = latOrder[a];
					for (int b = a + 1; b < n; ++b)
					{
						int j = latOrder[b];
						if ((zones[j].latDeg - zones[i].latDeg) * KM_PER_DEG > lockKm[i] + maxLock)
							break;
						if (iDistanceKm(zones[i].latDeg, zones[i].lonDegE, zones[j].latDeg, zones[j].lonDegE) < lockKm[i] + lockKm[j])
						{
							int x = iFind(parent, i), y = iFind(parent, j);
							if (x != y)		parent[x] = y;
						}
					}
				}

				// Each group's leader:  its most populous zone (ties:  registry order)
				Arrays.fill(leader, -1);
				for (int i = 0; i < n; ++i)
				{
					int root = iFind(parent, i);
					int L    = leader[root];
					if (L < 0 || zones[i].population > zones[L].population)
						leader[root] = i;
				}
				for (int i = 0; i < n; ++i)
					leader[i] = leader[iFind(parent, i)];

				// Layer order:  the most populous leader first (ties:  registry order);  members by group
				ArrayList<Integer> lead = new ArrayList<Integer>();
				for (int i = 0; i < n; ++i)
					if (leader[i] == i)		lead.add(i);
				Collections.sort(lead, new Comparator<Integer>()		// Stable
				{
					@Override public int compare(Integer a, Integer b)	{ return (zones[a].population > zones[b].population) ? -1 : ((zones[a].population < zones[b].population) ? 1 : 0); }
				});
				nGroups = lead.size();
				for (int pos = 0; pos < nGroups; ++pos)		{ order[pos] = lead.get(pos);  layerOf[order[pos]] = pos; }
				for (int i = 0; i < n; ++i)					layerOf[i] = layerOf[leader[i]];
				int[] count = new int[nGroups + 1];
				for (int i = 0; i < n; ++i)					++count[layerOf[i] + 1];
				for (int pos = 0; pos < nGroups; ++pos)		memberStart[pos + 1] = memberStart[pos] + count[pos + 1];
				for (int pos = 0; pos < nGroups; ++pos)		count[pos] = memberStart[pos];
				for (int i = 0; i < n; ++i)					members[count[layerOf[i]]++] = i;

				// Each group's clock:  the year table at its leader
				for (int pos = 0; pos < nGroups; ++pos)
				{
					int L = order[pos];
					tables[L] = new double[YEAR_DAYS];
					p.yearTable(zones[L].latDeg, tables[L]);
				}

				// Jumps and ramps, in layer order (each measured against the clock beneath it)
				for (int pos = 0; pos < nGroups; ++pos)
				{
					int L = order[pos];
					jump[L] = (zones[L].jumpSecs > 0.0) ? zones[L].jumpSecs : iJump(pos);
					iSizeRamp(pos);
					for (int j = memberStart[pos]; j < memberStart[pos + 1]; ++j)
						maxReach = Math.max(maxReach, lockKm[members[j]] + rampKm[L]);
				}
			}

			// Zones whose centers lie within reachKm of a latitude (the caller still checks the real distance)
			private int[] iNear(double latDeg, double reachKm)
			{
				double dLat = reachKm / KM_PER_DEG + 1.0e-9;
				int    lo   = 0, hi = n;
				while (lo < hi)
				{
					int mid = (lo + hi) >>> 1;
					if (zones[latOrder[mid]].latDeg < latDeg - dLat)	lo = mid + 1;
					else												hi = mid;
				}
				int end = lo;
				while (end < n && zones[latOrder[end]].latDeg <= latDeg + dLat)
					++end;
				return Arrays.copyOfRange(latOrder, lo, end);
			}

			//////////
			// Evaluating the layered clock.  A point is PREPARED once against the first nLayers groups --
			// its field table, and for every group touching it, the touch:  in the lock (inside any member's
			// disc), or in the ramp of one or more members, each with its fraction across the ramp and the
			// rim point on the same bearing prepared beneath that group -- and then evaluated for any day
			// (daily values) or instant.  In a ramp the cap on the lead falls linearly from max(J, |the lead
			// at that member's rim, right now|) to 0; a group's cap is the largest over its members.
			//////
				private static final class Step
				{
					int			leader;
					int			nMembers;			// 0 = in the lock
					int[]		member;
					double[]	frac;				// 0 at the rim
					Prep[]		rim;
				}

				private static final class Prep
				{
					double		lat, lon;
					double[]	sm;
					Step[]		steps;
				}

				private static final class Hit
				{
					final int		layer, zone;
					final double	frac;			// < 0 = in the lock
					Hit(int layer, int zone, double frac)	{ this.layer = layer;  this.zone = zone;  this.frac = frac; }
				}

				private Prep iPrepare(int nLayers, double latDeg, double lonDegE, double[] smKnown)
				{
					Prep pp	= new Prep();
					pp.lat	= latDeg;
					pp.lon	= lonDegE;
					if (smKnown != null)	pp.sm = smKnown.clone();
					else					{ pp.sm = new double[YEAR_DAYS];  p.yearTable(latDeg, pp.sm); }
					if (nLayers <= 0 || n <= 0)		{ pp.steps = new Step[0];  return pp; }

					// The touches, from the zones near enough
					ArrayList<Hit> hit = new ArrayList<Hit>();
					for (int e : iNear(latDeg, maxReach))
					{
						if (layerOf[e] >= nLayers)	continue;
						double ramp = rampKm[leader[e]];
						double d    = iDistanceKm(latDeg, lonDegE, zones[e].latDeg, zones[e].lonDegE);
						if (d <= lockKm[e])								hit.add(new Hit(layerOf[e], e, -1.0));
						else if (ramp > 0.0 && d < lockKm[e] + ramp)	hit.add(new Hit(layerOf[e], e, (d - lockKm[e]) / ramp));
					}
					Collections.sort(hit, new Comparator<Hit>()
					{
						@Override public int compare(Hit a, Hit b)	{ return (a.layer != b.layer) ? Integer.compare(a.layer, b.layer) : Integer.compare(a.zone, b.zone); }
					});

					// One step per group touched:  the lock if any member's disc holds the point, else the ramp
					ArrayList<Step> steps = new ArrayList<Step>();
					double[] rl = new double[2];
					for (int j = 0; j < hit.size(); )
					{
						int     end    = j;
						boolean locked = false;
						while (end < hit.size() && hit.get(end).layer == hit.get(j).layer)
						{
							if (hit.get(end).frac < 0.0)	locked = true;
							++end;
						}
						Step st	= new Step();
						st.leader	= order[hit.get(j).layer];
						if (!locked)
						{
							st.nMembers	= end - j;
							st.member	= new int[st.nMembers];
							st.frac		= new double[st.nMembers];
							st.rim		= new Prep[st.nMembers];
							for (int m = 0; m < st.nMembers; ++m)
							{
								Hit  h = hit.get(j + m);
								Zone z = zones[h.zone];
								destination(z.latDeg, z.lonDegE, lockKm[h.zone], bearing(z.latDeg, z.lonDegE, latDeg, lonDegE), rl);
								st.member[m]	= h.zone;
								st.frac[m]		= h.frac;
								st.rim[m]		= iPrepare(hit.get(j).layer, rl[0], rl[1], null);
							}
						}
						steps.add(st);
						j = end;
					}
					pp.steps = steps.toArray(new Step[0]);
					return pp;
				}

				// A group's own clock (its leader's center):  daily = day k's values;  else instant t
				private double iGroupValue(int L, boolean daily, long k, double t, boolean calendar)
				{
					Zone z = zones[L];
					if (calendar) return p.offsetUnixSecs(z.latDeg, z.lonDegE, t);
					return daily ? dayOffset(tables[L], z.lonDegE, k) : p.offsetAt(tables[L], z.latDeg, z.lonDegE, t);
				}

				private double iPrepValue(Prep pp, boolean daily, long k, double t) {
                    return iPrepValue(pp, daily, k, t, false);
                }

                private double iPrepValue(Prep pp, boolean daily, long k, double t, boolean calendar)
				{
					double clock = calendar ? p.offsetUnixSecs(pp.lat, pp.lon, t)
					    : daily ? dayOffset(pp.sm, pp.lon, k) : p.offsetAt(pp.sm, pp.lat, pp.lon, t);
					for (Step st : pp.steps)
					{
						double Oe   = iGroupValue(st.leader, daily, k, t, calendar);
						double lead = wrapDay(Oe - clock);				// Zones lock the time of day (§8)
						if (st.nMembers == 0)
						{
							clock += lead;								// In the lock:  exactly the group's clock
							continue;
						}
						double cap = 0.0;
						for (int m = 0; m < st.nMembers; ++m)
						{
							double rimLead = Math.abs(wrapDay(Oe - iPrepValue(st.rim[m], daily, k, t, calendar)));
							cap = Math.max(cap, Math.max(jump[st.leader], rimLead) * (1.0 - st.frac[m]));
						}
						clock += clamp(lead, -cap, cap);
					}
					return clock;
				}

				double iLayeredAt(double[] sm, double latDeg, double lonDegE, double t)
				{
					return iPrepValue(iPrepare(nGroups, latDeg, lonDegE, sm), false, 0, t);
				}

			//////////
			// J for the group at layer position pos:  its largest lead over the clock beneath anywhere in
			// its discs, all year.  Every member's rim, every 5 degrees, refined around the worst bearing;
			// rings inside a disc too where an earlier group reaches into it.
			//////
				private double iLeadAt(int pos, int member, double km, double brg)
				{
					int      L     = order[pos];
					Zone     m     = zones[member];
					double[] ll    = new double[2];
					double   worst = 0.0;
					destination(m.latDeg, m.lonDegE, km, brg, ll);
					Prep pp = iPrepare(pos, ll[0], ll[1], null);
					for (long k = 0; k < YEAR_DAYS; ++k)
						worst = Math.max(worst, Math.abs(wrapDay(dayOffset(tables[L], zones[L].lonDegE, k) - iPrepValue(pp, true, k, 0.0))));
					return worst;
				}

				private boolean iReachedBy(int pos, int m)
				{
					for (int e : iNear(zones[m].latDeg, lockKm[m] + maxReach))
					{
						if (layerOf[e] >= pos)	continue;
						double d = iDistanceKm(zones[m].latDeg, zones[m].lonDegE, zones[e].latDeg, zones[e].lonDegE);
						if (d < lockKm[m] + lockKm[e] + rampKm[leader[e]])		return true;
					}
					return false;
				}

				private double iJump(final int pos)
				{
					final double[] rings = { 1.0, 0.75, 0.5, 0.25, 0.0 };
					int    steps   = (int)(360.0 / RIM_STEP_DEG);
					double J       = 0.0, bestBrg = 0.0, bestKm = 0.0;
					int    bestM   = -1;
					for (int j = memberStart[pos]; j < memberStart[pos + 1]; ++j)
					{
						int m      = members[j];
						int nRings = iReachedBy(pos, m) ? rings.length : 1;
						for (int g = 0; g < nRings; ++g)
						{
							double km = lockKm[m] * rings[g];
							for (int a = 0; a < ((rings[g] > 0.0) ? steps : 1); ++a)
							{
								double v = iLeadAt(pos, m, km, (double)a * RIM_STEP_DEG);
								if (v > J)	{ J = v;  bestM = m;  bestKm = km;  bestBrg = (double)a * RIM_STEP_DEG; }
							}
						}
					}
					if (bestM < 0)	return J;
					final int    m  = bestM;
					final double km = bestKm;
					return Math.max(J, goldenMax(bestBrg - RIM_STEP_DEG, bestBrg + RIM_STEP_DEG, new BearingFn()
					{
						@Override public double at(double brg)	{ return iLeadAt(pos, m, km, brg); }
					}));
				}

			//////////
			// Ramp sizing (the drive budget)
			//////
				// The field's steepest slope (s/km), any direction, any day, at grid latitude k (-90 + 0.1 k)
				private double iGradAt(int k)
				{
					if (!Double.isNaN(gradCache[k]))	return gradCache[k];
					final double h   = 0.01;
					double       lat = -90.0 + 0.1 * (double)k;
					double[]     smA = new double[YEAR_DAYS], smB = new double[YEAR_DAYS];
					double       gE  = SECS_PER_DEG / (KM_PER_DEG * Math.cos(lat * DEG2RAD));
					double       g   = 0.0;
					p.yearTable(lat,     smA);
					p.yearTable(lat + h, smB);
					for (int d = 0; d < YEAR_DAYS; ++d)
					{
						double gN = (smB[d] - smA[d]) / (h * KM_PER_DEG);
						g = Math.max(g, Math.sqrt(gE * gE + gN * gN));
					}
					gradCache[k] = g;
					return g;
				}

				private double iFieldSteepest(double lat0, double lat1)
				{
					int k0 = (int)Math.floor((lat0 + 90.0) / 0.1);
					int k1 = (int)Math.ceil ((lat1 + 90.0) / 0.1);
					if (k0 < 1)		k0 = 1;
					if (k1 > 1799)	k1 = 1799;
					double g = 0.0;
					for (int k = k0; k <= k1; ++k)
						g = Math.max(g, iGradAt(k));
					return g;
				}

				// The steepest slope beneath the group at pos, over its members' rings [R, R + ramp]
				private double iBeneathSteepest(int pos, double ramp)
				{
					double lat0 = 90.0, lat1 = -90.0, g = 0.0;
					for (int j = memberStart[pos]; j < memberStart[pos + 1]; ++j)
					{
						int    m    = members[j];
						double span = (lockKm[m] + ramp) / KM_PER_DEG;
						lat0 = Math.min(lat0, zones[m].latDeg - span);
						lat1 = Math.max(lat1, zones[m].latDeg + span);
						for (int e : iNear(zones[m].latDeg, lockKm[m] + ramp + maxReach))
						{
							if (layerOf[e] >= pos)	continue;
							int    Le = leader[e];
							double d  = iDistanceKm(zones[m].latDeg, zones[m].lonDegE, zones[e].latDeg, zones[e].lonDegE);
							if (rampKm[Le] > 0.0 && d < lockKm[m] + ramp + lockKm[e] + rampKm[Le])
								g = Math.max(g, iSteepOf(Le));
						}
					}
					return Math.max(g, iFieldSteepest(lat0, lat1));
				}

				// An earlier group's steepest ramp slope, worked out when first needed
				private double iSteepOf(int L)
				{
					if (Double.isNaN(steep[L]))
						steep[L] = (rampKm[L] > 0.0) ? (iBeneathSteepest(layerOf[L], rampKm[L]) + jump[L] / rampKm[L]) : 0.0;
					return steep[L];
				}

				private double iNightRate(double[] sm, double latDeg, double lonDegE)
				{
					double   worst = 0.0;
					double[] w     = new double[2];
					for (long k = 0; k < YEAR_DAYS; ++k)
					{
						p.window(sm, latDeg, lonDegE, k, w);
						worst = Math.max(worst, 2.0 * Math.abs(dayOffset(sm, lonDegE, k + 1) - dayOffset(sm, lonDegE, k)) / (w[1] - w[0]));
					}
					return worst;
				}

				// The shortest gap from the group at pos to any earlier group's disc (a ramp stops there)
				private double iGapToEarlier(int pos, double ramp)
				{
					double gap = ramp;
					for (int j = memberStart[pos]; j < memberStart[pos + 1]; ++j)
					{
						int m = members[j];
						for (int e : iNear(zones[m].latDeg, lockKm[m] + ramp + maxLock))
						{
							if (layerOf[e] >= pos)	continue;
							double g = iDistanceKm(zones[m].latDeg, zones[m].lonDegE, zones[e].latDeg, zones[e].lonDegE) - lockKm[m] - lockKm[e];
							gap = Math.min(gap, Math.max(g, 0.0));
						}
					}
					return gap;
				}

				private void iSizeRamp(int pos)
				{
					int    L = order[pos];
					Zone   z = zones[L];
					double J = jump[L];
					if (z.rampKm == 0.0 || J <= 0.0)	{ rampKm[L] = 0.0;			steep[L] = 0.0;			return; }
					// BUG_FIX_PLAN_2026-10-05.md FIX-03 — fixed 2026-10-05 16:43:53 EDT (UTC-04:00).
					// Explicit widths must not reach into an earlier zone's locked disc.
					if (z.rampKm > 0.0) { rampKm[L] = iGapToEarlier(pos, z.rampKm); steep[L] = Double.NaN; return; }

					// Auto:  from the shortest ramp the budget could allow, grow the ring until the slope it
					// implies fits inside the ring it was measured on
					double budget = (p.maxCorrection - iNightRate(tables[L], z.latDeg, z.lonDegE)) / (p.designSpeedKmh / 3600.0);
					double span   = J / Math.max(budget, p.rampMinSecsPerKm);
					for (int it = 0; it < 16; ++it)
					{
						double gb    = iBeneathSteepest(pos, span);
						double slope = Math.max(budget - gb, p.rampMinSecsPerKm);
						double ramp  = J / slope;
						if (ramp <= span || it == 15)
						{
							ramp = (ramp <= span) ? ramp : (J / p.rampMinSecsPerKm);
							ramp = iGapToEarlier(pos, ramp);
							rampKm[L]	= ramp;
							steep[L]	= (ramp > 0.0) ? (gb + J / ramp) : gb;
							return;
						}
						span = ramp;
					}
				}

			//////////
			// Registry API
			//////
				public int		count()				{ return n; }

				public double offsetUnixSecs(double lat, double lon, double utc) {
					if (!validLatLon(lat, lon) || !SolarCalendar.validTime(utc)) return Double.NaN;
					return iPrepValue(iPrepare(nGroups, lat, lon, null), false, 0, utc, true);
				}
				public double zoneOffsetUnixSecs(int i, double utc) {
					if (i < 0 || i >= n || !SolarCalendar.validTime(utc)) return Double.NaN;
					Zone z = zones[leader[i]];
					return p.offsetUnixSecs(z.latDeg, z.lonDegE, utc);
				}
				public int		groupCount()		{ return nGroups; }
				public Zone		zone(int i)			{ return (i >= 0 && i < n) ? zones[i] : null; }
				public double	lockKm(int i)		{ return (i >= 0 && i < n) ? lockKm[i] : Double.NaN; }
				public double	rampKm(int i)		{ return (i >= 0 && i < n) ? rampKm[leader[i]] : Double.NaN; }
				public double	jumpSecs(int i)		{ return (i >= 0 && i < n) ? jump[leader[i]]   : Double.NaN; }
				public int		leader(int i)		{ return (i >= 0 && i < n) ? leader[i] : -2; }

				// Every merged zone covering the point (its leader), in layer order (last = on top):  the
				// total count; fills up to idx.length entries (weight:  1 = locked, falling to 0 across the
				// ramp; nullable).  -2 on invalid.
				public int zonesAt(double latDeg, double lonDegE, int[] idx, double[] weight)
				{
					if (!validLatLon(latDeg, lonDegE) || idx == null)	return -2;
					ArrayList<Hit> hit = new ArrayList<Hit>();
					for (int e : iNear(latDeg, maxReach))
					{
						double ramp = rampKm[leader[e]];
						double d    = iDistanceKm(latDeg, lonDegE, zones[e].latDeg, zones[e].lonDegE);
						if (d <= lockKm[e])								hit.add(new Hit(layerOf[e], e, 0.0));
						else if (ramp > 0.0 && d < lockKm[e] + ramp)	hit.add(new Hit(layerOf[e], e, (d - lockKm[e]) / ramp));
					}
					Collections.sort(hit, new Comparator<Hit>()
					{
						@Override public int compare(Hit a, Hit b)	{ return (a.layer != b.layer) ? Integer.compare(a.layer, b.layer) : Double.compare(a.frac, b.frac); }
					});

					// One entry per group:  the weight of its deepest member
					int count = 0;
					for (int j = 0; j < hit.size(); ++j)
					{
						if (j > 0 && hit.get(j).layer == hit.get(j - 1).layer)		continue;
						if (count < idx.length)
						{
							idx[count] = order[hit.get(j).layer];
							if (weight != null && count < weight.length)	weight[count] = 1.0 - hit.get(j).frac;
						}
						++count;
					}
					return count;
				}

				// The clock of zone i's merged zone, anywhere.  NaN on invalid.
				public double zoneOffsetSecs(int i, double day)
				{
					if (i < 0 || i >= n || !validDay(day))		return Double.NaN;
					int L = leader[i];
					return p.offsetAt(tables[L], zones[L].latDeg, zones[L].lonDegE, day * DAY_SECS);
				}

				// The layered clock at a point.  NaN on invalid.
				public double offsetSecsR(double latDeg, double lonDegE, double day)
				{
					if (!validLatLon(latDeg, lonDegE) || !validDay(day))	return Double.NaN;
					double[] sm = new double[YEAR_DAYS];
					p.yearTable(latDeg, sm);
					return iLayeredAt(sm, latDeg, lonDegE, day * DAY_SECS);
				}
		}


	//////////
	// The device clock (§10)
	//////
		public Clock clockCreate(Registry r)
		{
			return valid() ? new Clock(copy(), r) : null;
		}

		// Real-calendar clock: fix/show/nextCheckUtc all use Unix seconds on this instance.
		public Clock clockCreateUnix(Registry r) {
			Clock c = clockCreate(r);
			if (c != null) c.unixTime = true;
			return c;
		}

		public double offsetUnixSecs(double lat, double lon, double utc) {
			return valid() && validLatLon(lat, lon) && SolarCalendar.validTime(utc)
			    ? SolarCalendar.offset(this, lat, lon, utc) : Double.NaN;
		}
		// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
		// Choose solar events by this location's raw date, independently of a followed zone.
		public double solarDateUnix(double lat, double lon, double utc) {
			double date = Math.floor((utc + offsetUnixSecs(lat, lon, utc)) / DAY_SECS) * DAY_SECS;
			return SolarCalendar.validTime(date) ? date : Double.NaN;
		}
		public int sunTimesUnix(double lat, double lon, double date, double[] out) {
			if (!valid() || !validLatLon(lat, lon) || !SolarCalendar.validTime(date)) {
				if (out != null) Arrays.fill(out, Double.NaN);
				return -2;
			}
			long k = (long)Math.floor(date / DAY_SECS);
			double[] sun = SolarCalendar.solar(k);
			int[] state = new int[1];
			double ha = hourAngle(lat, sun[0], zenithDeg, state);
			double noon = k * DAY_SECS + (720 - 4 * lon - sun[1]) * 60;
			if (out != null) { out[0] = noon - ha * 240; out[1] = noon + ha * 240; out[2] = noon; }
			return state[0];
		}
		public double basisLatUnix(double lat, double date) {
			if (!valid() || !validLatLon(lat, 0) || !SolarCalendar.validTime(date)) return Double.NaN;
			double[] out = new double[1];
			basisHaDecl(lat, SolarCalendar.solar((long)Math.floor(date / DAY_SECS))[0], out);
			return out[0];
		}
		public int nightWindowUnix(double lat, double lon, double date, double[] out) {
			if (!valid() || !validLatLon(lat, lon) || !SolarCalendar.validTime(date)) {
				if (out != null) Arrays.fill(out, Double.NaN);
				return -2;
			}
			long k = (long)Math.floor(date / DAY_SECS);
			SolarCalendar.window(this, SolarCalendar.table(this, lat, SolarCalendar.yearOf(k)), lat, lon, k, out);
			return 0;
		}

		public static final class Clock
		{
			public static final int	NEW_ZONE	= 0x01;		// The display jumped (more than a minute's correction)
			public static final int	FAST		= 0x02;		// Moving faster than the correction can follow
			public static final int	NO_FIX		= 0x04;		// No position yet (show returns NaN)

			final SolTime	p;
			final Registry	r;
			boolean			exact	= false;
			int				follow	= -1;			// A zone leader to follow wherever the device is; -1 = by position
			long			lap		= 0;
			double			lastSeen = Double.NEGATIVE_INFINITY;
			boolean unixTime, hasShowLon, boundaryExact;
			double lastShowLon;

			boolean			hasFix	= false;
			double			fixUtc, fixLat, fixLon, fixSpeed, fixHeading;
			double			planUtc, planLat, planLon, planSpeed, planHeading;

			boolean			shown	= false;
			double			dispUtc0, disp0, rate = 1.0;
			double			nextCheck, interval;
			boolean			replan	= false;
			boolean			fast	= false;
			int				latched	= 0;

			boolean			cacheValid = false;
			double			cacheLat;
			final double[]	cacheTable = new double[YEAR_DAYS];

			Clock(SolTime p, Registry r)
			{
				this.p			= p;
				this.r			= r;
				this.interval	= p.checkMaxSecs;
			}

			private double iUnwrap(double utc)
			{
				if (unixTime) return utc;
				double t = utc + (double)lap * YEAR_SECS;
				if (t < lastSeen - YEAR_SECS * 0.5)
				{
					++lap;
					t += YEAR_SECS;
				}
				if (t > lastSeen)	lastSeen = t;
				return t;
			}

			private void iPos(double t, double[] ll)
			{
				double km = fixSpeed * (t - fixUtc) / 3600.0;
				if (km == 0.0)	{ ll[0] = fixLat;  ll[1] = fixLon;  return; }
				destination(fixLat, fixLon, km, fixHeading, ll);
			}

			private double iTruth(double t)
			{
				SolTime  q  = (r != null) ? r.p : p;
				double   tw = t - Math.floor(t / YEAR_SECS) * YEAR_SECS;		// The year is periodic
				if (follow >= 0)
				{
					Zone z = r.zones[follow];
					if (unixTime) return t + q.offsetUnixSecs(z.latDeg, z.lonDegE, t);
					return t + q.offsetAt(r.tables[follow], z.latDeg, z.lonDegE, tw);
				}
				double[] ll = new double[2];
				iPos(t, ll);
				if (unixTime) return t + (r != null ? r.offsetUnixSecs(ll[0], ll[1], t) : q.offsetUnixSecs(ll[0], ll[1], t));
				if (!cacheValid || cacheLat != ll[0])
				{
					q.yearTable(ll[0], cacheTable);
					cacheLat	= ll[0];
					cacheValid	= true;
				}
				double off = (r != null) ? r.iLayeredAt(cacheTable, ll[0], ll[1], tw) : q.offsetAt(cacheTable, ll[0], ll[1], tw);
				return t + off;
			}

			private void iPlanFix()
			{
				planUtc		= fixUtc;
				planLat		= fixLat;
				planLon		= fixLon;
				planSpeed	= fixSpeed;
				planHeading	= fixHeading;
				replan		= false;
			}

			private static boolean iOutside(double rate, double mc)		{ return (rate < 1.0 - mc || rate > 1.0 + mc); }

			private double iSnap(double T, double Dnow)
			{
				if (Math.abs(T - Dnow) > p.maxCorrection * 60.0)	latched |= NEW_ZONE;
				return T;
			}

			private void iCheckpoint(double t, int why)
			{
				double  mc   = p.maxCorrection;
				double  T    = iTruth(t);
				double oneSecond = iTruth(t + 1.0) - T;
				boolean fst = Math.abs(oneSecond - 1) < 43200 && iOutside(oneSecond, mc);
				boundaryExact = false;
				double  Dnow = (why == 0) ? T : (disp0 + rate * (t - dispUtc0));

				// BUG_FIX_PLAN_2026-10-05.md FIX-01 — fixed 2026-10-05 16:43:53 EDT (UTC-04:00).
				// Keep the deadline only for a small correction during an existing flight.
				if (why == 2 && fst && fast && Math.abs(T - Dnow) <= mc * 60.0
				    && nextCheck - t >= 1.0 && nextCheck - t <= Math.min(60.0, p.checkMaxSecs)
				    && Math.abs(iTruth(nextCheck) - T - (nextCheck - t)) < 43200)
				{
					rate		= (iTruth(nextCheck) - Dnow) / (nextCheck - t);
					disp0		= Dnow;
					dispUtc0	= t;
					fast		= true;
					iPlanFix();
					return;
				}

				if (why != 0)
				{
					if (fst)
					{
						Dnow = iSnap(T, Dnow);

					} else {
						double T60 = iTruth(t + 60.0);
						if (iOutside((T60 - Dnow) / 60.0, mc) && !iOutside((T60 - T) / 60.0, mc))
							Dnow = iSnap(T, Dnow);
					}
				}

				double hMax = fst ? Math.min(60.0, p.checkMaxSecs) : p.checkMaxSecs;
				double h    = Math.min(interval, hMax);
				double aim, err;
				for (;;)
				{
					double Th  = iTruth(t + h);
					if (Math.abs(Th - T - h) > 43200) {
						if (h > CHECK_MIN_SECS) { h = Math.max(CHECK_MIN_SECS, h * 0.5); continue; }
						boundaryExact = true; Dnow = T; aim = 1; err = p.checkToleranceSecs; break;
					}
					double raw = (Th - Dnow) / h;
					if (!fst)
					{
						if (iOutside((Th - T) / h, mc))
						{
							if (h > CHECK_MIN_SECS)	{ h = Math.max(CHECK_MIN_SECS, h * 0.5);  continue; }
							fst		= true;
							hMax	= Math.min(60.0, p.checkMaxSecs);
							Dnow	= iSnap(T, Dnow);
							raw		= (Th - Dnow) / h;

						} else if (iOutside(raw, mc)) {
							if (h > 60.0)	{ h = 60.0;  continue; }
							aim = clamp(raw, 1.0 - mc, 1.0 + mc);
							err = 0.0;
							break;
						}
					}
					aim = raw;
					err = Math.abs(Dnow + aim * h * 0.5 - iTruth(t + h * 0.5));
					if (err > p.checkToleranceSecs && h > CHECK_MIN_SECS)
					{
						h = Math.max(CHECK_MIN_SECS, h * 0.5);
						continue;
					}
					break;
				}

				fast		= fst;
				interval	= (err < p.checkToleranceSecs * 0.25) ? Math.min(hMax, h * 2.0) : h;
				disp0		= Dnow;
				dispUtc0	= t;
				rate		= aim;
				nextCheck	= t + h;
				shown		= true;
				iPlanFix();
			}

			// Feed a position fix (GPS or typed in):  speed km/h, heading degrees clockwise from north.  0 ok, -2 invalid.
			private boolean validUtc(double utc) {
				return unixTime ? SolarCalendar.validTime(utc) : fin(utc) && utc >= 0 && utc < YEAR_SECS;
			}
			public int fix(double utcSecsOfYear, double latDeg, double lonDegE, double speedKmh, double headingDeg)
			{
				if (!validUtc(utcSecsOfYear)
				 || !validLatLon(latDeg, lonDegE) || !fin(speedKmh) || speedKmh < 0.0 || !fin(headingDeg))
					return -2;

				double t	= iUnwrap(utcSecsOfYear);
				fixUtc		= t;
				fixLat		= latDeg;
				fixLon		= lonDegE;
				fixSpeed	= speedKmh;
				fixHeading	= Math.IEEEremainder(headingDeg, 360.0);
				hasFix		= true;

				if (shown)
				{
					double[] pl = { planLat, planLon };
					double   km = planSpeed * (t - planUtc) / 3600.0;
					if (km != 0.0)	destination(planLat, planLon, km, planHeading, pl);
					double off = iDistanceKm(pl[0], pl[1], latDeg, lonDegE);
					double dh  = Math.abs(Math.IEEEremainder(fixHeading - planHeading, 360.0));
					if (off > 0.05 || Math.abs(speedKmh - planSpeed) > 1.0 || (speedKmh > 1.0 && dh > 2.0))
						replan = true;
				}
				return 0;
			}

			// The local seconds-of-year to display now (raw); flags[0] (nullable) gets NEW_ZONE / FAST / NO_FIX
			public double show(double utcSecsOfYear, int[] flags)
			{
				if (flags != null)	flags[0] = 0;
				if (!validUtc(utcSecsOfYear)) return Double.NaN;
				if (!hasFix && follow < 0)
				{
					if (flags != null)	flags[0] = NO_FIX;
					return Double.NaN;
				}

				double t		= iUnwrap(utcSecsOfYear);
				double wrapTo	= unixTime ? 0 : (double)lap * YEAR_SECS;
				if (follow < 0) {
					double[] ll = new double[2]; iPos(t, ll);
					if (hasShowLon && Math.abs(ll[1] - lastShowLon) > 180) { latched |= NEW_ZONE; shown = false; }
					lastShowLon = ll[1]; hasShowLon = true;
				}

				if (exact)
				{
					double T = iTruth(t);
					if (shown && Math.abs((T - disp0) - (t - dispUtc0)) > p.maxCorrection * (60.0 + Math.abs(t - dispUtc0)))
						latched |= NEW_ZONE;
					shown		= true;
					disp0		= T;
					dispUtc0	= t;
					rate		= 1.0;
					nextCheck	= t;
					replan		= false;
					if (flags != null) { flags[0] = latched; }
					latched		= 0;
					return T - wrapTo;
				}

				if (!shown || t < dispUtc0)
				{
					iCheckpoint(t, 0);

				} else {
					if (t - nextCheck > CATCHUP_SECS)	iCheckpoint(t, 1);
					else while (t >= nextCheck)			iCheckpoint(nextCheck, 1);
					if (replan)							iCheckpoint(t, 2);
				}

				if (flags != null)	flags[0] = latched | (fast ? FAST : 0);
				latched = 0;
				return (boundaryExact ? iTruth(t) : disp0 + rate * (t - dispUtc0)) - wrapTo;
			}

			public void exact(boolean on)	{ exact = on; }

			// Follow zone i's clock wherever the device is (anyone may be on any zone);  -1 = back to the
			// clock of where the device is.  0 ok, -2 invalid (a zone needs the registry).
			public int followZone(int zone)
			{
				hasShowLon = false;
				if (zone < 0)							{ follow = -1;  replan = true;  return 0; }
				if (r == null || zone >= r.n)			return -2;
				follow = r.leader[zone];
				replan = true;
				return 0;
			}

			// The leader followed, or -1
			public int following()		{ return follow; }

			// When the next checkpoint is due (UTC seconds of year); NaN before the first show
			public double nextCheckUtc()	{ return shown ? (nextCheck - (double)lap * YEAR_SECS) : Double.NaN; }
		}


	//////////
	// Android-side helpers (not in the C ABI)
	//////
		// UTC seconds-of-year (0.0 = Jan 1 00:00:00.000 UTC) from epoch milliseconds
		public static double utcSecsOfYear(long epochMillis)
		{
			Calendar c = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
			c.setTimeInMillis(epochMillis);
			return (c.get(Calendar.DAY_OF_YEAR) - 1) * DAY_SECS
				 +  c.get(Calendar.HOUR_OF_DAY) * 3600.0
				 +  c.get(Calendar.MINUTE)      * 60.0
				 +  c.get(Calendar.SECOND)
				 +  c.get(Calendar.MILLISECOND) / 1000.0;
		}

		// Epoch milliseconds of Jan 1 00:00 UTC of the year containing epochMillis
		public static long yearStartMillis(long epochMillis)
		{
			Calendar c = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
			c.setTimeInMillis(epochMillis);
			int y = c.get(Calendar.YEAR);
			c.clear();
			c.set(y, Calendar.JANUARY, 1, 0, 0, 0);
			return c.getTimeInMillis();
		}

		// The subsolar point now (drives the globe's terminator); out = { latDeg, lonDegE }
		public static void subsolar(double utcSecsOfYear, double[] out)
		{
			int    m          = dayIndex((long)Math.floor(utcSecsOfYear / DAY_SECS));
			double minutesUtc = (utcSecsOfYear % DAY_SECS) / 60.0;
			double lon        = (720.0 - minutesUtc - EQTIME[m]) / 4.0;
			while (lon >  180.0)	lon -= 360.0;
			while (lon < -180.0)	lon += 360.0;
			out[0] = Math.toDegrees(DECL[m]);
			out[1] = lon;
		}

		public static void subsolarUnix(double utc, double[] out) {
			long k = (long)Math.floor(utc / DAY_SECS);
			double[] sun = SolarCalendar.solar(k);
			out[0] = Math.toDegrees(sun[0]);
			out[1] = Math.IEEEremainder((720 - (utc - k * DAY_SECS) / 60 - sun[1]) / 4, 360);
		}

		// "HH:MM:SS" of a local seconds value (wraps into one civil day)
		public static String formatHMS(double localSecs)
		{
			int t = (int)Math.floor(((localSecs % DAY_SECS) + DAY_SECS) % DAY_SECS);
			return String.format(Locale.US, "%02d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
		}

		// "+HH:MM" / "-HH:MM" of an offset
		public static String formatOffset(double offsetSecs)
		{
			int m = (int)Math.round(offsetSecs / 60.0);
			return String.format(Locale.US, "%s%02d:%02d", (m < 0) ? "-" : "+", Math.abs(m) / 60, Math.abs(m) % 60);
		}
}
