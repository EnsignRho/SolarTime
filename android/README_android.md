# SolTime Globe — Android evaluation example

**Snapshot:** 2026-10-05, evaluation draft 3. **Author:** Rick C. Hodgin.
**License:** public domain (CC0 1.0); see [LICENSE](../LICENSE) for the files it does not cover.

This Android source project demonstrates the proposed SolTime civil clock. Read the
companion [SolTime overview](../SolTime_overview.md) for the complete evaluation draft:
the policy, mathematical rules, zone system, limitations and requested review. The app
shows an experimental time display; it does not set Android’s system time.

The app is provided in source-code form for evaluation. No prebuilt APK is included,
by the author's choice: inspect the source, scan it, and build it with your own tools.

The project includes its city/zone data and can be built from this folder alone.
Its build has been checked both inside the evaluation package and from a copy of this
folder alone. Physical-phone use, accessibility and location/lifecycle behavior still
require testing; building an APK is not field validation.

## Build and run

Use JDK 17 or newer and Android SDK 36. The included wrapper selects Gradle 9.1.0; the project
uses Android Gradle Plugin 9.0.1, targets SDK 36 and supports Android API 26 or newer.
It needs a device/emulator with OpenGL ES 2.0. These are this snapshot’s build settings.

Open this `android/` folder in Android Studio, let Gradle sync, and run the `app`
configuration on a phone or emulator. Or, from this folder:

```sh
nice -n 19 env ANDROID_HOME=/path/to/Android/Sdk ./gradlew --no-daemon :app:assembleDebug :app:lintDebug
```

The first build needs network access for Gradle and build dependencies. The wrapper
downloads Gradle 9.1.0 and checks it against the SHA-256 in
`gradle/wrapper/gradle-wrapper.properties`, which is Gradle's published checksum.
Instead of `ANDROID_HOME`, you can set `sdk.dir` in an untracked `local.properties`. The debug APK is written to
`app/build/outputs/apk/debug/app-debug.apk`. With an authorized device connected:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

The app uses platform Activity, LocationManager and GLES 2.0, with no AndroidX or
Google Play Services dependency. The Java clock and calendar classes have no Android
imports. Build tooling still has its own dependencies. The included Gradle wrapper
is accompanied by the upstream distribution’s LICENSE and NOTICE under `gradle/wrapper/`.

## Included data and portable handoff

The following files are included in `data/`:

| File | Purpose |
| --- | --- |
| `places.tsv` | 34,152 place records for search and labels |
| `countries.tsv` | 252 country/territory entries |
| `zones.tsv` | 5,854 qualifying city records, grouped into 4,168 zones |
| `ATTRIBUTION.md` | Sources, transformations and data license credits |

Inside the evaluation package or the full repository, Gradle reads the shared `../data/`
directory. Without that shared registry, it uses this folder’s `data/` snapshot; the
two are identical in this release. It checks that all four files
exist. Keep the tables together from one release; mixing registry and place releases
has not been validated. Gradle packs the TSV tables beside `app/src/main/assets/earth.jpg`.

For another handoff, refresh `android/data/` from the full project’s `data/`, then
copy this folder with its wrapper, source, assets, bundled data and README. Leave out
`.gradle/`, `.idea/`, build directories, `local.properties` and signing credentials.
The accompanying evaluation archive contains a SHA-256 manifest identifying its files.
A new source/data snapshot needs a new manifest and document revision.

Data licenses are described in [data/ATTRIBUTION.md](data/ATTRIBUTION.md). Those terms
cover the data only. The SolTime software and documents are in the public domain by the
author's CC0 1.0 dedication ([LICENSE](../LICENSE)).

## A short evaluation walkthrough

1. Start the app. If location is unavailable, choose **Find**, enter `Indianapolis`,
   and select **Use as my location**. Inspect the date, offset, zone and sun times.
2. Search for **Seattle**, **Honolulu** and **Anchorage**, moving the globe or choosing
   each location. Compare raw geographic behavior with shared city clocks.
3. Choose **Follow its zone’s clock** for a distant city. The displayed clock follows
   that zone; sunrise/sunset still describe the chosen physical location, expressed
   on the followed clock. Those can be on different calendar dates.
4. Toggle **Exact** to compare direct evaluation with the checkpoint display.
5. Select **GPS** to resume location input, or **Unfollow** to stop following a zone.
   The app can announce **New Time Zone** when a change requires a display step.

The app shows current-time behavior; it has no date-simulation controls. Seasonal
examples and input/output values are in the overview. Driving/flight simulations and
C++/Java differential tests are in the full project at the evaluation package's root
(overview section 11.4). Observe a moving display as a passenger or with recorded
location input.

## Reproduce the written examples with a JDK

From this folder, with JDK 17 or newer and without Android SDK or Gradle:

```sh
nice -n 19 mkdir -p build/evaluation-classes
nice -n 19 javac -d build/evaluation-classes \
  app/src/main/java/com/lebeli/soltime/SolTime.java \
  app/src/main/java/com/lebeli/soltime/SolarCalendar.java \
  evaluation/OverviewExamples.java evaluation/SolarAccuracy.java
nice -n 19 java -cp build/evaluation-classes OverviewExamples data/places.tsv
nice -n 19 java -cp build/evaluation-classes com.lebeli.soltime.SolarAccuracy data/places.tsv
```

Compare the outputs with `evaluation/reference-output.txt` and
`evaluation/solar-accuracy-output.txt`. Both programs write UTF-8 with `\n` line ends in
any locale. The first line of the examples is the JDK's tzdb version (the reference uses
2026a) and can differ on another JDK; all lines after it should match.
`SolarAccuracy` measures the model's sunrise and sunset against the Meeus solar formulas
(overview section 5.1).
`evaluation/verification.txt` records the Android checks and their limits; the
package's `VERIFICATION.txt` records the full-project checks.

## The screen

- **Top:** your SolTime clock; date; offset from the input reference; local or followed
  zone and its leader; displayed second length and next checkpoint; today’s sunrise,
  sunset and adjustment window. **FAST** marks fast-motion tracking; **EXACT** marks
  direct evaluation. **New Time Zone** marks a relocation, hard edge, date boundary
  or follow-mode change that requires a step.
- **Globe:** city dots for zones led by cities of a million or more; bright cyan
  member discs and dim outer ramp rings; a red location marker. Overlapping discs
  share their most populous leader’s clock. The terminator uses the approximate solar
  model; it is not an independently validated observation of darkness.
- **Reticle:** local SolTime, nearest place, solar events and zone at the globe cursor.
- **Controls:** **Find**, **Exact**, **GPS**, **Me**, **Follow/Unfollow**. Find accepts
  cities, countries and coordinates, such as `39.77, -86.16` or `64.13N 21.9W`.
- **Credits:** GeoNames, GHSL and U.S. Census data credits appear on screen.

## Implementation map

| Class | Responsibility |
| --- | --- |
| `SolTime.java` | Parameters, raw field, polar basis, zones and device clock |
| `SolarCalendar.java` | Gregorian dates and common/leap-year smoothing tables |
| `LocationFixes.java` | Location age, accuracy and motion-prediction policy |
| `Cities.java` | Place/registry loading, search and nearest-place lookup |
| `GlobeRenderer.java` | OpenGL globe, modeled daylight, markers and zone rings |
| `GlobeView.java` | Rotation and zoom gestures |
| `MainActivity.java` | Permissions, location input, display updates and controls |

Java source is under `app/src/main/java/com/lebeli/soltime/`. The application uses
`clockCreateUnix()` and Unix-seconds field APIs. Its clock input is the Android wall
clock; it does not supply a separate authoritative UTC source or a leap-second policy.

`earth.jpg` is identified in the project as NASA Blue Marble *land_ocean_ice*,
2048 × 1024. If absent, the renderer falls back to a plain ocean and graticule.

## Limitations relevant to evaluation

- Dates use Gregorian leap-year and century rules. Calendar APIs accept years
  1600–9999, but that numerical range is not a claim of astronomical validity.
- The legacy synthetic 366-day API remains for simulations and registry design.
- The 06:45 ceiling is for **modeled sunrise on the raw field with its own basis**.
  Shared zones, ramps, polar borrowing and following a distant zone can show later
  sunrise. Terrain, altitude and variable refraction are not modeled.
- Both precise and approximate location permissions are supported. Cached fixes must
  be no more than 60 seconds old; out-of-order fixes and coarse replacements for a
  fresh precise fix are rejected. The app stops motion prediction after five seconds
  without a new fix and labels location age/accuracy. It resumes updates on return.
- Exact mode, manual location and followed-zone selection persist across app restarts.
- Solar-event dates follow the physical location; adjustment-window dates follow the
  clock owner. A followed clock can display a different date.
- Widgets, alarms, background operation, local-to-reference scheduling and recurring
  appointments are not implemented. Registry version/update coordination is unfinished.
- The build/lint record is software evidence only. Small screens, keyboard, navigation,
  enlarged text, TalkBack, location permissions, battery use and lifecycle behavior
  need device evaluation. Existing lint warnings remain visible.

The overview records the full-project and Android checks rerun for draft 3 separately
from the earlier reported results. It also lists the standards decisions that remain.
