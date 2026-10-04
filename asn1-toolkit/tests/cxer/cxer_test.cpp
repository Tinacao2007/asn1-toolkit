/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/cxer/cxer_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising cxer behaviour.
**
** Specification: See asn1-toolkit/docs/ARCHITECTURE.md for mapping to
**                 ITU-T encoding rules.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/cxer/codec.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

void expect_xml(const asn1::cxer::Element& el, const std::string& want) {
  EXPECT_EQ(asn1::cxer::to_string(el), want);
}

}  // namespace

TEST(Cxer, BooleanCanonicalForm) {
  auto t = asn1::cxer::encode_boolean("Boolean", true);
  expect_xml(t, "<Boolean><true/></Boolean>");
  auto parsed = asn1::cxer::parse_document("<Boolean><true/></Boolean>");
  ASSERT_TRUE(parsed.ok()) << parsed.error().message;
  auto dt = asn1::cxer::decode_boolean(parsed.value());
  ASSERT_TRUE(dt.ok());
  EXPECT_TRUE(dt.value());

  // BASIC-XER spacing / whitespace is non-canonical.
  auto spaced = asn1::cxer::parse_document("<Boolean><true /></Boolean>");
  ASSERT_FALSE(spaced.ok());
  EXPECT_EQ(spaced.error().code, asn1::Error::Code::NonCanonical);

  auto pretty = asn1::cxer::parse_document("<Boolean>\n  <true/>\n</Boolean>");
  ASSERT_FALSE(pretty.ok());
  EXPECT_EQ(pretty.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Cxer, NullAndInteger) {
  expect_xml(asn1::cxer::encode_null("A"), "<A/>");
  auto n = asn1::cxer::parse_document("<A/>");
  ASSERT_TRUE(n.ok());
  ASSERT_TRUE(asn1::cxer::decode_null(n.value()).ok());

  // Empty pair form forbidden when empty-element is required.
  auto pair = asn1::cxer::parse_document("<A></A>");
  ASSERT_FALSE(pair.ok());
  EXPECT_EQ(pair.error().code, asn1::Error::Code::NonCanonical);

  expect_xml(asn1::cxer::encode_integer("A", 128), "<A>128</A>");
  expect_xml(asn1::cxer::encode_integer("A", -255), "<A>-255</A>");
  expect_xml(asn1::cxer::encode_integer("A", 0), "<A>0</A>");

  auto lead = asn1::cxer::parse_document("<A>0128</A>");
  ASSERT_TRUE(lead.ok());  // document shape can round-trip as text
  auto bad = asn1::cxer::decode_integer(lead.value());
  ASSERT_FALSE(bad.ok());
  EXPECT_EQ(bad.error().code, asn1::Error::Code::NonCanonical);

  auto plus = asn1::cxer::parse_document("<A>+1</A>");
  ASSERT_TRUE(plus.ok());
  auto bad_plus = asn1::cxer::decode_integer(plus.value());
  ASSERT_FALSE(bad_plus.ok());
  EXPECT_EQ(bad_plus.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Cxer, OctetStringUppercaseEven) {
  std::uint8_t data[] = {0x01, 0x23};
  expect_xml(asn1::cxer::encode_octet_string("A", data), "<A>0123</A>");

  auto lower = asn1::cxer::parse_document("<A>0123</A>");
  ASSERT_TRUE(lower.ok());
  auto ok = asn1::cxer::decode_octet_string(lower.value());
  ASSERT_TRUE(ok.ok());

  auto bad_case = asn1::cxer::parse_document("<A>ab</A>");
  ASSERT_TRUE(bad_case.ok());
  auto dec = asn1::cxer::decode_octet_string(bad_case.value());
  ASSERT_FALSE(dec.ok());
  EXPECT_EQ(dec.error().code, asn1::Error::Code::NonCanonical);

  auto odd = asn1::cxer::parse_document("<A>123</A>");
  ASSERT_TRUE(odd.ok());
  auto odd_dec = asn1::cxer::decode_octet_string(odd.value());
  ASSERT_FALSE(odd_dec.ok());
  EXPECT_EQ(odd_dec.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Cxer, BitStringAndUtf8Raw) {
  std::uint8_t bits[] = {0x40};
  expect_xml(asn1::cxer::encode_bit_string("A", bits, 4), "<A>0100</A>");

  // Non-ASCII must appear as raw UTF-8, not decimal NCR.
  std::string u = "a";
  u.push_back(static_cast<char>(0xE1));
  u.push_back(static_cast<char>(0x80));
  u.push_back(static_cast<char>(0x90));  // U+1010
  u.push_back('c');
  auto el = asn1::cxer::encode_utf8_string("A", u);
  const std::string xml = asn1::cxer::to_string(el);
  EXPECT_EQ(xml.find("&#"), std::string::npos);
  EXPECT_NE(xml.find(u), std::string::npos);

  auto round = asn1::cxer::parse_document(xml);
  ASSERT_TRUE(round.ok()) << round.error().message;
  auto got = asn1::cxer::decode_utf8_string(round.value());
  ASSERT_TRUE(got.ok());
  EXPECT_EQ(got.value(), u);

  // NCR form is BASIC-XER-ok but not CXER.
  auto ncr = asn1::cxer::parse_document("<A>a&#4112;c</A>");
  ASSERT_FALSE(ncr.ok());
  EXPECT_EQ(ncr.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Cxer, EnumeratedChoiceSequence) {
  expect_xml(asn1::cxer::encode_enumerated("A", "r"), "<A><r/></A>");
  auto ch = asn1::cxer::encode_choice("A", asn1::cxer::encode_boolean("a", true));
  expect_xml(ch, "<A><a><true/></a></A>");

  auto sof = asn1::cxer::encode_sequence_of(
      "A", {asn1::cxer::encode_integer_item(1), asn1::cxer::encode_integer_item(4)});
  expect_xml(sof, "<A><INTEGER>1</INTEGER><INTEGER>4</INTEGER></A>");
}

TEST(Cxer, SetOfSortsByEncoding) {
  std::vector<asn1::cxer::Element> items = {
      asn1::cxer::encode_integer_item(3),
      asn1::cxer::encode_integer_item(1),
      asn1::cxer::encode_integer_item(2),
  };
  auto set = asn1::cxer::encode_set_of("S", items);
  expect_xml(set, "<S><INTEGER>1</INTEGER><INTEGER>2</INTEGER><INTEGER>3</INTEGER></S>");

  std::vector<asn1::cxer::Element> ordered = {
      asn1::cxer::encode_integer_item(1),
      asn1::cxer::encode_integer_item(2),
  };
  ASSERT_TRUE(asn1::cxer::require_set_of_order(ordered).ok());

  std::vector<asn1::cxer::Element> unordered = {
      asn1::cxer::encode_integer_item(2),
      asn1::cxer::encode_integer_item(1),
  };
  auto bad = asn1::cxer::require_set_of_order(unordered);
  ASSERT_FALSE(bad.ok());
  EXPECT_EQ(bad.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Cxer, RejectXmlProlog) {
  auto bad = asn1::cxer::parse_document("<?xml version=\"1.0\"?><A/>");
  ASSERT_FALSE(bad.ok());
  EXPECT_EQ(bad.error().code, asn1::Error::Code::NonCanonical);
}

TEST(Cxer, ObjectIdentifier) {
  std::uint64_t arcs[] = {1, 2, 840};
  expect_xml(asn1::cxer::encode_object_identifier("A", arcs), "<A>1.2.840</A>");
  auto el = asn1::cxer::parse_document("<A>1.2.840</A>");
  ASSERT_TRUE(el.ok());
  auto oid = asn1::cxer::decode_object_identifier(el.value());
  ASSERT_TRUE(oid.ok());
  ASSERT_EQ(oid.value().size(), 3u);
}
