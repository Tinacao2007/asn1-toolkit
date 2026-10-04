/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/person_roundtrip_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising person roundtrip behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
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

template <typename PersonT>
PersonT make_person() {
  PersonT person;
  person.id = 42;
  person.name = "Ada";
  person.age = 36;
  return person;
}

template <typename PersonT>
void expect_person(const PersonT& got) {
  EXPECT_EQ(got.id, 42);
  EXPECT_EQ(got.name, "Ada");
  ASSERT_TRUE(got.age.has_value());
  EXPECT_EQ(*got.age, 36);
}

}  // namespace

TEST(Validation, PersonUperRoundTrip) {
  auto person = make_person<asn1_gen_per::Person>();
  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen_per::encode_uper(w, person).ok());
  asn1::BitReader r(w.take());
  asn1_gen_per::Person got{};
  ASSERT_TRUE(asn1_gen_per::decode_uper(r, got).ok());
  expect_person(got);
}

TEST(Validation, PersonAperRoundTrip) {
  auto person = make_person<asn1_gen_per::Person>();
  asn1::BitWriter w;
  ASSERT_TRUE(asn1_gen_per::encode_aper(w, person).ok());
  asn1::BitReader r(w.take());
  asn1_gen_per::Person got{};
  ASSERT_TRUE(asn1_gen_per::decode_aper(r, got).ok());
  expect_person(got);
}

TEST(Validation, PersonOerRoundTrip) {
  auto person = make_person<asn1_gen_oer::Person>();
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_oer::encode_oer(w, person).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_oer::Person got{};
  ASSERT_TRUE(asn1_gen_oer::decode_oer(r, got).ok());
  expect_person(got);
  EXPECT_TRUE(r.eof());
}

TEST(Validation, PersonCoerRoundTrip) {
  auto person = make_person<asn1_gen_coer::Person>();
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_coer::encode_coer(w, person).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_coer::Person got{};
  ASSERT_TRUE(asn1_gen_coer::decode_coer(r, got).ok());
  expect_person(got);
  EXPECT_TRUE(r.eof());
}

TEST(Validation, PersonBerRoundTrip) {
  auto person = make_person<asn1_gen_ber::Person>();
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_ber::encode_ber(w, person).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_ber::Person got{};
  ASSERT_TRUE(asn1_gen_ber::decode_ber(r, got).ok());
  expect_person(got);
}

TEST(Validation, PersonDerRoundTrip) {
  auto person = make_person<asn1_gen_der::Person>();
  asn1::ByteWriter w;
  ASSERT_TRUE(asn1_gen_der::encode_der(w, person).ok());
  asn1::ByteReader r(w.buffer());
  asn1_gen_der::Person got{};
  ASSERT_TRUE(asn1_gen_der::decode_der(r, got).ok());
  expect_person(got);
}

TEST(Validation, PersonJerRoundTrip) {
  auto person = make_person<asn1_gen_jer::Person>();
  auto enc = asn1_gen_jer::encode_jer(person);
  ASSERT_TRUE(enc.ok()) << enc.error().message;
  asn1_gen_jer::Person got{};
  auto dec = asn1_gen_jer::decode_jer(enc.value(), got);
  ASSERT_TRUE(dec.ok()) << dec.error().message;
  expect_person(got);
}
