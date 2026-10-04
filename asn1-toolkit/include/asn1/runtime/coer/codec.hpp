/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/coer/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   COER (canonical OER) encode/decode API.
**
** Specification: ITU-T X.696 — Canonical OER (COER) on octet encoding
**                 rules.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/oer/codec.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace coer {

/// COER reuses OER constraint descriptors (X.696 CANONICAL-OER).
using oer::IntegerRange;
using oer::SizeRange;
using oer::IntegerConstraint;
using oer::SizeConstraint;
using oer::BitStringValue;

// ---- Length determinant (shortest form only) ----

/**
 *  Function    : encode_length
 *  Description : Performs encode length (declaration).
 *  Parameters  : out — ByteWriter& out; length — std::size_t length
 *  Returns     : void
 */
void encode_length(ByteWriter& out, std::size_t length);
/**
 *  Function    : decode_length
 *  Description : Returns success or an error from decode length.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<std::size_t>
 */
Result<std::size_t> decode_length(ByteReader& in);

// ---- Primitive types ----

/**
 *  Function    : encode_boolean
 *  Description : Performs encode boolean (declaration).
 *  Parameters  : out — ByteWriter& out; value — bool value
 *  Returns     : void
 */
void encode_boolean(ByteWriter& out, bool value);
/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from in.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(ByteReader& in);

/**
 *  Function    : encode_null
 *  Description : Performs encode null (declaration).
 *  Parameters  : out — ByteWriter& out
 *  Returns     : void
 */
void encode_null(ByteWriter& out);
/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<void>
 */
Result<void> decode_null(ByteReader& in);

void encode_integer(ByteWriter& out, std::int64_t value,
                    const IntegerConstraint& constraint = {});
void encode_integer(ByteWriter& out, const BigInteger& value,
                    const IntegerConstraint& constraint = {});
Result<std::int64_t> decode_integer(ByteReader& in,
                                    const IntegerConstraint& constraint = {});
Result<BigInteger> decode_big_integer(ByteReader& in,
                                      const IntegerConstraint& constraint = {});

void encode_octet_string(ByteWriter& out, Span<const std::uint8_t> value,
                         const SizeConstraint& size = {});
Result<std::vector<std::uint8_t>> decode_octet_string(ByteReader& in,
                                                      const SizeConstraint& size = {});

void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size = {});
Result<BitStringValue> decode_bit_string(ByteReader& in, const SizeConstraint& size = {});

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        const SizeConstraint& size = {});
Result<std::string> decode_utf8_string(ByteReader& in, const SizeConstraint& size = {});

/**
 *  Function    : encode_enumerated
 *  Description : Performs encode enumerated (declaration).
 *  Parameters  : out — ByteWriter& out; value — std::int64_t value
 *  Returns     : void
 */
void encode_enumerated(ByteWriter& out, std::int64_t value);
/**
 *  Function    : decode_enumerated
 *  Description : Returns success or an error from decode enumerated.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_enumerated(ByteReader& in);

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              bool relative = false);
Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in,
                                                           bool relative = false);

using oer::RealIeeeForm;

Result<void> encode_real(ByteWriter& out, double value,
                         RealIeeeForm form = RealIeeeForm::Unconstrained);
/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : in — ByteReader& in; form — RealIeeeForm form
 *  Returns     : Result<double>
 */
Result<double> decode_real(ByteReader& in, RealIeeeForm form = RealIeeeForm::Unconstrained);

// ---- Constructed scaffolding ----

using oer::SequencePreamble;
using oer::ExtensionAdditions;

void encode_sequence_preamble(ByteWriter& out, bool extensible, bool extensions_present,
                              Span<const bool> optionals_present);
Result<SequencePreamble> decode_sequence_preamble(ByteReader& in, bool extensible,
                                                  std::size_t n_optionals);

using oer::encode_open_type;
using oer::decode_open_type;
using oer::encode_extension_additions;
using oer::decode_extension_additions;

/**
 *  Function    : encode_choice_tag
 *  Description : Performs encode choice tag (declaration).
 *  Parameters  : out — ByteWriter& out; tag_number — std::uint64_t tag_number; constructed — bool constructed
 *  Returns     : void
 */
void encode_choice_tag(ByteWriter& out, std::uint64_t tag_number, bool constructed = false);
/**
 *  Function    : decode_choice_tag
 *  Description : Returns success or an error from decode choice tag.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<std::uint64_t>
 */
Result<std::uint64_t> decode_choice_tag(ByteReader& in);

/**
 *  Function    : encode_sequence_of_length
 *  Description : Performs encode sequence of length (declaration).
 *  Parameters  : out — ByteWriter& out; count — std::size_t count
 *  Returns     : void
 */
void encode_sequence_of_length(ByteWriter& out, std::size_t count);
/**
 *  Function    : decode_sequence_of_length
 *  Description : Returns success or an error from decode sequence of length.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<std::size_t>
 */
Result<std::size_t> decode_sequence_of_length(ByteReader& in);

/// SET OF: quantity + components sorted by ascending octet-string order (COER).
/// `components` are complete encodings of each element; they are sorted in place
/// order before writing (caller's vector is not modified — a copy is sorted).
/**
 *  Function    : encode_set_of
 *  Description : Performs encode set of (declaration).
 *  Parameters  : out — ByteWriter& out; components — std::vector<std::vector<std::uint8_t>> components
 *  Returns     : void
 */
void encode_set_of(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components);

/// Require already-split SET OF component encodings to be in ascending order.
/**
 *  Function    : require_set_of_order
 *  Description : Returns success or an error from require set of order.
 *  Parameters  : components — Span<const std::vector<std::uint8_t>> components
 *  Returns     : Result<void>
 */
Result<void> require_set_of_order(Span<const std::vector<std::uint8_t>> components);

}  // namespace coer
}  // namespace asn1
