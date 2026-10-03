# ASN.1 Toolchain Architecture

New code lives only in `asn1-toolkit/`. Sibling trees such as `pasn1/`, asn1c,
`Python_asn1tools/`, and `FFASN1dump/` stay untouched reference material.
`pasn1/tools/act` already shows the failure mode to avoid: parser, tag logic,
and BER/UPER generators sit in one compiler (`parser.h` next to
`ber-gen-enc.h` and `uper-gen-enc.h`).

This document is the design. **Phase 39** closes the validation gap: Person
round-trips across UPER/APER/OER/COER/BER/DER/JER, and telecom gates that
analyze full 3GPP modules plus emit/round-trip a named RRC slice. Full RRC/LPP/S1AP
codegen still blocked on anonymous nested CHOICE/SEQUENCE (emit reports the
gap). Phase 38 added `rrc_slice` UPER round-trips. Phase 37 added `--codec coer`.

## Pipeline

```text
ASN.1 source
  → Lexer → Parser → AST
  → Semantic passes (symbol table, tags, constraints)
  → Type IR
  → C++ code generator
  → Generated value types + thin encode/decode
  → Runtime primitives (BER / DER / PER / OER / COER / XER / CXER / E-XER / JER / JERI)
```

Compiler libraries never include codec algorithms. The runtime never includes
AST, symbol-table, or IR headers. Generated `.cpp` is the only bridge: it
calls runtime primitives.

## Architectural decisions

- **Hand-written lexer and recursive-descent parser.** ASN.1 separates
  `TypeReference` (initial capital) from `identifier` (initial lowercase), so
  type vs value assignments are a lexical distinction. A hand-written parser
  keeps source locations and diagnostics under our control and adds grammar
  phase by phase. Flex/Bison and ANTLR are rejected.
- **AST is syntax. IR is meaning.** The parser does not resolve imports,
  compute automatic tags, or fold `INTEGER (0..255)`. Codecs never walk the
  AST.
- **Codec algorithms live once, in the runtime.** Codegen emits readable
  orchestration that calls shared primitives. It does not emit a private PER
  implementation per type. Unit tests in Phases 6–10 call the primitives
  directly, so codecs do not wait for codegen (Phase 11).
- **UPER and APER share one PER implementation.**
  `per::Variant::{Unaligned, Aligned}` is an argument. Alignment happens only
  inside primitives, at the points X.691 requires it. APER is not UPER plus a
  final pad.
- **DER is BER with a canonical policy** (shortest definite length, no
  constructed strings when DER forbids them, SET component order).
- **Unsupported syntax is recognized and rejected.** Phase 23 adds `INSTANCE OF`
  and XER/EXTENDED-XER encoding instructions. Phase 13–14 parse `CLASS`, objects/object sets,
  `Class.&field`, table / component-relation constraints, parameterized types,
  `IMPORTS Name{}`, defined-syntax objects (WITH SYNTAX + heuristic fallback when
  the class appears later in a multi-module file), and version brackets `[[…]]`.
  Phase 12 adds `ENUMERATED`, `OBJECT IDENTIFIER`, `RELATIVE-OID`, `SET`,
  `SET OF`, and typed `REAL`. Phase 25 implements REAL codecs (IEEE-754
  `double` ↔ ASN.1 binary base-2; unconstrained OER/PER wrap BER content).
  Phase 26 parses `WITH COMPONENTS` and classifies OER IEEE binary32/64 when
  mantissa/base/exponent ranges match X.696.   Phase 27 expands `EXTERNAL`,
  `EMBEDDED PDV`, and `CHARACTER STRING` to associated SEQUENCE IR. Phase 28 implements
  unconstrained INTEGER wider than 64 bits via `asn1::BigInteger`. Phase 29–30 deepen
  BER/DER for associated types and add `encode_external_modern`. Phase 31 wires PER
  extension addition series. Phase 32 injects builtin `ABSTRACT-SYNTAX` (WITH SYNTAX
  `{ &Type IDENTIFIED BY &id [HAS PROPERTY &property] }`) and lowers `EXTERNAL` to the
  modern identification form in IR/codegen.
  `OCTET STRING` / `BIT STRING (CONTAINING Type)` [ENCODED BY …] lower to IR
  with a `containing` type id; outer codecs still treat the value as octets/bits.
- **C++17 only.** `std::optional`, `std::variant`, `std::string_view`,
  `std::filesystem`. No `std::expected` and no `std::span`. A small `Result<T>`
  and `Span<T>` live in `asn1_common`. Decode and constraint failures return
  `Result`; they do not throw. `std::bad_alloc` may propagate.
- **No Boost and no multiprecision dependency.** Unconstrained `INTEGER` uses
  an internal sign-plus-limbs type. Constrained integers that fit in
  8/16/32/64 bits lower to matching C++ types at codegen time.

## Directory layout

```text
asn1-toolkit/
  CMakeLists.txt
  README.md
  include/asn1/
    common/result.hpp
    common/span.hpp
    support/          (Phase 2+) source_location, diagnostics
    ast/              (Phase 3+)
    frontend/         (Phase 2–3)
    semantic/         (Phase 4+)
    constraints/      (Phase 5+)
    ir/               (Phase 4+)
    codegen/          (Phase 11+)
    runtime/          (Phase 6+) ber/ der/ per/
  src/                .cpp mirrors; no codec code under ast/
  tools/asn1cxx/
  tests/
  examples/
  docs/ARCHITECTURE.md
```

Empty directories are not created until a phase needs them.

## CMake targets

C++17, `CMAKE_CXX_STANDARD_REQUIRED ON`, extensions off. CMake 3.16+.

| Target | Kind | Role |
|--------|------|------|
| `asn1_common` | INTERFACE | `Result`, `Span` |
| `asn1_support` | static | source locations, diagnostics |
| `asn1_ast` | static | AST nodes |
| `asn1_frontend` | static | lexer + parser |
| `asn1_semantic` | static | symbol table, analyzer |
| `asn1_constraints` | static | constraint normalizer |
| `asn1_ir` | static | type arena / IR |
| `asn1_codegen` | static | C++ emitter (types + thin UPER/APER calls) |
| `asn1_runtime` | static | `ByteReader`/`ByteWriter`, `BitReader`/`BitWriter` |
| `asn1_ber` | static | BER TLV + primitive/constructed codecs |
| `asn1_der` | static | DER canonical encode/decode on top of BER |
| `asn1_per` | static | PER primitives + type codecs (`Variant::{Unaligned,Aligned}`) |
| `asn1_oer` | static | OER (BASIC) primitives + type codecs |
| `asn1_coer` | static | COER canonical encode/decode on top of OER |
| `asn1_xer` | static | BASIC-XER XML tree + type codecs |
| `asn1_cxer` | static | CXER canonical encode/decode on top of XER |
| `asn1_exer` | static | EXTENDED-XER encoding-instruction effects |
| `asn1_jer` | static | BASIC-JER JSON tree + type codecs |
| `asn1_jeri` | static | JER encoding-instruction effects |
| `asn1cxx` | executable | compiler driver |

`asn1_uper` and `asn1_aper` are **not** separate libraries. They are entry
points in `asn1_per` that pass `Variant::Unaligned` or `Variant::Aligned`.

Link rules:

- `asn1cxx` links the compiler chain only. It does not link BER/PER.
- `asn1_codegen` does not link the runtime. It prints calls such as
  `asn1::per::encode_constrained_whole_number`.
- Generated TUs and codec tests link `asn1_ber`, `asn1_der`, or `asn1_per`.
- Nothing under `src/runtime` includes `asn1/ast` or `asn1/ir`.

## Compiler interfaces (target API)

```cpp
struct SourceLocation { std::string file; int line; int column; };
struct SourceRange { SourceLocation begin, end; };

enum class Severity { Note, Warning, Error };
struct Diagnostic { Severity severity; SourceRange where; std::string message; };

class Diagnostics {
 public:
  void error(SourceRange, std::string message);
  bool ok() const;
  const std::vector<Diagnostic>& items() const;
};

class SourceFile;  // owned buffer; lexer string_views point into it

class Lexer {
 public:
  explicit Lexer(const SourceFile&, Diagnostics&);
  Token next();
};

class Parser {
 public:
  explicit Parser(Lexer&, Diagnostics&);
  std::unique_ptr<ast::Module> parse_module();
};

class Analyzer {
 public:
  explicit Analyzer(Diagnostics&);
  ir::Model analyze(std::vector<std::unique_ptr<ast::Module>> modules);
};

class CppGenerator {
 public:
  void emit(const ir::Model&, const EmitOptions&, Diagnostics&);
};
```

Semantic work is four explicit passes:

1. Declare symbols and diagnose duplicates.
2. Resolve `IMPORTS` and type/value references. Recursive `SEQUENCE` is legal;
   cycles through value expressions or constraint bounds are errors.
3. Assign effective tags (explicit, implicit, automatic). Store the tag on the
   IR component. Codecs do not recompute it.
4. Normalize constraints, then lower to IR.

## AST

Polymorphic nodes, `std::unique_ptr` ownership, `SourceRange` on every node,
one `Visitor`.

`Module` holds name, `TagDefault` (`Explicit`, `Implicit`, `Automatic`),
`EXTENSIBILITY IMPLIED`, imports, exports, and assignments.

Assignments: `TypeAssignment` and `ValueAssignment`.

`Type` base carries optional `Tag` and optional `Constraint`. Concrete types
cover the built-ins and constructed forms listed in the project requirements.
`SequenceType` / `SetType` / `ChoiceType` store a **flat** component list plus
`ExtensionMarker` nodes; the semantic pass splits root vs extension groups.

Constraints are a separate node family (`ValueRange`, `SingleValue`,
`SizeConstraint`, `UnionConstraint`, `IntersectionConstraint`,
`ExtensibleConstraint`). No codec methods on AST classes.

Values keep integer decimal text until bounds are known; host integer width is
decided only then.

## Semantic model and IR

`SymbolTable` maps `Module::Name` to a symbol. IR types live in a `TypeArena`
addressed by `TypeId` (32-bit index). Recursive structures point at a `TypeId`.

Codec-facing descriptors include `IntegerDesc` (bounds, extensible,
named numbers, optional `host_bits` / `is_signed`), `SequenceDesc` (root,
extension groups, trailing root), `Field` (presence, tag, default), and
`ConstraintDesc` (folded ranges for PER). PER reads descriptors; it does not
re-parse `(0..255)`.

`SetDesc` stores source order and canonical tag order (computed once in the
lowerer).

## Codec abstraction

Shared utilities: `ByteReader` / `ByteWriter`, `BitReader` / `BitWriter`.

BER: TLV tag/length/value, short and long definite length, high-tag-number
form, primitive and constructed values. Indefinite length may be decoded by
BER and is a hard error in DER.

DER: same writers with minimal length and canonical SET order; reject
non-canonical input on decode.

PER (parameterized by aligned/unaligned): constrained / semi-constrained /
unconstrained whole number, normally small non-negative whole number, length
determinant, choice index, optional and extension bitmaps, open type.
`align_to_octet()` is called only from APER branches where X.691 requires it.

OER (X.696 BASIC-OER): length determinant, fixed-width integers when the range
fits 1/2/4/8 octets, otherwise length + two's-complement (or unsigned) content;
BOOLEAN as `0x00`/`0xFF` on encode (any non-zero TRUE accepted on decode);
SEQUENCE presence bitmap padded to an octet; extension addition series
(length + unused-bits + presence bitmap + open types); CHOICE context tags
with extension alternatives as tag + open type; OCTET/BIT STRING with
fixed-size eliding the length.

COER (X.696 CANONICAL-OER): same writers with shortest length determinant,
minimal INTEGER/ENUMERATED content, TRUE must be `0xFF`, unused BIT STRING and
preamble/extension-bitmap padding bits must be zero, SET OF components in
ascending octet-string order; reject non-canonical input on decode.

XER (X.693 BASIC-XER): UTF-8 XML element tree (`asn1::xer::Element`); BOOLEAN as
`<true/>`/`<false/>` children; NULL as empty element; INTEGER as decimal text;
OCTET STRING as hex; BIT STRING as binary digits; UTF8String with decimal NCRs
for non-ASCII; ENUMERATED as named empty child; OID as dotted arcs; SEQUENCE /
CHOICE / SEQUENCE OF via nested elements.

CXER (X.693 CANONICAL-XER): empty prolog; no inter-tag whitespace; empty-element
tags as `<name/>`; text escapes only `&`/`<` (raw UTF-8 otherwise); INTEGER as
canonical SignedNumber; OCTET STRING uppercase even-length hex; SET OF sorted by
CXER character-string order; `parse_document` rejects non-canonical XML.

EXTENDED-XER (X.693 E-XER) runtime (`asn1::exer`): encoding-instruction effects
on the Element tree — ATTRIBUTE (XML attributes), BASE64, TEXT, USE-NUMBER,
LIST (space-separated character-encodable items: strings, INTEGER, BOOLEAN,
ENUMERATED identifiers, OID, REAL, OCTET/BIT STRING), UNTAGGED (merge into
parent), NAME, USE-NIL (`xsi:nil`).

JER (X.697 BASIC-JER): JSON value tree (`asn1::jer::Value`); BOOLEAN as
`true`/`false`; NULL as `null`; INTEGER as JSON number; OCTET STRING as hex
string; unconstrained BIT STRING as `{"value","length"}`; ENUMERATED / OID /
UTF8String as JSON strings; SEQUENCE as object; SEQUENCE OF as array; CHOICE as
single-property object.

JER encoding instructions (X.697) runtime (`asn1::jeri`): BASE64 (OCTET STRING as
Base64), ARRAY (SEQUENCE as JSON array; absent optionals as null), NAME
(component/alternative key transform), OBJECT (SET OF key/value as JSON object),
TEXT (ENUMERATED identifier transform), UNWRAPPED (CHOICE without wrapper object).
Frontend parses `JER INSTRUCTIONS`, type-prefix instructions, and
`ENCODING-CONTROL JER`; semantic IR records `JerEncoding` / field NAME;
`--codec` selects emitted codecs: `uper` / `aper` / `both` / `jer` / `xer` /
`exer` / `ber` / `der` / `oer` / `coer` / `cxer`. JER includes SequenceOf/SetOf
decode (array or OBJECT form).

Generated types prefer `std::optional` for `OPTIONAL`, plain values for
`DEFAULT` (decoder fills default when the bit is clear), `std::vector` for
`SEQUENCE OF`, `std::string` for character strings, and `asn1::ObjectIdentifier`
for OIDs.

## Errors

Frontend and semantic analysis append to `Diagnostics` and recover to the next
assignment when possible. Messages always include `file:line:column` when a
token exists. Codec errors use `Result` with a bit or byte offset. No path
reports only `"parse failed"` when a token or type name is known.

## Testing

GoogleTest, one binary per area, registered with CTest.

- Unit tests for lexer, parser, AST, semantic analyzer, constraints,
  BitReader/BitWriter, BER, DER, UPER, APER, OER, COER, XER, CXER, E-XER, JER, JERI.
- Round-trip: `decode(encode(v)) == v` without needing the compiler.
- Checked-in hex vectors from X.691 examples.
- Integration (Phase 11/39): `.asn` → `asn1cxx` → compile → encode → decode for
  Person across PER/OER/COER/BER/DER/JER and for the RRC telecom slice (UPER).
- Optional cross-check against pycrate/asn1c when present on `PATH`.

## Development phases

1. Project skeleton and build system (done)
2. Lexer (done)
3. Basic parser (done)
4. AST and semantic analyzer (done)
5. Constraint system (done)
6. BER (done)
7. DER (done)
8. BitReader / BitWriter and PER primitives (done)
9. UPER (done)
10. APER (done)
11. Code generator (done)
12. Advanced ASN.1 constructs (done)
13. Information Object Classes and table constraints (done)
14. Real-world telecom ASN.1 validation (done)
15. OER (done)
16. COER (done)
17. XER (done)
18. CXER (done)
19. EXTENDED-XER (done)
20. JER (done)
21. JER encoding instructions runtime (done)
22. JER encoding-instruction frontend + codegen (done)
23. INSTANCE OF + XER encoding-instruction frontend/codegen (done)
24. Full EXTENDED-XER codegen instruction matrix (done)
25. REAL encode/decode (done)
26. OER REAL WITH COMPONENTS IEEE binary32/64 (done)
27. EXTERNAL / EMBEDDED PDV / CHARACTER STRING (done)
28. Codecs INTEGER >64-bit (done)
29. Deeper BER for EMBEDDED PDV / CHARACTER STRING (done)
30. BER constructed/indefinite + DER + modern EXTERNAL (done)
31. PER extension additions (done)
32. ABSTRACT-SYNTAX + modern EXTERNAL identification IR (done)
33. JER SequenceOf/SetOf decode codegen (done)
34. Codegen BER / DER / OER / CXER (done)
35. EXER LIST character-encodable element types (done)
36. OER/COER extension additions (done)
37. COER `--codec coer` codegen (done)
38. Telecom UPER codec round-trips (done)
39. Validation — multi-codec Person + telecom emit gates (done)

## Telecom validation (Phase 14 / 38 / 39)

| Gate | Fixture | Checks |
|------|---------|--------|
| RRC Rel-8 | `tests/fixtures/telecom/rrc_8_6_0/` | parse + analyze + emit reports anonymous nesting |
| LPP Rel-14 | `tests/fixtures/telecom/lpp_14_3_0/` | parse + analyze + emit reports anonymous nesting |
| S1AP Rel-14 | `tests/fixtures/telecom/s1ap_14_4_0/` | parse + analyze + emit reports unresolved ProtocolIE fields |
| RRC slice | `tests/fixtures/telecom/rrc_slice/` | UPER/OER emit + UPER round-trip vs known hex |
| Person | `examples/person.asn` | UPER/APER/OER/COER/BER/DER/JER compile round-trips |

CTest: `asn1_telecom_tests`, `asn1_telecom_roundtrip_tests`, `asn1_codegen_roundtrip_tests`.
See `tests/fixtures/telecom/README.md`. Not full 3GPP codec parity.

## Tradeoffs

- Generated orchestration is slightly more code than one interpretive walker
  and keeps large 3GPP modules readable and separately compilable.
- `TypeId` arena is less convenient than pointers and makes recursive types
  and stable IR dumps straightforward.
- Recognizing unsupported constructs costs parser work now and avoids false
  syntax errors on real specifications.
- Internal `BigInt` is more code than Boost and keeps the runtime
  dependency-free.
