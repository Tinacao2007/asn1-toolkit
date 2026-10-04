/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/jer/json.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   JSON value AST for JER/JERI codecs.
**
** Specification: ITU-T X.697 — JSON-related helpers for JER/JERI codecs.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/common/result.hpp>
#include <asn1/runtime/bigint.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace asn1 {
namespace jer {

enum class ValueKind { Null, Bool, Number, String, Array, Object };

/// Minimal JSON value tree for JER (X.697). Numbers are host int64, BigInteger, or double.
struct Value {
  ValueKind kind = ValueKind::Null;
  bool bool_value = false;
  std::int64_t number = 0;
  double real = 0.0;
  bool number_is_real = false;     // meaningful when kind == Number
  bool number_is_bigint = false;   // decimal digits in string_value
  std::string string_value;
  std::vector<Value> array;
  std::vector<std::pair<std::string, Value>> object;

  /**
   *  Function    : null_value
   *  Description : Computes null value from (none).
   *  Parameters  : none
   *  Returns     : static Value
   */
  static Value null_value() { return Value{}; }
  /**
   *  Function    : boolean
   *  Description : Computes boolean from (v).
   *  Parameters  : v — bool v
   *  Returns     : static Value
   */
  static Value boolean(bool v) {
    Value x;
    x.kind = ValueKind::Bool;
    x.bool_value = v;
    return x;
  }
  /**
   *  Function    : integer
   *  Description : Computes integer from (v).
   *  Parameters  : v — std::int64_t v
   *  Returns     : static Value
   */
  static Value integer(std::int64_t v) {
    Value x;
    x.kind = ValueKind::Number;
    x.number = v;
    x.number_is_real = false;
    x.number_is_bigint = false;
    return x;
  }
  /**
   *  Function    : big_integer
   *  Description : Computes big integer from (v).
   *  Parameters  : v — BigInteger v
   *  Returns     : static Value
   */
  static Value big_integer(BigInteger v) {
    Value x;
    x.kind = ValueKind::Number;
    x.number_is_real = false;
    x.number_is_bigint = true;
    /**
     *  Function    : to_decimal
     *  Description : Computes to decimal from (none).
     *  Parameters  : none
     *  Returns     : x.string_value = v.
     */
    x.string_value = v.to_decimal();
    if (auto narrow = v.as_i64()) {
      x.number = *narrow;
      x.number_is_bigint = false;
    }
    return x;
  }
  /**
   *  Function    : real_number
   *  Description : Computes real number from (v).
   *  Parameters  : v — double v
   *  Returns     : static Value
   */
  static Value real_number(double v) {
    Value x;
    x.kind = ValueKind::Number;
    x.real = v;
    x.number_is_real = true;
    return x;
  }
  /**
   *  Function    : string
   *  Description : Computes string from (v).
   *  Parameters  : v — std::string v
   *  Returns     : static Value
   */
  static Value string(std::string v) {
    Value x;
    x.kind = ValueKind::String;
    /**
     *  Function    : move
     *  Description : Computes move from (v).
     *  Parameters  : v — v
     *  Returns     : x.string_value = std::
     */
    x.string_value = std::move(v);
    return x;
  }
  /**
   *  Function    : make_array
   *  Description : Computes make array from (items).
   *  Parameters  : items — std::vector<Value> items
   *  Returns     : static Value
   */
  static Value make_array(std::vector<Value> items = {}) {
    Value x;
    x.kind = ValueKind::Array;
    /**
     *  Function    : move
     *  Description : Computes move from (items).
     *  Parameters  : items — items
     *  Returns     : x.array = std::
     */
    x.array = std::move(items);
    return x;
  }
  /**
   *  Function    : make_object
   *  Description : Computes make object from (std::vector<std::pair<std::string, members).
   *  Parameters  : std::vector<std::pair<std::string — std::vector<std::pair<std::string; members — Value>> members
   *  Returns     : static Value
   */
  static Value make_object(std::vector<std::pair<std::string, Value>> members = {}) {
    Value x;
    x.kind = ValueKind::Object;
    /**
     *  Function    : move
     *  Description : Computes move from (members).
     *  Parameters  : members — members
     *  Returns     : x.object = std::
     */
    x.object = std::move(members);
    return x;
  }
};

/// Compact JSON (no insignificant whitespace).
/**
 *  Function    : write_value
 *  Description : Performs write value (declaration).
 *  Parameters  : out — std::string& out; v — const Value& v
 *  Returns     : void
 */
void write_value(std::string& out, const Value& v);
/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : v — const Value& v
 *  Returns     : std::string
 */
std::string to_string(const Value& v);

/**
 *  Function    : parse_document
 *  Description : Returns success or an error from parse document.
 *  Parameters  : json — const std::string& json
 *  Returns     : Result<Value>
 */
Result<Value> parse_document(const std::string& json);

}  // namespace jer
}  // namespace asn1
