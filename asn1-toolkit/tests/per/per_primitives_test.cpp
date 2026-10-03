#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/per/primitives.hpp>

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> hex(std::initializer_list<std::uint8_t> bytes) {
  return std::vector<std::uint8_t>(bytes);
}

void expect_bytes(const std::vector<std::uint8_t>& got,
                  const std::vector<std::uint8_t>& want) {
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

}  // namespace

TEST(BitIo, PutGetBitsRoundTrip) {
  asn1::BitWriter w;
  w.put_bits(0xA, 4);  // 1010
  w.put_bits(0x5, 4);  // 0101 -> A5
  auto bytes = w.take();
  expect_bytes(bytes, hex({0xA5}));

  asn1::BitReader r(bytes);
  auto a = r.get_bits(4);
  auto b = r.get_bits(4);
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_EQ(a.value(), 0xAu);
  EXPECT_EQ(b.value(), 0x5u);
}

TEST(BitIo, AlignToOctet) {
  asn1::BitWriter w;
  w.put_bits(1, 1);
  EXPECT_FALSE(w.octet_aligned());
  w.align_to_octet();
  EXPECT_TRUE(w.octet_aligned());
  EXPECT_EQ(w.bit_size(), 8u);
  expect_bytes(w.take(), hex({0x80}));

  const auto _bits13 = hex({0x80, 0xFF});
  asn1::BitReader r(_bits13);
  ASSERT_TRUE(r.get_bit().ok());
  ASSERT_TRUE(r.align_to_octet().ok());
  EXPECT_TRUE(r.octet_aligned());
  auto next = r.get_octet();
  ASSERT_TRUE(next.ok());
  EXPECT_EQ(next.value(), 0xFFu);
}

TEST(PerPrimitives, BitsForRange) {
  EXPECT_EQ(asn1::per::bits_for_range(1), 0u);
  EXPECT_EQ(asn1::per::bits_for_range(2), 1u);
  EXPECT_EQ(asn1::per::bits_for_range(8), 3u);
  EXPECT_EQ(asn1::per::bits_for_range(256), 8u);
  EXPECT_EQ(asn1::per::bits_for_range(257), 9u);
}

TEST(PerPrimitives, ConstrainedUperSmall) {
  // INTEGER (0..7) value 5 -> 3 bits 101, padded -> A0
  asn1::BitWriter w;
  asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Unaligned, 5, 0, 7);
  expect_bytes(w.take(), hex({0xA0}));

  const auto _bits14 = hex({0xA0});
  asn1::BitReader r(_bits14);
  auto v = asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Unaligned, 0, 7);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 5);
}

TEST(PerPrimitives, ConstrainedAperVsUperRange300) {
  // range 300 (0..299): UPER uses 9 bits; APER uses aligned 16 bits.
  {
    asn1::BitWriter w;
    asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Unaligned, 100, 0, 299);
    EXPECT_EQ(w.bit_size(), 9u);
    auto bytes = w.take();
    asn1::BitReader r(bytes);
    auto v =
        asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Unaligned, 0, 299);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 100);
  }
  {
    asn1::BitWriter w;
    w.put_bit(true);  // force misalignment
    asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Aligned, 100, 0, 299);
    // 1 bit + 7 pad + 16 bits = 24 bits
    EXPECT_EQ(w.bit_size(), 24u);
    auto bytes = w.take();
    asn1::BitReader r(bytes);
    ASSERT_TRUE(r.get_bit().ok());
    auto v = asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Aligned, 0, 299);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 100);
  }
}

TEST(PerPrimitives, ConstrainedSingleValue) {
  asn1::BitWriter w;
  asn1::per::encode_constrained_whole_number(w, asn1::per::Variant::Unaligned, 42, 42, 42);
  EXPECT_EQ(w.bit_size(), 0u);
  asn1::BitReader r(std::vector<std::uint8_t>{});
  auto v =
      asn1::per::decode_constrained_whole_number(r, asn1::per::Variant::Unaligned, 42, 42);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 42);
}

TEST(PerPrimitives, SemiAndUnconstrainedRoundTrip) {
  for (asn1::per::Variant v :
       {asn1::per::Variant::Unaligned, asn1::per::Variant::Aligned}) {
    asn1::BitWriter w;
    asn1::per::encode_semi_constrained_whole_number(w, v, 1000, 100);
    auto bytes = w.take();
    asn1::BitReader r(bytes);
    auto got = asn1::per::decode_semi_constrained_whole_number(r, v, 100);
    ASSERT_TRUE(got.ok());
    EXPECT_EQ(got.value(), 1000);

    asn1::BitWriter w2;
    asn1::per::encode_unconstrained_whole_number(w2, v, -129);
    auto bytes2 = w2.take();
    asn1::BitReader r2(bytes2);
    auto got2 = asn1::per::decode_unconstrained_whole_number(r2, v);
    ASSERT_TRUE(got2.ok());
    EXPECT_EQ(got2.value(), -129);
  }
}

TEST(PerPrimitives, NormallySmallAndLength) {
  asn1::BitWriter w;
  asn1::per::encode_normally_small_non_negative_whole_number(
      w, asn1::per::Variant::Unaligned, 3);
  // 0 + 000011 -> 00000110 padded -> 0x06
  expect_bytes(w.take(), hex({0x06}));

  const auto _bits15 = hex({0x06});
  asn1::BitReader r(_bits15);
  auto v = asn1::per::decode_normally_small_non_negative_whole_number(
      r, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(v.ok());
  EXPECT_EQ(v.value(), 3u);

  asn1::BitWriter w2;
  asn1::per::encode_normally_small_length(w2, asn1::per::Variant::Unaligned, 4);
  // length 4 -> 0 + (4-1=3 as 6 bits) = 0000011 -> 0x06
  expect_bytes(w2.take(), hex({0x06}));
  asn1::BitReader r2(hex({0x06}));
  auto len = asn1::per::decode_normally_small_length(r2, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(len.ok());
  EXPECT_EQ(len.value(), 4u);

  // X.691 11.9.3.4: length > 64 encodes as bit 1 followed by length determinant
  asn1::BitWriter w3;
  asn1::per::encode_normally_small_length(w3, asn1::per::Variant::Unaligned, 70);
  auto b3 = w3.take();
  asn1::BitReader r3(b3);
  auto len3 = asn1::per::decode_normally_small_length(r3, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(len3.ok());
  EXPECT_EQ(len3.value(), 70u);
}

TEST(PerPrimitives, LengthDeterminant) {
  asn1::BitWriter w;
  EXPECT_EQ(asn1::per::encode_length_determinant(w, asn1::per::Variant::Unaligned, 5), 5u);
  EXPECT_EQ(asn1::per::encode_length_determinant(w, asn1::per::Variant::Unaligned, 200), 200u);
  auto bytes = w.take();
  expect_bytes(bytes, hex({0x05, 0x80 | 0x00, 0xC8}));  // 05, 80 C8 for 200

  asn1::BitReader r(bytes);
  auto a = asn1::per::decode_length_determinant(r, asn1::per::Variant::Unaligned);
  auto b = asn1::per::decode_length_determinant(r, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(a.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_EQ(a.value(), 5u);
  EXPECT_EQ(b.value(), 200u);
}

TEST(PerPrimitives, ChoiceBitmapOpenType) {
  asn1::BitWriter w;
  asn1::per::encode_choice_index(w, asn1::per::Variant::Unaligned, 2, 4);  // 2 bits: 10
  const std::uint8_t bm[] = {1, 0, 1};
  asn1::per::encode_bitmap(w, asn1::Span<const std::uint8_t>(bm, 3));
  // so far 5 bits; then open type with 1 byte 0xAB
  asn1::per::encode_open_type(w, asn1::per::Variant::Unaligned, hex({0xAB}));
  auto bytes = w.take();

  asn1::BitReader r(bytes);
  auto idx = asn1::per::decode_choice_index(r, asn1::per::Variant::Unaligned, 4);
  auto bits = asn1::per::decode_bitmap(r, 3);
  auto ot = asn1::per::decode_open_type(r, asn1::per::Variant::Unaligned);
  ASSERT_TRUE(idx.ok());
  ASSERT_TRUE(bits.ok());
  ASSERT_TRUE(ot.ok());
  EXPECT_EQ(idx.value(), 2u);
  ASSERT_EQ(bits.value().size(), 3u);
  EXPECT_EQ(bits.value()[0], 1);
  EXPECT_EQ(bits.value()[1], 0);
  EXPECT_EQ(bits.value()[2], 1);
  expect_bytes(ot.value(), hex({0xAB}));
}

TEST(PerPrimitives, OpenTypeAperAligns) {
  asn1::BitWriter w;
  w.put_bit(true);
  asn1::per::encode_open_type(w, asn1::per::Variant::Aligned, hex({0x11, 0x22}));
  // 1 + 7 pad + length 02 + 11 22
  expect_bytes(w.take(), hex({0x80, 0x02, 0x11, 0x22}));

  const auto _bits16 = hex({0x80, 0x02, 0x11, 0x22});
  asn1::BitReader r(_bits16);
  ASSERT_TRUE(r.get_bit().ok());
  auto ot = asn1::per::decode_open_type(r, asn1::per::Variant::Aligned);
  ASSERT_TRUE(ot.ok());
  expect_bytes(ot.value(), hex({0x11, 0x22}));
}
