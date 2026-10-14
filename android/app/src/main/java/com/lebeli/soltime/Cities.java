//////////
//
// Cities.java -- the place data (spec §11):  the GeoNames places and countries
// (places.tsv / countries.tsv, CC BY 4.0) and the published zone registry (zones.tsv:  every
// city of 100,000+ people, its zone 1.5x the city's radius, merged zones' ramps and jumps
// computed once), all from the project's data/ folder, or android/data/ when this folder is built
// alone (Gradle packs that folder as the app's assets).
//
// Search:  a country by name (its largest places), a place by name (prefix first, then
// anywhere in the name, largest first within each), or typed coordinates ("39.77, -86.16" or
// "39.77N 86.16W").  Nearest place:  a plain scan, with a cheap latitude reject.
//
//////
package com.lebeli.soltime;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Locale;

public final class Cities
{
	public static final String	CREDIT = "Place data:  GeoNames (geonames.org), CC BY 4.0\n"
									   + "City areas:  GHSL Urban Centre Database R2024A, (c) European Union, CC BY 4.0;  U.S. Census Bureau Gazetteer";

	public static final class Place
	{
		public String	name, ascii, cc, admin1;
		public double	lat, lon;
		public long		pop;
	}

	public static final class Country
	{
		public String	cc, name;
		public int		first = -1, count = 0;		// Its places' range (places are sorted by country, then population)
	}

	public static final class ZoneCity
	{
		public String	name, cc;
		public double	lat, lon, population;
		public double	radiusKm;					// The zone's disc:  1.5x the city's radius
		public double	rampKm, jumpSecs;			// Its merged zone's (published);  -1 / 0 = not yet computed
		public boolean	leader;						// The most populous city of its merged zone (its zone column is its own row)
	}

	public Place[]		places;
	public Country[]	countries;
	public ZoneCity[]	zones;
	private final HashMap<String, Country>	byCode = new HashMap<>();


	//////////
	// Loading:  tab-separated, '#' lines are comments.  null on a read failure.
	//////
		private static ArrayList<String[]> iRows(InputStream in, int minFields)
		{
			ArrayList<String[]> rows = new ArrayList<>();
			try (BufferedReader r = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8)))
			{
				String line;
				while ((line = r.readLine()) != null)
				{
					if (line.isEmpty() || line.charAt(0) == '#')	continue;
					String[] f = line.split("\t", -1);
					if (f.length >= minFields)	rows.add(f);
				}
			}
			catch (Exception ex) { return null; }
			return rows;
		}

		public static Cities load(InputStream places, InputStream countries, InputStream zones)
		{
			ArrayList<String[]> pr = iRows(places, 7), cr = iRows(countries, 2), br = iRows(zones, 11);
			if (pr == null || cr == null || br == null)		return null;

			Cities c = new Cities();
			c.places = new Place[pr.size()];
			for (int i = 0; i < pr.size(); ++i)
			{
				String[] f = pr.get(i);
				Place    p = new Place();
				p.name		= f[0];
				p.ascii		= f[1];
				p.cc		= f[2];
				p.admin1	= f[3];
				p.lat		= Double.parseDouble(f[4]);
				p.lon		= Double.parseDouble(f[5]);
				p.pop		= Long.parseLong(f[6]);
				c.places[i]	= p;
			}

			c.countries = new Country[cr.size()];
			for (int i = 0; i < cr.size(); ++i)
			{
				Country k	= new Country();
				k.cc		= cr.get(i)[0];
				k.name		= cr.get(i)[1];
				c.countries[i] = k;
				c.byCode.put(k.cc, k);
			}
			for (int i = 0; i < c.places.length; ++i)
			{
				Country k = c.byCode.get(c.places[i].cc);
				if (k == null)		continue;
				if (k.first < 0)	k.first = i;
				++k.count;
			}

			c.zones = new ZoneCity[br.size()];
			for (int i = 0; i < br.size(); ++i)
			{
				// name country admin1 lat lon population city_radius_km radius_source zone_radius_km ramp_km jump_s [zone zone_name]
				String[] f = br.get(i);
				ZoneCity b = new ZoneCity();
				b.name			= f[0];
				b.cc			= f[1];
				b.lat			= Double.parseDouble(f[3]);
				b.lon			= Double.parseDouble(f[4]);
				b.population	= Double.parseDouble(f[5]);
				b.radiusKm		= Double.parseDouble(f[8]);
				b.rampKm		= f[9].equals("-")  ? -1.0 : Double.parseDouble(f[9]);
				b.jumpSecs		= f[10].equals("-") ?  0.0 : Double.parseDouble(f[10]);
				b.leader		= (f.length > 11) && f[11].equals(Integer.toString(i));		// zone:  the row of its leader
				c.zones[i]		= b;
			}
			return c;
		}

		// The zone list as the registry takes it:  the published discs, ramps and jumps
		public SolTime.Zone[] zoneArray()
		{
			SolTime.Zone[] out = new SolTime.Zone[zones.length];
			for (int i = 0; i < zones.length; ++i)
			{
				ZoneCity z = zones[i];
				out[i] = new SolTime.Zone(z.lat, z.lon, 0.0, z.radiusKm, z.rampKm, z.population, z.jumpSecs);
			}
			return out;
		}


	//////////
	// Labels and lookups
	//////
		public String countryName(String cc)
		{
			Country k = byCode.get(cc);
			return (k != null) ? k.name : cc;
		}

		public String label(int i)
		{
			Place p = places[i];
			if (!p.admin1.isEmpty() && !p.admin1.equals(p.ascii))
				return p.ascii + ", " + p.admin1 + ", " + countryName(p.cc);
			return p.ascii + ", " + countryName(p.cc);
		}

		// The nearest place to a point; km[0] (nullable) gets the distance.  -1 if there are no places.
		public int nearest(double lat, double lon, double[] km)
		{
			int    best  = -1;
			double bestD = Double.MAX_VALUE;
			for (int i = 0; i < places.length; ++i)
			{
				double dlat = places[i].lat - lat;
				if (best >= 0 && dlat * dlat * 12364.0 > bestD * bestD)	continue;		// 111.2 km per degree
				double d = SolTime.distanceKm(lat, lon, places[i].lat, places[i].lon);
				if (d < bestD)	{ bestD = d;  best = i; }
			}
			if (km != null)		km[0] = bestD;
			return best;
		}


	//////////
	// Search
	//////
		// Typed coordinates:  "39.77, -86.16"  or  "39.77N 86.16W".  out = { lat, lon }; false if it is not that.
		public static boolean parseCoords(String q, double[] out)
		{
			String[] parts = q.trim().split("[\\s,]+");
			if (parts.length != 2)		return false;
			double[] v = new double[2];
			for (int i = 0; i < 2; ++i)
			{
				String s = parts[i].toUpperCase(Locale.US);
				char h = 0;
				if (!s.isEmpty() && "NSEW".indexOf(s.charAt(s.length() - 1)) >= 0) {
					h = s.charAt(s.length() - 1);
					s = s.substring(0, s.length() - 1);
				}
				if ((i == 0 && (h == 'E' || h == 'W')) || (i == 1 && (h == 'N' || h == 'S'))) return false;
				try { v[i] = Double.parseDouble(s); }
				catch (NumberFormatException ex) { return false; }
				if (!Double.isFinite(v[i]) || (h != 0 && v[i] < 0)) return false;
				if (h == 'S' || h == 'W')	v[i] = -Math.abs(v[i]);
			}
			if (Math.abs(v[0]) > 90.0 || Math.abs(v[1]) > 180.0)	return false;
			out[0] = v[0];
			out[1] = v[1];
			return true;
		}

		private static boolean iPrefix(String s, String q)		{ return s.regionMatches(true, 0, q, 0, q.length()); }

		private static boolean iContains(String s, String q)
		{
			for (int i = 0; i + q.length() <= s.length(); ++i)
				if (s.regionMatches(true, i, q, 0, q.length()))	return true;
			return false;
		}

		// Up to max place indices for a query:  a country's largest places, then name prefix, then name contains
		public int[] search(String q, int max)
		{
			q = q.trim();
			int[] hits = new int[max];
			int   n    = 0;
			if (q.isEmpty())	return new int[0];

			boolean[] taken = new boolean[places.length];
			if (q.length() >= 3)
			{
				for (Country k : countries)
				{
					if (!iPrefix(k.name, q) || k.first < 0)		continue;
					for (int i = 0; i < k.count && i < 20 && n < max; ++i)
					{
						hits[n++] = k.first + i;
						taken[k.first + i] = true;
					}
				}
			}

			for (int pass = 0; pass < 2 && n < max; ++pass)
			{
				int start = n;
				ArrayList<Integer> found = new ArrayList<>();
				for (int i = 0; i < places.length; ++i)
				{
					if (taken[i])	continue;
					boolean m = (pass == 0) ? iPrefix(places[i].ascii, q) : iContains(places[i].ascii, q);
					if (m)	{ found.add(i);  taken[i] = true; }
				}
				Integer[] f = found.toArray(new Integer[0]);
				Arrays.sort(f, (a, b) -> Long.compare(places[b].pop, places[a].pop));
				for (int i = 0; i < f.length && n < max; ++i)	hits[n++] = f[i];
				if (n == start && pass == 1)	break;
			}
			return Arrays.copyOf(hits, n);
		}
}
