/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/cer/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
**
**  File Description:
**
**   CER encode/decode API (canonical subset of BER).
**
** Specification: ITU-T X.690 — Canonical Encoding Rules (CER).
**                 Clause 9 (CER restrictions) + clause 11 (restrictions
**                 common to CER and DER).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
**************************************************************************/
#pragma once

#include <asn1/runtime/ber/codec.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace cer {

/// CER is the BER basic encoding plus:
///  - 9.1: constructed encodings use the INDEFINITE length form
///         (tag, 0x80, contents, 0x00 0x00); primitive encodings use the
///         fewest length octets;
///  - 9.2: bitstring / octetstring / restricted character string values are
///         primitive when they need <= 1000 contents octets, otherwise
///         constructed with primitive fragments of exactly 1000 contents
///         octets each (last fragment 1..1000);
///  - 9.3: SET components in tag order (an untagged CHOICE component is
///         ordered as though it carried the smallest tag in the choice —
///         the emitter pre-sorts those, see encode_set_ordered);
///  - 11.6: SET OF components in ascending encoding order (shorter
///         components padded with 0-octets for comparison only);
///  - 11.5: DEFAULT components are omitted.
/// Clause 11 rules shared with DER (TRUE = 0xFF, zeroed unused bits, minimal
/// real encodings) are already satisfied by the BER primitives this library
/// reuses.

// ---- Primitives (same minimal/definite encodings as BER/DER) ----

void encode_boolean(ByteWriter& out, bool value,
                    ber::Tag tag = ber::universal(ber::kTagBoolean));
Result<bool> decode_boolean(ByteReader& in,
                            ber::Tag expected = ber::universal(ber::kTagBoolean));

void encode_integer(ByteWriter& out, std::int64_t value,
                    ber::Tag tag = ber::universal(ber::kTagInteger));
void encode_integer(ByteWriter& out, const BigInteger& value,
                    ber::Tag tag = ber::universal(ber::kTagInteger));
Result<std::int64_t> decode_integer(ByteReader& in,
                                    ber::Tag expected = ber::universal(ber::kTagInteger));
Result<BigInteger> decode_big_integer(ByteReader& in,
                                      ber::Tag expected = ber::universal(ber::kTagInteger));

void encode_null(ByteWriter& out, ber::Tag tag = ber::universal(ber::kTagNull));
Result<void> decode_null(ByteReader& in, ber::Tag expected = ber::universal(ber::kTagNull));

void encode_enumerated(ByteWriter& out, std::int64_t value,
                       ber::Tag tag = ber::universal(ber::kTagEnumerated));
Result<std::int64_t> decode_enumerated(ByteReader& in,
                                       ber::Tag expected = ber::universal(ber::kTagEnumerated));

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              ber::Tag tag = ber::universal(ber::kTagOid));
Result<std::vector<std::uint64_t>> decode_object_identifier(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagOid));

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs,
                         ber::Tag tag = ber::universal(ber::kTagRelativeOid));
Result<std::vector<std::uint64_t>> decode_relative_oid(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagRelativeOid));

void encode_real(ByteWriter& out, double value, ber::Tag tag = ber::universal(ber::kTagReal));
Result<double> decode_real(ByteReader& in, ber::Tag expected = ber::universal(ber::kTagReal));

// ---- Strings (9.2 fragmentation) ----

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         ber::Tag tag = ber::universal(ber::kTagOctetString));
Result<std::vector<std::uint8_t>> decode_octet_string(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagOctetString));

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       ber::Tag tag = ber::universal(ber::kTagBitString));
Result<ber::BitStringValue> decode_bit_string(ByteReader& in,
                                              ber::Tag expected = ber::universal(ber::kTagBitString));

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        ber::Tag tag = ber::universal(ber::kTagUtf8String));
Result<std::string> decode_utf8_string(ByteReader& in,
                                       ber::Tag expected = ber::universal(ber::kTagUtf8String));

// ---- Constructed wrappers (9.1 indefinite length) ----

/// SEQUENCE / SEQUENCE OF: indefinite constructed wrapper, component order
/// preserved.
void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components,
                     ber::Tag tag = ber::universal(ber::kTagSequence, true));
Result<std::vector<std::uint8_t>> decode_sequence(
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagSequence, true));

/// Generic constructed wrapper (EXPLICIT tags, INSTANCE OF bodies, ...).
void encode_constructed(ByteWriter& out, ber::Tag tag, Span<const std::uint8_t> components);
Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, ber::Tag expected);

/// SET: sorts component TLVs by ascending tag encoding before wrap
/// (X.690 9.3, X.680 8.6), then indefinite wrapper.
void encode_set(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                ber::Tag tag = ber::universal(ber::kTagSet, true));
Result<std::vector<std::uint8_t>> decode_set(ByteReader& in,
                                             ber::Tag expected = ber::universal(ber::kTagSet, true));

/// SET whose components are ALREADY in CER order (X.690 9.3): the emitter
/// sorts the components statically at compile time — an untagged CHOICE
/// component is keyed by the smallest tag in the choice, which the runtime
/// cannot see inside an encoded value. Keeps the given order; only wraps
/// indefinite.
void encode_set_ordered(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                        ber::Tag tag = ber::universal(ber::kTagSet, true));

}  // namespace cer
}  // namespace asn1
