"""Execute target dispatch, publication protection, and C++ cache regressions."""
import pathlib
import subprocess
import sys
import tempfile

compiler = sys.argv[1]
with tempfile.TemporaryDirectory(prefix="cgen-batch1-") as tmp:
    root = pathlib.Path(tmp)
    schema = root / "input.asn"
    schema.write_text("Sample DEFINITIONS ::= BEGIN Flag ::= BOOLEAN END\n")
    def run(args, output, success):
        result = subprocess.run([compiler, *args, "--emit-dir", str(output), str(schema)],
                                capture_output=True, text=True)
        assert (result.returncode == 0) == success, (args, result.stdout, result.stderr)
        return result
    cases = [(["--lang", "rust"], "unknown language"),
             (["--lang", "c", "--namespace", "asn1_gen"], "conflicts"),
             (["--c-prefix", "abc"], "requires --lang c"),
             (["--lang", "c", "--c-prefix", "bad-name"], "identifier"),
             (["--lang", "c", "--c-prefix", ""], "identifier"),
             (["--lang", "c", "--emit-inline", "--emit-split"], "conflict")]
    for args, message in cases:
        output = root / "rejected"
        result = run(args, output, False)
        assert message in result.stderr, result.stderr
        assert not output.exists()
    for option in ("--lang", "--c-prefix"):
        result = subprocess.run([compiler, option], capture_output=True, text=True)
        assert result.returncode != 0 and "requires a value" in result.stderr
    codecs = "uper aper ber der cer oer coer jer jeri xer cxer exer".split()
    for codec in codecs:
        for shape in ([], ["--emit-inline"], ["--emit-split"]):
            output = root / "unsupported"
            args = ["--lang", "c", "--c-prefix", "sample", "--codec", codec, *shape]
            result = run(args, output, False)
            assert "Unsupported" in result.stderr
            assert not output.exists()
    # Reject before touching a previously published C++ directory or its cache.
    output = root / "cpp"
    run([], output, True)
    def snapshot(directory):
        return {p.relative_to(directory): p.read_bytes() for p in directory.rglob("*") if p.is_file()}
    baseline = snapshot(output)
    run(["--lang", "c"], output, False)
    assert snapshot(output) == baseline
    run(["--lang", "cpp"], output, True)
    assert snapshot(output) == baseline
    for flags in ([], ["--emit-inline"], ["--emit-split"]):
        a, b = root / "default", root / "explicit"
        run(["--no-emit-cache", "--codec", "uper,ber", *flags], a, True)
        run(["--lang", "cpp", "--no-emit-cache", "--codec", "uper,ber", *flags], b, True)
        assert snapshot(a) == snapshot(b)
print("C dispatch rejected all 12 entries in three shapes; C++ artifacts/cache preserved")

# Compare the actual encoded results of all twelve entries across three shapes.
outputs = [subprocess.run([driver], check=True, capture_output=True).stdout
           for driver in sys.argv[2:]]
assert len(outputs) == 3 and outputs[0] == outputs[1] == outputs[2]
assert len(outputs[0].splitlines()) == 12
