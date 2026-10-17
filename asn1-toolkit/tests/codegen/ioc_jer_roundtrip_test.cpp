/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/ioc_jer_roundtrip_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising ioc jer roundtrip behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "generated.hpp"

#include <asn1/runtime/bigint.hpp>
#include <asn1/runtime/jer/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(IocJer, ErrorPduOpenTypeRoundTrip) {
  asn1_ioc_jer::ErrorPDU pdu;
  pdu.code = asn1::BigInteger::from_i64(1);
  pdu.param = {0xDE, 0xAD, 0xBE, 0xEF};

  auto enc = asn1_ioc_jer::encode_jer(pdu);
  ASSERT_TRUE(enc.ok()) << enc.error().message;

  // Object-class &Value open types lower to a byte vector; JER encodes them
  // as hex OCTET STRING values.
  EXPECT_EQ(asn1::jer::to_string(enc.value()),
            "{\"code\":1,\"param\":\"DEADBEEF\"}");

  asn1_ioc_jer::ErrorPDU got{};
  auto dec = asn1_ioc_jer::decode_jer(enc.value(), got);
  ASSERT_TRUE(dec.ok()) << dec.error().message;
  EXPECT_TRUE(got.code == pdu.code);
  EXPECT_EQ(got.param, pdu.param);
}

TEST(IocJer, ErrorPduEmptyOpenTypeRoundTrip) {
  asn1_ioc_jer::ErrorPDU pdu;
  pdu.code = asn1::BigInteger::from_i64(2);
  pdu.param = {};

  auto enc = asn1_ioc_jer::encode_jer(pdu);
  ASSERT_TRUE(enc.ok()) << enc.error().message;
  EXPECT_EQ(asn1::jer::to_string(enc.value()), "{\"code\":2,\"param\":\"\"}");

  asn1_ioc_jer::ErrorPDU got{};
  auto dec = asn1_ioc_jer::decode_jer(enc.value(), got);
  ASSERT_TRUE(dec.ok()) << dec.error().message;
  EXPECT_TRUE(got.code == pdu.code);
  EXPECT_TRUE(got.param.empty());
}

TEST(IocJer, OpenAsInstanceOfRoundTrip) {
  asn1_ioc_jer::OpenAS value;
  value.type_id = {2, 1, 1};
  value.value = {0x01, 0x02};

  auto enc = asn1_ioc_jer::encode_jer(value);
  ASSERT_TRUE(enc.ok()) << enc.error().message;
  EXPECT_EQ(asn1::jer::to_string(enc.value()),
            "[\"2.1.1\",\"0102\"]");

  asn1_ioc_jer::OpenAS got{};
  auto dec = asn1_ioc_jer::decode_jer(enc.value(), got);
  ASSERT_TRUE(dec.ok()) << dec.error().message;
  EXPECT_EQ(got.type_id, value.type_id);
  EXPECT_EQ(got.value, value.value);
}

TEST(IocJer, DecodeRejectsShortInstanceOfArray) {
  const auto parsed = asn1::jer::parse_document("[\"2.1.1\"]");
  ASSERT_TRUE(parsed.ok()) << parsed.error().message;

  asn1_ioc_jer::OpenAS got{};
  auto dec = asn1_ioc_jer::decode_jer(parsed.value(), got);
  EXPECT_FALSE(dec.ok());
}
