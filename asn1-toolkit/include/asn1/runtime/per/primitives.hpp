#pragma once

#include <asn1/runtime/bit_io.hpp>

#include <cstdint>
#include <vector>

namespace asn1 {
namespace per {

/// Shared PER implementation; UPER passes Unaligned, APER passes Aligned.
enum class Variant { Unaligned, Aligned };

inline void maybe_align(BitWriter& w, Variant v) {
  if (v == Variant::Aligned) {
    w.align_to_octet();
  }
}

inline Result<void> maybe_align(BitReader& r, Variant v) {
  if (v == Variant::Aligned) {
    return r.align_to_octet();
  }
  return Result<void>::success();
}

/// Minimum bits to encode values in [0, range-1]. range==0/1 => 0.
std::size_t bits_for_range(std::uint64_t range);

// ---- Whole numbers (X.691 clauses 11.5--11.8 / 10.6) ----

void encode_constrained_whole_number(BitWriter& out, Variant variant,
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
Result<std::int64_t> decode_unconstrained_whole_number(BitReader& in,
                                                       Variant variant);

void encode_normally_small_non_negative_whole_number(BitWriter& out, Variant variant,
                                                     std::uint64_t value);
Result<std::uint64_t> decode_normally_small_non_negative_whole_number(
    BitReader& in, Variant variant);

/// Normally small length (1..64 common case; up to 127 supported).
void encode_normally_small_length(BitWriter& out, Variant variant, std::size_t length);
Result<std::size_t> decode_normally_small_length(BitReader& in, Variant variant);

// ---- Length determinant (X.691 10.9) ----

/// Encode one length determinant fragment. Returns the fragment size actually
/// signaled (may be less than `length` when fragmented: 16K/32K/48K/64K).
std::size_t encode_length_determinant(BitWriter& out, Variant variant,
                                      std::size_t length);
Result<std::size_t> decode_length_determinant(BitReader& in, Variant variant);

// ---- Structural helpers ----

/// CHOICE index among `count` root alternatives (index in 0..count-1).
void encode_choice_index(BitWriter& out, Variant variant, std::size_t index,
                         std::size_t count);
Result<std::size_t> decode_choice_index(BitReader& in, Variant variant,
                                        std::size_t count);

/// OPTIONAL / DEFAULT presence bitmap (bit 0 = first field). Non-zero => 1.
void encode_bitmap(BitWriter& out, Span<const std::uint8_t> bits);
Result<std::vector<std::uint8_t>> decode_bitmap(BitReader& in, std::size_t nbits);

/// Open type: [align if APER] + length determinant + contents octets.
void encode_open_type(BitWriter& out, Variant variant,
                      Span<const std::uint8_t> content);
Result<std::vector<std::uint8_t>> decode_open_type(BitReader& in, Variant variant);

}  // namespace per
}  // namespace asn1
