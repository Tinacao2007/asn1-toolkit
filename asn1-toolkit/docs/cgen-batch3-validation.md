# CGEN 第 3 批验收记录

Date: 2026-10-24. **CGEN-07..09 complete within explicit type/lifecycle-only scope.**
No later codec batch was implemented. Historical batch-2 evidence is unchanged.
TODO acceptance wording and protected runner/matrix files are unchanged.

Final GCC Release **473/473**, ASan+UBSan **473/473**, actual C++ selector syntax
matrix **84/84**. Installed C, runtime-only, compiler-only and host-generated
consumers all pass; the installed C consumer has no C++ runtime dependency.
All **466** prior baseline test names remain; seven new stage cases are added.

## 实际实现

- Explicit `--lang c --types-only`, with no codec request; pure C artifacts through
  unified dispatch. All ordinary C codec requests continue to reject.
- Stable SCC-based type source shards; indivisible SCC warning; module ownership,
  complete shared public header and external prefixed private depth declarations.
- True `static inline` single header; ordinary/module/within-module outputs;
  independent C11 and C++17 header inclusion, two-TU calls and allocation failure.
- Shared REV-25 ownership/staging/publication for C/C++ transitions, source/sidecar
  registration, source JSON/CMake lists and conservative per-file verified cache.
- Metrics and actual function-definition uniqueness checks accompany compile/link
  runs. Documentation: [output contract](c-codegen-output.md).

## 检查与当前证据

Release passed **473/473** before and after the sanitizer dependency-check fix.
Final full-suite sanitizer rerun passed **473/473** with the corrected test.
The first sanitizer run passed all 466 baseline tests and five new cases, but two
new checks incorrectly treated libstdc++ pulled by GCC UBSan as a product leak.
Direct `ldd` confirms libubsan's transitive library. The correction keeps strict
C11 compile/link and sanitizer execution, and applies the product dependency
check to non-instrumented executables. No production code was changed for it.
The initial failed sanitizer log is retained, not reported as a passing run.

Seven `CGenBatch3.*` cases cover shape combinations, a single 40-type module
(8 -> 13 budget gives 5 -> 4 actual C files), RRC full type generation, ownership/
failed-publication protection, corrupt/disabled/hit/miss cache, option/case/SCC
negatives, and a C-only CMake project consuming the generated list.

Five shape runs compare generated DEFAULT values, recursive independent copy,
self-copy, OOM retention and multi-TU calls. Existing C PER primitives applied to
generated scalar DEFAULT values produce `87` in every shape. This is explicitly
a primitive integration projection, **not a generated PDU codec**. Complete C
PDU wire/oracle comparisons cannot be claimed before the later codec batches.
Existing C++ roundtrips/combination equality and the actual 84 selector syntax
matrix retain their separate validation roles.

Native MSVC Release: **8/8** (four batch-2 lifecycle cases and four new batch-3
CLI/cache/publication/CMake cases). An additional pure-C, two-TU static-inline
CMake consumer materializing/copying/freeing owning DEFAULTs passed **1/1**.
Existing enum conversion warnings occur in the batch-2 harness, so this result
is not a warning-free claim. WSL PATH has no `clang`/`clang++`; Clang verification
was not run. The final toolchain task remains open.

## 可复跑命令

```sh
cmake --build build-rev1-release -j1
ctest --test-dir build-rev1-release --output-on-failure --timeout 600
cmake --build build-rev1-san -j1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-rev1-san --output-on-failure --timeout 600
python3 scripts/validate_codec_matrix.py --compiler build-rev1-release/asn1cxx --cxx g++ --output build-rev1-quick/cgen3-matrix
python3 scripts/validate_installed_package.py --build build-rev1-release --output build-rev1-quick/cgen3-package
```

Both builds enable compiler, both runtimes and tests, use offline GTest source
`build/_deps/googletest-src`, GCC 13.3.0, Release with sanitizers OFF and Debug
with ASan+UBSan ON. Detailed current logs are ignored under
`build-rev1-quick/cgen3-*`. CTest shape/RRC output also writes `metrics.json`
under each build's `tests/cgen3-shapes` and `tests/cgen3-rrc_types` directories.
Peak memory is GNU time's maximum RSS for the compile/link command and its
children (KiB), not target PDU memory or whole build memory. Compile/link times
are observations on this host, without a speed guarantee.

## 实测产物/资源（前缀 stage3）

Small cross-module fixture, 32 reachable IR types, GNU strict C11 compile/link:

| Shape | C sources | Max source bytes | Header bytes | Max source functions | Compile+link seconds | Peak child RSS KiB |
|---|---:|---:|---:|---:|---:|---:|
| outline | 1 | 96040 | 19924 | 288 | 0.643 | 36224 |
| inline | 0 | 0 | 129620 | 0 | 0.496 | 28160 |
| modules | 2 | 91355 | 19924 | 270 | 0.701 | 35584 |
| shards | 4 | 27572 | 19924 | 72 | 0.823 | 27472 |
| module_shards | 5 | 27572 | 19924 | 72 | 0.892 | 27188 |

Whole LTE RRC type artifact set, budget 128 (common public header remains whole):

| Build | IR types | C sources | Max source bytes | Max source functions | Compile+link seconds | Peak child RSS KiB |
|---|---:|---:|---:|---:|---:|---:|
| Release | 1037 | 9 | 714307 | 1152 | 10.059 | 93616 |
| ASan+UBSan | 1037 | 9 | 714307 | 1152 | 19.777 | 190400 |

Source counts/bytes are generator outputs; function counts are independently
checked against actual definitions in the shape test, and each current C source
is compiled/linked. Time/RSS include the harness and linker, not only individual
shard parsing. Inline moves definitions into the header, so zero C source bytes
are not zero generated code. These type-only results are not a benchmark against
full C++ codec generation. Thresholds do not provide a hard byte bound.

Final logs under `build-rev1-quick`: `cgen3-release-final-tests.log`,
`cgen3-san-final-tests.log`, `cgen3-matrix.log`, `cgen3-package.log`,
`cgen3-msvc-tests.log` and `cgen3-msvc-inline.log`. The first failed sanitizer
run remains in `cgen3-san-tests.log`; both full builds have separate build logs.

## 未完成范围

Full codec orchestration, cumulative PDU budgets, independent generated-C PDU
oracles, the final C 7x12 matrix and full SDK/cross toolchain deployment remain
in batches 4+. RRC checks cover the whole fixture's type artifact set and selected
storage lifecycle, not valid values or wire behavior of every PDU. The single
public header is deliberately unsplit; count limits are soft, not byte limits.
