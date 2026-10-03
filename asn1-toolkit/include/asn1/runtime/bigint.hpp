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
  BigInteger() = default;

  static BigInteger zero();
  static BigInteger from_i64(std::int64_t value);
  static BigInteger from_u64(std::uint64_t value);

  /// Decimal text with optional leading '+' / '-'.
  static Result<BigInteger> from_decimal(std::string_view text);

  /// BER/DER INTEGER content octets (two's-complement, big-endian).
  static Result<BigInteger> from_twos_complement(Span<const std::uint8_t> bytes);

  /// Unsigned big-endian octets (OER unsigned variable-length INTEGER).
  static Result<BigInteger> from_unsigned_bytes(Span<const std::uint8_t> bytes);

  bool is_zero() const noexcept;
  bool is_negative() const noexcept { return negative_; }

  std::optional<std::int64_t> as_i64() const;
  std::optional<std::uint64_t> as_u64() const;

  std::string to_decimal() const;
  std::vector<std::uint8_t> to_twos_complement() const;
  /// Fails when negative.
  Result<std::vector<std::uint8_t>> to_unsigned_bytes() const;

  int compare(const BigInteger& other) const;
  bool operator==(const BigInteger& other) const { return compare(other) == 0; }
  bool operator!=(const BigInteger& other) const { return compare(other) != 0; }
  bool operator<(const BigInteger& other) const { return compare(other) < 0; }
  bool operator<=(const BigInteger& other) const { return compare(other) <= 0; }
  bool operator>(const BigInteger& other) const { return compare(other) > 0; }
  bool operator>=(const BigInteger& other) const { return compare(other) >= 0; }

 private:
  bool negative_ = false;
  std::vector<std::uint32_t> limbs_;  // little-endian magnitude

  void normalize();
  std::vector<std::uint8_t> magnitude_be() const;
  static int compare_mag(const BigInteger& a, const BigInteger& b);
  void mul_add_u32(std::uint32_t mul, std::uint32_t add);
  std::uint32_t div_mod_u32(std::uint32_t divisor);
};

}  // namespace asn1
