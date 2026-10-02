#include <asn1/runtime/uper.hpp>
#include <asn1/runtime/per/codec.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> hex(std::initializer_list<std::uint8_t> bytes) {
  return std::vector<std::uint8_t>(bytes);
}

void expect_bytes(const std::vector<std::uint8_t>& got,
                  const std::vector<std::uint8_t>& want) {
  ASSERT_EQ(got.size(), want.size()) << "size mismatch";
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

std::vector<std::uint8_t> finish(asn1::BitWriter& w) { return w.take(); }

}  // namespace

TEST(Uper, BooleanVectors) {
  {
    asn1::BitWriter w;
    asn1::uper::encode_boolean(w, true);
    expect_bytes(finish(w), hex({0x80}));
    const auto _bits1 = hex({0x80});
    asn1::BitReader r(_bits1);
    auto v = asn1::uper::decode_boolean(r);
    ASSERT_TRUE(v.ok());
    EXPECT_TRUE(v.value());
  }
  {
    asn1::BitWriter w;
    asn1::uper::encode_boolean(w, false);
    expect_bytes(finish(w), hex({0x00}));
  }
}

TEST(Uper, IntegerUnconstrainedVectors) {
  struct Case {
    std::int64_t value;
    std::vector<std::uint8_t> encoding;
  };
  const Case cases[] = {
      {0, hex({0x01, 0x00})},
      {1, hex({0x01, 0x01})},
      {127, hex({0x01, 0x7F})},
      {128, hex({0x02, 0x00, 0x80})},
      {-1, hex({0x01, 0xFF})},
      {-128, hex({0x01, 0x80})},
      {-129, hex({0x02, 0xFF, 0x7F})},
  };
  for (const auto& c : cases) {
    asn1::BitWriter w;
    asn1::uper::encode_integer(w, c.value);
    expect_bytes(finish(w), c.encoding);
    asn1::BitReader r(c.encoding);
    auto v = asn1::uper::decode_integer(r);
    ASSERT_TRUE(v.ok()) << c.value;
    EXPECT_EQ(v.value(), c.value);
  }
}

TEST(Uper, IntegerConstrainedVectors) {
  asn1::per::IntegerConstraint b;
  b.lower = 5;
  b.upper = 99;
  {
    asn1::BitWriter w;
    asn1::uper::encode_integer(w, 5, b);
    expect_bytes(finish(w), hex({0x00}));
  }
  {
    asn1::BitWriter w;
    asn1::uper::encode_integer(w, 6, b);
    expect_bytes(finish(w), hex({0x02}));
  }
  {
    asn1::BitWriter w;
    asn1::uper::encode_integer(w, 99, b);
    expect_bytes(finish(w), hex({0xBC}));
  }

  asn1::per::IntegerConstraint c;
  c.lower = -10;
  c.upper = 10;
  {
    asn1::BitWriter w;
    asn1::uper::encode_integer(w, -10, c);
    expect_bytes(finish(w), hex({0x00}));
    const auto _bits2 = hex({0x00});
    asn1::BitReader r(_bits2);
    auto v = asn1::uper::decode_integer(r, c);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), -10);
  }
  {
    asn1::BitWriter w;
    asn1::uper::encode_integer(w, 0, c);
    expect_bytes(finish(w), hex({0x50}));
  }
}

TEST(Uper, IntegerExtensible) {
  asn1::per::IntegerConstraint d;
  d.lower = 5;
  d.upper = 99;
  d.extensible = true;
  // value 99 in root: ext bit 0 + constrained encoding of 99
  // constrained alone was 0xBC = 10111100; with leading 0: 01011110 = 0x5E
  asn1::BitWriter w;
  asn1::uper::encode_integer(w, 99, d);
  expect_bytes(finish(w), hex({0x5E}));
  const auto _bits3 = hex({0x5E});
  asn1::BitReader r(_bits3);
  auto v = asn1::uper::decode_integer(r, d);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 99);
}

TEST(Uper, IntegerFixedSingle) {
  asn1::per::IntegerConstraint e;
  e.lower = 1000;
  e.upper = 1000;
  asn1::BitWriter w;
  asn1::uper::encode_integer(w, 1000, e);
  EXPECT_EQ(w.bit_size(), 0u);
  asn1::BitReader r(std::vector<std::uint8_t>{});
  auto v = asn1::uper::decode_integer(r, e);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 1000);
}

TEST(Uper, NullAndOctetString) {
  asn1::BitWriter wn;
  asn1::uper::encode_null(wn);
  EXPECT_EQ(wn.bit_size(), 0u);
  ASSERT_TRUE(asn1::uper::decode_null(asn1::BitReader(std::vector<std::uint8_t>{})).ok());

  {
    asn1::BitWriter w;
    asn1::uper::encode_octet_string(w, hex({0x00}));
    expect_bytes(finish(w), hex({0x01, 0x00}));
  }
  {
    asn1::per::SizeConstraint fixed2;
    fixed2.lower = 2;
    fixed2.upper = 2;
    asn1::BitWriter w;
    asn1::uper::encode_octet_string(w, hex({0xAB, 0xCD}), fixed2);
    expect_bytes(finish(w), hex({0xAB, 0xCD}));
  }
  {
    asn1::per::SizeConstraint range;
    range.lower = 3;
    range.upper = 7;
    asn1::BitWriter w;
    asn1::uper::encode_octet_string(w, hex({0x89, 0xAB, 0xCD, 0xEF}), range);
    expect_bytes(finish(w), hex({0x31, 0x35, 0x79, 0xBD, 0xE0}));
    const auto _bits4 = hex({0x31, 0x35, 0x79, 0xBD, 0xE0});
    asn1::BitReader r(_bits4);
    auto v = asn1::uper::decode_octet_string(r, range);
    ASSERT_TRUE(v.ok());
    expect_bytes(v.value(), hex({0x89, 0xAB, 0xCD, 0xEF}));
  }
}

TEST(Uper, BitStringAndUtf8) {
  {
    asn1::per::SizeConstraint fixed9;
    fixed9.lower = 9;
    fixed9.upper = 9;
    // 9 bits 101010101 -> AA80 with pad
    asn1::BitWriter w;
    asn1::uper::encode_bit_string(w, hex({0xAA, 0x80}), 9, fixed9);
    expect_bytes(finish(w), hex({0xAA, 0x80}));
    const auto _bits5 = hex({0xAA, 0x80});
    asn1::BitReader r(_bits5);
    auto v = asn1::uper::decode_bit_string(r, fixed9);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value().bit_length, 9u);
  }
  {
    asn1::BitWriter w;
    asn1::uper::encode_utf8_string(w, "Hi");
    // length 2 + 'H' 'i'
    expect_bytes(finish(w), hex({0x02, 'H', 'i'}));
    const auto _bits6 = hex({0x02, 'H', 'i'});
    asn1::BitReader r(_bits6);
    auto v = asn1::uper::decode_utf8_string(r);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), "Hi");
  }
}

TEST(Uper, SequenceWithOptional) {
  // SEQUENCE { a BOOLEAN, b BOOLEAN OPTIONAL }
  // a=true, b present true -> preamble bit 1 (optional present), then a=1, b=1
  // bits: 1 1 1 -> E0
  asn1::BitWriter w;
  const std::uint8_t present[] = {1};
  asn1::uper::encode_sequence_preamble(w, false, asn1::Span<const std::uint8_t>(present, 1));
  asn1::uper::encode_boolean(w, true);
  asn1::uper::encode_boolean(w, true);
  expect_bytes(finish(w), hex({0xE0}));

  const auto _bits7 = hex({0xE0});
  asn1::BitReader r(_bits7);
  auto bm = asn1::uper::decode_sequence_preamble(r, false, 1);
  ASSERT_TRUE(bm.ok());
  ASSERT_EQ(bm.value().size(), 1u);
  EXPECT_EQ(bm.value()[0], 1);
  auto a = asn1::uper::decode_boolean(r);
  auto b = asn1::uper::decode_boolean(r);
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_TRUE(a.value());
  EXPECT_TRUE(b.value());
}

TEST(Uper, ChoiceAndSequenceOf) {
  // CHOICE { a BOOLEAN, b INTEGER (0..7) } alternative b=5
  // index 1 of 2 -> 1 bit: 1, then INTEGER 5 with 3 bits: 101 -> 1101 0xxx -> D0
  asn1::BitWriter w;
  asn1::uper::encode_choice_root(w, 1, 2);
  asn1::uper::encode_integer(w, 5, asn1::per::IntegerConstraint{0, 7, false});
  expect_bytes(finish(w), hex({0xD0}));

  const auto _bits8 = hex({0xD0});
  asn1::BitReader r(_bits8);
  auto idx = asn1::uper::decode_choice_root(r, 2);
  ASSERT_TRUE(idx.ok());
  EXPECT_EQ(idx.value(), 1u);
  auto v = asn1::uper::decode_integer(r, asn1::per::IntegerConstraint{0, 7, false});
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 5);

  // SEQUENCE OF INTEGER (0..3) SIZE(1..3) with [1, 2]
  asn1::per::SizeConstraint sz;
  sz.lower = 1;
  sz.upper = 3;
  asn1::BitWriter w2;
  asn1::uper::encode_sequence_of_length(w2, 2, sz);
  asn1::uper::encode_integer(w2, 1, asn1::per::IntegerConstraint{0, 3, false});
  asn1::uper::encode_integer(w2, 2, asn1::per::IntegerConstraint{0, 3, false});
  auto bytes = finish(w2);
  asn1::BitReader r2(bytes);
  auto n = asn1::uper::decode_sequence_of_length(r2, sz);
  ASSERT_TRUE(n.ok());
  EXPECT_EQ(n.value(), 2u);
  auto e0 = asn1::uper::decode_integer(r2, asn1::per::IntegerConstraint{0, 3, false});
  auto e1 = asn1::uper::decode_integer(r2, asn1::per::IntegerConstraint{0, 3, false});
  ASSERT_TRUE(e0.ok());
  ASSERT_TRUE(e1.ok());
  EXPECT_EQ(e0.value(), 1);
  EXPECT_EQ(e1.value(), 2);
}

TEST(Uper, PersonLikeRoundTrip) {
  // SEQUENCE { id INTEGER(0..65535), name UTF8String, age INTEGER(0..150) OPTIONAL }
  const std::uint16_t id = 42;
  const std::string name = "Ada";
  const bool has_age = true;
  const std::int64_t age = 36;

  asn1::BitWriter w;
  const std::uint8_t opt[] = {has_age ? 1 : 0};
  asn1::uper::encode_sequence_preamble(w, false, asn1::Span<const std::uint8_t>(opt, 1));
  asn1::uper::encode_integer(w, id, asn1::per::IntegerConstraint{0, 65535, false});
  asn1::uper::encode_utf8_string(w, name);
  if (has_age) {
    asn1::uper::encode_integer(w, age, asn1::per::IntegerConstraint{0, 150, false});
  }
  auto bytes = finish(w);

  asn1::BitReader r(bytes);
  auto bm = asn1::uper::decode_sequence_preamble(r, false, 1);
  ASSERT_TRUE(bm.ok());
  auto id_v = asn1::uper::decode_integer(r, asn1::per::IntegerConstraint{0, 65535, false});
  auto name_v = asn1::uper::decode_utf8_string(r);
  ASSERT_TRUE(id_v.ok());
  ASSERT_TRUE(name_v.ok());
  EXPECT_EQ(id_v.value(), 42);
  EXPECT_EQ(name_v.value(), "Ada");
  ASSERT_EQ(bm.value()[0], 1);
  auto age_v = asn1::uper::decode_integer(r, asn1::per::IntegerConstraint{0, 150, false});
  ASSERT_TRUE(age_v.ok());
  EXPECT_EQ(age_v.value(), 36);
}

TEST(Uper, EnumeratedAndOid) {
  {
    asn1::BitWriter w;
    asn1::uper::encode_enumerated(w, 1, 3, false);
    auto bytes = finish(w);
    asn1::BitReader r(bytes);
    auto v = asn1::uper::decode_enumerated(r, 3, false);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 1u);
  }
  {
    asn1::BitWriter w;
    asn1::uper::encode_enumerated(w, 0, 2, true);
    auto bytes = finish(w);
    asn1::BitReader r(bytes);
    auto v = asn1::uper::decode_enumerated(r, 2, true);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 0u);
  }
  {
    std::vector<std::uint64_t> arcs = {1, 3, 6, 1};
    asn1::BitWriter w;
    asn1::uper::encode_object_identifier(w, arcs, false);
    auto bytes = finish(w);
    asn1::BitReader r(bytes);
    auto v = asn1::uper::decode_object_identifier(r, false);
    ASSERT_TRUE(v.ok());
    ASSERT_EQ(v.value().size(), arcs.size());
    for (std::size_t i = 0; i < arcs.size(); ++i) {
      EXPECT_EQ(v.value()[i], arcs[i]);
    }
  }
}
