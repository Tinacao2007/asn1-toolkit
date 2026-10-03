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
  // Multi-type modules use out-parameter decode (return-type-only overloads are ill-formed).
  EXPECT_NE(header.find("decode_uper(asn1::BitReader& r, Person& value)"), std::string::npos);
  EXPECT_EQ(header.find("Result<Person> decode_uper(asn1::BitReader& r)"), std::string::npos);
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

TEST(Codegen, UnconstrainedIntegerUsesBigInteger) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Wide ::= INTEGER
  Narrow ::= INTEGER (0..255)
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Uper;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());
  EXPECT_NE(header.find("asn1::BigInteger"), std::string::npos);
  EXPECT_NE(header.find("std::uint8_t"), std::string::npos);
  EXPECT_NE(header.find("decode_big_integer"), std::string::npos);
}

TEST(Codegen, EmitsJerCodecsWithInstructions) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
Oss DEFINITIONS JER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  Row ::= [ARRAY] SEQUENCE {
    id INTEGER,
    data OCTET STRING
  }
  ENCODING-CONTROL JER [BASE64] OCTET STRING
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Jer;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());

  EXPECT_NE(header.find("encode_jer"), std::string::npos);
  EXPECT_NE(header.find("decode_jer"), std::string::npos);
  EXPECT_NE(header.find("encode_sequence_array"), std::string::npos);
  EXPECT_NE(header.find("encode_octet_string_base64"), std::string::npos);
  EXPECT_EQ(header.find("encode_uper"), std::string::npos);
}

TEST(Codegen, EmitsJerSequenceOfAndSetOfDecode) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
Sof DEFINITIONS JER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  Ints ::= SEQUENCE OF INTEGER
  Pair ::= SEQUENCE {
    key UTF8String,
    val INTEGER
  }
  Map ::= [OBJECT] SET OF Pair
  Bag ::= SET OF INTEGER
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok()) << diag.items().front().message;

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Jer;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());

  EXPECT_NE(header.find("asn1::jeri::decode_sequence_of"), std::string::npos);
  EXPECT_NE(header.find("asn1::jeri::encode_sequence_of"), std::string::npos);
  EXPECT_NE(header.find("asn1::jeri::decode_set_of_object"), std::string::npos);
  EXPECT_NE(header.find("asn1::jeri::encode_set_of_object"), std::string::npos);
  EXPECT_EQ(header.find("JER decode not fully generated for this type"), std::string::npos);
}

TEST(Codegen, EmitsBerDerOerCxerCodecs) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Person ::= SEQUENCE {
    id INTEGER (0..65535),
    name UTF8String,
    age INTEGER (0..150) OPTIONAL
  }
  Pick ::= CHOICE { a BOOLEAN, b INTEGER }
  Bag ::= SET OF INTEGER
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok()) << diag.items().front().message;

  asn1::codegen::CppGenerator gen;
  {
    asn1::Diagnostics d;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Ber;
    const std::string header = gen.emit_header_string(model, opt, d);
    ASSERT_TRUE(d.ok());
    EXPECT_NE(header.find("encode_ber"), std::string::npos);
    EXPECT_NE(header.find("decode_ber"), std::string::npos);
    EXPECT_NE(header.find("asn1/runtime/ber/codec.hpp"), std::string::npos);
    EXPECT_EQ(header.find("encode_uper"), std::string::npos);
  }
  {
    asn1::Diagnostics d;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Der;
    const std::string header = gen.emit_header_string(model, opt, d);
    ASSERT_TRUE(d.ok());
    EXPECT_NE(header.find("encode_der"), std::string::npos);
    EXPECT_NE(header.find("decode_der"), std::string::npos);
    EXPECT_NE(header.find("asn1/runtime/der/codec.hpp"), std::string::npos);
    EXPECT_EQ(header.find("encode_uper"), std::string::npos);
  }
  {
    asn1::Diagnostics d;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Oer;
    const std::string header = gen.emit_header_string(model, opt, d);
    ASSERT_TRUE(d.ok());
    EXPECT_NE(header.find("encode_oer"), std::string::npos);
    EXPECT_NE(header.find("decode_oer"), std::string::npos);
    EXPECT_NE(header.find("asn1::oer::encode_sequence_preamble"), std::string::npos);
    EXPECT_EQ(header.find("encode_uper"), std::string::npos);
  }
  {
    asn1::Diagnostics d;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Coer;
    const std::string header = gen.emit_header_string(model, opt, d);
    ASSERT_TRUE(d.ok());
    EXPECT_NE(header.find("encode_coer"), std::string::npos);
    EXPECT_NE(header.find("decode_coer"), std::string::npos);
    EXPECT_NE(header.find("asn1/runtime/coer/codec.hpp"), std::string::npos);
    EXPECT_NE(header.find("asn1::coer::encode_sequence_preamble"), std::string::npos);
    EXPECT_NE(header.find("asn1::coer::encode_set_of"), std::string::npos);
    EXPECT_NE(header.find("asn1::coer::require_set_of_order"), std::string::npos);
    EXPECT_EQ(header.find("encode_oer"), std::string::npos);
    EXPECT_EQ(header.find("encode_uper"), std::string::npos);
  }
  {
    asn1::Diagnostics d;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Cxer;
    const std::string header = gen.emit_header_string(model, opt, d);
    ASSERT_TRUE(d.ok());
    EXPECT_NE(header.find("encode_cxer"), std::string::npos);
    EXPECT_NE(header.find("decode_cxer"), std::string::npos);
    EXPECT_NE(header.find("asn1::cxer::"), std::string::npos);
    EXPECT_NE(header.find("encode_set_of"), std::string::npos);
    EXPECT_EQ(header.find("encode_xer"), std::string::npos);
    EXPECT_EQ(header.find("encode_uper"), std::string::npos);
  }
}

TEST(Codegen, EmitsXerExerAndInstanceOf) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
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
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Exer;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());

  EXPECT_NE(header.find("encode_xer"), std::string::npos);
  EXPECT_NE(header.find("decode_xer"), std::string::npos);
  EXPECT_NE(header.find("encode_attribute_boolean"), std::string::npos);
  EXPECT_NE(header.find("encode_octet_string_base64"), std::string::npos);
  EXPECT_NE(header.find("encode_boolean_text"), std::string::npos);
  EXPECT_NE(header.find("struct Open"), std::string::npos);
  EXPECT_NE(header.find("type_id"), std::string::npos);
  EXPECT_NE(header.find("encode_object_identifier"), std::string::npos);
  EXPECT_EQ(header.find("encode_uper"), std::string::npos);
}

TEST(Codegen, EmitsExternalAsSequence) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS ::=
BEGIN
  Ext ::= EXTERNAL
  Pdv ::= EMBEDDED PDV
  Cs ::= CHARACTER STRING
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Uper;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());
  EXPECT_NE(header.find("struct Ext"), std::string::npos);
  EXPECT_NE(header.find("identification"), std::string::npos);
  EXPECT_NE(header.find("data_value"), std::string::npos);
  EXPECT_NE(header.find("data_value_descriptor"), std::string::npos);
  EXPECT_NE(header.find("syntaxes"), std::string::npos);
  EXPECT_NE(header.find("struct Pdv"), std::string::npos);
  EXPECT_NE(header.find("struct Cs"), std::string::npos);
  EXPECT_NE(header.find("string_value"), std::string::npos);
  EXPECT_NE(header.find("encode_uper"), std::string::npos);
  // Classic EXTERNAL fields must not appear on the modern associated form.
  EXPECT_EQ(header.find("direct_reference"), std::string::npos);
  EXPECT_EQ(header.find("octet_aligned"), std::string::npos);
}

TEST(Codegen, EmitsRealCodecs) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS ::=
BEGIN
  R ::= REAL
  S ::= SEQUENCE { x REAL }
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  {
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Uper;
    const std::string header = gen.emit_header_string(model, opt, diag);
    ASSERT_TRUE(diag.ok());
    EXPECT_NE(header.find("using R = double;"), std::string::npos);
    EXPECT_NE(header.find("asn1::uper::encode_real"), std::string::npos);
    EXPECT_NE(header.find("asn1::uper::decode_real"), std::string::npos);
    EXPECT_EQ(header.find("REAL encoding not implemented"), std::string::npos);
  }
  {
    asn1::Diagnostics d2;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Jer;
    const std::string header = gen.emit_header_string(model, opt, d2);
    ASSERT_TRUE(d2.ok());
    EXPECT_NE(header.find("asn1::jeri::encode_real"), std::string::npos);
    EXPECT_NE(header.find("asn1::jeri::decode_real"), std::string::npos);
  }
  {
    asn1::Diagnostics d3;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Exer;
    const std::string header = gen.emit_header_string(model, opt, d3);
    ASSERT_TRUE(d3.ok());
    EXPECT_NE(header.find("asn1::xer::encode_real"), std::string::npos);
    EXPECT_NE(header.find("asn1::xer::decode_real"), std::string::npos);
  }
}

TEST(Codegen, EmitsExerFullInstructionMatrix) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS XER INSTRUCTIONS AUTOMATIC TAGS ::=
BEGIN
  Color ::= [USE-NUMBER] ENUMERATED { red(1), blue(2) }
  ColorNums ::= [LIST] SEQUENCE OF Color
  Names ::= [LIST] SEQUENCE OF UTF8String
  Nums ::= [LIST] SEQUENCE OF INTEGER
  Flags ::= [LIST] SEQUENCE OF BOOLEAN
  Ids ::= [LIST] SEQUENCE OF OBJECT IDENTIFIER
  Hue ::= ENUMERATED { red, green, blue }
  Hues ::= [LIST] SEQUENCE OF Hue
  Inner ::= SEQUENCE {
    x INTEGER,
    y INTEGER
  }
  Outer ::= SEQUENCE {
    id [ATTRIBUTE] INTEGER,
    label [ATTRIBUTE] UTF8String,
    nest [UNTAGGED] Inner,
    opt [USE-NIL] INTEGER OPTIONAL,
    note [NAME AS "Note"] UTF8String
  }
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok()) << diag.items().front().message;

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Exer;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok());

  EXPECT_NE(header.find("encode_enumerated_number"), std::string::npos);
  EXPECT_NE(header.find("decode_enumerated_number"), std::string::npos);
  EXPECT_NE(header.find("encode_list_of_strings"), std::string::npos);
  EXPECT_NE(header.find("decode_list_of_strings"), std::string::npos);
  EXPECT_NE(header.find("encode_list_of_integers"), std::string::npos);
  EXPECT_NE(header.find("decode_list_of_big_integers"), std::string::npos);
  EXPECT_NE(header.find("encode_list_of_booleans"), std::string::npos);
  EXPECT_NE(header.find("encode_list_of_object_identifiers"), std::string::npos);
  EXPECT_EQ(header.find("LIST encoding requires string or integer elements"),
            std::string::npos);
  EXPECT_EQ(header.find("LIST decode requires string or integer elements"),
            std::string::npos);
  // Identifier ENUMERATED LIST (Hues) emits name mapping; USE-NUMBER ColorNums uses ints.
  EXPECT_NE(header.find("xer_enum_id"), std::string::npos);
  EXPECT_NE(header.find("append_untagged"), std::string::npos);
  EXPECT_NE(header.find("set_nil"), std::string::npos);
  EXPECT_NE(header.find("is_nil"), std::string::npos);
  EXPECT_NE(header.find("encode_attribute_integer"), std::string::npos);
  EXPECT_NE(header.find("encode_attribute_string"), std::string::npos);
  EXPECT_NE(header.find("\"Note\""), std::string::npos);
  // Generated USE-NIL branch must not reference a nonexistent `exer_mode` variable.
  EXPECT_EQ(header.find("exer_mode &&"), std::string::npos);
}

TEST(Codegen, EmitsPerExtensionAdditions) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  S ::= SEQUENCE {
    a BOOLEAN,
    ...,
    b BOOLEAN,
    c INTEGER (0..7)
  }
  C ::= CHOICE {
    x BOOLEAN,
    ...,
    y INTEGER (0..7)
  }
  E ::= ENUMERATED { red, green, ..., blue }
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Uper;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok()) << (diag.items().empty() ? "" : diag.items().front().message);

  EXPECT_NE(header.find("std::optional<bool> b"), std::string::npos);
  EXPECT_NE(header.find("encode_extension_additions"), std::string::npos);
  EXPECT_NE(header.find("decode_extension_additions"), std::string::npos);
  EXPECT_NE(header.find("encode_choice_extension"), std::string::npos);
  EXPECT_NE(header.find("decode_choice"), std::string::npos);
  EXPECT_NE(header.find("encode_enumerated_extension"), std::string::npos);
  EXPECT_NE(header.find("struct y_"), std::string::npos);
}

TEST(Codegen, EmitsOerExtensionAdditions) {
  asn1::Diagnostics diag;
  asn1::SourceFile file;
  auto model = analyze(R"(
M DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  S ::= SEQUENCE {
    a BOOLEAN,
    ...,
    b BOOLEAN,
    c INTEGER (0..7)
  }
  C ::= CHOICE {
    x BOOLEAN,
    ...,
    y INTEGER (0..7)
  }
END
)",
                       diag, file);
  ASSERT_TRUE(diag.ok());

  asn1::codegen::CppGenerator gen;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Oer;
  const std::string header = gen.emit_header_string(model, opt, diag);
  ASSERT_TRUE(diag.ok()) << (diag.items().empty() ? "" : diag.items().front().message);

  EXPECT_NE(header.find("encode_oer"), std::string::npos);
  EXPECT_NE(header.find("decode_oer"), std::string::npos);
  EXPECT_NE(header.find("asn1::oer::encode_extension_additions"), std::string::npos);
  EXPECT_NE(header.find("asn1::oer::decode_extension_additions"), std::string::npos);
  EXPECT_NE(header.find("asn1::oer::encode_open_type"), std::string::npos);
  EXPECT_NE(header.find("asn1::oer::decode_open_type"), std::string::npos);
  EXPECT_NE(header.find("std::optional<bool> b"), std::string::npos);
}
