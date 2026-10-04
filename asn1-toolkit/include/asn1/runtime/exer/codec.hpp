/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/exer/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Extended XER (E-XER) encode/decode API.
**
** Specification: ITU-T X.693 — Extended XER (E-XER) and encoding
**                 instructions.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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

/**
 *  Function    : set_attribute
 *  Description : Performs set attribute (declaration).
 *  Parameters  : el — Element& el; name — const std::string& name; value — const std::string& value
 *  Returns     : void
 */
void set_attribute(Element& el, const std::string& name, const std::string& value);
/**
 *  Function    : get_attribute
 *  Description : Builds and returns a string for get attribute.
 *  Parameters  : el — const Element& el; name — const std::string& name
 *  Returns     : Result<std::string>
 */
Result<std::string> get_attribute(const Element& el, const std::string& name);

void encode_attribute_string(Element& parent, const std::string& attr_name,
                             const std::string& value);
void encode_attribute_integer(Element& parent, const std::string& attr_name,
                              std::int64_t value);
void encode_attribute_integer(Element& parent, const std::string& attr_name,
                              const BigInteger& value);
/// ATTRIBUTE + TEXT-style boolean: attribute value "true" / "false".
/**
 *  Function    : encode_attribute_boolean
 *  Description : Performs encode attribute boolean (declaration).
 *  Parameters  : parent — Element& parent; attr_name — const std::string& attr_name; value — bool value
 *  Returns     : void
 */
void encode_attribute_boolean(Element& parent, const std::string& attr_name, bool value);
/**
 *  Function    : decode_attribute_boolean
 *  Description : Returns a boolean result from el, attr_name.
 *  Parameters  : el — const Element& el; attr_name — const std::string& attr_name
 *  Returns     : Result<bool>
 */
Result<bool> decode_attribute_boolean(const Element& el, const std::string& attr_name);

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
 *  Description : Computes encode octet string base64 from (name, value).
 *  Parameters  : name — const std::string& name; value — Span<const std::uint8_t> value
 *  Returns     : Element
 */
Element encode_octet_string_base64(const std::string& name, Span<const std::uint8_t> value);
/**
 *  Function    : decode_octet_string_base64
 *  Description : Returns success or an error from decode octet string base64.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string_base64(const Element& el);

// ---- TEXT (BOOLEAN / ENUMERATED as character data) ----

/**
 *  Function    : encode_boolean_text
 *  Description : Computes encode boolean text from (name, value).
 *  Parameters  : name — const std::string& name; value — bool value
 *  Returns     : Element
 */
Element encode_boolean_text(const std::string& name, bool value);
/**
 *  Function    : decode_boolean_text
 *  Description : Returns a boolean result from el.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean_text(const Element& el);

/**
 *  Function    : encode_enumerated_text
 *  Description : Computes encode enumerated text from (name, identifier).
 *  Parameters  : name — const std::string& name; identifier — const std::string& identifier
 *  Returns     : Element
 */
Element encode_enumerated_text(const std::string& name, const std::string& identifier);
/**
 *  Function    : decode_enumerated_text
 *  Description : Builds and returns a string for decode enumerated text.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated_text(const Element& el);

// ---- USE-NUMBER (ENUMERATED as decimal) ----

/**
 *  Function    : encode_enumerated_number
 *  Description : Computes encode enumerated number from (name, value).
 *  Parameters  : name — const std::string& name; value — std::int64_t value
 *  Returns     : Element
 */
Element encode_enumerated_number(const std::string& name, std::int64_t value);
/**
 *  Function    : decode_enumerated_number
 *  Description : Returns success or an error from decode enumerated number.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_enumerated_number(const Element& el);

// ---- LIST (SEQUENCE OF / SET OF as space-separated list) ----
// Element type must be character-encodable (X.693): no empty tokens, no whitespace
// inside a token. Supported: strings, INTEGER, ENUMERATED ids, BOOLEAN, OID,
// REAL, OCTET STRING (hex), BIT STRING (bit chars).

Element encode_list_of_strings(const std::string& name,
                               const std::vector<std::string>& items);
/**
 *  Function    : decode_list_of_strings
 *  Description : Builds and returns a string for decode list of strings.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::string>>
 */
Result<std::vector<std::string>> decode_list_of_strings(const Element& el);

Element encode_list_of_integers(const std::string& name,
                                const std::vector<std::int64_t>& items);
Element encode_list_of_integers(const std::string& name,
                                const std::vector<BigInteger>& items);
/**
 *  Function    : decode_list_of_integers
 *  Description : Returns success or an error from decode list of integers.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::int64_t>>
 */
Result<std::vector<std::int64_t>> decode_list_of_integers(const Element& el);
/**
 *  Function    : decode_list_of_big_integers
 *  Description : Returns success or an error from decode list of big integers.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<BigInteger>>
 */
Result<std::vector<BigInteger>> decode_list_of_big_integers(const Element& el);

Element encode_list_of_booleans(const std::string& name,
                                const std::vector<std::uint8_t>& items_as_0_1);
/**
 *  Function    : decode_list_of_booleans
 *  Description : Returns success or an error from decode list of booleans.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_list_of_booleans(const Element& el);

Element encode_list_of_object_identifiers(
    const std::string& name, const std::vector<std::vector<std::uint64_t>>& items);
Result<std::vector<std::vector<std::uint64_t>>> decode_list_of_object_identifiers(
    const Element& el);

/**
 *  Function    : encode_list_of_reals
 *  Description : Computes encode list of reals from (name, items).
 *  Parameters  : name — const std::string& name; items — const std::vector<double>& items
 *  Returns     : Element
 */
Element encode_list_of_reals(const std::string& name, const std::vector<double>& items);
/**
 *  Function    : decode_list_of_reals
 *  Description : Returns success or an error from decode list of reals.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<double>>
 */
Result<std::vector<double>> decode_list_of_reals(const Element& el);

Element encode_list_of_octet_strings(const std::string& name,
                                     const std::vector<std::vector<std::uint8_t>>& items);
Result<std::vector<std::vector<std::uint8_t>>> decode_list_of_octet_strings(
    const Element& el);

Element encode_list_of_bit_strings(const std::string& name,
                                   const std::vector<xer::BitStringValue>& items);
/**
 *  Function    : decode_list_of_bit_strings
 *  Description : Returns success or an error from decode list of bit strings.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<xer::BitStringValue>>
 */
Result<std::vector<xer::BitStringValue>> decode_list_of_bit_strings(const Element& el);

// ---- UNTAGGED (partial content merged into parent) ----

/// Append attributes and children of `fragment` into `parent` (no wrapper element).
/**
 *  Function    : append_untagged
 *  Description : Performs append untagged (declaration).
 *  Parameters  : parent — Element& parent; fragment — Element fragment
 *  Returns     : void
 */
void append_untagged(Element& parent, Element fragment);

// ---- NAME ----

/**
 *  Function    : set_name
 *  Description : Performs set name (declaration).
 *  Parameters  : el — Element& el; name — const std::string& name
 *  Returns     : void
 */
void set_name(Element& el, const std::string& name);

// ---- USE-NIL ----

/// Mark element with `xsi:nil="true"` (adds xmlns:xsi if missing).
/**
 *  Function    : set_nil
 *  Description : Performs set nil (declaration).
 *  Parameters  : el — Element& el; is_nil — bool is_nil
 *  Returns     : void
 */
void set_nil(Element& el, bool is_nil = true);
/**
 *  Function    : is_nil
 *  Description : Returns whether nil holds for the given inputs.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
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
