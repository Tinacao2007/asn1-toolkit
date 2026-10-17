/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/tlv_oer_golden_roundtrip_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
**
**  File Description:
**
**   GoogleTest exercising tlv oer golden roundtrip behaviour.
**
**   Round-trips the tlv_oer_golden.asn fixture (tags, DEFAULT values,
**   SET/SET OF, nested CHOICE, extensions) across DER/BER/OER/JER/XER and
**   pins the byte-level consequences of the emitter fixes:
**     - X.680 25.3: one textual tag in the list disables automatic tagging,
**       so an untagged component keeps its natural universal tag;
**     - DEFAULT on an unconstrained INTEGER lowers to
**       BigInteger::from_i64(...) and survives a DER round-trip;
**     - EXPLICIT vs IMPLICIT component tags (X.690 8.5);
**     - nested CHOICE decode (alternative struct qualification).
**
** Specification: ITU-T X.680 / X.690 / X.691 / X.696
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
**************************************************************************/

#include "generated.hpp"

#include <asn1/runtime/byte_io.hpp>
#include <asn1/runtime/jer/json.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

namespace gen = asn1_tlv_oer;

using gen::Coll;
using gen::Coll_s;
using gen::Misc;
using gen::Nested;
using gen::Nested_rec;
using gen::Nested_rec_q;
using gen::Tagged;

}  // namespace

TEST(TlvOerGolden, TaggedDerRoundTripWithMixedTags) {
  // X.680 25.3: the list carries textual tags ([1]/[2]/[3]), so automatic
  // tagging is OFF for the whole list and `plain` keeps its natural
  // UNIVERSAL INTEGER tag (02) — not CONTEXT [0] (A0).
  Tagged t;
  t.plain = asn1::BigInteger::from_i64(7);
  t.ctx = asn1::BigInteger::from_i64(8);
  t.impl = "hi";
  t.expl = true;

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_der(w, t).ok());
  const auto bytes = w.take();

  // 30 0F | 02 01 07 | 81 01 08 | 82 02 68 69 | A3 03 01 01 FF
  const std::vector<std::uint8_t> expected = {
      0x30, 0x0F, 0x02, 0x01, 0x07, 0x81, 0x01, 0x08,
      0x82, 0x02, 0x68, 0x69, 0xA3, 0x03, 0x01, 0x01, 0xFF};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  Tagged got{};
  ASSERT_TRUE(gen::decode_der(r, got).ok());
  EXPECT_EQ(got.plain, asn1::BigInteger::from_i64(7));
  ASSERT_TRUE(got.ctx.has_value());
  EXPECT_EQ(*got.ctx, asn1::BigInteger::from_i64(8));
  ASSERT_TRUE(got.impl.has_value());
  EXPECT_EQ(*got.impl, "hi");
  ASSERT_TRUE(got.expl.has_value());
  EXPECT_TRUE(*got.expl);
}

TEST(TlvOerGolden, TaggedDerRoundTripWithoutOptionals) {
  Tagged t;
  t.plain = asn1::BigInteger::from_i64(1);

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_der(w, t).ok());
  const auto bytes = w.take();

  // Only the mandatory INTEGER is present; optional components are absent.
  const std::vector<std::uint8_t> expected = {0x30, 0x03, 0x02, 0x01, 0x01};
  EXPECT_EQ(bytes, expected);

  asn1::ByteReader r(bytes);
  Tagged got{};
  ASSERT_TRUE(gen::decode_der(r, got).ok());
  EXPECT_EQ(got.plain, asn1::BigInteger::from_i64(1));
  EXPECT_FALSE(got.ctx.has_value());
  EXPECT_FALSE(got.impl.has_value());
  EXPECT_FALSE(got.expl.has_value());
}

TEST(TlvOerGolden, CollDerOmitsDefaults) {
  // DEFAULT values equal to the declared default are omitted from the DER
  // encoding (X.690 11.5) and restored on decode. `def` is an unconstrained
  // INTEGER: the default must lower to BigInteger::from_i64(42), and
  // `defS` is a character-string default whose quotes must not leak.
  Coll c;
  Coll_s s;
  s.x = asn1::BigInteger::from_i64(5);
  s.y = true;
  c.s = s;
  c.sof.push_back(asn1::BigInteger::from_i64(1));
  c.sof.push_back(asn1::BigInteger::from_i64(2));
  c.sqof.push_back("a");
  c.sqof.push_back("b");
  // def / defS keep their defaults (42 / "hi").

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_der(w, c).ok());
  const auto bytes = w.take();

  // [3] (def) and [4] (defS) tags must not appear: defaults were omitted.
  for (std::uint8_t b : bytes) {
    EXPECT_NE(b & 0x1Fu, 0x03u) << "unexpected [3] DEFAULT component in DER bytes";
    EXPECT_NE(b & 0x1Fu, 0x04u) << "unexpected [4] DEFAULT component in DER bytes";
  }

  asn1::ByteReader r(bytes);
  Coll got{};
  ASSERT_TRUE(gen::decode_der(r, got).ok());
  ASSERT_TRUE(got.s.has_value());
  EXPECT_EQ(got.s->x, asn1::BigInteger::from_i64(5));
  EXPECT_TRUE(got.s->y);
  ASSERT_EQ(got.sof.size(), 2u);
  EXPECT_EQ(got.sof[0], asn1::BigInteger::from_i64(1));
  EXPECT_EQ(got.sof[1], asn1::BigInteger::from_i64(2));
  ASSERT_EQ(got.sqof.size(), 2u);
  EXPECT_EQ(got.sqof[0], "a");
  EXPECT_EQ(got.sqof[1], "b");
  EXPECT_EQ(got.def, asn1::BigInteger::from_i64(42));
  EXPECT_EQ(got.defS, "hi");
}

TEST(TlvOerGolden, CollDerCarriesNonDefaultDefaults) {
  Coll c;
  c.def = asn1::BigInteger::from_i64(99);
  c.defS = "bye";

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_der(w, c).ok());
  const auto bytes = w.take();

  // [3] INTEGER 99 → 83 01 63 ; [4] UTF8String "bye" → 84 03 62 79 65
  // (primitive types carry primitive context tags, not constructed ones).
  auto contains = [&](const std::vector<std::uint8_t>& sub) {
    return std::search(bytes.begin(), bytes.end(), sub.begin(), sub.end()) != bytes.end();
  };
  EXPECT_TRUE(contains({0x83, 0x01, 0x63}));
  EXPECT_TRUE(contains({0x84, 0x03, 0x62, 0x79, 0x65}));

  asn1::ByteReader r(bytes);
  Coll got{};
  ASSERT_TRUE(gen::decode_der(r, got).ok());
  EXPECT_EQ(got.def, asn1::BigInteger::from_i64(99));
  EXPECT_EQ(got.defS, "bye");
}

TEST(TlvOerGolden, NestedChoiceDerRoundTrip) {
  // A CHOICE whose alternative is itself a CHOICE inside a SEQUENCE:
  // exercises nested alternative-struct qualification on decode.
  Nested n;
  Nested_rec rec;
  rec.p = asn1::BigInteger::from_i64(3);
  Nested_rec_q q;
  Nested_rec_q::n_ alt_n;
  alt_n.value = {0xDE, 0xAD};
  q.alt = alt_n;
  rec.q = q;
  Nested::rec_ alt_rec;
  alt_rec.value = rec;
  n.alt = alt_rec;

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_der(w, n).ok());
  asn1::ByteReader r(w.take());
  Nested got{};
  ASSERT_TRUE(gen::decode_der(r, got).ok());
  ASSERT_TRUE(std::holds_alternative<Nested::rec_>(got.alt));
  const auto& got_rec = std::get<Nested::rec_>(got.alt).value;
  EXPECT_EQ(got_rec.p, asn1::BigInteger::from_i64(3));
  ASSERT_TRUE(std::holds_alternative<Nested_rec_q::n_>(got_rec.q.alt));
  EXPECT_EQ(std::get<Nested_rec_q::n_>(got_rec.q.alt).value,
            (std::vector<std::uint8_t>{0xDE, 0xAD}));
}

TEST(TlvOerGolden, NestedChoiceJerRoundTrip) {
  Nested n;
  Nested::flag_ alt_flag;
  alt_flag.value = true;
  n.alt = alt_flag;

  auto v = gen::encode_jer(n);
  ASSERT_TRUE(v.ok());
  Nested got{};
  ASSERT_TRUE(gen::decode_jer(v.value(), got).ok());
  ASSERT_TRUE(std::holds_alternative<Nested::flag_>(got.alt));
  EXPECT_TRUE(std::get<Nested::flag_>(got.alt).value);
}

TEST(TlvOerGolden, MiscOerRoundTrip) {
  Misc m;
  m.neg = -1000;
  m.big = asn1::BigInteger::from_i64(1234567890123LL);
  m.en = 1;
  m.bstr.bits = {0x80};
  m.bstr.bit_length = 1;
  m.ostr = {1, 2, 3};

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_oer(w, m).ok());
  asn1::ByteReader r(w.take());
  Misc got{};
  ASSERT_TRUE(gen::decode_oer(r, got).ok());
  EXPECT_EQ(got.neg, -1000);
  EXPECT_EQ(got.big, asn1::BigInteger::from_i64(1234567890123LL));
  EXPECT_EQ(got.en, 1);
  EXPECT_EQ(got.ostr, (std::vector<std::uint8_t>{1, 2, 3}));
}

TEST(TlvOerGolden, TaggedBerRoundTrip) {
  // BER accepts the same encoding; optionals absent.
  Tagged t;
  t.plain = asn1::BigInteger::from_i64(42);
  t.impl = "abc";

  asn1::ByteWriter w;
  ASSERT_TRUE(gen::encode_ber(w, t).ok());
  asn1::ByteReader r(w.take());
  Tagged got{};
  ASSERT_TRUE(gen::decode_ber(r, got).ok());
  EXPECT_EQ(got.plain, asn1::BigInteger::from_i64(42));
  ASSERT_TRUE(got.impl.has_value());
  EXPECT_EQ(*got.impl, "abc");
}
