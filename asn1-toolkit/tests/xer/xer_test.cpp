/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/xer/xer_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising xer behaviour.
**
** Specification: See asn1-toolkit/docs/ARCHITECTURE.md for mapping to
**                 ITU-T encoding rules.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/xer/codec.hpp>
#include <asn1/runtime/xer/xml.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

void expect_xml(const asn1::xer::Element& el, const std::string& want) {
  EXPECT_EQ(asn1::xer::to_string(el), want);
}

}  // namespace

TEST(Xer, BooleanVectors) {
  auto t = asn1::xer::encode_boolean("Boolean", true);
  expect_xml(t, "<Boolean><true /></Boolean>");
  auto dt = asn1::xer::decode_boolean(t);
  ASSERT_TRUE(dt.ok());
  EXPECT_TRUE(dt.value());

  auto f = asn1::xer::encode_boolean("A", false);
  expect_xml(f, "<A><false /></A>");
  auto df = asn1::xer::decode_boolean(f);
  ASSERT_TRUE(df.ok());
  EXPECT_FALSE(df.value());
}

TEST(Xer, NullAndInteger) {
  auto n = asn1::xer::encode_null("A");
  expect_xml(n, "<A />");
  ASSERT_TRUE(asn1::xer::decode_null(n).ok());

  auto i = asn1::xer::encode_integer("A", 128);
  expect_xml(i, "<A>128</A>");
  auto di = asn1::xer::decode_integer(i);
  ASSERT_TRUE(di.ok());
  EXPECT_EQ(di.value(), 128);

  auto neg = asn1::xer::encode_integer("A", -255);
  expect_xml(neg, "<A>-255</A>");
  auto dn = asn1::xer::decode_integer(neg);
  ASSERT_TRUE(dn.ok());
  EXPECT_EQ(dn.value(), -255);
}

TEST(Xer, RealRoundTrip) {
  auto r = asn1::xer::encode_real("A", 1.5);
  auto dr = asn1::xer::decode_real(r);
  ASSERT_TRUE(dr.ok());
  EXPECT_DOUBLE_EQ(dr.value(), 1.5);

  auto inf = asn1::xer::encode_real("A", std::numeric_limits<double>::infinity());
  expect_xml(inf, "<A><PLUS-INFINITY /></A>");
  auto di = asn1::xer::decode_real(inf);
  ASSERT_TRUE(di.ok());
  EXPECT_TRUE(std::isinf(di.value()) && di.value() > 0);

  auto nan = asn1::xer::encode_real("A", std::numeric_limits<double>::quiet_NaN());
  expect_xml(nan, "<A><NOT-A-NUMBER /></A>");
  auto dn = asn1::xer::decode_real(nan);
  ASSERT_TRUE(dn.ok());
  EXPECT_TRUE(std::isnan(dn.value()));

  // Test trailing garbage rejection in REAL
  auto bad_doc = asn1::xer::parse_document("<A>1.23abc</A>");
  ASSERT_TRUE(bad_doc.ok());
  auto bad_real = asn1::xer::decode_real(bad_doc.value());
  EXPECT_FALSE(bad_real.ok());
}

TEST(Xer, OctetStringVectors) {
  auto empty = asn1::xer::encode_octet_string("A", {});
  expect_xml(empty, "<A />");

  std::uint8_t data[] = {0x01, 0x23};
  auto el = asn1::xer::encode_octet_string("A", data);
  expect_xml(el, "<A>0123</A>");
  auto got = asn1::xer::decode_octet_string(el);
  ASSERT_TRUE(got.ok());
  ASSERT_EQ(got.value().size(), 2u);
  EXPECT_EQ(got.value()[0], 0x01);
  EXPECT_EQ(got.value()[1], 0x23);

  // Odd-length hex accepted (leading zero pad), asn1tools-compatible.
  auto parsed = asn1::xer::parse_document("<A>123</A>");
  ASSERT_TRUE(parsed.ok());
  auto odd = asn1::xer::decode_octet_string(parsed.value());
  ASSERT_TRUE(odd.ok());
  ASSERT_EQ(odd.value().size(), 2u);
  EXPECT_EQ(odd.value()[0], 0x01);
  EXPECT_EQ(odd.value()[1], 0x23);
}

TEST(Xer, BitStringVectors) {
  auto empty = asn1::xer::encode_bit_string("A", {}, 0);
  expect_xml(empty, "<A />");

  std::uint8_t bits[] = {0x40};
  auto el = asn1::xer::encode_bit_string("A", bits, 4);
  expect_xml(el, "<A>0100</A>");
  auto got = asn1::xer::decode_bit_string(el);
  ASSERT_TRUE(got.ok());
  EXPECT_EQ(got.value().bit_length, 4u);
  ASSERT_FALSE(got.value().bits.empty());
  EXPECT_EQ(got.value().bits[0], 0x40);

  std::uint8_t bits9[] = {0x40, 0x80};
  auto el9 = asn1::xer::encode_bit_string("A", bits9, 9);
  expect_xml(el9, "<A>010000001</A>");
}

TEST(Xer, Utf8AndOid) {
  auto s = asn1::xer::encode_utf8_string("A", "bar");
  expect_xml(s, "<A>bar</A>");
  auto ds = asn1::xer::decode_utf8_string(s);
  ASSERT_TRUE(ds.ok());
  EXPECT_EQ(ds.value(), "bar");

  // Non-ASCII → decimal character reference.
  std::string u = "a";
  u.push_back(static_cast<char>(0xE1));
  u.push_back(static_cast<char>(0x80));
  u.push_back(static_cast<char>(0x90));  // U+1010
  u.push_back('c');
  auto su = asn1::xer::encode_utf8_string("A", u);
  expect_xml(su, "<A>a&#4112;c</A>");
  auto round = asn1::xer::parse_document(asn1::xer::to_string(su));
  ASSERT_TRUE(round.ok());
  auto du = asn1::xer::decode_utf8_string(round.value());
  ASSERT_TRUE(du.ok());
  EXPECT_EQ(du.value(), u);

  std::uint64_t arcs[] = {1, 2, 3};
  auto oid = asn1::xer::encode_object_identifier("A", arcs);
  expect_xml(oid, "<A>1.2.3</A>");
  auto doid = asn1::xer::decode_object_identifier(oid);
  ASSERT_TRUE(doid.ok());
  ASSERT_EQ(doid.value().size(), 3u);
  EXPECT_EQ(doid.value()[0], 1u);
  EXPECT_EQ(doid.value()[1], 2u);
  EXPECT_EQ(doid.value()[2], 3u);

  auto empty_oid = asn1::xer::parse_document("<A></A>");
  ASSERT_TRUE(empty_oid.ok());
  auto bad = asn1::xer::decode_object_identifier(empty_oid.value());
  ASSERT_FALSE(bad.ok());
}

TEST(Xer, EnumeratedAndChoice) {
  auto en = asn1::xer::encode_enumerated("A", "r");
  expect_xml(en, "<A><r /></A>");
  auto den = asn1::xer::decode_enumerated(en);
  ASSERT_TRUE(den.ok());
  EXPECT_EQ(den.value(), "r");

  auto alt = asn1::xer::encode_boolean("a", true);
  auto ch = asn1::xer::encode_choice("A", std::move(alt));
  expect_xml(ch, "<A><a><true /></a></A>");
  auto dch = asn1::xer::decode_choice_alternative(ch);
  ASSERT_TRUE(dch.ok());
  EXPECT_EQ(dch.value()->name, "a");
  auto db = asn1::xer::decode_boolean(*dch.value());
  ASSERT_TRUE(db.ok());
  EXPECT_TRUE(db.value());
}

TEST(Xer, SequenceAndSequenceOf) {
  auto seq = asn1::xer::make_sequence(
      "C", {asn1::xer::encode_null("a")});
  expect_xml(seq, "<C><a /></C>");

  auto sof = asn1::xer::encode_sequence_of(
      "A", {asn1::xer::encode_integer_item(1), asn1::xer::encode_integer_item(4)});
  expect_xml(sof, "<A><INTEGER>1</INTEGER><INTEGER>4</INTEGER></A>");

  auto bools = asn1::xer::encode_sequence_of(
      "C", {asn1::xer::encode_boolean_item(true), asn1::xer::encode_boolean_item(false)});
  expect_xml(bools, "<C><true /><false /></C>");
}

TEST(Xer, ParseRoundTripAndWhitespace) {
  auto parsed = asn1::xer::parse_document(
      "<A>\n  <true />\n</A>");
  ASSERT_TRUE(parsed.ok()) << parsed.error().message;
  EXPECT_EQ(parsed.value().name, "A");
  ASSERT_EQ(parsed.value().children.size(), 1u);
  EXPECT_EQ(parsed.value().children[0].name, "true");

  auto again = asn1::xer::parse_document(asn1::xer::to_string(parsed.value()));
  ASSERT_TRUE(again.ok());
  EXPECT_EQ(asn1::xer::to_string(again.value()), "<A><true /></A>");
}

TEST(Xer, MaxNestingDepthDoSProtection) {
  // Construct XML with > 256 nested elements to verify DoS prevention
  std::string deep_xml;
  for (int i = 0; i < 260; ++i) {
    deep_xml += "<node>";
  }
  deep_xml += "val";
  for (int i = 0; i < 260; ++i) {
    deep_xml += "</node>";
  }

  auto res = asn1::xer::parse_document(deep_xml);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.error().code, asn1::Error::Code::InvalidArgument);
  EXPECT_NE(res.error().message.find("nesting depth exceeded"), std::string::npos);
}

