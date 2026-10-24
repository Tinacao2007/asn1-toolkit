#include <asn1/codegen/emit.hpp>
#include <asn1/support/diagnostics.hpp>
#include <gtest/gtest.h>
#include <sstream>

TEST(CGenBatch1, ApiRejectsCWithoutWritingStreams) {
  asn1::ir::Model model;
  asn1::codegen::EmitOptions options;
  options.language = asn1::codegen::TargetLanguage::C;
  for (bool split : {false, true}) {
    options.emit_split = split;
    asn1::Diagnostics diag;
    std::ostringstream header, source;
    asn1::codegen::CppGenerator generator;
    generator.emit(model, options, diag, header, &source);
    EXPECT_FALSE(diag.ok());
    EXPECT_TRUE(header.str().empty());
    EXPECT_TRUE(source.str().empty());
  }
}
TEST(CGenBatch1, ApiRejectsCppWithCPrefix) {
  asn1::ir::Model model;
  asn1::codegen::EmitOptions options;
  options.c_prefix = "sample";
  asn1::Diagnostics diag;
  asn1::codegen::CppGenerator generator;
  EXPECT_TRUE(generator.emit_header_string(model, options, diag).empty());
  EXPECT_FALSE(diag.ok());
}
