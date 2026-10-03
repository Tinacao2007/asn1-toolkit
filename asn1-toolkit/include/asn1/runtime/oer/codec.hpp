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

/// INTEGER value constraints (host int64).
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

  bool is_fixed() const noexcept {
    return lower.has_value() && upper.has_value() && *lower == *upper && !extensible;
  }
};

// ---- Length determinant (X.696) ----

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

/// BIT STRING: `bit_length` is the ASN.1 length in bits.
void encode_bit_string(ByteWriter& out, Span<const std::uint8_t> bits, std::size_t bit_length,
                       const SizeConstraint& size = {});
using BitStringValue = asn1::BitStringValue;
Result<BitStringValue> decode_bit_string(ByteReader& in, const SizeConstraint& size = {});

void encode_utf8_string(ByteWriter& out, const std::string& value,
                        const SizeConstraint& size = {});
Result<std::string> decode_utf8_string(ByteReader& in, const SizeConstraint& size = {});

/// ENUMERATED as OER integer enumeration value (not PER index).
void encode_enumerated(ByteWriter& out, std::int64_t value);
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
void encode_open_type(ByteWriter& out, Span<const std::uint8_t> content);
Result<std::vector<std::uint8_t>> decode_open_type(ByteReader& in);

/// After root components when preamble.extensions_present.
/// `open_types` has one entry per set bit in `presence`, in bitmap order.
void encode_extension_additions(ByteWriter& out, Span<const bool> presence,
                                const std::vector<std::vector<std::uint8_t>>& open_types);
Result<ExtensionAdditions> decode_extension_additions(ByteReader& in);

/// CHOICE: encode/decode a context-specific tag number (AUTOMATIC TAGS style).
void encode_choice_tag(ByteWriter& out, std::uint64_t tag_number, bool constructed = false);
Result<std::uint64_t> decode_choice_tag(ByteReader& in);

/// SEQUENCE OF / SET OF length (always a length determinant in basic OER).
void encode_sequence_of_length(ByteWriter& out, std::size_t count);
Result<std::size_t> decode_sequence_of_length(ByteReader& in);

}  // namespace oer
}  // namespace asn1
