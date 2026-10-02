#pragma once

#include <asn1/runtime/ber/tlv.hpp>

namespace asn1 {
namespace der {

/// Decode length under DER rules: definite only, shortest form, no leading 0x00
/// in long-form length octets.
Result<ber::Length> decode_length(ByteReader& in);

Result<ber::TlvHeader> decode_tlv_header(ByteReader& in);

/// Encode tag using BER rules (already DER-canonical in our encoder).
using ber::encode_tag;

/// Encode definite length (shortest form). Same as BER encoder.
using ber::encode_length;

using ber::encode_tlv;

}  // namespace der
}  // namespace asn1
