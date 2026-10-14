//////////
//
// soltime_display.cpp -- SolTime on the TUI library (../tui):  the clock, the world, simulations,
// and place search (soltime_spec.md §10, §11).
//
//   F1 Clock      your device clock (§10), the raw field, your zone, UTC, today's sun, tonight's
//                 adjustment, and other zones' clocks
//   F2 World      the planet:  day and night, land traced by the GeoNames places, coloured by the
//                 local SolTime hour; zone centers; a cursor that reads out any spot
//   F3 Simulate   scenarios run against the device clock:  the display vs the true clock, the
//                 second length, checkpoints, the fast-motion rule, "New Time Zone"
//   F4 Find       countries, cities, or typed coordinates -- Enter makes it your location, F5
//                 puts your clock on its zone (anyone may follow any zone's clock;  u stops)
//   Tab cycles the screens;  e toggles exact mode;  q (or Ctrl+Q anywhere) quits.
//
// Headless self-test (no window):  TUI_BACKEND=offscreen SOLTIME_SELFTEST=1 ./soltime_display
// Home location:  SOLTIME_HOME="lat,lon" (default Indianapolis).  Data:  SOLTIME_DATA (default
// the source tree's data/ folder).
//
//////
	#include <stdio.h>
	#include <stdarg.h>
	#include <stdlib.h>
	#include <string.h>
	#include <math.h>
	#include <time.h>
	#include <ctype.h>
	#include <unistd.h>
	#include <list>										// tui.h uses std::list
	#include <pthread.h>								// tui.h uses pthread_mutex_t
	#include <xkbcommon/xkbcommon-keysyms.h>
	#include "../common/types.h"
	#include "../common/font.h"
	#include "../tui/tui.h"
	#include "soltime.h"

	#ifndef SOLTIME_DATA_DIR
	#define SOLTIME_DATA_DIR "data"
	#endif


//////////
// Constants
//////
	static const s32	_DISP_ROWS		= 40;
	static const s32	_DISP_COLS		= 100;
	static const s32	_MAP_TOP		= 2;			// World map rows _MAP_TOP .. _MAP_TOP + _MAP_ROWS - 1
	static const s32	_MAP_ROWS		= 32;
	static const s32	_MAP_COLS		= 100;
	static const s32	_SPARK_LEN		= 96;			// Simulation strip-chart width (cells)
	static const s32	_FIND_ROWS		= 30;			// Search results shown

	// VGA attributes:  (background << 4) | foreground
	static const u8		_A_TEXT			= 0x07;
	static const u8		_A_DIM			= 0x08;
	static const u8		_A_BRIGHT		= 0x0F;
	static const u8		_A_LABEL		= 0x0B;
	static const u8		_A_VALUE		= 0x0E;
	static const u8		_A_GOOD			= 0x0A;
	static const u8		_A_WARN			= 0x0C;
	static const u8		_A_BAR			= 0x1F;
	static const u8		_A_TAB_ON		= 0x4E;
	static const u8		_A_BANNER		= 0x4F;
	static const u8		_A_SELECT		= 0x30;

	static cs8			cgcTitle[]		= "SolTime";
	static cs8			cgcCaption[]	= "SolTime";
	static cs8			cgcCredit[]		= "Data:  GeoNames (CC BY 4.0);  GHSL (c) European Union (CC BY 4.0);  U.S. Census Bureau";
	static const f64	_BIG_ZONE_POP	= 1.0e6;		// The world map marks the zones led by cities this size


//////////
// Data:  places, countries, zones
//////
	struct SPlace
	{
		cs8*		name;			// UTF-8
		cs8*		ascii;			// Shown (the TUI draws CP437)
		cs8*		cc;				// Country code
		cs8*		admin1;			// State / province
		f64			lat, lon;
		s64			pop;
	};

	struct SCountry
	{
		cs8*		cc;
		cs8*		name;
		s32			first, count;	// Its places' range in gPlaces (sorted by country, then population)
	};

	static char*		gPlaceText		= NULL;			// The loaded files (fields point into them)
	static char*		gCountryText	= NULL;
	static char*		gZoneText		= NULL;
	static SPlace*		gPlaces			= NULL;
	static s32			gnPlaces		= 0;
	static SCountry*	gCountries		= NULL;
	static s32			gnCountries		= 0;
	static SSolZone*	gZones			= NULL;
	static cs8**		gZoneNames		= NULL;
	static cs8**		gZoneCC			= NULL;			// Each zone's country code
	static s32			gnZones		= 0;
	static s32*			gZoneMembers	= NULL;			// Per leader:  the cities in its merged zone (once the registry is ready)

	// Registry:  built on a worker thread at startup, published once
	static SSolRegistry*	gRegReady	= NULL;			// Written by the worker (atomic), read by the master
	static SSolRegistry*	gReg		= NULL;			// The master's copy once ready
	static bool				gRegFailed	= false;


//////////
// State (master thread only, except where noted)
//////
	static SITuiState*	gTui			= NULL;
	static STuiWin*		gWin			= NULL;
	static s32			gMode			= 0;			// 0 clock, 1 world, 2 simulate, 3 find

	// Your location and clock
	static f64			gLat			= 39.76838;
	static f64			gLon			= -86.15804;
	static char			gLocName[160]	= "Indianapolis, Indiana, United States";
	static SSolClock*	gClock			= NULL;
	static bool			gClockOnReg		= false;
	static bool			gExact			= false;
	static f64			gZoneUntil		= 0.0;			// Monotonic seconds:  show "New Time Zone" until then
	static f64			gLastShow		= NAN, gLastShowUtc = NAN, gShowRate = 1.0;
	static s32			gFollow			= -1;			// The zone (leader) your clock follows, or -1:  the clock of where you are

	// World
	static s32			gCurRow			= 0;
	static s32			gCurCol			= 0;
	static s32*			gLand			= NULL;			// Places per map cell (_MAP_ROWS x _MAP_COLS)
	static s32*			gZoneCell		= NULL;			// Zone index at a map cell, or -1

	struct SMapCell
	{
		u8			band;			// Local SolTime of day:  0 night, 1 morning, 2 afternoon, 3 evening
		u8			sunUp;
	};
	static pthread_mutex_t	gMapLock	= PTHREAD_MUTEX_INITIALIZER;
	static SMapCell*		gMap		= NULL;			// Ready grid (under gMapLock)
	static bool				gMapValid	= false;
	static s32				gMapStop	= 0;			// Atomic:  1 = worker should exit

	// Find
	static char			gQuery[96]		= "";
	static s32			gSel			= 0;
	static s32*			gHits			= NULL;			// Place indices (gnPlaces capacity)
	static s32			gnHits			= 0;
	static bool			gHitCoords		= false;		// The first hit is typed coordinates
	static f64			gCoordLat		= 0.0, gCoordLon = 0.0;
	static char			gFindMsg[200]	= "";


//////////
// Simulation
//////
	struct SSimLeg
	{
		f64			secs;			// Duration (sim seconds)
		f64			turn;			// Degrees per second (+ right)
	};

	struct SScenario
	{
		cs8*			title;
		cs8*			blurb;
		f64				lat, lon, kmh, heading;
		f64				dayOfYear, utcHour;		// Start
		f64				speedup;				// Sim seconds per real second
		bool			exact;
		const SSimLeg*	legs;
		s32				nLegs;
		f64				jumpAt;					// Sim seconds after the start:  a fix far away (< 0 = none)
		f64				jumpLat, jumpLon;
	};

	static const SSimLeg	cgLegsStill[]	= { { 1.0e9, 0.0 } };
	static const SSimLeg	cgLegsPlane[]	= { { 1500.0, 0.0 }, { 60.0, -1.5 }, { 900.0, 0.0 }, { 120.0, 1.5 }, { 1.0e9, 0.0 } };

	static const SScenario	cgScenarios[] =
	{
		{ "Standing still in Indianapolis, 600x",
		  "From 20:00 the seconds stretch (in spring:  shrink); true seconds again by 04:00.",
		  39.76838, -86.15804,   0.0,  0.0,  80.0, 23.0,    600.0, false, cgLegsStill, 1, -1.0, 0.0, 0.0 },
		{ "80 mph east on I-70, out of Indianapolis, 10x",
		  "Indy's time all through the zone; across its edge ramp the clock speeds up, within 0.5 s/s.",
		  39.76838, -85.98000, 128.75, 90.0, 171.0, 15.0,    10.0, false, cgLegsStill, 1, -1.0, 0.0, 0.0 },
		{ "Airliner over Chicago, heading east, with turns, 20x",
		  "Faster than the bound:  the true time at every checkpoint, re-aimed on every turn.",
		  41.97,    -87.90,    900.0, 95.0, 171.0, 15.0,    20.0, false, cgLegsPlane, 5, -1.0, 0.0, 0.0 },
		{ "A phone switched on in Denver after a trip, 1x",
		  "Standing in Indianapolis; 20 s in, the first fix in Denver arrives:  'New Time Zone'.",
		  39.76838, -86.15804,   0.0,  0.0, 200.0, 15.0,      1.0, false, cgLegsStill, 1, 20.0, 39.74, -104.99 },
		{ "Tromso through the year (exact mode), a day per tick",
		  "69.6N:  polar night borrows the 4-hour basis (noon at 08:45), the midnight sun too.",
		  69.64890,  18.95508,   0.0,  0.0,   0.0, 12.0, 864000.0, true,  cgLegsStill, 1, -1.0, 0.0, 0.0 },
		{ "Jet west at 60N (exact mode), 10x",
		  "Outrunning the sun:  the accurate clock runs backward.",
		  60.0,      10.0,     900.0, 270.0, 171.0, 12.0,    10.0, true,  cgLegsStill, 1, -1.0, 0.0, 0.0 },
	};
	static const s32		cgnScenarios	= (s32)(sizeof(cgScenarios) / sizeof(cgScenarios[0]));

	struct SSim
	{
		s32			which;
		bool		paused;
		f64			speedup;
		f64			uxt;					// Sim UTC
		f64			elapsed;				// Sim seconds since the start
		f64			lat, lon, kmh, heading;
		s32			leg;
		f64			legT;
		bool		jumped;
		SSolClock*	clock;
		f64			lastD, lastUnix, rate;	// The display's own rate (clock seconds per second)
		f64			spark[_SPARK_LEN];		// Display minus truth, seconds
		s32			nSpark;
		f64			zoneUntil;
		u32			flags;
	};
	static SSim		gSim;


//////////
// Time helpers
//////
	static f64 iDisp_nowUnix(void)
	{
		struct timespec	ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		return (f64)ts.tv_sec + (f64)ts.tv_nsec * 1.0e-9;
	}

	static f64 iDisp_mono(void)
	{
		struct timespec	ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (f64)ts.tv_sec + (f64)ts.tv_nsec * 1.0e-9;
	}

	// Jan 1 00:00 UTC of the year containing this instant (SolTime's v1 year:  spec §6)
	static f64 iDisp_yearStart(f64 uxt)
	{
		time_t		t = (time_t)floor(uxt);
		struct tm	tmv, y;
		gmtime_r(&t, &tmv);
		memset(&y, 0, sizeof(y));
		y.tm_year	= tmv.tm_year;
		y.tm_mday	= 1;
		return (f64)timegm(&y);
	}

	// "HH:MM:SS" (and the date) of a raw local seconds-of-year counted from yearStart
	static void iDisp_fmtLocal(f64 yearStart, f64 localSecs, char* hms, s32 nHms, char* date, s32 nDate)
	{
		f64			u	= yearStart + localSecs;
		time_t		t	= (time_t)floor(u);
		struct tm	tmv;
		gmtime_r(&t, &tmv);
		if (hms)	snprintf(hms, nHms, "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
		if (date)	strftime(date, nDate, "%a %b %d %Y", &tmv);
	}

	static void iDisp_fmtOffset(f64 secs, char* out, s32 n)
	{
		s64	s = (s64)llround(secs);
		snprintf(out, n, "%c%02lld:%02lld:%02lld", (s < 0) ? '-' : '+', llabs(s) / 3600, (llabs(s) / 60) % 60, llabs(s) % 60);
	}

	// A whole number with thousands separators
	static void iDisp_commas(f64 v, char* out, s32 n)
	{
		char	digits[32];
		s64		x	= (s64)llround(fabs(v));
		snprintf(digits, sizeof(digits), "%lld", (long long)x);
		s32		len	= (s32)strlen(digits), o = 0;
		if (v < 0.0 && o < n - 1)	out[o++] = '-';
		for (s32 i = 0; i < len && o < n - 1; ++i)
		{
			if (i > 0 && (len - i) % 3 == 0 && o < n - 1)	out[o++] = ',';
			out[o++] = digits[i];
		}
		out[o] = 0;
	}

	static void iDisp_fmtLatLon(f64 lat, f64 lon, char* out, s32 n)
	{
		snprintf(out, n, "%.4f%c %.4f%c", fabs(lat), (lat < 0.0) ? 'S' : 'N', fabs(lon), (lon < 0.0) ? 'W' : 'E');
	}


//////////
// Loading
//////
	static char* iDisp_slurp(cs8* path)
	{
		FILE*	f = fopen(path, "rb");
		if (!f)		return NULL;
		fseek(f, 0, SEEK_END);
		long	n = ftell(f);
		fseek(f, 0, SEEK_SET);
		char*	buf = new char[n + 1];
		size_t	got = fread(buf, 1, (size_t)n, f);
		fclose(f);
		buf[got] = 0;
		return buf;
	}

	// Split buf into lines (in place); returns the count of data lines (not '#' comments).  lines may be NULL to count.
	static s32 iDisp_lines(char* buf, char** lines)
	{
		s32		n = 0;
		char*	p = buf;
		while (*p)
		{
			char*	e = strchr(p, '\n');
			if (e)	*e = 0;
			if (*p && *p != '#')
			{
				if (lines)	lines[n] = p;
				++n;
			}
			if (!e)		break;
			if (!lines)	*e = '\n';				// Counting pass:  restore
			p = e + 1;
		}
		return n;
	}

	// Split a line into tab fields (in place); returns the field count
	static s32 iDisp_fields(char* line, char** f, s32 nMax)
	{
		s32	n = 0;
		f[n++] = line;
		for (char* p = line; *p && n < nMax; ++p)
		{
			if (*p == '\t')
			{
				*p = 0;
				f[n++] = p + 1;
			}
		}
		return n;
	}

	static bool iDisp_loadData(cs8* dir)
	{
		char	path[1024];

		// Places
		snprintf(path, sizeof(path), "%s/places.tsv", dir);
		if (!(gPlaceText = iDisp_slurp(path)))		{ fprintf(stderr, "soltime_display:  cannot read %s\n", path);  return false; }
		s32		n		= iDisp_lines(gPlaceText, NULL);
		char**	lines	= new char*[n > 0 ? n : 1];
		iDisp_lines(gPlaceText, lines);
		gPlaces = new SPlace[n > 0 ? n : 1];
		for (s32 i = 0; i < n; ++i)
		{
			char*	f[8];
			if (iDisp_fields(lines[i], f, 8) < 7)	continue;
			SPlace*	pl = &gPlaces[gnPlaces++];
			pl->name	= f[0];
			pl->ascii	= f[1];
			pl->cc		= f[2];
			pl->admin1	= f[3];
			pl->lat		= atof(f[4]);
			pl->lon		= atof(f[5]);
			pl->pop		= atoll(f[6]);
		}
		delete[] lines;

		// Countries, with each one's range of places
		snprintf(path, sizeof(path), "%s/countries.tsv", dir);
		if (!(gCountryText = iDisp_slurp(path)))	{ fprintf(stderr, "soltime_display:  cannot read %s\n", path);  return false; }
		n		= iDisp_lines(gCountryText, NULL);
		lines	= new char*[n > 0 ? n : 1];
		iDisp_lines(gCountryText, lines);
		gCountries = new SCountry[n > 0 ? n : 1];
		for (s32 i = 0; i < n; ++i)
		{
			char*	f[3];
			if (iDisp_fields(lines[i], f, 3) < 2)	continue;
			SCountry* c = &gCountries[gnCountries++];
			c->cc		= f[0];
			c->name		= f[1];
			c->first	= -1;
			c->count	= 0;
		}
		delete[] lines;
		for (s32 i = 0; i < gnPlaces; ++i)
		{
			for (s32 j = 0; j < gnCountries; ++j)
			{
				if (strcmp(gCountries[j].cc, gPlaces[i].cc) == 0)
				{
					if (gCountries[j].first < 0)	gCountries[j].first = i;
					++gCountries[j].count;
					break;
				}
			}
		}

		// Zones
		snprintf(path, sizeof(path), "%s/zones.tsv", dir);
		if (!(gZoneText = iDisp_slurp(path)))		{ fprintf(stderr, "soltime_display:  cannot read %s\n", path);  return false; }
		n		= iDisp_lines(gZoneText, NULL);
		lines	= new char*[n > 0 ? n : 1];
		iDisp_lines(gZoneText, lines);
		gZones		= new SSolZone[n > 0 ? n : 1];
		gZoneNames	= new cs8*[n > 0 ? n : 1];
		gZoneCC		= new cs8*[n > 0 ? n : 1];
		for (s32 i = 0; i < n; ++i)
		{
			// name country admin1 lat lon population city_radius_km radius_source zone_radius_km ramp_km jump_s zone
			char*	f[12];
			if (iDisp_fields(lines[i], f, 12) < 11)	continue;
			gZoneNames[gnZones]			= f[0];
			gZoneCC[gnZones]			= f[1];
			gZones[gnZones].latDeg		= atof(f[3]);
			gZones[gnZones].lonDegE		= atof(f[4]);
			gZones[gnZones].quantumSecs	= 0.0;
			gZones[gnZones].radiusKm	= atof(f[8]);
			gZones[gnZones].rampKm		= (strcmp(f[9],  "-") == 0) ? -1.0 : atof(f[9]);
			gZones[gnZones].population	= atof(f[5]);
			gZones[gnZones].jumpSecs	= (strcmp(f[10], "-") == 0) ?  0.0 : atof(f[10]);
			++gnZones;
		}
		delete[] lines;
		return true;
	}

	static cs8* iDisp_countryName(cs8* cc)
	{
		for (s32 j = 0; j < gnCountries; ++j)
			if (strcmp(gCountries[j].cc, cc) == 0)	return gCountries[j].name;
		return cc;
	}

	// The nearest place to a point (a plain scan:  34k places is microseconds of work)
	static s32 iDisp_nearestPlace(f64 lat, f64 lon, f64* km)
	{
		s32	best = -1;
		f64	bestD = 1.0e30;
		for (s32 i = 0; i < gnPlaces; ++i)
		{
			f64	dlat = gPlaces[i].lat - lat;
			if (fabs(dlat) > 3.0 && best >= 0 && dlat * dlat * 12364.0 > bestD * bestD)	continue;		// Cheap reject (111.2 km per degree)
			f64	d = soltime_distanceKm(lat, lon, gPlaces[i].lat, gPlaces[i].lon);
			if (d < bestD)	{ bestD = d;  best = i; }
		}
		if (km)		*km = bestD;
		return best;
	}

	static void iDisp_placeLabel(s32 i, char* out, s32 n)
	{
		const SPlace* pl = &gPlaces[i];
		if (pl->admin1[0] && strcmp(pl->admin1, pl->ascii) != 0)	snprintf(out, n, "%s, %s, %s", pl->ascii, pl->admin1, iDisp_countryName(pl->cc));
		else														snprintf(out, n, "%s, %s", pl->ascii, iDisp_countryName(pl->cc));
	}


//////////
// The clock at a point:  layered (zones) once the registry is ready, the raw field before
//////
	static f64 iDisp_offset(f64 lat, f64 lon, f64 utcUnix)
	{
		return gReg ? soltime_offsetUnixSecsR(gReg, lat, lon, utcUnix) : soltime_offsetUnixSecs(NULL, lat, lon, utcUnix);
	}

	// The clock you see:  the zone you follow, or the clock at the point
	static f64 iDisp_yourOffset(f64 lat, f64 lon, f64 utcUnix)
	{
		return (gFollow >= 0) ? soltime_zoneOffsetUnixSecs(gReg, gFollow, utcUnix) : iDisp_offset(lat, lon, utcUnix);
	}

	// The zone on top at a point (or -1) and its weight
	static s32 iDisp_topZone(f64 lat, f64 lon, f64* weight)
	{
		if (!gReg || gnZones <= 0)		{ if (weight) *weight = 0.0;  return -1; }
		s32*	idx	= new s32[gnZones];
		f64*	w	= new f64[gnZones];
		s32		n	= soltime_zonesAt(gReg, lat, lon, idx, w, gnZones);
		s32		top	= (n > 0) ? idx[n - 1] : -1;
		if (weight)	*weight = (n > 0) ? w[n - 1] : 0.0;
		delete[] idx;
		delete[] w;
		return top;
	}

	// A merged zone's name:  its leader, and how many cities share its clock
	static void iDisp_zoneLabel(s32 L, char* out, s32 n)
	{
		s32	m = gZoneMembers ? gZoneMembers[L] : 1;
		if (m > 1)	snprintf(out, n, "%s (%d cities)", gZoneNames[L], m);
		else		snprintf(out, n, "%s", gZoneNames[L]);
	}

	static void iDisp_setLocation(f64 lat, f64 lon, cs8* name)
	{
		gLat = lat;
		gLon = lon;
		snprintf(gLocName, sizeof(gLocName), "%s", name);
		gLastShow = NAN;
	}


//////////
// Drawing primitives
//////
	static void iDisp_print(s32 x, s32 y, u8 attr, cs8* fmt, ...) __attribute__((format(printf, 4, 5)));
	static void iDisp_print(s32 x, s32 y, u8 attr, cs8* fmt, ...)
	{
		char	buf[512];
		va_list	ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		// The TUI reads a few bytes as control codes (0xF0 0xF4 0xF5 0xAE 0xAF):  keep them out of text
		for (char* p = buf; *p; ++p)
		{
			u8 c = (u8)*p;
			if (c == 0xF0 || c == 0xF4 || c == 0xF5 || c == 0xAE || c == 0xAF || c >= 0x80)	*p = '?';
		}
		s32	room = _DISP_COLS - x;
		if (room <= 0)	return;
		gTui->tuiwin_printAttr(gWin, x, y, buf, (s32)strnlen(buf, (size_t)room), attr, false);
	}

	static void iDisp_cell(s32 x, s32 y, u8 ch, u8 attr)
	{
		if (x >= 0 && x < _DISP_COLS && y >= 0 && y < _DISP_ROWS)
			gTui->tuiwin_setCell(gWin, x, y, ch, attr);
	}

	// Big digits:  4 x 5 cells each (':' is 1 wide), drawn with full blocks
	static void iDisp_bigTime(s32 x, s32 y, cs8* hms, u8 attr)
	{
		static cs8*	glyphs[10][5] =
		{
			{ "####", "#  #", "#  #", "#  #", "####" },  { "  # ", " ## ", "  # ", "  # ", " ###" },
			{ "####", "   #", "####", "#   ", "####" },  { "####", "   #", " ###", "   #", "####" },
			{ "#  #", "#  #", "####", "   #", "   #" },  { "####", "#   ", "####", "   #", "####" },
			{ "####", "#   ", "####", "#  #", "####" },  { "####", "   #", "  # ", " #  ", " #  " },
			{ "####", "#  #", "####", "#  #", "####" },  { "####", "#  #", "####", "   #", "####" },
		};
		for (cs8* p = hms; *p; ++p)
		{
			if (*p == ':')
			{
				iDisp_cell(x, y + 1, 0xFE, attr);
				iDisp_cell(x, y + 3, 0xFE, attr);
				x += 2;
				continue;
			}
			if (*p < '0' || *p > '9')	{ x += 5;  continue; }
			for (s32 r = 0; r < 5; ++r)
				for (s32 c = 0; c < 4; ++c)
					iDisp_cell(x + c, y + r, (glyphs[*p - '0'][r][c] == '#') ? 0xDB : ' ', attr);
			x += 5;
		}
	}

	static void iDisp_hline(s32 y, u8 attr)
	{
		for (s32 x = 0; x < _DISP_COLS; ++x)	iDisp_cell(x, y, 0xC4, attr);
	}


//////////
// The world map's worker:  each cell's local SolTime band and whether the sun is up there now
//////
	static void iDisp_cellCenter(s32 row, s32 col, f64* lat, f64* lon)
	{
		*lat = 90.0  - (row + 0.5) * (180.0 / _MAP_ROWS);
		*lon = -180.0 + (col + 0.5) * (360.0 / _MAP_COLS);
	}

	static void* iDisp_mapWorker(void*)
	{
		SMapCell*	grid = new SMapCell[_MAP_ROWS * _MAP_COLS];
		while (!__atomic_load_n(&gMapStop, __ATOMIC_ACQUIRE))
		{
			SSolRegistry*	r		= __atomic_load_n(&gRegReady, __ATOMIC_ACQUIRE);
			f64				now		= iDisp_nowUnix();
			f64				utc		= now;
			for (s32 row = 0; row < _MAP_ROWS && !__atomic_load_n(&gMapStop, __ATOMIC_ACQUIRE); ++row)
			{
				for (s32 col = 0; col < _MAP_COLS; ++col)
				{
					f64	lat, lon, rise, set, noon;
					iDisp_cellCenter(row, col, &lat, &lon);
					f64	off		= r ? soltime_offsetUnixSecsR(r, lat, lon, utc) : soltime_offsetUnixSecs(NULL, lat, lon, utc);
					f64	local	= fmod(fmod(utc + off, 86400.0) + 86400.0, 86400.0);
					u8	band	= (local < 24300.0) ? 0 : (local < 43200.0) ? 1 : (local < 61200.0) ? 2 : (local < 79200.0) ? 3 : 0;
					s32	st		= soltime_sunTimesUnix(NULL, lat, lon, utc + off, &rise, &set, &noon);
					bool up		= (st == 1);
					if (st == 0)
						for (s32 w = -1; w <= 1; ++w)
							if (utc + w * 86400.0 >= rise && utc + w * 86400.0 <= set)	up = true;
					grid[row * _MAP_COLS + col].band	= band;
					grid[row * _MAP_COLS + col].sunUp	= up ? 1 : 0;
				}
			}
			pthread_mutex_lock(&gMapLock);
			memcpy(gMap, grid, sizeof(SMapCell) * _MAP_ROWS * _MAP_COLS);
			gMapValid = true;
			pthread_mutex_unlock(&gMapLock);

			// Refresh every 20 s (the terminator moves a twelfth of a degree in that time)
			for (s32 i = 0; i < 200 && !__atomic_load_n(&gMapStop, __ATOMIC_ACQUIRE); ++i)
				usleep(100000);
		}
		delete[] grid;
		return NULL;
	}

	static void* iDisp_registryWorker(void*)
	{
		SSolRegistry* r = soltime_registryCreate(NULL, gZones, gnZones);
		if (!r)		__atomic_store_n(&gRegFailed, true, __ATOMIC_RELEASE);
		__atomic_store_n(&gRegReady, r, __ATOMIC_RELEASE);
		return NULL;
	}


//////////
// Screen 1:  the clock
//////
	static void iDisp_drawClock(f64 now)
	{
		f64		ys		= 0.0;
		f64		utc		= now;
		char	hms[16], date[40], buf[64];

		// Your clock (§10):  a fix at your location, standing still
		if (gClock)
		{
			soltime_clockFix(gClock, utc, gLat, gLon, 0.0, 0.0);
		}
		u32		flags	= 0;
		f64		D		= gClock ? soltime_clockShow(gClock, utc, &flags) : NAN;
		if (flags & SOLCLOCK_NEW_ZONE)	gZoneUntil = iDisp_mono() + 6.0;
		if (isfinite(D) && isfinite(gLastShow) && utc > gLastShowUtc)
			gShowRate = (D - gLastShow) / (utc - gLastShowUtc);
		gLastShow		= D;
		gLastShowUtc	= utc;

		f64		bw;
		s32		top		= iDisp_topZone(gLat, gLon, &bw);
		f64		field	= soltime_offsetUnixSecs(NULL, gLat, gLon, utc);
		// BUG_FIX_PLAN_2026-10-05.md FIX-02 — cleanup 2026-10-05 16:55:58 EDT (UTC-04:00).
		// The physical/owner dates below no longer need a layered offset here.
		char	ll[48];
		iDisp_fmtLatLon(gLat, gLon, ll, sizeof(ll));

		iDisp_print(1, 2, _A_LABEL, "Location");
		iDisp_print(11, 2, _A_BRIGHT, "%s", gLocName);
		iDisp_print(11, 3, _A_TEXT, "%s", ll);
		char	zl[160];
		if (top >= 0)
		{
			iDisp_zoneLabel(top, zl, sizeof(zl));
			iDisp_print(40, 3, _A_GOOD, "In the %s zone%s", zl, (bw >= 1.0) ? ":  locked" : ":  on its edge ramp");
			iDisp_print(40, 4, _A_DIM, "everyone in it within %.1f min of their own SolTime", soltime_zoneJumpSecs(gReg, top) / 60.0);
		}
		else if (gReg)	iDisp_print(40, 3, _A_TEXT, "No zone here:  the raw SolTime field");
		else			iDisp_print(40, 3, _A_DIM, "Building the zone registry...");

		// The big clock
		if (isfinite(D))
		{
			iDisp_fmtLocal(ys, D, hms, sizeof(hms), date, sizeof(date));
			iDisp_bigTime(3, 5, hms, _A_VALUE);
			iDisp_print(49, 6, _A_BRIGHT, "%s", date);
			if (gFollow >= 0)	iDisp_print(49, 7, _A_GOOD, "Your clock:  on the %s zone%s", gZoneNames[gFollow], gExact ? " (exact)" : "");
			else				iDisp_print(49, 7, _A_TEXT, "Your clock%s", gExact ? " (exact mode)" : "");
			iDisp_print(49, 8, _A_TEXT, "1 clock second = %.6f s", (gShowRate > 0.0) ? 1.0 / gShowRate : 0.0);
			f64	nx = soltime_clockNextCheckUtc(gClock);
			if (!gExact && isfinite(nx))	iDisp_print(49, 9, _A_DIM, "next checkpoint in %.0f s", fmax(0.0, nx - utc));
		}
		if (iDisp_mono() < gZoneUntil)	iDisp_print(70, 6, _A_BANNER, " New Time Zone ");

		// The three readings
		iDisp_hline(11, _A_DIM);
		iDisp_print(1, 12, _A_LABEL, "Raw SolTime here");
		iDisp_fmtLocal(ys, utc + field, hms, sizeof(hms), NULL, 0);
		iDisp_fmtOffset(field, buf, sizeof(buf));
		iDisp_print(22, 12, _A_VALUE, "%s", hms);
		iDisp_print(32, 12, _A_DIM, "UTC%s", buf);
		iDisp_print(1, 13, _A_LABEL, "Your zone's clock");
		if (top >= 0)
		{
			f64	bo = soltime_zoneOffsetUnixSecs(gReg, top, utc);
			iDisp_fmtLocal(ys, utc + bo, hms, sizeof(hms), NULL, 0);
			iDisp_fmtOffset(bo, buf, sizeof(buf));
			iDisp_print(22, 13, _A_VALUE, "%s", hms);
			iDisp_print(32, 13, _A_DIM, "UTC%s  (%s)", buf, gZoneNames[top]);
		} else {
			iDisp_print(22, 13, _A_DIM, "--");
		}
		iDisp_print(1, 14, _A_LABEL, "UTC");
		iDisp_fmtLocal(ys, utc, hms, sizeof(hms), NULL, 0);
		iDisp_print(22, 14, _A_TEXT, "%s", hms);
		iDisp_print(1, 15, _A_LABEL, "Following");
		if (gFollow >= 0)
		{
			f64	fo = soltime_zoneOffsetUnixSecs(gReg, gFollow, utc);
			iDisp_fmtLocal(ys, utc + fo, hms, sizeof(hms), NULL, 0);
			iDisp_fmtOffset(fo, buf, sizeof(buf));
			iDisp_zoneLabel(gFollow, zl, sizeof(zl));
			iDisp_print(22, 15, _A_VALUE, "%s", hms);
			iDisp_print(32, 15, _A_DIM, "UTC%s  (%s, %s;  u = stop)", buf, zl, gZoneCC[gFollow]);
		} else {
			iDisp_print(22, 15, _A_DIM, "--  (F4, then F5 on a city:  your clock follows its zone)");
		}

		// Today's sun, in the clock you see (the zone you follow, when you follow one)
		f64	rise, set, noon;
		// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
		f64 sunDate = soltime_solarDateUnix(NULL, gLat, gLon, utc);
		s32	st		= soltime_sunTimesUnix(NULL, gLat, gLon, sunDate, &rise, &set, &noon);
		iDisp_hline(16, _A_DIM);
		iDisp_print(1, 17, _A_LABEL, "Today's sun");
		if (st == 0)
		{
			char	r_[16], n_[16], s_[16];
			iDisp_fmtLocal(ys, rise + iDisp_yourOffset(gLat, gLon, rise), r_, sizeof(r_), NULL, 0);
			iDisp_fmtLocal(ys, noon + iDisp_yourOffset(gLat, gLon, noon), n_, sizeof(n_), NULL, 0);
			iDisp_fmtLocal(ys, set  + iDisp_yourOffset(gLat, gLon, set), s_, sizeof(s_), NULL, 0);
			iDisp_print(22, 17, _A_TEXT, "sunrise %s   noon %s   sunset %s   daylight %dh %02dm", r_, n_, s_, (s32)((set - rise) / 3600.0), ((s32)((set - rise) / 60.0)) % 60);
		} else {
			iDisp_print(22, 17, _A_TEXT, "%s today", (st > 0) ? "midnight sun" : "polar night");
		}
		f64	bl = soltime_basisLatUnix(NULL, gLat, sunDate);
		// BUG_FIX_PLAN_2026-10-05.md FIX-07 — corrected 2026-10-05 16:55:58 EDT (UTC-04:00).
		// A shared/followed clock does not anchor this location's sunrise.
		if (gFollow >= 0)					iDisp_print(22, 18, _A_DIM, "sun times shown on the %s zone's clock", gZoneNames[gFollow]);
		else if (fabs(bl - gLat) > 1.0e-9)	iDisp_print(22, 18, _A_WARN, "Basis:  this place borrows the daylight timing of latitude %.2f (4 h of day and night)", bl);
		else								iDisp_print(22, 18, _A_DIM, "Basis:  its own latitude (raw solar clock targets 06:45)");

		// Tonight's adjustment (the zone leader's, when following a zone or locked in one:  that is the
		// clock it keeps)
		s32	cz	 = (gFollow >= 0) ? gFollow : ((top >= 0 && bw >= 1.0) ? top : -1);
		f64	cLat = (cz >= 0) ? gZones[cz].latDeg  : gLat;
		f64	cLon = (cz >= 0) ? gZones[cz].lonDegE : gLon;
		// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
		f64 nightDate = soltime_solarDateUnix(NULL, cLat, cLon, utc);
		f64	ws, we;
		if (soltime_nightWindowUnix(NULL, cLat, cLon, nightDate, &ws, &we) == 0)
		{
			f64	o0 = soltime_offsetUnixSecs(NULL, cLat, cLon, ws - 3600.0);
			f64	o1 = soltime_offsetUnixSecs(NULL, cLat, cLon, we + 3600.0);
			char	a_[16], b_[16];
			iDisp_fmtLocal(ys, ws + o0, a_, sizeof(a_), NULL, 0);
			iDisp_fmtLocal(ys, we + o1, b_, sizeof(b_), NULL, 0);
			f64	dO = o1 - o0;
			iDisp_print(1, 19, _A_LABEL, "Tonight");
			char	real_[32];
			iDisp_commas(86400.0 - dO, real_, sizeof(real_));
			iDisp_print(22, 19, _A_TEXT, "adjustment %s .. %s:  tomorrow's clock moves %+.1f s", a_, b_, dO);
			iDisp_print(22, 20, _A_DIM, "seconds up to %.2f%% %s at the peak;  today:  86,400 clock s = %s s",
						100.0 * 2.0 * fabs(dO) / fmax(1.0, we - ws), (dO > 0.0) ? "short" : "long", real_);
		}

		// Other zones' clocks:  the nearest few and a fixed set around the world
		iDisp_hline(22, _A_DIM);
		iDisp_print(1, 23, _A_LABEL, "Zone clocks");
		if (gReg)
		{
			static cs8*	world[][2] = { { "London", "GB" }, { "New York City", "US" }, { "Los Angeles", "US" }, { "Tokyo", "JP" }, { "Sydney", "AU" }, { "Moscow", "RU" },
									   { "Reykjavik", "IS" }, { "Singapore", "SG" }, { "Sao Paulo", "BR" }, { "Cairo", "EG" }, { "Mumbai", "IN" }, { "Anchorage", "US" } };
			s32	shown = 0;
			for (u32 j = 0; j < sizeof(world) / sizeof(world[0]); ++j)
			{
				for (s32 i = 0; i < gnZones; ++i)
				{
					if (strcmp(gZoneNames[i], world[j][0]) != 0 || strcmp(gZoneCC[i], world[j][1]) != 0)		continue;
					f64	bo = soltime_zoneOffsetUnixSecs(gReg, i, utc);
					iDisp_fmtLocal(ys, utc + bo, hms, sizeof(hms), date, sizeof(date));
					iDisp_fmtOffset(bo, buf, sizeof(buf));
					s32	col = (shown % 2) ? 50 : 1, row = 24 + shown / 2;
					iDisp_print(col + 2, row, _A_TEXT, "%-13.13s", gZoneNames[i]);
					iDisp_print(col + 16, row, _A_VALUE, "%s", hms);
					iDisp_print(col + 26, row, _A_DIM, "%.10s UTC%s", date, buf);
					++shown;
				}
			}
		} else {
			iDisp_print(22, 23, _A_DIM, "(building the zone registry...)");
		}
	}


//////////
// Screen 2:  the world
//////
	static void iDisp_drawWorld(f64 now)
	{
		static const u8	bandFg[4] = { 0x08, 0x0E, 0x0F, 0x0C };		// Night, morning, afternoon, evening
		SMapCell*		grid = NULL;

		pthread_mutex_lock(&gMapLock);
		if (gMapValid)
		{
			grid = new SMapCell[_MAP_ROWS * _MAP_COLS];
			memcpy(grid, gMap, sizeof(SMapCell) * _MAP_ROWS * _MAP_COLS);
		}
		pthread_mutex_unlock(&gMapLock);

		for (s32 row = 0; row < _MAP_ROWS; ++row)
		{
			for (s32 col = 0; col < _MAP_COLS; ++col)
			{
				s32	land	= gLand[row * _MAP_COLS + col];
				u8	bg		= (grid && grid[row * _MAP_COLS + col].sunUp) ? 0x10 : 0x00;
				u8	fg		= grid ? bandFg[grid[row * _MAP_COLS + col].band] : 0x07;
				u8	ch		= (land == 0) ? ' ' : (land < 3) ? 0xFA : (land < 10) ? 0xB0 : (land < 50) ? 0xB1 : 0xB2;
				if (gZoneCell[row * _MAP_COLS + col] >= 0)		{ ch = 0x04;  fg = 0x0B; }
				if (row == gCurRow && col == gCurCol)			{ ch = 0xC5;  fg = 0x0F;  bg = 0x40; }
				iDisp_cell(col, _MAP_TOP + row, ch, (u8)(bg | fg));
			}
		}
		delete[] grid;

		// The cursor's readout
		f64		lat, lon, km;
		char	ll[48], hms[16], buf[48], name[200];
		iDisp_cellCenter(gCurRow, gCurCol, &lat, &lon);
		f64		ys	= 0.0;
		f64		utc	= now;
		f64		off	= iDisp_offset(lat, lon, utc);
		s32		np	= iDisp_nearestPlace(lat, lon, &km);
		iDisp_fmtLatLon(lat, lon, ll, sizeof(ll));
		iDisp_fmtLocal(ys, utc + off, hms, sizeof(hms), NULL, 0);
		iDisp_fmtOffset(off, buf, sizeof(buf));
		if (np >= 0)	iDisp_placeLabel(np, name, sizeof(name));
		else			snprintf(name, sizeof(name), "--");
		s32		top	= iDisp_topZone(lat, lon, NULL);

		s32		y	= _MAP_TOP + _MAP_ROWS;
		iDisp_print(1, y, _A_LABEL, "Cursor");
		iDisp_print(9, y, _A_BRIGHT, "%s", ll);
		iDisp_print(36, y, _A_VALUE, "%s", hms);
		iDisp_print(46, y, _A_DIM, "UTC%s", buf);
		char	zl[160];
		if (top >= 0)	{ iDisp_zoneLabel(top, zl, sizeof(zl));  iDisp_print(62, y, _A_GOOD, "zone:  %s", zl); }
		iDisp_print(9, y + 1, _A_TEXT, "near %s (%.0f km)", name, km);
		iDisp_print(1, y + 2, _A_DIM, "Background:  blue = the sun is up.  Land by local SolTime:");
		iDisp_print(60, y + 2, 0x08, "night");
		iDisp_print(66, y + 2, 0x0E, "06:45-12");
		iDisp_print(75, y + 2, 0x0F, "12-17");
		iDisp_print(81, y + 2, 0x0C, "17-22");
		iDisp_cell(88, y + 2, 0x04, 0x0B);
		iDisp_print(90, y + 2, _A_DIM, "zone 1M+");
	}


//////////
// Screen 3:  simulate
//////
	static void iDisp_simStart(s32 which)
	{
		const SScenario*	sc = &cgScenarios[which];
		if (gSim.clock)		soltime_clockDelete(gSim.clock);
		memset(&gSim, 0, sizeof(gSim));
		gSim.which		= which;
		gSim.speedup	= sc->speedup;
		gSim.lat		= sc->lat;
		gSim.lon		= sc->lon;
		gSim.kmh		= sc->kmh;
		gSim.heading	= sc->heading;
		gSim.lastD		= NAN;
		gSim.rate		= 1.0;

		// The start:  this year, the scenario's day and hour (UTC)
		f64	ys			= iDisp_yearStart(iDisp_nowUnix());
		gSim.uxt		= ys + sc->dayOfYear * 86400.0 + sc->utcHour * 3600.0;
		gSim.clock		= soltime_clockCreateUnix(NULL, gReg);
		soltime_clockExact(gSim.clock, sc->exact ? 1 : 0);
	}

	static void iDisp_simStep(f64 realDt)
	{
		const SScenario*	sc = &cgScenarios[gSim.which];
		if (!gSim.clock || gSim.paused)		return;

		f64	dt		= realDt * gSim.speedup;
		gSim.uxt		+= dt;
		gSim.elapsed	+= dt;

		// Move:  legs of straight flight and turns
		if (gSim.kmh > 0.0)
		{
			f64	left = dt;
			while (left > 0.0 && gSim.leg < sc->nLegs)
			{
				f64	step = fmin(left, sc->legs[gSim.leg].secs - gSim.legT);
				f64	la, lo;
				// Short sub-steps keep turns smooth
				for (f64 s = 0.0; s < step; s += 1.0)
				{
					f64	h = fmin(1.0, step - s);
					f64	d = gSim.kmh * h / 3600.0;
					f64	p1 = gSim.lat * M_PI / 180.0, th = gSim.heading * M_PI / 180.0, dd = d / 6371.0;
					f64	sp2 = sin(p1) * cos(dd) + cos(p1) * sin(dd) * cos(th);
					la = asin(fmax(-1.0, fmin(1.0, sp2))) * 180.0 / M_PI;
					lo = gSim.lon + atan2(sin(th) * sin(dd) * cos(p1), cos(dd) - sin(p1) * sp2) * 180.0 / M_PI;
					gSim.lat	= la;
					gSim.lon	= remainder(lo, 360.0);
					gSim.heading = remainder(gSim.heading + sc->legs[gSim.leg].turn * h, 360.0);
					if (gSim.heading < 0.0)		gSim.heading += 360.0;
				}
				gSim.legT	+= step;
				left		-= step;
				if (gSim.legT >= sc->legs[gSim.leg].secs)	{ ++gSim.leg;  gSim.legT = 0.0; }
			}
		}

		// A relocation
		if (sc->jumpAt >= 0.0 && !gSim.jumped && gSim.elapsed >= sc->jumpAt)
		{
			gSim.lat	= sc->jumpLat;
			gSim.lon	= sc->jumpLon;
			gSim.jumped	= true;
		}

		f64	utc = gSim.uxt;
		soltime_clockFix(gSim.clock, utc, gSim.lat, gSim.lon, gSim.kmh, gSim.heading);
		u32	fl	= 0;
		f64	D	= soltime_clockShow(gSim.clock, utc, &fl);
		gSim.flags = fl;
		if (fl & SOLCLOCK_NEW_ZONE)		gSim.zoneUntil = iDisp_mono() + 6.0;
		if (isfinite(gSim.lastD) && gSim.uxt > gSim.lastUnix)
			gSim.rate = (D - gSim.lastD) / (gSim.uxt - gSim.lastUnix);
		gSim.lastD		= D;
		gSim.lastUnix	= gSim.uxt;

		f64	T = utc + iDisp_offset(gSim.lat, gSim.lon, utc);
		if (gSim.nSpark < _SPARK_LEN)	gSim.spark[gSim.nSpark++] = remainder(D - T, 86400.0);
		else
		{
			memmove(gSim.spark, gSim.spark + 1, sizeof(f64) * (_SPARK_LEN - 1));
			gSim.spark[_SPARK_LEN - 1] = remainder(D - T, 86400.0);
		}
	}

	static void iDisp_drawSim(void)
	{
		const SScenario*	sc = &cgScenarios[gSim.which];
		char				hms[16], date[40], buf[48], ll[48];

		// Scenario list
		iDisp_print(1, 2, _A_LABEL, "Scenarios  (1-%d choose, space pause, + - speed, r restart)", cgnScenarios);
		for (s32 i = 0; i < cgnScenarios; ++i)
			iDisp_print(3, 3 + i, (i == gSim.which) ? _A_SELECT : _A_TEXT, " %d  %-60s ", i + 1, cgScenarios[i].title);
		iDisp_print(3, 4 + cgnScenarios, _A_DIM, "%s", sc->blurb);
		if (!gSim.clock)	return;

		f64	ys	= 0.0;
		f64	utc	= gSim.uxt;
		s32	y0	= 6 + cgnScenarios;
		iDisp_hline(y0 - 1, _A_DIM);

		// The display, big, and the truth beside it
		if (isfinite(gSim.lastD))
		{
			iDisp_fmtLocal(ys, gSim.lastD, hms, sizeof(hms), date, sizeof(date));
			iDisp_bigTime(3, y0 + 1, hms, (gSim.flags & SOLCLOCK_FAST) ? _A_WARN : _A_VALUE);
			iDisp_print(49, y0 + 1, _A_BRIGHT, "%s", date);
		}
		f64	off	= iDisp_offset(gSim.lat, gSim.lon, utc);
		iDisp_fmtLocal(ys, utc + off, hms, sizeof(hms), NULL, 0);
		iDisp_print(49, y0 + 2, _A_LABEL, "true SolTime ");
		iDisp_print(62, y0 + 2, _A_TEXT, "%s", hms);
		f64	gap = remainder(gSim.lastD - (utc + off), 86400.0);
		iDisp_print(49, y0 + 3, _A_LABEL, "display-true ");
		iDisp_print(62, y0 + 3, (fabs(gap) < 0.1) ? _A_GOOD : _A_WARN, "%+.3f s", gap);
		iDisp_print(49, y0 + 4, _A_LABEL, "clock rate   ");
		iDisp_print(62, y0 + 4, _A_TEXT, "%.4f clock s per s", gSim.rate);
		f64	nx = soltime_clockNextCheckUtc(gSim.clock);
		iDisp_print(49, y0 + 5, _A_LABEL, "rule         ");
		if (sc->exact)							iDisp_print(62, y0 + 5, _A_TEXT, "exact mode:  the true value every tick");
		else if (gSim.flags & SOLCLOCK_FAST)	iDisp_print(62, y0 + 5, _A_WARN, "FAST:  true time each checkpoint (%.0f s)", isfinite(nx) ? fmax(0.0, nx - utc) : 0.0);
		else									iDisp_print(62, y0 + 5, _A_TEXT, "within +/-0.5 s/s; next check %.0f s", isfinite(nx) ? fmax(0.0, nx - utc) : 0.0);
		if (iDisp_mono() < gSim.zoneUntil)		iDisp_print(80, y0 + 1, _A_BANNER, " New Time Zone ");

		// Where
		iDisp_fmtLatLon(gSim.lat, gSim.lon, ll, sizeof(ll));
		iDisp_fmtLocal(ys, utc, hms, sizeof(hms), date, sizeof(date));
		iDisp_print(1, y0 + 7, _A_LABEL, "sim UTC");
		iDisp_print(12, y0 + 7, _A_TEXT, "%s %s   x%.0f%s", date, hms, gSim.speedup, gSim.paused ? "   PAUSED" : "");
		iDisp_print(1, y0 + 8, _A_LABEL, "position");
		iDisp_print(12, y0 + 8, _A_TEXT, "%s   %.0f km/h  heading %03.0f", ll, gSim.kmh, gSim.heading);
		f64	bw;
		s32	top	= iDisp_topZone(gSim.lat, gSim.lon, &bw);
		iDisp_print(1, y0 + 9, _A_LABEL, "zone");
		char	zl[160];
		if (top >= 0)	{ iDisp_zoneLabel(top, zl, sizeof(zl));  iDisp_print(12, y0 + 9, _A_GOOD, "%s  (%s)", zl, (bw >= 1.0) ? "locked" : "edge ramp"); }
		else			iDisp_print(12, y0 + 9, _A_DIM, "none:  the raw field");
		iDisp_fmtOffset(off, buf, sizeof(buf));
		iDisp_print(1, y0 + 10, _A_LABEL, "vs UTC");
		iDisp_print(12, y0 + 10, _A_TEXT, "%s", buf);
		f64	rise, set, noon;
		s32	st	= soltime_sunTimesUnix(NULL, gSim.lat, gSim.lon, utc + off, &rise, &set, &noon);
		f64	bl	= soltime_basisLatUnix(NULL, gSim.lat, utc + off);
		iDisp_print(1, y0 + 11, _A_LABEL, "sun");
		if (st == 0)
		{
			char	r_[16], s_[16];
			iDisp_fmtLocal(ys, rise + iDisp_offset(gSim.lat, gSim.lon, rise), r_, sizeof(r_), NULL, 0);
			iDisp_fmtLocal(ys, set + iDisp_offset(gSim.lat, gSim.lon, set), s_, sizeof(s_), NULL, 0);
			iDisp_print(12, y0 + 11, _A_TEXT, "rise %s  set %s", r_, s_);
		} else {
			iDisp_print(12, y0 + 11, _A_TEXT, "%s", (st > 0) ? "midnight sun" : "polar night");
		}
		if (fabs(bl - gSim.lat) > 1.0e-9)	iDisp_print(40, y0 + 11, _A_WARN, "basis latitude %.2f (borrowed)", bl);

		// Strip chart:  display minus truth
		f64	scale = 0.05;
		for (s32 i = 0; i < gSim.nSpark; ++i)	scale = fmax(scale, fabs(gSim.spark[i]));
		iDisp_print(1, y0 + 13, _A_LABEL, "display - true, last %d ticks (full scale %.3f s)", gSim.nSpark, scale);
		static const u8	bars[] = { 0xC4, 0xDC, 0xDB };
		for (s32 i = 0; i < gSim.nSpark; ++i)
		{
			f64	v = gSim.spark[i] / scale;
			u8	ch = (fabs(v) < 0.1) ? bars[0] : (fabs(v) < 0.6) ? bars[1] : bars[2];
			iDisp_cell(2 + i, y0 + 14, ch, (v >= 0.0) ? 0x0A : 0x0C);
		}
	}


//////////
// Screen 4:  find
//////
	static bool iDisp_parseCoords(cs8* q, f64* lat, f64* lon)
	{
		// "39.77, -86.16"  or  "39.77N 86.16W"
		f64		v[2];
		char	h[2] = { 0, 0 };
		s32		n = 0;
		cs8*	p = q;
		while (*p && n < 2)
		{
			while (*p == ' ' || *p == ',')	++p;
			char*	e;
			v[n] = strtod(p, &e);
			if (e == p)		return false;
			p = e;
			while (*p == ' ')	++p;
			if (*p == 'N' || *p == 'S' || *p == 'E' || *p == 'W' || *p == 'n' || *p == 's' || *p == 'e' || *p == 'w')
				h[n] = (char)toupper(*p++);
			++n;
		}
		while (*p == ' ')	++p;
		if (n != 2 || *p)		return false;
		if (!isfinite(v[0]) || !isfinite(v[1]) || h[0] == 'E' || h[0] == 'W' || h[1] == 'N' || h[1] == 'S'
		    || (h[0] && v[0] < 0.0) || (h[1] && v[1] < 0.0)) return false;
		if (h[0] == 'S')	v[0] = -fabs(v[0]);
		if (h[1] == 'W')	v[1] = -fabs(v[1]);
		if (fabs(v[0]) > 90.0 || fabs(v[1]) > 180.0)	return false;
		*lat = v[0];
		*lon = v[1];
		return true;
	}

	static bool iDisp_prefix(cs8* s, cs8* q)
	{
		for (; *q; ++s, ++q)
			if (tolower((u8)*s) != tolower((u8)*q))		return false;
		return true;
	}

	static bool iDisp_contains(cs8* s, cs8* q)
	{
		for (; *s; ++s)
			if (iDisp_prefix(s, q))		return true;
		return false;
	}

	static void iDisp_runFind(void)
	{
		gnHits		= 0;
		gSel		= 0;
		gHitCoords	= iDisp_parseCoords(gQuery, &gCoordLat, &gCoordLon);
		if (!gQuery[0] || gHitCoords)	return;

		// A country by name:  its largest places first
		if (strlen(gQuery) >= 3)
		{
			for (s32 j = 0; j < gnCountries; ++j)
			{
				if (!iDisp_prefix(gCountries[j].name, gQuery) || gCountries[j].first < 0)	continue;
				for (s32 i = 0; i < gCountries[j].count && gnHits < 20; ++i)
					gHits[gnHits++] = gCountries[j].first + i;
			}
		}

		// Places:  name prefix, then name contains -- largest first within each
		for (s32 pass = 0; pass < 2; ++pass)
		{
			s32	start = gnHits;
			for (s32 i = 0; i < gnPlaces; ++i)
			{
				bool m = (pass == 0) ? iDisp_prefix(gPlaces[i].ascii, gQuery) : (!iDisp_prefix(gPlaces[i].ascii, gQuery) && iDisp_contains(gPlaces[i].ascii, gQuery));
				if (!m)		continue;
				bool dup = false;
				for (s32 j = 0; j < start && !dup; ++j)		dup = (gHits[j] == i);
				if (!dup)	gHits[gnHits++] = i;
			}
			// Population order within the pass (insertion sort; the result lists are short in practice)
			for (s32 a = start + 1; a < gnHits; ++a)
			{
				s32	v = gHits[a], b = a - 1;
				while (b >= start && gPlaces[gHits[b]].pop < gPlaces[v].pop)	{ gHits[b + 1] = gHits[b];  --b; }
				gHits[b + 1] = v;
			}
		}
	}

	static void iDisp_drawFind(f64 now)
	{
		f64		ys	= 0.0;
		f64		utc	= now;
		char	hms[16], label[200], ll[48];

		iDisp_print(1, 2, _A_LABEL, "Find");
		iDisp_print(7, 2, _A_BRIGHT, "%s", gQuery);
		iDisp_cell(7 + (s32)strlen(gQuery), 2, 0xDB, 0x07);
		iDisp_print(50, 2, _A_DIM, "a city, a country, or coordinates (39.77, -86.16)");
		iDisp_print(1, 3, _A_DIM, "Up/Down choose, Enter = make it your location, F5 = put your clock on its zone, Esc clears");
		iDisp_hline(4, _A_DIM);

		s32	row = 5, total = (gHitCoords ? 1 : 0) + gnHits;
		if (gSel >= total)	gSel = (total > 0) ? total - 1 : 0;
		s32	first = (gSel >= _FIND_ROWS) ? gSel - _FIND_ROWS + 1 : 0;
		for (s32 k = first; k < total && row < 5 + _FIND_ROWS; ++k, ++row)
		{
			f64	lat, lon;
			if (gHitCoords && k == 0)
			{
				lat = gCoordLat;  lon = gCoordLon;
				snprintf(label, sizeof(label), "Typed coordinates");
			} else {
				s32	i = gHits[k - (gHitCoords ? 1 : 0)];
				lat = gPlaces[i].lat;  lon = gPlaces[i].lon;
				iDisp_placeLabel(i, label, sizeof(label));
			}
			iDisp_fmtLatLon(lat, lon, ll, sizeof(ll));
			iDisp_fmtLocal(ys, utc + iDisp_offset(lat, lon, utc), hms, sizeof(hms), NULL, 0);
			u8	a = (k == gSel) ? _A_SELECT : _A_TEXT;
			iDisp_print(1, row, a, " %-58.58s %-22s %s ", label, ll, hms);
		}
		if (gQuery[0] && total == 0)	iDisp_print(3, 5, _A_DIM, "no match");
		if (total > _FIND_ROWS)		iDisp_print(1, 5 + _FIND_ROWS, _A_DIM, "%d matches", total);
		if (gFindMsg[0])			iDisp_print(1, 6 + _FIND_ROWS, _A_WARN, "%s", gFindMsg);
	}

	// The chosen result's place (false:  nothing chosen)
	static bool iDisp_findChosen(f64* lat, f64* lon, char* label, s32 n)
	{
		if (gHitCoords && gSel == 0)
		{
			*lat = gCoordLat;
			*lon = gCoordLon;
			snprintf(label, n, "Typed coordinates");
			return true;
		}
		s32	k = gSel - (gHitCoords ? 1 : 0);
		if (k < 0 || k >= gnHits)	return false;
		*lat = gPlaces[gHits[k]].lat;
		*lon = gPlaces[gHits[k]].lon;
		iDisp_placeLabel(gHits[k], label, n);
		return true;
	}

	// F5:  your clock follows the zone at the chosen place, wherever you are
	static void iDisp_findFollow(void)
	{
		f64		lat, lon;
		char	label[200];
		if (!iDisp_findChosen(&lat, &lon, label, sizeof(label)))	return;
		if (!gReg)		{ snprintf(gFindMsg, sizeof(gFindMsg), "The zone registry is still being built:  try again in a moment");  return; }
		s32	top = iDisp_topZone(lat, lon, NULL);
		if (top < 0)	{ snprintf(gFindMsg, sizeof(gFindMsg), "No zone at %.150s:  nothing to follow", label);  return; }
		if (gClock && soltime_clockFollowZone(gClock, top) == 0)
		{
			gFollow		= top;
			gFindMsg[0]	= 0;
			gMode		= 0;
		}
	}

	static void iDisp_findChoose(void)
	{
		f64		lat, lon;
		char	label[200];
		if (!iDisp_findChosen(&lat, &lon, label, sizeof(label)))	return;
		iDisp_setLocation(lat, lon, label);
		gFindMsg[0]	= 0;
		gMode		= 0;
	}


//////////
// Frame:  title bar, the screen, help + credit
//////
	static void iDisp_draw(SITuiState* its)
	{
		(void)its;
		f64	now = iDisp_nowUnix();

		// The registry arrives:  the clocks move onto it
		if (!gReg)
		{
			SSolRegistry*	r = __atomic_load_n(&gRegReady, __ATOMIC_ACQUIRE);
			if (r)
			{
				gReg = r;
				gZoneMembers = new s32[gnZones > 0 ? gnZones : 1];
				for (s32 i = 0; i < gnZones; ++i)	gZoneMembers[i] = 0;
				for (s32 i = 0; i < gnZones; ++i)	++gZoneMembers[soltime_zoneLeader(gReg, i)];
				for (s32 i = 0; i < gnZones; ++i)
				{
					if (soltime_zoneLeader(gReg, i) != i || gZones[i].population < _BIG_ZONE_POP)	continue;
					s32	row = (s32)((90.0 - gZones[i].latDeg) / (180.0 / _MAP_ROWS)), col = (s32)((gZones[i].lonDegE + 180.0) / (360.0 / _MAP_COLS));
					if (row >= 0 && row < _MAP_ROWS && col >= 0 && col < _MAP_COLS)		gZoneCell[row * _MAP_COLS + col] = i;
				}
				if (gClock)		soltime_clockDelete(gClock);
				gClock		= soltime_clockCreateUnix(NULL, gReg);
				gClockOnReg	= true;
				soltime_clockExact(gClock, gExact ? 1 : 0);
				iDisp_simStart(gSim.which);
			}
		}

		gTui->tuiwin_clear(gWin, 0, 0, _DISP_COLS - 1, _DISP_ROWS - 1);
		for (s32 x = 0; x < _DISP_COLS; ++x)	iDisp_cell(x, 0, ' ', _A_BAR);
		iDisp_print(1, 0, _A_BAR, "SolTime");
		static cs8*	tabs[] = { "F1 Clock", "F2 World", "F3 Simulate", "F4 Find" };
		s32	x = 11;
		for (s32 i = 0; i < 4; ++i)
		{
			iDisp_print(x, 0, (i == gMode) ? _A_TAB_ON : _A_BAR, " %s ", tabs[i]);
			x += (s32)strlen(tabs[i]) + 3;
		}
		iDisp_print(70, 0, _A_BAR, "e exact %s   q quit", gExact ? "ON " : "off");
		if (gRegFailed)		iDisp_print(1, 1, _A_WARN, "The zone registry was refused (zones.tsv):  showing the raw field only");

		switch (gMode)
		{
			case 0:		iDisp_drawClock(now);	break;
			case 1:		iDisp_drawWorld(now);	break;
			case 2:		iDisp_drawSim();		break;
			default:	iDisp_drawFind(now);	break;
		}

		static cs8*	help[] =
		{
			"Tab / F1-F4 screens   e exact mode   u stop following a zone   q quit",
			"Arrows move the cursor (Shift: x10)   Enter = my location   h = back home",
			"1-6 scenario   space pause   + - speed   r restart",
			"Type to search   Up/Down   Enter = my location   F5 = follow its zone   Esc clear   Ctrl+Q quit",
		};
		iDisp_print(1, _DISP_ROWS - 2, _A_DIM, "%s", help[gMode]);
		iDisp_print(_DISP_COLS - 1 - (s32)strlen(cgcCredit), _DISP_ROWS - 1, _A_DIM, "%s", cgcCredit);
	}

	static f64	gLastTick = NAN;

	static void iDisp_onTimer(SITuiState* its, STuiTimer*, void*)
	{
		f64	m = iDisp_mono();
		if (gMode == 2 && isfinite(gLastTick))	iDisp_simStep(m - gLastTick);
		gLastTick = m;
		iDisp_draw(its);
	}


//////////
// Keys
//////
	static f64	gHomeLat = 39.76838, gHomeLon = -86.15804;
	static char	gHomeName[160] = "Indianapolis, Indiana, United States";

	static void iDisp_onKey(SITuiState* its, u32 key, bool down, s8* text, s32 textLen, bool ctrl, bool shift, bool alt, bool super)
	{
		(void)alt;  (void)super;
		if (!down)		return;

		if (ctrl && (key == 'q' || key == 'Q'))		{ tui_canvas_close(its);  return; }
		switch (key)
		{
			case XKB_KEY_F1:			gMode = 0;					iDisp_draw(its);	return;
			case XKB_KEY_F2:			gMode = 1;					iDisp_draw(its);	return;
			case XKB_KEY_F3:			gMode = 2;					iDisp_draw(its);	return;
			case XKB_KEY_F4:			gMode = 3;					iDisp_draw(its);	return;
			case XKB_KEY_Tab:			gMode = (gMode + 1) % 4;	iDisp_draw(its);	return;
			case XKB_KEY_ISO_Left_Tab:	gMode = (gMode + 3) % 4;	iDisp_draw(its);	return;
		}

		if (gMode == 3)
		{
			// Find:  typing
			size_t	len = strlen(gQuery);
			if (key == XKB_KEY_Escape)			{ if (len) { gQuery[0] = 0;  iDisp_runFind(); } else gMode = 0; }
			else if (key == XKB_KEY_BackSpace)	{ if (len) { gQuery[len - 1] = 0;  iDisp_runFind(); } }
			else if (key == XKB_KEY_Return || key == XKB_KEY_KP_Enter)	iDisp_findChoose();
			else if (key == XKB_KEY_F5)			iDisp_findFollow();
			else if (key == XKB_KEY_Up)			{ if (gSel > 0) --gSel; }
			else if (key == XKB_KEY_Down)		++gSel;
			else if (key == XKB_KEY_Page_Down)	gSel += _FIND_ROWS;
			else if (key == XKB_KEY_Page_Up)	gSel = (gSel > _FIND_ROWS) ? gSel - _FIND_ROWS : 0;
			else if (textLen == 1 && text && (u8)text[0] >= 0x20 && (u8)text[0] < 0x7F && len + 1 < sizeof(gQuery))
			{
				gQuery[len]		= (char)text[0];
				gQuery[len + 1]	= 0;
				iDisp_runFind();
			}
			iDisp_draw(its);
			return;
		}

		if (key == 'q' || key == 'Q' || key == XKB_KEY_Escape)	{ tui_canvas_close(its);  return; }
		if ((key == 'u' || key == 'U') && gFollow >= 0)
		{
			if (gClock)		soltime_clockFollowZone(gClock, -1);
			gFollow = -1;
			iDisp_draw(its);
			return;
		}
		if (key == 'e' || key == 'E')
		{
			gExact = !gExact;
			if (gClock)		soltime_clockExact(gClock, gExact ? 1 : 0);
			iDisp_draw(its);
			return;
		}

		if (gMode == 1)
		{
			s32	step = shift ? 10 : 1;
			if (key == XKB_KEY_Left)		gCurCol = (gCurCol - step + _MAP_COLS * 10) % _MAP_COLS;
			if (key == XKB_KEY_Right)		gCurCol = (gCurCol + step) % _MAP_COLS;
			if (key == XKB_KEY_Up)			gCurRow = (gCurRow - step < 0) ? 0 : gCurRow - step;
			if (key == XKB_KEY_Down)		gCurRow = (gCurRow + step >= _MAP_ROWS) ? _MAP_ROWS - 1 : gCurRow + step;
			if (key == 'h' || key == 'H')
			{
				iDisp_setLocation(gHomeLat, gHomeLon, gHomeName);
				gCurRow = (s32)((90.0 - gLat) / (180.0 / _MAP_ROWS));
				gCurCol = (s32)((gLon + 180.0) / (360.0 / _MAP_COLS)) % _MAP_COLS;
			}
			if (key == XKB_KEY_Return || key == XKB_KEY_KP_Enter)
			{
				f64		lat, lon, km;
				char	label[220], near_[200];
				iDisp_cellCenter(gCurRow, gCurCol, &lat, &lon);
				s32		np = iDisp_nearestPlace(lat, lon, &km);
				if (np >= 0)	{ iDisp_placeLabel(np, near_, sizeof(near_));  snprintf(label, sizeof(label), "Map point near %s", near_); }
				else			snprintf(label, sizeof(label), "Map point");
				iDisp_setLocation(lat, lon, label);
				gMode = 0;
			}
			iDisp_draw(its);
			return;
		}

		if (gMode == 2)
		{
			if (key >= '1' && key < (u32)('1' + cgnScenarios))	iDisp_simStart((s32)(key - '1'));
			if (key == ' ')										gSim.paused = !gSim.paused;
			if (key == '+' || key == '=' || key == XKB_KEY_KP_Add)		gSim.speedup *= 2.0;
			if (key == '-' || key == XKB_KEY_KP_Subtract)				gSim.speedup = fmax(0.125, gSim.speedup * 0.5);
			if (key == 'r' || key == 'R')						iDisp_simStart(gSim.which);
			iDisp_draw(its);
		}
	}


//////////
// Self-test (headless):  every screen, a search, a scenario -- dumped as text
//////
	static void iDisp_dumpScreen(SITuiState* its, void*)
	{
		(void)its;
		s32		n		= gWin->w * gWin->h;
		u16*	cells	= new u16[n];
		if (gTui->tuiwin_composeCells(gWin, cells, n))
		{
			for (s32 y = 0; y < gWin->h && y < _DISP_ROWS; ++y)
			{
				char	line[_DISP_COLS + 1];
				s32		w = (gWin->w < _DISP_COLS) ? gWin->w : _DISP_COLS;
				for (s32 x = 0; x < w; ++x)
				{
					u8	ch = (u8)(cells[y * gWin->w + x] & 0xFF);
					line[x] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : (ch == 0xDB || ch == 0xB2) ? '#' : (ch == 0xB1 || ch == 0xB0) ? '+' : (ch == 0xFA) ? '.' : (ch == 0xC4) ? '-' : (ch == 0) ? ' ' : '*';
				}
				s32	e = w;
				while (e > 0 && line[e - 1] == ' ')	--e;
				line[e] = 0;
				printf("|%s\n", line);
			}
		}
		delete[] cells;
	}

	static void iDisp_setMode(SITuiState* its, void* arg)		{ gMode = (s32)(sptr)arg;  iDisp_draw(its); }
	static void iDisp_setQuery(SITuiState* its, void* arg)		{ snprintf(gQuery, sizeof(gQuery), "%s", (cs8*)arg);  iDisp_runFind();  iDisp_draw(its); }
	static void iDisp_chooseSim(SITuiState* its, void* arg)		{ iDisp_simStart((s32)(sptr)arg);  iDisp_draw(its); }
	static void iDisp_firstDraw(SITuiState* its, void*)			{ iDisp_draw(its); }
	static void iDisp_choose(SITuiState* its, void*)			{ iDisp_findChoose();  iDisp_draw(its); }
	static void iDisp_follow(SITuiState* its, void*)			{ iDisp_findFollow();  iDisp_draw(its); }

	static s32 iDisp_selfTest(SITuiState* cv)
	{
		// Wait for the zone registry (it is built on a worker thread)
		for (s32 i = 0; i < 600 && !__atomic_load_n(&gRegReady, __ATOMIC_ACQUIRE) && !__atomic_load_n(&gRegFailed, __ATOMIC_ACQUIRE); ++i)
			usleep(100000);
		usleep(400000);

		cs8*	titles[] = { "CLOCK", "WORLD", "SIMULATE (scenario 2)", "FIND tromso", "FIND norway", "FIND 64.13N 21.9W", "CLOCK in Denver, following the Indianapolis zone" };
		for (s32 step = 0; step < 7; ++step)
		{
			if (step == 6)
			{
				tui_canvas_invoke(cv, &iDisp_setQuery, (void*)"denver", 2000);
				tui_canvas_invoke(cv, &iDisp_choose, NULL, 2000);
				tui_canvas_invoke(cv, &iDisp_setQuery, (void*)"indianapolis", 2000);
				tui_canvas_invoke(cv, &iDisp_follow, NULL, 2000);
			}
			if (step <= 1)		tui_canvas_invoke(cv, &iDisp_setMode, (void*)(sptr)step, 2000);
			if (step == 2)		{ tui_canvas_invoke(cv, &iDisp_setMode, (void*)(sptr)2, 2000);  tui_canvas_invoke(cv, &iDisp_chooseSim, (void*)(sptr)1, 2000);  usleep(3000000); }
			if (step == 3)		{ tui_canvas_invoke(cv, &iDisp_setMode, (void*)(sptr)3, 2000);  tui_canvas_invoke(cv, &iDisp_setQuery, (void*)"tromso", 2000); }
			if (step == 4)		tui_canvas_invoke(cv, &iDisp_setQuery, (void*)"norway", 2000);
			if (step == 5)		tui_canvas_invoke(cv, &iDisp_setQuery, (void*)"64.13N 21.9W", 2000);
			usleep(400000);
			printf("==== %s\n", titles[step]);
			tui_canvas_invoke(cv, &iDisp_dumpScreen, NULL, 5000);
		}

		// Keys through the real path:  F1, then quit
		tui_canvas_injectKey(cv, XKB_KEY_F1, true, NULL, 0, 0);
		tui_canvas_injectKey(cv, XKB_KEY_F1, false, NULL, 0, 0);
		tui_canvas_injectFlush(cv, 2000);
		printf("==== SELFTEST DONE (mode %d)\n", gMode);
		tui_canvas_injectKey(cv, 'q', true, "q", 1, 0);
		return 0;
	}


//////////
// Main
//////
	int main(void)
	{
		setvbuf(stdout, NULL, _IOLBF, 0);

		cs8*	dataDir = getenv("SOLTIME_DATA");
		if (!dataDir)	dataDir = SOLTIME_DATA_DIR;
		if (!iDisp_loadData(dataDir))
			return 1;

		// Home
		cs8*	home = getenv("SOLTIME_HOME");
		f64		hl, hn;
		if (home && iDisp_parseCoords(home, &hl, &hn))
		{
			gHomeLat = hl;
			gHomeLon = hn;
			s32	np = iDisp_nearestPlace(hl, hn, NULL);
			if (np >= 0)	iDisp_placeLabel(np, gHomeName, sizeof(gHomeName));
		}
		iDisp_setLocation(gHomeLat, gHomeLon, gHomeName);
		gCurRow = (s32)((90.0 - gLat) / (180.0 / _MAP_ROWS));
		gCurCol = (s32)((gLon + 180.0) / (360.0 / _MAP_COLS)) % _MAP_COLS;

		// Map backdrop:  places per cell (the zone marks come with the registry)
		gLand		= new s32[_MAP_ROWS * _MAP_COLS];
		gZoneCell	= new s32[_MAP_ROWS * _MAP_COLS];
		gMap		= new SMapCell[_MAP_ROWS * _MAP_COLS];
		gHits		= new s32[gnPlaces > 0 ? gnPlaces : 1];
		for (s32 i = 0; i < _MAP_ROWS * _MAP_COLS; ++i)		{ gLand[i] = 0;  gZoneCell[i] = -1; }
		for (s32 i = 0; i < gnPlaces; ++i)
		{
			s32	r = (s32)((90.0 - gPlaces[i].lat) / (180.0 / _MAP_ROWS)), c = (s32)((gPlaces[i].lon + 180.0) / (360.0 / _MAP_COLS));
			if (r >= 0 && r < _MAP_ROWS && c >= 0 && c < _MAP_COLS)		++gLand[r * _MAP_COLS + c];
		}

		// The clock (raw field until the registry is ready), the workers, the first scenario
		gClock = soltime_clockCreateUnix(NULL, NULL);
		pthread_t	regThread, mapThread;
		pthread_create(&regThread, NULL, &iDisp_registryWorker, NULL);
		pthread_create(&mapThread, NULL, &iDisp_mapWorker, NULL);
		memset(&gSim, 0, sizeof(gSim));
		iDisp_simStart(0);

		// The canvas
		SITuiState	its;
		memset(&its, 0, sizeof(its));
		its.cTitleBar					= cgcTitle;
		its.callback_canvas_keypress	= &iDisp_onKey;
		gTui = tui_run_canvas_by_row_col_ttf(_DISP_ROWS, _DISP_COLS, "DejaVu Sans Mono", 16, false, false, false, false,
											 _A_TEXT, cgcCaption, true, true, &its, &gWin);
		s32	rc = 0;
		if (!gTui)
		{
			fprintf(stderr, "soltime_display:  no canvas (result %d)\n", its.result);
			rc = 1;

		} else {
			while (tui_canvas_frame(gTui) == 0 && tui_canvas_result(gTui) == 0)		usleep(10000);
			if (tui_canvas_result(gTui) == 0)
			{
				SFont*	font = gWin->font;
				gTui->tuiwin_setMovable(gWin, false);
				gTui->tuiwin_setResizable(gWin, false);
				tui_canvas_invoke(gTui, &iDisp_firstDraw, NULL, 2000);
				gTui->tuitimer_add(gTui, 100, &iDisp_onTimer, NULL);

				if (getenv("SOLTIME_SELFTEST"))		iDisp_selfTest(gTui);

				while (tui_canvas_result(gTui) >= 0)	usleep(100000);
				for (s32 i = 0; i < 100 && !tui_canvas_isClosed(gTui); ++i)		usleep(100000);
				gTui->tuifont_destroy(font);

			} else {
				fprintf(stderr, "soltime_display:  the canvas failed (result %d)\n", tui_canvas_result(gTui));
				rc = 1;
			}
		}

		// Shut down
		__atomic_store_n(&gMapStop, 1, __ATOMIC_RELEASE);
		pthread_join(mapThread, NULL);
		pthread_join(regThread, NULL);
		if (gSim.clock)		soltime_clockDelete(gSim.clock);
		if (gClock)			soltime_clockDelete(gClock);
		if (gRegReady)		soltime_registryDelete(gRegReady);
		delete[] gLand;
		delete[] gZoneCell;
		delete[] gMap;
		delete[] gHits;
		delete[] gPlaces;
		delete[] gCountries;
		delete[] gZones;
		delete[] gZoneNames;
		delete[] gZoneCC;
		delete[] gZoneMembers;
		delete[] gPlaceText;
		delete[] gCountryText;
		delete[] gZoneText;
		return rc;
	}
