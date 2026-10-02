#include <asn1/ast/module.hpp>
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

TEST(Parser, UnsupportedClassReportsPreciseError) {
  auto r = parse_string(R"(
M DEFINITIONS ::=
BEGIN
  C ::= CLASS { &id INTEGER }
END
)",
                        "bad.asn");
  EXPECT_FALSE(r.diag.ok());
  ASSERT_FALSE(r.diag.items().empty());
  EXPECT_EQ(r.diag.items().front().where.begin.file, "bad.asn");
  EXPECT_NE(r.diag.items().front().message.find("CLASS"), std::string::npos);
  EXPECT_NE(r.diag.items().front().message.find("not supported"), std::string::npos);
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
