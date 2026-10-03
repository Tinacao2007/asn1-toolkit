#include <asn1/runtime/jeri/codec.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

void expect_json(const asn1::jeri::Value& v, const std::string& want) {
  EXPECT_EQ(asn1::jeri::to_string(v), want);
}

}  // namespace

TEST(Jeri, Base64OctetString) {
  // OSS / X.697 example fragment: '0102030405FFEE88AACC'H
  std::uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xEE, 0x88, 0xAA, 0xCC};
  expect_json(asn1::jeri::encode_octet_string_base64(data), "\"AQIDBAX/7oiqzA==\"");
  auto got = asn1::jeri::decode_octet_string_base64(
      asn1::jeri::encode_octet_string_base64(data));
  ASSERT_TRUE(got.ok()) << got.error().message;
  ASSERT_EQ(got.value().size(), 10u);
  EXPECT_EQ(got.value()[0], 0x01);
  EXPECT_EQ(got.value()[5], 0xFF);
  EXPECT_EQ(got.value()[9], 0xCC);

  expect_json(asn1::jeri::encode_octet_string_base64({}), "\"\"");
}

TEST(Jeri, ArraySequence) {
  // A2 ::= [ARRAY] A  →  [1, 2, 3, "AQIDBAX/7oiqzA==", null] with trailing omit
  std::uint8_t oct[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xEE, 0x88, 0xAA, 0xCC};
  auto enc = asn1::jeri::encode_sequence_array({
      asn1::jeri::encode_integer(1),
      asn1::jeri::encode_integer(2),
      asn1::jeri::encode_integer(3),
      asn1::jeri::encode_octet_string_base64(oct),
      asn1::jeri::Value::null_value(),  // absent OPTIONAL
  });
  expect_json(enc, "[1,2,3,\"AQIDBAX/7oiqzA==\"]");

  auto with_null = asn1::jeri::encode_sequence_array(
      {
          asn1::jeri::encode_integer(1),
          asn1::jeri::Value::null_value(),
      },
      /*omit_trailing_nulls=*/false);
  expect_json(with_null, "[1,null]");

  auto mid_null = asn1::jeri::encode_sequence_array({
      asn1::jeri::encode_integer(1),
      asn1::jeri::Value::null_value(),
      asn1::jeri::encode_integer(3),
  });
  expect_json(mid_null, "[1,null,3]");

  auto arr = asn1::jeri::decode_sequence_array(enc);
  ASSERT_TRUE(arr.ok());
  ASSERT_EQ(arr.value()->size(), 4u);
}

TEST(Jeri, NameOnSequenceMember) {
  auto seq = asn1::jeri::make_sequence({
      asn1::jeri::named_member("a1", asn1::jeri::encode_integer(1),
                               asn1::jeri::NameForm::AsIs),
      asn1::jeri::named_member("a2", asn1::jeri::encode_integer(2),
                               asn1::jeri::NameForm::Literal, "_1/ (2@3&"),
      asn1::jeri::named_member("color", asn1::jeri::encode_utf8_string("red"),
                               asn1::jeri::NameForm::Capitalized),
  });
  expect_json(seq, "{\"a1\":1,\"_1/ (2@3&\":2,\"Color\":\"red\"}");

  EXPECT_EQ(asn1::jeri::transform_name("fooBar", asn1::jeri::NameForm::Uppercased),
            "FOOBAR");
  EXPECT_EQ(asn1::jeri::transform_name("FooBar", asn1::jeri::NameForm::Lowercased),
            "foobar");
}

TEST(Jeri, ObjectSetOf) {
  auto obj = asn1::jeri::encode_set_of_object({
      {"one", asn1::jeri::encode_integer(551)},
      {"two", asn1::jeri::encode_integer(1615)},
  });
  expect_json(obj, "{\"one\":551,\"two\":1615}");

  auto entries = asn1::jeri::decode_set_of_object(obj);
  ASSERT_TRUE(entries.ok());
  ASSERT_EQ(entries.value().size(), 2u);
  EXPECT_EQ(entries.value()[0].first, "one");
  auto v0 = asn1::jeri::decode_integer(*entries.value()[0].second);
  ASSERT_TRUE(v0.ok());
  EXPECT_EQ(v0.value(), 551);
}

TEST(Jeri, TextEnumerated) {
  expect_json(asn1::jeri::encode_enumerated_text("b", asn1::jeri::NameForm::Capitalized),
              "\"B\"");
  expect_json(asn1::jeri::encode_enumerated_text("b", asn1::jeri::NameForm::Uppercased),
              "\"B\"");
  expect_json(
      asn1::jeri::encode_enumerated_text("ready", asn1::jeri::NameForm::Literal, "READY"),
      "\"READY\"");

  std::vector<std::string> ids = {"a", "b", "c", "d", "e"};
  auto sof = asn1::jeri::encode_sequence_of({
      asn1::jeri::encode_enumerated_text("b", asn1::jeri::NameForm::Capitalized),
      asn1::jeri::encode_enumerated_text("c", asn1::jeri::NameForm::Capitalized),
      asn1::jeri::encode_enumerated_text("d", asn1::jeri::NameForm::Capitalized),
      asn1::jeri::encode_enumerated_text("e", asn1::jeri::NameForm::Capitalized),
  });
  expect_json(sof, "[\"B\",\"C\",\"D\",\"E\"]");

  auto parsed = asn1::jeri::parse_document("\"B\"");
  ASSERT_TRUE(parsed.ok());
  auto id = asn1::jeri::decode_enumerated_text(parsed.value(), ids,
                                               asn1::jeri::NameForm::Capitalized);
  ASSERT_TRUE(id.ok()) << id.error().message;
  EXPECT_EQ(id.value(), "b");
}

TEST(Jeri, UnwrappedChoice) {
  // C ::= [UNWRAPPED] CHOICE { c1 BOOLEAN, c2 SEQUENCE OF ENUMERATED ... }
  // c ::= c2 : { b, c, d, e }  →  ["B","C","D","E"]  (with TEXT ALL AS CAPITALIZED)
  auto inner = asn1::jeri::encode_sequence_of({
      asn1::jeri::encode_enumerated_text("b", asn1::jeri::NameForm::Capitalized),
      asn1::jeri::encode_enumerated_text("c", asn1::jeri::NameForm::Capitalized),
      asn1::jeri::encode_enumerated_text("d", asn1::jeri::NameForm::Capitalized),
      asn1::jeri::encode_enumerated_text("e", asn1::jeri::NameForm::Capitalized),
  });
  auto ch = asn1::jeri::encode_choice_unwrapped(std::move(inner));
  expect_json(ch, "[\"B\",\"C\",\"D\",\"E\"]");

  // Contrast: wrapped CHOICE would be {"c2":[...]}
  auto wrapped = asn1::jeri::encode_choice(
      "c2", asn1::jeri::encode_sequence_of({asn1::jeri::encode_enumerated("b")}));
  expect_json(wrapped, "{\"c2\":[\"b\"]}");

  auto u = asn1::jeri::decode_choice_unwrapped(ch);
  ASSERT_TRUE(u.ok());
  EXPECT_EQ(u.value()->kind, asn1::jeri::ValueKind::Array);
}

TEST(Jeri, CombinedOssExampleObject) {
  // Object form of A with NAME + BASE64 (absent a5 omitted, not null)
  std::uint8_t oct[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xEE, 0x88, 0xAA, 0xCC};
  auto a = asn1::jeri::make_sequence({
      {"a1", asn1::jeri::encode_integer(1)},
      asn1::jeri::named_member("a2", asn1::jeri::encode_integer(2),
                               asn1::jeri::NameForm::Literal, "_1/ (2@3&"),
      {"a3", asn1::jeri::encode_integer(3)},
      {"a4", asn1::jeri::encode_octet_string_base64(oct)},
  });
  expect_json(a, "{\"a1\":1,\"_1/ (2@3&\":2,\"a3\":3,\"a4\":\"AQIDBAX/7oiqzA==\"}");
}
