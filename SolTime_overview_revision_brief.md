# SolTime overview: revision brief

**For:** Claude Code, working in the SolTime project folder
**From:** a review of `SolTime_overview.md` (evaluation draft 1, 2026-10-05) done in a separate Claude chat with Rick
**Result goes back to:** that chat, for a second review

## Background

Rick is sending the SolTime evaluation package to the U.S. government. The first reader will be non-technical and will have a few minutes. If that reader passes it on, a technical team will evaluate it and may build from it.

The overview serves the second reader well: it is complete, reproducible, and candid about its limits. It does not yet serve the first reader. The idea is not stated plainly until §1, and the opening is status and caveats.

The review saw only the overview document. It did not see the code or the data. Where this brief describes how the code behaves, confirm it against the source before acting.

## How to work

1. Read `SolTime_overview.md`, `android/README_android.md`, and the evaluation folder before changing anything.
2. Rick wants the approach discussed and agreed before any implementation. Present your plan and the questions in "Decisions that are Rick's" to him first. Do not edit until he has answered.
3. Do not invent answers to those decisions. If one is still open when you write, leave a clearly marked `[RICK: ...]` placeholder and list it in your report.
4. Do not change any clock algorithm unless Rick approves it under item 7a.
5. Leave the technical body's hedging and caveats as they are. They are correct for evaluators.
6. Every number you add must come from the code or its output, not from estimation. Say which command produced it.

## Decisions that are Rick's

Ask these before editing:

- **License.** Which license applies to the software, and which to the document? §12.3 lists this as open. A team cannot build on code that grants no rights, so this blocks the stated goal.
- **The ask.** What does Rick want a government reader to do: commission a study, run a pilot, assign a technical review, something else?
- **Morning light.** In his own words, why does morning light outweigh the earlier winter sunsets? The overview lists the trade-off as a consequence to evaluate but gives no argument for it.
- **The extremes.** Anchorage's modeled December sunset is 12:12 and its June sunset is 02:08 the next day. Is that acceptable as designed, or does he want a different rule or a stated position?
- **Zip file name.** The link Rick is publishing uses `SolTime_evaluation_2026_10_05.zip`. §11.1 names it `SolTime_evaluation_2026-10-05.zip`. Which one stands?
- **Full project.** Should the package include the full C++ project, so the §10.2 test results can be reproduced instead of taken as reported?
- **Items 7a and 7b below**, each of which has options.

## Changes

### 1. Add a plain-language front page

Place it before everything else, ahead of the status block. One page at most. No formulas, no defined terms, no section references. A reader with no background should finish it in two minutes and know what is proposed.

It should cover, in this order:

- **The idea**, in two or three sentences. Clocks set themselves from the phone's location so that sunrise lands at about 6:45 every day of the year, and the clock never needs a spring or fall change.
- **What one person would see.** Use Indianapolis for a full year, with figures from `android/evaluation/reference-output.txt`. Include December and June sunrise and sunset, conventional and SolTime.
- **How much the clock moves each day.** Compute the largest and the typical day-to-day offset change for Indianapolis across 2026 from the Java implementation, and state them. State the 240 s/day cap as the limit everywhere. Say the change happens overnight.
- **Shared city time.** Everyone in and around a city reads the same clock.
- **The trade-off**, stated plainly: winter sunsets come earlier, summer sunsets come later. Give the Indianapolis numbers.
- **Clocks that are not phones.** One or two sentences from §12.1.
- **What exists today.** A working reference implementation and an Android demonstration. Not adopted, not field-tested.
- **What is asked.** From Rick's answer above.

The front page must agree with the body. In particular it must not say any of the following, each of which the body contradicts:

- that the clock changes by "a few seconds a day" (the cap is 240 s, and mid-latitude spring and fall changes are larger than a few seconds)
- that the design maximizes daylight around midday (the rule pins sunrise and puts the remainder in the evening, §1.1)
- that manual clock changes end for every clock (§12.1)
- that zones can stop at a national border (the prototype's zones are discs; border handling is an open policy decision, §7.1)
- that a moving clock runs backward in general (only westbound at high latitude, §8.2)

### 2. Answer the obvious objections early

Add a short section directly after the front page, or fold it into §1, that takes these on by name:

- **Earlier winter sunsets.** State the numbers, then Rick's reasoning from his answer above.
- **Non-phone clocks.** Move the substance of §12.1's third paragraph up. Leave a pointer in §12.1.
- **The extremes.** Anchorage and the far north, with Rick's position.

### 3. State the license

Add the license Rick chooses to the overview, to the package (a `LICENSE` file), and to §12.3 and §13. Update the opening status paragraph, which currently says no license is assigned. Keep third-party data terms separate, as §13 already does.

### 4. Make the package runnable without a toolchain

- Build the debug APK and include it in the zip.
- Update §11.1's file list and its statement that the archive contains source only.
- Add brief install instructions for a reader with an Android phone and no development tools, including that a debug-signed APK needs installation from unknown sources allowed.
- If Rick decides to include the full C++ project, add it with its own verification record and hashes, as §11.4 already describes, and update §10 and §11 to match.

### 5. State the ask for a government reader

"Requested evaluation" in the opening speaks to technical reviewers. Keep it, and add Rick's ask for a policy reader next to it and in §12.

### 6. Make the file name consistent

Apply Rick's choice to §11.1 and anywhere else the archive is named.

### 7. Internal fixes

**7a. §6.1, the nine-year window.** The lower envelope `O(k) = min over j of [T(j) + D × |k − j|]` can only be affected by dates within `(max T − min T) / D` days. At defaults the sunrise range on a polar basis is roughly 8 hours, so the reach is about 120 days. The document says the implementation evaluates nine full years on each side and gives leap-year patterns as the reason. That reason does not explain the size.

Separately, validation requires only `D > 0`. If the window is fixed at nine years, a `D` below roughly 9 s/day would have a reach longer than the window, and the result would be truncated without warning.

First confirm both points against the source. Then put the options to Rick:

- validate a minimum `D`
- size the window from `D`
- keep the code and document the limit

Whichever he picks, correct §6.1's explanation. If code changes, change the C++ and Java implementations together, add tests, and rerun the differential check.

**7b. §7.1, the city radius rule.** For U.S. cities the rule takes the larger of Census land area and GHSL urban-core area. That inflates consolidated city-counties. Anchorage's 35-mile reach comes from its municipal boundary, most of which is wilderness, and that feeds its 8.3-minute `J` and 106-mile ramp.

From the data, list the U.S. cities where Census land area exceeds GHSL area by a large factor, so Rick can see how far this goes. Then put the options to him:

- keep the rule and state the effect in §7.1
- change the rule

Changing the rule changes merging, leaders, `J`, and ramps. It means rebuilding the registry and updating the §7.1 table, the row and group counts, and any §2 or §10 figures that depend on it.

**7c. §1.1 wording.** "Winter sunsets become earlier in many northern cities" understates the §2 table. All 13 listed cities, including Miami and Honolulu, have earlier December sunsets under SolTime. Make the sentence match the table.

### 8. Housekeeping

- Change the document identifier to evaluation draft 2 and add a line to the draft history describing this revision.
- Regenerate `SHA256SUMS` and rebuild the zip.
- Check that the table of contents and internal links still resolve.

## Verification before reporting

- Rerun `OverviewExamples` and confirm `reference-output.txt` still matches, or explain every difference.
- Run `:app:assembleDebug` and `:app:lintDebug` on the portable copy.
- If any code changed, run the full CTest suite and the C++/Java differential test.
- Trace every number on the front page to its source.

## What to hand back

Give Rick these to take back to the chat:

1. The revised `SolTime_overview.md`, complete.
2. A list of changes by section.
3. Each decision Rick made, in his words.
4. Verification results: what was run, what passed, what was not run and why.
5. Anything in this brief you did not do or disagreed with, and the reason.
6. Whether the reading of the code in 7a was correct.
