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
  {
    asn1::ByteWriter w;
    asn1::der::encode_real(w, 100.0);
    expect_bytes(w.buffer(), hex({0x09, 0x03, 0x80, 0x02, 0x19}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::der::decode_real(r);
    ASSERT_TRUE(v.ok());
    EXPECT_EQ(v.value(), 100.0);
  }
}

TEST(DerCodec, EmbeddedPdvCharacterStringAndModernExternal) {
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Fixed;
    v.data_value = {0xAB};
    asn1::ByteWriter w;
    asn1::der::encode_embedded_pdv(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::der::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().data_value, v.data_value);
  }
  {
    asn1::ber::CharacterStringValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Syntax;
    v.identification.transfer_syntax = {1, 2, 3};
    v.string_value = {0x41};
    asn1::ByteWriter w;
    asn1::der::encode_character_string(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::der::decode_character_string(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Syntax);
    EXPECT_EQ(d.value().string_value, v.string_value);
  }
  {
    asn1::ber::ModernExternalValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Fixed;
    v.data_value = {0x09};
    asn1::ByteWriter w;
    asn1::der::encode_external_modern(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::der::decode_external_modern(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().data_value, v.data_value);
  }
  // DER rejects constructed OCTET STRING value component (accepted by BER).
  {
    const auto enc = hex({0x2B, 0x0E, 0xA0, 0x02, 0x85, 0x00, 0xA2, 0x08, 0x04, 0x02, 0xAA,
                          0xBB, 0x04, 0x02, 0xCC, 0xDD});
    asn1::ByteReader r(enc);
    auto d = asn1::der::decode_embedded_pdv(r);
    ASSERT_FALSE(d.ok());
    EXPECT_EQ(d.error().code, asn1::Error::Code::NonCanonical);
  }
  // DER rejects indefinite outer length.
  {
    const auto enc =
        hex({0x2B, 0x80, 0xA0, 0x02, 0x85, 0x00, 0x82, 0x01, 0x7E, 0x00, 0x00});
    asn1::ByteReader r(enc);
    auto d = asn1::der::decode_embedded_pdv(r);
    ASSERT_FALSE(d.ok());
    EXPECT_EQ(d.error().code, asn1::Error::Code::NonCanonical);
  }
}

TEST(DerCodec, DefaultFieldOmissionAndBackfill) {
  // In DER (X.690 §11.5), if a component value is equal to its default value,
  // it SHALL NOT be encoded.
  const asn1::ber::Tag id_tag = asn1::ber::context(0);
  const asn1::ber::Tag flag_tag = asn1::ber::context(1);
  const asn1::ber::Tag priority_tag = asn1::ber::context(2);

  // Equal to default: flag=true, priority=5 -> omitted
  asn1::ByteWriter w;
  asn1::der::encode_integer(w, 42, id_tag);
  // flag and priority omitted
  asn1::ByteWriter seq_w;
  asn1::der::encode_sequence(seq_w, w.buffer());

  // Decode from DER stream
  asn1::ByteReader r(seq_w.buffer());
  auto seq_content = asn1::der::decode_sequence(r);
  ASSERT_TRUE(seq_content.ok());

  asn1::ByteReader f_reader(seq_content.value());
  auto id_res = asn1::der::decode_integer(f_reader, id_tag);
  ASSERT_TRUE(id_res.ok());
  EXPECT_EQ(id_res.value(), 42);

  bool flag_val = true;
  if (f_reader.remaining() > 0) {
    asn1::ByteReader peek(f_reader.remaining_span());
    auto hdr = asn1::der::decode_tlv_header(peek);
    if (hdr && hdr.value().tag == flag_tag) {
      auto dec = asn1::der::decode_boolean(f_reader, flag_tag);
      ASSERT_TRUE(dec.ok());
      flag_val = dec.value();
    }
  }
  EXPECT_EQ(flag_val, true);

  std::int64_t priority_val = 5;
  if (f_reader.remaining() > 0) {
    asn1::ByteReader peek(f_reader.remaining_span());
    auto hdr = asn1::der::decode_tlv_header(peek);
    if (hdr && hdr.value().tag == priority_tag) {
      auto dec = asn1::der::decode_integer(f_reader, priority_tag);
      ASSERT_TRUE(dec.ok());
      priority_val = dec.value();
    }
  }
  EXPECT_EQ(priority_val, 5);
}

