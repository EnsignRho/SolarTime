// Reproduce the evaluation draft's 2026 raw-field examples with a JDK, without Android.
// Conventional comparisons use the running JDK's tzdb; its version is printed.
// This is reference-model output, not an independent astronomical validation.
// Output is UTF-8 with "\n" line ends on every platform and locale, so it can be compared byte
// for byte with reference-output.txt.
import com.lebeli.soltime.SolTime;
import java.io.BufferedOutputStream;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.time.Instant;
import java.time.LocalDate;
import java.time.ZoneId;
import java.time.ZoneOffset;
import java.time.zone.ZoneRulesProvider;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

public final class OverviewExamples {
    private static final double DAY = 86400.0;
    private static final String[][] CITIES = {
        {"Miami", "America/New_York"},
        {"Honolulu", "Pacific/Honolulu"},
        {"Houston", "America/Chicago"},
        {"Phoenix", "America/Phoenix"},
        {"Los Angeles", "America/Los_Angeles"},
        {"Atlanta", "America/New_York"},
        {"Denver", "America/Denver"},
        {"Indianapolis", "America/Indiana/Indianapolis"},
        {"New York City", "America/New_York"},
        {"Chicago", "America/Chicago"},
        {"Minneapolis", "America/Chicago"},
        {"Seattle", "America/Los_Angeles"},
        {"Anchorage", "America/Anchorage"}
    };
    // These cities have no after-midnight sunsets in this comparison. Counting a
    // sunset's clock hour alone would be misleading in, for example, Anchorage.
    private static final Set<String> ANNUAL_CITIES = new HashSet<>(Arrays.asList(
        "Miami", "Houston", "Atlanta", "Indianapolis", "New York City", "Chicago",
        "Minneapolis", "Seattle"));

    private static Map<String, double[]> coordinates(String path) throws Exception {
        Map<String, double[]> result = new HashMap<>();
        for (String row : Files.readAllLines(Paths.get(path))) {
            if (row.isEmpty() || row.startsWith("#")) continue;
            String[] f = row.split("\t", -1);
            if (f.length > 6 && f[2].equals("US") && Long.parseLong(f[6]) > 100000) {
                result.put(f[0], new double[] {
                    Double.parseDouble(f[4]), Double.parseDouble(f[5])
                });
            }
        }
        return result;
    }

    private static double midnight(LocalDate date) {
        return date.atStartOfDay(ZoneOffset.UTC).toEpochSecond();
    }

    private static String clockLabel(double encodedLocal) {
        long minutes = Math.round(encodedLocal / 60.0);
        return String.format(Locale.ROOT, "%02d:%02d",
            Math.floorMod(minutes / 60, 24), Math.floorMod(minutes, 60));
    }

    private static double conventional(double instant, ZoneId zone) {
        return instant + zone.getRules().getOffset(
            Instant.ofEpochSecond((long) Math.floor(instant))).getTotalSeconds();
    }

    private static double hour(double encodedLocal) {
        return (encodedLocal - Math.floor(encodedLocal / DAY) * DAY) / 3600.0;
    }

    private static double[] sun(SolTime model, double[] ll, LocalDate date) {
        double[] result = new double[3];
        if (model.sunTimesUnix(ll[0], ll[1], midnight(date), result) != 0) {
            throw new IllegalStateException("No ordinary rise/set on " + date);
        }
        return result;
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 1) {
            throw new IllegalArgumentException("Usage: OverviewExamples path/to/places.tsv");
        }
        PrintStream out = new PrintStream(new BufferedOutputStream(new FileOutputStream(FileDescriptor.out)),
            false, StandardCharsets.UTF_8.name());
        SolTime model = new SolTime();
        Map<String, double[]> places = coordinates(args[0]);
        out.print("TZDB: " + ZoneRulesProvider.getVersions("America/New_York").keySet() + "\n");
        for (String[] city : CITIES) {
            double[] ll = places.get(city[0]);
            if (ll == null) throw new IllegalStateException("Missing city: " + city[0]);
            ZoneId zone = ZoneId.of(city[1]);
            StringBuilder line = new StringBuilder("| " + city[0] + " |");
            for (String label : new String[] {"2026-12-21", "2026-06-21"}) {
                LocalDate date = LocalDate.parse(label);
                double[] events = sun(model, ll, date);
                for (int event = 0; event < 2; event++) {
                    double t = events[event];
                    double sol = t + model.offsetUnixSecs(ll[0], ll[1], t);
                    long dateDelta = (long) (Math.floor(sol / DAY) - Math.floor(midnight(date) / DAY));
                    String suffix = dateDelta == 0 ? "" : " (" + (dateDelta > 0 ? "+" : "") + dateDelta + " day)";
                    line.append(" ").append(clockLabel(conventional(t, zone))).append(" → ")
                        .append(clockLabel(sol)).append(suffix).append(" |");
                }
            }
            out.print(line + "\n");
            if (!ANNUAL_CITIES.contains(city[0])) continue;
            int[] counts = new int[6];
            for (LocalDate date = LocalDate.of(2026, 1, 1); date.getYear() == 2026; date = date.plusDays(1)) {
                double[] events = sun(model, ll, date);
                double[] rise = {conventional(events[0], zone),
                    events[0] + model.offsetUnixSecs(ll[0], ll[1], events[0])};
                double[] set = {conventional(events[1], zone),
                    events[1] + model.offsetUnixSecs(ll[0], ll[1], events[1])};
                for (int version = 0; version < 2; version++) {
                    if (hour(rise[version]) > 7) counts[version]++;
                    if (hour(set[version]) < 17) counts[2 + version]++;
                    if (hour(set[version]) > 21) counts[4 + version]++;
                }
            }
            out.printf(Locale.ROOT, "ANNUAL | %s | %d → %d | %d → %d | %d → %d |\n",
                city[0], counts[0], counts[1], counts[2], counts[3], counts[4], counts[5]);
        }
        double input = Instant.parse("2026-06-21T12:00:00Z").getEpochSecond();
        double[][] samples = {{0, 0}, {39.76838, -86.15804},
            {69.6492, 18.9553}, {-33.8688, 151.2093}};
        for (double[] ll : samples) {
            out.printf(Locale.ROOT, "VECTOR %.5f %.5f %.0f offset=%.9f basis=%.9f\n",
                ll[0], ll[1], input, model.offsetUnixSecs(ll[0], ll[1], input),
                model.basisLatUnix(ll[0], input));
        }
        out.flush();
    }
}
