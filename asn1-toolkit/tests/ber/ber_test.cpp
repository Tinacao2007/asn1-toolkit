#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/ber/tlv.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
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

TEST(BerCodec, AssociatedPdvConstructedOctetAndModernExternal) {
  // EMBEDDED PDV with constructed [2] OCTET STRING fragments.
  // A0 02 85 00 | A2 08 04 02 AA BB 04 02 CC DD
  {
    const auto enc = hex({0x2B, 0x0E, 0xA0, 0x02, 0x85, 0x00, 0xA2, 0x08, 0x04, 0x02, 0xAA,
                          0xBB, 0x04, 0x02, 0xCC, 0xDD});
    asn1::ByteReader r(enc);
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Fixed);
    ASSERT_EQ(d.value().data_value.size(), 4u);
    EXPECT_EQ(d.value().data_value[0], 0xAA);
    EXPECT_EQ(d.value().data_value[3], 0xDD);
  }
  // Indefinite-length outer SEQUENCE (EOC).
  {
    const auto enc =
        hex({0x2B, 0x80, 0xA0, 0x02, 0x85, 0x00, 0x82, 0x01, 0x7E, 0x00, 0x00});
    asn1::ByteReader r(enc);
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    ASSERT_EQ(d.value().data_value.size(), 1u);
    EXPECT_EQ(d.value().data_value[0], 0x7E);
  }
  // Modern EXTERNAL (UNIVERSAL 8) shares associated SEQUENCE shape.
  {
    asn1::ber::ModernExternalValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Fixed;
    v.data_value_descriptor = "d";
    v.data_value = {0x01};
    asn1::ByteWriter w;
    asn1::ber::encode_external_modern(w, v);
    expect_bytes(w.buffer(),
                 hex({0x28, 0x0A, 0xA0, 0x02, 0x85, 0x00, 0x81, 0x01, 'd', 0x82, 0x01, 0x01}));
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_external_modern(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Fixed);
    ASSERT_TRUE(d.value().data_value_descriptor.has_value());
    EXPECT_EQ(*d.value().data_value_descriptor, "d");
    ASSERT_EQ(d.value().data_value.size(), 1u);
    EXPECT_EQ(d.value().data_value[0], 0x01);
  }
}

TEST(BerCodec, EmbeddedPdvAndCharacterString) {
  // fixed identification + data-value 0xAB
  // 2B 07 A0 02 85 00 82 01 AB
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Fixed;
    v.data_value = {0xAB};
    asn1::ByteWriter w;
    asn1::ber::encode_embedded_pdv(w, v);
    expect_bytes(w.buffer(), hex({0x2B, 0x07, 0xA0, 0x02, 0x85, 0x00, 0x82, 0x01, 0xAB}));
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Fixed);
    ASSERT_EQ(d.value().data_value.size(), 1u);
    EXPECT_EQ(d.value().data_value[0], 0xAB);
    EXPECT_FALSE(d.value().data_value_descriptor.has_value());
  }
  // syntax OID 1.2.3 + descriptor + value
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Syntax;
    v.identification.transfer_syntax = {1, 2, 3};
    v.data_value_descriptor = "x";
    v.data_value = {0x01, 0x02};
    asn1::ByteWriter w;
    asn1::ber::encode_embedded_pdv(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Syntax);
    ASSERT_EQ(d.value().identification.transfer_syntax.size(), 3u);
    EXPECT_EQ(d.value().identification.transfer_syntax[0], 1u);
    EXPECT_EQ(d.value().identification.transfer_syntax[2], 3u);
    ASSERT_TRUE(d.value().data_value_descriptor.has_value());
    EXPECT_EQ(*d.value().data_value_descriptor, "x");
    ASSERT_EQ(d.value().data_value.size(), 2u);
    EXPECT_EQ(d.value().data_value[1], 0x02);
  }
  // syntaxes + context-negotiation + transfer-syntax + presentation-context-id round-trips
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Syntaxes;
    v.identification.abstract_syntax = {1, 2, 3};
    v.identification.transfer_syntax = {1, 2, 4};
    v.data_value = {0xEE};
    asn1::ByteWriter w;
    asn1::ber::encode_embedded_pdv(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Syntaxes);
    EXPECT_EQ(d.value().identification.abstract_syntax, (std::vector<std::uint64_t>{1, 2, 3}));
    EXPECT_EQ(d.value().identification.transfer_syntax, (std::vector<std::uint64_t>{1, 2, 4}));
  }
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::ContextNegotiation;
    v.identification.presentation_context_id = 7;
    v.identification.transfer_syntax = {1, 3, 6};
    v.data_value = {0x00};
    asn1::ByteWriter w;
    asn1::ber::encode_embedded_pdv(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind,
              asn1::ber::Identification::Kind::ContextNegotiation);
    EXPECT_EQ(d.value().identification.presentation_context_id, 7);
    EXPECT_EQ(d.value().identification.transfer_syntax, (std::vector<std::uint64_t>{1, 3, 6}));
  }
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::PresentationContextId;
    v.identification.presentation_context_id = 42;
    v.data_value = {0xFF};
    asn1::ByteWriter w;
    asn1::ber::encode_embedded_pdv(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind,
              asn1::ber::Identification::Kind::PresentationContextId);
    EXPECT_EQ(d.value().identification.presentation_context_id, 42);
  }
  {
    asn1::ber::EmbeddedPdvValue v;
    v.identification.kind = asn1::ber::Identification::Kind::TransferSyntax;
    v.identification.transfer_syntax = {2, 1};
    v.data_value = {0x10};
    asn1::ByteWriter w;
    asn1::ber::encode_embedded_pdv(w, v);
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_embedded_pdv(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::TransferSyntax);
    EXPECT_EQ(d.value().identification.transfer_syntax, (std::vector<std::uint64_t>{2, 1}));
  }
  // CHARACTER STRING UNIVERSAL 29 (0x3D constructed)
  {
    asn1::ber::CharacterStringValue v;
    v.identification.kind = asn1::ber::Identification::Kind::Fixed;
    v.string_value = {0x41, 0x42};
    asn1::ByteWriter w;
    asn1::ber::encode_character_string(w, v);
    expect_bytes(w.buffer(),
                 hex({0x3D, 0x08, 0xA0, 0x02, 0x85, 0x00, 0x82, 0x02, 0x41, 0x42}));
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_character_string(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().identification.kind, asn1::ber::Identification::Kind::Fixed);
    ASSERT_EQ(d.value().string_value.size(), 2u);
    EXPECT_EQ(d.value().string_value[0], 0x41);
    EXPECT_EQ(d.value().string_value[1], 0x42);
  }
}

TEST(BerCodec, ExternalClassicVectors) {
  {
    asn1::ber::ExternalValue v;
    v.encoding = asn1::ber::ExternalValue::Encoding::OctetAligned;
    v.encoding_value = {0x12};
    asn1::ByteWriter w;
    asn1::ber::encode_external(w, v);
    expect_bytes(w.buffer(), hex({0x28, 0x03, 0x81, 0x01, 0x12}));
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_external(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().encoding, asn1::ber::ExternalValue::Encoding::OctetAligned);
    ASSERT_EQ(d.value().encoding_value.size(), 1u);
    EXPECT_EQ(d.value().encoding_value[0], 0x12);
  }
  {
    asn1::ber::ExternalValue v;
    v.data_value_descriptor = "12";
    v.encoding = asn1::ber::ExternalValue::Encoding::OctetAligned;
    v.encoding_value = {0x34};
    asn1::ByteWriter w;
    asn1::ber::encode_external(w, v);
    expect_bytes(w.buffer(), hex({0x28, 0x07, 0x07, 0x02, '1', '2', 0x81, 0x01, 0x34}));
    asn1::ByteReader r(w.buffer());
    auto d = asn1::ber::decode_external(r);
    ASSERT_TRUE(d.ok()) << d.error().message;
    ASSERT_TRUE(d.value().data_value_descriptor.has_value());
    EXPECT_EQ(*d.value().data_value_descriptor, "12");
    ASSERT_EQ(d.value().encoding_value.size(), 1u);
    EXPECT_EQ(d.value().encoding_value[0], 0x34);
  }
}

TEST(BerCodec, RealKnownVectors) {
  struct Case {
    double value;
    std::vector<std::uint8_t> encoding;
  };
  const Case cases[] = {
      {0.0, hex({0x09, 0x00})},
      {1.0, hex({0x09, 0x03, 0x80, 0x00, 0x01})},
      {100.0, hex({0x09, 0x03, 0x80, 0x02, 0x19})},
      {-100.0, hex({0x09, 0x03, 0xC0, 0x02, 0x19})},
      {8.0, hex({0x09, 0x03, 0x80, 0x03, 0x01})},
      {0.625, hex({0x09, 0x03, 0x80, 0xFD, 0x05})},
      {1.1, hex({0x09, 0x09, 0x80, 0xCD, 0x08, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCD})},
      {std::numeric_limits<double>::infinity(), hex({0x09, 0x01, 0x40})},
      {-std::numeric_limits<double>::infinity(), hex({0x09, 0x01, 0x41})},
  };
  for (const auto& c : cases) {
    asn1::ByteWriter w;
    asn1::ber::encode_real(w, c.value);
    expect_bytes(w.buffer(), c.encoding);

    asn1::ByteReader r(c.encoding);
    auto v = asn1::ber::decode_real(r);
    ASSERT_TRUE(v.ok()) << c.encoding.size();
    EXPECT_EQ(v.value(), c.value);
  }

  {
    asn1::ByteWriter w;
    asn1::ber::encode_real(w, std::numeric_limits<double>::quiet_NaN());
    expect_bytes(w.buffer(), hex({0x09, 0x01, 0x42}));
    asn1::ByteReader r(w.buffer());
    auto v = asn1::ber::decode_real(r);
    ASSERT_TRUE(v.ok());
    EXPECT_TRUE(std::isnan(v.value()));
  }

  // Decimal NR form (decode only).
  {
    auto enc = hex({0x09, 0x05, 0x03, '1', '.', 'E', '2'});
    asn1::ByteReader r(enc);
    auto v = asn1::ber::decode_real(r);
    ASSERT_TRUE(v.ok());
    EXPECT_DOUBLE_EQ(v.value(), 100.0);
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
    EXPECT_EQ(v.value().bit_length, 4u);
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
