/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/xer/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   XER mapping between ASN.1 values and XML elements.
**
** Specification: ITU-T X.693 — Basic XML Encoding Rules (XER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/xer/codec.hpp>
#include <asn1/common/float_conv.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cctype>
#include <cstdio>
#include <limits>

namespace asn1 {
namespace xer {
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
 *  Function    : trim
 *  Description : Builds and returns a string for trim.
 *  Parameters  : s — const std::string& s
 *  Returns     : std::string
 */
std::string trim(const std::string& s) {
  std::size_t a = 0;
  while (a < s.size() &&
         (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) {
    ++a;
  }
  std::size_t b = s.size();
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                   s[b - 1] == '\r')) {
    --b;
  }
  return s.substr(a, b - a);
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

}  // namespace

/**
 *  Function    : encode_boolean
 *  Description : Computes encode boolean from (name, value).
 *  Parameters  : name — const std::string& name; value — bool value
 *  Returns     : Element
 */
Element encode_boolean(const std::string& name, bool value) {
  Element el;
  el.name = name;
  el.children.push_back(encode_boolean_item(value));
  return el;
}

/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from el.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(const Element& el) {
  if (el.children.size() != 1) {
    return bad(0, "XER BOOLEAN expects a single <true/> or <false/> child");
  }
  const auto& c = el.children[0].name;
  if (c == "true") {
    return true;
  }
  if (c == "false") {
    return false;
  }
  return bad(0, "XER BOOLEAN child must be true or false");
}

/**
 *  Function    : encode_null
 *  Description : Computes encode null from (name).
 *  Parameters  : name — const std::string& name
 *  Returns     : Element
 */
Element encode_null(const std::string& name) {
  Element el;
  el.name = name;
  return el;
}

/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<void>
 */
Result<void> decode_null(const Element& /*el*/) {
  return Result<void>::success();
}

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (name, value).
 *  Parameters  : name — const std::string& name; value — std::int64_t value
 *  Returns     : Element
 */
Element encode_integer(const std::string& name, std::int64_t value) {
  return encode_integer(name, BigInteger::from_i64(value));
}

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (name, value).
 *  Parameters  : name — const std::string& name; value — const BigInteger& value
 *  Returns     : Element
 */
Element encode_integer(const std::string& name, const BigInteger& value) {
  Element el;
  el.name = name;
  el.text = value.to_decimal();
  return el;
}

/**
 *  Function    : decode_big_integer
 *  Description : Returns success or an error from decode big integer.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer(const Element& el) {
  const std::string t = trim(el.text);
  if (t.empty()) {
    return bad(0, "XER INTEGER text empty");
  }
  return BigInteger::from_decimal(t);
}

/**
 *  Function    : decode_integer
 *  Description : Returns success or an error from decode integer.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer(const Element& el) {
  auto big = decode_big_integer(el);
  if (!big.ok()) {
    return big.error();
  }
  auto v = big.value().as_i64();
  if (!v) {
    return bad(0, "XER INTEGER wider than 64 bits; use decode_big_integer");
  }
  return *v;
}

/**
 *  Function    : encode_octet_string
 *  Description : Computes encode octet string from (name, value).
 *  Parameters  : name — const std::string& name; value — Span<const std::uint8_t> value
 *  Returns     : Element
 */
Element encode_octet_string(const std::string& name, Span<const std::uint8_t> value) {
  Element el;
  el.name = name;
  if (!value.empty()) {
    static const char* kHex = "0123456789ABCDEF";
    el.text.reserve(value.size() * 2);
    for (std::uint8_t b : value) {
      el.text.push_back(kHex[b >> 4]);
      el.text.push_back(kHex[b & 0x0F]);
    }
  }
  return el;
}

/**
 *  Function    : decode_octet_string
 *  Description : Returns success or an error from decode octet string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string(const Element& el) {
  std::string t = trim(el.text);
  if (t.empty()) {
    return std::vector<std::uint8_t>{};
  }
  if (t.size() % 2 != 0) {
    t.insert(t.begin(), '0');
  }
  std::vector<std::uint8_t> out;
  out.reserve(t.size() / 2);
  for (std::size_t i = 0; i < t.size(); i += 2) {
    const int hi = hex_nibble(t[i]);
    const int lo = hex_nibble(t[i + 1]);
    if (hi < 0 || lo < 0) {
      return bad(0, "invalid XER OCTET STRING hex");
    }
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return out;
}

Element encode_bit_string(const std::string& name, Span<const std::uint8_t> bits,
                          std::size_t bit_length) {
  Element el;
  el.name = name;
  if (bit_length == 0) {
    return el;
  }
  el.text.reserve(bit_length);
  for (std::size_t i = 0; i < bit_length; ++i) {
    const std::size_t byte_i = i / 8;
    const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (i % 8));
    const std::uint8_t b = byte_i < bits.size() ? bits[byte_i] : 0;
    el.text.push_back((b & mask) ? '1' : '0');
  }
  return el;
}

/**
 *  Function    : decode_bit_string
 *  Description : Returns success or an error from decode bit string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_string(const Element& el) {
  BitStringValue out;
  const std::string t = trim(el.text);
  if (t.empty()) {
    return out;
  }
  out.bit_length = t.size();
  const std::size_t n_bytes = (out.bit_length + 7) / 8;
  out.bits.assign(n_bytes, 0);
  for (std::size_t i = 0; i < t.size(); ++i) {
    if (t[i] != '0' && t[i] != '1') {
      return bad(0, "invalid XER BIT STRING binary digit");
    }
    if (t[i] == '1') {
      out.bits[i / 8] |= static_cast<std::uint8_t>(0x80u >> (i % 8));
    }
  }
  return out;
}

/**
 *  Function    : encode_utf8_string
 *  Description : Computes encode utf8 string from (name, value).
 *  Parameters  : name — const std::string& name; value — const std::string& value
 *  Returns     : Element
 */
Element encode_utf8_string(const std::string& name, const std::string& value) {
  Element el;
  el.name = name;
  el.text = value;
  return el;
}

/**
 *  Function    : decode_utf8_string
 *  Description : Builds and returns a string for decode utf8 string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string(const Element& el) {
  return el.text;
}

/**
 *  Function    : encode_enumerated
 *  Description : Computes encode enumerated from (name, identifier).
 *  Parameters  : name — const std::string& name; identifier — const std::string& identifier
 *  Returns     : Element
 */
Element encode_enumerated(const std::string& name, const std::string& identifier) {
  Element el;
  el.name = name;
  Element child;
  child.name = identifier;
  el.children.push_back(std::move(child));
  return el;
}

/**
 *  Function    : decode_enumerated
 *  Description : Builds and returns a string for decode enumerated.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated(const Element& el) {
  if (el.children.size() != 1) {
    return bad(0, "XER ENUMERATED expects a single identifier child");
  }
  return el.children[0].name;
}

/**
 *  Function    : encode_object_identifier
 *  Description : Computes encode object identifier from (name, arcs).
 *  Parameters  : name — const std::string& name; arcs — Span<const std::uint64_t> arcs
 *  Returns     : Element
 */
Element encode_object_identifier(const std::string& name, Span<const std::uint64_t> arcs) {
  Element el;
  el.name = name;
  for (std::size_t i = 0; i < arcs.size(); ++i) {
    if (i != 0) {
      el.text.push_back('.');
    }
    el.text += std::to_string(arcs[i]);
  }
  return el;
}

/**
 *  Function    : decode_object_identifier
 *  Description : Returns success or an error from decode object identifier.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint64_t>>
 */
Result<std::vector<std::uint64_t>> decode_object_identifier(const Element& el) {
  const std::string t = trim(el.text);
  if (t.empty()) {
    return bad(0, "Expected an OBJECT IDENTIFIER, but got ''.");
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
 *  Description : Computes encode real from (name, value).
 *  Parameters  : name — const std::string& name; value — double value
 *  Returns     : Element
 */
Element encode_real(const std::string& name, double value) {
  Element el;
  el.name = name;
  if (std::isnan(value)) {
    Element child;
    child.name = "NOT-A-NUMBER";
    el.children.push_back(std::move(child));
    return el;
  }
  if (std::isinf(value)) {
    Element child;
    child.name = value > 0.0 ? "PLUS-INFINITY" : "MINUS-INFINITY";
    el.children.push_back(std::move(child));
    return el;
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", value);
  el.text = buf;
  return el;
}

/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<double>
 */
Result<double> decode_real(const Element& el) {
  if (el.children.size() == 1 && el.text.empty()) {
    const std::string& n = el.children[0].name;
    if (n == "PLUS-INFINITY") {
      return std::numeric_limits<double>::infinity();
    }
    if (n == "MINUS-INFINITY") {
      return -std::numeric_limits<double>::infinity();
    }
    if (n == "NOT-A-NUMBER") {
      return std::numeric_limits<double>::quiet_NaN();
    }
    return bad(0, "unknown XER REAL special value");
  }
  const std::string t = trim(el.text);
  if (t.empty()) {
    return bad(0, "XER REAL expects numeric text or a special child");
  }
  double v = 0.0;
  if (!parse_double_c_locale(t, v)) {
    return bad(0, "invalid XER REAL");
  }
  return v;
}

/**
 *  Function    : make_sequence
 *  Description : Computes make sequence from (name, members).
 *  Parameters  : name — const std::string& name; members — std::vector<Element> members
 *  Returns     : Element
 */
Element make_sequence(const std::string& name, std::vector<Element> members) {
  Element el;
  el.name = name;
  el.children = std::move(members);
  return el;
}

/**
 *  Function    : find_child
 *  Description : Returns success or an error from find child.
 *  Parameters  : parent — const Element& parent; name — const std::string& name
 *  Returns     : Result<const Element*>
 */
Result<const Element*> find_child(const Element& parent, const std::string& name) {
  for (const auto& c : parent.children) {
    if (c.name == name) {
      return &c;
    }
  }
  return bad(0, "XER SEQUENCE member not found: " + name);
}

/**
 *  Function    : encode_choice
 *  Description : Computes encode choice from (name, alternative).
 *  Parameters  : name — const std::string& name; alternative — Element alternative
 *  Returns     : Element
 */
Element encode_choice(const std::string& name, Element alternative) {
  Element el;
  el.name = name;
  el.children.push_back(std::move(alternative));
  return el;
}

/**
 *  Function    : decode_choice_alternative
 *  Description : Returns success or an error from decode choice alternative.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<const Element*>
 */
Result<const Element*> decode_choice_alternative(const Element& el) {
  if (el.children.size() != 1) {
    return bad(0, "XER CHOICE expects exactly one alternative child");
  }
  return &el.children[0];
}

/**
 *  Function    : encode_sequence_of
 *  Description : Computes encode sequence of from (name, items).
 *  Parameters  : name — const std::string& name; items — std::vector<Element> items
 *  Returns     : Element
 */
Element encode_sequence_of(const std::string& name, std::vector<Element> items) {
  return make_sequence(name, std::move(items));
}

/**
 *  Function    : encode_boolean_item
 *  Description : Computes encode boolean item from (value).
 *  Parameters  : value — bool value
 *  Returns     : Element
 */
Element encode_boolean_item(bool value) {
  Element el;
  el.name = value ? "true" : "false";
  return el;
}

/**
 *  Function    : encode_integer_item
 *  Description : Computes encode integer item from (value).
 *  Parameters  : value — std::int64_t value
 *  Returns     : Element
 */
Element encode_integer_item(std::int64_t value) {
  return encode_integer("INTEGER", value);
}

/**
 *  Function    : encode_integer_item
 *  Description : Computes encode integer item from (value).
 *  Parameters  : value — const BigInteger& value
 *  Returns     : Element
 */
Element encode_integer_item(const BigInteger& value) {
  return encode_integer("INTEGER", value);
}

/**
 *  Function    : encode_null_item
 *  Description : Computes encode null item from (none).
 *  Parameters  : none
 *  Returns     : Element
 */
Element encode_null_item() {
  return encode_null("NULL");
}

}  // namespace xer
}  // namespace asn1
