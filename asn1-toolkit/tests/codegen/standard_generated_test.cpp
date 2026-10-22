/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/standard_generated_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising standard generated behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
// PROCODEC (2026). X.690 8.9/8.11 and 11.5 regression, all emission shapes.
#include "generated.hpp"
#include <gtest/gtest.h>
#ifdef STANDARD_SPLIT
namespace gen = asn1_gen::Standards;
#else
namespace gen = asn1_gen;
#endif

TEST(GeneratedPolicy, SequenceExplicitDefaultIsBerOnly) {
  gen::Def value{};
  const std::vector<std::uint8_t> definite{0x30, 3, 0x80, 1, 7};
  const std::vector<std::uint8_t> indefinite{0x30, 0x80, 0x80, 1, 7, 0, 0};
  asn1::ByteReader ber(definite), der(definite), cer(indefinite);
  ASSERT_TRUE(gen::decode_ber(ber, value));
  EXPECT_EQ(value.count.to_decimal(), "7");
  EXPECT_FALSE(gen::decode_der(der, value));
  EXPECT_FALSE(gen::decode_cer(cer, value));
}

TEST(GeneratedPolicy, SetExplicitDefaultIsBerOnly) {
  gen::DefSet value{};
  const std::vector<std::uint8_t> definite{0x31, 3, 0x80, 1, 7};
  const std::vector<std::uint8_t> indefinite{0x31, 0x80, 0x80, 1, 7, 0, 0};
  asn1::ByteReader ber(definite), der(definite), cer(indefinite);
  ASSERT_TRUE(gen::decode_ber(ber, value));
  EXPECT_EQ(value.count.to_decimal(), "7");
  EXPECT_FALSE(gen::decode_der(der, value));
  EXPECT_FALSE(gen::decode_cer(cer, value));
}

TEST(GeneratedPolicy, DuplicateSetComponentsRejected) {
  gen::DefSet value{};
  const std::vector<std::uint8_t> definite{0x31, 6, 0x80, 1, 8, 0x80, 1, 9};
  const std::vector<std::uint8_t> indefinite{0x31, 0x80, 0x80, 1, 8, 0x80, 1, 9, 0, 0};
  asn1::ByteReader ber(definite), der(definite), cer(indefinite);
  EXPECT_FALSE(gen::decode_ber(ber, value));
  EXPECT_FALSE(gen::decode_der(der, value));
  EXPECT_FALSE(gen::decode_cer(cer, value));
}

TEST(GeneratedPolicy, ValueParameterChangesActualPerBitWidth) {
  gen::Small small{}; small.value = 3;
  gen::Wide wide{}; wide.value = 3;
  asn1::BitWriter a, b;
  ASSERT_TRUE(gen::encode_uper(a, small));
  ASSERT_TRUE(gen::encode_uper(b, wide));
  EXPECT_EQ(a.buffer(), (std::vector<std::uint8_t>{0xc0}));
  EXPECT_EQ(b.buffer(), (std::vector<std::uint8_t>{0x60}));
  asn1::BitReader ra(a.buffer()), rb(b.buffer());
  gen::Small ds{}; gen::Wide dw{};
  ASSERT_TRUE(gen::decode_uper(ra, ds));
  ASSERT_TRUE(gen::decode_uper(rb, dw));
  EXPECT_EQ(ds.value, 3); EXPECT_EQ(dw.value, 3);
}

TEST(GeneratedPolicy, NativeIntegerCannotTruncateOrEscapeSubtype) {
  gen::Record value{};
  // count 300 used to narrow to uint8_t 44 before reporting success.
  const std::vector<std::uint8_t> bytes{0x30, 0x0d, 0x80, 2, 1, 0x2c,
                                      0x81, 1, 0xff, 0x82, 4, 'A', 'S', 'N', '1'};
  asn1::ByteReader ber(bytes), der(bytes);
  EXPECT_FALSE(gen::decode_ber(ber, value));
  EXPECT_FALSE(gen::decode_der(der, value));
  auto json = asn1::jer::parse_document("{\"count\":300,\"enabled\":true,\"text\":\"ASN1\"}");
  ASSERT_TRUE(json);
  EXPECT_FALSE(gen::decode_jer(json.value(), value));
  auto xml = asn1::xer::parse_document("<Record><count>300</count><enabled><true/></enabled><text>ASN1</text></Record>");
  ASSERT_TRUE(xml);
  EXPECT_FALSE(gen::decode_xer(xml.value(), value));
  gen::Small small{};
  const std::vector<std::uint8_t> outside{0x30, 3, 0x80, 1, 4};
  asn1::ByteReader limited(outside);
  EXPECT_FALSE(gen::decode_ber(limited, small));
}

std::vector<std::uint8_t> generated_second_tu();
TEST(GeneratedPolicy, MultipleTranslationUnitsLinkAndCallSameCodecs) {
  EXPECT_EQ(generated_second_tu(), (std::vector<std::uint8_t>{0xc0}));
}
