"""Compile and execute the C naming matrix, including a C++17 header consumer."""
import argparse
from pathlib import Path
import tempfile
from cgen_batch2_link import run


def spell(text):
    return "".join(c if c.isascii() and c.isalnum() else f"_x{ord(c):02x}" for c in text)


def main():
    p = argparse.ArgumentParser()
    for key in ("cc", "cxx", "generator", "include", "work"):
        p.add_argument("--" + key, required=True)
    p.add_argument("--sanitizers", action="store_true")
    a = p.parse_args()
    Path(a.work).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=a.work, prefix="names-") as tmp:
        work = Path(tmp)
        run([a.generator, "--names", work / "generated.h", work / "generated.c"])
        body = ['#include "generated.h"', 'int second(void);', 'int main(void) {']
        for module in ("M", "m", "M-X", "M_x2dX"):
            for name in ("T", "t", "T-X", "T_x2dX", "_T", "__T", "T_init", "anon_0"):
                n = "stage2_m" + spell(module) + "_t" + spell(name)
                body.append(f'{{ {n} v, out; {n}_init(&v); {n}_init(&out); '
                            'v.has_fauto = 1; v.fauto.value = 1; '
                            'v.has_fa_x2db = 1; v.fa_x2db.value = 1; '
                            'v.has_fa_x5fb = 1; v.fa_x5fb.value = 0; '
                            f'if ({n}_copy(&v, &out) != ASN1_OK || !out.fauto.value || '
                            '!out.fa_x2db.value || out.fa_x5fb.value) return 1; '
                            f'{n}_free(&v); {n}_free(&out); }}')
        body.append('return second(); }')
        (work / "main.c").write_text("\n".join(body), encoding="utf-8")
        (work / "second.c").write_text(
            '#include "generated.h"\nint second(void) { '
            'stage2_mM_tAlias a; stage2_mM_tAlias_x2dcopy b; '
            'stage2_mM_tAlias_init(&a); stage2_mM_tAlias_x2dcopy_init(&b); '
            'return a.value.value || b.value.value; }', encoding="utf-8")
        (work / "header.cpp").write_text(
            '#include "generated.h"\n'
            'extern "C" void stage2_mM_tAlias_init(stage2_mM_tAlias*);\n'
            'static_assert(sizeof(stage2_mM_x2dX_tT) > 0, "complete");\n', encoding="utf-8")
        sanitizers = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                      "-fno-sanitize-recover=undefined"] if a.sanitizers else []
        flags = [*sanitizers, "-pedantic-errors", "-Wall", "-Wextra", "-Werror", "-I", a.include, "-I", work]
        objects = []
        for source in ("generated", "main", "second"):
            obj = work / (source + ".o")
            run([a.cc, "-std=c11", *flags, "-c", work / (source + ".c"), "-o", obj])
            objects.append(obj)
        run([a.cxx, "-std=c++17", *flags, "-c", work / "header.cpp", "-o", work / "header.o"])
        run([a.cc, *sanitizers, *objects, "-o", work / "names"])
        run([work / "names"])


if __name__ == "__main__":
    main()
