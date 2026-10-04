/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/parser/parser_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising parser behaviour.
**
** Specification: ITU-T X.680 (and X.681 where covered) — front-end
**                 behaviour under test.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/ast/encoding.hpp>
#include <asn1/ast/module.hpp>
#include <asn1/ast/ioc.hpp>
#include <asn1/ast/print.hpp>
#include <asn1/ast/type.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <memory>
#include <string>

#include <gtest/gtest.h>

namespace {

struct ParseResult {
  asn1::SourceFile file;
  asn1::Diagnostics diag;
  std::unique_ptr<asn1::ast::Module> module;
};

ParseResult parse_string(const std::string& text, const std::string& path = "test.asn") {
  ParseResult r;
  r.file = asn1::SourceFile::from_string(path, text);
  asn1::Lexer lexer(r.file, r.diag);
  asn1::Parser parser(lexer, r.diag);
  r.module = parser.parse_module();
  return r;
}

}  // namespace

TEST(Parser, SmokeIntegerConstraint) {
  auto r = parse_string(R"(
Example DEFINITIONS ::=
BEGIN
  Id ::= INTEGER (0..255)
END
)");
  ASSERT_TRUE(r.diag.ok()) << asn1::ast::to_string(*r.module);
  ASSERT_NE(r.module, nullptr);
  EXPECT_EQ(r.module->name(), "Example");
  ASSERT_EQ(r.module->assignments().size(), 1u);

  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  ASSERT_NE(ta, nullptr);
  EXPECT_EQ(ta->name(), "Id");
  auto* integer = dynamic_cast<const asn1::ast::IntegerType*>(&ta->type());
  ASSERT_NE(integer, nullptr);
  auto* range = dynamic_cast<const asn1::ast::ValueRangeConstraint*>(integer->constraint());
  ASSERT_NE(range, nullptr);
  auto* lo = dynamic_cast<const asn1::ast::IntegerValue*>(range->lower());
  auto* hi = dynamic_cast<const asn1::ast::IntegerValue*>(range->upper());
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->text(), "0");
  EXPECT_EQ(hi->text(), "255");
}

TEST(Parser, PersonSequenceOptionalAndTags) {
  auto r = parse_string(R"(
MyModule DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Person ::= SEQUENCE {
    id      INTEGER (0..65535),
    name    UTF8String,
    age     INTEGER (0..150) OPTIONAL,
    ...
  }
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d);
      s += '\n';
    }
    return s;
  }();
  ASSERT_NE(r.module, nullptr);
  EXPECT_EQ(r.module->tag_default(), asn1::ast::TagDefault::Automatic);

  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  ASSERT_NE(ta, nullptr);
  auto* seq = dynamic_cast<const asn1::ast::SequenceType*>(&ta->type());
  ASSERT_NE(seq, nullptr);
  ASSERT_EQ(seq->items().size(), 4u);

  auto* id = dynamic_cast<const asn1::ast::Component*>(seq->items()[0].get());
  auto* name = dynamic_cast<const asn1::ast::Component*>(seq->items()[1].get());
  auto* age = dynamic_cast<const asn1::ast::Component*>(seq->items()[2].get());
  auto* ext = dynamic_cast<const asn1::ast::ExtensionMarker*>(seq->items()[3].get());
  ASSERT_NE(id, nullptr);
  ASSERT_NE(name, nullptr);
  ASSERT_NE(age, nullptr);
  ASSERT_NE(ext, nullptr);
  EXPECT_EQ(id->name(), "id");
  EXPECT_EQ(name->name(), "name");
  EXPECT_EQ(age->presence(), asn1::ast::Presence::Optional);
  EXPECT_NE(dynamic_cast<const asn1::ast::StringType*>(&name->type()), nullptr);
}

TEST(Parser, ChoiceBitOctetSequenceOfAndSize) {
  auto r = parse_string(R"(
M DEFINITIONS ::=
BEGIN
  Flag ::= BOOLEAN
  Bits ::= BIT STRING { a(0), b(1) }
  Data ::= OCTET STRING (SIZE(1..128))
  List ::= SEQUENCE OF INTEGER
  Alt ::= CHOICE {
    i INTEGER,
    o OCTET STRING
  }
  Tagged ::= [APPLICATION 3] IMPLICIT INTEGER
END
)");
  ASSERT_TRUE(r.diag.ok());
  ASSERT_EQ(r.module->assignments().size(), 6u);

  auto* bits_a =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[1].get());
  auto* bits = dynamic_cast<const asn1::ast::BitStringType*>(&bits_a->type());
  ASSERT_NE(bits, nullptr);
  ASSERT_EQ(bits->named_bits().size(), 2u);
  EXPECT_EQ(bits->named_bits()[0].name, "a");

  auto* data_a =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[2].get());
  auto* data = dynamic_cast<const asn1::ast::OctetStringType*>(&data_a->type());
  ASSERT_NE(data, nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::SizeConstraint*>(data->constraint()), nullptr);

  auto* list_a =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[3].get());
  EXPECT_NE(dynamic_cast<const asn1::ast::SequenceOfType*>(&list_a->type()), nullptr);

  auto* alt_a =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[4].get());
  auto* choice = dynamic_cast<const asn1::ast::ChoiceType*>(&alt_a->type());
  ASSERT_NE(choice, nullptr);
  EXPECT_EQ(choice->alternatives().size(), 2u);

  auto* tagged_a =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[5].get());
  ASSERT_TRUE(tagged_a->type().tag().has_value());
  EXPECT_EQ(tagged_a->type().tag()->cls, asn1::ast::TagClass::Application);
  EXPECT_EQ(tagged_a->type().tag()->number_text, "3");
  EXPECT_EQ(*tagged_a->type().tag()->mode, asn1::ast::TagMode::Implicit);
}

TEST(Parser, ValueAssignmentAndDefault) {
  auto r = parse_string(R"(
M DEFINITIONS ::=
BEGIN
  Color ::= INTEGER { red(0), green(1) }
  defaultColor Color ::= red
  S ::= SEQUENCE {
    c Color DEFAULT red,
    n INTEGER
  }
END
)");
  ASSERT_TRUE(r.diag.ok());
  ASSERT_EQ(r.module->assignments().size(), 3u);
  auto* va = dynamic_cast<const asn1::ast::ValueAssignment*>(r.module->assignments()[1].get());
  ASSERT_NE(va, nullptr);
  EXPECT_EQ(va->name(), "defaultColor");
  EXPECT_NE(dynamic_cast<const asn1::ast::ValueReference*>(&va->value()), nullptr);

  auto* sa = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[2].get());
  auto* seq = dynamic_cast<const asn1::ast::SequenceType*>(&sa->type());
  auto* c = dynamic_cast<const asn1::ast::Component*>(seq->items()[0].get());
  EXPECT_EQ(c->presence(), asn1::ast::Presence::Default);
}

TEST(Parser, ImportsExportsAndExtensibility) {
  auto r = parse_string(R"(
M DEFINITIONS EXPLICIT TAGS EXTENSIBILITY IMPLIED ::=
BEGIN
EXPORTS Person;
IMPORTS Id FROM Other;
  Person ::= SEQUENCE { id Id }
END
)");
  ASSERT_TRUE(r.diag.ok());
  EXPECT_TRUE(r.module->extensibility_implied());
  EXPECT_EQ(r.module->tag_default(), asn1::ast::TagDefault::Explicit);
  ASSERT_EQ(r.module->exports().size(), 1u);
  EXPECT_EQ(r.module->exports()[0], "Person");
  ASSERT_EQ(r.module->imports().size(), 1u);
  EXPECT_EQ(r.module->imports()[0].module, "Other");
  EXPECT_EQ(r.module->imports()[0].symbols[0].name, "Id");
}

TEST(Parser, InstanceOfTypeIdentifier) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Open ::= INSTANCE OF TYPE-IDENTIFIER
  Other ::= INSTANCE OF MyClass
END
)",
                        "inst.asn");
  ASSERT_TRUE(r.diag.ok());
  ASSERT_NE(r.module, nullptr);
  const asn1::ast::TypeAssignment* open = nullptr;
  for (const auto& asn : r.module->assignments()) {
    if (const auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(asn.get())) {
      if (ta->name() == "Open") {
        open = ta;
      }
    }
  }
  ASSERT_NE(open, nullptr);
  const auto* inst = dynamic_cast<const asn1::ast::InstanceOfType*>(&open->type());
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->class_name(), "TYPE-IDENTIFIER");

  const std::string ast_dump = asn1::ast::to_string(*r.module);
  EXPECT_NE(ast_dump.find("InstanceOfType"), std::string::npos);
  EXPECT_NE(ast_dump.find("TYPE-IDENTIFIER"), std::string::npos);
}

TEST(Parser, AbstractSyntaxUsefulObjectClass) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  AsType ::= ABSTRACT-SYNTAX.&Type
  AsId ::= ABSTRACT-SYNTAX.&id
  Open ::= INSTANCE OF ABSTRACT-SYNTAX
  my-as ABSTRACT-SYNTAX ::= { INTEGER IDENTIFIED BY {1 2 3} }
  AsSet ABSTRACT-SYNTAX ::= { my-as }
END
)",
                        "abs.asn");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();
  ASSERT_NE(r.module, nullptr);

  auto* as_type = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* ocf = dynamic_cast<const asn1::ast::ObjectClassFieldType*>(&as_type->type());
  ASSERT_NE(ocf, nullptr);
  EXPECT_EQ(ocf->class_name(), "ABSTRACT-SYNTAX");
  EXPECT_EQ(ocf->field_name(), "Type");

  auto* open = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[2].get());
  auto* inst = dynamic_cast<const asn1::ast::InstanceOfType*>(&open->type());
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->class_name(), "ABSTRACT-SYNTAX");

  auto* obj = dynamic_cast<const asn1::ast::ObjectAssignment*>(r.module->assignments()[3].get());
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->class_name(), "ABSTRACT-SYNTAX");
  ASSERT_EQ(obj->defn().settings().size(), 2u);
  EXPECT_EQ(obj->defn().settings()[0].field_name, "Type");
  EXPECT_EQ(obj->defn().settings()[1].field_name, "id");
}

TEST(Parser, XerInstructionsAndEncodingControl) {
  auto r = parse_string(R"asn(
Oss DEFINITIONS XER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  Row ::= SEQUENCE {
    flag [ATTRIBUTE] BOOLEAN,
    data OCTET STRING
  }
  Ok ::= [TEXT] BOOLEAN
  ENCODING-CONTROL XER [BASE64] OCTET STRING
END
)asn");
  ASSERT_TRUE(r.diag.ok());
  ASSERT_NE(r.module, nullptr);
  EXPECT_TRUE(r.module->xer_instructions());
  ASSERT_EQ(r.module->xer_encoding_control().size(), 1u);
  EXPECT_EQ(r.module->xer_encoding_control()[0].family,
            asn1::ast::EncodingControlFamily::Xer);
  EXPECT_EQ(r.module->xer_encoding_control()[0].target,
            asn1::ast::EncodingControlTarget::OctetString);
  EXPECT_EQ(r.module->xer_encoding_control()[0].instruction.kind,
            asn1::ast::EncodingInstructionKind::Base64);

  const std::string ast_dump = asn1::ast::to_string(*r.module);
  EXPECT_NE(ast_dump.find("xerInstructions=true"), std::string::npos);
  EXPECT_NE(ast_dump.find("encodingInstructions ATTRIBUTE"), std::string::npos);
}

TEST(Parser, Phase13ObjectClassAndFieldType) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  ERROR ::= CLASS {
    &errorCode INTEGER UNIQUE,
    &Type
  } WITH SYNTAX {&Type IDENTIFIED BY &errorCode}

  err1 ERROR ::= { &errorCode 1, &Type INTEGER }

  ErrorSet ERROR ::= { err1, ... }

  PDU ::= SEQUENCE {
    code ERROR.&errorCode ({ErrorSet}),
    val  ERROR.&Type ({ErrorSet}{@code})
  }
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d);
      s += '\n';
    }
    return s;
  }();
  ASSERT_GE(r.module->assignments().size(), 4u);
  auto* oc =
      dynamic_cast<const asn1::ast::ObjectClassAssignment*>(r.module->assignments()[0].get());
  ASSERT_NE(oc, nullptr);
  EXPECT_EQ(oc->name(), "ERROR");
  ASSERT_EQ(oc->defn().fields().size(), 2u);
  EXPECT_FALSE(oc->defn().with_syntax().empty());

  auto* obj =
      dynamic_cast<const asn1::ast::ObjectAssignment*>(r.module->assignments()[1].get());
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->class_name(), "ERROR");

  auto* oset =
      dynamic_cast<const asn1::ast::ObjectSetAssignment*>(r.module->assignments()[2].get());
  ASSERT_NE(oset, nullptr);
  EXPECT_TRUE(oset->defn().extensible());

  auto* pdu =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[3].get());
  auto* seq = dynamic_cast<const asn1::ast::SequenceType*>(&pdu->type());
  ASSERT_NE(seq, nullptr);
  auto* code = dynamic_cast<const asn1::ast::Component*>(seq->items()[0].get());
  auto* ocf = dynamic_cast<const asn1::ast::ObjectClassFieldType*>(&code->type());
  ASSERT_NE(ocf, nullptr);
  EXPECT_EQ(ocf->class_name(), "ERROR");
  EXPECT_EQ(ocf->field_name(), "errorCode");
  EXPECT_NE(dynamic_cast<const asn1::ast::TableConstraint*>(ocf->constraint()), nullptr);
}

TEST(Parser, ExtensibleIntegerConstraint) {
  auto r = parse_string(R"(
M DEFINITIONS ::=
BEGIN
  X ::= INTEGER (0..255, ...)
END
)");
  ASSERT_TRUE(r.diag.ok());
  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* integer = dynamic_cast<const asn1::ast::IntegerType*>(&ta->type());
  EXPECT_NE(dynamic_cast<const asn1::ast::ExtensibleConstraint*>(integer->constraint()),
            nullptr);
}

TEST(Parser, Phase12EnumeratedOidSet) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Color ::= ENUMERATED { red, green(5), blue, ..., other }
  Id ::= OBJECT IDENTIFIER
  Rel ::= RELATIVE-OID
  S ::= SET { a INTEGER, b BOOLEAN OPTIONAL }
  L ::= SET OF INTEGER
  oidVal OBJECT IDENTIFIER ::= { 1 2 840 }
  namedOid OBJECT IDENTIFIER ::= { iso(1) member-body(2) }
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d);
      s += '\n';
    }
    return s;
  }();
  ASSERT_GE(r.module->assignments().size(), 5u);

  auto* color =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* en = dynamic_cast<const asn1::ast::EnumeratedType*>(&color->type());
  ASSERT_NE(en, nullptr);
  ASSERT_EQ(en->root().size(), 3u);
  EXPECT_EQ(en->root()[0].name, "red");
  EXPECT_TRUE(en->root()[0].number_text.empty());
  EXPECT_EQ(en->root()[1].number_text, "5");
  EXPECT_TRUE(en->extensible());
  ASSERT_EQ(en->extensions().size(), 1u);

  auto* id = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[1].get());
  EXPECT_NE(dynamic_cast<const asn1::ast::ObjectIdentifierType*>(&id->type()), nullptr);

  auto* set_ta =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[3].get());
  EXPECT_NE(dynamic_cast<const asn1::ast::SetType*>(&set_ta->type()), nullptr);

  auto* oid_va =
      dynamic_cast<const asn1::ast::ValueAssignment*>(r.module->assignments()[5].get());
  auto* oidv = dynamic_cast<const asn1::ast::ObjectIdentifierValue*>(&oid_va->value());
  ASSERT_NE(oidv, nullptr);
  ASSERT_EQ(oidv->arcs().size(), 3u);
  EXPECT_EQ(*oidv->arcs()[0].number, 1u);
}

TEST(Parser, SequenceOfSizeConstraint) {
  auto r = parse_string(R"(
M DEFINITIONS ::=
BEGIN
  List ::= SEQUENCE (SIZE (1..10)) OF INTEGER
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();
  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* sof = dynamic_cast<const asn1::ast::SequenceOfType*>(&ta->type());
  ASSERT_NE(sof, nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::SizeConstraint*>(sof->constraint()), nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::IntegerType*>(&sof->element()), nullptr);
}

TEST(Parser, VersionBracketsInSequence) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  S ::= SEQUENCE {
    a INTEGER,
    ...,
    [[
      b BOOLEAN OPTIONAL,
      c UTF8String
    ]]
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
  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* seq = dynamic_cast<const asn1::ast::SequenceType*>(&ta->type());
  ASSERT_NE(seq, nullptr);
  ASSERT_EQ(seq->items().size(), 3u);
  EXPECT_NE(dynamic_cast<const asn1::ast::ExtensionMarker*>(seq->items()[1].get()), nullptr);
  auto* grp = dynamic_cast<const asn1::ast::VersionAdditionGroup*>(seq->items()[2].get());
  ASSERT_NE(grp, nullptr);
  ASSERT_EQ(grp->items().size(), 2u);
  auto* b = dynamic_cast<const asn1::ast::Component*>(grp->items()[0].get());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->name(), "b");
  EXPECT_EQ(b->presence(), asn1::ast::Presence::Optional);
}

TEST(Parser, UTCTimeField) {
  auto r = parse_string(R"(
M DEFINITIONS ::=
BEGIN
  Stamp ::= SEQUENCE {
    when UTCTime,
    when2 GeneralizedTime
  }
END
)");
  ASSERT_TRUE(r.diag.ok());
  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* seq = dynamic_cast<const asn1::ast::SequenceType*>(&ta->type());
  auto* when = dynamic_cast<const asn1::ast::Component*>(seq->items()[0].get());
  auto* s = dynamic_cast<const asn1::ast::StringType*>(&when->type());
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->kind(), asn1::ast::StringKind::UTCTime);
  auto* when2 = dynamic_cast<const asn1::ast::Component*>(seq->items()[1].get());
  auto* s2 = dynamic_cast<const asn1::ast::StringType*>(&when2->type());
  ASSERT_NE(s2, nullptr);
  EXPECT_EQ(s2->kind(), asn1::ast::StringKind::GeneralizedTime);
}

TEST(Parser, ParameterizedTypeAndDefinedSyntax) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  PROTOCOL-IES ::= CLASS {
    &id INTEGER UNIQUE,
    &Type
  } WITH SYNTAX {
    &Type IDENTIFIED BY &id
  }

  ProtocolIE-Field {PROTOCOL-IES : IEsSetParam} ::= SEQUENCE {
    id PROTOCOL-IES.&id ({IEsSetParam}),
    value PROTOCOL-IES.&Type ({IEsSetParam}{@id})
  }

  myIE PROTOCOL-IES ::= { INTEGER IDENTIFIED BY 1 }

  MyIEs PROTOCOL-IES ::= { myIE }

  MyField ::= ProtocolIE-Field {{MyIEs}}
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();

  auto* field_ta =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[1].get());
  ASSERT_NE(field_ta, nullptr);
  ASSERT_EQ(field_ta->parameters().size(), 1u);
  EXPECT_EQ(field_ta->parameters()[0].governor, "PROTOCOL-IES");
  EXPECT_EQ(field_ta->parameters()[0].name, "IEsSetParam");

  auto* obj =
      dynamic_cast<const asn1::ast::ObjectAssignment*>(r.module->assignments()[2].get());
  ASSERT_NE(obj, nullptr);
  ASSERT_EQ(obj->defn().settings().size(), 2u);
  EXPECT_EQ(obj->defn().settings()[0].field_name, "Type");
  EXPECT_NE(obj->defn().settings()[0].type_setting, nullptr);
  EXPECT_EQ(obj->defn().settings()[1].field_name, "id");
  EXPECT_NE(obj->defn().settings()[1].value_setting, nullptr);

  auto* inst =
      dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[4].get());
  auto* ref = dynamic_cast<const asn1::ast::ReferencedType*>(&inst->type());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->name(), "ProtocolIE-Field");
  ASSERT_EQ(ref->actuals().size(), 1u);
  ASSERT_TRUE(ref->actuals()[0].object_set_name.has_value());
  EXPECT_EQ(*ref->actuals()[0].object_set_name, "MyIEs");
}

TEST(Parser, MultiModuleAndImportsParameterized) {
  auto r = parse_string(R"(
A DEFINITIONS ::=
BEGIN
  T ::= INTEGER
END

B DEFINITIONS ::=
BEGIN
IMPORTS T{} FROM A;
  U ::= T
END
)");
  // parse_module only gets the first; use parser.parse_modules via a local helper.
  asn1::SourceFile file = asn1::SourceFile::from_string("multi.asn", R"(
A DEFINITIONS ::=
BEGIN
  T ::= INTEGER
END

B DEFINITIONS ::=
BEGIN
IMPORTS T{} FROM A;
  U ::= T
END
)");
  asn1::Diagnostics diag;
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto modules = parser.parse_modules();
  ASSERT_TRUE(diag.ok()) << [&] {
    std::string s;
    for (const auto& d : diag.items()) {
      s += diag.format(d) + "\n";
    }
    return s;
  }();
  ASSERT_EQ(modules.size(), 2u);
  EXPECT_EQ(modules[0]->name(), "A");
  EXPECT_EQ(modules[1]->name(), "B");
  ASSERT_EQ(modules[1]->imports().size(), 1u);
  ASSERT_EQ(modules[1]->imports()[0].symbols.size(), 1u);
  EXPECT_TRUE(modules[1]->imports()[0].symbols[0].parameterized);
  (void)r;
}

TEST(Parser, ExternalEmbeddedPdvCharacterString) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  A ::= EXTERNAL
  B ::= EMBEDDED PDV
  C ::= CHARACTER STRING
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();
  auto* a = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  auto* b = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[1].get());
  auto* c = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[2].get());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::ExternalType*>(&a->type()), nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::EmbeddedPdvType*>(&b->type()), nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::CharacterStringType*>(&c->type()), nullptr);
}

TEST(Parser, RealWithComponents) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  B ::= REAL (WITH COMPONENTS {
                mantissa (-16777215..16777215),
                base (2),
                exponent (-149..104)
            })
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();
  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  ASSERT_NE(ta, nullptr);
  auto* real = dynamic_cast<const asn1::ast::RealType*>(&ta->type());
  ASSERT_NE(real, nullptr);
  auto* wc = dynamic_cast<const asn1::ast::WithComponentsConstraint*>(real->constraint());
  ASSERT_NE(wc, nullptr);
  ASSERT_EQ(wc->components().size(), 3u);
  EXPECT_EQ(wc->components()[0].name, "mantissa");
  EXPECT_EQ(wc->components()[1].name, "base");
  EXPECT_EQ(wc->components()[2].name, "exponent");
  EXPECT_NE(wc->components()[0].value_constraint, nullptr);
}

TEST(Parser, OctetStringContaining) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Inner ::= INTEGER
  Wrapped ::= OCTET STRING (CONTAINING Inner)
  Sized ::= OCTET STRING (SIZE (1..8)) (CONTAINING Inner)
  Bits ::= BIT STRING (CONTAINING Inner)
END
)");
  ASSERT_TRUE(r.diag.ok()) << [&] {
    std::string s;
    for (const auto& d : r.diag.items()) {
      s += r.diag.format(d) + "\n";
    }
    return s;
  }();

  auto* wrapped = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[1].get());
  ASSERT_NE(wrapped, nullptr);
  auto* oct = dynamic_cast<const asn1::ast::OctetStringType*>(&wrapped->type());
  ASSERT_NE(oct, nullptr);
  auto* contents = dynamic_cast<const asn1::ast::ContentsConstraint*>(oct->constraint());
  ASSERT_NE(contents, nullptr);
  ASSERT_NE(contents->contained(), nullptr);
  auto* inner_ref = dynamic_cast<const asn1::ast::ReferencedType*>(contents->contained());
  ASSERT_NE(inner_ref, nullptr);
  EXPECT_EQ(inner_ref->name(), "Inner");

  auto* bits_a = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[3].get());
  auto* bits = dynamic_cast<const asn1::ast::BitStringType*>(&bits_a->type());
  ASSERT_NE(bits, nullptr);
  EXPECT_NE(dynamic_cast<const asn1::ast::ContentsConstraint*>(bits->constraint()), nullptr);
}

TEST(Parser, JerInstructionsAndEncodingControl) {
  auto r = parse_string(R"asn(
Oss DEFINITIONS JER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  A ::= [ARRAY] SEQUENCE {
    a1 INTEGER,
    a2 [NAME AS "_1/ (2@3&)"] INTEGER OPTIONAL
  }
  C ::= [UNWRAPPED] CHOICE { c1 BOOLEAN, c2 INTEGER }
  Color ::= [TEXT ALL AS CAPITALIZED] ENUMERATED { red, green, blue }
  ENCODING-CONTROL JER [BASE64] OCTET STRING
END
)asn");
  ASSERT_TRUE(r.diag.ok());
  ASSERT_NE(r.module, nullptr);
  EXPECT_TRUE(r.module->jer_instructions());
  EXPECT_EQ(r.module->jer_encoding_control().size(), 1u);
  EXPECT_EQ(r.module->jer_encoding_control()[0].target,
            asn1::ast::EncodingControlTarget::OctetString);
  EXPECT_EQ(r.module->jer_encoding_control()[0].instruction.kind,
            asn1::ast::EncodingInstructionKind::Base64);

  const asn1::ast::TypeAssignment* a_ta = nullptr;
  for (const auto& asn : r.module->assignments()) {
    if (const auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(asn.get())) {
      if (ta->name() == "A") {
        a_ta = ta;
      }
    }
  }
  ASSERT_NE(a_ta, nullptr);
  const auto& eis = a_ta->type().encoding_instructions();
  ASSERT_EQ(eis.size(), 1u);
  EXPECT_EQ(eis[0].kind, asn1::ast::EncodingInstructionKind::Array);

  const std::string ast_dump = asn1::ast::to_string(*r.module);
  EXPECT_NE(ast_dump.find("jerInstructions=true"), std::string::npos);
  EXPECT_NE(ast_dump.find("encodingInstructions ARRAY"), std::string::npos);
}

TEST(Parser, ApplicationTagNotConfusedWithEncodingInstruction) {
  auto r = parse_string(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Tagged ::= [APPLICATION 3] INTEGER
END
)");
  ASSERT_TRUE(r.diag.ok());
  auto* ta = dynamic_cast<const asn1::ast::TypeAssignment*>(r.module->assignments()[0].get());
  ASSERT_NE(ta, nullptr);
  const auto* integer = dynamic_cast<const asn1::ast::IntegerType*>(&ta->type());
  ASSERT_NE(integer, nullptr);
  ASSERT_TRUE(integer->tag());
  EXPECT_EQ(integer->tag()->cls, asn1::ast::TagClass::Application);
  EXPECT_EQ(integer->tag()->number_text, "3");
  EXPECT_TRUE(integer->encoding_instructions().empty());
}

