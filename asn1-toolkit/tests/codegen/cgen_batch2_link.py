"""Compile actual generated C independently and require duplicate-TU rejection."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def run(args, success=True):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True)
    if (result.returncode == 0) != success:
        raise RuntimeError(f"unexpected exit {result.returncode}: {args}\n{result.stdout}{result.stderr}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cc", required=True)
    p.add_argument("--include", required=True)
    p.add_argument("--generated", required=True)
    p.add_argument("--work", required=True)
    p.add_argument("--runtime", required=True)
    p.add_argument("--sanitizers", action="store_true")
    a = p.parse_args()
    generated = Path(a.generated)
    Path(a.work).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=a.work, prefix="link-") as tmp:
        work = Path(tmp)
        (work / "main.c").write_text(
            '#include "generated.h"\n'
            'int second(void);\n'
            'int main(void) { stage2_mA_tFlag v; stage2_mA_tFlag_init(&v); '
            'return v.value || second(); }\n', encoding="utf-8")
        (work / "second.c").write_text(
            '#include "generated.h"\n'
            'int second(void) { stage2_mA_tFlag v = {0}; '
            'stage2_mA_tFlag_free(&v); return v.value; }\n', encoding="utf-8")
        sanitizers = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                      "-fno-sanitize-recover=undefined"] if a.sanitizers else []
        flags = [a.cc, *sanitizers, "-std=c11", "-pedantic-errors", "-Wall", "-Wextra", "-Werror",
                 "-I", a.include, "-I", generated]
        run([*flags, "-c", generated / "generated.c", "-o", work / "generated.o"])
        run([*flags, "-c", work / "main.c", "-o", work / "main.o"])
        run([*flags, "-c", work / "second.c", "-o", work / "second.o"])
        objects = [work / "generated.o", work / "main.o", work / "second.o"]
        run([a.cc, *sanitizers, *objects, a.runtime, "-o", work / "good"])
        run([work / "good"])
        # A proved working linker must reject two implementations, while two
        # independent consumers of the public header linked and ran above.
        run([a.cc, *sanitizers, *objects, work / "generated.o", a.runtime, "-o", work / "bad"], success=False)


if __name__ == "__main__":
    main()
