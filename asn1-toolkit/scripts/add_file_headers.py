#!/usr/bin/env python3
"""Prepend or refresh PROCODEC file header blocks on asn1-toolkit sources."""

from __future__ import annotations

import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MARKER = "PROCODEC All rights reserved"
HEADER_END = "***************************************************************************/"
DATE = "2026-10-03"
AUTHOR = "tina.cao"
VERSION = "0.1"
WRAP = 72

# File-local description paragraphs (this translation unit only).
DESCRIPTIONS: dict[str, list[str]] = {
    "src/ast/print.cpp": [
        "Pretty-prints parse-time AST (modules, type/value assignments, constraints,",
        "information object syntax) to a character stream for --dump-ast and tests.",
        "Reflects ASN.1 source structure only; does not encode or decode protocol data.",
    ],
    "include/asn1/ast/print.hpp": [
        "Declares AST pretty-printer entry points used by asn1cxx and unit tests.",
    ],
    "src/ir/print.cpp": [
        "Pretty-prints lowered type IR (arena types, constraints, tags) for --dump-ir.",
    ],
    "include/asn1/ir/print.hpp": [
        "Declares type IR debug printer API.",
    ],
    "src/codegen/emit.cpp": [
        "Generates C++ headers (value types plus inline encode/decode) from semantic IR,",
        "calling shared runtime codec namespaces selected by EmitOptions.",
    ],
    "include/asn1/codegen/emit.hpp": [
        "Public API for C++ code generation (EmitOptions, CodecKind, CppGenerator).",
    ],
    "tools/asn1cxx/main.cpp": [
        "Command-line driver: lex/parse ASN.1 modules, run semantic analysis, optional",
        "AST/IR dumps, and emit generated C++ under --emit-dir.",
    ],
}

# Optional per-file specification lines (standards / specs this file implements or tests).
SPECS: dict[str, list[str]] = {}


def description_for(rel_posix: str) -> list[str]:
    if rel_posix in DESCRIPTIONS:
        return DESCRIPTIONS[rel_posix]
    # One-line fallbacks from legacy roles
    one = _fallback_one_line(rel_posix)
    return [one]


def _fallback_one_line(rel_posix: str) -> str:
    roles = {
        "include/asn1/common/result.hpp": "Result<T> and Error for non-throwing compiler/codec control flow.",
        "include/asn1/common/span.hpp": "Span<T> non-owning buffer view (C++17 replacement for std::span).",
        "include/asn1/common/float_conv.hpp": "Helpers converting ASN.1 REAL (binary base-2) and IEEE-754 double.",
        "include/asn1/support/diagnostics.hpp": "Collects lexer/parser/semantic errors with SourceLocation.",
        "src/support/diagnostics.cpp": "Diagnostic message storage and formatting.",
        "include/asn1/support/source_file.hpp": "Read-only source text buffer indexed by lexer tokens.",
        "src/support/source_file.cpp": "SourceFile load and line/column offset mapping.",
        "include/asn1/support/source_location.hpp": "File/line/column tuple attached to AST and diagnostics.",
        "include/asn1/frontend/token.hpp": "TokenKind enumeration and Token span into SourceFile.",
        "src/frontend/token.cpp": "Token spelling tables and keyword classification.",
        "include/asn1/frontend/lexer.hpp": "Lexer interface: yields ASN.1 tokens from a SourceFile.",
        "src/frontend/lexer.cpp": "Hand-written lexer for ASN.1 lexical syntax (X.680 clause 12).",
        "include/asn1/frontend/parser.hpp": "Parser interface building ast::Module trees from tokens.",
        "src/frontend/parser.cpp": "Recursive-descent parser for ASN.1 modules, types, values, constraints.",
        "src/frontend/parser_ioc.cpp": "Parser clauses for object classes, object sets, and table constraints.",
        "include/asn1/ast/node.hpp": "Common AST node bases, source locations, and forward declarations.",
        "include/asn1/ast/module.hpp": "AST for MODULE headers, imports/exports, and top-level assignments.",
        "include/asn1/ast/type.hpp": "AST nodes for ASN.1 type constructors and sequence/choice components.",
        "include/asn1/ast/encoding.hpp": "AST for encoding instructions (XER/EXTENDED-XER and related).",
        "include/asn1/ast/ioc.hpp": "AST for information object classes, objects, sets, and field specs.",
        "include/asn1/ast/visitor.hpp": "Visitor interface over concrete AST node types.",
        "include/asn1/ir/type.hpp": "Semantic type IR: arena, TypeKind, constraints, tagging metadata.",
        "src/ir/type.cpp": "IR type graph helpers and builtin lowering hooks.",
        "include/asn1/semantic/symbol_table.hpp": "Cross-module symbol resolution for types and values.",
        "src/semantic/symbol_table.cpp": "Symbol table insert, lookup, and import wiring.",
        "include/asn1/semantic/analyzer.hpp": "Semantic passes: resolve names, tags, constraints, emit IR.",
        "src/semantic/analyzer.cpp": "Analyzer implementation lowering AST to ir::Model.",
        "include/asn1/constraints/normalize.hpp": "Normalizes ASN.1 constraint notation for IR and codecs.",
        "src/constraints/normalize.cpp": "Constraint normalization algorithms.",
        "include/asn1/runtime/limits.hpp": "Hard limits guarding decode depth and string sizes.",
        "include/asn1/runtime/bit_string.hpp": "BitStringValue storage (bits + bit length) for codecs.",
        "include/asn1/runtime/bigint.hpp": "BigInteger sign-and-limbs INTEGER beyond 64 bits.",
        "src/runtime/bigint.cpp": "BigInteger parse, compare, and encode/decode decimal text.",
        "include/asn1/runtime/bit_io.hpp": "BitReader/BitWriter used by PER-family codecs.",
        "src/runtime/bit_io.cpp": "Bit-aligned I/O primitives.",
        "include/asn1/runtime/byte_io.hpp": "ByteReader/ByteWriter for TLV and octet-aligned rules.",
        "src/runtime/byte_io.cpp": "Byte-oriented I/O helpers.",
        "include/asn1/runtime/ber/tlv.hpp": "BER TLV tag/length/value helpers and type tags.",
        "src/runtime/ber/tlv.cpp": "BER TLV parsing and serialization.",
        "include/asn1/runtime/ber/codec.hpp": "BER encode/decode API for ASN.1 values.",
        "src/runtime/ber/codec.cpp": "BER codec implementation (constructed/ primitive types).",
        "include/asn1/runtime/der/tlv.hpp": "DER canonical length and ordering checks on BER TLV.",
        "src/runtime/der/tlv.cpp": "DER TLV canonicalization utilities.",
        "include/asn1/runtime/der/codec.hpp": "DER encode/decode API (canonical subset of BER).",
        "src/runtime/der/codec.cpp": "DER codec implementation.",
        "include/asn1/runtime/per/primitives.hpp": "PER constrained types, fragments, alignment hooks.",
        "src/runtime/per/primitives.cpp": "PER primitive encode/decode (integers, strings, etc.).",
        "include/asn1/runtime/per/codec.hpp": "PER codec surface (UPER/APER via alignment mode).",
        "src/runtime/per/codec.cpp": "PER structured-type orchestration.",
        "include/asn1/runtime/uper.hpp": "Thin include forwarding to unaligned PER (UPER) entry points.",
        "include/asn1/runtime/aper.hpp": "Thin include forwarding to aligned PER (APER) entry points.",
        "include/asn1/runtime/oer/codec.hpp": "OER encode/decode API.",
        "src/runtime/oer/codec.cpp": "OER codec implementation.",
        "include/asn1/runtime/coer/codec.hpp": "COER (canonical OER) encode/decode API.",
        "src/runtime/coer/codec.cpp": "COER codec implementation.",
        "include/asn1/runtime/xer/xml.hpp": "Lightweight XML tree for XER-family codecs.",
        "src/runtime/xer/xml.cpp": "XML parse/serialize helpers for XER elements.",
        "include/asn1/runtime/xer/codec.hpp": "Basic XER encode/decode API.",
        "src/runtime/xer/codec.cpp": "XER mapping between ASN.1 values and XML elements.",
        "include/asn1/runtime/cxer/codec.hpp": "Canonical XER (CXER) encode/decode API.",
        "src/runtime/cxer/codec.cpp": "CXER canonicalization rules on top of XER.",
        "include/asn1/runtime/exer/codec.hpp": "Extended XER (E-XER) encode/decode API.",
        "src/runtime/exer/codec.cpp": "E-XER encoding instructions and special cases.",
        "include/asn1/runtime/jer/json.hpp": "JSON value AST for JER/JERI codecs.",
        "src/runtime/jer/json.cpp": "JSON text lexer/parser for JER.",
        "include/asn1/runtime/jer/codec.hpp": "JSON Encoding Rules (JER) codec API.",
        "src/runtime/jer/codec.cpp": "JER mapping between ASN.1 values and JSON.",
        "include/asn1/runtime/jeri/codec.hpp": "Canonical JER (JERI) codec API.",
        "src/runtime/jeri/codec.cpp": "JERI canonical JSON encoding on top of JER.",
        "tests/telecom/telecom_test.cpp": "Loads large 3GPP-oriented ASN.1 fixtures; parse and analyze smoke tests.",
        "tests/telecom/rrc_slice_roundtrip_test.cpp": "Codegen and compile checks on an RRC ASN.1 slice fixture.",
    }
    if rel_posix in roles:
        return roles[rel_posix]
    name = Path(rel_posix).name
    if rel_posix.startswith("tests/"):
        stem = name.replace("_test.cpp", "").replace(".cpp", "")
        return f"GoogleTest exercising {stem.replace('_', ' ')} behaviour."
    if rel_posix.startswith("include/"):
        tail = rel_posix.replace("include/asn1/", "")
        return f"Public interface declarations for {tail.replace('/', ' / ')}."
    if rel_posix.startswith("src/"):
        tail = rel_posix.replace("src/", "").replace(".cpp", "")
        return f"Implementation source for {tail.replace('/', ' / ')}."
    return "Source file in the asn1-toolkit tree."


def spec_for(rel_posix: str) -> list[str]:
    if rel_posix in SPECS:
        return SPECS[rel_posix]
    p = rel_posix.replace("\\", "/")
    if p.startswith("tests/telecom/"):
        return [
            "3GPP TS 36.331 (NR/LTE RRC), TS 37.355 (LPP), TS 36.413 (S1AP) ASN.1 modules (fixtures);",
            "compiler front-end only in this file (ITU-T X.680 module syntax).",
        ]
    if "/runtime/ber/" in p or p == "tests/ber/ber_test.cpp":
        return ["ITU-T X.690 — ASN.1 encoding rules: Basic Encoding Rules (BER)."]
    if "/runtime/der/" in p or p == "tests/der/der_test.cpp":
        return ["ITU-T X.690 — Distinguished Encoding Rules (DER), canonical BER subset."]
    if "/runtime/per/" in p or "/runtime/uper.hpp" in p or "/runtime/aper.hpp" in p:
        return ["ITU-T X.691 — ASN.1 encoding rules: Packed Encoding Rules (PER); UPER/APER variants."]
    if p in ("tests/uper/uper_test.cpp", "tests/aper/aper_test.cpp", "tests/per/per_primitives_test.cpp"):
        return ["ITU-T X.691 — Packed Encoding Rules (PER); test vectors for UPER/APER/primitives."]
    if "/runtime/oer/" in p or p == "tests/oer/oer_test.cpp":
        return ["ITU-T X.696 — ASN.1 encoding rules: Octet Encoding Rules (OER)."]
    if "/runtime/coer/" in p or p == "tests/coer/coer_test.cpp":
        return ["ITU-T X.696 — Canonical OER (COER) on octet encoding rules."]
    if "/runtime/xer/" in p or "/runtime/cxer/" in p or "/runtime/exer/" in p:
        if "cxer" in p or p == "tests/cxer/cxer_test.cpp":
            return ["ITU-T X.693 — Canonical XML Encoding Rules (CXER)."]
        if "exer" in p or p == "tests/exer/exer_test.cpp":
            return ["ITU-T X.693 — Extended XER (E-XER) and encoding instructions."]
        if p == "tests/xer/xer_test.cpp" or "/xer/codec" in p:
            return ["ITU-T X.693 — Basic XML Encoding Rules (XER)."]
        return ["ITU-T X.693 — XML Encoding Rules (support types in this file)."]
    if "/runtime/jer/" in p or "/runtime/jeri/" in p:
        if "jeri" in p or p == "tests/jeri/jeri_test.cpp":
            return ["ITU-T X.697 — Canonical JSON Encoding Rules (JERI)."]
        if p == "tests/jer/jer_test.cpp" or "/jer/codec" in p:
            return ["ITU-T X.697 — JSON Encoding Rules (JER)."]
        return ["ITU-T X.697 — JSON-related helpers for JER/JERI codecs."]
    if p.startswith("src/frontend/") or p.startswith("include/asn1/frontend/"):
        return ["ITU-T X.680 — ASN.1 abstract syntax (lexical and syntactic notation)."]
    if p.startswith("src/ast/") or p.startswith("include/asn1/ast/"):
        if "encoding.hpp" in p:
            return [
                "ITU-T X.680 — abstract syntax;",
                "ITU-T X.693 — encoding instruction syntax (XER / EXTENDED-XER references).",
            ]
        if "ioc.hpp" in p or "parser_ioc" in p:
            return [
                "ITU-T X.680 — abstract syntax;",
                "ITU-T X.681 — Information object specification (classes, objects, object sets).",
            ]
        return ["ITU-T X.680 — ASN.1 abstract syntax (parse tree produced/consumed here)."]
    if "constraints" in p:
        return ["ITU-T X.680 — subtyping and constraint notation (X.682 constraint application)."]
    if p.startswith("src/semantic/") or p.startswith("include/asn1/semantic/"):
        return [
            "ITU-T X.680 — name and module semantics;",
            "ITU-T X.681 / X.683 — object and parameterization semantics where implemented.",
        ]
    if p.startswith("src/ir/") or p.startswith("include/asn1/ir/"):
        return ["Internal type IR (lowering target for X.680/X.681 constructs; not a wire standard)."]
    if p.startswith("src/codegen/") or p.startswith("include/asn1/codegen/"):
        return ["Generated code layout follows ITU type rules; emission logic is toolchain-internal."]
    if p.startswith("tools/asn1cxx/"):
        return ["Invokes X.680/X.681 front end; optional codecs per X.690/X.691/X.696/X.693/X.697."]
    if p.startswith("include/asn1/common/") or p.startswith("include/asn1/support/"):
        return ["No external protocol; C++ infrastructure shared by compiler and runtime."]
    if p.startswith("src/support/"):
        return ["No external protocol; compiler infrastructure."]
    if "/runtime/bigint" in p or p == "tests/runtime/bigint_codec_test.cpp":
        return ["ITU-T X.680 INTEGER type; unconstrained encoding as used by BER/PER/OER callers."]
    if "/runtime/bit_io" in p or "/runtime/byte_io" in p or "bit_string.hpp" in p or "limits.hpp" in p:
        return ["Internal I/O and safety limits serving ITU encoding implementations in this codebase."]
    if p.startswith("tests/codegen/") or p == "tests/constraints/constraints_test.cpp":
        return ["Validates toolchain output against internal IR/codegen contracts (X.680 type model)."]
    if p.startswith("tests/lexer/") or p.startswith("tests/parser/") or p.startswith("tests/semantic/"):
        return ["ITU-T X.680 (and X.681 where covered) — front-end behaviour under test."]
    if p.startswith("tests/common/"):
        return ["No external protocol; tests for shared C++ helpers."]
    return ["See asn1-toolkit/docs/ARCHITECTURE.md for mapping to ITU-T encoding rules."]


def wrap_desc_paragraph(paragraph: str) -> list[str]:
    wrapped = textwrap.wrap(
        paragraph.strip(),
        width=WRAP,
        break_long_words=False,
        break_on_hyphens=False,
    )
    return wrapped or [paragraph.strip()]


def format_body_lines(paragraphs: list[str]) -> list[str]:
    out: list[str] = []
    for para in paragraphs:
        for line in wrap_desc_paragraph(para):
            out.append(f"**   {line}")
    return out


def make_header(rel_posix: str) -> str:
    filename = f"asn1-toolkit/{rel_posix}"
    desc_lines = format_body_lines(description_for(rel_posix))
    spec_lines = spec_for(rel_posix)
    spec_formatted: list[str] = []
    for i, s in enumerate(spec_lines):
        for j, chunk in enumerate(textwrap.wrap(s, width=WRAP - 17)):
            if i == 0 and j == 0:
                spec_formatted.append(f"** Specification: {chunk}")
            else:
                spec_formatted.append(f"**                 {chunk}")

    lines = [
        "/***************************************************************************",
        "** Copyright (C)  2026-2031 PROCODEC All rights reserved.",
        "** -------------------------------------------------------------------------",
        "** This document contains proprietary information belonging to PROCODEC.",
        "** Passing on and copying of this document, use and communication of its",
        "** contents is not permitted without prior written authorisation.",
        "** -------------------------------------------------------------------------",
        "** Revision Information :",
        f"**   $Filename: {filename}",
        f"**   $Version: {VERSION}",
        f"**   $Date:   {DATE}",
        f"**   $Author: {AUTHOR}",
        "***************************************************************************",
        "**  File Description:",
        "**",
        *desc_lines,
        "**",
        *spec_formatted,
        "** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md",
        "**                 asn1-toolkit/README.md",
        "***************************************************************************/",
        "",
    ]
    return "\n".join(lines)


def strip_existing_header(text: str) -> str:
    if not text.startswith("/********"):
        return text
    end = text.find(HEADER_END)
    if end == -1:
        return text
    return text[end + len(HEADER_END) :].lstrip("\n")


def process_file(path: Path, force: bool = True) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    text = path.read_text(encoding="utf-8")
    if MARKER in text and not force:
        return False
    body = strip_existing_header(text) if MARKER in text else text.lstrip("\n")
    header = make_header(rel)
    path.write_text(header + body, encoding="utf-8", newline="\n")
    return True


def main() -> None:
    count = 0
    for path in sorted(ROOT.rglob("*")):
        if path.suffix not in (".cpp", ".hpp", ".h"):
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel.startswith("build") or "/build/" in rel:
            continue
        if process_file(path):
            count += 1
            print(f"header  {rel}")
    print(f"Updated headers on {count} files")


if __name__ == "__main__":
    main()
