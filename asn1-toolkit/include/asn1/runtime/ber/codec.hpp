/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/ber/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   BER encode/decode API for ASN.1 values.
**
** Specification: ITU-T X.690 — ASN.1 encoding rules: Basic Encoding
**                 Rules (BER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/bit_string.hpp>
#include <asn1/runtime/ber/tlv.hpp>
#include <asn1/runtime/bigint.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace ber {

// ---- Content codecs (no tag/length) ----

/**
 *  Function    : encode_boolean_content
 *  Description : Performs encode boolean content (declaration).
 *  Parameters  : out — ByteWriter& out; value — bool value
 *  Returns     : void
 */
void encode_boolean_content(ByteWriter& out, bool value);
/**
 *  Function    : decode_boolean_content
 *  Description : Returns a boolean result from in, length.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean_content(ByteReader& in, std::size_t length);

/// Two's-complement minimal encoding of a signed integer.
/**
 *  Function    : encode_integer_content
 *  Description : Performs encode integer content (declaration).
 *  Parameters  : out — ByteWriter& out; value — std::int64_t value
 *  Returns     : void
 */
void encode_integer_content(ByteWriter& out, std::int64_t value);
/**
 *  Function    : encode_integer_content
 *  Description : Performs encode integer content (declaration).
 *  Parameters  : out — ByteWriter& out; value — const BigInteger& value
 *  Returns     : void
 */
void encode_integer_content(ByteWriter& out, const BigInteger& value);
/**
 *  Function    : decode_integer_content
 *  Description : Returns success or an error from decode integer content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer_content(ByteReader& in, std::size_t length);
/**
 *  Function    : decode_big_integer_content
 *  Description : Returns success or an error from decode big integer content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer_content(ByteReader& in, std::size_t length);

/**
 *  Function    : encode_null_content
 *  Description : Performs encode null content (declaration).
 *  Parameters  : out — ByteWriter& out
 *  Returns     : void
 */
void encode_null_content(ByteWriter& out);
/**
 *  Function    : decode_null_content
 *  Description : Returns success or an error from decode null content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<void>
 */
Result<void> decode_null_content(ByteReader& in, std::size_t length);

/**
 *  Function    : encode_octet_string_content
 *  Description : Performs encode octet string content (declaration).
 *  Parameters  : out — ByteWriter& out; value — Span<const std::uint8_t> value
 *  Returns     : void
 */
void encode_octet_string_content(ByteWriter& out, Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_octet_string_content(ByteReader& in,
                                                              std::size_t length);

/// Bit string as bytes with `unused_bits` in the last octet (0..7).
void encode_bit_string_content(ByteWriter& out, Span<const std::uint8_t> bits,
                               std::uint8_t unused_bits);
using BitStringValue = asn1::BitStringValue;
/**
 *  Function    : decode_bit_string_content
 *  Description : Returns success or an error from decode bit string content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_string_content(ByteReader& in, std::size_t length);

/**
 *  Function    : encode_utf8_string_content
 *  Description : Performs encode utf8 string content (declaration).
 *  Parameters  : out — ByteWriter& out; value — const std::string& value
 *  Returns     : void
 */
void encode_utf8_string_content(ByteWriter& out, const std::string& value);
/**
 *  Function    : decode_utf8_string_content
 *  Description : Builds and returns a string for decode utf8 string content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string_content(ByteReader& in, std::size_t length);

// ---- Full TLV codecs (universal tags by default) ----

/**
 *  Function    : universal
 *  Description : Returns a boolean result from out, value, tag.
 *  Parameters  : out — ByteWriter& out; value — bool value; tag — Tag tag
 *  Returns     : void encode_boolean(ByteWriter& out, bool value, Tag tag =
 */
void encode_boolean(ByteWriter& out, bool value, Tag tag = universal(kTagBoolean));
/**
 *  Function    : universal
 *  Description : Returns a boolean result from in, expected.
 *  Parameters  : in — ByteReader& in; expected — Tag expected
 *  Returns     : Result<bool> decode_boolean(ByteReader& in, Tag expected =
 */
Result<bool> decode_boolean(ByteReader& in, Tag expected = universal(kTagBoolean));

/**
 *  Function    : universal
 *  Description : Performs universal (declaration).
 *  Parameters  : out — ByteWriter& out; value — std::int64_t value; tag — Tag tag
 *  Returns     : void encode_integer(ByteWriter& out, std::int64_t value, Tag tag =
 */
void encode_integer(ByteWriter& out, std::int64_t value, Tag tag = universal(kTagInteger));
void encode_integer(ByteWriter& out, const BigInteger& value,
                    /**
                     *  Function    : universal
                     *  Description : Computes universal from (none).
                     *  Parameters  : none
                     *  Returns     : Tag tag =
                     */
                    Tag tag = universal(kTagInteger));
/**
 *  Function    : universal
 *  Description : Returns success or an error from universal.
 *  Parameters  : in — ByteReader& in; expected — Tag expected
 *  Returns     : Result<std::int64_t> decode_integer(ByteReader& in, Tag expected =
 */
Result<std::int64_t> decode_integer(ByteReader& in, Tag expected = universal(kTagInteger));
Result<BigInteger> decode_big_integer(ByteReader& in,
                                      /**
                                       *  Function    : universal
                                       *  Description : Computes universal from (none).
                                       *  Parameters  : none
                                       *  Returns     : Tag expected =
                                       */
                                      Tag expected = universal(kTagInteger));

/**
 *  Function    : universal
 *  Description : Performs universal (declaration).
 *  Parameters  : out — ByteWriter& out; tag — Tag tag
 *  Returns     : void encode_null(ByteWriter& out, Tag tag =
 */
void encode_null(ByteWriter& out, Tag tag = universal(kTagNull));
/**
 *  Function    : universal
 *  Description : Returns success or an error from universal.
 *  Parameters  : in — ByteReader& in; expected — Tag expected
 *  Returns     : Result<void> decode_null(ByteReader& in, Tag expected =
 */
Result<void> decode_null(ByteReader& in, Tag expected = universal(kTagNull));

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (none).
                          *  Parameters  : none
                          *  Returns     : Tag tag =
                          */
                         Tag tag = universal(kTagOctetString));
Result<std::vector<std::uint8_t>> decode_octet_string(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, Tag expected =
     */
    ByteReader& in, Tag expected = universal(kTagOctetString));

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::uint8_t unused_bits,
                       /**
                        *  Function    : universal
                        *  Description : Computes universal from (none).
                        *  Parameters  : none
                        *  Returns     : Tag tag =
                        */
                       Tag tag = universal(kTagBitString));
Result<BitStringValue> decode_bit_string(ByteReader& in,
                                         /**
                                          *  Function    : universal
                                          *  Description : Computes universal from (none).
                                          *  Parameters  : none
                                          *  Returns     : Tag expected =
                                          */
                                         Tag expected = universal(kTagBitString));

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        /**
                         *  Function    : universal
                         *  Description : Computes universal from (none).
                         *  Parameters  : none
                         *  Returns     : Tag tag =
                         */
                        Tag tag = universal(kTagUtf8String));
Result<std::string> decode_utf8_string(ByteReader& in,
                                       /**
                                        *  Function    : universal
                                        *  Description : Computes universal from (none).
                                        *  Parameters  : none
                                        *  Returns     : Tag expected =
                                        */
                                       Tag expected = universal(kTagUtf8String));

/// ENUMERATED content is identical to INTEGER (two's-complement).
/**
 *  Function    : encode_enumerated_content
 *  Description : Performs encode enumerated content (declaration).
 *  Parameters  : out — ByteWriter& out; value — std::int64_t value
 *  Returns     : void
 */
void encode_enumerated_content(ByteWriter& out, std::int64_t value);
/**
 *  Function    : decode_enumerated_content
 *  Description : Returns success or an error from decode enumerated content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_enumerated_content(ByteReader& in, std::size_t length);
void encode_enumerated(ByteWriter& out, std::int64_t value,
                       /**
                        *  Function    : universal
                        *  Description : Computes universal from (none).
                        *  Parameters  : none
                        *  Returns     : Tag tag =
                        */
                       Tag tag = universal(kTagEnumerated));
Result<std::int64_t> decode_enumerated(ByteReader& in,
                                       /**
                                        *  Function    : universal
                                        *  Description : Computes universal from (none).
                                        *  Parameters  : none
                                        *  Returns     : Tag expected =
                                        */
                                       Tag expected = universal(kTagEnumerated));

/// OBJECT IDENTIFIER arcs (at least two for absolute OID). First two packed as 40*a+b.
void encode_object_identifier_content(ByteWriter& out,
                                      Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_object_identifier_content(ByteReader& in,
                                                                   std::size_t length);
void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              /**
                               *  Function    : universal
                               *  Description : Computes universal from (none).
                               *  Parameters  : none
                               *  Returns     : Tag tag =
                               */
                              Tag tag = universal(kTagOid));
Result<std::vector<std::uint64_t>> decode_object_identifier(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, Tag expected =
     */
    ByteReader& in, Tag expected = universal(kTagOid));

/// RELATIVE-OID: each arc is an independent base-128 subidentifier.
/**
 *  Function    : encode_relative_oid_content
 *  Description : Performs encode relative oid content (declaration).
 *  Parameters  : out — ByteWriter& out; arcs — Span<const std::uint64_t> arcs
 *  Returns     : void
 */
void encode_relative_oid_content(ByteWriter& out, Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_relative_oid_content(ByteReader& in,
                                                              std::size_t length);
void encode_relative_oid(ByteWriter& out, Span<const std::uint64_t> arcs,
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (none).
                          *  Parameters  : none
                          *  Returns     : Tag tag =
                          */
                         Tag tag = universal(kTagRelativeOid));
Result<std::vector<std::uint64_t>> decode_relative_oid(
    /**
     *  Function    : universal
     *  Description : Computes universal from (none).
     *  Parameters  : none
     *  Returns     : ByteReader& in, Tag expected =
     */
    ByteReader& in, Tag expected = universal(kTagRelativeOid));

/// REAL (X.690): binary base-2 encoding of host `double`; empty content = 0;
/// specials 0x40/+INF, 0x41/-INF, 0x42/NaN, 0x43/-0. Decimal NR forms accepted on decode.
/**
 *  Function    : encode_real_content
 *  Description : Performs encode real content (declaration).
 *  Parameters  : out — ByteWriter& out; value — double value
 *  Returns     : void
 */
void encode_real_content(ByteWriter& out, double value);
/**
 *  Function    : decode_real_content
 *  Description : Returns success or an error from decode real content.
 *  Parameters  : in — ByteReader& in; length — std::size_t length
 *  Returns     : Result<double>
 */
Result<double> decode_real_content(ByteReader& in, std::size_t length);
/**
 *  Function    : universal
 *  Description : Performs universal (declaration).
 *  Parameters  : out — ByteWriter& out; value — double value; tag — Tag tag
 *  Returns     : void encode_real(ByteWriter& out, double value, Tag tag =
 */
void encode_real(ByteWriter& out, double value, Tag tag = universal(kTagReal));
/**
 *  Function    : universal
 *  Description : Returns success or an error from universal.
 *  Parameters  : in — ByteReader& in; expected — Tag expected
 *  Returns     : Result<double> decode_real(ByteReader& in, Tag expected =
 */
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
                     /**
                      *  Function    : universal
                      *  Description : Computes universal from (kTagExternal).
                      *  Parameters  : kTagExternal — kTagExternal
                      *  Returns     : Tag tag =
                      */
                     Tag tag = universal(kTagExternal, /*constructed=*/true));
Result<ExternalValue> decode_external(ByteReader& in,
                                      /**
                                       *  Function    : universal
                                       *  Description : Computes universal from (kTagExternal).
                                       *  Parameters  : kTagExternal — kTagExternal
                                       *  Returns     : Tag expected =
                                       */
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
                         /**
                          *  Function    : universal
                          *  Description : Computes universal from (kTagEmbeddedPdv).
                          *  Parameters  : kTagEmbeddedPdv — kTagEmbeddedPdv
                          *  Returns     : Tag tag =
                          */
                         Tag tag = universal(kTagEmbeddedPdv, /*constructed=*/true));
Result<EmbeddedPdvValue> decode_embedded_pdv(
    /**
     *  Function    : universal
     *  Description : Computes universal from (kTagEmbeddedPdv).
     *  Parameters  : kTagEmbeddedPdv — kTagEmbeddedPdv
     *  Returns     : ByteReader& in, Tag expected =
     */
    ByteReader& in, Tag expected = universal(kTagEmbeddedPdv, /*constructed=*/true));

/// Unrestricted CHARACTER STRING (UNIVERSAL 29) associated SEQUENCE value.
struct CharacterStringValue {
  Identification identification;
  std::optional<std::string> data_value_descriptor;
  std::vector<std::uint8_t> string_value;
};

void encode_character_string(ByteWriter& out, const CharacterStringValue& value,
                             /**
                              *  Function    : universal
                              *  Description : Computes universal from (kTagCharacterString).
                              *  Parameters  : kTagCharacterString — kTagCharacterString
                              *  Returns     : Tag tag =
                              */
                             Tag tag = universal(kTagCharacterString, /*constructed=*/true));
Result<CharacterStringValue> decode_character_string(
    /**
     *  Function    : universal
     *  Description : Computes universal from (kTagCharacterString).
     *  Parameters  : kTagCharacterString — kTagCharacterString
     *  Returns     : ByteReader& in, Tag expected =
     */
    ByteReader& in, Tag expected = universal(kTagCharacterString, /*constructed=*/true));

/// Modern EXTERNAL associated SEQUENCE (X.680): same shape as EMBEDDED PDV, UNIVERSAL 8.
/// Distinct from classic `ExternalValue` (direct/indirect-reference + encoding CHOICE).
struct ModernExternalValue {
  Identification identification;
  std::optional<std::string> data_value_descriptor;
  std::vector<std::uint8_t> data_value;
};

void encode_external_modern(ByteWriter& out, const ModernExternalValue& value,
                            /**
                             *  Function    : universal
                             *  Description : Computes universal from (kTagExternal).
                             *  Parameters  : kTagExternal — kTagExternal
                             *  Returns     : Tag tag =
                             */
                            Tag tag = universal(kTagExternal, /*constructed=*/true));
Result<ModernExternalValue> decode_external_modern(
    /**
     *  Function    : universal
     *  Description : Computes universal from (kTagExternal).
     *  Parameters  : kTagExternal — kTagExternal
     *  Returns     : ByteReader& in, Tag expected =
     */
    ByteReader& in, Tag expected = universal(kTagExternal, /*constructed=*/true));

/// Wrap already-encoded component TLVs in a constructed SEQUENCE/SET (or explicit tag).
/**
 *  Function    : encode_constructed
 *  Description : Performs encode constructed (declaration).
 *  Parameters  : out — ByteWriter& out; tag — Tag tag; components — Span<const std::uint8_t> components
 *  Returns     : void
 */
void encode_constructed(ByteWriter& out, Tag tag, Span<const std::uint8_t> components);

/// Decode a constructed value; returns the raw content bytes (definite) or reads until EOC
/// (indefinite) and returns concatenated inner encodings.
/**
 *  Function    : decode_constructed
 *  Description : Returns success or an error from decode constructed.
 *  Parameters  : in — ByteReader& in; expected — Tag expected
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_constructed(ByteReader& in, Tag expected);

}  // namespace ber
}  // namespace asn1
