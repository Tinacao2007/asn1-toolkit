# ASN.1 Toolchain Architecture

New code lives only in `asn1-toolkit/`. Sibling trees such as `pasn1/`, asn1c,
`Python_asn1tools/`, and `FFASN1dump/` stay untouched reference material.
`pasn1/tools/act` already shows the failure mode to avoid: parser, tag logic,
and BER/UPER generators sit in one compiler (`parser.h` next to
`ber-gen-enc.h` and `uper-gen-enc.h`).

# ASN.1 Toolchain Architecture

New code lives only in `asn1-toolkit/`. Sibling trees such as `pasn1/`, asn1c,
`Python_asn1tools/`, and `FFASN1dump/` stay untouched reference material.
`pasn1/tools/act` already shows the failure mode to avoid: parser, tag logic,
and BER/UPER generators sit in one compiler (`parser.h` next to
`ber-gen-enc.h` and `uper-gen-enc.h`).

This document is the design. **Phase 12 (advanced ASN.1 constructs) is implemented.**
Information Object Classes remain Phase 13.

## Pipeline

```text
ASN.1 source
  → Lexer → Parser → AST
  → Semantic passes (symbol table, tags, constraints)
  → Type IR
  → C++ code generator
  → Generated value types + thin encode/decode
  → Runtime primitives (BER / DER / PER)
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
- **Unsupported syntax is recognized and rejected.** `CLASS`, `OBJECT SET`,
  parameterized types, and table constraints get a diagnostic at the keyword
  with file, line, and column. They are not parsed as if they were `INTEGER`.
  Phase 12 adds `ENUMERATED`, `OBJECT IDENTIFIER`, `RELATIVE-OID`, `SET`,
  `SET OF`, and a typed `REAL` AST/IR stub (REAL codecs deferred).
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
  BitReader/BitWriter, BER, DER, UPER, APER.
- Round-trip: `decode(encode(v)) == v` without needing the compiler.
- Checked-in hex vectors from X.691 examples.
- Integration (Phase 11): `.asn` → `asn1cxx` → compile → encode → decode.
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
12. Advanced ASN.1 constructs
13. Information Object Classes and table constraints
14. Real-world telecom ASN.1 validation

## Tradeoffs

- Generated orchestration is slightly more code than one interpretive walker
  and keeps large 3GPP modules readable and separately compilable.
- `TypeId` arena is less convenient than pointers and makes recursive types
  and stable IR dumps straightforward.
- Recognizing unsupported constructs costs parser work now and avoids false
  syntax errors on real specifications.
- Internal `BigInt` is more code than Boost and keeps the runtime
  dependency-free.
