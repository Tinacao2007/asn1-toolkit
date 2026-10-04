/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/oer/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   OER encode/decode API.
**
** Specification: ITU-T X.696 — ASN.1 encoding rules: Octet Encoding
**                 Rules (OER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/bit_string.hpp>
#include <asn1/runtime/bigint.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace oer {

/// Discrete range for disjoint integer constraints.
struct IntegerRange {
  std::optional<std::int64_t> lower;
  std::optional<std::int64_t> upper;

  /**
   *  Function    : contains
   *  Description : Returns a boolean result from v.
   *  Parameters  : v — std::int64_t v
   *  Returns     : bool
   */
  bool contains(std::int64_t v) const noexcept {
    if (lower && v < *lower) return false;
    if (upper && v > *upper) return false;
    return true;
  }
};

/// INTEGER value constraints (host int64).
struct IntegerConstraint {
  std::optional<std::int64_t> lower;
  std::optional<std::int64_t> upper;
  bool extensible = false;
  std::vector<IntegerRange> ranges = {};

  /**
   *  Function    : contains
   *  Description : Returns a boolean result from v.
   *  Parameters  : v — std::int64_t v
   *  Returns     : bool
   */
  bool contains(std::int64_t v) const noexcept {
    if (!ranges.empty()) {
      for (const auto& r : ranges) {
        if (r.contains(v)) return true;
      }
      return false;
    }
    if (lower && v < *lower) return false;
    if (upper && v > *upper) return false;
    return true;
  }
};

/// Discrete range for disjoint size constraints.
struct SizeRange {
  std::optional<std::size_t> lower;
  std::optional<std::size_t> upper;

  /**
   *  Function    : contains
   *  Description : Returns a boolean result from v.
   *  Parameters  : v — std::size_t v
   *  Returns     : bool
   */
  bool contains(std::size_t v) const noexcept {
    if (lower && v < *lower) return false;
    if (upper && v > *upper) return false;
    return true;
  }
};

/// SIZE constraint for strings / SEQUENCE OF / BIT STRING bit-count.
struct SizeConstraint {
  std::optional<std::size_t> lower;
  std::optional<std::size_t> upper;
  bool extensible = false;
  std::vector<SizeRange> ranges = {};

  /**
   *  Function    : contains
   *  Description : Returns a boolean result from v.
   *  Parameters  : v — std::size_t v
   *  Returns     : bool
   */
  bool contains(std::size_t v) const noexcept {
    if (!ranges.empty()) {
      for (const auto& r : ranges) {
        if (r.contains(v)) return true;
      }
      return false;
    }
    if (lower && v < *lower) return false;
    if (upper && v > *upper) return false;
    return true;
  }

  /**
   *  Function    : is_fixed
   *  Description : Returns whether fixed holds for the given inputs.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool is_fixed() const noexcept {
    return lower.has_value() && upper.has_value() && *lower == *upper && !extensible;
  }
};

// ---- Length determinant (X.696) ----

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

/// BIT STRING: `bit_length` is the ASN.1 length in bits.
void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size = {});
using BitStringValue = asn1::BitStringValue;
Result<BitStringValue> decode_bit_string(ByteReader& in, const SizeConstraint& size = {});

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        const SizeConstraint& size = {});
Result<std::string> decode_utf8_string(ByteReader& in, const SizeConstraint& size = {});

/// ENUMERATED as OER integer enumeration value (not PER index).
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

/// OBJECT IDENTIFIER: length + BER content octets.
void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              bool relative = false);
Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in,
                                                           bool relative = false);

/// REAL IEEE fixed forms for OER (X.696). Unconstrained uses length + BER content.
enum class RealIeeeForm {
  Unconstrained,
  Binary32,
  Binary64,
};

/// Unconstrained: length + BER REAL content. Binary32/64: fixed IEEE octets (BE).
/// Binary32/64 encode fails if a finite value overflows the target format.
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

/// SEQUENCE/SET preamble result (X.696).
struct SequencePreamble {
  bool extensions_present = false;
  std::vector<bool> optionals;
};

/// Extension addition series after root components (X.696 16.4–16.5):
/// length + unused-bits octet + presence bitmap, then one open type per set bit.
struct ExtensionAdditions {
  std::vector<bool> presence;
  std::vector<std::vector<std::uint8_t>> open_types;
};

/// SEQUENCE/SET preamble: optional extension bit + OPTIONAL/DEFAULT presence bits,
/// then pad to an octet boundary.
void encode_sequence_preamble(ByteWriter& out, bool extensible, bool extensions_present,
                              Span<const bool> optionals_present);
Result<SequencePreamble> decode_sequence_preamble(ByteReader& in, bool extensible,
                                                  std::size_t n_optionals);

/// Open type: length determinant + content octets.
/**
 *  Function    : encode_open_type
 *  Description : Performs encode open type (declaration).
 *  Parameters  : out — ByteWriter& out; content — Span<const std::uint8_t> content
 *  Returns     : void
 */
void encode_open_type(ByteWriter& out, Span<const std::uint8_t> content);
/**
 *  Function    : decode_open_type
 *  Description : Returns success or an error from decode open type.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_open_type(ByteReader& in);

/// After root components when preamble.extensions_present.
/// `open_types` has one entry per set bit in `presence`, in bitmap order.
void encode_extension_additions(ByteWriter& out, Span<const bool> presence,
                                const std::vector<std::vector<std::uint8_t>>& open_types);
/**
 *  Function    : decode_extension_additions
 *  Description : Returns success or an error from decode extension additions.
 *  Parameters  : in — ByteReader& in
 *  Returns     : Result<ExtensionAdditions>
 */
Result<ExtensionAdditions> decode_extension_additions(ByteReader& in);

/// CHOICE: encode/decode a context-specific tag number (AUTOMATIC TAGS style).
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

/// SEQUENCE OF / SET OF length (always a length determinant in basic OER).
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

}  // namespace oer
}  // namespace asn1
