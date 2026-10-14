//////////
//
// difftest.cpp -- the C side of the C <-> Java differential test (run_difftest.sh).
//
// Prints one line per query:  TAG <index> <values...>.  DiffTest.java asks the identical
// questions of SolTime.java; compare.pl holds the two outputs together.
//
//////
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <math.h>
	#include "../soltime.h"

	static const f64	cgLats[] = { -89.5, -60.0, -45.3, -20.0, 0.0, 12.7, 39.77, 51.5, 59.9, 64.1, 69.6, 78.2, 89.9 };
	static const f64	cgLons[] = { -179.9, -105.0, -86.16, 0.0, 18.95, 151.2, 179.9 };
	static const f64	cgDays[] = { 0.05, 79.6, 80.95, 171.3, 264.85, 354.2, 365.97 };
	#define NELEM(a)	((s32)(sizeof(a) / sizeof((a)[0])))

	// Load zones.tsv (name country admin1 lat lon population city_radius_km radius_source
	// zone_radius_km ramp_km jump_s zone):  the published registry, and each row's country
	// BUG_FIX_PLAN_2026-10-05.md FIX-05 — fixed 2026-10-05 16:48:37 EDT (UTC-04:00).
	static s32 iLoadZones(const char* path, SSolZone** out, char (**country)[4])
	{
		FILE*	f = fopen(path, "r");
		if (!f)		return -1;
		char	line[4096];
		s32		n = 0, cap = 0;
		SSolZone*	b = NULL;
		char		(*cc)[4] = NULL;
		while (fgets(line, sizeof(line), f))
		{
			if (line[0] == '#' || line[0] == '\n')	continue;
			char*	fld[12];
			s32		nf = 0;
			fld[nf++] = line;
			for (char* p = line; *p && nf < 12; ++p)
				if (*p == '\t')		{ *p = 0;  fld[nf++] = p + 1; }
			if (nf < 11)	continue;
			if (n == cap)
			{
				cap = cap ? cap * 2 : 1024;
				SSolZone*	nb = new SSolZone[cap];
				char		(*nc)[4] = new char[cap][4];
				for (s32 i = 0; i < n; ++i)		{ nb[i] = b[i];  memcpy(nc[i], cc[i], 4); }
				delete[] b;
				delete[] cc;
				b  = nb;
				cc = nc;
			}
			snprintf(cc[n], 4, "%s", fld[1]);
			b[n].latDeg			= atof(fld[3]);
			b[n].lonDegE		= atof(fld[4]);
			b[n].quantumSecs	= 0.0;
			b[n].radiusKm		= atof(fld[8]);
			b[n].rampKm			= (strcmp(fld[9],  "-") == 0) ? -1.0 : atof(fld[9]);
			b[n].population		= atof(fld[5]);
			b[n].jumpSecs		= (strcmp(fld[10], "-") == 0) ?  0.0 : atof(fld[10]);
			++n;
		}
		fclose(f);
		*out		= b;
		*country	= cc;
		return n;
	}

	// The layered clock and the zones around zone i:  in its disc, across its ramp, beyond
	static void iAround(const char* tagL, const char* tagZ, SSolRegistry* r, const SSolZone* z, s32 i, s32* q)
	{
		f64	brgs[] = { 0.0, 135.0, 250.0 }, days[] = { 80.7, 171.2, 354.9 };
		f64	R = soltime_zoneLockKm(r, i), ramp = soltime_zoneRampKm(r, i);
		f64	dist[] = { 0.0, 0.5 * R, R - 0.01, R + 0.3 * ramp, R + 0.9 * ramp, R + ramp + 1.0 };
		for (s32 a = 0; a < NELEM(dist); ++a)
			for (s32 b = 0; b < NELEM(brgs); ++b)
			{
				// Destination point (spherical)
				f64	d = dist[a] / 6371.0, p1 = z->latDeg * M_PI / 180.0, th = brgs[b] * M_PI / 180.0;
				f64	sp2 = sin(p1) * cos(d) + cos(p1) * sin(d) * cos(th);
				f64	la = asin(sp2) * 180.0 / M_PI;
				f64	lo = remainder(z->lonDegE + atan2(sin(th) * sin(d) * cos(p1), cos(d) - sin(p1) * sp2) * 180.0 / M_PI, 360.0);
				for (s32 k = 0; k < NELEM(days); ++k)
					printf("%s %d %.17e\n", tagL, (*q)++, soltime_offsetSecsR(r, la, lo, days[k]));
				s32	idx[8];
				f64	w[8];
				s32	nz = soltime_zonesAt(r, la, lo, idx, w, 8);
				printf("%s %d %d", tagZ, *q - 1, nz);
				for (s32 j = 0; j < nz && j < 8; ++j)
					printf(" %d %.17e", idx[j], w[j]);
				printf("\n");
			}
	}

	// The device clock on a path:  legs of { seconds, turn deg/s }, a fix and a show every second
	static void iTrip(const char* tag, SSolRegistry* r, f64 lat, f64 lon, f64 heading, f64 kmh, const f64 (*legs)[2], s32 nLegs, f64 utc0, s32 every, f64 jumpAt, f64 jLat, f64 jLon, s32 follow = -1, bool unixTime = false)
	{
		SSolClock*	c = unixTime ? soltime_clockCreateUnix(NULL, r) : soltime_clockCreate(NULL, r);
		if (follow >= 0)	soltime_clockFollowZone(c, follow);
		f64			t = utc0;
		s32			s = 0;
		for (s32 j = 0; j < nLegs; ++j)
		{
			for (s32 i = 0; i < (s32)legs[j][0]; ++i, ++s)
			{
				if (jumpAt >= 0.0 && s == (s32)jumpAt)	{ lat = jLat;  lon = jLon; }
				soltime_clockFix(c, (unixTime ? t : fmod(t, 366.0 * 86400.0)), lat, lon, kmh, heading);
				u32	fl;
				f64	D = soltime_clockShow(c, (unixTime ? t : fmod(t, 366.0 * 86400.0)), &fl);
				if (s % every == 0)
					printf("%s %d %.17e %u %.17e\n", tag, s, D, fl, soltime_clockNextCheckUtc(c));

				// One second along the heading (spherical), then turn
				f64	d = kmh / 3600.0 / 6371.0, p1 = lat * M_PI / 180.0, th = heading * M_PI / 180.0;
				f64	sp2 = sin(p1) * cos(d) + cos(p1) * sin(d) * cos(th);
				f64	nl = asin(sp2) * 180.0 / M_PI;
				f64	no = lon + atan2(sin(th) * sin(d) * cos(p1), cos(d) - sin(p1) * sp2) * 180.0 / M_PI;
				lat = nl;
				lon = remainder(no, 360.0);
				heading += legs[j][1];
				t += 1.0;
			}
		}
		soltime_clockDelete(c);
	}

	int main(int argc, char* argv[])
	{
		const char*	zonesPath = (argc > 1) ? argv[1] : "../data/zones.tsv";
		s32		q = 0;

		// The field
		for (s32 a = 0; a < NELEM(cgLats); ++a)
			for (s32 b = 0; b < NELEM(cgLons); ++b)
				for (s32 d = 0; d < NELEM(cgDays); ++d)
					printf("FIELD %d %.17e\n", q++, soltime_offsetSecs(NULL, cgLats[a], cgLons[b], cgDays[d]));

		// Sun, basis, night window
		q = 0;
		for (s32 a = 0; a < NELEM(cgLats); ++a)
			for (s32 d = 0; d < NELEM(cgDays); ++d)
			{
				f64	r, s, n, ws, we;
				s32	st = soltime_sunTimesUtc(NULL, cgLats[a], cgLons[a % NELEM(cgLons)], cgDays[d], &r, &s, &n);
				soltime_nightWindowUtc(NULL, cgLats[a], cgLons[a % NELEM(cgLons)], cgDays[d], &ws, &we);
				printf("SUN %d %d %.17e %.17e %.17e %.17e %.17e %.17e\n", q++, st, r, s, n,
					   soltime_basisLatDeg(NULL, cgLats[a], cgDays[d]), ws, we);
			}

        // Common years, leap days, the real New Year and century rules.
        q = 0;
        const f64 dates[] = {1767225600., 1798761599., 1798761600., 1835395200., 1861920000., 4133980800.};
        for (f64 t : dates) for (f64 lat : cgLats) for (f64 lon : cgLons) {
            f64 rise, set, noon, ws, we;
            s32 state = soltime_sunTimesUnix(NULL, lat, lon, t, &rise, &set, &noon);
            soltime_nightWindowUnix(NULL, lat, lon, t, &ws, &we);
            printf("CALENDAR %d %.17e %d %.17e %.17e %.17e %.17e %.17e %.17e\n", q++,
                soltime_offsetUnixSecs(NULL, lat, lon, t), state, rise, set, noon, ws, we, soltime_basisLatUnix(NULL, lat, t));
        }
        const f64 shortTrip[][2] = {{120, 0}};
        iTrip("UNIXNEWYEAR", NULL, 40, -86, 90, 128.75, shortTrip, 1, 1798761540., 1, -1, 0, 0, -1, true);
        iTrip("UNIXDATELINE", NULL, 70, 179.99, 90, 900, shortTrip, 1, 1774180800., 1, -1, 0, 0, -1, true);

		// Honest radii
		f64	qs[] = { 60.0, 300.0, 600.0, 1200.0 }, rl[] = { 0.0, 23.5, 40.0, 55.0, 60.0, 64.0, 70.0 };
		q = 0;
		for (s32 a = 0; a < NELEM(qs); ++a)
			for (s32 b = 0; b < NELEM(rl); ++b)
				printf("RADIUS %d %.17e\n", q++, soltime_zoneRadiusKm(NULL, qs[a], rl[b]));

		// The published registry (data/zones.tsv, given jumps and ramps):  every zone's disc, its merged
		// zone's ramp and jump, and its leader (the merge);  the layered clock around a spread of zones
		SSolZone*	zones	= NULL;
		char		(*country)[4] = NULL;
		s32			nb		= iLoadZones(zonesPath, &zones, &country);
		SSolRegistry* r = (nb > 0) ? soltime_registryCreate(NULL, zones, nb) : NULL;
		printf("REGISTRY %d %d\n", nb, r ? 1 : 0);
		if (r)
		{
			for (s32 i = 0; i < nb; ++i)
				printf("ZONE %d %.17e %.17e %.17e %d\n", i, soltime_zoneLockKm(r, i), soltime_zoneRampKm(r, i), soltime_zoneJumpSecs(r, i), soltime_zoneLeader(r, i));

			q = 0;
			for (s32 i = 0; i < nb; i += (nb + 29) / 30)
				iAround("LAYERED", "ZONESAT", r, &zones[i], i, &q);

            q = 0;
            for (s32 i = 0; i < nb; i += (nb + 29) / 30) for (f64 t : dates)
                printf("UNIXLAYER %d %.17e %.17e\n", q++, soltime_offsetUnixSecsR(r, zones[i].latDeg + .3, zones[i].lonDegE, t), soltime_zoneOffsetUnixSecs(r, i, t));

			// The computed path:  the U.S. rows with their jumps and ramps worked out (merging, J, the
			// drive budget, the gap rule)
			s32			nu	= 0;
			SSolZone*	us	= new SSolZone[nb];
			for (s32 i = 0; i < nb; ++i)
			{
				if (strcmp(country[i], "US") != 0)	continue;
				us[nu]			= zones[i];
				us[nu].rampKm	= -1.0;
				us[nu].jumpSecs	= 0.0;
				++nu;
			}
			SSolRegistry*	ru = soltime_registryCreate(NULL, us, nu);
			printf("CREGISTRY %d %d\n", nu, ru ? 1 : 0);
			if (ru)
			{
				for (s32 i = 0; i < nu; ++i)
					printf("CZONE %d %.17e %.17e %.17e %d\n", i, soltime_zoneLockKm(ru, i), soltime_zoneRampKm(ru, i), soltime_zoneJumpSecs(ru, i), soltime_zoneLeader(ru, i));
				q = 0;
				for (s32 i = 0; i < nu; i += (nu + 14) / 15)
					iAround("CLAYERED", "CZONESAT", ru, &us[i], i, &q);
				soltime_registryDelete(ru);
			}
			delete[] us;

			// Device clocks
			const f64	still[][2] = { { 2.0 * 86400.0, 0.0 } };
			const f64	drive[][2] = { { 2.0 * 3600.0, 0.0 } };
			const f64	jet[][2]   = { { 900.0, 0.0 }, { 60.0, -1.5 }, { 900.0, 0.0 }, { 30.0, -3.0 }, { 900.0, 0.0 } };
			iTrip("STILL", r, 39.77, -86.16,  0.0,   0.0,  still, 1,  80.0 * 86400.0 + 12.0 * 3600.0, 600, -1.0, 0.0, 0.0);
			iTrip("DRIVE", r, 39.77, -86.16, 90.0, 128.75, drive, 1, 171.0 * 86400.0 + 14.0 * 3600.0,  30, -1.0, 0.0, 0.0);
			iTrip("JET",   r, 41.97, -87.90, 95.0, 900.0,  jet,   5, 171.0 * 86400.0 + 15.0 * 3600.0,  30, -1.0, 0.0, 0.0);
			iTrip("JUMP",  r, 39.77, -86.16,  0.0,   0.0,  drive, 1, 200.0 * 86400.0 + 15.0 * 3600.0,  30, 600.0, 39.74, -104.99);
			s32	idx[8];
			s32	nz = soltime_zonesAt(r, 39.77, -86.16, idx, NULL, 8);
			if (nz > 0)
				iTrip("FOLLOW", r, 39.74, -104.99, 0.0, 0.0, still, 1, 300.0 * 86400.0 + 9.0 * 3600.0, 600, -1.0, 0.0, 0.0, idx[((nz < 8) ? nz : 8) - 1]);
			soltime_registryDelete(r);
		}
		delete[] zones;
		delete[] country;
		return 0;
	}
