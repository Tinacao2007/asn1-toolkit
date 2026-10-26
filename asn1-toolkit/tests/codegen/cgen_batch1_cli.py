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
    def snapshot(directory):
        return {p.relative_to(directory): p.read_bytes() for p in directory.rglob("*") if p.is_file()}
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
    for codec in ("uper", "aper", "per", "uper,aper", "ber", "der", "cer", "uper,ber", "ber,der,cer"):
        for shape in ([], ["--emit-inline"], ["--emit-split"]):
            output = root / (codec.replace(',', '_') + ('_inline' if shape and shape[0]=='--emit-inline' else '_split' if shape else '_out'))
            run(["--lang", "c", "--c-prefix", "sample", "--codec", codec, *shape], output, True)
            manifest = (output / "generated-sources.json").read_text()
            assert '"types_only":false' in manifest and '"codec_mask":' in manifest
            assert "encode_uper" in (output / "generated.h").read_text() if codec in ("uper", "per", "uper,aper") else True
            assert "encode_aper" in (output / "generated.h").read_text() if codec in ("aper", "per", "uper,aper") else True
    for codec in "oer coer jer jeri xer cxer exer".split():
        for shape in ([], ["--emit-inline"], ["--emit-split"]):
            output = root / "family6" / codec / ("inline" if shape and shape[0]=='--emit-inline' else "split" if shape else "out")
            run(["--lang", "c", "--c-prefix", "sample", "--codec", codec, *shape], output, True)
            assert "encode_" + codec in (output / "generated.h").read_text()
    # Language switching publishes one owned artifact set and retires the old one.
    output = root / "cpp"
    run([], output, True)
    assert (output / "generated.hpp").exists() and (output / "generated.cpp").exists()
    run(["--lang", "c"], output, True)
    assert (output / "generated.h").exists() and (output / "generated.c").exists()
    assert not (output / "generated.hpp").exists() and not (output / "generated.cpp").exists()
    run(["--lang", "cpp"], output, True)
    assert (output / "generated.hpp").exists() and (output / "generated.cpp").exists()
    assert not (output / "generated.h").exists() and not (output / "generated.c").exists()
    for flags in ([], ["--emit-inline"], ["--emit-split"]):
        a, b = root / "default", root / "explicit"
        run(["--no-emit-cache", "--codec", "uper,ber", *flags], a, True)
        run(["--lang", "cpp", "--no-emit-cache", "--codec", "uper,ber", *flags], b, True)
        assert snapshot(a) == snapshot(b)
print("C emits supported PER/TLV subsets and supports all twelve entries in the declared value profile; C++ artifacts/cache preserved")

# Compare the actual encoded results of all twelve entries across three shapes.
outputs = [subprocess.run([driver], check=True, capture_output=True).stdout
           for driver in sys.argv[2:]]
assert len(outputs) == 3 and outputs[0] == outputs[1] == outputs[2]
assert len(outputs[0].splitlines()) == 12
