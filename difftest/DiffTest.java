//////////
//
// DiffTest.java -- the Java side of the C <-> Java differential test (run_difftest.sh).
// Asks SolTime.java exactly the questions difftest.cpp asks the C library, printing the
// same lines.
//
//////
import com.lebeli.soltime.SolTime;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Locale;

public class DiffTest
{
	static final double[]	LATS = { -89.5, -60.0, -45.3, -20.0, 0.0, 12.7, 39.77, 51.5, 59.9, 64.1, 69.6, 78.2, 89.9 };
	static final double[]	LONS = { -179.9, -105.0, -86.16, 0.0, 18.95, 151.2, 179.9 };
	static final double[]	DAYS = { 0.05, 79.6, 80.95, 171.3, 264.85, 354.2, 365.97 };
	static final StringBuilder	out = new StringBuilder(1 << 20);

	static String e(double v)	{ return String.format(Locale.US, "%.17e", v); }

	// Load zones.tsv (name country admin1 lat lon population city_radius_km radius_source
	// zone_radius_km ramp_km jump_s zone):  the published registry, and each row's country
	static SolTime.Zone[] loadZones(String path, ArrayList<String> country)
	{
		ArrayList<SolTime.Zone> list = new ArrayList<>();
		try (BufferedReader in = new BufferedReader(new FileReader(path)))
		{
			String line;
			while ((line = in.readLine()) != null)
			{
				if (line.isEmpty() || line.charAt(0) == '#')	continue;
				String[] f = line.split("\t", -1);
				if (f.length < 11)	continue;
				country.add(f[1]);
				list.add(new SolTime.Zone(Double.parseDouble(f[3]), Double.parseDouble(f[4]), 0.0, Double.parseDouble(f[8]),
										  f[9].equals("-")  ? -1.0 : Double.parseDouble(f[9]),
										  Double.parseDouble(f[5]),
										  f[10].equals("-") ?  0.0 : Double.parseDouble(f[10])));
			}
		}
		catch (Exception ex) { return null; }
		return list.toArray(new SolTime.Zone[0]);
	}

	// The layered clock and the zones around zone i:  in its disc, across its ramp, beyond
	static int around(String tagL, String tagZ, SolTime.Registry r, SolTime.Zone z, int i, int q)
	{
		double[] brgs = { 0.0, 135.0, 250.0 }, days = { 80.7, 171.2, 354.9 };
		double   R = r.lockKm(i), ramp = r.rampKm(i);
		double[] dist = { 0.0, 0.5 * R, R - 0.01, R + 0.3 * ramp, R + 0.9 * ramp, R + ramp + 1.0 };
		int[]    idx  = new int[8];
		double[] w    = new double[8];
		for (double dd : dist)
			for (double brg : brgs)
			{
				double d = dd / 6371.0, p1 = z.latDeg * Math.PI / 180.0, th = brg * Math.PI / 180.0;
				double sp2 = Math.sin(p1) * Math.cos(d) + Math.cos(p1) * Math.sin(d) * Math.cos(th);
				double la = Math.asin(sp2) * 180.0 / Math.PI;
				double lo = Math.IEEEremainder(z.lonDegE + Math.atan2(Math.sin(th) * Math.sin(d) * Math.cos(p1), Math.cos(d) - Math.sin(p1) * sp2) * 180.0 / Math.PI, 360.0);
				for (double day : days)
					out.append(tagL).append(' ').append(q++).append(' ').append(e(r.offsetSecsR(la, lo, day))).append('\n');
				int nz = r.zonesAt(la, lo, idx, w);
				out.append(tagZ).append(' ').append(q - 1).append(' ').append(nz);
				for (int j = 0; j < nz && j < 8; ++j)
					out.append(' ').append(idx[j]).append(' ').append(e(w[j]));
				out.append('\n');
			}
		return q;
	}

	static void trip(String tag, SolTime p, SolTime.Registry r, double lat, double lon, double heading, double kmh, double[][] legs, double utc0, int every, double jumpAt, double jLat, double jLon)
	{
		trip(tag, p, r, lat, lon, heading, kmh, legs, utc0, every, jumpAt, jLat, jLon, -1);
	}

	static void trip(String tag, SolTime p, SolTime.Registry r, double lat, double lon, double heading, double kmh, double[][] legs, double utc0, int every, double jumpAt, double jLat, double jLon, int follow)
	{
        trip(tag, p, r, lat, lon, heading, kmh, legs, utc0, every, jumpAt, jLat, jLon, follow, false);
    }
    static void trip(String tag, SolTime p, SolTime.Registry r, double lat, double lon, double heading, double kmh, double[][] legs, double utc0, int every, double jumpAt, double jLat, double jLon, int follow, boolean unixTime)
    {
        SolTime.Clock c = unixTime ? p.clockCreateUnix(r) : p.clockCreate(r);
		if (follow >= 0)	c.followZone(follow);
		double t = utc0;
		int    s = 0;
		int[]  fl = new int[1];
		for (double[] leg : legs)
		{
			for (int i = 0; i < (int)leg[0]; ++i, ++s)
			{
				if (jumpAt >= 0.0 && s == (int)jumpAt)	{ lat = jLat;  lon = jLon; }
				c.fix((unixTime ? t : t % (366.0 * 86400.0)), lat, lon, kmh, heading);
				double D = c.show((unixTime ? t : t % (366.0 * 86400.0)), fl);
				if (s % every == 0)
					out.append(tag).append(' ').append(s).append(' ').append(e(D)).append(' ').append(fl[0]).append(' ').append(e(c.nextCheckUtc())).append('\n');

				double d = kmh / 3600.0 / 6371.0, p1 = lat * Math.PI / 180.0, th = heading * Math.PI / 180.0;
				double sp2 = Math.sin(p1) * Math.cos(d) + Math.cos(p1) * Math.sin(d) * Math.cos(th);
				double nl = Math.asin(sp2) * 180.0 / Math.PI;
				double no = lon + Math.atan2(Math.sin(th) * Math.sin(d) * Math.cos(p1), Math.cos(d) - Math.sin(p1) * sp2) * 180.0 / Math.PI;
				lat = nl;
				lon = Math.IEEEremainder(no, 360.0);
				heading += leg[1];
				t += 1.0;
			}
		}
	}

	public static void main(String[] args)
	{
		String  zonesPath = (args.length > 0) ? args[0] : "../data/zones.tsv";
		SolTime p = new SolTime();
		int     q = 0;

		for (double lat : LATS)
			for (double lon : LONS)
				for (double day : DAYS)
					out.append("FIELD ").append(q++).append(' ').append(e(p.offsetSecs(lat, lon, day))).append('\n');

		q = 0;
		double[] rsn = new double[3], w = new double[2];
		for (int a = 0; a < LATS.length; ++a)
			for (double day : DAYS)
			{
				int st = p.sunTimesUtc(LATS[a], LONS[a % LONS.length], day, rsn);
				p.nightWindowUtc(LATS[a], LONS[a % LONS.length], day, w);
				out.append("SUN ").append(q++).append(' ').append(st).append(' ').append(e(rsn[0])).append(' ').append(e(rsn[1])).append(' ').append(e(rsn[2]))
				   .append(' ').append(e(p.basisLatDeg(LATS[a], day))).append(' ').append(e(w[0])).append(' ').append(e(w[1])).append('\n');
			}

        q = 0;
        double[] dates = {1767225600., 1798761599., 1798761600., 1835395200., 1861920000., 4133980800.};
        for (double t : dates) for (double lat : LATS) for (double lon : LONS) {
            int state = p.sunTimesUnix(lat, lon, t, rsn);
            p.nightWindowUnix(lat, lon, t, w);
            out.append("CALENDAR ").append(q++).append(' ').append(e(p.offsetUnixSecs(lat, lon, t))).append(' ').append(state)
                .append(' ').append(e(rsn[0])).append(' ').append(e(rsn[1])).append(' ').append(e(rsn[2]))
                .append(' ').append(e(w[0])).append(' ').append(e(w[1])).append(' ').append(e(p.basisLatUnix(lat, t))).append('\n');
        }
        double[][] shortTrip = {{120, 0}};
        trip("UNIXNEWYEAR", p, null, 40, -86, 90, 128.75, shortTrip, 1798761540., 1, -1, 0, 0, -1, true);
        trip("UNIXDATELINE", p, null, 70, 179.99, 90, 900, shortTrip, 1774180800., 1, -1, 0, 0, -1, true);

		double[] qs = { 60.0, 300.0, 600.0, 1200.0 }, rl = { 0.0, 23.5, 40.0, 55.0, 60.0, 64.0, 70.0 };
		q = 0;
		for (double qq : qs)
			for (double la : rl)
				out.append("RADIUS ").append(q++).append(' ').append(e(p.zoneRadiusKm(qq, la))).append('\n');

		ArrayList<String> country = new ArrayList<>();
		SolTime.Zone[]    zones   = loadZones(zonesPath, country);
		int               nb      = (zones != null) ? zones.length : -1;
		SolTime.Registry  r       = (nb > 0) ? p.registryCreate(zones) : null;
		out.append("REGISTRY ").append(nb).append(' ').append(r != null ? 1 : 0).append('\n');
		if (r != null)
		{
			for (int i = 0; i < nb; ++i)
				out.append("ZONE ").append(i).append(' ').append(e(r.lockKm(i))).append(' ').append(e(r.rampKm(i))).append(' ').append(e(r.jumpSecs(i))).append(' ').append(r.leader(i)).append('\n');

			q = 0;
			for (int i = 0; i < nb; i += (nb + 29) / 30)
				q = around("LAYERED", "ZONESAT", r, zones[i], i, q);

            q = 0;
            for (int i = 0; i < nb; i += (nb + 29) / 30) for (double t : dates)
                out.append("UNIXLAYER ").append(q++).append(' ').append(e(r.offsetUnixSecs(zones[i].latDeg + .3, zones[i].lonDegE, t)))
                    .append(' ').append(e(r.zoneOffsetUnixSecs(i, t))).append('\n');

			// The computed path:  the U.S. rows with their jumps and ramps worked out
			ArrayList<SolTime.Zone> us = new ArrayList<>();
			for (int i = 0; i < nb; ++i)
			{
				if (!country.get(i).equals("US"))	continue;
				SolTime.Zone z = zones[i];
				us.add(new SolTime.Zone(z.latDeg, z.lonDegE, z.quantumSecs, z.radiusKm, -1.0, z.population, 0.0));
			}
			SolTime.Zone[]   ua = us.toArray(new SolTime.Zone[0]);
			int              nu = ua.length;
			SolTime.Registry ru = p.registryCreate(ua);
			out.append("CREGISTRY ").append(nu).append(' ').append(ru != null ? 1 : 0).append('\n');
			if (ru != null)
			{
				for (int i = 0; i < nu; ++i)
					out.append("CZONE ").append(i).append(' ').append(e(ru.lockKm(i))).append(' ').append(e(ru.rampKm(i))).append(' ').append(e(ru.jumpSecs(i))).append(' ').append(ru.leader(i)).append('\n');
				q = 0;
				for (int i = 0; i < nu; i += (nu + 14) / 15)
					q = around("CLAYERED", "CZONESAT", ru, ua[i], i, q);
			}

			trip("STILL", p, r, 39.77, -86.16,  0.0,   0.0,  new double[][] { { 2.0 * 86400.0, 0.0 } }, 80.0 * 86400.0 + 12.0 * 3600.0, 600, -1.0, 0.0, 0.0);
			trip("DRIVE", p, r, 39.77, -86.16, 90.0, 128.75, new double[][] { { 2.0 * 3600.0, 0.0 } }, 171.0 * 86400.0 + 14.0 * 3600.0, 30, -1.0, 0.0, 0.0);
			trip("JET",   p, r, 41.97, -87.90, 95.0, 900.0,  new double[][] { { 900.0, 0.0 }, { 60.0, -1.5 }, { 900.0, 0.0 }, { 30.0, -3.0 }, { 900.0, 0.0 } },
				 171.0 * 86400.0 + 15.0 * 3600.0, 30, -1.0, 0.0, 0.0);
			trip("JUMP",  p, r, 39.77, -86.16,  0.0,   0.0,  new double[][] { { 2.0 * 3600.0, 0.0 } }, 200.0 * 86400.0 + 15.0 * 3600.0, 30, 600.0, 39.74, -104.99);
			int[] idx = new int[8];
			int   nz  = r.zonesAt(39.77, -86.16, idx, null);
			if (nz > 0)
				trip("FOLLOW", p, r, 39.74, -104.99, 0.0, 0.0, new double[][] { { 2.0 * 86400.0, 0.0 } }, 300.0 * 86400.0 + 9.0 * 3600.0, 600, -1.0, 0.0, 0.0, idx[((nz < 8) ? nz : 8) - 1]);
		}
		System.out.print(out);
	}
}
