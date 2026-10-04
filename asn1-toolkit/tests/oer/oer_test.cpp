/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/oer/oer_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising oer behaviour.
**
** Specification: ITU-T X.696 — ASN.1 encoding rules: Octet Encoding
**                 Rules (OER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/oer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> bytes(std::initializer_list<std::uint8_t> b) {
  return std::vector<std::uint8_t>(b);
}

void expect_eq(const std::vector<std::uint8_t>& got, const std::vector<std::uint8_t>& want) {
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

template <typename Encode, typename Decode, typename T>
void roundtrip(Encode enc, Decode dec, const T& value,
               const std::vector<std::uint8_t>& expected) {
  asn1::ByteWriter w;
  enc(w, value);
  expect_eq(w.buffer(), expected);
  asn1::ByteReader r(w.buffer());
  auto got = dec(r);
  ASSERT_TRUE(got.ok()) << got.error().message;
  EXPECT_EQ(got.value(), value);
  EXPECT_TRUE(r.eof());
}

}  // namespace

TEST(Oer, BooleanVectors) {
  roundtrip(
      [](asn1::ByteWriter& w, bool v) { asn1::oer::encode_boolean(w, v); },
      [](asn1::ByteReader& r) { return asn1::oer::decode_boolean(r); }, true, bytes({0xFF}));
  roundtrip(
      [](asn1::ByteWriter& w, bool v) { asn1::oer::encode_boolean(w, v); },
      [](asn1::ByteReader& r) { return asn1::oer::decode_boolean(r); }, false, bytes({0x00}));
}

TEST(Oer, NullIsEmpty) {
  asn1::ByteWriter w;
  asn1::oer::encode_null(w);
  EXPECT_TRUE(w.buffer().empty());
  asn1::ByteReader r(w.buffer());
  ASSERT_TRUE(asn1::oer::decode_null(r).ok());
}

TEST(Oer, IntegerUnconstrainedVectors) {
  // From asn1tools test_oer: unconstrained INTEGER.
  const asn1::oer::IntegerConstraint unc{};
  auto enc = [&](asn1::ByteWriter& w, std::int64_t v) {
    asn1::oer::encode_integer(w, v, unc);
  };
  auto dec = [&](asn1::ByteReader& r) { return asn1::oer::decode_integer(r, unc); };

  roundtrip(enc, dec, 0, bytes({0x01, 0x00}));
  roundtrip(enc, dec, 128, bytes({0x02, 0x00, 0x80}));
  roundtrip(enc, dec, 100000, bytes({0x03, 0x01, 0x86, 0xA0}));
  roundtrip(enc, dec, -255, bytes({0x02, 0xFF, 0x01}));
  roundtrip(enc, dec, -1234567, bytes({0x03, 0xED, 0x29, 0x79}));
}

TEST(Oer, IntegerFixedWidthVectors) {
  // B ::= INTEGER (-128..127) → 1 signed byte
  asn1::oer::IntegerConstraint b{-128, 127, false};
  {
    asn1::ByteWriter w;
    asn1::oer::encode_integer(w, -2, b);
    expect_eq(w.buffer(), bytes({0xFE}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::oer::decode_integer(r, b);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), -2);
  }
  // F ::= INTEGER (0..255)
  asn1::oer::IntegerConstraint f{0, 255, false};
  {
    asn1::ByteWriter w;
    asn1::oer::encode_integer(w, 128, f);
    expect_eq(w.buffer(), bytes({0x80}));
  }
  // G ::= INTEGER (0..65535)
  asn1::oer::IntegerConstraint g{0, 65535, false};
  {
    asn1::ByteWriter w;
    asn1::oer::encode_integer(w, 1000, g);
    expect_eq(w.buffer(), bytes({0x03, 0xE8}));
  }
}

TEST(Oer, OctetStringVectors) {
  // A ::= OCTET STRING
  {
    asn1::ByteWriter w;
    auto data = bytes({0x12, 0x34});
    asn1::oer::encode_octet_string(w, data);
    expect_eq(w.buffer(), bytes({0x02, 0x12, 0x34}));
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_octet_string(r);
    ASSERT_TRUE(got.ok());
    expect_eq(got.value(), data);
  }
  // B ::= OCTET STRING (SIZE (3)) — no length determinant
  {
    asn1::oer::SizeConstraint sz{3, 3, false};
    asn1::ByteWriter w;
    auto data = bytes({0x12, 0x34, 0x56});
    asn1::oer::encode_octet_string(w, data, sz);
    expect_eq(w.buffer(), data);
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_octet_string(r, sz);
    ASSERT_TRUE(got.ok());
    expect_eq(got.value(), data);
  }
  // Long length: 999 bytes → 0x82 0x03 0xE7
  {
    std::vector<std::uint8_t> data(999, 0x01);
    asn1::ByteWriter w;
    asn1::oer::encode_octet_string(w, data);
    ASSERT_GE(w.buffer().size(), 3u);
    EXPECT_EQ(w.buffer()[0], 0x82);
    EXPECT_EQ(w.buffer()[1], 0x03);
    EXPECT_EQ(w.buffer()[2], 0xE7);
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_octet_string(r);
    ASSERT_TRUE(got.ok());
    EXPECT_EQ(got.value().size(), 999u);
  }
}

TEST(Oer, BitStringVectors) {
  // A ::= BIT STRING — (b'\x40', 4) → 0x02 0x04 0x40
  {
    asn1::ByteWriter w;
    auto bits = bytes({0x40});
    asn1::oer::encode_bit_string(w, bits, 4);
    expect_eq(w.buffer(), bytes({0x02, 0x04, 0x40}));
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_bit_string(r);
    ASSERT_TRUE(got.ok());
    EXPECT_EQ(got.value().bit_length, 4u);
    ASSERT_FALSE(got.value().bits.empty());
    EXPECT_EQ(got.value().bits[0], 0x40);
  }
  // B ::= BIT STRING (SIZE (9)) — fixed
  {
    asn1::oer::SizeConstraint sz{9, 9, false};
    asn1::ByteWriter w;
    auto bits = bytes({0x12, 0x80});
    asn1::oer::encode_bit_string(w, bits, 9, sz);
    expect_eq(w.buffer(), bytes({0x12, 0x80}));
  }
}

TEST(Oer, Utf8StringRoundTrip) {
  asn1::ByteWriter w;
  asn1::oer::encode_utf8_string(w, "ABC");
  expect_eq(w.buffer(), bytes({0x03, 'A', 'B', 'C'}));
  asn1::ByteReader r(w.buffer());
  auto got = asn1::oer::decode_utf8_string(r);
  ASSERT_TRUE(got.ok());
  EXPECT_EQ(got.value(), "ABC");
}

TEST(Oer, EnumeratedSmallAndLarge) {
  asn1::ByteWriter w;
  asn1::oer::encode_enumerated(w, 3);
  expect_eq(w.buffer(), bytes({0x03}));
  asn1::ByteReader r(w.buffer());
  auto got = asn1::oer::decode_enumerated(r);
  ASSERT_TRUE(got.ok());
  EXPECT_EQ(got.value(), 3);

  asn1::ByteWriter w2;
  asn1::oer::encode_enumerated(w2, 128);
  // length 2 content 0x00 0x80 with distinguishing bit on length → 0x82 0x00 0x80
  expect_eq(w2.buffer(), bytes({0x82, 0x00, 0x80}));
  asn1::ByteReader r2(w2.buffer());
  auto got2 = asn1::oer::decode_enumerated(r2);
  ASSERT_TRUE(got2.ok()) << got2.error().message;
  EXPECT_EQ(got2.value(), 128);
}

TEST(Oer, SequencePreambleOptionals) {
  // Two optionals, first present second absent → bits 10 + pad → 0x80
  asn1::ByteWriter w;
  bool opts[] = {true, false};
  asn1::oer::encode_sequence_preamble(w, /*extensible=*/false, false, opts);
  expect_eq(w.buffer(), bytes({0x80}));
  asn1::ByteReader r(w.buffer());
  auto got = asn1::oer::decode_sequence_preamble(r, false, 2);
  ASSERT_TRUE(got.ok());
  EXPECT_FALSE(got.value().extensions_present);
  ASSERT_EQ(got.value().optionals.size(), 2u);
  EXPECT_TRUE(got.value().optionals[0]);
  EXPECT_FALSE(got.value().optionals[1]);
}

TEST(Oer, SequenceExtensionAdditions) {
  // SEQUENCE { a BOOLEAN, ..., b BOOLEAN, c BOOLEAN }
  // a=true, b=true, c=true: ext bit 1, a=0xFF, bitmap len=2 unused=6 bits=11 →
  // 0x02 0x06 0xC0, then open types 0x01 0xFF each.
  asn1::ByteWriter w;
  asn1::oer::encode_sequence_preamble(w, true, true,
                                      asn1::Span<const bool>(nullptr, 0));
  asn1::oer::encode_boolean(w, true);
  const bool ep[] = {true, true};
  std::vector<std::vector<std::uint8_t>> ots;
  {
    asn1::ByteWriter ow;
    asn1::oer::encode_boolean(ow, true);
    ots.push_back(ow.take());
  }
  {
    asn1::ByteWriter ow;
    asn1::oer::encode_boolean(ow, true);
    ots.push_back(ow.take());
  }
  asn1::oer::encode_extension_additions(w, ep, ots);
  auto buf = w.take();

  asn1::ByteReader r(buf);
  auto pre = asn1::oer::decode_sequence_preamble(r, true, 0);
  ASSERT_TRUE(pre.ok());
  EXPECT_TRUE(pre.value().extensions_present);
  auto a = asn1::oer::decode_boolean(r);
  ASSERT_TRUE(a.ok());
  EXPECT_TRUE(a.value());
  auto ext = asn1::oer::decode_extension_additions(r);
  ASSERT_TRUE(ext.ok()) << ext.error().message;
  ASSERT_EQ(ext.value().presence.size(), 2u);
  EXPECT_TRUE(ext.value().presence[0]);
  EXPECT_TRUE(ext.value().presence[1]);
  ASSERT_EQ(ext.value().open_types.size(), 2u);
  {
    asn1::ByteReader er(ext.value().open_types[0]);
    auto b = asn1::oer::decode_boolean(er);
    ASSERT_TRUE(b.ok());
    EXPECT_TRUE(b.value());
  }
  {
    asn1::ByteReader er(ext.value().open_types[1]);
    auto c = asn1::oer::decode_boolean(er);
    ASSERT_TRUE(c.ok());
    EXPECT_TRUE(c.value());
  }
}

TEST(Oer, ChoiceTagAndSequenceOfLength) {
  asn1::ByteWriter w;
  asn1::oer::encode_choice_tag(w, 1, false);
  expect_eq(w.buffer(), bytes({0x81}));  // context|1
  asn1::ByteReader r(w.buffer());
  auto tag = asn1::oer::decode_choice_tag(r);
  ASSERT_TRUE(tag.ok());
  EXPECT_EQ(tag.value(), 1u);

  asn1::ByteWriter w2;
  asn1::oer::encode_sequence_of_length(w2, 4);
  expect_eq(w2.buffer(), bytes({0x01, 0x04}));
}

TEST(Oer, ObjectIdentifierRoundTrip) {
  std::uint64_t arcs[] = {1, 2, 3};
  asn1::ByteWriter w;
  asn1::oer::encode_object_identifier(w, arcs);
  asn1::ByteReader r(w.buffer());
  auto got = asn1::oer::decode_object_identifier(r);
  ASSERT_TRUE(got.ok()) << got.error().message;
  ASSERT_EQ(got.value().size(), 3u);
  EXPECT_EQ(got.value()[0], 1u);
  EXPECT_EQ(got.value()[1], 2u);
  EXPECT_EQ(got.value()[2], 3u);
}

TEST(Oer, RealUnconstrainedRoundTrip) {
  {
    asn1::ByteWriter w;
    ASSERT_TRUE(asn1::oer::encode_real(w, 100.0).ok());
    expect_eq(w.buffer(), bytes({0x03, 0x80, 0x02, 0x19}));
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_real(r);
    ASSERT_TRUE(got.ok()) << got.error().message;
    EXPECT_EQ(got.value(), 100.0);
  }
  {
    asn1::ByteWriter w;
    ASSERT_TRUE(asn1::oer::encode_real(w, 0.0).ok());
    expect_eq(w.buffer(), bytes({0x00}));
  }
}

TEST(Oer, RealBinary32FixedForm) {
  using Form = asn1::oer::RealIeeeForm;
  struct Case {
    double value;
    std::vector<std::uint8_t> encoding;
  };
  const Case cases[] = {
      {0.0, bytes({0x00, 0x00, 0x00, 0x00})},
      {1.0, bytes({0x3F, 0x80, 0x00, 0x00})},
      {std::ldexp(1.0, -126), bytes({0x00, 0x80, 0x00, 0x00})},
      {(1.0 - std::ldexp(1.0, -24)) * std::ldexp(1.0, 128),
       bytes({0x7F, 0x7F, 0xFF, 0xFF})},
  };
  for (const auto& c : cases) {
    asn1::ByteWriter w;
    ASSERT_TRUE(asn1::oer::encode_real(w, c.value, Form::Binary32).ok()) << c.value;
    expect_eq(w.buffer(), c.encoding);
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_real(r, Form::Binary32);
    ASSERT_TRUE(got.ok()) << got.error().message;
    EXPECT_FLOAT_EQ(static_cast<float>(got.value()), static_cast<float>(c.value));
  }

  asn1::ByteWriter w;
  auto err = asn1::oer::encode_real(w, std::ldexp(1.0, 128), Form::Binary32);
  ASSERT_FALSE(err.ok());
  EXPECT_EQ(err.error().code, asn1::Error::Code::InvalidArgument);
}

TEST(Oer, RealBinary64FixedForm) {
  using Form = asn1::oer::RealIeeeForm;
  struct Case {
    double value;
    std::vector<std::uint8_t> encoding;
  };
  const Case cases[] = {
      {0.0, bytes({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})},
      {1.0, bytes({0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})},
      {std::ldexp(1.0, -1022), bytes({0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})},
      {(2.0 - std::ldexp(1.0, -52)) * std::ldexp(1.0, 1023),
       bytes({0x7F, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})},
  };
  for (const auto& c : cases) {
    asn1::ByteWriter w;
    ASSERT_TRUE(asn1::oer::encode_real(w, c.value, Form::Binary64).ok()) << c.value;
    expect_eq(w.buffer(), c.encoding);
    asn1::ByteReader r(w.buffer());
    auto got = asn1::oer::decode_real(r, Form::Binary64);
    ASSERT_TRUE(got.ok()) << got.error().message;
    EXPECT_EQ(got.value(), c.value);
  }
}
