#pragma once

#include <asn1/runtime/ber/tlv.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace ber {

// ---- Content codecs (no tag/length) ----

void encode_boolean_content(ByteWriter& out, bool value);
Result<bool> decode_boolean_content(ByteReader& in, std::size_t length);

/// Two's-complement minimal encoding of a signed 64-bit integer.
void encode_integer_content(ByteWriter& out, std::int64_t value);
Result<std::int64_t> decode_integer_content(ByteReader& in, std::size_t length);

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
Result<std::int64_t> decode_integer(ByteReader& in, Tag expected = universal(kTagInteger));

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

/// Wrap already-encoded component TLVs in a constructed SEQUENCE/SET (or explicit tag).
void encode_constructed(ByteWriter& out, Tag tag, Span<const std::uint8_t> components);

/// Decode a constructed value; returns the raw content bytes (definite) or reads until EOC
/// (indefinite) and returns concatenated inner encodings.
Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, Tag expected);

}  // namespace ber
}  // namespace asn1
