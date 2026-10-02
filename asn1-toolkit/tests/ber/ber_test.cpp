#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/ber/tlv.hpp>
#include <asn1/runtime/byte_io.hpp>

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

}  // namespace

TEST(BerTlv, ShortTagAndLength) {
  asn1::ByteWriter w;
  asn1::ber::encode_tag(w, asn1::ber::universal(2));
  asn1::ber::encode_length(w, 1);
  w.put(0x00);
  expect_bytes(w.buffer(), hex({0x02, 0x01, 0x00}));

  asn1::ByteReader r(w.buffer());
  auto tag = asn1::ber::decode_tag(r);
  ASSERT_TRUE(tag.ok());
  EXPECT_EQ(tag.value().number, 2u);
  EXPECT_FALSE(tag.value().constructed);
  auto len = asn1::ber::decode_length(r);
  ASSERT_TRUE(len.ok());
  EXPECT_FALSE(len.value().indefinite);
  EXPECT_EQ(len.value().value, 1u);
}

TEST(BerTlv, HighTagNumber) {
  asn1::ByteWriter w;
  asn1::ber::encode_tag(w, asn1::ber::context(128, /*constructed=*/true));
  // 0xBF = context|constructed|0x1F, then 0x81 0x00 for 128
  expect_bytes(w.take(), hex({0xBF, 0x81, 0x00}));

  asn1::ByteWriter w2;
  asn1::ber::encode_tag(w2, asn1::ber::context(128, true));
  asn1::ByteReader r(w2.buffer());
  auto tag = asn1::ber::decode_tag(r);
  ASSERT_TRUE(tag.ok());
  EXPECT_EQ(tag.value().cls, asn1::ber::TagClass::Context);
  EXPECT_TRUE(tag.value().constructed);
  EXPECT_EQ(tag.value().number, 128u);
}

TEST(BerTlv, LongFormLength) {
  asn1::ByteWriter w;
  asn1::ber::encode_length(w, 200);
  expect_bytes(w.buffer(), hex({0x81, 0xC8}));

  asn1::ByteReader r(w.buffer());
  auto len = asn1::ber::decode_length(r);
  ASSERT_TRUE(len.ok());
  EXPECT_EQ(len.value().value, 200u);
}

TEST(BerTlv, IndefiniteLengthDecode) {
  auto bytes = hex({0x80});
  asn1::ByteReader r(bytes);
  auto len = asn1::ber::decode_length(r);
  ASSERT_TRUE(len.ok());
  EXPECT_TRUE(len.value().indefinite);
}

TEST(BerCodec, BooleanRoundTripAndVectors) {
  {
    asn1::ByteWriter w;
    asn1::ber::encode_boolean(w, true);
    expect_bytes(w.buffer(), hex({0x01, 0x01, 0xFF}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_boolean(r);
    ASSERT_TRUE(v.ok());
    EXPECT_TRUE(v.value());
  }
  {
    asn1::ByteWriter w;
    asn1::ber::encode_boolean(w, false);
    expect_bytes(w.buffer(), hex({0x01, 0x01, 0x00}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_boolean(r);
    ASSERT_TRUE(v.ok());
    EXPECT_FALSE(v.value());
  }
}

TEST(BerCodec, IntegerKnownVectors) {
  struct Case {
    std::int64_t value;
    std::vector<std::uint8_t> encoding;
  };
  const Case cases[] = {
      {0, hex({0x02, 0x01, 0x00})},
      {127, hex({0x02, 0x01, 0x7F})},
      {128, hex({0x02, 0x02, 0x00, 0x80})},
      {256, hex({0x02, 0x02, 0x01, 0x00})},
      {-128, hex({0x02, 0x01, 0x80})},
      {-129, hex({0x02, 0x02, 0xFF, 0x7F})},
  };
  for (const auto& c : cases) {
    asn1::ByteWriter w;
    asn1::ber::encode_integer(w, c.value);
    expect_bytes(w.buffer(), c.encoding);

    asn1::ByteReader r(c.encoding);
    auto v = asn1::ber::decode_integer(r);
    ASSERT_TRUE(v.ok()) << c.value;
    EXPECT_EQ(v.value(), c.value);
  }
}

TEST(BerCodec, NullOctetBitUtf8RoundTrip) {
  {
    asn1::ByteWriter w;
    asn1::ber::encode_null(w);
    expect_bytes(w.buffer(), hex({0x05, 0x00}));
    asn1::ByteReader r(w.buffer());
    ASSERT_TRUE(asn1::ber::decode_null(r).ok());
  }
  {
    const auto payload = hex({0xDE, 0xAD, 0xBE, 0xEF});
    asn1::ByteWriter w;
    asn1::ber::encode_octet_string(w, payload);
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_octet_string(r);
    ASSERT_TRUE(v.ok());
    expect_bytes(v.value(), payload);
  }
  {
    const auto bits = hex({0xF0});  // 11110000, 4 unused
    asn1::ByteWriter w;
    asn1::ber::encode_bit_string(w, bits, 4);
    expect_bytes(w.buffer(), hex({0x03, 0x02, 0x04, 0xF0}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_bit_string(r);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value().unused_bits, 4);
    expect_bytes(v.value().bits, bits);
  }
  {
    asn1::ByteWriter w;
    asn1::ber::encode_utf8_string(w, "hi");
    expect_bytes(w.buffer(), hex({0x0C, 0x02, 'h', 'i'}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_utf8_string(r);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), "hi");
  }
}

TEST(BerCodec, SequenceConstructed) {
  asn1::ByteWriter comps;
  asn1::ber::encode_integer(comps, 1);
  asn1::ber::encode_boolean(comps, true);

  asn1::ByteWriter w;
  asn1::ber::encode_constructed(w, asn1::ber::universal(asn1::ber::kTagSequence, true),
                                comps.buffer());

  // SEQUENCE { INTEGER 1, BOOLEAN TRUE }
  expect_bytes(w.buffer(), hex({0x30, 0x06, 0x02, 0x01, 0x01, 0x01, 0x01, 0xFF}));

  asn1::ByteReader r(w.buffer());
  auto content = asn1::ber::decode_constructed(
      r, asn1::ber::universal(asn1::ber::kTagSequence, true));
  ASSERT_TRUE(content.ok());
  expect_bytes(content.value(), comps.buffer());

  asn1::ByteReader inner(content.value());
  auto i = asn1::ber::decode_integer(inner);
  auto b = asn1::ber::decode_boolean(inner);
  ASSERT_TRUE(i.ok());
  ASSERT_TRUE(b.ok());
  EXPECT_EQ(i.value(), 1);
  EXPECT_TRUE(b.value());
  EXPECT_TRUE(inner.eof());
}

TEST(BerCodec, IndefiniteConstructedSequence) {
  // 30 80  02 01 05  00 00
  auto bytes = hex({0x30, 0x80, 0x02, 0x01, 0x05, 0x00, 0x00});
  asn1::ByteReader r(bytes);
  auto content = asn1::ber::decode_constructed(
      r, asn1::ber::universal(asn1::ber::kTagSequence, true));
  ASSERT_TRUE(content.ok());
  expect_bytes(content.value(), hex({0x02, 0x01, 0x05}));
  EXPECT_TRUE(r.eof());
}

TEST(BerCodec, TagMismatch) {
  auto bytes = hex({0x01, 0x01, 0xFF});  // BOOLEAN
  asn1::ByteReader r(bytes);
  auto v = asn1::ber::decode_integer(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::TagMismatch);
}

TEST(BerCodec, TruncatedInput) {
  auto bytes = hex({0x02, 0x05, 0x01});  // claims 5 content bytes
  asn1::ByteReader r(bytes);
  auto v = asn1::ber::decode_integer(r);
  ASSERT_FALSE(v.ok());
  EXPECT_EQ(v.error().code, asn1::Error::Code::Truncated);
}

TEST(BerCodec, EnumeratedAndOidRoundTrip) {
  {
    asn1::ByteWriter w;
    asn1::ber::encode_enumerated(w, 2);
    expect_bytes(w.buffer(), hex({0x0A, 0x01, 0x02}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_enumerated(r);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 2);
  }
  {
    std::vector<std::uint64_t> arcs = {1, 2, 840, 113549};
    asn1::ByteWriter w;
    asn1::ber::encode_object_identifier(w, arcs);
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_object_identifier(r);
    ASSERT_TRUE(v.ok());
    ASSERT_EQ(v.value().size(), arcs.size());
    for (std::size_t i = 0; i < arcs.size(); ++i) {
      EXPECT_EQ(v.value()[i], arcs[i]);
    }
  }
  {
    std::vector<std::uint64_t> arcs = {8571, 1};
    asn1::ByteWriter w;
    asn1::ber::encode_relative_oid(w, arcs);
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_relative_oid(r);
    ASSERT_TRUE(v.ok());
    ASSERT_EQ(v.value().size(), 2u);
    EXPECT_EQ(v.value()[0], 8571u);
    EXPECT_EQ(v.value()[1], 1u);
  }
}
