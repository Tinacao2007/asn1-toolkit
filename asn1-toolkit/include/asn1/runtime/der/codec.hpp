/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/der/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   DER encode/decode API (canonical subset of BER).
**
** Specification: ITU-T X.690 — Distinguished Encoding Rules (DER),
**                 canonical BER subset.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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
                    /**
                     *  Function    : universal
                     *  Description : Computes universal from (none).
                     *  Parameters  : none
                     *  Returns     : ber::Tag tag = ber::
                     */
                    ber::Tag tag = ber::universal(ber::kTagBoolean));
Result<bool> decode_boolean(ByteReader& in,
                            /**
                             *  Function    : universal
                             *  Description : Computes universal from (none).
                             *  Parameters  : none
                             *  Returns     : ber::Tag expected = ber::
                             */
                            ber::Tag expected = ber::universal(ber::kTagBoolean));

void encode_integer(ByteWriter& out, std::int64_t value,
                    /**
                     *  Function    : universal
                     *  Description : Computes universal from (none).
                     *  Parameters  : none
                     *  Returns     : ber::Tag tag = ber::
                     */
                    ber::Tag tag = ber::universal(ber::kTagInteger));
void encode_integer(ByteWriter& out, const BigInteger& value,
                    /**
                     *  Function    : universal
                     *  Description : Computes universal from (none).
                     *  Parameters  : none
                     *  Returns     : ber::Tag tag = ber::
                     */
                    ber::Tag tag = ber::universal(ber::kTagInteger));
Result<std::int64_t> decode_integer(ByteReader& in,
                                    /**
                                     *  Function    : universal
                                     *  Description : Computes universal from (none).
                                     *  Parameters  : none
                                     *  Returns     : ber::Tag expected = ber::
                                     */
                                    ber::Tag expected = ber::universal(ber::kTagInteger));
Result<BigInteger> decode_big_integer(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagInteger));

/**
 *  Function    : universal
 *  Description : Performs universal (declaration).
 *  Parameters  : out — ByteWriter& out; tag — ber::Tag tag
 *  Returns     : void encode_null(ByteWriter& out, ber::Tag tag = ber::
 */
void encode_null(ByteWriter& out, ber::Tag tag = ber::universal(ber::kTagNull));
Result<void> decode_null(ByteReader& in,
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (none).
                          *  Parameters  : none
                          *  Returns     : ber::Tag expected = ber::
                          */
                         ber::Tag expected = ber::universal(ber::kTagNull));

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (none).
                          *  Parameters  : none
                          *  Returns     : ber::Tag tag = ber::
                          */
                         ber::Tag tag = ber::universal(ber::kTagOctetString));
Result<std::vector<std::uint8_t>> decode_octet_string(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagOctetString));

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits,
                       std::uint8_t unused_bits,
                       /**
                        *  Function    : universal
                        *  Description : Computes universal from (none).
                        *  Parameters  : none
                        *  Returns     : ber::Tag tag = ber::
                        */
                       ber::Tag tag = ber::universal(ber::kTagBitString));
Result<ber::BitStringValue> decode_bit_string(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagBitString));

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        /**
                         *  Function    : universal
                         *  Description : Computes universal from (none).
                         *  Parameters  : none
                         *  Returns     : ber::Tag tag = ber::
                         */
                        ber::Tag tag = ber::universal(ber::kTagUtf8String));
Result<std::string> decode_utf8_string(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagUtf8String));

void encode_enumerated(ByteWriter& out, std::int64_t value,
                       /**
                        *  Function    : universal
                        *  Description : Computes universal from (none).
                        *  Parameters  : none
                        *  Returns     : ber::Tag tag = ber::
                        */
                       ber::Tag tag = ber::universal(ber::kTagEnumerated));
Result<std::int64_t> decode_enumerated(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagEnumerated));

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              /**
                               *  Function    : universal
                               *  Description : Computes universal from (none).
                               *  Parameters  : none
                               *  Returns     : ber::Tag tag = ber::
                               */
                              ber::Tag tag = ber::universal(ber::kTagOid));
Result<std::vector<std::uint64_t>> decode_object_identifier(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagOid));

void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs,
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (none).
                          *  Parameters  : none
                          *  Returns     : ber::Tag tag = ber::
                          */
                         ber::Tag tag = ber::universal(ber::kTagRelativeOid));
Result<std::vector<std::uint64_t>> decode_relative_oid(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagRelativeOid));

void encode_real(ByteWriter& out, double value,
                 /**
                  *  Function    : universal
                  *  Description : Computes universal from (none).
                  *  Parameters  : none
                  *  Returns     : ber::Tag tag = ber::
                  */
                 ber::Tag tag = ber::universal(ber::kTagReal));
Result<double> decode_real(ByteReader& in,
                           /**
                            *  Function    : universal
                            *  Description : Computes universal from (none).
                            *  Parameters  : none
                            *  Returns     : ber::Tag expected = ber::
                            */
                           ber::Tag expected = ber::universal(ber::kTagReal));

/// SEQUENCE: definite constructed; component order preserved.
void encode_sequence(ByteWriter& out, Span<const std::uint8_t> components,
                     /**
                      *  Function    : universal
                      *  Description : Computes universal from (ber::kTagSequence).
                      *  Parameters  : ber::kTagSequence — ber::kTagSequence
                      *  Returns     : ber::Tag tag = ber::
                      */
                     ber::Tag tag = ber::universal(ber::kTagSequence, true));
Result<std::vector<std::uint8_t>> decode_sequence(
    /**
     *  Function    : universal
     *  Description : Computes universal from (ber::kTagSequence).
     *  Parameters  : ber::kTagSequence — ber::kTagSequence
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagSequence, true));

/// SET: sorts component TLVs by ascending tag encoding before wrap.
/// `components` are complete TLV encodings of each SET member.
void encode_set(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components,
                /**
                 *  Function    : universal
                 *  Description : Computes universal from (ber::kTagSet).
                 *  Parameters  : ber::kTagSet — ber::kTagSet
                 *  Returns     : ber::Tag tag = ber::
                 */
                ber::Tag tag = ber::universal(ber::kTagSet, true));

/// Decode SET content; requires ascending tag-encoding order (DER).
Result<std::vector<std::uint8_t>> decode_set(
    /**
     *  Function    : universal
     *  Description : Computes universal from (ber::kTagSet).
     *  Parameters  : ber::kTagSet — ber::kTagSet
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagSet, true));

/// Associated SEQUENCE types: encode reuses BER (already definite/minimal);
/// decode rejects indefinite length and constructed string components.
void encode_embedded_pdv(ByteWriter& out, const ber::EmbeddedPdvValue& value,
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (ber::kTagEmbeddedPdv).
                          *  Parameters  : ber::kTagEmbeddedPdv — ber::kTagEmbeddedPdv
                          *  Returns     : ber::Tag tag = ber::
                          */
                         ber::Tag tag = ber::universal(ber::kTagEmbeddedPdv, true));
Result<ber::EmbeddedPdvValue> decode_embedded_pdv(
    /**
     *  Function    : universal
     *  Description : Computes universal from (ber::kTagEmbeddedPdv).
     *  Parameters  : ber::kTagEmbeddedPdv — ber::kTagEmbeddedPdv
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagEmbeddedPdv, true));

void encode_character_string(ByteWriter& out, const ber::CharacterStringValue& value,
                             /**
                              *  Function    : universal
                              *  Description : Computes universal from (ber::kTagCharacterString).
                              *  Parameters  : ber::kTagCharacterString — ber::kTagCharacterString
                              *  Returns     : ber::Tag tag = ber::
                              */
                             ber::Tag tag = ber::universal(ber::kTagCharacterString, true));
Result<ber::CharacterStringValue> decode_character_string(
    /**
     *  Function    : universal
     *  Description : Computes universal from (ber::kTagCharacterString).
     *  Parameters  : ber::kTagCharacterString — ber::kTagCharacterString
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagCharacterString, true));

void encode_external_modern(ByteWriter& out, const ber::ModernExternalValue& value,
                            /**
                             *  Function    : universal
                             *  Description : Computes universal from (ber::kTagExternal).
                             *  Parameters  : ber::kTagExternal — ber::kTagExternal
                             *  Returns     : ber::Tag tag = ber::
                             */
                            ber::Tag tag = ber::universal(ber::kTagExternal, true));
Result<ber::ModernExternalValue> decode_external_modern(
    /**
     *  Function    : universal
     *  Description : Computes universal from (ber::kTagExternal).
     *  Parameters  : ber::kTagExternal — ber::kTagExternal
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagExternal, true));

void encode_external(ByteWriter& out, const ber::ExternalValue& value,
                     /**
                      *  Function    : universal
                      *  Description : Computes universal from (ber::kTagExternal).
                      *  Parameters  : ber::kTagExternal — ber::kTagExternal
                      *  Returns     : ber::Tag tag = ber::
                      */
                     ber::Tag tag = ber::universal(ber::kTagExternal, true));
Result<ber::ExternalValue> decode_external(
    /**
     *  Function    : universal
     *  Description : Computes universal from (ber::kTagExternal).
     *  Parameters  : ber::kTagExternal — ber::kTagExternal
     *  Returns     : ByteReader& in, ber::Tag expected = ber::
     */
    ByteReader& in, ber::Tag expected = ber::universal(ber::kTagExternal, true));

}  // namespace der
}  // namespace asn1
