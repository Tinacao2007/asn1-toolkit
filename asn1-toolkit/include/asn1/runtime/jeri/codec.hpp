#pragma once

#include <asn1/runtime/jer/codec.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace asn1 {
namespace jeri {

/// JER encoding-instruction effects on the BASIC-JER Value tree (X.697).
/// Frontend parsing of `[ARRAY]` / ENCODING-CONTROL JER remains later.
using jer::BitStringValue;
using jer::Value;
using jer::ValueKind;

using jer::parse_document;
using jer::to_string;
using jer::write_value;

// ---- NAME (component / alternative / enumeration identifier) ----

enum class NameForm {
  AsIs,
  Capitalized,  // first letter upper, rest unchanged
  Uppercased,
  Lowercased,
  Literal,  // use `literal` argument
};

std::string transform_name(const std::string& identifier, NameForm form,
                           const std::string& literal = {});

/// Build a SEQUENCE/SET object member using a NAME instruction for the key.
std::pair<std::string, Value> named_member(const std::string& identifier, Value encoding,
                                           NameForm form,
                                           const std::string& literal = {});

// ---- BASE64 (OCTET STRING) ----

std::string encode_base64(Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_base64(const std::string& text);

Value encode_octet_string_base64(Span<const std::uint8_t> value);
Result<std::vector<std::uint8_t>> decode_octet_string_base64(const Value& v);

// ---- ARRAY (SEQUENCE as JSON array) ----

/// Encode SEQUENCE components in declaration order. Absent OPTIONAL/DEFAULT
/// components are JSON null. Trailing nulls are omitted when `omit_trailing_nulls`.
Value encode_sequence_array(std::vector<Value> components,
                            bool omit_trailing_nulls = true);
Result<const std::vector<Value>*> decode_sequence_array(const Value& v);

// ---- OBJECT (SET OF SEQUENCE { key, value } as JSON object) ----

Value encode_set_of_object(std::vector<std::pair<std::string, Value>> entries);
Result<std::vector<std::pair<std::string, const Value*>>> decode_set_of_object(
    const Value& v);

// ---- TEXT (ENUMERATED identifier transform) ----

Value encode_enumerated_text(const std::string& identifier, NameForm form,
                             const std::string& literal = {});

/// Match JSON string against transformed forms of `identifiers`; returns ASN.1 id.
Result<std::string> decode_enumerated_text(
    const Value& v, const std::vector<std::string>& identifiers, NameForm form,
    const std::vector<std::string>& literals = {});

// ---- UNWRAPPED (CHOICE without single-property wrapper) ----

Value encode_choice_unwrapped(Value alternative_encoding);
/// Identity: the Value *is* the selected alternative's encoding.
Result<const Value*> decode_choice_unwrapped(const Value& v);

// ---- Default BASIC-JER primitives (convenience) ----

using jer::encode_boolean;
using jer::decode_boolean;
using jer::encode_null;
using jer::decode_null;
using jer::encode_integer;
using jer::decode_integer;
using jer::decode_big_integer;
using jer::encode_octet_string;
using jer::decode_octet_string;
using jer::encode_bit_string;
using jer::decode_bit_string;
using jer::encode_utf8_string;
using jer::decode_utf8_string;
using jer::encode_enumerated;
using jer::decode_enumerated;
using jer::encode_object_identifier;
using jer::decode_object_identifier;
using jer::encode_real;
using jer::decode_real;
using jer::make_sequence;
using jer::find_member;
using jer::encode_choice;
using jer::decode_choice;
using jer::encode_sequence_of;
using jer::decode_sequence_of;

}  // namespace jeri
}  // namespace asn1
