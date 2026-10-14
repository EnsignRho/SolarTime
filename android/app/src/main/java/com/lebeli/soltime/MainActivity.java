//////////
//
// MainActivity.java -- SolTime Globe (spec §10, §11).
//
// Top panel:     YOUR clock -- the device clock (§10):  fed by GPS (position, speed, heading), or
//                by a place / coordinates you chose; it recomputes the true SolTime at its
//                checkpoints and sets the length of its seconds to stay on it.  Date, offset vs
//                UTC, your zone, today's sun, tonight's adjustment; "New Time Zone" when it jumps.
// Globe:         spin it; the fixed center reticle picks a surface point.
// Bottom panel:  the reticle's SolTime, the nearest GeoNames place, its sun and zone; the buttons
//                (Find, Exact, GPS, Me, Follow) and the data credit.
//
// Following a zone (spec §8):  anyone may keep any zone's clock, wherever they are.  Follow puts
// your clock on the zone under the reticle (or, in Find, a place's zone); Unfollow returns it to
// the clock of where you are.
//
// Location:  plain LocationManager (no Play Services) -- last-known fix at once, then live
// updates.  The place data loads and the zone registry builds on a worker thread at startup.
// Everything else ticks on a 200 ms handler; the globe renders continuously (live terminator).
//
//////
package com.lebeli.soltime;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Typeface;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.io.InputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

public class MainActivity extends Activity implements LocationListener
{
	private static final int	REQ_LOCATION	= 1;
	private static final int	TICK_MS			= 200;
	private static final int	FIND_MAX		= 60;
	private static final double	BIG_ZONE_POP	= 1.0e6;	// The globe dots the zones led by cities this size

	private GlobeView			globe;
	private TextView			youLabel, bigTime, youInfo, youSun, zoneText, hereText, statusText;
	private Button				exactBtn, followBtn;
	private LocationManager		locMgr;
	private SharedPreferences prefs;
	private final LocationFixes fixes = new LocationFixes();
	private boolean resumed, fixFrozen;
	private String savedFollow = "";
	private final Handler		handler		= new Handler(Looper.getMainLooper());
	private final SolTime		solTime		= new SolTime();
	private final SimpleDateFormat	dateFmt	= new SimpleDateFormat("EEE MMM d", Locale.US);

	// Place data and the zone registry (loaded / built on a worker thread)
	private volatile Cities				data		= null;
	private volatile SolTime.Registry	registry	= null;
	private volatile String				loadStatus	= "Loading places and zones...";
	private volatile int[]				zoneMembers	= null;		// Per leader:  the cities in its merged zone

	// Your clock
	private SolTime.Clock		clock;
	private boolean				clockOnRegistry	= false;
	private boolean				exact			= false;
	private int					follow			= -1;		// The zone (leader) your clock follows, or -1
	private long				zoneUntilMs		= 0;
	private double				lastD = Double.NaN, lastUtc = Double.NaN, showRate = 1.0;

	// Where you are:  GPS, or a place / coordinates you chose
	private boolean				manual			= false;
	private double				youLat = Double.NaN, youLon = Double.NaN;
	private String				youName			= "";
	private boolean				centeredOnce	= false;

	// Caches (the reticle's nearest place; today's sun at your place)
	private double				nearLat = Double.NaN, nearLon = Double.NaN;
	private String				nearText		= "";
	private String				sunKey			= "", sunText = "";

	private final Runnable tick = new Runnable()
	{
		@Override
		public void run()
		{
			iUpdateClocks();
			handler.postDelayed(this, TICK_MS);
		}
	};


	//////////
	// Lifecycle
	//////
		@Override
		protected void onCreate(Bundle savedInstanceState)
		{
			super.onCreate(savedInstanceState);
			dateFmt.setTimeZone(TimeZone.getTimeZone("UTC"));
			prefs = getSharedPreferences("soltime", MODE_PRIVATE);
			exact = prefs.getBoolean("exact", false);
			manual = prefs.getBoolean("manual", false);
			if (manual) {
				youLat = Double.longBitsToDouble(prefs.getLong("latitude", Double.doubleToLongBits(Double.NaN)));
				youLon = Double.longBitsToDouble(prefs.getLong("longitude", Double.doubleToLongBits(Double.NaN)));
				youName = prefs.getString("place", "Saved location");
				if (!Double.isFinite(youLat) || !Double.isFinite(youLon) || Math.abs(youLat) > 90 || Math.abs(youLon) > 180) {
					manual = false; youLat = youLon = Double.NaN;
				}
			}
			savedFollow = prefs.getString("follow", "");
			clock = solTime.clockCreateUnix(null);
			clock.exact(exact);

			// Built in code so nothing depends on resource files
			FrameLayout root = new FrameLayout(this);
			globe = new GlobeView(this);
			root.addView(globe);

			TextView reticle = iText(16, Gravity.CENTER);
			reticle.setText("+");
			reticle.setTextSize(28.0f);
			reticle.setBackgroundColor(Color.TRANSPARENT);
			root.addView(reticle, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));

			// Top:  your clock
			LinearLayout top = iPanel();
			youLabel = iText(13, Gravity.CENTER_HORIZONTAL);
			bigTime  = iText(44, Gravity.CENTER_HORIZONTAL);
			youInfo  = iText(14, Gravity.CENTER_HORIZONTAL);
			youSun   = iText(13, Gravity.CENTER_HORIZONTAL);
			zoneText = iText(16, Gravity.CENTER_HORIZONTAL);
			zoneText.setBackgroundColor(0xCCB71C1C);
			zoneText.setText("  New Time Zone  ");
			zoneText.setVisibility(View.GONE);
			top.addView(youLabel);
			top.addView(bigTime);
			top.addView(youInfo);
			top.addView(youSun);
			top.addView(zoneText);
			bigTime.setText("--:--:--");
			root.addView(top, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP));

			// Bottom:  the reticle, the buttons, the credit
			LinearLayout bottom = iPanel();
			hereText   = iText(14, Gravity.CENTER_HORIZONTAL);
			statusText = iText(11, Gravity.CENTER_HORIZONTAL);
			LinearLayout buttons = new LinearLayout(this);
			buttons.setOrientation(LinearLayout.HORIZONTAL);
			buttons.setGravity(Gravity.CENTER_HORIZONTAL);
			Button findBtn = iButton(buttons, "Find");
			exactBtn       = iButton(buttons, "Exact: off");
			Button gpsBtn  = iButton(buttons, "GPS");
			Button meBtn   = iButton(buttons, "Me");
			followBtn      = iButton(buttons, "Follow");
			findBtn.setOnClickListener(v -> iShowFind());
			exactBtn.setOnClickListener(v -> iToggleExact());
			gpsBtn.setOnClickListener(v -> iUseGps());
			meBtn.setOnClickListener(v -> { if (!Double.isNaN(youLat)) globe.renderer.lookAt(youLat, youLon); });
			followBtn.setOnClickListener(v -> iFollowButton());
			bottom.addView(hereText);
			bottom.addView(buttons);
			bottom.addView(statusText);
			root.addView(bottom, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM));
			setContentView(root);
			// BUG_FIX_PLAN_2026-10-05.md FIX-06 — fixed 2026-10-05 16:49:54 EDT (UTC-04:00).
			iProtectInsets(root, getWindow());
			exactBtn.setText(exact ? "Exact: on" : "Exact: off");
			if (manual) {
				globe.renderer.userLat = youLat; globe.renderer.userLon = youLon;
				globe.renderer.lookAt(youLat, youLon); centeredOnce = true;
			}

			// The places and the registry, off the UI thread
			new Thread(this::iLoadData, "soltime-data").start();

			locMgr = (LocationManager)getSystemService(Context.LOCATION_SERVICE);
			if (!manual && !iHasLocationPermission()) iRequestLocation();
		}

		@Override
		public void onRequestPermissionsResult(int req, String[] perms, int[] grants)
		{
			super.onRequestPermissionsResult(req, perms, grants);
			if (req == REQ_LOCATION && iHasLocationPermission()) iStartLocation();
		}

		@Override
		protected void onResume()
		{
			super.onResume();
			resumed = true;
			globe.onResume();
			iStartLocation();
			handler.removeCallbacks(tick);
			handler.post(tick);
		}

		@Override
		protected void onPause()
		{
			super.onPause();
			resumed = false;
			globe.onPause();
			handler.removeCallbacks(tick);
			iStopLocation();
			iSaveSettings();
		}

		private boolean iHasLocationPermission() {
			return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
			    || checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED;
		}
		private void iRequestLocation() {
			requestPermissions(new String[] { Manifest.permission.ACCESS_FINE_LOCATION,
			    Manifest.permission.ACCESS_COARSE_LOCATION }, REQ_LOCATION);
		}
		private void iStopLocation() {
			if (locMgr != null) try { locMgr.removeUpdates(this); } catch (SecurityException ignored) { }
		}
		private String iZoneKey(int i) {
			Cities.ZoneCity z = data.zones[i];
			return z.cc + "|" + z.name + "|" + z.lat + "|" + z.lon;
		}
		private void iSaveSettings() {
			if (prefs == null) return;
			prefs.edit().putBoolean("exact", exact).putBoolean("manual", manual)
			    .putLong("latitude", Double.doubleToLongBits(youLat)).putLong("longitude", Double.doubleToLongBits(youLon))
			    .putString("place", youName).putString("follow", savedFollow).apply();
		}


	//////////
	// Data:  the places and the zone registry (worker thread)
	//////
		private void iLoadData()
		{
			Cities c = null;
			try (InputStream p = getAssets().open("places.tsv");
				 InputStream k = getAssets().open("countries.tsv");
				 InputStream b = getAssets().open("zones.tsv"))
			{
				c = Cities.load(p, k, b);
			}
			catch (Exception ex) { c = null; }
			if (c == null)
			{
				loadStatus = "The place data is missing (assets places.tsv / countries.tsv / zones.tsv)";
				return;
			}
			data = c;

			// Dots:  the zones led by cities of a million or more (every zone gets its rings)
			int nBig = 0;
			for (Cities.ZoneCity z : c.zones)	if (z.leader && z.population >= BIG_ZONE_POP)	++nBig;
			double[] lat = new double[nBig], lon = new double[nBig];
			nBig = 0;
			for (Cities.ZoneCity z : c.zones)	if (z.leader && z.population >= BIG_ZONE_POP)	{ lat[nBig] = z.lat;  lon[nBig] = z.lon;  ++nBig; }
			globe.renderer.setDots(lat, lon);

			loadStatus = "Building the zone registry...";
			SolTime.Registry r = solTime.registryCreate(c.zoneArray());
			if (r == null)
			{
				loadStatus = "The zone registry was refused (zones.tsv):  showing the raw field";
				return;
			}
			int[] members = new int[r.count()];
			for (int i = 0; i < r.count(); ++i)		++members[r.leader(i)];
			globe.renderer.setZones(r);
			zoneMembers	= members;
			registry	= r;
			loadStatus	= "";
		}


	//////////
	// Location plumbing
	//////
		private void iStartLocation()
		{
			if (!resumed || manual || locMgr == null || !iHasLocationPermission()) return;
			iStopLocation();
			try
			{
				boolean fine = checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
				for (String provider : new String[] {LocationManager.NETWORK_PROVIDER, LocationManager.GPS_PROVIDER}) {
					if (!locMgr.getAllProviders().contains(provider) || (!fine && provider.equals(LocationManager.GPS_PROVIDER))) continue;
					Location last = locMgr.getLastKnownLocation(provider);
					if (last != null) onLocationChanged(last);
					// Subscribe even while disabled so enabling location resumes updates.
					locMgr.requestLocationUpdates(provider, 1000, 0.0f, this);
				}
			}
			catch (SecurityException ignored) { }
		}

		@Override
		public void onLocationChanged(Location loc)
		{
			if (manual)		return;
			double elapsed = SystemClock.elapsedRealtimeNanos() / 1.0e9;
			LocationFixes.Fix sample = new LocationFixes.Fix(loc.getLatitude(), loc.getLongitude(),
			    loc.hasAccuracy() ? loc.getAccuracy() : Double.NaN, loc.getElapsedRealtimeNanos() / 1.0e9,
			    loc.hasSpeed() ? loc.getSpeed() * 3.6 : 0, loc.hasBearing() ? loc.getBearing() : Double.NaN);
			if (!fixes.accept(sample, elapsed)) return;
			youLat	= loc.getLatitude();
			youLon	= loc.getLongitude();
			youName	= "";
			globe.renderer.userLat = youLat;
			globe.renderer.userLon = youLon;
			iFeedLocation(System.currentTimeMillis() / 1000.0, elapsed);
			if (!centeredOnce)
			{
				globe.renderer.lookAt(youLat, youLon);
				centeredOnce = true;
			}
		}

		private void iUseGps()
		{
			manual = false;
			youLat = youLon = Double.NaN;
			globe.renderer.userLat = globe.renderer.userLon = Double.NaN;
			fixes.latest = null;
			iResetClock();
			if (iHasLocationPermission()) iStartLocation(); else iRequestLocation();
			iSaveSettings();
		}

		private void iResetClock() {
			clock = solTime.clockCreateUnix(registry);
			clockOnRegistry = registry != null;
			clock.exact(exact);
			if (follow >= 0) clock.followZone(follow);
			lastD = lastUtc = Double.NaN;
		}
		private void iFeedLocation(double utc, double elapsed) {
			if (fixes.latest == null) return;
			double speed = fixes.speed(elapsed), age = fixes.age(elapsed);
			fixFrozen = age >= LocationFixes.PREDICT_SECS;
			// The position's real age is measured with Android's monotonic timestamp.
			clock.fix(fixFrozen ? utc : utc - age, youLat, youLon, speed,
			    speed > 0 ? fixes.latest.heading : 0);
		}

		private void iUsePlace(double lat, double lon, String name)
		{
			manual	= true;
			iStopLocation();
			youLat	= lat;
			youLon	= lon;
			youName	= name;
			globe.renderer.userLat = lat;
			globe.renderer.userLon = lon;
			globe.renderer.lookAt(lat, lon);
			iSaveSettings();
		}

		// The merged zone (its leader) at a point, or -1
		private int iZoneAt(double lat, double lon)
		{
			SolTime.Registry r = registry;
			if (r == null)	return -1;
			int[] idx = new int[8];
			int   n   = Math.min(r.zonesAt(lat, lon, idx, null), idx.length);
			return (n > 0) ? idx[n - 1] : -1;
		}

		// Your clock follows zone L wherever you are (-1:  back to the clock of where you are)
		private void iFollow(int L)
		{
			if (L >= 0 && registry == null)		return;
			follow = L;
			savedFollow = L >= 0 ? iZoneKey(L) : "";
			clock.followZone(L);
			sunKey = "";
			followBtn.setText((L >= 0) ? "Unfollow" : "Follow");
			iSaveSettings();
		}

		// Follow the zone under the reticle;  Unfollow when following
		private void iFollowButton()
		{
			if (follow >= 0)	{ iFollow(-1);  return; }
			int L = iZoneAt(globe.renderer.centerLat, globe.renderer.centerLon);
			if (L >= 0)		iFollow(L);
			else			new AlertDialog.Builder(this).setMessage((registry == null) ? loadStatus : "No zone under the reticle:  turn the globe to a city's zone")
								.setPositiveButton("OK", null).show();
		}

		private void iToggleExact()
		{
			exact = !exact;
			clock.exact(exact);
			exactBtn.setText(exact ? "Exact: on" : "Exact: off");
			iSaveSettings();
		}


	//////////
	// The clocks (200 ms tick)
	//////
		private double iOffset(double lat, double lon, double utc)
		{
			SolTime.Registry r = registry;
			return (r != null) ? r.offsetUnixSecs(lat, lon, utc) : solTime.offsetUnixSecs(lat, lon, utc);
		}

		// "Denver (7 cities)" -- a merged zone's leader, and how many cities share its clock
		private String iZoneName(int L)
		{
			Cities c = data;
			int[]  m = zoneMembers;
			if (c == null)	return "";
			return (m != null && m[L] > 1) ? String.format(Locale.US, "%s (%d cities)", c.zones[L].name, m[L]) : c.zones[L].name;
		}

		// "[Denver zone]" when locked in a zone, "[~Denver zone]" on its edge ramp, "" outside
		private String iZoneTag(double lat, double lon)
		{
			SolTime.Registry r = registry;
			if (r == null || data == null)		return "";
			int[]    idx = new int[8];
			double[] w   = new double[8];
			int      n   = Math.min(r.zonesAt(lat, lon, idx, w), idx.length);
			if (n <= 0)		return "";
			return String.format(Locale.US, "[%s%s zone]", (w[n - 1] >= 1.0) ? "" : "~", iZoneName(idx[n - 1]));
		}

		private void iUpdateClocks()
		{
			long   now = System.currentTimeMillis();
			double utc = now / 1000.0;

			// The registry arrives:  your clock moves onto it
			SolTime.Registry r = registry;
			if (r != null && !clockOnRegistry)
			{
				if (!savedFollow.isEmpty()) for (int i = 0; i < r.count(); ++i)
					if (savedFollow.equals(iZoneKey(i))) { follow = r.leader(i); break; }
				iResetClock();
				followBtn.setText(follow >= 0 ? "Unfollow" : "Follow");
				if (!manual) iFeedLocation(utc, SystemClock.elapsedRealtimeNanos() / 1.0e9);
			}
			if (manual)		clock.fix(utc, youLat, youLon, 0.0, 0.0);
			double fixAge = fixes.age(SystemClock.elapsedRealtimeNanos() / 1.0e9);
			if (!manual && !fixFrozen && fixAge >= LocationFixes.PREDICT_SECS && fixes.latest != null)
				iFeedLocation(utc, SystemClock.elapsedRealtimeNanos() / 1.0e9);

			// Your clock
			int[]  fl = new int[1];
			double D  = clock.show(utc, fl);
			if ((fl[0] & SolTime.Clock.NEW_ZONE) != 0)		zoneUntilMs = now + 6000;
			zoneText.setVisibility((now < zoneUntilMs) ? View.VISIBLE : View.GONE);
			if (!Double.isNaN(D))
			{
				if (!Double.isNaN(lastD) && utc > lastUtc)	showRate = (D - lastD) / (utc - lastUtc);
				lastD	= D;
				lastUtc	= utc;

				double off	= D - utc;
				String date	= dateFmt.format(new Date((long)Math.floor(D * 1000.0)));
				String mode	= exact ? "  EXACT" : ((fl[0] & SolTime.Clock.FAST) != 0) ? "  FAST" : "";
				double next	= clock.nextCheckUtc();
				bigTime.setText(SolTime.formatHMS(D));
				youInfo.setText(String.format(Locale.US, "%s   UTC%s   %s\n1 clock second = %.6f s%s%s",
					date, SolTime.formatOffset(off), (follow >= 0) ? "[following the " + iZoneName(follow) + " zone]" : iZoneTag(youLat, youLon),
					(showRate > 0.0) ? 1.0 / showRate : 0.0,
					(!exact && !Double.isNaN(next)) ? String.format(Locale.US, "   next check %.0f s", Math.max(0.0, next - utc)) : "", mode));
				youSun.setText(iSunAndNight(utc));
			} else {
				bigTime.setText("--:--:--"); youInfo.setText(""); youSun.setText("");
			}
			youLabel.setText(Double.isNaN(youLat)
			    ? (follow >= 0 ? "YOUR CLOCK  --  following " + iZoneName(follow)
			        : iHasLocationPermission() ? "YOUR CLOCK  --  waiting for location (or Find a place)"
			        : "YOUR CLOCK  --  tap GPS to allow location, or Find a place")
			    : String.format(Locale.US, "YOUR CLOCK  --  %s  %s", manual ? youName : "Location", iFormatLL(youLat, youLon)));

			// The reticle
			double clat = globe.renderer.centerLat, clon = globe.renderer.centerLon;
			double coff = iOffset(clat, clon, utc);
			hereText.setText(String.format(Locale.US, "%s   UTC%s   %s\n%s\n%s   %s",
				SolTime.formatHMS(utc + coff), SolTime.formatOffset(coff), iFormatLL(clat, clon),
				iNear(clat, clon), iDaylight(clat, clon, utc), iZoneTag(clat, clon)));

			String status = loadStatus;
			if (!manual && fixes.latest != null) status += String.format(Locale.US,
			    "%s%sfix %.0f s ago, accuracy ±%.0f m", status.isEmpty() ? "" : "\n",
			    fixAge >= LocationFixes.PREDICT_SECS ? "Using last known location; " : "Location ", fixAge, fixes.latest.accuracy);
			statusText.setText(status.isEmpty() ? Cities.CREDIT : status + "\n" + Cities.CREDIT);
		}

		// "near Oslo, Norway (3 km)", recomputed only when the reticle moves
		private String iNear(double lat, double lon)
		{
			Cities c = data;
			if (c == null)	return "";
			if (lat != nearLat || lon != nearLon)
			{
				double[] km = new double[1];
				int      i  = c.nearest(lat, lon, km);
				nearText = (i >= 0) ? String.format(Locale.US, "near %s (%.0f km)", c.label(i), km[0]) : "";
				nearLat  = lat;
				nearLon  = lon;
			}
			return nearText;
		}

		// Today's sun at your place in your clock, and tonight's adjustment -- once a minute
		private String iSunAndNight(double utc)
		{
			if (Double.isNaN(youLat))	return "";
			String key = String.format(Locale.US, "%.4f %.4f %d %b %d", youLat, youLon, (long)(utc / 60.0), registry != null, follow);
			if (key.equals(sunKey))		return sunText;
			sunKey = key;

			// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
			// The sun belongs to the physical day's schedule; only its display uses the followed clock.
			double date = solTime.solarDateUnix(youLat, youLon, utc);
			double[] rsn = new double[3];
			int      st  = solTime.sunTimesUnix(youLat, youLon, date, rsn);
			String   sun;
			if (st == 0)
				sun = String.format(Locale.US, "sunrise %s   sunset %s",
					SolTime.formatHMS(rsn[0] + iYourOffset(rsn[0])).substring(0, 5),
					SolTime.formatHMS(rsn[1] + iYourOffset(rsn[1])).substring(0, 5));
			else
				sun = (st > 0) ? "midnight sun" : "polar night";

			// Tonight:  the clock you keep (the zone you follow, or your zone's leader when locked in one)
			double   cLat = youLat, cLon = youLon;
			SolTime.Registry r = registry;
			if (r != null && follow >= 0)
			{
				cLat = r.zone(follow).latDeg;
				cLon = r.zone(follow).lonDegE;

			} else if (r != null) {
				int[]    idx = new int[8];
				double[] w   = new double[8];
				int      n   = Math.min(r.zonesAt(youLat, youLon, idx, w), idx.length);
				if (n > 0 && w[n - 1] >= 1.0)	{ cLat = r.zone(idx[n - 1]).latDeg;  cLon = r.zone(idx[n - 1]).lonDegE; }
			}
			double[] w = new double[2];
			String   night = "";
			// BUG_FIX_PLAN_2026-10-05.md FIX-02 — fixed 2026-10-05 16:44:47 EDT (UTC-04:00).
			// The adjustment belongs to the clock owner.
			double nightDate = solTime.solarDateUnix(cLat, cLon, utc);
			if (solTime.nightWindowUnix(cLat, cLon, nightDate, w) == 0)
			{
				double o0 = solTime.offsetUnixSecs(cLat, cLon, w[0] - 3600);
				double o1 = solTime.offsetUnixSecs(cLat, cLon, w[1] + 3600);
				night = String.format(Locale.US, "\ntonight %s-%s:  tomorrow's clock moves %+.1f s",
					SolTime.formatHMS(w[0] + o0).substring(0, 5), SolTime.formatHMS(w[1] + o1).substring(0, 5), o1 - o0);
			}
			double bl = solTime.basisLatUnix(youLat, date);
			String basis = (follow >= 0) ? "   (on the " + data.zones[follow].name + " zone's clock)"
						 : (Math.abs(bl - youLat) > 1.0e-9) ? String.format(Locale.US, "   (basis %.2f)", bl) : "";
			sunText = sun + basis + night;
			return sunText;
		}

		// The clock you see:  the zone you follow, or the clock where you are
		private double iYourOffset(double utc)
		{
			SolTime.Registry r = registry;
			if (r != null && follow >= 0)	return r.zoneOffsetUnixSecs(follow, utc);
			return iOffset(youLat, youLon, utc);
		}

		private String iDaylight(double lat, double lon, double utc)
		{
			double[] rsn  = new double[3];
			double date = Math.floor((utc + iOffset(lat, lon, utc)) / SolTime.DAY_SECS) * SolTime.DAY_SECS;
			int      st   = solTime.sunTimesUnix(lat, lon, date, rsn);
			if (st == +1)	return "midnight sun";
			if (st == -1)	return "polar night";
			return String.format(Locale.US, "sun %s-%s",
				SolTime.formatHMS(rsn[0] + iOffset(lat, lon, rsn[0])).substring(0, 5),
				SolTime.formatHMS(rsn[1] + iOffset(lat, lon, rsn[1])).substring(0, 5));
		}

		private static String iFormatLL(double lat, double lon)
		{
			return String.format(Locale.US, "%.2f%s %.2f%s",
				Math.abs(lat), (lat >= 0.0) ? "N" : "S",
				Math.abs(lon), (lon >= 0.0) ? "E" : "W");
		}


	//////////
	// Find:  a city, a country, or typed coordinates
	//////
		private void iShowFind()
		{
			final Cities c = data;
			if (c == null)
			{
				new AlertDialog.Builder(this).setMessage(loadStatus).setPositiveButton("OK", null).show();
				return;
			}

			LinearLayout box = new LinearLayout(this);
			box.setOrientation(LinearLayout.VERTICAL);
			box.setPadding(24, 16, 24, 0);
			final EditText query = new EditText(this);
			query.setHint("city, country, or 39.77, -86.16");
			query.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
			final ListView list = new ListView(this);
			final ArrayList<String> labels = new ArrayList<>();
			final ArrayList<double[]> spots = new ArrayList<>();
			final ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, labels);
			list.setAdapter(adapter);
			box.addView(query);
			// BUG_FIX_PLAN_2026-10-05.md FIX-06 — fixed 2026-10-05 16:49:54 EDT (UTC-04:00).
			// The list takes the space left by the query and dialog buttons, even with the IME open.
			box.addView(list, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f));

			final AlertDialog dlg = new AlertDialog.Builder(this)
				.setTitle("Find a place")
				.setView(box)
				.setNegativeButton("Close", null)
				.create();

			query.addTextChangedListener(new TextWatcher()
			{
				@Override public void beforeTextChanged(CharSequence s, int a, int b, int n)	{ }
				@Override public void onTextChanged(CharSequence s, int a, int b, int n)		{ }
				@Override public void afterTextChanged(Editable s)
				{
					labels.clear();
					spots.clear();
					String   q   = s.toString();
					double   utc = System.currentTimeMillis() / 1000.0;
					double[] ll  = new double[2];
					if (Cities.parseCoords(q, ll))
					{
						labels.add(String.format(Locale.US, "Typed coordinates  %s   %s", iFormatLL(ll[0], ll[1]), SolTime.formatHMS(utc + iOffset(ll[0], ll[1], utc))));
						spots.add(new double[] { ll[0], ll[1] });
					}
					for (int i : c.search(q, FIND_MAX))
					{
						Cities.Place p = c.places[i];
						labels.add(String.format(Locale.US, "%s   %s", c.label(i), SolTime.formatHMS(utc + iOffset(p.lat, p.lon, utc))));
						spots.add(new double[] { p.lat, p.lon });
					}
					adapter.notifyDataSetChanged();
				}
			});

			list.setOnItemClickListener((parent, view, pos, id) ->
			{
				final double[] ll    = spots.get(pos);
				final String   label = labels.get(pos).replaceAll("\\s+\\d\\d:\\d\\d:\\d\\d$", "");
				new AlertDialog.Builder(this)
					.setTitle(label)
					.setItems(new String[] { "Show on the globe", "Use as my location", "Follow its zone's clock", "Cancel" }, (d, which) ->
					{
						if (which == 0)			{ globe.renderer.lookAt(ll[0], ll[1]);  dlg.dismiss(); }
						else if (which == 1)	{ iUsePlace(ll[0], ll[1], label);  dlg.dismiss(); }
						else if (which == 2)
						{
							int L = iZoneAt(ll[0], ll[1]);
							if (L >= 0)		{ iFollow(L);  dlg.dismiss(); }
							else			new AlertDialog.Builder(this).setMessage((registry == null) ? loadStatus : "No zone at " + label).setPositiveButton("OK", null).show();
						}
					})
					.show();
			});
			dlg.show();
			// BUG_FIX_PLAN_2026-10-05.md FIX-06 — fixed 2026-10-05 16:49:54 EDT (UTC-04:00).
			Window findWindow = dlg.getWindow();
			if (findWindow != null) {
				findWindow.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
				findWindow.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
				findWindow.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
				iProtectInsets(findWindow.findViewById(android.R.id.content), findWindow);
			}
		}


	//////////
	// View helpers:  monospace white text on a scrim
	//////
		// BUG_FIX_PLAN_2026-10-05.md FIX-06 — fixed 2026-10-05 16:49:54 EDT (UTC-04:00).
		// Keep controls clear of bars, cutouts and the keyboard; padding is absolute, not cumulative.
		private void iProtectInsets(View content, Window window)
		{
			if (Build.VERSION.SDK_INT >= 30) {
				window.setDecorFitsSystemWindows(false);
				final int left = content.getPaddingLeft(), top = content.getPaddingTop();
				final int right = content.getPaddingRight(), bottom = content.getPaddingBottom();
				content.setOnApplyWindowInsetsListener((view, insets) -> {
					Insets safe = insets.getInsets(WindowInsets.Type.systemBars()
					    | WindowInsets.Type.displayCutout() | WindowInsets.Type.ime());
					view.setPadding(left + safe.left, top + safe.top, right + safe.right, bottom + safe.bottom);
					return WindowInsets.CONSUMED;
				});
				content.requestApplyInsets();
			} else {
				// On API 26–29 the platform handles system insets and dialog adjustResize.
				content.setFitsSystemWindows(true);
			}
		}

		private LinearLayout iPanel()
		{
			LinearLayout l = new LinearLayout(this);
			l.setOrientation(LinearLayout.VERTICAL);
			l.setBackgroundColor(0x88000000);
			l.setPadding(16, 12, 16, 12);
			return l;
		}

		private TextView iText(float sp, int gravity)
		{
			TextView t = new TextView(this);
			t.setTextColor(Color.WHITE);
			t.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
			t.setTextSize(sp);
			t.setGravity(gravity);
			t.setShadowLayer(4.0f, 0.0f, 0.0f, Color.BLACK);
			return t;
		}

		private Button iButton(LinearLayout row, String text)
		{
			Button b = new Button(this);
			b.setText(text);
			b.setAllCaps(false);
			row.addView(b, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
			return b;
		}

		// LocationListener leftovers (pre-API-29 compatibility)
		@Override public void onStatusChanged(String provider, int status, Bundle extras)	{ }
		@Override public void onProviderEnabled(String provider)							{ }
		@Override public void onProviderDisabled(String provider)							{ }
}
