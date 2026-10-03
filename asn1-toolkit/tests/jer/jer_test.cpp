#include <asn1/runtime/jer/codec.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

void expect_json(const asn1::jer::Value& v, const std::string& want) {
  EXPECT_EQ(asn1::jer::to_string(v), want);
}

}  // namespace

TEST(Jer, BooleanNullInteger) {
  expect_json(asn1::jer::encode_boolean(true), "true");
  expect_json(asn1::jer::encode_boolean(false), "false");
  auto tb = asn1::jer::decode_boolean(asn1::jer::encode_boolean(true));
  ASSERT_TRUE(tb.ok());
  EXPECT_TRUE(tb.value());

  expect_json(asn1::jer::encode_null(), "null");
  ASSERT_TRUE(asn1::jer::decode_null(asn1::jer::encode_null()).ok());

  expect_json(asn1::jer::encode_integer(128), "128");
  expect_json(asn1::jer::encode_integer(-255), "-255");
  auto ni = asn1::jer::decode_integer(asn1::jer::encode_integer(-255));
  ASSERT_TRUE(ni.ok());
  EXPECT_EQ(ni.value(), -255);
}

TEST(Jer, OctetAndBitString) {
  std::uint8_t data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  expect_json(asn1::jer::encode_octet_string(data), "\"0123456789ABCDEF\"");
  auto od = asn1::jer::decode_octet_string(asn1::jer::encode_octet_string(data));
  ASSERT_TRUE(od.ok());
  ASSERT_EQ(od.value().size(), 8u);
  EXPECT_EQ(od.value()[0], 0x01);
  EXPECT_EQ(od.value()[7], 0xEF);

  std::uint8_t bits[] = {0x40};
  expect_json(asn1::jer::encode_bit_string(bits, 4),
              "{\"value\":\"40\",\"length\":4}");
  auto bd = asn1::jer::decode_bit_string(asn1::jer::encode_bit_string(bits, 4));
  ASSERT_TRUE(bd.ok());
  EXPECT_EQ(bd.value().bit_length, 4u);
  ASSERT_FALSE(bd.value().bits.empty());
  EXPECT_EQ(bd.value().bits[0], 0x40);

  std::uint8_t fixed[] = {0xAC};
  expect_json(asn1::jer::encode_bit_string(fixed, 6, /*fixed_size=*/true), "\"AC\"");
}

TEST(Jer, Utf8EnumeratedOid) {
  expect_json(asn1::jer::encode_utf8_string("bar"), "\"bar\"");
  expect_json(asn1::jer::encode_utf8_string(""), "\"\"");
  auto s = asn1::jer::decode_utf8_string(asn1::jer::encode_utf8_string("hi"));
  ASSERT_TRUE(s.ok());
  EXPECT_EQ(s.value(), "hi");

  expect_json(asn1::jer::encode_enumerated("a"), "\"a\"");
  auto e = asn1::jer::decode_enumerated(asn1::jer::encode_enumerated("b"));
  ASSERT_TRUE(e.ok());
  EXPECT_EQ(e.value(), "b");

  std::uint64_t arcs[] = {1, 2, 3};
  expect_json(asn1::jer::encode_object_identifier(arcs), "\"1.2.3\"");
  auto oid = asn1::jer::decode_object_identifier(asn1::jer::encode_object_identifier(arcs));
  ASSERT_TRUE(oid.ok());
  ASSERT_EQ(oid.value().size(), 3u);
  EXPECT_EQ(oid.value()[2], 3u);
}

TEST(Jer, SequenceChoiceSequenceOf) {
  auto seq = asn1::jer::make_sequence({
      {"a", asn1::jer::encode_integer(1)},
      {"c", asn1::jer::encode_integer(2)},
      {"d", asn1::jer::encode_boolean(true)},
  });
  expect_json(seq, "{\"a\":1,\"c\":2,\"d\":true}");
  auto member = asn1::jer::find_member(seq, "c");
  ASSERT_TRUE(member.ok());
  auto cv = asn1::jer::decode_integer(*member.value());
  ASSERT_TRUE(cv.ok());
  EXPECT_EQ(cv.value(), 2);

  auto ch = asn1::jer::encode_choice("a", asn1::jer::encode_boolean(true));
  expect_json(ch, "{\"a\":true}");
  auto dch = asn1::jer::decode_choice(ch);
  ASSERT_TRUE(dch.ok());
  EXPECT_EQ(dch.value().first, "a");
  auto b = asn1::jer::decode_boolean(*dch.value().second);
  ASSERT_TRUE(b.ok());
  EXPECT_TRUE(b.value());

  auto sof = asn1::jer::encode_sequence_of(
      {asn1::jer::encode_integer(1), asn1::jer::encode_integer(3)});
  expect_json(sof, "[1,3]");
  // asn1tools uses "[1, 3]" with space after comma — both parse; we emit compact.
  auto parsed = asn1::jer::parse_document("[1, 3]");
  ASSERT_TRUE(parsed.ok());
  auto arr = asn1::jer::decode_sequence_of(parsed.value());
  ASSERT_TRUE(arr.ok());
  ASSERT_EQ(arr.value()->size(), 2u);
}

TEST(Jer, NestedSequenceAndWhitespaceParse) {
  auto nested = asn1::jer::make_sequence({
      {"a", asn1::jer::encode_sequence_of({asn1::jer::make_sequence({})})},
  });
  expect_json(nested, "{\"a\":[{}]}");

  auto pretty = asn1::jer::parse_document("{\n  \"id\": 1,\n  \"answer\": false\n}");
  ASSERT_TRUE(pretty.ok()) << pretty.error().message;
  EXPECT_EQ(asn1::jer::to_string(pretty.value()), "{\"id\":1,\"answer\":false}");
}

TEST(Jer, EmptyContainers) {
  expect_json(asn1::jer::make_sequence({}), "{}");
  expect_json(asn1::jer::encode_sequence_of({}), "[]");
}

TEST(Jer, RealRoundTrip) {
  expect_json(asn1::jer::encode_real(1.5), "1.5");
  auto d = asn1::jer::decode_real(asn1::jer::encode_real(1.5));
  ASSERT_TRUE(d.ok());
  EXPECT_DOUBLE_EQ(d.value(), 1.5);

  expect_json(asn1::jer::encode_real(std::numeric_limits<double>::infinity()), "\"INF\"");
  expect_json(asn1::jer::encode_real(-std::numeric_limits<double>::infinity()), "\"-INF\"");
  expect_json(asn1::jer::encode_real(std::numeric_limits<double>::quiet_NaN()), "\"NaN\"");

  auto inf = asn1::jer::decode_real(asn1::jer::encode_real(std::numeric_limits<double>::infinity()));
  ASSERT_TRUE(inf.ok());
  EXPECT_TRUE(std::isinf(inf.value()) && inf.value() > 0);

  auto parsed = asn1::jer::parse_document("2.5e1");
  ASSERT_TRUE(parsed.ok()) << parsed.error().message;
  auto r = asn1::jer::decode_real(parsed.value());
  ASSERT_TRUE(r.ok());
  EXPECT_DOUBLE_EQ(r.value(), 25.0);
}
