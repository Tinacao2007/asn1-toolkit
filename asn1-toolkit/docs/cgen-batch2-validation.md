# CGEN batch 2 validation

Current result (2026-10-24): **CGEN-04..06 complete within the explicit storage
and DEFAULT profiles below.** REV-27 shared-planning consumption is demonstrated
by actual compiled C calls. Historical attempts remain below as partial-progress
evidence; they do not describe current support.

## Manual completion: implementation and acceptance scope

- Added owned BigInt, BIT STRING, ENUMERATED, OID/RELATIVE-OID, host-double REAL,
  open raw content and fixed object-class-field layouts with real lifecycle calls.
- Added transactional `reset_defaults`, required nested owning DEFAULTs and all
  four literal-allocation failure points. Init/free remain allocation-free.
- Shared field plans generate executable descriptor/selector APIs, arbitrary-size
  decimal selector keys, root/trailing-root presence and version/singleton group
  decisions. Generated C tests consume those APIs and validate group coherence.
- Preserved BIT/OCTET literals in semantic IR; shared literal normalization fixes
  quoted strings and supplies C/C++ DEFAULT rendering. Three C++ output shapes
  verify default omission and changed-bit encoding. asn1tools 0.166.0 independently
  confirms UPER vectors `00` and `8000` for `cgen2_cpp_defaults.asn` (SHA256
  `a85a88236087634baa0936b1f315c39a1694aec949c7658dbe2c81aea989ec5b`).
- Strict C11 link/run, C++17 header inclusion, reserved/collision names, multiple
  translation units and duplicate-definition rejection remain in acceptance.

These are storage profiles, not all ASN.1 values: enum known numbers fit int64,
OID arcs fit uint64, REAL uses host double; structured/list and unresolved
non-enum defaults remain diagnosed. No wire codec, source sharding, cumulative
PDU allocation budget or complete character semantics is claimed. CLI `--lang c`
still rejects. REV-27 planning reuse is evidenced by actual generated C consumers;
complete generated-PDU/oracle acceptance belongs to later codec batches.

Native MSVC Release built the type generator, generated C implementation and
standalone C++17 header consumer. The four `CGenBatch2.Generated*` lifecycle,
owning-profile, planning and SIZE tests passed 4/4, including malformed OID
capacity rejection. The harness explicitly converts discriminant numbers across
named CHOICE and field-specific views; the final targeted rebuild has no enum-conversion warnings.

Latest Release refresh passed **466/466** after the OID capacity-order fix.
The saved batch-1 baseline of **448** names is a subset of all 466 registered
tests; no tests disappeared. The runner/launcher/tests/matrix protected hashes
and immutable TODO acceptance text are unchanged. Final ASan+UBSan passed
**466/466**, and the independent actual-selector syntax matrix passed **84/84**.
Installed C, runtime-only, compiler-only and host-generated consumers all passed;
the pure-C consumer has no C++ runtime dependency. These are storage/lifecycle
and existing C++ wire/syntax results, not C codec interoperability. Logs live under
`build-rev1-quick/cgen2-*` and are intentionally ignored.

### Final reproducible gates

Run from the project root under the configured native GCC toolchain (13.3.0 here),
with offline GoogleTest at `build/_deps/googletest-src`. Configure Release with
`ASN1_ENABLE_SANITIZERS=OFF` and Debug with it `ON`; all compiler/runtime/C-runtime
and test options are ON:

```sh
cmake -S . -B build-rev1-release -DCMAKE_BUILD_TYPE=Release -DASN1_ENABLE_SANITIZERS=OFF -DASN1_BUILD_TESTS=ON -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=ON -DASN1_BUILD_C_RUNTIME=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-release -j1
ctest --test-dir build-rev1-release --output-on-failure --timeout 600
cmake -S . -B build-rev1-san -DCMAKE_BUILD_TYPE=Debug -DASN1_ENABLE_SANITIZERS=ON -DASN1_BUILD_TESTS=ON -DASN1_BUILD_COMPILER=ON -DASN1_BUILD_RUNTIME=ON -DASN1_BUILD_C_RUNTIME=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src"
cmake --build build-rev1-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-rev1-san --output-on-failure --timeout 600
python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx --cxx g++ --output build-rev1-quick/cgen2-matrix
python3 scripts/validate_installed_package.py --build build-rev1-release --output build-rev1-quick/cgen2-package
```

Both configurations completed full builds. Final source changes were then
refreshed through `asn1cxx`, `asn1_cgen_batch2_lifecycle`, `asn1_emit_cache_tests`
and `asn1_cgen2_cpp_{outline,inline,split}_tests`, followed by full-suite reruns.
Final logs: `cgen2-release-final-test.log`, `cgen2-san-final-test.log`,
`cgen2-matrix.log` and `cgen2-msvc-test.log` under `build-rev1-quick`.
The independent C link/naming subprocesses inherit ASan+UBSan compile/link flags
in sanitizer mode; they are not merely uninstrumented compilation checks.
`Architecture.Layering` and `Architecture.PublicHeaders` pass in both full suites.
No runner/launcher/protected matrix script or TODO acceptance body was changed.
No later batch was started; no push was performed.

## Attempt 1 — partial implementation

Date: 2026-10-24. **Not complete: CGEN-04, CGEN-05 and CGEN-06 remain open.**
No later batch was started. The TODO acceptance text and all task checkboxes
are unchanged. No commit, branch/HEAD operation, runner change or baseline-test
removal was performed.

## Implemented scope

`codegen/c_types.hpp` exposes an experimental `emit_c_types` stream API. It
validates the reachable IR before writing either stream. It emits actual C11
structs, CHOICE enums/unions and lifecycle functions for BOOLEAN, NULL, bounded
non-extensible int64 INTEGER, OCTET STRING/character-string storage, SEQUENCE/
SET, CHOICE, SEQUENCE OF/SET OF and resolved references. Unsupported layouts
produce Diagnostics rather than successful codec/type placeholders. Directory
dispatch and the CLI still reject C codec generation; the test generator calls
the type-only API explicitly after the real parser/analyzer pipeline.

Records have explicit OPTIONAL/extension presence flags. CHOICE starts invalid,
and cleanup visits only its active arm. OF containers retain data/count/capacity.
Cyclic dependency edges become owned pointers; noncyclic members retain complete
types. Free restores the typed initialized state, including partially constructed
members with false presence flags. Deep copy uses a temporary, checks count
arithmetic, limits recursive calls to 256 frames, and preserves the initialized
destination on allocation or validation failure. No shallow copy of an owning
source is committed. Self-copy succeeds through the same temporary path.

The emitter calls shared SequencePlan, ChoicePlan and ConstraintPlan. Real
generated copy code checks disjoint INTEGER membership; shared CHOICE planning
retains borrowed effective tags, selectors and root/extension ordinals. This is
partial planning consumption, not evidence of complete wire planning or REV-27
completion. No PER or other codec runtime implementation changed.

Names qualify modules and types with the selected prefix. Non-alphanumeric bytes
escape injectively, including underscore and hyphen; members have a letter
prefix, protecting C keywords/reserved spellings. Lowered named clones retain
distinct `_view<TypeId>` names, preserving field-specific constraints instead of
merging by module/name. Public headers include only the existing C common header
and standard C types, with extern-C declaration guards. Private helpers carry the
same prefix. Type-only output uses one header/source pair; no module split,
source shards, output transaction or cache feature was added.

## Acceptance actually run

Environment: WSL Ubuntu, GCC/G++ 13.3.0, CMake 3.28.3, Ninja, Python 3.12.3.
WSL startup initially failed inside the sandbox; approved WSL execution then
worked. Builds were isolated from the parent runner's build directories.

Commands below are the Linux command bodies invoked through `wsl --exec` from
the Windows workspace. Repository path was `/mnt/d/H/ACT/asn1-toolkit`.

```sh
cmake -S . -B build-cgen2-stage -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/mnt/d/H/ACT/asn1-toolkit/build/_deps/googletest-src \
  -DASN1_BUILD_TESTS=ON
cmake -S . -B build-cgen2-san -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DASN1_ENABLE_SANITIZERS=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/mnt/d/H/ACT/asn1-toolkit/build/_deps/googletest-src \
  -DASN1_BUILD_TESTS=ON
cmake --build build-cgen2-stage --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
cmake --build build-cgen2-san --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
ctest --test-dir build-cgen2-stage \
  -R 'CGenBatch2|EmissionPlanning|EmitCache|CGenBatch1.Api|Architecture' --output-on-failure
ctest --test-dir build-cgen2-san \
  -R 'CGenBatch2|EmissionPlanning|CGenBatch1.Api' --output-on-failure
git diff --check
```

Final results: Release **22/22**, ASan+UBSan **9/9**, diff whitespace check passed.
The five stage-specific tests passed in both builds:

| CTest | Actual check |
|---|---|
| CGenBatch2.UnsupportedLayoutsLeaveStreamsUntouched | Unsupported IR layouts reject without altering either supplied stream |
| CGenBatch2.RejectsReservedPrefixAndUnresolvedReference | Invalid/reserved prefix and unresolved type rejection |
| CGenBatch2.ChoicePlanBorrowsTagsSelectorsAndOrdinals | Actual shared-plan borrowed identities, tags, selector and ordinal values |
| CGenBatch2.GeneratedLifecycle | Real schema → generated C → pure C executable: init/free, deep copy, self-copy, CHOICE replacement, five allocation-failure positions, partial cleanup, cross-module recursion, depth rollback, exact disjoint INTEGER rejection, invalid discriminator |
| CGenBatch2.C11LinkAndDuplicateDefinition | Independently compile generated C under `-std=c11 -pedantic-errors -Wall -Wextra -Werror`; link/run two C header consumers using the C linker; require duplicate implementation linkage to fail |

The lifecycle target also builds a standalone C++17 consumer of the generated
public C header. Allocation hooks are in the test wrapper only. The sanitizer
lifecycle executable is instrumented with both ASan and UBSan; the independent
strict-C11 link subprocess checks language/linkage, and is not sanitizer evidence.

Fixture: `tests/fixtures/cgen_batch2.asn`, two modules with cross-module recursive
records, duplicate type names across modules, an alias, keyword/hyphen field
names, CHOICE, OF, strings and disjoint INTEGER ranges. Final output sizes:
`generated.h` **4,652 bytes**, `generated.c` **16,725 bytes**. There is one source
file; no shard-size, compile-time or peak-memory performance claim is made.

## Concrete remaining work

- **CGEN-04:** implement semantic DEFAULT initialization/materialization and its
  ownership/failure contract, with scalar and owning DEFAULT acceptance cases.
  BigInt INTEGER, ENUMERATED, BIT STRING, OID, REAL and open/table-constrained
  layouts are still rejected by this experimental subset. Extend lifecycle tests
  with the required layouts and cumulative allocation/element budgets as needed.
- **CGEN-05:** consume/render the complete shared effective-tag, presence,
  selector and singleton/version-group decisions. Current output uses only
  ordering/membership and native constraint checks; it does not close REV-27.
- **CGEN-06:** finish the naming/ABI review and compiled acceptance for reserved
  identifiers, transformed-name collisions, case-insensitive output collisions
  and repeated aliases across the full supported type set. Existing tests cover
  keyword/hyphen fields, module qualification, aliases and C11/C++17 headers, but
  do not constitute the entire required naming matrix.

Full Release/sanitizer suites, the 84-entry C++ matrix, MSVC/Clang, installation,
external codec oracles and later batches were not run in this attempt. Parent
independent gates remain separate from the targeted evidence above. There is no
claim of supported C codecs, full generated PDU validation or a completed batch.

## Attempt 2 — scalar defaults and naming acceptance

Date: 2026-10-24. Inspected attempt 1 `result.json`, stderr and its separate
evidence above. The reported blocker was missing implementation, rather than a
failing acceptance test. No applicable AGENTS.md was found in the project or
its parent directories. The remediation skill was read and updated.

Implemented lowered BOOLEAN and bounded INTEGER literal DEFAULT initialization,
including nested by-value records and aliases. Generated init now calls child
init; generated copy initializes its temporary through the same function.
Free restores semantic defaults without allocating. Missing/out-of-arena default
IDs, mismatched kinds and values outside exact normalized INTEGER intervals
reject without changing either stream. DEFAULT values not representable by this
subset still reject, including owning defaults; no unresolved value lookup was
added to the emitter. The real parser/analyzer fixture now includes scalar
defaults, aliases and mandatory/optional nested default records. The compiled
lifecycle test checks nonzero defaults, explicit replacement values, deep copy
and repeated cleanup alongside all previous fault-injection checks.

Added `CGenBatch2.CompiledNamingMatrix`: the actual type emitter consumes a
synthetic IR with 32 record types across four modules plus two repeated aliases.
It includes case-distinct module/type names, hyphen/underscore/escape-lookalike
collisions, leading and double underscores, generated-helper-like names, all
C11 keywords and selected C++ keywords. Synthetic IR deliberately tests names
the ASN.1 lexer would reject. The test compiles the generated implementation and
two independent C consumers with `-std=c11 -pedantic-errors -Wall -Wextra
-Werror`, links with the C linker and executes init/copy/free on every record.
It separately compiles an independent C++17 header consumer with an explicit
extern-C redeclaration. The original duplicate-definition negative test remains.
New model rejection tests cover duplicate qualified type/member names and header
shadowing, including case variants of standard C11 header names. Names of
modules/types never become filenames in the current one-pair stream interface;
per-module output management remains batch 3 work.

**CGEN-06 is now checked. CGEN-04/05 and REV-27 remain open.** The TODO acceptance
text was preserved; no later-batch checkbox was changed. No runner, launcher,
runner test or `scripts/validate_codec_matrix.py` was edited. No baseline test
was removed, and no Git mutation command was executed.

### Commands and results

Same WSL Ubuntu/GCC 13.3.0 environment and isolated build configurations as
attempt 1. Commands were invoked from PowerShell. WSL required sandbox escalation
for startup and was approved. The builds succeeded. An initial shell-wrapped
combined CTest regex was split at `|`; the commands below reran CTest directly
and succeeded, so the malformed invocation is not counted as passing evidence.

```powershell
wsl --exec cmake --build /mnt/d/H/ACT/asn1-toolkit/build-cgen2-stage --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
wsl --exec cmake --build /mnt/d/H/ACT/asn1-toolkit/build-cgen2-san --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
wsl --exec ctest --test-dir /mnt/d/H/ACT/asn1-toolkit/build-cgen2-stage -R 'CGenBatch2|EmissionPlanning|EmitCache|CGenBatch1.Api|Architecture' --output-on-failure
wsl --exec ctest --test-dir /mnt/d/H/ACT/asn1-toolkit/build-cgen2-san -R 'CGenBatch2|EmissionPlanning|CGenBatch1.Api' --output-on-failure
git diff --check
```

Results: Release **25/25**, ASan+UBSan **12/12**, including **8 CGenBatch2**
tests in each configuration. The generated lifecycle/fault-injection executable
is sanitizer-instrumented. The independent strict compilation/linking subprocesses
in the naming and duplicate-definition tests validate language and ABI only,
and are not claimed as sanitizer-instrumented target code. Architecture.Layering
and Architecture.PublicHeaders both passed in the Release selection.

### Remaining assigned work

- **CGEN-04:** owning DEFAULT initialization/materialization with allocation-failure
  acceptance; BigInt INTEGER, ENUMERATED, BIT STRING, OID, REAL and open/selector
  layouts with real lifecycle coverage and their explicit supported profiles.
- **CGEN-05:** complete actual generated-C consumption of effective tags,
  presence order, selectors and singleton/version extension-group decisions.
  Current generated lifecycle code consumes ordering and exact native INTEGER
  intervals, but the full planning acceptance is still absent.

Parent-owned full Release/ASan+UBSan suites and 84-entry C++ matrix were not run
here. Later batches, codecs, sharding, installation and external codec oracles
remain outside this attempt. This is still a blocked batch, not successful
completion of all three assigned tasks.

## Attempt 3 — exact storage SIZE checks; batch still blocked

Date: 2026-10-24. Read the project remediation skill and inspected the preceding
attempt's `result.json`, `agent.stderr.log` and evidence. The previous result
reported missing implementations; stderr additionally contained two historical
documentation patch-context failures. There was no failing stage gate to repair
in those logs. No applicable AGENTS.md was found at the project root, in its
parent directories or under the affected docs/src/include/tests trees.

Found and repaired an additional concrete CGEN-05 consumption gap: generated
OCTET STRING and OF copy functions previously ignored SIZE entirely. Added a
language-independent `storage_size(Type)` view, and used its ConstraintPlan
intervals to generate membership checks before allocation. Disjoint root ranges
retain their holes; empty non-extensible domains reject all lengths. Extension
values remain permitted for extensible constraints. Unknown and unrepresentable
SIZE plans reject with Diagnostics before either stream changes. No runtime API
was invented or changed. Existing C++ emitters do not call the new helper.

Added two real stage tests:

- `CGenBatch2.GeneratedSizeMembership`: parse/analyze the fixture and compile
  actual emitted C, then execute copies for OCTET STRING, SEQUENCE OF and SET OF
  lengths 0 through 9 against `SIZE (0..2 | 7..9)`. Verify holes reject while the
  destination remains unchanged, owners are independent on success, allocation
  failures preserve live allocation counts and destination storage, zero-length
  fixed SIZE succeeds and positive length rejects, and all owners are released.
- `CGenBatch2.InvalidSizePlansLeaveStreamsUntouched`: reject unresolved, negative
  and beyond-int64 SIZE bounds while preserving both caller streams.

The generated-C executable is instrumented by the existing test-only allocator
wrapper; production output contains no test hooks. This addresses storage lengths,
not character counts or codec validation. Acceptance text and all checkboxes
were unchanged in this attempt. CGEN-06's preceding checked state was preserved.

### Commands and environment

Used the same isolated WSL GCC 13.3.0 Release and ASan+UBSan build directories as
attempt 2. WSL startup was denied by the sandbox; an escalated build invocation
was approved and the following commands ran successfully. Builds were sequential
within each directory; the independent configurations could run concurrently.

```powershell
wsl --exec cmake --build /mnt/d/H/ACT/asn1-toolkit/build-cgen2-stage --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
wsl --exec cmake --build /mnt/d/H/ACT/asn1-toolkit/build-cgen2-san --target asn1_cgen_batch2_lifecycle asn1_emit_cache_tests -j 2
wsl --exec ctest --test-dir /mnt/d/H/ACT/asn1-toolkit/build-cgen2-stage -R 'CGenBatch2|EmissionPlanning|EmitCache|CGenBatch1.Api|Architecture' --output-on-failure
wsl --exec ctest --test-dir /mnt/d/H/ACT/asn1-toolkit/build-cgen2-san -R 'CGenBatch2|EmissionPlanning|CGenBatch1.Api' --output-on-failure
git diff --check
```

Final Release: **27/27 passed**; ASan+UBSan: **14/14 passed**, including
**10 CGenBatch2** tests in each. The initial Release selection passed 26/26;
it was rebuilt and rerun to include the new invalid-SIZE model test. Independent
strict C11 compilation/linkage and C++17 public-header checks passed in both
configurations; subprocess compilation is language/ABI evidence, not sanitizer
instrumentation evidence. No full-suite or 84-entry matrix result is claimed.
`Architecture.Layering` and `Architecture.PublicHeaders` passed in Release;
`git diff --check` passed.

### Remaining work required to finish this batch

- **CGEN-04:** implement owning DEFAULT materialization with a fallible ownership
  contract and generated-C allocation-failure tests. Complete BigInt INTEGER,
  ENUMERATED, BIT STRING, OID, REAL and open/selector layouts and lifecycle
  coverage for their explicit supported profiles. These layouts still reject;
  there are no successful stubs.
- **CGEN-05:** actually render and exercise shared effective tags, presence order,
  selector decisions and singleton/version extension-group distinctions in
  generated C. Shared-plan tests and the existing type-only lifecycle output
  do not fulfill that acceptance; REV-27 remains open.

No later batch, codec dispatch, runner/launcher/test or protected matrix script
was edited. No baseline tests were removed, and no commit, push, branch/HEAD,
reset, clean, stash or delegated work was performed. All edits were inside the
project. This attempt does not complete the assigned batch.
