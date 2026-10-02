#pragma once

#include <asn1/runtime/ber/codec.hpp>
#include <asn1/runtime/der/tlv.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace der {

// DER encode reuses BER content encoders (already minimal / 0xFF TRUE).

void encode_boolean(ByteWriter& out, bool value,
                    ber::Tag tag = ber::universal(ber::kTagBoolean));
Result<bool> decode_boolean(ByteReader& in,
                            ber::Tag expected = ber::universal(ber::kTagBoolean));

void encode_integer(ByteWriter& out, std::int64_t value,
                    ber::Tag tag = ber::universal(ber::kTagInteger));
Result<std::int64_t> decode_integer(ByteReader& in,
                                    ber::Tag expected = ber::universal(ber::kTagInteger));

void encode_null(ByteWriter& out, ber::Tag tag = ber::universal(ber::kTagNull));
Result<void> decode_null(ByteReader& in,
                         ber::Tag expected = ber::universal(ber::kTagNull));

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         ber::Tag tag = ber::universal(ber::kTagOctetString));
Result<std::vector<std::uint8_t>> decode_octet_string(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagOctetString));

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits,
                       std::uint8_t unused_bits,
                       ber::Tag tag = ber::universal(ber::kTagBitString));
Result<ber::BitStringValue> decode_bit_string(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagBitString));

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        ber::Tag tag = ber::universal(ber::kTagUtf8String));
Result<std::string> decode_utf8_string(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagUtf8String));

void encode_enumerated(ByteWriter& out, std::int64_t value,
                       ber::Tag tag = ber::universal(ber::kTagEnumerated));
Result<std::int64_t> decode_enumerated(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagEnumerated));

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              ber::Tag tag = ber::universal(ber::kTagOid));
Result<std::vector<std::uint64_t>> decode_object_identifier(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagOid));

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs,
                         ber::Tag tag = ber::universal(ber::kTagRelativeOid));
Result<std::vector<std::uint64_t>> decode_relative_oid(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagRelativeOid));

/// SEQUENCE: definite constructed; component order preserved.
void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components,
                     ber::Tag tag = ber::universal(ber::kTagSequence, true));
Result<std::vector<std::uint8_t>> decode_sequence(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagSequence, true));

/// SET: sorts component TLVs by ascending tag encoding before wrap.
/// `components` are complete TLV encodings of each SET member.
void encode_set(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                ber::Tag tag = ber::universal(ber::kTagSet, true));

/// Decode SET content; requires ascending tag-encoding order (DER).
Result<std::vector<std::uint8_t>> decode_set(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagSet, true));

}  // namespace der
}  // namespace asn1
