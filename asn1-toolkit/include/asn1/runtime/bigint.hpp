/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/bigint.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   BigInteger sign-and-limbs INTEGER beyond 64 bits.
**
** Specification: ITU-T X.680 INTEGER type; unconstrained encoding as
**                 used by BER/PER/OER callers.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/common/result.hpp>
#include <asn1/common/span.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace asn1 {

/// Sign-plus-limbs arbitrary-precision integer for unconstrained ASN.1 INTEGER.
/// Absolute value is little-endian base-2^32 limbs (no leading zero limbs except zero).
class BigInteger {
 public:
  /**
   *  Function    : BigInteger
   *  Description : Constructs or initializes BigInteger.
   *  Parameters  : none
   *  Returns     : void
   */
  BigInteger() = default;

  /**
   *  Function    : zero
   *  Description : Computes zero from (none).
   *  Parameters  : none
   *  Returns     : static BigInteger
   */
  static BigInteger zero();
  /**
   *  Function    : from_i64
   *  Description : Computes from i64 from (value).
   *  Parameters  : value — std::int64_t value
   *  Returns     : static BigInteger
   */
  static BigInteger from_i64(std::int64_t value);
  /**
   *  Function    : from_u64
   *  Description : Computes from u64 from (value).
   *  Parameters  : value — std::uint64_t value
   *  Returns     : static BigInteger
   */
  static BigInteger from_u64(std::uint64_t value);

  /// Decimal text with optional leading '+' / '-'.
  /**
   *  Function    : from_decimal
   *  Description : Returns success or an error from from decimal.
   *  Parameters  : text — std::string_view text
   *  Returns     : static Result<BigInteger>
   */
  static Result<BigInteger> from_decimal(std::string_view text);

  /// BER/DER INTEGER content octets (two's-complement, big-endian).
  /**
   *  Function    : from_twos_complement
   *  Description : Returns success or an error from from twos complement.
   *  Parameters  : bytes — Span<const std::uint8_t> bytes
   *  Returns     : static Result<BigInteger>
   */
  static Result<BigInteger> from_twos_complement(Span<const std::uint8_t> bytes);

  /// Unsigned big-endian octets (OER unsigned variable-length INTEGER).
  /**
   *  Function    : from_unsigned_bytes
   *  Description : Returns success or an error from from unsigned bytes.
   *  Parameters  : bytes — Span<const std::uint8_t> bytes
   *  Returns     : static Result<BigInteger>
   */
  static Result<BigInteger> from_unsigned_bytes(Span<const std::uint8_t> bytes);

  bool is_zero() const noexcept;
  /**
   *  Function    : is_negative
   *  Description : Returns whether negative holds for the given inputs.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool is_negative() const noexcept { return negative_; }

  /**
   *  Function    : as_i64
   *  Description : Computes as i64 from (none).
   *  Parameters  : none
   *  Returns     : std::optional<std::int64_t>
   */
  std::optional<std::int64_t> as_i64() const;
  /**
   *  Function    : as_u64
   *  Description : Computes as u64 from (none).
   *  Parameters  : none
   *  Returns     : std::optional<std::uint64_t>
   */
  std::optional<std::uint64_t> as_u64() const;

  /**
   *  Function    : to_decimal
   *  Description : Builds and returns a string for to decimal.
   *  Parameters  : none
   *  Returns     : std::string
   */
  std::string to_decimal() const;
  /**
   *  Function    : to_twos_complement
   *  Description : Computes to twos complement from (none).
   *  Parameters  : none
   *  Returns     : std::vector<std::uint8_t>
   */
  std::vector<std::uint8_t> to_twos_complement() const;
  /// Fails when negative.
  /**
   *  Function    : to_unsigned_bytes
   *  Description : Returns success or an error from to unsigned bytes.
   *  Parameters  : none
   *  Returns     : Result<std::vector<std::uint8_t>>
   */
  Result<std::vector<std::uint8_t>> to_unsigned_bytes() const;

  /**
   *  Function    : compare
   *  Description : Computes compare from (other).
   *  Parameters  : other — const BigInteger& other
   *  Returns     : int
   */
  int compare(const BigInteger& other) const;
  /**
   *  Function    : compare
   *  Description : Performs compare (definition).
   *  Parameters  : compare(other — const BigInteger& other) const { return compare(other
   *  Returns     : —
   */
  bool operator==(const BigInteger& other) const { return compare(other) == 0; }
  /**
   *  Function    : compare
   *  Description : Performs compare (definition).
   *  Parameters  : compare(other — const BigInteger& other) const { return compare(other
   *  Returns     : —
   */
  bool operator!=(const BigInteger& other) const { return compare(other) != 0; }
  /**
   *  Function    : compare
   *  Description : Performs compare (definition).
   *  Parameters  : compare(other — const BigInteger& other) const { return compare(other
   *  Returns     : —
   */
  bool operator<(const BigInteger& other) const { return compare(other) < 0; }
  /**
   *  Function    : compare
   *  Description : Performs compare (definition).
   *  Parameters  : compare(other — const BigInteger& other) const { return compare(other
   *  Returns     : —
   */
  bool operator<=(const BigInteger& other) const { return compare(other) <= 0; }
  /**
   *  Function    : compare
   *  Description : Performs compare (definition).
   *  Parameters  : compare(other — const BigInteger& other) const { return compare(other
   *  Returns     : —
   */
  bool operator>(const BigInteger& other) const { return compare(other) > 0; }
  /**
   *  Function    : compare
   *  Description : Performs compare (definition).
   *  Parameters  : compare(other — const BigInteger& other) const { return compare(other
   *  Returns     : —
   */
  bool operator>=(const BigInteger& other) const { return compare(other) >= 0; }

 private:
  bool negative_ = false;
  std::vector<std::uint32_t> limbs_;  // little-endian magnitude

  /**
   *  Function    : normalize
   *  Description : Performs normalize (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void normalize();
  /**
   *  Function    : magnitude_be
   *  Description : Computes magnitude be from (none).
   *  Parameters  : none
   *  Returns     : std::vector<std::uint8_t>
   */
  std::vector<std::uint8_t> magnitude_be() const;
  /**
   *  Function    : compare_mag
   *  Description : Computes compare mag from (a, b).
   *  Parameters  : a — const BigInteger& a; b — const BigInteger& b
   *  Returns     : static int
   */
  static int compare_mag(const BigInteger& a, const BigInteger& b);
  /**
   *  Function    : mul_add_u32
   *  Description : Performs mul add u32 (declaration).
   *  Parameters  : mul — std::uint32_t mul; add — std::uint32_t add
   *  Returns     : void
   */
  void mul_add_u32(std::uint32_t mul, std::uint32_t add);
  /**
   *  Function    : div_mod_u32
   *  Description : Computes div mod u32 from (divisor).
   *  Parameters  : divisor — std::uint32_t divisor
   *  Returns     : std::uint32_t
   */
  std::uint32_t div_mod_u32(std::uint32_t divisor);
};

}  // namespace asn1
