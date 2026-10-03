#pragma once

#include <asn1/runtime/ber/tlv.hpp>
#include <asn1/runtime/bigint.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace ber {

// ---- Content codecs (no tag/length) ----

void encode_boolean_content(ByteWriter& out, bool value);
Result<bool> decode_boolean_content(ByteReader& in, std::size_t length);

/// Two's-complement minimal encoding of a signed integer.
void encode_integer_content(ByteWriter& out, std::int64_t value);
void encode_integer_content(ByteWriter& out, const BigInteger& value);
Result<std::int64_t> decode_integer_content(ByteReader& in, std::size_t length);
Result<BigInteger> decode_big_integer_content(ByteReader& in, std::size_t length);

void encode_null_content(ByteWriter& out);
Result<void> decode_null_content(ByteReader& in, std::size_t length);

void encode_octet_string_content(ByteWriter& out, Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_octet_string_content(ByteReader& in,
                                                              std::size_t length);

/// Bit string as bytes with `unused_bits` in the last octet (0..7).
void encode_bit_string_content(ByteWriter& out, Span<const std::uint8_t> bits,
                               std::uint8_t unused_bits);
struct BitStringValue {
  std::vector<std::uint8_t> bits;
  std::uint8_t unused_bits = 0;
};
Result<BitStringValue> decode_bit_string_content(ByteReader& in, std::size_t length);

void encode_utf8_string_content(ByteWriter& out, const std::string& value);
Result<std::string> decode_utf8_string_content(ByteReader& in, std::size_t length);

// ---- Full TLV codecs (universal tags by default) ----

void encode_boolean(ByteWriter& out, bool value, Tag tag = universal(kTagBoolean));
Result<bool> decode_boolean(ByteReader& in, Tag expected = universal(kTagBoolean));

void encode_integer(ByteWriter& out, std::int64_t value, Tag tag = universal(kTagInteger));
void encode_integer(ByteWriter& out, const BigInteger& value,
                    Tag tag = universal(kTagInteger));
Result<std::int64_t> decode_integer(ByteReader& in, Tag expected = universal(kTagInteger));
Result<BigInteger> decode_big_integer(ByteReader& in,
                                      Tag expected = universal(kTagInteger));

void encode_null(ByteWriter& out, Tag tag = universal(kTagNull));
Result<void> decode_null(ByteReader& in, Tag expected = universal(kTagNull));

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         Tag tag = universal(kTagOctetString));
Result<std::vector<std::uint8_t>> decode_octet_string(
    ByteReader& in, Tag expected = universal(kTagOctetString));

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       Tag tag = universal(kTagBitString));
Result<BitStringValue> decode_bit_string(ByteReader& in,
                                         Tag expected = universal(kTagBitString));

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        Tag tag = universal(kTagUtf8String));
Result<std::string> decode_utf8_string(ByteReader& in,
                                       Tag expected = universal(kTagUtf8String));

/// ENUMERATED content is identical to INTEGER (two's-complement).
void encode_enumerated_content(ByteWriter& out, std::int64_t value);
Result<std::int64_t> decode_enumerated_content(ByteReader& in, std::size_t length);
void encode_enumerated(ByteWriter& out, std::int64_t value,
                       Tag tag = universal(kTagEnumerated));
Result<std::int64_t> decode_enumerated(ByteReader& in,
                                       Tag expected = universal(kTagEnumerated));

/// OBJECT IDENTIFIER arcs (at least two for absolute OID). First two packed as 40*a+b.
void encode_object_identifier_content(ByteWriter& out,
                                      Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_object_identifier_content(ByteReader& in,
                                                                   std::size_t length);
void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              Tag tag = universal(kTagOid));
Result<std::vector<std::uint64_t>> decode_object_identifier(
    ByteReader& in, Tag expected = universal(kTagOid));

/// RELATIVE-OID: each arc is an independent base-128 subidentifier.
void encode_relative_oid_content(ByteWriter& out, Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_relative_oid_content(ByteReader& in,
                                                              std::size_t length);
void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs,
                         Tag tag = universal(kTagRelativeOid));
Result<std::vector<std::uint64_t>> decode_relative_oid(
    ByteReader& in, Tag expected = universal(kTagRelativeOid));

/// REAL (X.690): binary base-2 encoding of host `double`; empty content = 0;
/// specials 0x40/+INF, 0x41/-INF, 0x42/NaN, 0x43/-0. Decimal NR forms accepted on decode.
void encode_real_content(ByteWriter& out, double value);
Result<double> decode_real_content(ByteReader& in, std::size_t length);
void encode_real(ByteWriter& out, double value, Tag tag = universal(kTagReal));
Result<double> decode_real(ByteReader& in, Tag expected = universal(kTagReal));

/// Classic EXTERNAL value (X.680 associated SEQUENCE, UNIVERSAL 8).
struct ExternalValue {
  std::optional<std::vector<std::uint64_t>> direct_reference;
  std::optional<std::int64_t> indirect_reference;
  std::optional<std::string> data_value_descriptor;  // ObjectDescriptor
  enum class Encoding { SingleAsn1Type = 0, OctetAligned = 1, Arbitrary = 2 } encoding =
      Encoding::OctetAligned;
  std::vector<std::uint8_t> encoding_value;
  std::size_t arbitrary_bit_length = 0;  // used when encoding == Arbitrary
};

void encode_external(ByteWriter& out, const ExternalValue& value,
                     Tag tag = universal(kTagExternal, /*constructed=*/true));
Result<ExternalValue> decode_external(ByteReader& in,
                                      Tag expected = universal(kTagExternal, /*constructed=*/true));

/// Identification CHOICE shared by modern EXTERNAL / EMBEDDED PDV / CHARACTER STRING.
struct Identification {
  enum class Kind {
    Syntaxes = 0,
    Syntax = 1,
    PresentationContextId = 2,
    ContextNegotiation = 3,
    TransferSyntax = 4,
    Fixed = 5,
  } kind = Kind::Fixed;

  /// Used by Syntaxes (abstract + transfer), Syntax, ContextNegotiation (transfer),
  /// and TransferSyntax.
  std::vector<std::uint64_t> abstract_syntax;
  std::vector<std::uint64_t> transfer_syntax;
  std::int64_t presentation_context_id = 0;
};

/// EMBEDDED PDV (UNIVERSAL 11) associated SEQUENCE value.
struct EmbeddedPdvValue {
  Identification identification;
  std::optional<std::string> data_value_descriptor;
  std::vector<std::uint8_t> data_value;
};

void encode_embedded_pdv(ByteWriter& out, const EmbeddedPdvValue& value,
                         Tag tag = universal(kTagEmbeddedPdv, /*constructed=*/true));
Result<EmbeddedPdvValue> decode_embedded_pdv(
    ByteReader& in, Tag expected = universal(kTagEmbeddedPdv, /*constructed=*/true));

/// Unrestricted CHARACTER STRING (UNIVERSAL 29) associated SEQUENCE value.
struct CharacterStringValue {
  Identification identification;
  std::optional<std::string> data_value_descriptor;
  std::vector<std::uint8_t> string_value;
};

void encode_character_string(ByteWriter& out, const CharacterStringValue& value,
                             Tag tag = universal(kTagCharacterString, /*constructed=*/true));
Result<CharacterStringValue> decode_character_string(
    ByteReader& in, Tag expected = universal(kTagCharacterString, /*constructed=*/true));

/// Modern EXTERNAL associated SEQUENCE (X.680): same shape as EMBEDDED PDV, UNIVERSAL 8.
/// Distinct from classic `ExternalValue` (direct/indirect-reference + encoding CHOICE).
struct ModernExternalValue {
  Identification identification;
  std::optional<std::string> data_value_descriptor;
  std::vector<std::uint8_t> data_value;
};

void encode_external_modern(ByteWriter& out, const ModernExternalValue& value,
                            Tag tag = universal(kTagExternal, /*constructed=*/true));
Result<ModernExternalValue> decode_external_modern(
    ByteReader& in, Tag expected = universal(kTagExternal, /*constructed=*/true));

/// Wrap already-encoded component TLVs in a constructed SEQUENCE/SET (or explicit tag).
void encode_constructed(ByteWriter& out, Tag tag, Span<const std::uint8_t> components);

/// Decode a constructed value; returns the raw content bytes (definite) or reads until EOC
/// (indefinite) and returns concatenated inner encodings.
Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, Tag expected);

}  // namespace ber
}  // namespace asn1
