# CGEN batch 5: C BER/DER/CER and whole RRC acceptance

Date: 2026-10-26. Scope: CGEN-13..15 in [the unchanged acceptance list](c-codegen-todo.md).
This is the supported C value profile, not a claim of complete X.690 coverage.

## Implementation and contracts

`byte_io.h` provides bounded owning readers and transactional writers. `tlv.h`
parses complete definite/indefinite envelopes, verifies nested envelopes and EOC,
and supplies primitive values. `tlv_schema.h` interprets generated C storage
metadata. The compiler still does not link runtime; runtime includes no IR/AST.
Generated schema getters permit recursive types and all three output shapes.
BER, DER and CER share this implementation with a per-call policy. PER shares
X.690 REAL/OID contents through `value_content.c`, not the TLV schema engine.

Supported values: BOOLEAN, NULL, int64/BigInt INTEGER, ENUMERATED, BIT/OCTET
STRING, OID/RELATIVE-OID (uint64 arcs), host-double REAL, UTF8String,
IA5/Numeric/Printable/VisibleString; recursive SEQUENCE/SET/CHOICE/OF,
OPTIONAL, supported scalar and bit/octet/string literal DEFAULTs, extension
additions/version groups and opaque complete-TLV open fields. Constraints are
checked at each node, including character-count SIZE for UTF8String. Owning
DEFAULTs materialize only when absent; partial decode frees owned data.

Canonical checks include minimal headers/integers, definite DER vs indefinite
constructed CER, DEFAULT omission, DER actual-tag SET order vs CER static
minimum-tag CHOICE order, lexicographic SET OF, REAL binary normalization,
unused tail bits and 1000-content-octet CER segments. Restricted character
string fragments carry OCTET STRING tags (X.690 8.23.3). An automatic tag on
an untagged CHOICE wraps the selected complete TLV; replacing its selected
alternative tag would make decoding ambiguous.

Generated public TLV decoders replace an initialized value and consume one TLV
only on success; failure leaves value and reader cursor unchanged. Reader input
ownership is separate from the per-PDU decode budget. Defaults: 16 MiB wire/PDU
requested payload allocation, 65536 elements, depth 256, individual string 64 KiB,
BigInt 1024 32-bit limbs. String contents are validated/count-budgeted first, then copied into one exact
allocation (no unbudgeted growth buffer);
collection slots, indirect nodes, integers, OIDs and DEFAULT allocations reserve
before allocation. Writer capacity/temporary envelope allocations obey the byte
writer cap; this is not a live-heap or CPU-time quota.

`asn1_ber_tag_t.number` is now uint64_t, fixing decoded tag truncation above 32
bits. This changes that public struct ABI: rebuild C runtime and its consumers
together. Generated value layout ABI remains version 2. Descriptors are runtime
metadata with C function pointers; they contain no compiler objects.

## Executable evidence

- `CGenBatch5.TlvCore`: high tags through UINT64_MAX, long/minimal lengths,
  indefinite/nested envelopes and EOC, signed >128-bit INTEGER, constructed BER
  strings, CER boundaries/truncations, depth rejection and byte cursor failures.
- `CGenBatch5.Generated{outline,inline,split}`: complete C PDU values, canonical
  negative cases, explicit/implicit and CHOICE tags, owning DEFAULT omission and
  materialization, SET/SET OF ordering, REAL, OID, UTF8 character SIZE/repertoire,
  CER strings and duplicate/unknown extension member behavior.
- `CGenBatch5.AllocationFailures`: fail every allocation in encode/decode until
  success for owning DEFAULT, SET OF, recursive collections and CER fragments;
  verify no leak, unchanged destination/cursor and unchanged writer on failure.
- `CGenBatch5.ResourceLimits`: rebuild the runtime with a 96-byte cumulative
  payload budget and 8-element limit; small wires trigger aggregate limits while
  leaving destination/cursor unchanged.
- `CGenBatch5.EntryShapeMatrix`: 21 independently compiled consumers, C11/C++17,
  UPER-only/BER-only/combined, out-of-line/inline/split and C module-internal
  shards. Compare exact bytes and actually decode each result.
- `CGenBatch5.WholeRrc`: generate the entire LTE RRC 8.6.0 source, compile/link
  compile the RRC slice separately, then all 1037 full-module types in 9 `.c` sources (largest 1,601,238 bytes in the
  recorded run), then encode/decode RRCConnectionRequest in UPER and BER.
  This proves a representative PDU, not every RRC PDU or all field values.

Whole RRC schema SHA256:
`53be8d24536eff9084361af881329fd7c49fdb658ceda84f605197c65adb5895`.
Live independent oracle: Ubuntu 24.04 `/usr/local/bin/asn1c`, v0.9.29,
`-gen-PER -fcompound-names`, compiled from the same entire schema. Independently
encode the same value, compare exact UPER/BER bytes, and decode project output
with asn1c. Project decoding uses those same independently matched bytes.

| Wire | Frozen vector |
|---|---|
| UPER | `404080c10158` |
| BER (asn1c DER encoder emits a valid BER representative) | `3015a013a011a008810600010203040581010382020700` |

The script writes `evidence.json` with schema/oracle-source hashes, version,
actual source/type counts, vectors and sanitizer configuration. Oracle source
hash for the recorded run:
`037f317dc3652e7718853457fc406acebc96dc5ab9a7c4fb7875a46324146956`.
CER has no independent oracle in this gate: vectors/negative cases derive from
X.690, and do not count as external differential evidence. asn1tools is absent.

## Explicit telecom blockers and unsupported inputs

The unchanged CGEN-15 acceptance permits filling full-module gaps **or listing
blockers** for LPP/S1AP. Neither full module is counted as successful C output.

- LPP 14.3.0: full UPER+BER emission rejects UTCTime, including
  `CommonIEsProvideLocationInformation.locationTimestamp-r13` and sensor
  `measurementReferenceTime-r13`. Calendar/time grammar and DER/CER time
  normalization have not been implemented in the C TLV value profile.
- S1AP 14.4.0: combined emission rejects the existing C PER profile starting at
  extensible `E-RAB-ID` and propagating through IE containers. Table-driven
  generated TLV typed-open decoding also remains unsupported; no stubs emitted.
- C TLV rejects BMPString/UniversalString and other nonlisted character/time
  kinds, named-bit canonical shortening, CONTAINING, unsupported DEFAULT literal shapes and unsupported lowered
  types before publication. REAL is binary64/selected decimal BER input, not an
  exact arbitrary REAL representation; decimal DER/CER returns Unsupported because
  a double cannot retain its abstract base-10 triple. Other valid REAL encodings can return
  Unsupported. An opaque open field contains one complete TLV; unknown payload
  primitive canonical semantics cannot be inferred without its schema. Existing
  PER typed-open views cannot be reused silently as BER payloads: nonnull typed
  views return Unsupported on the TLV path.

Standard reference: [ITU-T X.690 (02/2021)](https://www.itu.int/rec/T-REC-X.690-202102-I/en),
8.1 (envelopes), 8.23 (strings), 9/10/11 (CER/DER restrictions).

## Final gates and environment

| Gate | Result |
|---|---|
| Windows MSVC Release, full default build and CTest | 461/461 passed |
| Ubuntu 24.04 GCC 13.3 ASan+UBSan, all 80 executable test/consumer targets and full CTest | 494/494 passed |
| Original C++ 7 fixtures ×12 independent emit + `-fsyntax-only` | 84/84 passed |
| C/C++ single/combined entry and shape consumers | 21/21 passed |
| Whole RRC generated C + live asn1c UPER/BER | passed; 1037 types / 9 sources |
| Standalone non-instrumented GCC strict C11 PDU / dependencies | passed; libc/libm, no C++ standard library |
| Generated complete C inline header included as C++17 | passed |
| Remediation skill structural validation | passed using WSL Python/PyYAML |

Sanitizer runtime and every executable test target remain instrumented. Large
non-executed C++ compile-gate libraries need not be recompiled into instrumented
machine code: their independent 84 syntax checks are a separate full matrix.
MSBuild full gate uses `/m:1`: parallel projects can request the same existing
C++ output directory and correctly trip the publication lock. This pre-existing
shared-generation scheduling issue remains an engineering follow-up, not a
reason to disable the lock or skip tests. Independent test execution uses two jobs.
The old batch-3 standalone consumer now explicitly links Unix libm after the
REAL/OID content extraction; CMake `asn1::c_runtime` already propagates libm.

WSL Clang and asn1tools are not installed; neither is reported as executed.
Live asn1c is installed and exercised. Windows Python lacks PyYAML, so the skill
validator ran successfully using installed WSL Python/PyYAML. No missing tool
blocked the declared batch-5 acceptance. Clang/all-family C coverage remains
part of the unchecked later batches.

Evidence logs from this run (ignored build artifacts):
`build-rev4-msvc/cgen5-final-full-{build,test}.log`,
`build-rev5-wsl-san/final-all-tests.log`,
`build-rev5-wsl-san/matrix-84.log`, and
`build-rev5-wsl-san/tests/cgen5-{matrix,rrc-acceptance}/evidence.json`.
The checklist closes CGEN-13..15 with the value-profile limits and the expressly
permitted LPP/S1AP blockers above; CGEN-16..24 remain unchecked.
