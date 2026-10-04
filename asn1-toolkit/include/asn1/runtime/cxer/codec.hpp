/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/cxer/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Canonical XER (CXER) encode/decode API.
**
** Specification: ITU-T X.693 — Canonical XML Encoding Rules (CXER).
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
namespace cxer {

/// CXER reuses the BASIC-XER element tree.
using xer::Element;
using xer::BitStringValue;

/// Canonical serialize: empty prolog, no inter-tag whitespace, empty tags as
/// `<name/>`, text escapes only `&` / `<` (raw UTF-8 for other characters).
/**
 *  Function    : write_element
 *  Description : Performs write element (declaration).
 *  Parameters  : out — std::string& out; el — const Element& el
 *  Returns     : void
 */
void write_element(std::string& out, const Element& el);
/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : el — const Element& el
 *  Returns     : std::string
 */
std::string to_string(const Element& el);

/// Parse then require `to_string(el) == xml` (rejects whitespace, prolog, NCRs,
/// `<name></name>` empty form, `<name />` spacing, etc.).
/**
 *  Function    : parse_document
 *  Description : Returns success or an error from parse document.
 *  Parameters  : xml — const std::string& xml
 *  Returns     : Result<Element>
 */
Result<Element> parse_document(const std::string& xml);

// ---- Named type encodings (same Element shape as BASIC-XER) ----

/**
 *  Function    : encode_boolean
 *  Description : Computes encode boolean from (name, value).
 *  Parameters  : name — const std::string& name; value — bool value
 *  Returns     : Element
 */
Element encode_boolean(const std::string& name, bool value);
/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from el.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(const Element& el);

/**
 *  Function    : encode_null
 *  Description : Computes encode null from (name).
 *  Parameters  : name — const std::string& name
 *  Returns     : Element
 */
Element encode_null(const std::string& name);
/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<void>
 */
Result<void> decode_null(const Element& el);

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (name, value).
 *  Parameters  : name — const std::string& name; value — std::int64_t value
 *  Returns     : Element
 */
Element encode_integer(const std::string& name, std::int64_t value);
/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (name, value).
 *  Parameters  : name — const std::string& name; value — const BigInteger& value
 *  Returns     : Element
 */
Element encode_integer(const std::string& name, const BigInteger& value);
/**
 *  Function    : decode_integer
 *  Description : Returns success or an error from decode integer.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer(const Element& el);
/**
 *  Function    : decode_big_integer
 *  Description : Returns success or an error from decode big integer.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer(const Element& el);

/**
 *  Function    : encode_octet_string
 *  Description : Computes encode octet string from (name, value).
 *  Parameters  : name — const std::string& name; value — Span<const std::uint8_t> value
 *  Returns     : Element
 */
Element encode_octet_string(const std::string& name, Span<const std::uint8_t> value);
/**
 *  Function    : decode_octet_string
 *  Description : Returns success or an error from decode octet string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string(const Element& el);

Element encode_bit_string(const std::string& name, Span<const std::uint8_t> bits,
                          std::size_t bit_length);
/**
 *  Function    : decode_bit_string
 *  Description : Returns success or an error from decode bit string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_string(const Element& el);

/**
 *  Function    : encode_utf8_string
 *  Description : Computes encode utf8 string from (name, value).
 *  Parameters  : name — const std::string& name; value — const std::string& value
 *  Returns     : Element
 */
Element encode_utf8_string(const std::string& name, const std::string& value);
/**
 *  Function    : decode_utf8_string
 *  Description : Builds and returns a string for decode utf8 string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string(const Element& el);

/**
 *  Function    : encode_enumerated
 *  Description : Computes encode enumerated from (name, identifier).
 *  Parameters  : name — const std::string& name; identifier — const std::string& identifier
 *  Returns     : Element
 */
Element encode_enumerated(const std::string& name, const std::string& identifier);
/**
 *  Function    : decode_enumerated
 *  Description : Builds and returns a string for decode enumerated.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated(const Element& el);

/**
 *  Function    : encode_object_identifier
 *  Description : Computes encode object identifier from (name, arcs).
 *  Parameters  : name — const std::string& name; arcs — Span<const std::uint64_t> arcs
 *  Returns     : Element
 */
Element encode_object_identifier(const std::string& name, Span<const std::uint64_t> arcs);
/**
 *  Function    : decode_object_identifier
 *  Description : Returns success or an error from decode object identifier.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint64_t>>
 */
Result<std::vector<std::uint64_t>> decode_object_identifier(const Element& el);

/**
 *  Function    : encode_real
 *  Description : Computes encode real from (name, value).
 *  Parameters  : name — const std::string& name; value — double value
 *  Returns     : Element
 */
Element encode_real(const std::string& name, double value);
/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<double>
 */
Result<double> decode_real(const Element& el);

// ---- Constructed scaffolding ----

/**
 *  Function    : make_sequence
 *  Description : Computes make sequence from (name, members).
 *  Parameters  : name — const std::string& name; members — std::vector<Element> members
 *  Returns     : Element
 */
Element make_sequence(const std::string& name, std::vector<Element> members);
/**
 *  Function    : find_child
 *  Description : Returns success or an error from find child.
 *  Parameters  : parent — const Element& parent; name — const std::string& name
 *  Returns     : Result<const Element*>
 */
Result<const Element*> find_child(const Element& parent, const std::string& name);

/**
 *  Function    : encode_choice
 *  Description : Computes encode choice from (name, alternative).
 *  Parameters  : name — const std::string& name; alternative — Element alternative
 *  Returns     : Element
 */
Element encode_choice(const std::string& name, Element alternative);
/**
 *  Function    : decode_choice_alternative
 *  Description : Returns success or an error from decode choice alternative.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<const Element*>
 */
Result<const Element*> decode_choice_alternative(const Element& el);

/**
 *  Function    : encode_sequence_of
 *  Description : Computes encode sequence of from (name, items).
 *  Parameters  : name — const std::string& name; items — std::vector<Element> items
 *  Returns     : Element
 */
Element encode_sequence_of(const std::string& name, std::vector<Element> items);

/// SET OF: children sorted by ascending CXER octet/character string order.
/**
 *  Function    : encode_set_of
 *  Description : Computes encode set of from (name, items).
 *  Parameters  : name — const std::string& name; items — std::vector<Element> items
 *  Returns     : Element
 */
Element encode_set_of(const std::string& name, std::vector<Element> items);
/**
 *  Function    : require_set_of_order
 *  Description : Returns success or an error from require set of order.
 *  Parameters  : items — Span<const Element> items
 *  Returns     : Result<void>
 */
Result<void> require_set_of_order(Span<const Element> items);

/**
 *  Function    : encode_boolean_item
 *  Description : Computes encode boolean item from (value).
 *  Parameters  : value — bool value
 *  Returns     : Element
 */
Element encode_boolean_item(bool value);
/**
 *  Function    : encode_integer_item
 *  Description : Computes encode integer item from (value).
 *  Parameters  : value — std::int64_t value
 *  Returns     : Element
 */
Element encode_integer_item(std::int64_t value);
/**
 *  Function    : encode_integer_item
 *  Description : Computes encode integer item from (value).
 *  Parameters  : value — const BigInteger& value
 *  Returns     : Element
 */
Element encode_integer_item(const BigInteger& value);
/**
 *  Function    : encode_null_item
 *  Description : Computes encode null item from (none).
 *  Parameters  : none
 *  Returns     : Element
 */
Element encode_null_item();

}  // namespace cxer
}  // namespace asn1
