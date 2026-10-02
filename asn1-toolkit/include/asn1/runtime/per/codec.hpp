#pragma once

#include <asn1/runtime/per/primitives.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace per {

/// INTEGER value constraints used by the runtime codecs (host int64).
struct IntegerConstraint {
  std::optional<std::int64_t> lower;
  std::optional<std::int64_t> upper;
  bool extensible = false;
};

/// SIZE constraint for strings / SEQUENCE OF / BIT STRING bit-count.
struct SizeConstraint {
  std::optional<std::size_t> lower;
  std::optional<std::size_t> upper;
  bool extensible = false;

  /// Fixed SIZE(n).
  bool is_fixed() const noexcept {
    return lower.has_value() && upper.has_value() && *lower == *upper;
  }
  /// Fully constrained with ub <= 65535 (X.691 / common practice).
  bool is_fully_constrained() const noexcept {
    return lower.has_value() && upper.has_value() && *upper <= 65535;
  }
};

// ---- Primitive types ----

void encode_boolean(BitWriter& out, Variant variant, bool value);
Result<bool> decode_boolean(BitReader& in, Variant variant);

void encode_null(BitWriter& out, Variant variant);
Result<void> decode_null(BitReader& in, Variant variant);

void encode_integer(BitWriter& out, Variant variant, std::int64_t value,
                    const IntegerConstraint& constraint = {});
Result<std::int64_t> decode_integer(BitReader& in, Variant variant,
                                    const IntegerConstraint& constraint = {});

void encode_octet_string(BitWriter& out, Variant variant,
                         Span<const std::uint8_t> value,
                         const SizeConstraint& size = {});
Result<std::vector<std::uint8_t>> decode_octet_string(
    BitReader& in, Variant variant, const SizeConstraint& size = {});

/// BIT STRING as packed bits; `bit_length` is the ASN.1 length in bits.
void encode_bit_string(BitWriter& out, Variant variant,
                       Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size = {});
struct BitStringValue {
  std::vector<std::uint8_t> bits;
  std::size_t bit_length = 0;
};
Result<BitStringValue> decode_bit_string(BitReader& in, Variant variant,
                                         const SizeConstraint& size = {});

/// UTF8String: length determinant (octets) + UTF-8 bytes (UPER: no alphabet packing).
void encode_utf8_string(BitWriter& out, Variant variant, const std::string& value,
                        const SizeConstraint& size = {});
Result<std::string> decode_utf8_string(BitReader& in, Variant variant,
                                       const SizeConstraint& size = {});

// ---- Constructed scaffolding ----

/// SEQUENCE preamble: optional extension bit (0 = no additions) + OPTIONAL bitmap.
void encode_sequence_preamble(BitWriter& out, Variant variant, bool extensible,
                              Span<const std::uint8_t> optionals_present);
/// On success, extension additions are absent (Phase 9 rejects extension bit = 1).
Result<std::vector<std::uint8_t>> decode_sequence_preamble(BitReader& in,
                                                           Variant variant,
                                                           bool extensible,
                                                           std::size_t n_optionals);

/// CHOICE root alternative index (writes extension bit 0 when extensible).
void encode_choice_root(BitWriter& out, Variant variant, std::size_t index,
                        std::size_t root_count, bool extensible = false);
Result<std::size_t> decode_choice_root(BitReader& in, Variant variant,
                                       std::size_t root_count,
                                       bool extensible = false);

/// SEQUENCE OF / SET OF length determinant under a SIZE constraint.
void encode_sequence_of_length(BitWriter& out, Variant variant, std::size_t count,
                               const SizeConstraint& size = {});
Result<std::size_t> decode_sequence_of_length(BitReader& in, Variant variant,
                                              const SizeConstraint& size = {});

/// ENUMERATED: root index encoded as constrained whole number; extensible adds
/// extension bit + normally-small index for additions (Phase 12: encode root only).
void encode_enumerated(BitWriter& out, Variant variant, std::size_t root_index,
                       std::size_t root_count, bool extensible = false);
Result<std::size_t> decode_enumerated(BitReader& in, Variant variant,
                                      std::size_t root_count, bool extensible = false);

/// OBJECT IDENTIFIER / RELATIVE-OID: length determinant + BER content octets.
void encode_object_identifier(BitWriter& out, Variant variant,
                              Span<const std::uint64_t> arcs, bool relative = false);
Result<std::vector<std::uint64_t>> decode_object_identifier(BitReader& in, Variant variant,
                                                           bool relative = false);

}  // namespace per

namespace uper {

inline constexpr per::Variant kVariant = per::Variant::Unaligned;

inline void encode_boolean(BitWriter& out, bool value) {
  per::encode_boolean(out, kVariant, value);
}
inline Result<bool> decode_boolean(BitReader& in) {
  return per::decode_boolean(in, kVariant);
}

inline void encode_null(BitWriter& out) { per::encode_null(out, kVariant); }
inline Result<void> decode_null(BitReader& in) {
  return per::decode_null(in, kVariant);
}

inline void encode_integer(BitWriter& out, std::int64_t value,
                           const per::IntegerConstraint& c = {}) {
  per::encode_integer(out, kVariant, value, c);
}
inline Result<std::int64_t> decode_integer(BitReader& in,
                                           const per::IntegerConstraint& c = {}) {
  return per::decode_integer(in, kVariant, c);
}

inline void encode_octet_string(BitWriter& out, Span<const std::uint8_t> value,
                                const per::SizeConstraint& size = {}) {
  per::encode_octet_string(out, kVariant, value, size);
}
inline Result<std::vector<std::uint8_t>> decode_octet_string(
    BitReader& in, const per::SizeConstraint& size = {}) {
  return per::decode_octet_string(in, kVariant, size);
}

inline void encode_bit_string(BitWriter& out, Span<const std::uint8_t> bits,
                              std::size_t bit_length,
                              const per::SizeConstraint& size = {}) {
  per::encode_bit_string(out, kVariant, bits, bit_length, size);
}
inline Result<per::BitStringValue> decode_bit_string(
    BitReader& in, const per::SizeConstraint& size = {}) {
  return per::decode_bit_string(in, kVariant, size);
}

inline void encode_utf8_string(BitWriter& out, const std::string& value,
                               const per::SizeConstraint& size = {}) {
  per::encode_utf8_string(out, kVariant, value, size);
}
inline Result<std::string> decode_utf8_string(BitReader& in,
                                              const per::SizeConstraint& size = {}) {
  return per::decode_utf8_string(in, kVariant, size);
}

inline void encode_sequence_preamble(BitWriter& out, bool extensible,
                                     Span<const std::uint8_t> optionals_present) {
  per::encode_sequence_preamble(out, kVariant, extensible, optionals_present);
}
inline Result<std::vector<std::uint8_t>> decode_sequence_preamble(
    BitReader& in, bool extensible, std::size_t n_optionals) {
  return per::decode_sequence_preamble(in, kVariant, extensible, n_optionals);
}

inline void encode_choice_root(BitWriter& out, std::size_t index, std::size_t root_count,
                               bool extensible = false) {
  per::encode_choice_root(out, kVariant, index, root_count, extensible);
}
inline Result<std::size_t> decode_choice_root(BitReader& in, std::size_t root_count,
                                              bool extensible = false) {
  return per::decode_choice_root(in, kVariant, root_count, extensible);
}

inline void encode_sequence_of_length(BitWriter& out, std::size_t count,
                                      const per::SizeConstraint& size = {}) {
  per::encode_sequence_of_length(out, kVariant, count, size);
}
inline Result<std::size_t> decode_sequence_of_length(BitReader& in,
                                                     const per::SizeConstraint& size = {}) {
  return per::decode_sequence_of_length(in, kVariant, size);
}

inline void encode_enumerated(BitWriter& out, std::size_t root_index, std::size_t root_count,
                              bool extensible = false) {
  per::encode_enumerated(out, kVariant, root_index, root_count, extensible);
}
inline Result<std::size_t> decode_enumerated(BitReader& in, std::size_t root_count,
                                             bool extensible = false) {
  return per::decode_enumerated(in, kVariant, root_count, extensible);
}

inline void encode_object_identifier(BitWriter& out, Span<const std::uint64_t> arcs,
                                     bool relative = false) {
  per::encode_object_identifier(out, kVariant, arcs, relative);
}
inline Result<std::vector<std::uint64_t>> decode_object_identifier(BitReader& in,
                                                                  bool relative = false) {
  return per::decode_object_identifier(in, kVariant, relative);
}

}  // namespace uper

namespace aper {

inline constexpr per::Variant kVariant = per::Variant::Aligned;

inline void encode_boolean(BitWriter& out, bool value) {
  per::encode_boolean(out, kVariant, value);
}
inline Result<bool> decode_boolean(BitReader& in) {
  return per::decode_boolean(in, kVariant);
}

inline void encode_null(BitWriter& out) { per::encode_null(out, kVariant); }
inline Result<void> decode_null(BitReader& in) {
  return per::decode_null(in, kVariant);
}

inline void encode_integer(BitWriter& out, std::int64_t value,
                           const per::IntegerConstraint& c = {}) {
  per::encode_integer(out, kVariant, value, c);
}
inline Result<std::int64_t> decode_integer(BitReader& in,
                                           const per::IntegerConstraint& c = {}) {
  return per::decode_integer(in, kVariant, c);
}

inline void encode_octet_string(BitWriter& out, Span<const std::uint8_t> value,
                                const per::SizeConstraint& size = {}) {
  per::encode_octet_string(out, kVariant, value, size);
}
inline Result<std::vector<std::uint8_t>> decode_octet_string(
    BitReader& in, const per::SizeConstraint& size = {}) {
  return per::decode_octet_string(in, kVariant, size);
}

inline void encode_bit_string(BitWriter& out, Span<const std::uint8_t> bits,
                              std::size_t bit_length,
                              const per::SizeConstraint& size = {}) {
  per::encode_bit_string(out, kVariant, bits, bit_length, size);
}
inline Result<per::BitStringValue> decode_bit_string(
    BitReader& in, const per::SizeConstraint& size = {}) {
  return per::decode_bit_string(in, kVariant, size);
}

inline void encode_utf8_string(BitWriter& out, const std::string& value,
                               const per::SizeConstraint& size = {}) {
  per::encode_utf8_string(out, kVariant, value, size);
}
inline Result<std::string> decode_utf8_string(BitReader& in,
                                              const per::SizeConstraint& size = {}) {
  return per::decode_utf8_string(in, kVariant, size);
}

inline void encode_sequence_preamble(BitWriter& out, bool extensible,
                                     Span<const std::uint8_t> optionals_present) {
  per::encode_sequence_preamble(out, kVariant, extensible, optionals_present);
}
inline Result<std::vector<std::uint8_t>> decode_sequence_preamble(
    BitReader& in, bool extensible, std::size_t n_optionals) {
  return per::decode_sequence_preamble(in, kVariant, extensible, n_optionals);
}

inline void encode_choice_root(BitWriter& out, std::size_t index, std::size_t root_count,
                               bool extensible = false) {
  per::encode_choice_root(out, kVariant, index, root_count, extensible);
}
inline Result<std::size_t> decode_choice_root(BitReader& in, std::size_t root_count,
                                              bool extensible = false) {
  return per::decode_choice_root(in, kVariant, root_count, extensible);
}

inline void encode_sequence_of_length(BitWriter& out, std::size_t count,
                                      const per::SizeConstraint& size = {}) {
  per::encode_sequence_of_length(out, kVariant, count, size);
}
inline Result<std::size_t> decode_sequence_of_length(BitReader& in,
                                                     const per::SizeConstraint& size = {}) {
  return per::decode_sequence_of_length(in, kVariant, size);
}

inline void encode_enumerated(BitWriter& out, std::size_t root_index, std::size_t root_count,
                              bool extensible = false) {
  per::encode_enumerated(out, kVariant, root_index, root_count, extensible);
}
inline Result<std::size_t> decode_enumerated(BitReader& in, std::size_t root_count,
                                             bool extensible = false) {
  return per::decode_enumerated(in, kVariant, root_count, extensible);
}

inline void encode_object_identifier(BitWriter& out, Span<const std::uint64_t> arcs,
                                     bool relative = false) {
  per::encode_object_identifier(out, kVariant, arcs, relative);
}
inline Result<std::vector<std::uint64_t>> decode_object_identifier(BitReader& in,
                                                                  bool relative = false) {
  return per::decode_object_identifier(in, kVariant, relative);
}

}  // namespace aper
}  // namespace asn1
