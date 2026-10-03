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

void encode_length(ByteWriter& out, std::size_t length);
Result<std::size_t> decode_length(ByteReader& in);

// ---- Primitive types ----

void encode_boolean(ByteWriter& out, bool value);
Result<bool> decode_boolean(ByteReader& in);

void encode_null(ByteWriter& out);
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

void encode_enumerated(ByteWriter& out, std::int64_t value);
Result<std::int64_t> decode_enumerated(ByteReader& in);

void encode_object_identifier(ByteWriter& out, Span<const std::uint64_t> arcs,
                              bool relative = false);
Result<std::vector<std::uint64_t>> decode_object_identifier(ByteReader& in,
                                                           bool relative = false);

using oer::RealIeeeForm;

Result<void> encode_real(ByteWriter& out, double value,
                         RealIeeeForm form = RealIeeeForm::Unconstrained);
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

void encode_choice_tag(ByteWriter& out, std::uint64_t tag_number, bool constructed = false);
Result<std::uint64_t> decode_choice_tag(ByteReader& in);

void encode_sequence_of_length(ByteWriter& out, std::size_t count);
Result<std::size_t> decode_sequence_of_length(ByteReader& in);

/// SET OF: quantity + components sorted by ascending octet-string order (COER).
/// `components` are complete encodings of each element; they are sorted in place
/// order before writing (caller's vector is not modified — a copy is sorted).
void encode_set_of(ByteWriter& out, std::vector<std::vector<std::uint8_t>> components);

/// Require already-split SET OF component encodings to be in ascending order.
Result<void> require_set_of_order(Span<const std::vector<std::uint8_t>> components);

}  // namespace coer
}  // namespace asn1
