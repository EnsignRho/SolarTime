"""Publisher recalculation and BUG_FIX_PLAN_2026-10-05.md FIX-04 input/output regressions."""
# FIX-04 added 2026-10-05 16:46:30 EDT (UTC-04:00).
import pathlib
import subprocess
import sys
import tempfile
import os
import resource
import signal

with tempfile.TemporaryDirectory(prefix="soltime-publisher-") as directory:
    root = pathlib.Path(directory)

    def publish(name, row):
        source, output = root / (name + ".in"), root / (name + ".out")
        source.write_text(row + "\n")
        subprocess.run([sys.argv[1], str(source), str(output)], check=True, stdout=subprocess.DEVNULL)
        return next(line for line in output.read_text().splitlines() if not line.startswith("#")).split("\t")

    original = publish("initial", "Test\tUS\tIN\t40\t-86\t1000000\t7\ttest\t10\t-\t-")
    edited = original.copy()
    edited[8] = "100"
    fresh = edited.copy()
    fresh[9:11] = ["-", "-"]
    reused = publish("edited", "\t".join(edited))
    clean = publish("fresh", "\t".join(fresh))
    assert reused == clean, "old derived columns influenced the new registry"
    assert reused[9:11] != original[9:11], "radius edit did not change the derived jump/ramp"

    valid = "Test\tUS\tIN\t40\t-86\t1000000\t7\ttest\t10\t-\t-"
    malformed = ["", "# only a comment\n", "Test\tUS\tIN\t40", valid + "\textra", valid + "\x00"]
    for column, values in {
        3: ["bad-latitude", "40junk", "", "NaN", "Infinity", "91", "1e999"],
        4: ["-181"], 5: ["-1", "1.5"], 6: ["0"], 8: ["-1"], 9: ["garbage"], 10: ["NaN"],
    }.items():
        for value in values:
            fields = valid.split("\t"); fields[column] = value
            malformed.append("\t".join(fields))

    for i, content in enumerate(malformed):
        source, output = root / f"invalid-{i}.in", root / f"invalid-{i}.out"
        source.write_text(content)
        output.write_bytes(b"existing registry\n")
        result = subprocess.run([sys.argv[1], str(source), str(output)], capture_output=True)
        assert result.returncode == 1 and result.stderr, (i, result)
        assert output.read_bytes() == b"existing registry\n", "invalid input replaced existing output"
        assert not list(root.glob(output.name + ".tmp.*")), "temporary output leaked"
        before = source.read_bytes()
        result = subprocess.run([sys.argv[1], str(source), str(source)], capture_output=True)
        assert result.returncode == 1 and source.read_bytes() == before, "invalid in-place input was changed"

    # A valid prefix followed by a bad row must not publish a partial registry.
    source.write_text(valid + "\nshort\trow\n")
    result = subprocess.run([sys.argv[1], str(source), str(output)], capture_output=True)
    assert result.returncode == 1 and b":2:" in result.stderr
    assert output.read_bytes() == b"existing registry\n"

    # No fixed line buffer: long names survive both initial and in-place publication.
    source.write_text(valid.replace("Test", "T" * 5000) + "\n")
    subprocess.run([sys.argv[1], str(source), str(source)], check=True, stdout=subprocess.DEVNULL)
    first = source.read_bytes()
    assert first.startswith(b"T" * 5000 + b"\t")
    subprocess.run([sys.argv[1], str(source), str(source)], check=True, stdout=subprocess.DEVNULL)
    assert source.read_bytes() == first, "published output must be a fixed point"

    # Exercise write failure before rename without filling the disk.
    def limit_output():
        resource.setrlimit(resource.RLIMIT_FSIZE, (64, 64))
        signal.signal(signal.SIGXFSZ, signal.SIG_IGN)

    result = subprocess.run([sys.argv[1], str(source), str(output)], capture_output=True, preexec_fn=limit_output)
    assert result.returncode == 1, "write error should be reported cleanly"
    assert output.read_bytes() == b"existing registry\n", "failed write replaced output"
    assert not list(root.glob(output.name + ".tmp.*")), "failed write left a temporary file"

    output.chmod(0o640)
    source.write_text(valid + "\n")
    subprocess.run([sys.argv[1], str(source), str(output)], check=True, stdout=subprocess.DEVNULL)
    assert os.stat(output).st_mode & 0o777 == 0o640, "replacement changed output permissions"
    print("Publisher recalculation, malformed input, atomic output and write failure: passed")
