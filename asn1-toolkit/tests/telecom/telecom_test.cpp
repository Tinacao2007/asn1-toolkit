#include <asn1/codegen/emit.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixture(const char* relative) {
  return std::filesystem::path(ASN1_TELECOM_FIXTURE_DIR) / relative;
}

struct LoadResult {
  asn1::Diagnostics diag;
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  asn1::ir::Model model;
};

LoadResult parse_file(const std::filesystem::path& path) {
  LoadResult r;
  asn1::SourceFile file = asn1::SourceFile::from_path(path.string());
  asn1::Lexer lexer(file, r.diag);
  asn1::Parser parser(lexer, r.diag);
  r.modules = parser.parse_modules();
  return r;
}

LoadResult analyze_file(const std::filesystem::path& path) {
  LoadResult r = parse_file(path);
  if (!r.diag.ok()) {
    return r;
  }
  asn1::Analyzer analyzer(r.diag);
  r.model = analyzer.analyze(std::move(r.modules));
  return r;
}

std::string format_diag(const asn1::Diagnostics& diag) {
  std::string s;
  for (const auto& d : diag.items()) {
    s += diag.format(d);
    s += '\n';
  }
  return s;
}

}  // namespace

TEST(Telecom, ParseRrc86Definitions) {
  const auto path = fixture("rrc_8_6_0/EUTRA-RRC-Definitions.asn");
  ASSERT_TRUE(std::filesystem::exists(path)) << path.string();
  auto r = parse_file(path);
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  ASSERT_EQ(r.modules.size(), 1u);
  EXPECT_EQ(r.modules[0]->name(), "EUTRA-RRC-Definitions");
  EXPECT_GE(r.modules[0]->assignments().size(), 100u);
}

TEST(Telecom, AnalyzeRrc86Definitions) {
  const auto path = fixture("rrc_8_6_0/EUTRA-RRC-Definitions.asn");
  auto r = analyze_file(path);
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  EXPECT_GE(r.model.arena.type_count(), 100u);
}

TEST(Telecom, ParseLpp143) {
  const auto path = fixture("lpp_14_3_0/LPP-PDU-Definitions.asn");
  ASSERT_TRUE(std::filesystem::exists(path)) << path.string();
  auto r = parse_file(path);
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  ASSERT_EQ(r.modules.size(), 1u);
  EXPECT_GE(r.modules[0]->assignments().size(), 100u);
}

TEST(Telecom, AnalyzeLpp143) {
  const auto path = fixture("lpp_14_3_0/LPP-PDU-Definitions.asn");
  auto r = analyze_file(path);
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  EXPECT_GE(r.model.arena.type_count(), 100u);
}

TEST(Telecom, ParseS1ap144) {
  const auto path = fixture("s1ap_14_4_0/s1ap_14_4_0.asn");
  ASSERT_TRUE(std::filesystem::exists(path)) << path.string();
  auto r = parse_file(path);
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  EXPECT_GE(r.modules.size(), 6u);
}

TEST(Telecom, AnalyzeS1ap144) {
  const auto path = fixture("s1ap_14_4_0/s1ap_14_4_0.asn");
  auto r = analyze_file(path);
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  EXPECT_GE(r.model.arena.type_count(), 100u);
  EXPECT_FALSE(r.model.object_classes.empty());
}

namespace {

std::size_t count_messages_containing(const asn1::Diagnostics& diag, const char* needle) {
  std::size_t n = 0;
  for (const auto& d : diag.items()) {
    if (d.message.find(needle) != std::string::npos) {
      ++n;
    }
  }
  return n;
}

std::string emit_header(const asn1::ir::Model& model, asn1::codegen::CodecKind codec,
                        asn1::Diagnostics& diag) {
  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = codec;
  return gen.emit_header_string(model, opt, diag);
}

}  // namespace

// Phase 40: synthetic names for nested SEQUENCE/SET/CHOICE + parameterized
// ProtocolIE-Field instantiation enable full-module emit.
TEST(TelecomValidation, EmitRrc86UperSucceeds) {
  auto r = analyze_file(fixture("rrc_8_6_0/EUTRA-RRC-Definitions.asn"));
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  asn1::Diagnostics emit_diag;
  const std::string header =
      emit_header(r.model, asn1::codegen::CodecKind::Uper, emit_diag);
  ASSERT_TRUE(emit_diag.ok()) << format_diag(emit_diag);
  EXPECT_EQ(count_messages_containing(emit_diag, "anonymous SEQUENCE/SET/CHOICE"), 0u);
  EXPECT_NE(header.find("encode_uper"), std::string::npos);
  EXPECT_NE(header.find("BCCH_BCH_Message"), std::string::npos);
}

TEST(TelecomValidation, EmitLpp143UperSucceeds) {
  auto r = analyze_file(fixture("lpp_14_3_0/LPP-PDU-Definitions.asn"));
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  asn1::Diagnostics emit_diag;
  const std::string header =
      emit_header(r.model, asn1::codegen::CodecKind::Uper, emit_diag);
  ASSERT_TRUE(emit_diag.ok()) << format_diag(emit_diag);
  EXPECT_EQ(count_messages_containing(emit_diag, "anonymous SEQUENCE/SET/CHOICE"), 0u);
  EXPECT_NE(header.find("encode_uper"), std::string::npos);
}

TEST(TelecomValidation, EmitS1ap144AperSucceeds) {
  auto r = analyze_file(fixture("s1ap_14_4_0/s1ap_14_4_0.asn"));
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  asn1::Diagnostics emit_diag;
  const std::string header =
      emit_header(r.model, asn1::codegen::CodecKind::Aper, emit_diag);
  ASSERT_TRUE(emit_diag.ok()) << format_diag(emit_diag);
  EXPECT_EQ(count_messages_containing(emit_diag, "unresolved type reference"), 0u);
  EXPECT_NE(header.find("encode_aper"), std::string::npos);
  EXPECT_NE(header.find("ProtocolIE_Field"), std::string::npos);
}

// Named-slice fixture used for real codec round-trips (see asn1_telecom_roundtrip_tests).
TEST(TelecomValidation, EmitRrcSliceUperSucceeds) {
  auto r = analyze_file(fixture("rrc_slice/rrc_slice.asn"));
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  asn1::Diagnostics emit_diag;
  const std::string header =
      emit_header(r.model, asn1::codegen::CodecKind::Uper, emit_diag);
  ASSERT_TRUE(emit_diag.ok()) << format_diag(emit_diag);
  EXPECT_NE(header.find("encode_uper"), std::string::npos);
  EXPECT_NE(header.find("decode_uper"), std::string::npos);
  EXPECT_NE(header.find("BCCH_BCH_Message"), std::string::npos);
  EXPECT_NE(header.find("PCCH_Message"), std::string::npos);
}

TEST(TelecomValidation, EmitRrcSliceOerSucceeds) {
  auto r = analyze_file(fixture("rrc_slice/rrc_slice.asn"));
  ASSERT_TRUE(r.diag.ok()) << format_diag(r.diag);
  asn1::Diagnostics emit_diag;
  const std::string header =
      emit_header(r.model, asn1::codegen::CodecKind::Oer, emit_diag);
  ASSERT_TRUE(emit_diag.ok()) << format_diag(emit_diag);
  EXPECT_NE(header.find("encode_oer"), std::string::npos);
  EXPECT_NE(header.find("decode_oer"), std::string::npos);
}
