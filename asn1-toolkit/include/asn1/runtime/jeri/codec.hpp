/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/jeri/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Canonical JER (JERI) codec API.
**
** Specification: ITU-T X.697 — Canonical JSON Encoding Rules (JERI).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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

/**
 *  Function    : encode_base64
 *  Description : Builds and returns a string for encode base64.
 *  Parameters  : value — Span<const std::uint8_t> value
 *  Returns     : std::string
 */
std::string encode_base64(Span<const std::uint8_t> value);
/**
 *  Function    : decode_base64
 *  Description : Returns success or an error from decode base64.
 *  Parameters  : text — const std::string& text
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_base64(const std::string& text);

/**
 *  Function    : encode_octet_string_base64
 *  Description : Computes encode octet string base64 from (value).
 *  Parameters  : value — Span<const std::uint8_t> value
 *  Returns     : Value
 */
Value encode_octet_string_base64(Span<const std::uint8_t> value);
/**
 *  Function    : decode_octet_string_base64
 *  Description : Returns success or an error from decode octet string base64.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string_base64(const Value& v);

// ---- ARRAY (SEQUENCE as JSON array) ----

/// Encode SEQUENCE components in declaration order. Absent OPTIONAL/DEFAULT
/// components are JSON null. Trailing nulls are omitted when `omit_trailing_nulls`.
Value encode_sequence_array(std::vector<Value> components,
                            bool omit_trailing_nulls = true);
/**
 *  Function    : decode_sequence_array
 *  Description : Returns success or an error from decode sequence array.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<const std::vector<Value>*>
 */
Result<const std::vector<Value>*> decode_sequence_array(const Value& v);

// ---- OBJECT (SET OF SEQUENCE { key, value } as JSON object) ----

/**
 *  Function    : encode_set_of_object
 *  Description : Computes encode set of object from (std::vector<std::pair<std::string, entries).
 *  Parameters  : std::vector<std::pair<std::string — std::vector<std::pair<std::string; entries — Value>> entries
 *  Returns     : Value
 */
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

/**
 *  Function    : encode_choice_unwrapped
 *  Description : Computes encode choice unwrapped from (alternative_encoding).
 *  Parameters  : alternative_encoding — Value alternative_encoding
 *  Returns     : Value
 */
Value encode_choice_unwrapped(Value alternative_encoding);
/// Identity: the Value *is* the selected alternative's encoding.
/**
 *  Function    : decode_choice_unwrapped
 *  Description : Returns success or an error from decode choice unwrapped.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<const Value*>
 */
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
