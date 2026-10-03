#pragma once

#include <asn1/runtime/xer/codec.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace exer {

/// EXTENDED-XER runtime: BASIC-XER Element tree plus encoding-instruction effects.
/// Frontend parsing of `[ATTRIBUTE]` / encoding-control sections remains later.
using xer::Attribute;
using xer::BitStringValue;
using xer::Element;

using xer::parse_document;
using xer::to_string;
using xer::write_element;

// ---- ATTRIBUTE ----

void set_attribute(Element& el, const std::string& name, const std::string& value);
Result<std::string> get_attribute(const Element& el, const std::string& name);

void encode_attribute_string(Element& parent, const std::string& attr_name,
                             const std::string& value);
void encode_attribute_integer(Element& parent, const std::string& attr_name,
                              std::int64_t value);
void encode_attribute_integer(Element& parent, const std::string& attr_name,
                              const BigInteger& value);
/// ATTRIBUTE + TEXT-style boolean: attribute value "true" / "false".
void encode_attribute_boolean(Element& parent, const std::string& attr_name, bool value);
Result<bool> decode_attribute_boolean(const Element& el, const std::string& attr_name);

// ---- BASE64 (OCTET STRING) ----

std::string encode_base64(Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_base64(const std::string& text);

Element encode_octet_string_base64(const std::string& name, Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_octet_string_base64(const Element& el);

// ---- TEXT (BOOLEAN / ENUMERATED as character data) ----

Element encode_boolean_text(const std::string& name, bool value);
Result<bool> decode_boolean_text(const Element& el);

Element encode_enumerated_text(const std::string& name, const std::string& identifier);
Result<std::string> decode_enumerated_text(const Element& el);

// ---- USE-NUMBER (ENUMERATED as decimal) ----

Element encode_enumerated_number(const std::string& name, std::int64_t value);
Result<std::int64_t> decode_enumerated_number(const Element& el);

// ---- LIST (SEQUENCE OF / SET OF as space-separated list) ----
// Element type must be character-encodable (X.693): no empty tokens, no whitespace
// inside a token. Supported: strings, INTEGER, ENUMERATED ids, BOOLEAN, OID,
// REAL, OCTET STRING (hex), BIT STRING (bit chars).

Element encode_list_of_strings(const std::string& name,
                               const std::vector<std::string>& items);
Result<std::vector<std::string>> decode_list_of_strings(const Element& el);

Element encode_list_of_integers(const std::string& name,
                                const std::vector<std::int64_t>& items);
Element encode_list_of_integers(const std::string& name,
                                const std::vector<BigInteger>& items);
Result<std::vector<std::int64_t>> decode_list_of_integers(const Element& el);
Result<std::vector<BigInteger>> decode_list_of_big_integers(const Element& el);

Element encode_list_of_booleans(const std::string& name,
                                const std::vector<std::uint8_t>& items_as_0_1);
Result<std::vector<std::uint8_t>> decode_list_of_booleans(const Element& el);

Element encode_list_of_object_identifiers(
    const std::string& name, const std::vector<std::vector<std::uint64_t>>& items);
Result<std::vector<std::vector<std::uint64_t>>> decode_list_of_object_identifiers(
    const Element& el);

Element encode_list_of_reals(const std::string& name, const std::vector<double>& items);
Result<std::vector<double>> decode_list_of_reals(const Element& el);

Element encode_list_of_octet_strings(const std::string& name,
                                     const std::vector<std::vector<std::uint8_t>>& items);
Result<std::vector<std::vector<std::uint8_t>>> decode_list_of_octet_strings(
    const Element& el);

Element encode_list_of_bit_strings(const std::string& name,
                                   const std::vector<xer::BitStringValue>& items);
Result<std::vector<xer::BitStringValue>> decode_list_of_bit_strings(const Element& el);

// ---- UNTAGGED (partial content merged into parent) ----

/// Append attributes and children of `fragment` into `parent` (no wrapper element).
void append_untagged(Element& parent, Element fragment);

// ---- NAME ----

void set_name(Element& el, const std::string& name);

// ---- USE-NIL ----

/// Mark element with `xsi:nil="true"` (adds xmlns:xsi if missing).
void set_nil(Element& el, bool is_nil = true);
Result<bool> is_nil(const Element& el);

// ---- Default BASIC-XER primitives (convenience) ----

using xer::encode_boolean;
using xer::decode_boolean;
using xer::encode_null;
using xer::decode_null;
using xer::encode_integer;
using xer::decode_integer;
using xer::decode_big_integer;
using xer::encode_octet_string;
using xer::decode_octet_string;
using xer::encode_bit_string;
using xer::decode_bit_string;
using xer::encode_utf8_string;
using xer::decode_utf8_string;
using xer::encode_enumerated;
using xer::decode_enumerated;
using xer::encode_object_identifier;
using xer::decode_object_identifier;
using xer::encode_real;
using xer::decode_real;
using xer::make_sequence;
using xer::find_child;
using xer::encode_choice;
using xer::decode_choice_alternative;
using xer::encode_sequence_of;

}  // namespace exer
}  // namespace asn1
