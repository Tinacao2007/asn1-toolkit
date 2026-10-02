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
