#include <asn1/runtime/aper.hpp>
#include <asn1/runtime/uper.hpp>

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

TEST(Aper, BooleanSameAsUper) {
  asn1::BitWriter w;
  asn1::aper::encode_boolean(w, true);
  expect_bytes(finish(w), hex({0x80}));
}

TEST(Aper, IntegerRange256IsOneAlignedOctet) {
  // INTEGER (0..255) value 253 -> single octet FD (APER aligns, already at boundary)
  asn1::per::IntegerConstraint c;
  c.lower = 0;
  c.upper = 255;
  asn1::BitWriter w;
  asn1::aper::encode_integer(w, 253, c);
  expect_bytes(finish(w), hex({0xFD}));

  const auto _bits9 = hex({0xFD});
  asn1::BitReader r(_bits9);
  auto v = asn1::aper::decode_integer(r, c);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 253);
}

TEST(Aper, IntegerRange257IsTwoAlignedOctets) {
  // INTEGER (0..256) value 253 -> 00 FD
  asn1::per::IntegerConstraint c;
  c.lower = 0;
  c.upper = 256;
  asn1::BitWriter w;
  asn1::aper::encode_integer(w, 253, c);
  expect_bytes(finish(w), hex({0x00, 0xFD}));

  const auto _bits10 = hex({0x00, 0xFD});
  asn1::BitReader r(_bits10);
  auto v = asn1::aper::decode_integer(r, c);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 253);
}

TEST(Aper, Integer0To65535TwoOctets) {
  asn1::per::IntegerConstraint c;
  c.lower = 0;
  c.upper = 65535;
  asn1::BitWriter w;
  asn1::aper::encode_integer(w, 253, c);
  expect_bytes(finish(w), hex({0x00, 0xFD}));
}

TEST(Aper, DiffersFromUperOnRange300) {
  asn1::per::IntegerConstraint c;
  c.lower = 0;
  c.upper = 299;

  asn1::BitWriter uper_w;
  asn1::uper::encode_integer(uper_w, 100, c);
  EXPECT_EQ(uper_w.bit_size(), 9u);  // minimal bits, no align

  asn1::BitWriter aper_w;
  aper_w.put_bit(true);  // force misalignment before the INTEGER
  asn1::aper::encode_integer(aper_w, 100, c);
  // 1 bit + 7 pad + 16-bit field
  EXPECT_EQ(aper_w.bit_size(), 24u);

  auto bytes = finish(aper_w);
  asn1::BitReader r(bytes);
  ASSERT_TRUE(r.get_bit().ok());
  auto v = asn1::aper::decode_integer(r, c);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 100);
}

TEST(Aper, SequenceAlignmentMatrix) {
  // SEQUENCE {
  //   a BOOLEAN,
  //   b INTEGER (0..254),
  //   c INTEGER (0..255),
  //   d BOOLEAN,
  //   e INTEGER (0..256)
  // } with a=false, b=253, c=253, d=false, e=253
  // Known APER vector from asn1tools: 7E 80 FD 00 00 FD
  asn1::BitWriter w;
  asn1::aper::encode_boolean(w, false);
  asn1::aper::encode_integer(w, 253, asn1::per::IntegerConstraint{0, 254, false});
  asn1::aper::encode_integer(w, 253, asn1::per::IntegerConstraint{0, 255, false});
  asn1::aper::encode_boolean(w, false);
  asn1::aper::encode_integer(w, 253, asn1::per::IntegerConstraint{0, 256, false});
  expect_bytes(finish(w), hex({0x7E, 0x80, 0xFD, 0x00, 0x00, 0xFD}));

  const auto _bits11 = hex({0x7E, 0x80, 0xFD, 0x00, 0x00, 0xFD});
  asn1::BitReader r(_bits11);
  auto a = asn1::aper::decode_boolean(r);
  auto b = asn1::aper::decode_integer(r, asn1::per::IntegerConstraint{0, 254, false});
  auto c = asn1::aper::decode_integer(r, asn1::per::IntegerConstraint{0, 255, false});
  auto d = asn1::aper::decode_boolean(r);
  auto e = asn1::aper::decode_integer(r, asn1::per::IntegerConstraint{0, 256, false});
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  ASSERT_TRUE(c.ok());
  ASSERT_TRUE(d.ok());
  ASSERT_TRUE(e.ok());
  EXPECT_FALSE(a.value());
  EXPECT_EQ(b.value(), 253);
  EXPECT_EQ(c.value(), 253);
  EXPECT_FALSE(d.value());
  EXPECT_EQ(e.value(), 253);
}

TEST(Aper, OctetStringAlignsAfterBoolean) {
  // SEQUENCE { a BOOLEAN, b OCTET STRING } a=true, b=00
  // APER: bit 1, pad 7, length 01, byte 00 -> 80 01 00
  asn1::BitWriter w;
  asn1::aper::encode_boolean(w, true);
  asn1::aper::encode_octet_string(w, hex({0x00}));
  expect_bytes(finish(w), hex({0x80, 0x01, 0x00}));

  const auto _bits12 = hex({0x80, 0x01, 0x00});
  asn1::BitReader r(_bits12);
  auto a = asn1::aper::decode_boolean(r);
  auto b = asn1::aper::decode_octet_string(r);
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_TRUE(a.value());
  expect_bytes(b.value(), hex({0x00}));
}

TEST(Aper, OctetStringFixedSize3Aligns) {
  // OCTET STRING (SIZE(3)) -- APER aligns when fixed size > 2
  asn1::per::SizeConstraint sz;
  sz.lower = 3;
  sz.upper = 3;
  asn1::BitWriter w;
  w.put_bit(true);
  asn1::aper::encode_octet_string(w, hex({0xAB, 0xCD, 0xEF}), sz);
  // 1 + 7 pad + AB CD EF
  expect_bytes(finish(w), hex({0x80, 0xAB, 0xCD, 0xEF}));
}

TEST(Aper, ChoiceAndSequenceOfRoundTrip) {
  asn1::BitWriter w;
  asn1::aper::encode_choice_root(w, 0, 3);
  asn1::aper::encode_boolean(w, true);
  // index 0 of 3 needs 2 bits (00) + boolean 1 -> 001xxxxx -> 0x20
  auto bytes = finish(w);

  asn1::BitReader r(bytes);
  auto idx = asn1::aper::decode_choice_root(r, 3);
  auto b = asn1::aper::decode_boolean(r);
  ASSERT_TRUE(idx.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_EQ(idx.value(), 0u);
  EXPECT_TRUE(b.value());

  asn1::per::SizeConstraint sz;
  sz.lower = 0;
  sz.upper = 7;
  asn1::BitWriter w2;
  w2.put_bit(true);
  asn1::aper::encode_sequence_of_length(w2, 3, sz);
  // after 1 bit, constrained length 0..7 is 3 bits (no APER align for range<=255)
  // still works round-trip
  auto bytes2 = finish(w2);
  asn1::BitReader r2(bytes2);
  ASSERT_TRUE(r2.get_bit().ok());
  auto n = asn1::aper::decode_sequence_of_length(r2, sz);
  ASSERT_TRUE(n.ok());
  EXPECT_EQ(n.value(), 3u);
}

TEST(Aper, PersonLikeRoundTrip) {
  asn1::BitWriter w;
  const std::uint8_t opt[] = {1};
  asn1::aper::encode_sequence_preamble(w, false, false,
                                       asn1::Span<const std::uint8_t>(opt, 1));
  asn1::aper::encode_integer(w, 42, asn1::per::IntegerConstraint{0, 65535, false});
  asn1::aper::encode_utf8_string(w, "Ada");
  asn1::aper::encode_integer(w, 36, asn1::per::IntegerConstraint{0, 150, false});
  auto bytes = finish(w);

  asn1::BitReader r(bytes);
  auto bm = asn1::aper::decode_sequence_preamble(r, false, 1);
  auto id = asn1::aper::decode_integer(r, asn1::per::IntegerConstraint{0, 65535, false});
  auto name = asn1::aper::decode_utf8_string(r);
  auto age = asn1::aper::decode_integer(r, asn1::per::IntegerConstraint{0, 150, false});
  ASSERT_TRUE(bm.ok());
  ASSERT_TRUE(id.ok());
  ASSERT_TRUE(name.ok());
  ASSERT_TRUE(age.ok());
  EXPECT_EQ(bm.value().optionals[0], 1);
  EXPECT_EQ(id.value(), 42);
  EXPECT_EQ(name.value(), "Ada");
  EXPECT_EQ(age.value(), 36);
}
