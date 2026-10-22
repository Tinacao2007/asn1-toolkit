"""Check source include boundaries; generated runtime include strings are not directives."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

FRONT = {'ast', 'frontend', 'ir', 'constraints', 'semantic', 'codegen'}
RUNTIME = {'runtime', 'c_runtime', 'ber', 'der', 'cer', 'per', 'oer', 'coer', 'xer', 'cxer', 'exer', 'jer', 'jeri'}
# Preserve newlines while removing comments and C++ string tokens, including raw strings.
TOKENS = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|R"([^ ()\\\t\r\n]*)\([\s\S]*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]asn1/([^/]+)/', re.M)

def violations(text, layer):
    # Keep quoted include operands; erase other literals to avoid emitter false positives.
    lines = text.splitlines(keepends=True)
    directives = []
    for i, line in enumerate(lines):
        if re.match(r'^\s*#\s*include\s*"asn1/', line):
            directives.append((i, line.replace('"', '<', 1).replace('"', '>', 1)))
    for i, line in directives:
        lines[i] = line
    clean = TOKENS.sub(lambda m: '\n' * m.group().count('\n') + ' ', ''.join(lines))
    forbidden = RUNTIME if layer in FRONT else FRONT if layer in RUNTIME else FRONT | RUNTIME
    return [m.group(1) for m in INCLUDE.finditer(clean) if m.group(1) in forbidden]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cmake")
    args = parser.parse_args()
    assert violations('#include <asn1/runtime/bigint.hpp>\n', 'codegen')
    assert violations('#include "asn1/ir/type.hpp"\n', 'c_runtime')
    assert not violations('out << "#include <asn1/runtime/bigint.hpp>\\n";\n', 'codegen')
    assert not violations('/*\n#include <asn1/ir/type.hpp>\n*/', 'runtime')
    errors = []
    count = 0
    for base, fixed_layer in ((args.root/'src', None), (args.root/'include/asn1', None),
                              (args.root/'tools/asn1cxx', 'codegen')):
        for path in sorted(base.rglob('*')):
            if path.suffix not in {'.h', '.hpp', '.c', '.cpp'}: continue
            layer = fixed_layer or path.relative_to(base).parts[0]
            count += 1
            for dependency in violations(path.read_text(encoding='utf-8-sig'), layer):
                errors.append(f'{path}: forbidden {layer} -> {dependency} include')
    if errors: raise SystemExit('\n'.join(errors))
    if args.cmake:
        with tempfile.TemporaryDirectory(prefix="asn1-layer-graph-") as temporary:
            folder = Path(temporary)
            cases = [([], True), (["target_link_libraries(asn1_codegen INTERFACE asn1_ber)"], False),
                     (["target_link_libraries(asn1_runtime INTERFACE helper)",
                       "target_link_libraries(helper INTERFACE asn1_ir)"], False)]
            for i, (edges, success) in enumerate(cases):
                source = folder / str(i)
                source.mkdir()
                text = ["cmake_minimum_required(VERSION 3.16)", "project(LayerProbe NONE)"]
                text += [f"add_library({target} INTERFACE)" for target in
                         ("asn1_codegen", "asn1_ber", "asn1_runtime", "asn1_common", "asn1_ir", "helper")]
                text += edges + [f'include("{(args.root / "cmake/Layering.cmake").as_posix()}")']
                (source / "CMakeLists.txt").write_text("\n".join(text), encoding="utf-8")
                result = subprocess.run([args.cmake, "-S", str(source), "-B", str(source/"build")],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                if (result.returncode == 0) != success:
                    raise SystemExit("Layer graph probe failed: " + result.stdout)
                if not success and "layer violation" not in result.stdout:
                    raise SystemExit("Negative probe failed for unrelated reason: " + result.stdout)
        print("Target graph: valid graph and two forbidden direct/transitive probes passed")
    print(f'Layer boundaries: {count} files passed; positive/negative probes passed')

if __name__ == '__main__': main()
