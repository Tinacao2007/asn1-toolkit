#include <asn1/codegen/emit.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

asn1::ir::Model analyze(const std::string& text, asn1::Diagnostics& diag,
                        asn1::SourceFile& file) {
  file = asn1::SourceFile::from_string("test.asn", text);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto module = parser.parse_module();
  EXPECT_NE(module, nullptr);
  asn1::Analyzer analyzer(diag);
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  modules.push_back(std::move(module));
  return analyzer.analyze(std::move(modules));
}

}  // namespace

TEST(Codegen, EmitsPersonStructAndUperCodecs) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
MyModule DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Person ::= SEQUENCE {
    id      INTEGER (0..65535),
    name    UTF8String,
    age     INTEGER (0..150) OPTIONAL
  }
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.namespace_name = "asn1_gen";
  opt.codec = asn1::codegen::CodecKind::Uper;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());

  EXPECT_NE(header.find("struct Person"), std::string::npos);
  EXPECT_NE(header.find("std::uint16_t id"), std::string::npos);
  EXPECT_NE(header.find("std::string name"), std::string::npos);
  EXPECT_NE(header.find("std::optional<std::uint8_t> age"), std::string::npos);
  EXPECT_NE(header.find("encode_uper"), std::string::npos);
  EXPECT_NE(header.find("decode_uper"), std::string::npos);
  EXPECT_NE(header.find("65535"), std::string::npos);
  EXPECT_NE(header.find("asn1::uper::encode_integer"), std::string::npos);
  EXPECT_NE(header.find("encode_sequence_preamble"), std::string::npos);
}

TEST(Codegen, EmitsChoiceVariant) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS ::=
BEGIN
  Pick ::= CHOICE {
    flag BOOLEAN,
    num  INTEGER (0..7)
  }
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());
  EXPECT_NE(header.find("struct Pick"), std::string::npos);
  EXPECT_NE(header.find("std::variant"), std::string::npos);
  EXPECT_NE(header.find("encode_choice_root"), std::string::npos);
}
