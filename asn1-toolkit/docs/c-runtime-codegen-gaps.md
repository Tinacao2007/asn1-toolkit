# C generation requirements ? runtime gap audit (CGEN-02)

Audit date: 2026-10-24. Implementation inspection covers `src/c_runtime/*.c`,
`include/asn1/c_runtime/*.h` and C++ emitters listed below. Evidence of primitive
execution is [the declaration register](c-runtime-test-coverage.md),
[REV-01..03](rev01-03-validation.md) and [REV-04..09](rev04-09-validation.md).
A header name, a TODO comment or reader initialization does not count as a codec.
CGEN batch 6 now opens all twelve entries within the declared C value profile; see [batch 6 evidence](cgen-batch6-validation.md). Batch 4 now
has a tested UPER/APER generated-PDU subset; the rows below distinguish reusable
PER/TLV primitives from generated schema consumption. The added OER/text families
are established by their generated-PDU acceptance, not by header names.

Legend: R = existing reusable implementation, M = implementation required,
U = outside the currently supported value/instruction domain and must diagnose.
The ABI/lifecycle design is [c-generated-abi.md](c-generated-abi.md).

## C++ call paths used as the comparison baseline

CLI `emit_code` ? unified `emit_generated_to_dir` ? CppGenerator ? Emitter.
`src/codegen/emit_per.cpp` renders `encode/decode_boolean`, `integer`, string,
`sequence_preamble`, `extension_additions`, OF chunk and open-type calls into
`asn1::uper`/`asn1::aper`; `src/runtime/per/{codec,primitives}.cpp` share Variant.
C `per_core.c` already shares `asn1_per_variant_t`; `per.c` is legacy UPER wrapping.
Do not duplicate either core when adding generated C orchestration.

`emit_tlv.cpp` renders `asn1::{ber,der,cer}::encode/decode_*`, constructed writers,
inner ByteReaders and tagged content. The runtime implementations are
`src/runtime/{ber,der,cer}`. `emit_oer.cpp` similarly uses OER/COER value codecs,
presence/extension preambles and policy-specific entry calls. `emit_jer.cpp`
traverses parsed JSON values; `emit_xer.cpp` traverses parsed XML elements and
attributes, with XER/CXER/E-XER policy. C writer puts/printf cannot replace these
tree parsers, validation and structured decode paths.

## Twelve-entry register

Every row separately records primitive, container, policy, complete decode,
location and resource requirements. M entries are not currently callable APIs.

| Entry | Primitives (R / M / U) | Containers (M) | Policy (R / M) | Complete decode | Error location | Resource enforcement |
|---|---|---|---|---|---|---|
| uper | R bool, int64/BigInt, fragments, bits/octets, open bytes, normally-small, ENUMERATED, uint64-arc OID, common REAL and restricted strings; M full string repertoire/profile | R tested root/extension CHOICE, OF, recursive SEQUENCE OF, trailing-root, collection fragmentation, extensible SEQUENCE/SET additions and version groups, opaque and selector-driven typed open payload; M unsupported unknown extension preservation | R unaligned Variant; M complete schema membership/default coverage | R tested generated PDU paths; M broader interop | R bit offset; M field path/first error | R primitive caps/depth and cumulative bytes/count budget; M complete fault matrix |
| aper | same reusable PER primitives and value families as uper | same schema orchestration using shared Variant; same unsupported structures | R aligned Variant; M complete schema/default checks | R tested aligned PDU cases; M broader interop | R primitive caps; M field path | M aggregate bytes/count budget |
| ber | R BOOLEAN/NULL/signed INTEGER/ENUMERATED, bits/octet/restricted strings, binary64 REAL, uint64 OID; R UTCTime; U GeneralizedTime/other string kinds | R recursive SEQUENCE/SET/CHOICE/OF, tags, indefinite/EOC, extensions, opaque TLV open bytes | R BER envelope/value rules | R generated transactional decode; U automatic TLV typed-open view | R reader cursor; M field path | R depth, string/limb caps and cumulative payload bytes/elements |
| der | R shared BER value profile | R shared structures plus SET/SET OF order | R minimal/definite forms, DEFAULT omission, REAL/tail-bit checks; R UTCTime canonical profile; U GeneralizedTime | R strict supported-profile decode | R reader cursor; M field path | R shared caps/budgets |
| cer | R shared BER value profile | R indefinite constructed values and 1000-octet fragments | R SET static-CHOICE order, SET OF, DEFAULT, REAL, fragments/tail bits; R UTCTime canonical profile; U GeneralizedTime | R strict supported-profile decode | R reader cursor; M field path | R shared caps/budgets |
| oer | R exact integer/BigInt, lengths, bool/enum/tag/open, bits/octets, OID, host-double REAL, declared strings | R SEQUENCE/SET/CHOICE/OF and extension groups | R BASIC policy | R transactional generated PDU decode | R byte cursor; M field path | R cumulative value budgets |
| coer | same C OER values | same structures, SET OF order | R per-entry canonical lengths/integers/BOOLEAN/quantity/defaults | R strict decode | R byte cursor; M field path | R shared OER budgets |
| jer | R exact integers, UTF-8 escaping, supported value profile | R objects/arrays/choice/OF and supported instructions | R shared X.697 syntax; no canonical JSON claim | R full document parser and structured decode | R parser byte offset; M field path | R tree/value/depth/count/byte budgets |
| jeri | R exact integers, UTF-8 escaping, supported value profile | R objects/arrays/choice/OF and supported instructions | R shared X.697 syntax; no canonical JSON claim | R full document parser and structured decode | R parser byte offset; M field path | R tree/value/depth/count/byte budgets |
| xer | R XML parsing/escaping, supported values | R elements/attributes/structures/OF | R per-entry XML policy; E-XER unsupported instructions diagnose | R structured full-document decode | R parser byte offset; M field path | R tree/value/depth/count/byte budgets |
| cxer | R XML parsing/escaping, supported values | R elements/attributes/structures/OF | R per-entry XML policy; E-XER unsupported instructions diagnose | R structured full-document decode | R parser byte offset; M field path | R tree/value/depth/count/byte budgets |
| exer | R XML parsing/escaping, supported values | R elements/attributes/structures/OF | R per-entry XML policy; E-XER unsupported instructions diagnose | R structured full-document decode | R parser byte offset; M field path | R tree/value/depth/count/byte budgets |

`per` is the uper|aper alias, not a thirteenth entry. NULL can require no PER
payload, but that does not supply a generated NULL codec. BigInt conversion is
not a BER/OER/JSON value codec. `ByteReader` now supports bounded byte reads/views/skips; generated TLV decode
commits value and cursor together. OER/COER, JER/JERI and XER/CXER/E-XER now use `schema_codec.h`, `text_tree.h` and the family schema implementations; legacy raw writer helpers are not the generated codec entry APIs. Existing primitive failures follow
[c-runtime-contracts.md](c-runtime-contracts.md); no blanket cursor rollback.

## Dependency allocation and opening gates

CGEN-04..06 must implement generated layouts/lifecycle, naming and actual shared
plan consumption. CGEN-07..09 owns shards and `.h/.c` manifest publication.
CGEN-10..12 owns missing PER values, generated orchestration and failure budgets;
CGEN-13..15 owns byte reader and BER-derived families; CGEN-16..19 owns OER and
XML/JSON parsing/policies. Keep unsupported domains explicit in every gate.
Opening an entry requires pure C11 compile/link/run, schema-specific negative
cases, allocation-failure cleanup and applicable independent wire vectors.
Self-roundtrip or raw writer output is insufficient. Later checklist boxes remain
unchanged. REV-27 type/lifecycle consumption is recorded in the batch 2 evidence.


Batch 2 now provides the separate experimental type/lifecycle stream API,
including explicit BigInt/bit/enum/OID/REAL/open-content storage profiles and
shared-plan descriptors/selector lookup. See [ABI boundary](c-generated-abi.md).
This removes type-layout blockers within that declared profile; none of the
missing wire APIs in this audit is thereby implemented. Full C CLI entries and
generated-PDU/oracle acceptance is now recorded in batches 4–6; REV-27 planning reuse is covered
by the batch 2 generated-C consumers.


Batch 3 adds explicit type-only artifact generation, static-inline/module/source
shapes, verified cache and source-list publication. Batch 4 now has checked C
UPER/APER paths for recursive SEQUENCE OF, plain extension additions, version-bracket
groups, opaque complete-encoding open payloads, Numeric/IA5 strings, and binary64
plus decimal NR REAL decode. Out-of-line, inline and split generated C consumers
compile, link and run. Known table-constrained open fields retain raw octets and
automatically decode into an owned selected type; typed values are re-encoded by type.
Choice extensions and trailing-root are covered. The declared first PER profile is
closed; representative asn1c UPER vectors cover fragmentation, extension CHOICE and
trailing-root. Full restricted alphabet/string-fragmentation coverage, the complete
generated-allocation/cursor fault matrix and independent oracle vectors for every
schema shape remain open profile extensions. Collection fragmentation, cumulative
PDU budgets and collection-growth fault injection are implemented. The CLI accepts
only the supported profile and diagnoses unsupported shapes before publication.
See [C artifact contract](c-codegen-output.md) and [batch 4 evidence](cgen-batch4-validation.md).

Batch 5 implements the C BER/DER/CER profile and whole-RRC UPER+BER PDU gate;
see [evidence, ABI change and LPP/S1AP blockers](cgen-batch5-validation.md).

## CGEN7 closure of the telecom generation blockers

C11 now handles named-bit shortening/padding across the twelve entries, UTCTime
calendar/offset validation and DER/CER/PER/COER/CXER canonical forms, and finite
sparse INTEGER roots. X.691 13.1 uses the root RANGE for its extension bit; holes
are still rejected for non-extensible constraints. The independent asn1c root
vectors are separate from asn1tools, whose UNION handling has a documented gap.

Known table-governed JSON/XML fields now decode into owned selected views and
encode the actual selected value (X.697 41 / X.693 open-type notation). JSON can
put the governor last; selection occurs after ordinary fields. Unknown selectors,
missing contained-type context and unsupported instructions remain explicit
errors, with transactional output and cumulative allocation limits. Empty private
object sets use opaque complete PER payloads without inventing scalar selectors.
C 84 syntax gates do not certify every LPP/S1AP PDU or arbitrary future IE type.
