# SolTime bug fixes and verification

Requested by Rick on October 5, 2026. This plan tracks corrections from the app review,
their regression tests, and the changes made.
Times below and in source comments use America/Indiana/Indianapolis: EDT, UTC−04:00.

## Plan and status

| Reference | Work | Status |
| --- | --- | --- |
| FIX-01 | Correct relocation and stationary-to-fast-motion replanning in C++ and Java | Fixed; regression, integration and differential checks passed |
| FIX-02 | Select solar events by the physical location's date when following a distant zone | Fixed; C++/Java regressions and desktop/Android builds passed |
| FIX-03 | Keep explicitly supplied ramps outside earlier zones' locked discs | Fixed; locked-disc and continuity checks passed; published registry unchanged |
| FIX-04 | Reject empty and malformed publisher input before replacing output | Fixed; publisher and address/UB sanitizer regressions passed |
| FIX-05 | Make the public C header self-contained and compile/link it from C | Fixed; isolated-header C11 client compiled, linked and passed |
| FIX-06 | Apply Android system/keyboard insets and make the Find dialog fit its available height | Implemented; assembly/lint passed; physical-device interaction unverified |
| FIX-07 | Clarify the sunrise guarantee, record feature gaps, and complete integration checks | Complete; results and remaining limits recorded below |

Each source correction carries `BUG_FIX_PLAN_2026-10-05.md FIX-NN` and its actual
implementation date/time. Completion is recorded only after the relevant checks pass.

## FIX-01 Clock replanning

Before: a fresh fix reporting fast motion could reuse a stationary five-minute checkpoint.
A relocation from 40°N, 86°W to 40°N, 0° at 900 km/h left the display 20,640 seconds
behind and catching up at approximately 71 times real speed, without a zone-change flag.
The same shortcut also accepted relocations during an existing flight.

Plan: retain smooth ordinary turns only for an already-fast clock, a checkpoint within
one minute, and a small display error. Otherwise run the normal correction and adaptive
planning path. Test stationary-to-flight changes, relocations while stationary and already
flying, ordinary turns, flags, and deadlines in both periodic and Gregorian clocks.

## FIX-02 Solar event dates

Before: Tokyo following San Francisco used San Francisco's calendar date to select
Tokyo's sunrise, yielding an event approximately 24 hours too early.

Plan: select sunrise, sunset, and basis latitude using the physical location's own
SolTime date; render those instants in the chosen clock. Select the overnight adjustment
using the clock owner's date separately. Test opposite date offsets, New Year, and leap day.

## FIX-03 Supplied ramps

Before: two 10 km discs at 40°N, 0° and 40°N, 1°, with supplied 100 km ramps, made the
first city's center differ from its own zone by 10.45 seconds.

Plan: apply the existing gap limit to supplied positive ramps in both implementations.
Preserve hard edges and nonconflicting widths. Test both locked interiors and continuity
at the adjoining boundary, and compare the published registry across C++ and Java.

## FIX-04 Publisher input

Before: empty input reached an out-of-bounds statistics read; malformed numeric fields
were silently converted to zero; short rows were skipped. Output could be replaced first.

Plan: validate complete rows with line/column diagnostics, reject nonfinite/out-of-range
values and empty registries, detect read errors, and publish through a checked temporary
file. Test that invalid input leaves existing output unchanged, including in-place use;
test valid rebuilding and run the failing cases with address/undefined-behavior sanitizers.

## FIX-05 C interface

Before: `#include "soltime.h"` failed in C because of C++ types in a sibling header
and missing structure typedefs.

Plan: publish only standard fixed-width C types and structure typedefs in the standalone
header, keeping the ABI unchanged. Build a C client, link to the actual shared library,
and exercise field, registry, and clock calls. Keep private implementation types private.

## FIX-06 Android layout

The Android 16 layout risk from the review is included as preventive compatibility work.
The full-screen root has no system-bar/cutout/keyboard inset handling; Find uses a fixed
900-pixel list height. Apply platform insets with API-level guards and size the dialog to
the available window. Verify compilation and lint; device interaction needs an emulator
or phone, which is unavailable in this environment (`/dev/kvm` is absent).

Implementation follows the platform's [inset guidance](https://developer.android.com/develop/ui/views/layout/edge-to-edge)
and [WindowInsets API](https://developer.android.com/reference/android/view/WindowInsets).
Still to check on a device: API 26–29 and 30–36; small screens and rotation; gesture
and three-button navigation; Find with the keyboard open; enlarged text and TalkBack.

## FIX-07 Documentation and final checks

Clarify that the 06:45 sunrise bound applies to the raw field where the location keeps
its own basis. A shared city clock can place another neighborhood's sunrise later, and
following a distant zone does not preserve that bound. Preserve the existing clock policy.

Record these feature gaps as future work: local-to-UTC scheduling and recurrence rules;
stable zone IDs, registry versions and coordinated update dates; widgets/alarms/background
integration; broad independent astronomical validation; physical-phone accessibility,
permission, location and lifecycle testing. They are product additions beyond this bug-fix
pass, not claimed as implemented.

Final checks: CTest, C++/Java differential comparisons, sanitizer regressions, desktop
headless screens, Android assembly/lint and packaged assets. Review the final diff and
all dated fix comments against this plan.

## Change log

- 2026-10-05 16:41 EDT (UTC−04:00): Plan created; current source inspected and a pre-fix
  snapshot taken for comparison.
- 2026-10-05 16:43:53 EDT (UTC−04:00), FIX-01 and FIX-03: Restricted the flight-turn
  shortcut to existing fast motion, a deadline within one minute and at most 30 seconds
  of error. Applied gap clipping to supplied ramps in C++ and Java. Added CTest/Java
  regressions: the C++ scenarios failed 449 checks before the changes and now pass;
  Java location and review tests also pass. Full integration remains in FIX-07.
- 2026-10-05 16:44:47 EDT (UTC−04:00), FIX-02: Added `soltime_solarDateUnix` and
  Java `solarDateUnix`. Both apps now select local sun/basis by physical location and
  select the adjustment window separately by clock owner. C++ and Java tests pass for
  Tokyo/San Francisco, New Year, and leap day; invalid inputs are rejected.
- 2026-10-05 16:46:30 EDT (UTC−04:00), FIX-04: Replaced permissive/truncating input
  parsing with owned strings and complete numeric/row validation. Empty input is refused
  before statistics or output. Publication writes and checks a temporary file before
  renaming, preserving existing permissions. Tests pass for malformed inputs, unchanged
  destination/in-place failures, valid repeated publication, long names and forced write
  failures. Address/undefined-behavior sanitizer checks are running.
- 2026-10-05 16:49 EDT (UTC−04:00), FIX-04 verification: Publisher tests pass with
  AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer is disabled because
  this sandbox's tracing prevents it from running; no leak-test result is claimed.
- 2026-10-05 16:48:37 EDT (UTC−04:00), FIX-05 implementation: Public header now uses
  standard fixed-width types and explicit structure typedefs, with unchanged ABI
  layouts. Added an isolated-header C11 client linked against `libsoltime.so`.
- 2026-10-05 16:49:54 EDT (UTC−04:00), FIX-06 implementation: Main screen and Find
  dialog apply system-bar, cutout and keyboard insets on API 30+, with platform handling
  on API 26–29. Find fills the available window and uses a weighted list instead of a
  fixed 900-pixel height. Device interaction remains unverified.
- 2026-10-05 16:54:43 EDT (UTC−04:00), FIX-07: Corrected the proposal, specification
  and API comments to scope the sunrise guarantee to the raw field with its own basis.
  Documented supplied-ramp clipping, physical/owner solar dates, the standalone C header,
  publisher validation, new regression checks and future product work.
- 2026-10-05 16:55:58 EDT (UTC−04:00), FIX-02/FIX-07 cleanup: Removed the desktop's
  unused layered-offset calculation and corrected its sunrise explanation so a followed
  or shared clock does not imply a local 06:45 guarantee. Clock policy is unchanged.
- 2026-10-05 17:00 EDT (UTC−04:00), integration verification: All six CTest suites
  passed; C++/Java differential comparisons agree; address/undefined-behavior sanitizer
  regressions passed; desktop headless screens passed; Android assembly/lint passed.
  All four packaged assets match their source files. Rebuilding all 5,854 city rows
  produces byte-for-byte identical registry data (4,168 merged zones).

## Final changes and verification

The five confirmed defects are corrected in the C++/Java libraries, both app displays,
the registry publisher and the C interface. Android layout compatibility and documentation
corrections are included. New regression tests cover the failures rather than just the
implementation branches. This pass did not change city data or the sunrise/zone policy.

| Check | Result |
| --- | --- |
| CMake build and CTest | 6/6 passed: core, calendar/travel, review regressions, C interface, Java, publisher |
| C11 interface | Isolated public header, strict C compilation, real shared-library linkage; assertions enabled in release builds |
| C++/Java differential harness | All categories agree, including the complete registry, real dates and travel |
| Address/undefined-behavior sanitizers | Review regressions and publisher malformed-input/write-failure tests pass; LeakSanitizer unavailable under sandbox tracing |
| World registry rebuild | 5,854 rows accepted; resulting TSV identical to `data/zones.tsv` |
| Desktop | Offscreen self-test renders Clock, World, Simulate and Find and exits successfully |
| Android | Debug APK assembled; lint reports 0 errors and 5 existing warnings |
| APK assets | `zones.tsv`, `places.tsv`, `countries.tsv` and `earth.jpg` exactly match sources |
| Diff and fix annotations | Checked against the pre-fix snapshot; `git diff --check` clean; dated plan references present near corrections |

The five existing lint warnings concern the Gradle version, two portrait-orientation
checks, globe click accessibility and one untranslated string. They remain visible;
no warnings were suppressed. The Gradle-version warning (a newer Gradle is available)
depends on lint's access to Gradle's published version list. The later draft 2 package
checks, run offline, reported the other four; the draft 3 check with a fresh download
reports all five (see `VERIFICATION.txt`). Physical-device behavior and the product additions listed
under FIX-07 remain open and are not claimed as verified or implemented.

To reproduce the automated checks from the project root:

```sh
nice -n 19 cmake -S . -B build-review -DCMAKE_BUILD_TYPE=Release
nice -n 19 cmake --build build-review -j2
nice -n 19 ctest --test-dir build-review --output-on-failure
nice -n 19 bash difftest/run_difftest.sh /tmp/soltime-review-difftest
cd android
nice -n 19 ./gradlew --no-daemon :app:assembleDebug :app:lintDebug
```

The desktop self-test (`TUI_BACKEND=offscreen SOLTIME_SELFTEST=1 build-review/soltime_display`)
needs the full repository's TUI project; the desktop display is not in the evaluation
package. The Android check here used a cached Gradle 9.1.0 with `--offline`. Its logs
were temporary and are not part of the project.

- 2026-10-05 17:02 EDT (UTC−04:00): Final desktop self-test and Java checks passed after
  the comment/display-text cleanup.
