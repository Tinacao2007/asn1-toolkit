#pragma once

#include <asn1/runtime/bit_string.hpp>
#include <asn1/runtime/bigint.hpp>
#include <asn1/runtime/xer/xml.hpp>
#include <asn1/common/span.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace xer {

using BitStringValue = asn1::BitStringValue;

// ---- Named type encodings (BASIC-XER / X.680 XMLValue style) ----

Element encode_boolean(const std::string& name, bool value);
Result<bool> decode_boolean(const Element& el);

Element encode_null(const std::string& name);
Result<void> decode_null(const Element& el);

Element encode_integer(const std::string& name, std::int64_t value);
Element encode_integer(const std::string& name, const BigInteger& value);
Result<std::int64_t> decode_integer(const Element& el);
Result<BigInteger> decode_big_integer(const Element& el);

Element encode_octet_string(const std::string& name, Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_octet_string(const Element& el);

Element encode_bit_string(const std::string& name, Span<const std::uint8_t> bits,
                          std::size_t bit_length);
Result<BitStringValue> decode_bit_string(const Element& el);

Element encode_utf8_string(const std::string& name, const std::string& value);
Result<std::string> decode_utf8_string(const Element& el);

/// ENUMERATED: `<name><identifier /></name>`.
Element encode_enumerated(const std::string& name, const std::string& identifier);
Result<std::string> decode_enumerated(const Element& el);

Element encode_object_identifier(const std::string& name, Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_object_identifier(const Element& el);

Element encode_real(const std::string& name, double value);
Result<double> decode_real(const Element& el);

// ---- Constructed scaffolding ----

Element make_sequence(const std::string& name, std::vector<Element> members);
Result<const Element*> find_child(const Element& parent, const std::string& name);

/// CHOICE: wrap the selected alternative element under `name`.
Element encode_choice(const std::string& name, Element alternative);
/// Returns the single child (the alternative element), or error.
Result<const Element*> decode_choice_alternative(const Element& el);

Element encode_sequence_of(const std::string& name, std::vector<Element> items);

/// Untagged / SEQUENCE OF item helpers (type-name as element).
Element encode_boolean_item(bool value);
Element encode_integer_item(std::int64_t value);
Element encode_integer_item(const BigInteger& value);
Element encode_null_item();

}  // namespace xer
}  // namespace asn1
