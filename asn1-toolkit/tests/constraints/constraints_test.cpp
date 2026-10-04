/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/constraints/constraints_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising constraints behaviour.
**
** Specification: ITU-T X.680 — subtyping and constraint notation (X.682
**                 constraint application).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/constraints/normalize.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

asn1::ir::ConstraintDesc normalize_from_type_constraint(const std::string& module_body) {
  const std::string text = "M DEFINITIONS ::=\nBEGIN\n  T ::= " + module_body + "\nEND\n";
  asn1::Diagnostics diag;
  auto file = asn1::SourceFile::from_string("c.asn", text);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto mod = parser.parse_module();
  EXPECT_TRUE(diag.ok()) << [&] {
    std::string s;
    for (const auto& d : diag.items()) {
      s += diag.format(d) + "\n";
    }
    return s;
  }();
  EXPECT_NE(mod, nullptr);
  const auto* ta =
      dynamic_cast<const asn1::ast::TypeAssignment*>(mod->assignments().front().get());
  EXPECT_NE(ta, nullptr);
  return asn1::constraints::normalize(ta->type().constraint(), diag);
}

const asn1::ir::Type* analyze_named(const std::string& text, const std::string& name,
                                    asn1::ir::Model& model, asn1::Diagnostics& diag) {
  auto file = asn1::SourceFile::from_string("t.asn", text);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto mod = parser.parse_module();
  asn1::Analyzer analyzer(diag);
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  modules.push_back(std::move(mod));
  model = analyzer.analyze(std::move(modules));
  for (const auto& t : model.arena.types()) {
    if (t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

}  // namespace

TEST(Constraints, ValueRangeConstrained) {
  auto c = normalize_from_type_constraint("INTEGER (0..255)");
  ASSERT_TRUE(c.statically_foldable);
  EXPECT_FALSE(c.extensible);
  EXPECT_EQ(c.per_kind(), asn1::ir::PerBoundKind::Constrained);
  ASSERT_EQ(c.root_ranges.size(), 1u);
  ASSERT_TRUE(c.lower && c.upper);
  EXPECT_EQ(c.lower->to_string(), "0");
  EXPECT_EQ(c.upper->to_string(), "255");
  ASSERT_TRUE(c.constrained_span().has_value());
  EXPECT_EQ(*c.constrained_span(), 255u);
}

TEST(Constraints, SingleValue) {
  auto c = normalize_from_type_constraint("INTEGER (5)");
  EXPECT_EQ(c.per_kind(), asn1::ir::PerBoundKind::Constrained);
  ASSERT_EQ(c.root_ranges.size(), 1u);
  EXPECT_EQ(c.lower->to_string(), "5");
  EXPECT_EQ(c.upper->to_string(), "5");
  EXPECT_EQ(*c.constrained_span(), 0u);
}

TEST(Constraints, UnionMergesAdjacentRanges) {
  auto c = normalize_from_type_constraint("INTEGER (0..10 | 11..20)");
  ASSERT_EQ(c.root_ranges.size(), 1u);
  EXPECT_EQ(c.lower->to_string(), "0");
  EXPECT_EQ(c.upper->to_string(), "20");
  EXPECT_EQ(c.per_kind(), asn1::ir::PerBoundKind::Constrained);
}

TEST(Constraints, UnionKeepsDisjointRanges) {
  auto c = normalize_from_type_constraint("INTEGER (0..10 | 20..30)");
  ASSERT_EQ(c.root_ranges.size(), 2u);
  EXPECT_EQ(c.root_ranges[0].lower->to_string(), "0");
  EXPECT_EQ(c.root_ranges[0].upper->to_string(), "10");
  EXPECT_EQ(c.root_ranges[1].lower->to_string(), "20");
  EXPECT_EQ(c.root_ranges[1].upper->to_string(), "30");
  // Envelope for host / rough PER span.
  EXPECT_EQ(c.lower->to_string(), "0");
  EXPECT_EQ(c.upper->to_string(), "30");
}

TEST(Constraints, Intersection) {
  auto c = normalize_from_type_constraint("INTEGER ((0..100) ^ (50..200))");
  ASSERT_EQ(c.root_ranges.size(), 1u);
  EXPECT_EQ(c.lower->to_string(), "50");
  EXPECT_EQ(c.upper->to_string(), "100");
}

TEST(Constraints, IntersectionEmpty) {
  asn1::Diagnostics diag;
  const std::string text = R"(
M DEFINITIONS ::=
BEGIN
  T ::= INTEGER ((0..10) ^ (20..30))
END
)";
  auto file = asn1::SourceFile::from_string("e.asn", text);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto mod = parser.parse_module();
  ASSERT_NE(mod, nullptr);
  const auto* ta =
      dynamic_cast<const asn1::ast::TypeAssignment*>(mod->assignments().front().get());
  auto c = asn1::constraints::normalize(ta->type().constraint(), diag);
  EXPECT_TRUE(c.empty);
  // Empty intersection is a warning, not a hard error.
  EXPECT_TRUE(diag.ok());
  bool saw = false;
  for (const auto& d : diag.items()) {
    if (d.message.find("empty") != std::string::npos) {
      saw = true;
    }
  }
  EXPECT_TRUE(saw);
}

TEST(Constraints, ExtensibleRange) {
  auto c = normalize_from_type_constraint("INTEGER (0..255, ...)");
  EXPECT_TRUE(c.extensible);
  EXPECT_EQ(c.per_kind(), asn1::ir::PerBoundKind::Constrained);
  EXPECT_EQ(c.lower->to_string(), "0");
  EXPECT_EQ(c.upper->to_string(), "255");
}

TEST(Constraints, SizeConstraint) {
  auto c = normalize_from_type_constraint("OCTET STRING (SIZE(1..128))");
  EXPECT_TRUE(c.is_size);
  EXPECT_EQ(c.per_kind(), asn1::ir::PerBoundKind::Constrained);
  EXPECT_EQ(c.lower->to_string(), "1");
  EXPECT_EQ(c.upper->to_string(), "128");
}

TEST(Constraints, SemiConstrained) {
  auto c = normalize_from_type_constraint("INTEGER (0..MAX)");
  EXPECT_EQ(c.per_kind(), asn1::ir::PerBoundKind::SemiConstrained);
  ASSERT_TRUE(c.lower);
  EXPECT_FALSE(c.upper.has_value());
}

TEST(Constraints, AnalyzerUsesNormalizedUnion) {
  asn1::Diagnostics diag;
  asn1::ir::Model model;
  const auto* t = analyze_named(R"(
M DEFINITIONS ::=
BEGIN
  U ::= INTEGER (0..10 | 20..30)
END
)",
                                "U", model, diag);
  ASSERT_NE(t, nullptr);
  ASSERT_TRUE(diag.ok());
  EXPECT_EQ(t->integer.constraint.root_ranges.size(), 2u);
  EXPECT_EQ(t->integer.constraint.per_kind(), asn1::ir::PerBoundKind::Constrained);
}

TEST(Constraints, InvalidRangeErrors) {
  asn1::Diagnostics diag;
  const std::string text = R"(
M DEFINITIONS ::=
BEGIN
  Bad ::= INTEGER (10..1)
END
)";
  auto file = asn1::SourceFile::from_string("bad.asn", text);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto mod = parser.parse_module();
  const auto* ta =
      dynamic_cast<const asn1::ast::TypeAssignment*>(mod->assignments().front().get());
  auto c = asn1::constraints::normalize(ta->type().constraint(), diag);
  EXPECT_TRUE(c.empty);
  EXPECT_FALSE(diag.ok());
}
