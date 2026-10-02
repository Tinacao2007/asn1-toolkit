#include <asn1/runtime/der/codec.hpp>
#include <asn1/runtime/der/tlv.hpp>

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
  ASSERT_EQ(got.size(), want.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    EXPECT_EQ(got[i], want[i]) << "byte " << i;
  }
}

}  // namespace

TEST(DerTlv, RejectsIndefiniteLength) {
  auto bytes = hex({0x80});
  asn1::ByteReader r(bytes);
  auto len = asn1::der::decode_length(r);
  ASSERT_FALSE(len.ok());
  EXPECT_EQ(len.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerTlv, RejectsNonShortestLength) {
  // Long form for length 1: 81 01 -- forbidden in DER.
  auto bytes = hex({0x81, 0x01});
  asn1::ByteReader r(bytes);
  auto len = asn1::der::decode_length(r);
  ASSERT_FALSE(len.ok());
  EXPECT_EQ(len.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerTlv, RejectsLeadingZeroInLongLength) {
  // 82 00 C8 for 200 -- leading zero forbidden.
  auto bytes = hex({0x82, 0x00, 0xC8});
  asn1::ByteReader r(bytes);
  auto len = asn1::der::decode_length(r);
  ASSERT_FALSE(len.ok());
  EXPECT_EQ(len.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerTlv, AcceptsCanonicalLongLength) {
  asn1::ByteWriter w;
  asn1::der::encode_length(w, 200);
  expect_bytes(w.buffer(), hex({0x81, 0xC8}));
  asn1::ByteReader r(w.buffer());
  auto len = asn1::der::decode_length(r);
  ASSERT_TRUE(len.ok());
  EXPECT_EQ(len.value().value, 200u);
}

TEST(DerCodec, BooleanTrueMustBeFF) {
  asn1::ByteWriter w;
  asn1::der::encode_boolean(w, true);
  expect_bytes(w.buffer(), hex({0x01, 0x01, 0xFF}));

  auto bad = hex({0x01, 0x01, 0x01});  // BER-true but not DER
  asn1::ByteReader r(bad);
  auto v = asn1::der::decode_boolean(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerCodec, IntegerRejectsNonMinimal) {
  // Non-minimal 0: 02 02 00 00
  auto bad = hex({0x02, 0x02, 0x00, 0x00});
  asn1::ByteReader r(bad);
  auto v = asn1::der::decode_integer(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);

  asn1::ByteWriter w;
  asn1::der::encode_integer(w, 128);
  expect_bytes(w.buffer(), hex({0x02, 0x02, 0x00, 0x80}));
  asn1::ByteReader r2(w.buffer());
  auto ok = asn1::der::decode_integer(r2);
  ASSERT_TRUE(ok.ok());
  EXPECT_EQ(ok.value(), 128);
}

TEST(DerCodec, BitStringClearsUnusedBits) {
  // Input has unused low bits set; DER encode must clear them.
  auto bits = hex({0xF1});  // unused 4 => keep 0xF0
  asn1::ByteWriter w;
  asn1::der::encode_bit_string(w, bits, 4);
  expect_bytes(w.buffer(), hex({0x03, 0x02, 0x04, 0xF0}));

  auto dirty = hex({0x03, 0x02, 0x04, 0xF1});
  asn1::ByteReader r(dirty);
  auto v = asn1::der::decode_bit_string(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerCodec, RejectsConstructedOctetString) {
  // Constructed OCTET STRING wrapping one fragment -- forbidden in DER.
  auto bytes = hex({0x24, 0x04, 0x04, 0x02, 0xAB, 0xCD});
  asn1::ByteReader r(bytes);
  auto v = asn1::der::decode_octet_string(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerCodec, SequenceRoundTrip) {
  asn1::ByteWriter comps;
  asn1::der::encode_integer(comps, 1);
  asn1::der::encode_boolean(comps, true);

  asn1::ByteWriter w;
  asn1::der::encode_sequence(w, comps.buffer());
  expect_bytes(w.buffer(), hex({0x30, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01, 0xFF}));

  asn1::ByteReader r(w.buffer());
  auto content = asn1::der::decode_sequence(r);
  ASSERT_TRUE(content.ok());
  expect_bytes(content.value(), comps.buffer());
}

TEST(DerCodec, SetSortsByTag) {
  asn1::ByteWriter a;
  asn1::der::encode_boolean(a, true);  // tag 1
  asn1::ByteWriter b;
  asn1::der::encode_integer(b, 5);  // tag 2

  // Pass out of order: INTEGER then BOOLEAN; encode_set must sort to BOOLEAN, INTEGER.
  asn1::ByteWriter w;
  asn1::der::encode_set(w, {b.buffer(), a.buffer()});
  expect_bytes(w.buffer(),
               hex({0x31, 0x06, 0x01, 0x01, 0xFF, 0x02, 0x01, 0x05}));

  asn1::ByteReader r(w.buffer());
  auto content = asn1::der::decode_set(r);
  ASSERT_TRUE(content.ok());
}

TEST(DerCodec, SetRejectsDescendingTagOrder) {
  // SET with INTEGER then BOOLEAN (tags 2 then 1) -- descending, not DER.
  auto bytes = hex({0x31, 0x06, 0x02, 0x01, 0x05, 0x01, 0x01, 0xFF});
  asn1::ByteReader r(bytes);
  auto v = asn1::der::decode_set(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::NonCanonical);
}

TEST(DerCodec, RoundTripPrimitives) {
  {
    asn1::ByteWriter w;
    asn1::der::encode_null(w);
    asn1::ByteReader r(w.buffer());
    ASSERT_TRUE(asn1::der::decode_null(r).ok());
  }
  {
    auto payload = hex({1, 2, 3});
    asn1::ByteWriter w;
    asn1::der::encode_octet_string(w, payload);
    asn1::ByteReader r(w.buffer());
    auto v = asn1::der::decode_octet_string(r);
    ASSERT_TRUE(v.ok());
    expect_bytes(v.value(), payload);
  }
  {
    asn1::ByteWriter w;
    asn1::der::encode_utf8_string(w, "der");
    asn1::ByteReader r(w.buffer());
    auto v = asn1::der::decode_utf8_string(r);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), "der");
  }
}
