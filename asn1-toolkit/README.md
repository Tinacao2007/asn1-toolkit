# asn1-toolkit

C++17 ASN.1 compiler and codec runtime.

## Status

**Phase 39** -- Validation.

- Person multi-codec round-trips: UPER, APER, OER, COER, BER, DER, JER
- Telecom: full modules parse+analyze+emit-gap checks; `rrc_slice` UPER/OER emit + UPER round-trips
- CLI: `--namespace` for multi-codec generated headers

## Requirements

- CMake 3.16+
- C++17 compiler (GCC or Clang; MSVC also accepted for local Windows work)
- Network once at configure time (GoogleTest via FetchContent)

Primary target: Linux / Ubuntu 24.04 with GCC or Clang.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

```bash
./build/asn1cxx --dump-ast examples/person.asn
./build/asn1cxx --dump-ir examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec uper examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec jer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec exer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec ber examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec oer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec coer examples/person.asn
./build/asn1cxx --emit-dir build/gen --codec cxer examples/person.asn
./build/asn1cxx --dump-ir tests/fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn
```

## Layout

```text
include/asn1/common/    Result, Span
include/asn1/support/   SourceLocation, Diagnostics, SourceFile
include/asn1/ast/       AST nodes + printer
include/asn1/frontend/  Token, Lexer, Parser
include/asn1/ir/        Type IR
include/asn1/semantic/  SymbolTable, Analyzer
src/                    implementations
tools/asn1cxx/          compiler driver
tests/                  unit + telecom fixture tests
examples/               sample .asn files
docs/ARCHITECTURE.md    design
```

Reference trees elsewhere in the parent workspace (`pasn1/`, asn1c, `Python_asn1tools/`) are not part of this build.

## License

TBD (open-source; license file will be added when the project is published).
