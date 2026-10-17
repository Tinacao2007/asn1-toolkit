/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/telecom/table_constraint_ie_roundtrip_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-13
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising table constraint ie roundtrip behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "generated.hpp"

#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/per/codec.hpp>

#include <cstdint>
#include <optional>
#include <variant>

#include <gtest/gtest.h>

namespace {

// Encode then decode a HandoverIE whose typed view (decoded_value) is set.
tc_test::HandoverIE round_trip_typed(const tc_test::HandoverIE& in) {
  asn1::BitWriter w;
  auto enc = tc_test::encode_uper(w, in);
  EXPECT_TRUE(enc.ok()) << enc.error().message;
  asn1::BitReader r(w.buffer());
  tc_test::HandoverIE out{};
  auto dec = tc_test::decode_uper(r, out);
  EXPECT_TRUE(dec.ok()) << dec.error().message;
  return out;
}

}  // namespace

// Encoding with the typed view set fills the raw open-type bytes and the
// governor id from the table entry.
TEST(TableConstraintIE, TypedEncodeFillsOpenTypeBytesAndId) {
  tc_test::HandoverIE ie{};
  ie.criticality = tc_test::Criticality::ignore;
  tc_test::HandoverIE::Decoded_value typed{};
  typed.choice = tc_test::HandoverIE::Decoded_value::id_MME_UE_S1AP_ID_{
      tc_test::MME_UE_S1AP_ID{12345}};
  ie.decoded_value = std::move(typed);
  ie.value.clear();

  asn1::BitWriter w;
  auto enc = tc_test::encode_uper(w, ie);
  ASSERT_TRUE(enc.ok()) << enc.error().message;
  EXPECT_FALSE(ie.value.empty());  // open-type bytes were produced
  EXPECT_EQ(static_cast<std::uint16_t>(ie.id), 0);  // governor set from entry

  // And the produced bytes decode back into the same typed view.
  asn1::BitReader r(w.buffer());
  tc_test::HandoverIE back{};
  auto dec = tc_test::decode_uper(r, back);
  ASSERT_TRUE(dec.ok()) << dec.error().message;
  ASSERT_TRUE(back.decoded_value.has_value());
  ASSERT_TRUE(
      std::holds_alternative<tc_test::HandoverIE::Decoded_value::id_MME_UE_S1AP_ID_>(
          back.decoded_value->choice));
  const auto& typed_back =
      std::get<tc_test::HandoverIE::Decoded_value::id_MME_UE_S1AP_ID_>(
          back.decoded_value->choice);
  EXPECT_EQ(static_cast<std::uint32_t>(typed_back.value), 12345u);
  EXPECT_EQ(static_cast<std::uint16_t>(back.id), 0);
}

// Decoding dispatches on the governor id into the matching entry type.
TEST(TableConstraintIE, TypedDecodeDispatchesOnGovernorId) {
  tc_test::HandoverIE ie{};
  ie.id = 1;  // id-eNB-UE-S1AP-ID -> ENB-UE-S1AP-ID
  ie.criticality = tc_test::Criticality::reject;
  {
    asn1::BitWriter sub;
    asn1::uper::encode_integer(
        sub, 16777215, asn1::per::IntegerConstraint{0, 16777215, false});
    ie.value = sub.take();
  }

  tc_test::HandoverIE back = round_trip_typed(ie);
  ASSERT_TRUE(back.decoded_value.has_value());
  ASSERT_TRUE(
      std::holds_alternative<tc_test::HandoverIE::Decoded_value::id_eNB_UE_S1AP_ID_>(
          back.decoded_value->choice));
  const auto& typed_back =
      std::get<tc_test::HandoverIE::Decoded_value::id_eNB_UE_S1AP_ID_>(
          back.decoded_value->choice);
  EXPECT_EQ(static_cast<std::uint32_t>(typed_back.value), 16777215u);
}

// The fixed-type &criticality field carries the same {ObjectSet}{@id}
// component-relation constraint but is NOT specialized: no Decoded_ variant
// is emitted for it and it round-trips as its fixed enum.
TEST(TableConstraintIE, FixedTypeFieldIsNotSpecialized) {
  tc_test::HandoverIE ie{};
  ie.id = 0;
  ie.criticality = tc_test::Criticality::notify;
  {
    asn1::BitWriter sub;
    asn1::uper::encode_integer(
        sub, 7, asn1::per::IntegerConstraint{0, 4294967295, false});
    ie.value = sub.take();
  }

  tc_test::HandoverIE back = round_trip_typed(ie);
  EXPECT_EQ(back.criticality, tc_test::Criticality::notify);
  ASSERT_TRUE(back.decoded_value.has_value());
  ASSERT_TRUE(
      std::holds_alternative<tc_test::HandoverIE::Decoded_value::id_MME_UE_S1AP_ID_>(
          back.decoded_value->choice));
  const auto& typed_back =
      std::get<tc_test::HandoverIE::Decoded_value::id_MME_UE_S1AP_ID_>(
          back.decoded_value->choice);
  EXPECT_EQ(static_cast<std::uint32_t>(typed_back.value), 7u);
}

// An unknown governor id keeps the raw bytes and leaves the typed view unset.
TEST(TableConstraintIE, UnknownGovernorIdKeepsRawBytes) {
  tc_test::HandoverIE ie{};
  ie.id = 99;  // not in HandoverRequiredIEs
  ie.criticality = tc_test::Criticality::ignore;
  {
    asn1::BitWriter sub;
    asn1::uper::encode_integer(
        sub, 42, asn1::per::IntegerConstraint{0, 4294967295, false});
    ie.value = sub.take();
  }

  tc_test::HandoverIE back = round_trip_typed(ie);
  EXPECT_EQ(static_cast<std::uint16_t>(back.id), 99);
  EXPECT_FALSE(back.value.empty());
  EXPECT_FALSE(back.decoded_value.has_value());
}
