/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/jer/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   JER mapping between ASN.1 values and JSON.
**
** Specification: ITU-T X.697 — JSON Encoding Rules (JER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/jer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cctype>
#include <limits>

namespace asn1 {
namespace jer {
namespace {

/**
 *  Function    : bad
 *  Description : Computes bad from (offset, message).
 *  Parameters  : offset — std::size_t offset; message — std::string message
 *  Returns     : Error
 */
Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

/**
 *  Function    : hex_nibble
 *  Description : Computes hex nibble from (c).
 *  Parameters  : c — char c
 *  Returns     : int
 */
int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + c - 'a';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }
  return -1;
}

/**
 *  Function    : to_hex_upper
 *  Description : Builds and returns a string for to hex upper.
 *  Parameters  : bytes — Span<const std::uint8_t> bytes
 *  Returns     : std::string
 */
std::string to_hex_upper(Span<const std::uint8_t> bytes) {
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (std::uint8_t b : bytes) {
    out.push_back(kHex[b >> 4]);
    out.push_back(kHex[b & 0x0F]);
  }
  return out;
}

/**
 *  Function    : from_hex
 *  Description : Returns success or an error from from hex.
 *  Parameters  : text — const std::string& text
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> from_hex(const std::string& text) {
  if (text.size() % 2 != 0) {
    return bad(0, "JER hex string must have even length");
  }
  std::vector<std::uint8_t> out;
  out.reserve(text.size() / 2);
  for (std::size_t i = 0; i < text.size(); i += 2) {
    const int hi = hex_nibble(text[i]);
    const int lo = hex_nibble(text[i + 1]);
    if (hi < 0 || lo < 0) {
      return bad(0, "invalid JER hex digit");
    }
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return out;
}

}  // namespace

/**
 *  Function    : encode_boolean
 *  Description : Computes encode boolean from (value).
 *  Parameters  : value — bool value
 *  Returns     : Value
 */
Value encode_boolean(bool value) {
  return Value::boolean(value);
}

/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from v.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(const Value& v) {
  if (v.kind != ValueKind::Bool) {
    return bad(0, "JER BOOLEAN expects JSON true/false");
  }
  return v.bool_value;
}

/**
 *  Function    : encode_null
 *  Description : Computes encode null from (none).
 *  Parameters  : none
 *  Returns     : Value
 */
Value encode_null() {
  return Value::null_value();
}

/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<void>
 */
Result<void> decode_null(const Value& v) {
  if (v.kind != ValueKind::Null) {
    return bad(0, "JER NULL expects JSON null");
  }
  return Result<void>::success();
}

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (value).
 *  Parameters  : value — std::int64_t value
 *  Returns     : Value
 */
Value encode_integer(std::int64_t value) {
  return Value::integer(value);
}

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (value).
 *  Parameters  : value — const BigInteger& value
 *  Returns     : Value
 */
Value encode_integer(const BigInteger& value) {
  return Value::big_integer(value);
}

/**
 *  Function    : decode_big_integer
 *  Description : Returns success or an error from decode big integer.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer(const Value& v) {
  if (v.kind != ValueKind::Number || v.number_is_real) {
    return bad(0, "JER INTEGER expects JSON integer number");
  }
  if (v.number_is_bigint) {
    return BigInteger::from_decimal(v.string_value);
  }
  return BigInteger::from_i64(v.number);
}

/**
 *  Function    : decode_integer
 *  Description : Returns success or an error from decode integer.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer(const Value& v) {
  auto big = decode_big_integer(v);
  if (!big.ok()) {
    return big.error();
  }
  auto n = big.value().as_i64();
  if (!n) {
    return bad(0, "JER INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *n;
}

/**
 *  Function    : encode_octet_string
 *  Description : Computes encode octet string from (value).
 *  Parameters  : value — Span<const std::uint8_t> value
 *  Returns     : Value
 */
Value encode_octet_string(Span<const std::uint8_t> value) {
  return Value::string(to_hex_upper(value));
}

/**
 *  Function    : decode_octet_string
 *  Description : Returns success or an error from decode octet string.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER OCTET STRING expects JSON string");
  }
  return from_hex(v.string_value);
}

/**
 *  Function    : encode_bit_string
 *  Description : Computes encode bit string from (bits, bit_length, fixed_size).
 *  Parameters  : bits — Span<const std::uint8_t> bits; bit_length — std::size_t bit_length; fixed_size — bool fixed_size
 *  Returns     : Value
 */
Value encode_bit_string(Span<const std::uint8_t> bits, std::size_t bit_length, bool fixed_size) {
  const std::size_t n_bytes = (bit_length + 7) / 8;
  std::vector<std::uint8_t> data(n_bytes, 0);
  for (std::size_t i = 0; i < bit_length; ++i) {
    const std::size_t bi = i / 8;
    const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (i % 8));
    const std::uint8_t b = bi < bits.size() ? bits[bi] : 0;
    if (b & mask) {
      data[bi] |= mask;
    }
  }
  // Clear unused trailing bits in last octet.
  if (bit_length % 8 != 0 && !data.empty()) {
    data.back() &= static_cast<std::uint8_t>(0xFFu << (8 - (bit_length % 8)));
  }
  const std::string hex = to_hex_upper(data);
  if (fixed_size) {
    return Value::string(hex);
  }
  Value obj = Value::make_object();
  obj.object.emplace_back("value", Value::string(hex));
  obj.object.emplace_back("length", Value::integer(static_cast<std::int64_t>(bit_length)));
  return obj;
}

/**
 *  Function    : decode_bit_string
 *  Description : Returns success or an error from decode bit string.
 *  Parameters  : v — const Value& v; fixed_size — bool fixed_size
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_string(const Value& v, bool fixed_size) {
  BitStringValue out;
  if (fixed_size) {
    if (v.kind != ValueKind::String) {
      return bad(0, "JER fixed BIT STRING expects JSON string");
    }
    auto bytes = from_hex(v.string_value);
    if (!bytes.ok()) {
      return bytes.error();
    }
    out.bits = std::move(bytes.value());
    out.bit_length = out.bits.size() * 8;
    return out;
  }
  if (v.kind != ValueKind::Object) {
    return bad(0, "JER BIT STRING expects JSON object with value/length");
  }
  const Value* hex_v = nullptr;
  const Value* len_v = nullptr;
  for (const auto& m : v.object) {
    if (m.first == "value") {
      hex_v = &m.second;
    } else if (m.first == "length") {
      len_v = &m.second;
    }
  }
  if (!hex_v || !len_v) {
    return bad(0, "JER BIT STRING missing value or length");
  }
  if (hex_v->kind != ValueKind::String || len_v->kind != ValueKind::Number) {
    return bad(0, "JER BIT STRING value/length type mismatch");
  }
  if (len_v->number < 0) {
    return bad(0, "JER BIT STRING length negative");
  }
  auto bytes = from_hex(hex_v->string_value);
  if (!bytes.ok()) {
    return bytes.error();
  }
  out.bits = std::move(bytes.value());
  const std::size_t req_len = static_cast<std::size_t>(len_v->number);
  if (req_len > out.bits.size() * 8) {
    return bad(0, "JER BIT STRING length exceeds byte buffer size");
  }
  out.bit_length = req_len;
  return out;
}

/**
 *  Function    : encode_utf8_string
 *  Description : Computes encode utf8 string from (value).
 *  Parameters  : value — const std::string& value
 *  Returns     : Value
 */
Value encode_utf8_string(const std::string& value) {
  return Value::string(value);
}

/**
 *  Function    : decode_utf8_string
 *  Description : Builds and returns a string for decode utf8 string.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER UTF8String expects JSON string");
  }
  return v.string_value;
}

/**
 *  Function    : encode_enumerated
 *  Description : Computes encode enumerated from (identifier).
 *  Parameters  : identifier — const std::string& identifier
 *  Returns     : Value
 */
Value encode_enumerated(const std::string& identifier) {
  return Value::string(identifier);
}

/**
 *  Function    : decode_enumerated
 *  Description : Builds and returns a string for decode enumerated.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER ENUMERATED expects JSON string");
  }
  return v.string_value;
}

/**
 *  Function    : encode_object_identifier
 *  Description : Computes encode object identifier from (arcs).
 *  Parameters  : arcs — Span<const std::uint64_t> arcs
 *  Returns     : Value
 */
Value encode_object_identifier(Span<const std::uint64_t> arcs) {
  std::string text;
  for (std::size_t i = 0; i < arcs.size(); ++i) {
    if (i != 0) {
      text.push_back('.');
    }
    text += std::to_string(arcs[i]);
  }
  return Value::string(std::move(text));
}

/**
 *  Function    : decode_object_identifier
 *  Description : Returns success or an error from decode object identifier.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::vector<std::uint64_t>>
 */
Result<std::vector<std::uint64_t>> decode_object_identifier(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER OBJECT IDENTIFIER expects JSON string");
  }
  const std::string& t = v.string_value;
  if (t.empty()) {
    return bad(0, "empty OBJECT IDENTIFIER");
  }
  std::vector<std::uint64_t> arcs;
  std::uint64_t cur = 0;
  bool have = false;
  for (char c : t) {
    if (c == '.') {
      if (!have) {
        return bad(0, "invalid OBJECT IDENTIFIER");
      }
      arcs.push_back(cur);
      cur = 0;
      have = false;
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return bad(0, "invalid OBJECT IDENTIFIER digit");
    }
    cur = cur * 10 + static_cast<std::uint64_t>(c - '0');
    have = true;
  }
  if (!have) {
    return bad(0, "invalid OBJECT IDENTIFIER");
  }
  arcs.push_back(cur);
  return arcs;
}

/**
 *  Function    : encode_real
 *  Description : Computes encode real from (value).
 *  Parameters  : value — double value
 *  Returns     : Value
 */
Value encode_real(double value) {
  if (std::isnan(value)) {
    return Value::string("NaN");
  }
  if (std::isinf(value)) {
    return Value::string(value > 0.0 ? "INF" : "-INF");
  }
  return Value::real_number(value);
}

/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<double>
 */
Result<double> decode_real(const Value& v) {
  if (v.kind == ValueKind::Number) {
    if (v.number_is_real) {
      return v.real;
    }
    return static_cast<double>(v.number);
  }
  if (v.kind == ValueKind::String) {
    if (v.string_value == "INF") {
      return std::numeric_limits<double>::infinity();
    }
    if (v.string_value == "-INF") {
      return -std::numeric_limits<double>::infinity();
    }
    if (v.string_value == "NaN") {
      return std::numeric_limits<double>::quiet_NaN();
    }
  }
  return bad(0, "JER REAL expects JSON number or INF/-INF/NaN string");
}

/**
 *  Function    : make_sequence
 *  Description : Computes make sequence from (std::vector<std::pair<std::string, members).
 *  Parameters  : std::vector<std::pair<std::string — std::vector<std::pair<std::string; members — Value>> members
 *  Returns     : Value
 */
Value make_sequence(std::vector<std::pair<std::string, Value>> members) {
  return Value::make_object(std::move(members));
}

/**
 *  Function    : find_member
 *  Description : Returns success or an error from find member.
 *  Parameters  : object — const Value& object; name — const std::string& name
 *  Returns     : Result<const Value*>
 */
Result<const Value*> find_member(const Value& object, const std::string& name) {
  if (object.kind != ValueKind::Object) {
    return bad(0, "JER SEQUENCE expects JSON object");
  }
  for (const auto& m : object.object) {
    if (m.first == name) {
      return &m.second;
    }
  }
  return bad(0, "JER SEQUENCE member not found: " + name);
}

/**
 *  Function    : encode_choice
 *  Description : Computes encode choice from (alternative, encoding).
 *  Parameters  : alternative — const std::string& alternative; encoding — Value encoding
 *  Returns     : Value
 */
Value encode_choice(const std::string& alternative, Value encoding) {
  Value obj = Value::make_object();
  obj.object.emplace_back(alternative, std::move(encoding));
  return obj;
}

/**
 *  Function    : decode_choice
 *  Description : Builds and returns a string for decode choice.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::pair<std::string, const Value*>>
 */
Result<std::pair<std::string, const Value*>> decode_choice(const Value& v) {
  if (v.kind != ValueKind::Object || v.object.size() != 1) {
    return bad(0, "JER CHOICE expects a single-property JSON object");
  }
  return std::make_pair(v.object[0].first, &v.object[0].second);
}

/**
 *  Function    : encode_sequence_of
 *  Description : Computes encode sequence of from (items).
 *  Parameters  : items — std::vector<Value> items
 *  Returns     : Value
 */
Value encode_sequence_of(std::vector<Value> items) {
  return Value::make_array(std::move(items));
}

/**
 *  Function    : decode_sequence_of
 *  Description : Returns success or an error from decode sequence of.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<const std::vector<Value>*>
 */
Result<const std::vector<Value>*> decode_sequence_of(const Value& v) {
  if (v.kind != ValueKind::Array) {
    return bad(0, "JER SEQUENCE OF expects JSON array");
  }
  return &v.array;
}

}  // namespace jer
}  // namespace asn1
