# C generated artifact and ABI design (CGEN-01)

This is the batch 1 contract for the future emitter, not an implemented C API.
`--lang c` currently fails with an Unsupported diagnostic and publishes nothing.
The compiler stays C++17, uses Diagnostics/Result, and never links either runtime.
Generated C11 alone bridges compiler decisions to `asn1::c_runtime`.

## Names, artifacts and versioning

The default C prefix will be `asn1_gen`; `--c-prefix p` selects `p` independently
of the C++ namespace. Prefixes start with an ASCII letter, then letters, digits
or underscores, with no double underscores. A prefix is always followed by `_`,
so a keyword prefix cannot create a keyword identifier. All public type, enum
constant, helper, lifecycle and codec names include the prefix and module name:
`p_Module_Type`, `p_Module_Type_init`, `p_Module_Type_free`,
`p_Module_Type_encode_uper`, `p_Module_Type_decode_uper`.
ASN.1 hyphens become underscores; collisions, reserved identifiers, C11 keywords,
and case-insensitive filename collisions require deterministic escaping or a
pre-publication diagnostic (CGEN-06). Do not silently merge names.

Outline artifacts will be `generated.h` and `generated.c`; module split adds
`Module.h`/`Module.c` and an aggregate header. Single-header definitions use
`static inline`. Module-internal source shards and machine-readable source lists
are CGEN-07..09, not implemented options in batch 1. Header guards include the
prefix and module; headers include only C standard and C runtime headers, with
extern-C guards for C++ consumers. No C++ types, exceptions, RTTI or standard
library linkage are allowed in generated C.

Generated headers will declare a prefixed `GENERATED_ABI_VERSION` macro, initially
1, and the required C runtime ABI version. No generated ABI is exported yet.
Layout/signature/ownership changes require a generated ABI major increment and
regeneration/recompilation of all consumers; no stable binary interchange across
compiler/CRT, architecture, packing or ABI versions is promised. Runtime ABI is
versioned separately; existing runtime declarations are not changed by this plan.

## Value layouts and capability boundaries

All owning members initialize to a non-owning zero state. The table specifies
intended layouts; no generated type or lifecycle implementation exists yet.

| ASN.1 value | C layout / semantics | Limits or missing capability |
|---|---|---|
| INTEGER | wrapper containing int64_t when normalized domain fits; otherwise asn1_bigint_t | check root membership and host range before conversion; BigInt discrete/bounded domains need checked comparisons, not narrowing |
| BOOLEAN / NULL | wrapper containing int (0/1); NULL wrapper with one reserved uint8_t | invalid BOOLEAN rejected; NULL emits no payload where the encoding requires none |
| ENUMERATED | int64_t root/known value plus explicit extension representation | unknown extension ordinal must be preserved or diagnosed; PER enum/normally-small helpers missing |
| OCTET STRING / character strings | owned uint8_t* data, size_t count/capacity; length-delimited, never dependent on NUL | character repertoire/UTF validation and transcoding absent; full ASN.1 character domains unsupported |
| BIT STRING | asn1_bitstring_t, bit count separate from storage capacity | unused tail bits normalized; NamedBitList trimming needs schema semantics |
| OBJECT IDENTIFIER / RELATIVE-OID | owned array of unsigned arc values with count/capacity | arc representation must preserve the supported domain; arbitrary-size arcs unsupported until explicitly implemented; no C OID codec yet |
| REAL | wrapper containing double for declared binary64 profile | arbitrary-precision ASN.1 REAL unsupported; reject unrepresentable mantissa/range, preserve negative zero; C REAL codec missing |
| SEQUENCE / SET | struct in declaration order; members use generated wrappers | wire order, effective tags and extension grouping come from shared plan, not C struct order |
| OPTIONAL / DEFAULT | OPTIONAL has explicit has_field; DEFAULT stores semantic value initialized to default | absent DEFAULT materializes default, encoding compares semantic equality; no pointer-as-presence convention |
| CHOICE | enum discriminator (0 invalid) plus union of alternatives | init selects invalid, free dispatches only active arm; decode constructs replacement arm before commit |
| SEQUENCE OF / SET OF | owned T* data; size_t count/capacity | checked count*sizeof(T), element cleanup, canonical SET OF sorting where required |
| recursive types | owned indirect pointers on recursive edges | dependency SCC planning prevents infinite by-value layout; depth/cycle checks required |
| open / table-constrained types | selector plus discriminated known value; owned encoded octets for explicitly permitted unknown values | retain codec identity and exact bit/octet length; never reinterpret PER bytes as BER; unresolved selectors unsupported |

General constraints, arbitrary REAL, oversized OID arcs and unsupported encoding
instructions are diagnosed before publication until their semantics and wire paths
exist. Parse/IR acceptance does not imply C generation support. All generated C
values and all codecs remain Unsupported in batch 1, including simple BOOLEAN.

## Function and ownership contract

Planned lifecycle signatures: `void p_Module_T_init(p_Module_T *out)` and
`void p_Module_T_free(p_Module_T *value)`. Init accepts new/zero/freed storage,
allocates nothing and installs semantic defaults. Free accepts initialized or
partially constructed values and NULL, releases nested storage, and restores the
initialized state; repeat free is safe. Never init over a live owning value.
Owning values cannot be copied by struct assignment. A future
`asn1_status_t p_Module_T_copy(const p_Module_T *source, p_Module_T *out)`
performs a deep temporary copy and commits only on success. Explicit move must
leave the source initialized. No public copy API may succeed with shared owners.

Planned binary signatures put the runtime stream first, then value, then a
prefixed generated operation context: encode `(writer*, const T*, context*)`,
decode `(reader*, T* out, context*)`, returning `asn1_status_t`. Text encoders use
caller buffer/capacity, const value, written-out, context in that order; text
decoders use input/size, value-out, consumed-out, context. These contexts and
complete byte/text stream APIs do not currently exist; do not emit calls to them.
The context must carry depth/element/cumulative allocation budgets and first-error
location (bit offset for PER, byte offset otherwise, plus schema field path).
No global errno, longjmp or mutable global diagnostics are used.

Input streams and encode values are borrowed for the call. Decoded values own
all retained storage, including strings and unknown open content; no implicit
borrowing from input. Caller-supplied buffers remain borrowed. Use the same
runtime allocator/CRT for allocation and release, as in
[c-runtime-contracts.md](c-runtime-contracts.md). No per-object allocator switch.

Decode builds an initialized temporary; failure cleans it and preserves the old
initialized output. Success frees/replaces old output. Scalar out/written/consumed
parameters commit only on success. Stream cursor may advance on failure; composite
encode may leave a prefix. Caller must discard/reset a failed writer. This does
not change existing primitive contracts, some of which require a fresh output.
Every runtime call's status propagates immediately, retaining the first error.
C compiler-internal failures remain Result/Diagnostics, not target status casts.

Generated checks must enforce recursion depth, element counts, total bytes,
BigInt limbs and arithmetic overflow before allocation. The existing C primitive
limits (64KiB string default, 1024 limbs, depth constant 256, CHOICE constant 256)
are not proof of cumulative generated-PDU enforcement. New budgets may tighten
but cannot silently bypass primitive limits. OOM, truncation, unsupported,
resource-limit and invalid-value failures remain distinguishable.
UPER/APER share one Variant-parameterized core; derived canonical families share
base mechanisms with separate policy. Shared `codegen/plan.hpp` remains the source
of presence/tag/constraint/extension/selector decisions (CGEN-05 still pending).
