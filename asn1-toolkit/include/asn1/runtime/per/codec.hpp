#pragma once

#include <asn1/runtime/bit_string.hpp>
#include <asn1/runtime/per/primitives.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace per {

/// Discrete range for disjoint integer constraints.
struct IntegerRange {
  std::optional<std::int64_t> lower;
  std::optional<std::int64_t> upper;

  bool contains(std::int64_t v) const noexcept {
    if (lower && v < *lower) return false;
    if (upper && v > *upper) return false;
    return true;
  }
};

/// INTEGER value constraints used by the runtime codecs (host int64).
struct IntegerConstraint {
  std::optional<std::int64_t> lower;
  std::optional<std::int64_t> upper;
  bool extensible = false;
  std::vector<IntegerRange> ranges = {};

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

Result<void> encode_integer(BitWriter& out, Variant variant, std::int64_t value,
                            const IntegerConstraint& constraint = {});
Result<void> encode_integer(BitWriter& out, Variant variant, const BigInteger& value,
                            const IntegerConstraint& constraint = {});
Result<std::int64_t> decode_integer(BitReader& in, Variant variant,
                                    const IntegerConstraint& constraint = {});
Result<BigInteger> decode_big_integer(BitReader& in, Variant variant,
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
using BitStringValue = asn1::BitStringValue;
Result<BitStringValue> decode_bit_string(BitReader& in, Variant variant,
                                         const SizeConstraint& size = {});

/// UTF8String: length determinant (octets) + UTF-8 bytes (UPER: no alphabet packing).
void encode_utf8_string(BitWriter& out, Variant variant, const std::string& value,
                        const SizeConstraint& size = {});
Result<std::string> decode_utf8_string(BitReader& in, Variant variant,
                                       const SizeConstraint& size = {});

// ---- Constructed scaffolding ----

/// Index into root or extension series (CHOICE / ENUMERATED).
struct ExtensionIndex {
  bool extension = false;
  std::size_t index = 0;
};

/// SEQUENCE preamble decode result.
struct SequencePreamble {
  bool extensions_present = false;
  std::vector<std::uint8_t> optionals;
};

/// SEQUENCE/SET extension addition series (X.691 18.8): normally-small length of
/// the presence bitmap, the bitmap itself, then one open type per set bit.
struct ExtensionAdditions {
  std::vector<std::uint8_t> presence;
  std::vector<std::vector<std::uint8_t>> open_types;
};

/// SEQUENCE preamble: optional extension bit + OPTIONAL/DEFAULT presence bitmap.
void encode_sequence_preamble(BitWriter& out, Variant variant, bool extensible,
                              bool extensions_present,
                              Span<const std::uint8_t> optionals_present);
Result<SequencePreamble> decode_sequence_preamble(BitReader& in, Variant variant,
                                                  bool extensible,
                                                  std::size_t n_optionals);

/// After root components, when extensions_present: NSL(n) + n-bit presence + open types.
/// `open_types` has one entry per set bit in `presence`, in bitmap order.
void encode_extension_additions(BitWriter& out, Variant variant,
                                Span<const std::uint8_t> presence,
                                const std::vector<std::vector<std::uint8_t>>& open_types);
Result<ExtensionAdditions> decode_extension_additions(BitReader& in, Variant variant);

/// CHOICE root alternative (writes extension bit 0 when extensible).
void encode_choice_root(BitWriter& out, Variant variant, std::size_t index,
                        std::size_t root_count, bool extensible = false);
/// CHOICE extension alternative: extension bit 1 + normally-small index + open type.
void encode_choice_extension(BitWriter& out, Variant variant, std::size_t ext_index,
                             Span<const std::uint8_t> open_type_content);
Result<ExtensionIndex> decode_choice(BitReader& in, Variant variant,
                                     std::size_t root_count, bool extensible = false);
/// Root-only decode (fails if the extension bit is set).
Result<std::size_t> decode_choice_root(BitReader& in, Variant variant,
                                       std::size_t root_count,
                                       bool extensible = false);

/// SEQUENCE OF / SET OF length determinant under a SIZE constraint.
void encode_sequence_of_length(BitWriter& out, Variant variant, std::size_t count,
                               const SizeConstraint& size = {});
Result<std::size_t> decode_sequence_of_length(BitReader& in, Variant variant,
                                              const SizeConstraint& size = {});

/// SEQUENCE OF / SET OF chunk length determinant for multi-fragment encoding (X.691 11.9.3.8).
/// Returns the number of items to encode in this chunk.
std::size_t encode_sequence_of_chunk(BitWriter& out, Variant variant,
                                     std::size_t remaining, bool is_first,
                                     const SizeConstraint& size = {});

/// Decodes the length determinant for one chunk of a SEQUENCE OF / SET OF.
/// Returns the number of items in this chunk. When < 16384, this is the final chunk.
Result<std::size_t> decode_sequence_of_chunk(BitReader& in, Variant variant,
                                             bool is_first,
                                             const SizeConstraint& size = {});

/// ENUMERATED: root index as constrained whole number; extensible adds extension bit.
void encode_enumerated(BitWriter& out, Variant variant, std::size_t root_index,
                       std::size_t root_count, bool extensible = false);
/// ENUMERATED extension addition: extension bit 1 + normally-small index.
void encode_enumerated_extension(BitWriter& out, Variant variant, std::size_t ext_index);
Result<ExtensionIndex> decode_enumerated(BitReader& in, Variant variant,
                                         std::size_t root_count,
                                         bool extensible = false);

/// OBJECT IDENTIFIER / RELATIVE-OID: length determinant + BER content octets.
void encode_object_identifier(BitWriter& out, Variant variant,
                              Span<const std::uint64_t> arcs, bool relative = false);
Result<std::vector<std::uint64_t>> decode_object_identifier(BitReader& in, Variant variant,
                                                           bool relative = false);

/// REAL: length determinant + BER REAL content octets (X.691).
void encode_real(BitWriter& out, Variant variant, double value);
Result<double> decode_real(BitReader& in, Variant variant);

}  // namespace per

namespace uper {

inline constexpr per::Variant kVariant = per::Variant::Unaligned;

using per::IntegerRange;
using per::SizeRange;
using per::IntegerConstraint;
using per::SizeConstraint;

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

inline Result<void> encode_integer(BitWriter& out, std::int64_t value,
                                   const per::IntegerConstraint& c = {}) {
  return per::encode_integer(out, kVariant, value, c);
}
inline Result<void> encode_integer(BitWriter& out, const BigInteger& value,
                                   const per::IntegerConstraint& c = {}) {
  return per::encode_integer(out, kVariant, value, c);
}
inline Result<std::int64_t> decode_integer(BitReader& in,
                                           const per::IntegerConstraint& c = {}) {
  return per::decode_integer(in, kVariant, c);
}
inline Result<BigInteger> decode_big_integer(BitReader& in,
                                             const per::IntegerConstraint& c = {}) {
  return per::decode_big_integer(in, kVariant, c);
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
                                     bool extensions_present,
                                     Span<const std::uint8_t> optionals_present) {
  per::encode_sequence_preamble(out, kVariant, extensible, extensions_present,
                                optionals_present);
}
inline Result<per::SequencePreamble> decode_sequence_preamble(BitReader& in, bool extensible,
                                                              std::size_t n_optionals) {
  return per::decode_sequence_preamble(in, kVariant, extensible, n_optionals);
}

inline void encode_extension_additions(
    BitWriter& out, Span<const std::uint8_t> presence,
    const std::vector<std::vector<std::uint8_t>>& open_types) {
  per::encode_extension_additions(out, kVariant, presence, open_types);
}
inline Result<per::ExtensionAdditions> decode_extension_additions(BitReader& in) {
  return per::decode_extension_additions(in, kVariant);
}

inline void encode_choice_root(BitWriter& out, std::size_t index, std::size_t root_count,
                               bool extensible = false) {
  per::encode_choice_root(out, kVariant, index, root_count, extensible);
}
inline void encode_choice_extension(BitWriter& out, std::size_t ext_index,
                                    Span<const std::uint8_t> open_type_content) {
  per::encode_choice_extension(out, kVariant, ext_index, open_type_content);
}
inline Result<per::ExtensionIndex> decode_choice(BitReader& in, std::size_t root_count,
                                                 bool extensible = false) {
  return per::decode_choice(in, kVariant, root_count, extensible);
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
inline std::size_t encode_sequence_of_chunk(BitWriter& out, std::size_t remaining,
                                            bool is_first,
                                            const per::SizeConstraint& size = {}) {
  return per::encode_sequence_of_chunk(out, kVariant, remaining, is_first, size);
}
inline Result<std::size_t> decode_sequence_of_chunk(BitReader& in, bool is_first,
                                                    const per::SizeConstraint& size = {}) {
  return per::decode_sequence_of_chunk(in, kVariant, is_first, size);
}

inline void encode_enumerated(BitWriter& out, std::size_t root_index, std::size_t root_count,
                              bool extensible = false) {
  per::encode_enumerated(out, kVariant, root_index, root_count, extensible);
}
inline void encode_enumerated_extension(BitWriter& out, std::size_t ext_index) {
  per::encode_enumerated_extension(out, kVariant, ext_index);
}
inline Result<per::ExtensionIndex> decode_enumerated(BitReader& in, std::size_t root_count,
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

inline void encode_real(BitWriter& out, double value) {
  per::encode_real(out, kVariant, value);
}
inline Result<double> decode_real(BitReader& in) {
  return per::decode_real(in, kVariant);
}

inline void encode_open_type(BitWriter& out, Span<const std::uint8_t> content) {
  per::encode_open_type(out, kVariant, content);
}
inline Result<std::vector<std::uint8_t>> decode_open_type(BitReader& in) {
  return per::decode_open_type(in, kVariant);
}

}  // namespace uper

namespace aper {

inline constexpr per::Variant kVariant = per::Variant::Aligned;

using per::IntegerRange;
using per::SizeRange;
using per::IntegerConstraint;
using per::SizeConstraint;

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

inline Result<void> encode_integer(BitWriter& out, std::int64_t value,
                                   const per::IntegerConstraint& c = {}) {
  return per::encode_integer(out, kVariant, value, c);
}
inline Result<void> encode_integer(BitWriter& out, const BigInteger& value,
                                   const per::IntegerConstraint& c = {}) {
  return per::encode_integer(out, kVariant, value, c);
}
inline Result<std::int64_t> decode_integer(BitReader& in,
                                           const per::IntegerConstraint& c = {}) {
  return per::decode_integer(in, kVariant, c);
}
inline Result<BigInteger> decode_big_integer(BitReader& in,
                                             const per::IntegerConstraint& c = {}) {
  return per::decode_big_integer(in, kVariant, c);
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
                                     bool extensions_present,
                                     Span<const std::uint8_t> optionals_present) {
  per::encode_sequence_preamble(out, kVariant, extensible, extensions_present,
                                optionals_present);
}
inline Result<per::SequencePreamble> decode_sequence_preamble(BitReader& in, bool extensible,
                                                              std::size_t n_optionals) {
  return per::decode_sequence_preamble(in, kVariant, extensible, n_optionals);
}

inline void encode_extension_additions(
    BitWriter& out, Span<const std::uint8_t> presence,
    const std::vector<std::vector<std::uint8_t>>& open_types) {
  per::encode_extension_additions(out, kVariant, presence, open_types);
}
inline Result<per::ExtensionAdditions> decode_extension_additions(BitReader& in) {
  return per::decode_extension_additions(in, kVariant);
}

inline void encode_choice_root(BitWriter& out, std::size_t index, std::size_t root_count,
                               bool extensible = false) {
  per::encode_choice_root(out, kVariant, index, root_count, extensible);
}
inline void encode_choice_extension(BitWriter& out, std::size_t ext_index,
                                    Span<const std::uint8_t> open_type_content) {
  per::encode_choice_extension(out, kVariant, ext_index, open_type_content);
}
inline Result<per::ExtensionIndex> decode_choice(BitReader& in, std::size_t root_count,
                                                 bool extensible = false) {
  return per::decode_choice(in, kVariant, root_count, extensible);
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
inline std::size_t encode_sequence_of_chunk(BitWriter& out, std::size_t remaining,
                                            bool is_first,
                                            const per::SizeConstraint& size = {}) {
  return per::encode_sequence_of_chunk(out, kVariant, remaining, is_first, size);
}
inline Result<std::size_t> decode_sequence_of_chunk(BitReader& in, bool is_first,
                                                    const per::SizeConstraint& size = {}) {
  return per::decode_sequence_of_chunk(in, kVariant, is_first, size);
}

inline void encode_enumerated(BitWriter& out, std::size_t root_index, std::size_t root_count,
                              bool extensible = false) {
  per::encode_enumerated(out, kVariant, root_index, root_count, extensible);
}
inline void encode_enumerated_extension(BitWriter& out, std::size_t ext_index) {
  per::encode_enumerated_extension(out, kVariant, ext_index);
}
inline Result<per::ExtensionIndex> decode_enumerated(BitReader& in, std::size_t root_count,
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

inline void encode_real(BitWriter& out, double value) {
  per::encode_real(out, kVariant, value);
}
inline Result<double> decode_real(BitReader& in) {
  return per::decode_real(in, kVariant);
}

inline void encode_open_type(BitWriter& out, Span<const std::uint8_t> content) {
  per::encode_open_type(out, kVariant, content);
}
inline Result<std::vector<std::uint8_t>> decode_open_type(BitReader& in) {
  return per::decode_open_type(in, kVariant);
}

}  // namespace aper
}  // namespace asn1
