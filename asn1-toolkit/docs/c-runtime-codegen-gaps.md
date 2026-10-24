# C generation requirements ? runtime gap audit (CGEN-02)

Audit date: 2026-10-24. Implementation inspection covers `src/c_runtime/*.c`,
`include/asn1/c_runtime/*.h` and C++ emitters listed below. Evidence of primitive
execution is [the declaration register](c-runtime-test-coverage.md),
[REV-01..03](rev01-03-validation.md) and [REV-04..09](rev04-09-validation.md).
A header name, a TODO comment or reader initialization does not count as a codec.
All twelve generated C entries are currently Unsupported. No runtime API was
added in batch 1; the gaps are dependencies of later batches, not successful stubs.

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
| uper | R bool, int64/BigInt, length fragments, bits/octets/open bytes; M enum/normally-small, OID, REAL, characters; U general REAL/unsupported constraints | preambles, CHOICE/OF, extension groups, selector dispatch | R unaligned Variant; M schema membership/defaults | R listed scalar/string values; M generated PDU and inner open decode | R bit-reader position; M field path/first error | R string/limb limits, overflow checks; M cumulative bytes/count/depth |
| aper | same reusable PER primitives and missing value families as uper | same orchestration using shared plan | R aligned Variant; M schema/default checks | R listed values with alignment; M PDU | R bit-reader position; M field path | R primitive caps; M cumulative PDU budgets |
| ber | R tag/length, BigInt signed conversion; M bool/null/int/OID/REAL/string TLV values | explicit/implicit tags, SET/CHOICE/OF, indefinite/EOC, unknown members | R tag/length syntax only; M base-rule validation | M byte reader operations and full TLV/value decode | R helper consumed on success; M byte cursor/path/error location | R helper arithmetic caps; M nesting/aggregate content/count |
| der | R BER helpers; M value codecs | BER structures plus canonical SET/SET OF | M minimal integers/lengths, definite form, DEFAULT omission, canonical REAL/order | M strict DER decode | M byte cursor/path | M aggregate/depth limits; reuse primitive caps |
| cer | R BER helpers; M values and constructed string chunks | BER structures plus indefinite forms, EOC and fragments | M 1000-octet rules, canonical order/DEFAULT/REAL/tail bits | M strict CER decode and fragment accounting | M byte cursor/path | M nested/cumulative fragment/depth limits |
| oer | R BigInt conversion only; M OER values, byte I/O/lengths/OID/REAL | presence/extension preambles, CHOICE/OF/open | M base OER rules; U unrepresentable REAL | M full OER decode | M byte cursor/path | M total allocations/count/depth; reuse limb caps |
| coer | R BigInt conversion only; M OER values | same structures as OER | M canonical lengths/preambles/order, separate COER policy | M strict COER decode | M byte cursor/path | M total allocations/count/depth |
| jer | R bounded raw writer init/puts/printf; M escaping, exact integer/string/value encode; U unsupported instructions | object/array, duplicate/unknown member and required-field handling | M X.697 semantics and instruction checks | M lexer/parser/value traversal; reader init only borrows input | R reader pos storage; M actual parse byte/path errors | R output capacity; M input/token/tree/depth/cumulative caps |
| jeri | R JER raw writer helpers; M JER value layer | same JER structures | M separate entry dispatch with shared X.697 syntax; no canonical-JSON claim | M same JER parser/value decode | M byte/path | R output cap; M parse/PDU budgets |
| xer | R bounded raw writer init/puts; M XML escaping/value encode | element/sequence/choice/OF traversal | M BASIC-XER semantics | M XML parser and typed decode; init only borrows | R pos storage; M byte/path diagnostics | R output cap; M token/tree/depth/cumulative caps |
| cxer | R raw XER writer only; M XML value layer | shared XML structures | M canonical lexical/element/order/DEFAULT rules | M XML parser plus CXER checks | M byte/path | R output cap; M parser/PDU budgets |
| exer | R raw XER writer only; M XML value layer | attribute/element mappings and instruction-aware traversal | M implemented instruction mapping; U unimplemented instructions | M XML parser and attribute/required/typed checks | M byte/path | R output cap; M parser/PDU budgets |

`per` is the uper|aper alias, not a thirteenth entry. NULL can require no PER
payload, but that does not supply a generated NULL codec. BigInt conversion is
not a BER/OER/JSON value codec. `ByteReader` currently only creates/destroys an
input copy. DER/CER/OER/COER/CXER/E-XER/JERI headers contain skeletons rather than
those claimed policy implementations. Existing primitive failures follow
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
generated-PDU/oracle acceptance remain open; REV-27 planning reuse is covered
by the batch 2 generated-C consumers.


Batch 3 adds explicit type-only artifact generation, static-inline/module/source
shapes, verified cache and source-list publication. No missing wire primitive in
this audit is thereby implemented; all twelve ordinary C codec requests still
reject. See [C artifact contract](c-codegen-output.md).
