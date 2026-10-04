/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/semantic/semantic_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising semantic behaviour.
**
** Specification: ITU-T X.680 (and X.681 where covered) — front-end
**                 behaviour under test.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/ir/print.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct AnalyzeResult {
  asn1::SourceFile file;
  asn1::Diagnostics diag;
  asn1::ir::Model model;
  asn1::Analyzer* analyzer = nullptr;
  std::unique_ptr<asn1::Analyzer> owned;
};

AnalyzeResult analyze_string(const std::string& text) {
  AnalyzeResult r;
  r.file = asn1::SourceFile::from_string("test.asn", text);
  asn1::Lexer lexer(r.file, r.diag);
  asn1::Parser parser(lexer, r.diag);
  auto module = parser.parse_module();
  EXPECT_NE(module, nullptr);
  r.owned = std::make_unique<asn1::Analyzer>(r.diag);
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  modules.push_back(std::move(module));
  r.model = r.owned->analyze(std::move(modules));
  return r;
}

const asn1::ir::Type* find_named(const asn1::ir::Model& model, const std::string& name) {
  for (const auto& t : model.arena.types()) {
    if (t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

}  // namespace

TEST(Semantic, PersonAutomaticTagsAndHostBits) {
  auto r = analyze_string(R"(
MyModule DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Person ::= SEQUENCE {
    id      INTEGER (0..65535),
    name    UTF8String,
    age     INTEGER (0..150) OPTIONAL
  }
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();

  const asn1::ir::Type* person = find_named(r.model, "Person");
  ASSERT_NE(person, nullptr);
  ASSERT_EQ(person->kind, asn1::ir::TypeKind::Sequence);
  ASSERT_EQ(person->sequence.root.size(), 3u);

  EXPECT_EQ(person->sequence.root[0].tag.cls, asn1::ir::TagClass::Context);
  EXPECT_EQ(person->sequence.root[0].tag.number, 0u);
  EXPECT_FALSE(person->sequence.root[0].tag.is_explicit);
  EXPECT_EQ(person->sequence.root[1].tag.number, 1u);
  EXPECT_EQ(person->sequence.root[2].tag.number, 2u);
  EXPECT_EQ(person->sequence.root[2].presence, asn1::ir::Presence::Optional);

  const asn1::ir::Type& id_type = r.model.arena.get(person->sequence.root[0].type);
  ASSERT_EQ(id_type.kind, asn1::ir::TypeKind::Integer);
  ASSERT_TRUE(id_type.integer.host_bits.has_value());
  EXPECT_EQ(*id_type.integer.host_bits, 16);
  EXPECT_FALSE(id_type.integer.is_signed);
}

TEST(Semantic, DuplicateAndUndefined) {
  auto r = analyze_string(R"(
M DEFINITIONS ::=
BEGIN
  A ::= INTEGER
  A ::= BOOLEAN
  B ::= Missing
END
)");
  EXPECT_FALSE(r.diag.ok());
  bool saw_dup = false;
  bool saw_undef = false;
  for (const auto& d : r.diag.items()) {
    if (d.message.find("duplicate") != std::string::npos) {
      saw_dup = true;
    }
    if (d.message.find("undefined ASN.1 type 'Missing'") != std::string::npos) {
      saw_undef = true;
    }
  }
  EXPECT_TRUE(saw_dup);
  EXPECT_TRUE(saw_undef);
}

TEST(Semantic, ExplicitApplicationTag) {
  auto r = analyze_string(R"(
M DEFINITIONS ::=
BEGIN
  T ::= [APPLICATION 3] IMPLICIT INTEGER (0..255)
END
)");
  ASSERT_TRUE(r.diag.ok());
  const asn1::ir::Type* t = find_named(r.model, "T");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->tag.cls, asn1::ir::TagClass::Application);
  EXPECT_EQ(t->tag.number, 3u);
  EXPECT_FALSE(t->tag.is_explicit);
  ASSERT_TRUE(t->integer.host_bits.has_value());
  EXPECT_EQ(*t->integer.host_bits, 8);
}

TEST(Semantic, RecursiveSequence) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Node ::= SEQUENCE {
    value INTEGER,
    next  Node OPTIONAL
  }
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();
  const asn1::ir::Type* node = find_named(r.model, "Node");
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->sequence.root.size(), 2u);
  const asn1::ir::TypeId next_id = node->sequence.root[1].type;
  // After rewrite, next should point at Node itself.
  EXPECT_EQ(&r.model.arena.get(next_id), node);
}

TEST(Semantic, CrossModuleImport) {
  asn1::Diagnostics diag;
  auto file_a = asn1::SourceFile::from_string("a.asn", R"(
A DEFINITIONS ::=
BEGIN
EXPORTS Id;
  Id ::= INTEGER (0..255)
END
)");
  auto file_b = asn1::SourceFile::from_string("b.asn", R"(
B DEFINITIONS ::=
BEGIN
IMPORTS Id FROM A;
  Box ::= SEQUENCE { id Id }
END
)");
  asn1::Lexer la(file_a, diag);
  asn1::Parser pa(la, diag);
  auto ma = pa.parse_module();
  asn1::Lexer lb(file_b, diag);
  asn1::Parser pb(lb, diag);
  auto mb = pb.parse_module();
  ASSERT_NE(ma, nullptr);
  ASSERT_NE(mb, nullptr);

  asn1::Analyzer analyzer(diag);
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  modules.push_back(std::move(ma));
  modules.push_back(std::move(mb));
  auto model = analyzer.analyze(std::move(modules));
  ASSERT_TRUE(diag.ok()) << [&] {
    std::string s;
    for (const auto& d : diag.items()) {
      s += diag.format(d) + "\n";
    }
    return s;
  }();

  const asn1::ir::Type* box = find_named(model, "Box");
  ASSERT_NE(box, nullptr);
  ASSERT_EQ(box->sequence.root.size(), 1u);
  const asn1::ir::Type& id_t = model.arena.get(box->sequence.root[0].type);
  // Id may be Integer directly or still a resolved reference rewritten to Integer.
  EXPECT_TRUE(id_t.kind == asn1::ir::TypeKind::Integer ||
              (id_t.kind == asn1::ir::TypeKind::Referenced &&
               id_t.referenced.resolved != asn1::ir::kInvalidType));
}

TEST(Semantic, ExternalEmbeddedPdvCharacterStringExpand) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  A ::= EXTERNAL
  B ::= EMBEDDED PDV
  C ::= CHARACTER STRING
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  const asn1::ir::Type* a = find_named(r.model, "A");
  const asn1::ir::Type* b = find_named(r.model, "B");
  const asn1::ir::Type* c = find_named(r.model, "C");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);

  // EXTERNAL uses modern associated SEQUENCE (identification CHOICE), UNIVERSAL 8.
  EXPECT_EQ(a->kind, asn1::ir::TypeKind::Sequence);
  EXPECT_EQ(a->tag.number, 8u);
  ASSERT_EQ(a->sequence.root.size(), 3u);
  EXPECT_EQ(a->sequence.root[0].name, "identification");
  EXPECT_EQ(a->sequence.root[1].name, "data-value-descriptor");
  EXPECT_EQ(a->sequence.root[1].presence, asn1::ir::Presence::Optional);
  EXPECT_EQ(a->sequence.root[2].name, "data-value");
  const asn1::ir::Type& ident = r.model.arena.get(a->sequence.root[0].type);
  EXPECT_EQ(ident.kind, asn1::ir::TypeKind::Choice);
  ASSERT_GE(ident.choice.alternatives.size(), 6u);
  EXPECT_EQ(ident.choice.alternatives[0].name, "syntaxes");
  EXPECT_EQ(ident.choice.alternatives[5].name, "fixed");

  EXPECT_EQ(b->kind, asn1::ir::TypeKind::Sequence);
  EXPECT_EQ(b->tag.number, 11u);
  ASSERT_EQ(b->sequence.root.size(), 3u);
  EXPECT_EQ(b->sequence.root[0].name, "identification");
  EXPECT_EQ(b->sequence.root[1].name, "data-value-descriptor");
  EXPECT_EQ(b->sequence.root[1].presence, asn1::ir::Presence::Optional);
  EXPECT_EQ(b->sequence.root[2].name, "data-value");

  EXPECT_EQ(c->kind, asn1::ir::TypeKind::Sequence);
  EXPECT_EQ(c->tag.number, 29u);
  ASSERT_EQ(c->sequence.root.size(), 3u);
  EXPECT_EQ(c->sequence.root[1].name, "data-value-descriptor");
  EXPECT_EQ(c->sequence.root[2].name, "string-value");
}

TEST(Semantic, RealWithComponentsIeeeForms) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  A ::= REAL
  B ::= REAL (WITH COMPONENTS {
                mantissa (-16777215..16777215),
                base (2),
                exponent (-149..104)
            })
  C ::= REAL (WITH COMPONENTS {
                mantissa (-9007199254740991..9007199254740991),
                base (2),
                exponent (-1074..971)
            })
  D ::= REAL (WITH COMPONENTS {
                mantissa (-1..1),
                base (10),
                exponent (-1..1)
            })
  E ::= REAL (WITH COMPONENTS { mantissa (1..2) })
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  const asn1::ir::Type* a = find_named(r.model, "A");
  const asn1::ir::Type* b = find_named(r.model, "B");
  const asn1::ir::Type* c = find_named(r.model, "C");
  const asn1::ir::Type* d = find_named(r.model, "D");
  const asn1::ir::Type* e = find_named(r.model, "E");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(d, nullptr);
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(a->real.ieee_form, asn1::ir::RealIeeeForm::Unconstrained);
  EXPECT_EQ(b->real.ieee_form, asn1::ir::RealIeeeForm::Binary32);
  EXPECT_EQ(c->real.ieee_form, asn1::ir::RealIeeeForm::Binary64);
  EXPECT_EQ(d->real.ieee_form, asn1::ir::RealIeeeForm::Unconstrained);
  EXPECT_EQ(e->real.ieee_form, asn1::ir::RealIeeeForm::Unconstrained);
}

TEST(Semantic, Phase12AdvancedTypes) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Color ::= ENUMERATED { red, green(5), blue }
  Id ::= OBJECT IDENTIFIER
  Point ::= SET { x INTEGER, y INTEGER }
  Points ::= SET OF Point
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  const asn1::ir::Type* color = find_named(r.model, "Color");
  ASSERT_NE(color, nullptr);
  EXPECT_EQ(color->kind, asn1::ir::TypeKind::Enumerated);
  ASSERT_EQ(color->enumerated.root.size(), 3u);
  EXPECT_EQ(color->enumerated.root[0].name, "red");
  ASSERT_TRUE(color->enumerated.root[0].value.as_i64.has_value());
  EXPECT_EQ(*color->enumerated.root[0].value.as_i64, 0);
  EXPECT_EQ(*color->enumerated.root[1].value.as_i64, 5);
  EXPECT_EQ(*color->enumerated.root[2].value.as_i64, 6);
  EXPECT_EQ(color->tag.number, 10u);

  const asn1::ir::Type* id = find_named(r.model, "Id");
  ASSERT_NE(id, nullptr);
  EXPECT_EQ(id->kind, asn1::ir::TypeKind::ObjectIdentifier);
  EXPECT_EQ(id->tag.number, 6u);

  const asn1::ir::Type* point = find_named(r.model, "Point");
  ASSERT_NE(point, nullptr);
  EXPECT_EQ(point->kind, asn1::ir::TypeKind::Set);
  EXPECT_EQ(point->tag.number, 17u);
  ASSERT_EQ(point->set.root.size(), 2u);

  const asn1::ir::Type* points = find_named(r.model, "Points");
  ASSERT_NE(points, nullptr);
  EXPECT_EQ(points->kind, asn1::ir::TypeKind::SetOf);
}

TEST(Semantic, Phase13ObjectClassField) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  ERROR ::= CLASS {
    &errorCode INTEGER UNIQUE,
    &Type
  }
  Open ::= ERROR.&Type
  Code ::= ERROR.&errorCode
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  ASSERT_FALSE(r.model.object_classes.empty());
  bool found = false;
  for (const auto& c : r.model.object_classes) {
    if (c.name == "ERROR") {
      found = true;
      ASSERT_EQ(c.fields.size(), 2u);
      EXPECT_EQ(c.fields[0].name, "errorCode");
      EXPECT_EQ(c.fields[0].kind, asn1::ir::ClassFieldKind::FixedTypeValueField);
      EXPECT_EQ(c.fields[1].kind, asn1::ir::ClassFieldKind::TypeField);
    }
  }
  EXPECT_TRUE(found);

  const asn1::ir::Type* open = find_named(r.model, "Open");
  ASSERT_NE(open, nullptr);
  EXPECT_EQ(open->kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_TRUE(open->object_class_field.open_type);

  const asn1::ir::Type* code = find_named(r.model, "Code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_FALSE(code->object_class_field.open_type);
}

TEST(Semantic, SequenceOfSizeAndVersionBrackets) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  List ::= SEQUENCE (SIZE (1..10)) OF INTEGER
  S ::= SEQUENCE {
    a INTEGER,
    ...,
    [[
      b BOOLEAN,
      c UTF8String
    ]]
  }
  Lone ::= SEQUENCE {
    a BOOLEAN,
    ...,
    b BOOLEAN,
    c INTEGER
  }
  Pick ::= CHOICE {
    x BOOLEAN,
    ...,
    y INTEGER
  }
  When ::= UTCTime
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  const asn1::ir::Type* list = find_named(r.model, "List");
  ASSERT_NE(list, nullptr);
  EXPECT_EQ(list->kind, asn1::ir::TypeKind::SequenceOf);
  ASSERT_TRUE(list->sequence_of.size.lower.has_value());
  ASSERT_TRUE(list->sequence_of.size.upper.has_value());
  EXPECT_EQ(*list->sequence_of.size.lower->as_i64, 1);
  EXPECT_EQ(*list->sequence_of.size.upper->as_i64, 10);

  const asn1::ir::Type* s = find_named(r.model, "S");
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->sequence.extensible);
  ASSERT_EQ(s->sequence.root.size(), 1u);
  ASSERT_EQ(s->sequence.extension_groups.size(), 1u);
  EXPECT_EQ(s->sequence.extension_groups[0].size(), 2u);

  const asn1::ir::Type* lone = find_named(r.model, "Lone");
  ASSERT_NE(lone, nullptr);
  ASSERT_EQ(lone->sequence.extension_groups.size(), 2u);
  EXPECT_EQ(lone->sequence.extension_groups[0].size(), 1u);
  EXPECT_EQ(lone->sequence.extension_groups[1].size(), 1u);
  EXPECT_EQ(lone->sequence.extension_groups[0][0].name, "b");
  EXPECT_EQ(lone->sequence.extension_groups[1][0].name, "c");

  const asn1::ir::Type* pick = find_named(r.model, "Pick");
  ASSERT_NE(pick, nullptr);
  EXPECT_TRUE(pick->choice.extensible);
  ASSERT_EQ(pick->choice.alternatives.size(), 1u);
  ASSERT_EQ(pick->choice.extensions.size(), 1u);
  EXPECT_EQ(pick->choice.extensions[0].name, "y");

  const asn1::ir::Type* when = find_named(r.model, "When");
  ASSERT_NE(when, nullptr);
  EXPECT_EQ(when->kind, asn1::ir::TypeKind::String);
  EXPECT_EQ(when->string.kind, asn1::ir::StringKind::UTCTime);
  EXPECT_EQ(when->tag.number, 23u);
}

TEST(Semantic, ParameterizedTypeInstantiation) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Box { T } ::= SEQUENCE {
    item T
  }
  IntBox ::= Box { INTEGER }
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  const asn1::ir::Type* box = find_named(r.model, "Box");
  EXPECT_EQ(box, nullptr);  // template not lowered as concrete type

  const asn1::ir::Type* ib = find_named(r.model, "IntBox");
  ASSERT_NE(ib, nullptr);
  EXPECT_EQ(ib->kind, asn1::ir::TypeKind::Sequence);
  ASSERT_EQ(ib->sequence.root.size(), 1u);
  const asn1::ir::Type& item = r.model.arena.get(ib->sequence.root[0].type);
  EXPECT_EQ(item.kind, asn1::ir::TypeKind::Integer);
}

TEST(Semantic, NestedAnonymousChoiceGetsSyntheticName) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Msg ::= CHOICE {
    c1 CHOICE {
      a INTEGER,
      b BOOLEAN
    },
    ext SEQUENCE {}
  }
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  const asn1::ir::Type* msg = find_named(r.model, "Msg");
  ASSERT_NE(msg, nullptr);
  ASSERT_EQ(msg->kind, asn1::ir::TypeKind::Choice);
  ASSERT_EQ(msg->choice.alternatives.size(), 2u);

  const asn1::ir::Type& c1 = r.model.arena.get(msg->choice.alternatives[0].type);
  EXPECT_EQ(c1.kind, asn1::ir::TypeKind::Choice);
  EXPECT_EQ(c1.name, "Msg-c1");

  const asn1::ir::Type& ext = r.model.arena.get(msg->choice.alternatives[1].type);
  EXPECT_EQ(ext.kind, asn1::ir::TypeKind::Sequence);
  EXPECT_EQ(ext.name, "Msg-ext");
}

TEST(Semantic, ObjectClassFixedFieldsResolveAfterAllModules) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  PROTO-IES ::= CLASS {
    &id INTEGER UNIQUE,
    &criticality ENUMERATED { ignore(0), reject(1) },
    &Value
  }
  IE-Field { PROTO-IES : Set } ::= SEQUENCE {
    id          PROTO-IES.&id          ({Set}),
    criticality PROTO-IES.&criticality ({Set}{@id}),
    value       PROTO-IES.&Value       ({Set}{@id})
  }
  MyIEs PROTO-IES ::= { ... }
  Box ::= IE-Field {{MyIEs}}
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  // Top-level `Box ::= IE-Field {{MyIEs}}` keeps the assignment name on the
  // instantiation body (mangled IE-Field-MyIEs is used when assigned_name is empty).
  const asn1::ir::Type* field = find_named(r.model, "Box");
  ASSERT_NE(field, nullptr) << asn1::ir::to_string(r.model);
  ASSERT_EQ(field->sequence.root.size(), 3u);
  const asn1::ir::Type& id_t = r.model.arena.get(field->sequence.root[0].type);
  ASSERT_EQ(id_t.kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_FALSE(id_t.object_class_field.open_type);
  EXPECT_NE(id_t.object_class_field.fixed_type, asn1::ir::kInvalidType);
  const asn1::ir::Type& crit_t = r.model.arena.get(field->sequence.root[1].type);
  ASSERT_EQ(crit_t.kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_FALSE(crit_t.object_class_field.open_type);
  EXPECT_NE(crit_t.object_class_field.fixed_type, asn1::ir::kInvalidType);
  const asn1::ir::Type& val_t = r.model.arena.get(field->sequence.root[2].type);
  ASSERT_EQ(val_t.kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_TRUE(val_t.object_class_field.open_type);
}

TEST(Semantic, NestedParameterizedObjectSetInstantiation) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  PROTO-IES ::= CLASS {
    &id INTEGER UNIQUE,
    &criticality ENUMERATED { ignore(0), reject(1) },
    &Value
  }
  IE-Field { PROTO-IES : Set } ::= SEQUENCE {
    id          PROTO-IES.&id          ({Set}),
    criticality PROTO-IES.&criticality ({Set}{@id}),
    value       PROTO-IES.&Value       ({Set}{@id})
  }
  IE-Container { PROTO-IES : Set } ::= SEQUENCE (SIZE (0..65535)) OF
    IE-Field {{Set}}
  MyIEs PROTO-IES ::= { ... }
  Pdu ::= SEQUENCE {
    ies IE-Container {{MyIEs}}
  }
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  const asn1::ir::Type* field = find_named(r.model, "IE-Field-MyIEs");
  const asn1::ir::Type* container = find_named(r.model, "IE-Container-MyIEs");
  ASSERT_NE(field, nullptr) << asn1::ir::to_string(r.model);
  ASSERT_NE(container, nullptr) << asn1::ir::to_string(r.model);
  EXPECT_EQ(field->kind, asn1::ir::TypeKind::Sequence);
  ASSERT_EQ(field->sequence.root.size(), 3u);
  EXPECT_EQ(container->kind, asn1::ir::TypeKind::SequenceOf);
}

TEST(Semantic, DefinedSyntaxObject) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  GC ::= CLASS {
    &id INTEGER UNIQUE,
    &Value
  } WITH SYNTAX {
    ID &id VALUE &Value
  }
  g0 GC ::= { ID 0 VALUE INTEGER }
  GCs GC ::= { g0 }
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  ASSERT_FALSE(r.model.objects.empty());
  EXPECT_EQ(r.model.objects[0].name, "g0");
  ASSERT_EQ(r.model.objects[0].settings.size(), 2u);
}

TEST(Semantic, OctetStringContaining) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Inner ::= INTEGER (0..255)
  Wrapped ::= OCTET STRING (CONTAINING Inner)
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  const asn1::ir::Type* wrapped = find_named(r.model, "Wrapped");
  ASSERT_NE(wrapped, nullptr);
  EXPECT_EQ(wrapped->kind, asn1::ir::TypeKind::OctetString);
  ASSERT_NE(wrapped->octet_string.containing, asn1::ir::kInvalidType);
  const asn1::ir::Type& inner = r.model.arena.get(wrapped->octet_string.containing);
  EXPECT_EQ(inner.kind, asn1::ir::TypeKind::Integer);
}

TEST(Semantic, JerEncodingInstructionsOnIr) {
  auto r = analyze_string(R"(
Oss DEFINITIONS JER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  A ::= [ARRAY] SEQUENCE {
    a1 INTEGER,
    a2 [NAME AS "lit"] INTEGER OPTIONAL
  }
  C ::= [UNWRAPPED] CHOICE { c1 BOOLEAN, c2 INTEGER }
  Color ::= [TEXT ALL AS CAPITALIZED] ENUMERATED { red, green, blue }
  Blob ::= OCTET STRING
  ENCODING-CONTROL JER [BASE64] OCTET STRING
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  ASSERT_EQ(r.model.modules.size(), 1u);
  EXPECT_TRUE(r.model.modules[0].jer_instructions);

  const asn1::ir::Type* a = find_named(r.model, "A");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->jer.array);
  ASSERT_EQ(a->sequence.root.size(), 2u);
  EXPECT_EQ(a->sequence.root[1].jer_name_form, asn1::ir::Field::JerNameForm::Literal);
  EXPECT_EQ(a->sequence.root[1].jer_name_literal, "lit");

  const asn1::ir::Type* c = find_named(r.model, "C");
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->jer.unwrapped);

  const asn1::ir::Type* color = find_named(r.model, "Color");
  ASSERT_NE(color, nullptr);
  EXPECT_EQ(color->jer.text_form, asn1::ir::JerEncoding::TextForm::Capitalized);

  const asn1::ir::Type* blob = find_named(r.model, "Blob");
  ASSERT_NE(blob, nullptr);
  EXPECT_TRUE(blob->jer.base64);

  const std::string ir_dump = asn1::ir::to_string(r.model);
  EXPECT_NE(ir_dump.find("jer array"), std::string::npos);
  EXPECT_NE(ir_dump.find("jer unwrapped"), std::string::npos);
  EXPECT_NE(ir_dump.find("jer base64"), std::string::npos);
}

TEST(Semantic, AbstractSyntaxBuiltinAndModernExternal) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  AsType ::= ABSTRACT-SYNTAX.&Type
  AsId ::= ABSTRACT-SYNTAX.&id
  Open ::= INSTANCE OF ABSTRACT-SYNTAX
  my-as ABSTRACT-SYNTAX ::= { INTEGER IDENTIFIED BY {1 2 3} }
  Ext ::= EXTERNAL
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);

  bool have_as = false;
  for (const auto& c : r.model.object_classes) {
    if (c.name == "ABSTRACT-SYNTAX") {
      have_as = true;
      ASSERT_EQ(c.fields.size(), 3u);
      EXPECT_EQ(c.fields[0].name, "id");
      EXPECT_TRUE(c.fields[0].unique);
      EXPECT_EQ(c.fields[1].name, "Type");
      EXPECT_EQ(c.fields[1].kind, asn1::ir::ClassFieldKind::TypeField);
      EXPECT_EQ(c.fields[2].name, "property");
    }
  }
  EXPECT_TRUE(have_as);

  const asn1::ir::Type* as_type = find_named(r.model, "AsType");
  ASSERT_NE(as_type, nullptr);
  EXPECT_EQ(as_type->kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_TRUE(as_type->object_class_field.open_type);

  const asn1::ir::Type* as_id = find_named(r.model, "AsId");
  ASSERT_NE(as_id, nullptr);
  EXPECT_EQ(as_id->kind, asn1::ir::TypeKind::ObjectClassField);
  EXPECT_FALSE(as_id->object_class_field.open_type);

  const asn1::ir::Type* open = find_named(r.model, "Open");
  ASSERT_NE(open, nullptr);
  EXPECT_EQ(open->kind, asn1::ir::TypeKind::InstanceOf);
  EXPECT_EQ(open->instance_of.class_name, "ABSTRACT-SYNTAX");

  ASSERT_FALSE(r.model.objects.empty());
  EXPECT_EQ(r.model.objects[0].name, "my-as");
  EXPECT_EQ(r.model.objects[0].class_name, "ABSTRACT-SYNTAX");

  const asn1::ir::Type* ext = find_named(r.model, "Ext");
  ASSERT_NE(ext, nullptr);
  EXPECT_EQ(ext->kind, asn1::ir::TypeKind::Sequence);
  EXPECT_EQ(ext->tag.number, 8u);
  ASSERT_EQ(ext->sequence.root.size(), 3u);
  EXPECT_EQ(ext->sequence.root[0].name, "identification");
  EXPECT_EQ(ext->sequence.root[2].name, "data-value");
}

TEST(Semantic, InstanceOfAndExerInstructionsOnIr) {
  auto r = analyze_string(R"(
Oss DEFINITIONS XER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  Row ::= SEQUENCE {
    flag [ATTRIBUTE] BOOLEAN,
    data OCTET STRING
  }
  Ok ::= [TEXT] BOOLEAN
  Open ::= INSTANCE OF TYPE-IDENTIFIER
  ENCODING-CONTROL XER [BASE64] OCTET STRING
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ir::to_string(r.model);
  ASSERT_EQ(r.model.modules.size(), 1u);
  EXPECT_TRUE(r.model.modules[0].xer_instructions);

  const asn1::ir::Type* open = find_named(r.model, "Open");
  ASSERT_NE(open, nullptr);
  EXPECT_EQ(open->kind, asn1::ir::TypeKind::InstanceOf);
  EXPECT_EQ(open->instance_of.class_name, "TYPE-IDENTIFIER");
  EXPECT_EQ(open->tag.number, 8u);

  const asn1::ir::Type* row = find_named(r.model, "Row");
  ASSERT_NE(row, nullptr);
  ASSERT_EQ(row->sequence.root.size(), 2u);
  EXPECT_TRUE(row->sequence.root[0].exer.attribute);

  const asn1::ir::Type* ok = find_named(r.model, "Ok");
  ASSERT_NE(ok, nullptr);
  EXPECT_TRUE(ok->exer.text);

  const asn1::ir::Type* data_ty = &r.model.arena.get(row->sequence.root[1].type);
  while (data_ty->kind == asn1::ir::TypeKind::Referenced &&
         data_ty->referenced.resolved != asn1::ir::kInvalidType) {
    data_ty = &r.model.arena.get(data_ty->referenced.resolved);
  }
  ASSERT_EQ(data_ty->kind, asn1::ir::TypeKind::OctetString);
  EXPECT_TRUE(data_ty->exer.base64);

  const std::string ir_dump = asn1::ir::to_string(r.model);
  EXPECT_NE(ir_dump.find("InstanceOf"), std::string::npos);
  EXPECT_NE(ir_dump.find("exerAttribute"), std::string::npos);
  EXPECT_NE(ir_dump.find("exer text"), std::string::npos);
  EXPECT_TRUE(data_ty->exer.base64);
}

TEST(Semantic, TypeAliasBeforeTargetKeepsReferenced) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Alias ::= Target
  Target ::= SEQUENCE { a BOOLEAN }
END
)");
  ASSERT_TRUE(r.diag.ok()) << (r.diag.items().empty() ? "" : r.diag.items().front().message);
  const asn1::ir::Type* alias = find_named(r.model, "Alias");
  const asn1::ir::Type* target = find_named(r.model, "Target");
  ASSERT_NE(alias, nullptr);
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(alias->kind, asn1::ir::TypeKind::Referenced);
  EXPECT_EQ(alias->referenced.name, "Target");
  ASSERT_NE(alias->referenced.resolved, asn1::ir::kInvalidType);
  const asn1::ir::Type& resolved = r.model.arena.get(alias->referenced.resolved);
  EXPECT_EQ(resolved.kind, asn1::ir::TypeKind::Sequence);
  EXPECT_EQ(resolved.name, "Target");
  EXPECT_EQ(target->kind, asn1::ir::TypeKind::Sequence);
}

TEST(Semantic, TableConstraintSpecialization) {
  auto r = analyze_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  PROTO-IES ::= CLASS {
    &id INTEGER UNIQUE,
    &criticality ENUMERATED { ignore(0), reject(1) },
    &Value
  }
  IE-Field { PROTO-IES : Set } ::= SEQUENCE {
    id          PROTO-IES.&id          ({Set}),
    criticality PROTO-IES.&criticality ({Set}{@id}),
    value       PROTO-IES.&Value       ({Set}{@id})
  }
  item1 PROTO-IES ::= { &id 101, &criticality ignore, &Value INTEGER }
  item2 PROTO-IES ::= { &id 102, &criticality reject, &Value BOOLEAN }
  MyIEs PROTO-IES ::= { item1 | item2 }
  Box ::= IE-Field {{MyIEs}}
END
)");
  ASSERT_TRUE(r.diag.ok()) << (r.diag.items().empty() ? "" : r.diag.items().front().message);
  const asn1::ir::Type* field = find_named(r.model, "Box");
  ASSERT_NE(field, nullptr);
  ASSERT_EQ(field->sequence.root.size(), 3u);
  const auto& val_field = field->sequence.root[2];
  ASSERT_TRUE(val_field.table_constraint.has_value());
  EXPECT_EQ(val_field.table_constraint->object_set_name, "MyIEs");
  EXPECT_EQ(val_field.table_constraint->governor_field_name, "id");
  ASSERT_EQ(val_field.table_constraint->entries.size(), 2u);
  EXPECT_EQ(val_field.table_constraint->entries[0].id_value.as_i64.value_or(0), 101);
  EXPECT_EQ(val_field.table_constraint->entries[1].id_value.as_i64.value_or(0), 102);
}


