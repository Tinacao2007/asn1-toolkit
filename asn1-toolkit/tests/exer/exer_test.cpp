/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/exer/exer_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising exer behaviour.
**
** Specification: See asn1-toolkit/docs/ARCHITECTURE.md for mapping to
**                 ITU-T encoding rules.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/exer/codec.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

void expect_xml(const asn1::exer::Element& el, const std::string& want) {
  EXPECT_EQ(asn1::exer::to_string(el), want);
}

}  // namespace

TEST(Exer, AttributeOnSequence) {
  // Product { color [ATTRIBUTE], available [ATTRIBUTE], name, price }
  asn1::exer::Element product;
  product.name = "Product";
  asn1::exer::encode_attribute_string(product, "color", "red");
  asn1::exer::encode_attribute_boolean(product, "available", false);
  product.children.push_back(asn1::exer::encode_utf8_string("name", "Shoes"));
  product.children.push_back(asn1::exer::encode_integer("price", 25));

  expect_xml(product,
             "<Product color=\"red\" available=\"false\"><name>Shoes</name><price>25</price></Product>");

  auto parsed = asn1::exer::parse_document(asn1::exer::to_string(product));
  ASSERT_TRUE(parsed.ok()) << parsed.error().message;
  auto color = asn1::exer::get_attribute(parsed.value(), "color");
  ASSERT_TRUE(color.ok());
  EXPECT_EQ(color.value(), "red");
  auto avail = asn1::exer::decode_attribute_boolean(parsed.value(), "available");
  ASSERT_TRUE(avail.ok());
  EXPECT_FALSE(avail.value());
}

TEST(Exer, Base64OctetString) {
  std::uint8_t data[] = {0x01, 0x23, 0x45};
  auto el = asn1::exer::encode_octet_string_base64("A", data);
  // 012345 → AQNF
  expect_xml(el, "<A>ASNF</A>");
  auto got = asn1::exer::decode_octet_string_base64(el);
  ASSERT_TRUE(got.ok()) << got.error().message;
  ASSERT_EQ(got.value().size(), 3u);
  EXPECT_EQ(got.value()[0], 0x01);
  EXPECT_EQ(got.value()[1], 0x23);
  EXPECT_EQ(got.value()[2], 0x45);

  // Empty
  expect_xml(asn1::exer::encode_octet_string_base64("A", {}), "<A />");
}

TEST(Exer, TextBooleanAndEnumerated) {
  expect_xml(asn1::exer::encode_boolean_text("flag", true), "<flag>true</flag>");
  expect_xml(asn1::exer::encode_boolean_text("flag", false), "<flag>false</flag>");
  auto el = asn1::exer::parse_document("<flag>true</flag>");
  ASSERT_TRUE(el.ok());
  auto v = asn1::exer::decode_boolean_text(el.value());
  ASSERT_TRUE(v.ok());
  EXPECT_TRUE(v.value());

  expect_xml(asn1::exer::encode_enumerated_text("Color", "red"), "<Color>red</Color>");
  auto en = asn1::exer::decode_enumerated_text(
      asn1::exer::encode_enumerated_text("Color", "blue"));
  ASSERT_TRUE(en.ok());
  EXPECT_EQ(en.value(), "blue");
}

TEST(Exer, UseNumber) {
  expect_xml(asn1::exer::encode_enumerated_number("A", 5), "<A>5</A>");
  auto v = asn1::exer::decode_enumerated_number(
      asn1::exer::encode_enumerated_number("A", -2));
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), -2);
}

TEST(Exer, ListOfStringsAndIntegers) {
  expect_xml(asn1::exer::encode_list_of_strings("colors", {"red", "blue", "green"}),
             "<colors>red blue green</colors>");
  auto parts = asn1::exer::decode_list_of_strings(
      asn1::exer::encode_list_of_strings("colors", {"a", "b"}));
  ASSERT_TRUE(parts.ok());
  ASSERT_EQ(parts.value().size(), 2u);
  EXPECT_EQ(parts.value()[0], "a");
  EXPECT_EQ(parts.value()[1], "b");

  expect_xml(asn1::exer::encode_list_of_integers("nums", {1, 4, -3}),
             "<nums>1 4 -3</nums>");
  auto nums = asn1::exer::decode_list_of_integers(
      asn1::exer::encode_list_of_integers("nums", {10, 20}));
  ASSERT_TRUE(nums.ok());
  ASSERT_EQ(nums.value().size(), 2u);
  EXPECT_EQ(nums.value()[0], 10);
  EXPECT_EQ(nums.value()[1], 20);
}

TEST(Exer, ListOfBooleansOidsRealsOctetsBits) {
  expect_xml(asn1::exer::encode_list_of_booleans("flags", {1, 0, 1}),
             "<flags>true false true</flags>");
  auto flags = asn1::exer::decode_list_of_booleans(
      asn1::exer::encode_list_of_booleans("flags", {0, 1}));
  ASSERT_TRUE(flags.ok());
  ASSERT_EQ(flags.value().size(), 2u);
  EXPECT_EQ(flags.value()[0], 0);
  EXPECT_EQ(flags.value()[1], 1);

  std::vector<std::vector<std::uint64_t>> oids = {{1, 2, 3}, {1, 2, 840}};
  expect_xml(asn1::exer::encode_list_of_object_identifiers("ids", oids),
             "<ids>1.2.3 1.2.840</ids>");
  auto got_oids = asn1::exer::decode_list_of_object_identifiers(
      asn1::exer::encode_list_of_object_identifiers("ids", oids));
  ASSERT_TRUE(got_oids.ok());
  ASSERT_EQ(got_oids.value().size(), 2u);
  EXPECT_EQ(got_oids.value()[0], oids[0]);
  EXPECT_EQ(got_oids.value()[1], oids[1]);

  auto reals = asn1::exer::decode_list_of_reals(
      asn1::exer::encode_list_of_reals("rs", {1.5, 2.0}));
  ASSERT_TRUE(reals.ok());
  ASSERT_EQ(reals.value().size(), 2u);
  EXPECT_DOUBLE_EQ(reals.value()[0], 1.5);
  EXPECT_DOUBLE_EQ(reals.value()[1], 2.0);

  std::vector<std::vector<std::uint8_t>> octs = {{0x0A, 0x0B}, {0xFF}};
  expect_xml(asn1::exer::encode_list_of_octet_strings("os", octs),
             "<os>0A0B FF</os>");
  auto got_octs = asn1::exer::decode_list_of_octet_strings(
      asn1::exer::encode_list_of_octet_strings("os", octs));
  ASSERT_TRUE(got_octs.ok());
  ASSERT_EQ(got_octs.value().size(), 2u);
  EXPECT_EQ(got_octs.value()[0], octs[0]);
  EXPECT_EQ(got_octs.value()[1], octs[1]);

  std::vector<asn1::xer::BitStringValue> bits = {
      {{0xA0}, 4},
      {{0xFF}, 8},
  };
  expect_xml(asn1::exer::encode_list_of_bit_strings("bs", bits),
             "<bs>1010 11111111</bs>");
  auto got_bits = asn1::exer::decode_list_of_bit_strings(
      asn1::exer::encode_list_of_bit_strings("bs", bits));
  ASSERT_TRUE(got_bits.ok());
  ASSERT_EQ(got_bits.value().size(), 2u);
  EXPECT_EQ(got_bits.value()[0].bit_length, 4u);
  EXPECT_EQ(got_bits.value()[1].bit_length, 8u);
}

TEST(Exer, UntaggedMergesIntoParent) {
  asn1::exer::Element parent;
  parent.name = "Outer";
  asn1::exer::Element frag;
  frag.name = "Inner";  // wrapper discarded conceptually
  asn1::exer::encode_attribute_string(frag, "id", "7");
  frag.children.push_back(asn1::exer::encode_integer("x", 1));
  frag.children.push_back(asn1::exer::encode_integer("y", 2));
  asn1::exer::append_untagged(parent, std::move(frag));

  expect_xml(parent, "<Outer id=\"7\"><x>1</x><y>2</y></Outer>");
}

TEST(Exer, UseNil) {
  asn1::exer::Element el;
  el.name = "opt";
  el.children.push_back(asn1::exer::encode_integer("a", 1));
  asn1::exer::set_nil(el, true);
  EXPECT_TRUE(el.children.empty());
  auto nil = asn1::exer::is_nil(el);
  ASSERT_TRUE(nil.ok());
  EXPECT_TRUE(nil.value());
  EXPECT_NE(asn1::exer::to_string(el).find("xsi:nil=\"true\""), std::string::npos);
}

TEST(Exer, NameRename) {
  auto el = asn1::exer::encode_integer("old", 3);
  asn1::exer::set_name(el, "new");
  expect_xml(el, "<new>3</new>");
}

TEST(Exer, AttributeRoundTripWithEscapes) {
  asn1::exer::Element el;
  el.name = "E";
  asn1::exer::set_attribute(el, "note", "a&b\"c");
  const std::string xml = asn1::exer::to_string(el);
  EXPECT_NE(xml.find("&amp;"), std::string::npos);
  EXPECT_NE(xml.find("&quot;"), std::string::npos);
  auto parsed = asn1::exer::parse_document(xml);
  ASSERT_TRUE(parsed.ok()) << parsed.error().message;
  auto note = asn1::exer::get_attribute(parsed.value(), "note");
  ASSERT_TRUE(note.ok());
  EXPECT_EQ(note.value(), "a&b\"c");
}
