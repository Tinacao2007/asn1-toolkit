# CGEN-01..03 batch 1 evidence

Date: 2026-10-24. Scope: batch 1 only. No C type/codec emitter, source sharding,
new runtime API, generated C PDU, or REV-27 real C consumption is claimed.
No applicable AGENTS.md was found in the project or its D:/, D:/H/ and
D:/H/ACT ancestors. Project remediation skill and referenced C/runtime/planning
contracts were read. Baseline working tree had no changes.

## Changes

- CGEN-01: [generated ABI contract](c-generated-abi.md) fixes naming, representation,
  lifecycle, signature order, ownership, output/cursor failure behavior, budgets
  and ABI policy, with unsupported value domains explicit.
- CGEN-02: [runtime gap audit](c-runtime-codegen-gaps.md) inspects implementation
  and C++ call sites and records primitives, containers, policies, full decoding,
  locations and budgets separately for each of twelve entries. No assumed APIs.
- CGEN-03: CLI parses cpp/c and C prefix, rejects unknown/missing/invalid values,
  C namespace conflict and C inline+split conflict. Unified directory dispatch and
  legacy CppGenerator paths diagnose unsupported C before directory/cache I/O.
  Language and prefix join codec/namespace/inline/split/dependency/generator cache
  identity. New fields are appended to preserve old aggregate initialization.

## Executed stage checks

Environment: Windows 10, Visual Studio 2022/MSVC 19.44 Release, Windows SDK
10.0.26100.0, CMake 3.31.6-msvc6, Python 3.12.10, existing offline GoogleTest source.
Build directory: `build-rev4-msvc` (existing native test build).

```powershell
cmake --build build-rev4-msvc --config Release --target asn1cxx asn1_emit_cache_tests asn1_entry_combined asn1_entry_combined_inline asn1_entry_combined_split -j 2
ctest --test-dir build-rev4-msvc -C Release -R '^CGenBatch1\.' --output-on-failure
ctest --test-dir build-rev4-msvc -C Release -R '^(EmitCache|EmitOutput\.|EmissionPlanning\.|Architecture.Layering)' --output-on-failure
git diff --check
```

Final rebuild succeeded. The combined final CTest selection passed **24/24**:
six CGenBatch1 tests, fifteen cache/publication tests, two planning tests and
Architecture.Layering. `git diff --check` passed. A baseline comparison verified only CGEN-01..03
checkboxes changed from the first task onward; evidence/skill relative links resolve. CGenBatch1 tests use
real API/CLI invocations and linked codecs, not document wording checks:

- ApiRejectsCWithoutWritingStreams checks diagnostics and empty header/source
  after requesting C through the existing C++ stream API.
- ApiRejectsCppWithCPrefix checks API option conflict without output.
- CliDispatch runs all twelve C selections in outline/inline/split, requires
  failure and no output directory, checks invalid options/missing arguments,
  preserves an existing C++ output/cache snapshot, compares explicit/default cpp
  bytes in three shapes, and executes/compares all twelve C++ codec results across
  the three linked shape drivers.
- Cpp.combined, Cpp.combined_inline and Cpp.combined_split run existing generated
  encode/decode drivers with twelve real runtime entries each.

## Verification boundaries

WSL invocation was attempted and returned Wsl/Service/CreateInstance/E_ACCESSDENIED.
No Linux Release, ASan+UBSan, full baseline suite or 84-entry matrix is claimed by
this stage run. The parent independently runs these gates. MSVC Release is not
ASan+UBSan. No new C wire algorithm was implemented, so new independent oracle or
C PDU tests are not applicable to this dispatch/design batch; existing primitive
oracle evidence is referenced without claiming it was rerun.
No runner/launcher/runner tests or validate_codec_matrix.py were changed. No
commit, push, branch/HEAD operation, reset/clean/stash or credential access occurred.
Later CGEN checkboxes and TODO acceptance text are unchanged.
