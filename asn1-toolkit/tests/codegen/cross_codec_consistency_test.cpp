/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/cross_codec_consistency_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-12
**   $Author: tina.cao
***************************************************************************/
/**  File Description:
**
**   Cross-codec consistency: the same logical Person value must survive an
**   encode/decode round-trip under every codec, yielding identical field
**   values. This guards against a codec drifting from the shared type model.
**
** Specification: ITU-T X.680 type model; per-codec rules X.691 (PER),
**                 X.696 (OER/COER), X.690 (BER/DER), X.697 (JER).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "per/generated.hpp"
#include "oer/generated.hpp"
#include "coer/generated.hpp"
#include "ber/generated.hpp"
#include "der/generated.hpp"
#include "jer/generated.hpp"

#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/byte_io.hpp>
#include <asn1/runtime/jer/json.hpp>

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

namespace {

// The canonical logical value every codec must preserve.
struct PersonView {
  std::uint32_t id;
  std::string name;
  bool age_present;
  std::uint32_t age;
};

const PersonView kPerson{42, "Ada", true, 36};

}  // namespace

// ---- PER family (UPER / APER) ----

TEST(CrossCodec, UperRoundTripPreservesValue) {
  asn1_gen_per::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen_per::encode_uper(w, p).ok());
  const auto bytes = w.take();
  asn1::BitReader r(bytes);
  asn1_gen_per::Person got{};
  ASSERT_TRUE(asn1_gen_per::decode_uper(r, got).ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

TEST(CrossCodec, AperRoundTripPreservesValue) {
  asn1_gen_per::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen_per::encode_aper(w, p).ok());
  const auto bytes = w.take();
  asn1::BitReader r(bytes);
  asn1_gen_per::Person got{};
  ASSERT_TRUE(asn1_gen_per::decode_aper(r, got).ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

// ---- OER / COER (octet-aligned) ----

TEST(CrossCodec, OerRoundTripPreservesValue) {
  asn1_gen_oer::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_oer::encode_oer(w, p).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_oer::Person got{};
  ASSERT_TRUE(asn1_gen_oer::decode_oer(r, got).ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

TEST(CrossCodec, CoerRoundTripPreservesValue) {
  asn1_gen_coer::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_coer::encode_coer(w, p).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_coer::Person got{};
  ASSERT_TRUE(asn1_gen_coer::decode_coer(r, got).ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

// ---- BER / DER ----

TEST(CrossCodec, BerRoundTripPreservesValue) {
  asn1_gen_ber::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_ber::encode_ber(w, p).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_ber::Person got{};
  ASSERT_TRUE(asn1_gen_ber::decode_ber(r, got).ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

TEST(CrossCodec, DerRoundTripPreservesValue) {
  asn1_gen_der::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_der::encode_der(w, p).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_der::Person got{};
  ASSERT_TRUE(asn1_gen_der::decode_der(r, got).ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

// ---- JER (JSON) ----

TEST(CrossCodec, JerRoundTripPreservesValue) {
  asn1_gen_jer::Person p{};
  p.id = static_cast<std::uint16_t>(kPerson.id);
  p.name = kPerson.name;
  p.age = static_cast<std::uint8_t>(kPerson.age);
  auto enc = asn1_gen_jer::encode_jer(p);
  ASSERT_TRUE(enc.ok());
  asn1_gen_jer::Person got{};
  auto dec = asn1_gen_jer::decode_jer(enc.value(), got);
  ASSERT_TRUE(dec.ok());
  EXPECT_EQ(got.id, kPerson.id);
  EXPECT_EQ(got.name, kPerson.name);
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, kPerson.age);
}

// Optional field must round-trip as absent too (not just present).
TEST(CrossCodec, UperOptionalAbsentRoundTrip) {
  asn1_gen_per::Person p{};
  p.id = 7;
  p.name = "Bo";
  p.age = std::nullopt;
  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen_per::encode_uper(w, p).ok());
  const auto bytes = w.take();
  asn1::BitReader r(bytes);
  asn1_gen_per::Person got{};
  ASSERT_TRUE(asn1_gen_per::decode_uper(r, got).ok());
  EXPECT_EQ(got.id, 7u);
  EXPECT_EQ(got.name, "Bo");
  EXPECT_FALSE(got.age.has_value());
}
