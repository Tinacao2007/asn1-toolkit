/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/runtime/bigint_codec_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising bigint codec behaviour.
**
** Specification: ITU-T X.680 INTEGER type; unconstrained encoding as
**                 used by BER/PER/OER callers.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/bigint.hpp>
#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/der/codec.hpp>
#include <asn1/runtime/oer/codec.hpp>
#include <asn1/runtime/coer/codec.hpp>
#include <asn1/runtime/uper.hpp>
#include <asn1/runtime/aper.hpp>
#include <asn1/runtime/xer/codec.hpp>
#include <asn1/runtime/jer/codec.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

asn1::BigInteger must_dec(const char* text) {
  auto v = asn1::BigInteger::from_decimal(text);
  EXPECT_TRUE(v.ok()) << text;
  return std::move(v.value());
}

}  // namespace

TEST(BigInteger, DecimalAndTwosComplementRoundTrip) {
  const char* samples[] = {
      "0",
      "1",
      "-1",
      "127",
      "128",
      "-128",
      "255",
      "256",
      "-256",
      "9223372036854775807",   // INT64_MAX
      "-9223372036854775808",  // INT64_MIN
      "18446744073709551616",  // 2^64
      "-18446744073709551616",
      "123456789012345678901234567890",
      "-987654321098765432109876543210",
  };
  for (const char* s : samples) {
    auto v = must_dec(s);
    EXPECT_EQ(v.to_decimal(), s);
    auto bytes = v.to_twos_complement();
    auto back = asn1::BigInteger::from_twos_complement(
        asn1::Span<const std::uint8_t>(bytes.data(), bytes.size()));
    ASSERT_TRUE(back.ok()) << s;
    EXPECT_EQ(back.value().to_decimal(), s);
  }
}

TEST(BigInteger, FitsI64) {
  EXPECT_EQ(*must_dec("42").as_i64(), 42);
  EXPECT_EQ(*must_dec("-42").as_i64(), -42);
  EXPECT_FALSE(must_dec("18446744073709551616").as_i64().has_value());
}

TEST(BerBigInteger, RoundTripWide) {
  const auto v = must_dec("123456789012345678901234567890");
  asn1::ByteWriter w;
  asn1::ber::encode_integer(w, v);
  asn1::ByteReader r(asn1::Span<const std::uint8_t>(w.buffer().data(), w.size()));
  auto d = asn1::ber::decode_big_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);

  // Narrow API rejects this value.
  asn1::ByteReader r2(asn1::Span<const std::uint8_t>(w.buffer().data(), w.size()));
  auto narrow = asn1::ber::decode_integer(r2);
  EXPECT_FALSE(narrow.ok());
}

TEST(DerBigInteger, RoundTripAndMinimal) {
  const auto v = must_dec("-123456789012345678901234567890");
  asn1::ByteWriter w;
  asn1::der::encode_integer(w, v);
  asn1::ByteReader r(asn1::Span<const std::uint8_t>(w.buffer().data(), w.size()));
  auto d = asn1::der::decode_big_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(UperBigInteger, UnconstrainedRoundTrip) {
  const auto v = must_dec("999999999999999999999999999999");
  asn1::BitWriter w;
  asn1::uper::encode_integer(w, v);
  const auto bytes = w.take();
  asn1::BitReader r(asn1::Span<const std::uint8_t>(bytes.data(), bytes.size()));
  auto d = asn1::uper::decode_big_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(AperBigInteger, UnconstrainedRoundTrip) {
  const auto v = must_dec("-999999999999999999999999999999");
  asn1::BitWriter w;
  asn1::aper::encode_integer(w, v);
  const auto bytes = w.take();
  asn1::BitReader r(asn1::Span<const std::uint8_t>(bytes.data(), bytes.size()));
  auto d = asn1::aper::decode_big_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(OerBigInteger, VariableSignedRoundTrip) {
  const auto v = must_dec("115792089237316195423570985008687907853269984665640564039457584007913129639935");
  asn1::ByteWriter w;
  asn1::oer::encode_integer(w, v);
  asn1::ByteReader r(asn1::Span<const std::uint8_t>(w.buffer().data(), w.size()));
  auto d = asn1::oer::decode_big_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(CoerBigInteger, VariableSignedRoundTrip) {
  const auto v = must_dec("-115792089237316195423570985008687907853269984665640564039457584007913129639936");
  asn1::ByteWriter w;
  asn1::coer::encode_integer(w, v);
  asn1::ByteReader r(asn1::Span<const std::uint8_t>(w.buffer().data(), w.size()));
  auto d = asn1::coer::decode_big_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(XerBigInteger, DecimalRoundTrip) {
  const auto v = must_dec("31415926535897932384626433832795");
  auto el = asn1::xer::encode_integer("Big", v);
  EXPECT_EQ(el.text, v.to_decimal());
  auto d = asn1::xer::decode_big_integer(el);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(JerBigInteger, JsonNumberRoundTrip) {
  const auto v = must_dec("27182818284590452353602874713527");
  auto jv = asn1::jer::encode_integer(v);
  const std::string json = asn1::jer::to_string(jv);
  EXPECT_EQ(json, v.to_decimal());
  auto parsed = asn1::jer::parse_document(json);
  ASSERT_TRUE(parsed.ok());
  auto d = asn1::jer::decode_big_integer(parsed.value());
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), v);
}

TEST(BerInteger, StillSupportsInt64) {
  asn1::ByteWriter w;
  asn1::ber::encode_integer(w, std::int64_t{-1});
  asn1::ByteReader r(asn1::Span<const std::uint8_t>(w.buffer().data(), w.size()));
  auto d = asn1::ber::decode_integer(r);
  ASSERT_TRUE(d.ok());
  EXPECT_EQ(d.value(), -1);
}
