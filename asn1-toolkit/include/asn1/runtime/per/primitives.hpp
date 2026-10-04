/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/per/primitives.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   PER constrained types, fragments, alignment hooks.
**
** Specification: ITU-T X.691 — ASN.1 encoding rules: Packed Encoding
**                 Rules (PER); UPER/APER variants.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/bigint.hpp>

#include <cstdint>
#include <vector>

namespace asn1 {
namespace per {

/// Shared PER implementation; UPER passes Unaligned, APER passes Aligned.
enum class Variant { Unaligned, Aligned };

/**
 *  Function    : maybe_align
 *  Description : Computes maybe align from (w, v).
 *  Parameters  : w — BitWriter& w; v — Variant v
 *  Returns     : inline void
 */
inline void maybe_align(BitWriter& w, Variant v) {
  if (v == Variant::Aligned) {
    /**
     *  Function    : align_to_octet
     *  Description : Computes align to octet from (none).
     *  Parameters  : none
     *  Returns     : w.
     */
    w.align_to_octet();
  }
}

/**
 *  Function    : maybe_align
 *  Description : Returns success or an error from maybe align.
 *  Parameters  : r — BitReader& r; v — Variant v
 *  Returns     : inline Result<void>
 */
inline Result<void> maybe_align(BitReader& r, Variant v) {
  if (v == Variant::Aligned) {
    /**
     *  Function    : align_to_octet
     *  Description : Computes align to octet from (none).
     *  Parameters  : none
     *  Returns     : return r.
     */
    return r.align_to_octet();
  }
  /**
   *  Function    : success
   *  Description : Returns success or an error from success.
   *  Parameters  : none
   *  Returns     : return Result<void>::
   */
  return Result<void>::success();
}

/// Minimum bits to encode values in [0, range-1]. range==0/1 => 0.
/**
 *  Function    : bits_for_range
 *  Description : Computes bits for range from (range).
 *  Parameters  : range — std::uint64_t range
 *  Returns     : std::size_t
 */
std::size_t bits_for_range(std::uint64_t range);

// ---- Whole numbers (X.691 clauses 11.5--11.8 / 10.6) ----

Result<void> encode_constrained_whole_number(BitWriter& out, Variant variant,
                                             std::int64_t value, std::int64_t lower,
                                             std::int64_t upper);
Result<std::int64_t> decode_constrained_whole_number(BitReader& in, Variant variant,
                                                     std::int64_t lower,
                                                     std::int64_t upper);

void encode_semi_constrained_whole_number(BitWriter& out, Variant variant,
                                          std::int64_t value, std::int64_t lower);
Result<std::int64_t> decode_semi_constrained_whole_number(BitReader& in,
                                                          Variant variant,
                                                          std::int64_t lower);

void encode_unconstrained_whole_number(BitWriter& out, Variant variant,
                                       std::int64_t value);
void encode_unconstrained_whole_number(BitWriter& out, Variant variant,
                                       const BigInteger& value);
Result<std::int64_t> decode_unconstrained_whole_number(BitReader& in,
                                                       Variant variant);
/**
 *  Function    : decode_unconstrained_big_integer
 *  Description : Returns success or an error from decode unconstrained big integer.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_unconstrained_big_integer(BitReader& in, Variant variant);

void encode_normally_small_non_negative_whole_number(BitWriter& out, Variant variant,
                                                     std::uint64_t value);
Result<std::uint64_t> decode_normally_small_non_negative_whole_number(
    BitReader& in, Variant variant);

/// Normally small length (1..64 common case; up to 127 supported).
/**
 *  Function    : encode_normally_small_length
 *  Description : Performs encode normally small length (declaration).
 *  Parameters  : out — BitWriter& out; variant — Variant variant; length — std::size_t length
 *  Returns     : void
 */
void encode_normally_small_length(BitWriter& out, Variant variant, std::size_t length);
/**
 *  Function    : decode_normally_small_length
 *  Description : Returns success or an error from decode normally small length.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::size_t>
 */
Result<std::size_t> decode_normally_small_length(BitReader& in, Variant variant);

// ---- Length determinant (X.691 10.9) ----

/// Encode one length determinant fragment. Returns the fragment size actually
/// signaled (may be less than `length` when fragmented: 16K/32K/48K/64K).
std::size_t encode_length_determinant(BitWriter& out, Variant variant,
                                      std::size_t length);
/**
 *  Function    : decode_length_determinant
 *  Description : Returns success or an error from decode length determinant.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::size_t>
 */
Result<std::size_t> decode_length_determinant(BitReader& in, Variant variant);

// ---- Structural helpers ----

/// CHOICE index among `count` root alternatives (index in 0..count-1).
void encode_choice_index(BitWriter& out, Variant variant, std::size_t index,
                         std::size_t count);
Result<std::size_t> decode_choice_index(BitReader& in, Variant variant,
                                        std::size_t count);

/// OPTIONAL / DEFAULT presence bitmap (bit 0 = first field). Non-zero => 1.
/**
 *  Function    : encode_bitmap
 *  Description : Performs encode bitmap (declaration).
 *  Parameters  : out — BitWriter& out; bits — Span<const std::uint8_t> bits
 *  Returns     : void
 */
void encode_bitmap(BitWriter& out, Span<const std::uint8_t> bits);
/**
 *  Function    : decode_bitmap
 *  Description : Returns success or an error from decode bitmap.
 *  Parameters  : in — BitReader& in; nbits — std::size_t nbits
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_bitmap(BitReader& in, std::size_t nbits);

/// Open type: [align if APER] + length determinant + contents octets.
void encode_open_type(BitWriter& out, Variant variant,
                      Span<const std::uint8_t> content);
/**
 *  Function    : decode_open_type
 *  Description : Returns success or an error from decode open type.
 *  Parameters  : in — BitReader& in; variant — Variant variant
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_open_type(BitReader& in, Variant variant);

}  // namespace per
}  // namespace asn1
