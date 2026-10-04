/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/jer/codec.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   JSON Encoding Rules (JER) codec API.
**
** Specification: ITU-T X.697 — JSON Encoding Rules (JER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/runtime/bit_string.hpp>
#include <asn1/runtime/jer/json.hpp>
#include <asn1/common/span.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace asn1 {
namespace jer {

using BitStringValue = asn1::BitStringValue;

// ---- Primitive types (BASIC-JER / X.697) ----

/**
 *  Function    : encode_boolean
 *  Description : Computes encode boolean from (value).
 *  Parameters  : value — bool value
 *  Returns     : Value
 */
Value encode_boolean(bool value);
/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from v.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(const Value& v);

/**
 *  Function    : encode_null
 *  Description : Computes encode null from (none).
 *  Parameters  : none
 *  Returns     : Value
 */
Value encode_null();
/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<void>
 */
Result<void> decode_null(const Value& v);

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (value).
 *  Parameters  : value — std::int64_t value
 *  Returns     : Value
 */
Value encode_integer(std::int64_t value);
/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (value).
 *  Parameters  : value — const BigInteger& value
 *  Returns     : Value
 */
Value encode_integer(const BigInteger& value);
/**
 *  Function    : decode_integer
 *  Description : Returns success or an error from decode integer.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer(const Value& v);
/**
 *  Function    : decode_big_integer
 *  Description : Returns success or an error from decode big integer.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer(const Value& v);

/**
 *  Function    : encode_octet_string
 *  Description : Computes encode octet string from (value).
 *  Parameters  : value — Span<const std::uint8_t> value
 *  Returns     : Value
 */
Value encode_octet_string(Span<const std::uint8_t> value);
/**
 *  Function    : decode_octet_string
 *  Description : Returns success or an error from decode octet string.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string(const Value& v);

/// Unconstrained BIT STRING: `{"value":"<hex>","length":N}`.
/// Fixed-size BIT STRING: JSON hex string only (`fixed_size` bits).
Value encode_bit_string(Span<const std::uint8_t> bits, std::size_t bit_length,
                        bool fixed_size = false);
/**
 *  Function    : decode_bit_string
 *  Description : Returns success or an error from decode bit string.
 *  Parameters  : v — const Value& v; fixed_size — bool fixed_size
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_string(const Value& v, bool fixed_size = false);

/**
 *  Function    : encode_utf8_string
 *  Description : Computes encode utf8 string from (value).
 *  Parameters  : value — const std::string& value
 *  Returns     : Value
 */
Value encode_utf8_string(const std::string& value);
/**
 *  Function    : decode_utf8_string
 *  Description : Builds and returns a string for decode utf8 string.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string(const Value& v);

/**
 *  Function    : encode_enumerated
 *  Description : Computes encode enumerated from (identifier).
 *  Parameters  : identifier — const std::string& identifier
 *  Returns     : Value
 */
Value encode_enumerated(const std::string& identifier);
/**
 *  Function    : decode_enumerated
 *  Description : Builds and returns a string for decode enumerated.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated(const Value& v);

/**
 *  Function    : encode_object_identifier
 *  Description : Computes encode object identifier from (arcs).
 *  Parameters  : arcs — Span<const std::uint64_t> arcs
 *  Returns     : Value
 */
Value encode_object_identifier(Span<const std::uint64_t> arcs);
/**
 *  Function    : decode_object_identifier
 *  Description : Returns success or an error from decode object identifier.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::vector<std::uint64_t>>
 */
Result<std::vector<std::uint64_t>> decode_object_identifier(const Value& v);

/// REAL: JSON number, or strings "INF" / "-INF" / "NaN".
/**
 *  Function    : encode_real
 *  Description : Computes encode real from (value).
 *  Parameters  : value — double value
 *  Returns     : Value
 */
Value encode_real(double value);
/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<double>
 */
Result<double> decode_real(const Value& v);

// ---- Constructed scaffolding ----

/**
 *  Function    : make_sequence
 *  Description : Computes make sequence from (std::vector<std::pair<std::string, members).
 *  Parameters  : std::vector<std::pair<std::string — std::vector<std::pair<std::string; members — Value>> members
 *  Returns     : Value
 */
Value make_sequence(std::vector<std::pair<std::string, Value>> members);
/**
 *  Function    : find_member
 *  Description : Returns success or an error from find member.
 *  Parameters  : object — const Value& object; name — const std::string& name
 *  Returns     : Result<const Value*>
 */
Result<const Value*> find_member(const Value& object, const std::string& name);

/// CHOICE: single-property object `{"altName": <encoding>}`.
/**
 *  Function    : encode_choice
 *  Description : Computes encode choice from (alternative, encoding).
 *  Parameters  : alternative — const std::string& alternative; encoding — Value encoding
 *  Returns     : Value
 */
Value encode_choice(const std::string& alternative, Value encoding);
/**
 *  Function    : decode_choice
 *  Description : Builds and returns a string for decode choice.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::pair<std::string, const Value*>>
 */
Result<std::pair<std::string, const Value*>> decode_choice(const Value& v);

/**
 *  Function    : encode_sequence_of
 *  Description : Computes encode sequence of from (items).
 *  Parameters  : items — std::vector<Value> items
 *  Returns     : Value
 */
Value encode_sequence_of(std::vector<Value> items);
/**
 *  Function    : decode_sequence_of
 *  Description : Returns success or an error from decode sequence of.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<const std::vector<Value>*>
 */
Result<const std::vector<Value>*> decode_sequence_of(const Value& v);

}  // namespace jer
}  // namespace asn1
