#pragma once

#include <asn1/runtime/xer/codec.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace cxer {

/// CXER reuses the BASIC-XER element tree.
using xer::Element;
using xer::BitStringValue;

/// Canonical serialize: empty prolog, no inter-tag whitespace, empty tags as
/// `<name/>`, text escapes only `&` / `<` (raw UTF-8 for other characters).
void write_element(std::string& out, const Element& el);
std::string to_string(const Element& el);

/// Parse then require `to_string(el) == xml` (rejects whitespace, prolog, NCRs,
/// `<name></name>` empty form, `<name />` spacing, etc.).
Result<Element> parse_document(const std::string& xml);

// ---- Named type encodings (same Element shape as BASIC-XER) ----

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

Element encode_enumerated(const std::string& name, const std::string& identifier);
Result<std::string> decode_enumerated(const Element& el);

Element encode_object_identifier(const std::string& name, Span<const std::uint64_t> arcs);
Result<std::vector<std::uint64_t>> decode_object_identifier(const Element& el);

Element encode_real(const std::string& name, double value);
Result<double> decode_real(const Element& el);

// ---- Constructed scaffolding ----

Element make_sequence(const std::string& name, std::vector<Element> members);
Result<const Element*> find_child(const Element& parent, const std::string& name);

Element encode_choice(const std::string& name, Element alternative);
Result<const Element*> decode_choice_alternative(const Element& el);

Element encode_sequence_of(const std::string& name, std::vector<Element> items);

/// SET OF: children sorted by ascending CXER octet/character string order.
Element encode_set_of(const std::string& name, std::vector<Element> items);
Result<void> require_set_of_order(Span<const Element> items);

Element encode_boolean_item(bool value);
Element encode_integer_item(std::int64_t value);
Element encode_integer_item(const BigInteger& value);
Element encode_null_item();

}  // namespace cxer
}  // namespace asn1
