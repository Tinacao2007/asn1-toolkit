/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/exer/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   E-XER encoding instructions and special cases.
**
** Specification: ITU-T X.693 — Extended XER (E-XER) and encoding
**                 instructions.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/exer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <algorithm>
#include <cctype>

namespace asn1 {
namespace exer {
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

constexpr char kBase64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 *  Function    : base64_index
 *  Description : Computes base64 index from (c).
 *  Parameters  : c — char c
 *  Returns     : int
 */
int base64_index(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return 26 + c - 'a';
  }
  if (c >= '0' && c <= '9') {
    return 52 + c - '0';
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
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

}  // namespace

/**
 *  Function    : set_attribute
 *  Description : Performs set attribute (definition).
 *  Parameters  : el — Element& el; name — const std::string& name; value — const std::string& value
 *  Returns     : void
 */
void set_attribute(Element& el, const std::string& name, const std::string& value) {
  for (auto& a : el.attributes) {
    if (a.name == name) {
      a.value = value;
      return;
    }
  }
  el.attributes.push_back(Attribute{name, value});
}

/**
 *  Function    : get_attribute
 *  Description : Builds and returns a string for get attribute.
 *  Parameters  : el — const Element& el; name — const std::string& name
 *  Returns     : Result<std::string>
 */
Result<std::string> get_attribute(const Element& el, const std::string& name) {
  for (const auto& a : el.attributes) {
    if (a.name == name) {
      return a.value;
    }
  }
  return bad(0, "EXTENDED-XER attribute not found: " + name);
}

void encode_attribute_string(Element& parent, const std::string& attr_name,
                             const std::string& value) {
  set_attribute(parent, attr_name, value);
}

void encode_attribute_integer(Element& parent, const std::string& attr_name,
                              std::int64_t value) {
  set_attribute(parent, attr_name, std::to_string(value));
}

void encode_attribute_integer(Element& parent, const std::string& attr_name,
                              const BigInteger& value) {
  set_attribute(parent, attr_name, value.to_decimal());
}

/**
 *  Function    : encode_attribute_boolean
 *  Description : Performs encode attribute boolean (definition).
 *  Parameters  : parent — Element& parent; attr_name — const std::string& attr_name; value — bool value
 *  Returns     : void
 */
void encode_attribute_boolean(Element& parent, const std::string& attr_name, bool value) {
  set_attribute(parent, attr_name, value ? "true" : "false");
}

/**
 *  Function    : decode_attribute_boolean
 *  Description : Returns a boolean result from el, attr_name.
 *  Parameters  : el — const Element& el; attr_name — const std::string& attr_name
 *  Returns     : Result<bool>
 */
Result<bool> decode_attribute_boolean(const Element& el, const std::string& attr_name) {
  auto v = get_attribute(el, attr_name);
  if (!v.ok()) {
    return v.error();
  }
  if (v.value() == "true") {
    return true;
  }
  if (v.value() == "false") {
    return false;
  }
  return bad(0, "EXTENDED-XER boolean attribute must be true or false");
}

/**
 *  Function    : encode_base64
 *  Description : Builds and returns a string for encode base64.
 *  Parameters  : value — Span<const std::uint8_t> value
 *  Returns     : std::string
 */
std::string encode_base64(Span<const std::uint8_t> value) {
  std::string out;
  out.reserve(((value.size() + 2) / 3) * 4);
  std::size_t i = 0;
  while (i + 2 < value.size()) {
    const std::uint32_t n = (static_cast<std::uint32_t>(value[i]) << 16) |
                            (static_cast<std::uint32_t>(value[i + 1]) << 8) |
                            static_cast<std::uint32_t>(value[i + 2]);
    out.push_back(kBase64[(n >> 18) & 63]);
    out.push_back(kBase64[(n >> 12) & 63]);
    out.push_back(kBase64[(n >> 6) & 63]);
    out.push_back(kBase64[n & 63]);
    i += 3;
  }
  const std::size_t rem = value.size() - i;
  if (rem == 1) {
    const std::uint32_t n = static_cast<std::uint32_t>(value[i]) << 16;
    out.push_back(kBase64[(n >> 18) & 63]);
    out.push_back(kBase64[(n >> 12) & 63]);
    out += "==";
  } else if (rem == 2) {
    const std::uint32_t n = (static_cast<std::uint32_t>(value[i]) << 16) |
                            (static_cast<std::uint32_t>(value[i + 1]) << 8);
    out.push_back(kBase64[(n >> 18) & 63]);
    out.push_back(kBase64[(n >> 12) & 63]);
    out.push_back(kBase64[(n >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}

/**
 *  Function    : decode_base64
 *  Description : Returns success or an error from decode base64.
 *  Parameters  : text — const std::string& text
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_base64(const std::string& text) {
  std::string t;
  t.reserve(text.size());
  for (char c : text) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      continue;
    }
    t.push_back(c);
  }
  if (t.size() % 4 != 0) {
    return bad(0, "invalid BASE64 length");
  }
  std::vector<std::uint8_t> out;
  out.reserve(t.size() / 4 * 3);
  for (std::size_t i = 0; i < t.size(); i += 4) {
    const int a = base64_index(t[i]);
    const int b = base64_index(t[i + 1]);
    if (a < 0 || b < 0) {
      return bad(0, "invalid BASE64 character");
    }
    const bool pad1 = t[i + 2] == '=';
    const bool pad2 = t[i + 3] == '=';
    const int c = pad1 ? 0 : base64_index(t[i + 2]);
    const int d = pad2 ? 0 : base64_index(t[i + 3]);
    if ((!pad1 && c < 0) || (!pad2 && d < 0)) {
      return bad(0, "invalid BASE64 character");
    }
    if (pad1 && !pad2) {
      return bad(0, "invalid BASE64 padding");
    }
    const std::uint32_t n = (static_cast<std::uint32_t>(a) << 18) |
                            (static_cast<std::uint32_t>(b) << 12) |
                            (static_cast<std::uint32_t>(c) << 6) |
                            static_cast<std::uint32_t>(d);
    out.push_back(static_cast<std::uint8_t>((n >> 16) & 0xFF));
    if (!pad1) {
      out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFF));
    }
    if (!pad2) {
      out.push_back(static_cast<std::uint8_t>(n & 0xFF));
    }
  }
  return out;
}

/**
 *  Function    : encode_octet_string_base64
 *  Description : Computes encode octet string base64 from (name, value).
 *  Parameters  : name — const std::string& name; value — Span<const std::uint8_t> value
 *  Returns     : Element
 */
Element encode_octet_string_base64(const std::string& name, Span<const std::uint8_t> value) {
  Element el;
  el.name = name;
  if (!value.empty()) {
    el.text = encode_base64(value);
  }
  return el;
}

/**
 *  Function    : decode_octet_string_base64
 *  Description : Returns success or an error from decode octet string base64.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string_base64(const Element& el) {
  if (el.text.empty()) {
    return std::vector<std::uint8_t>{};
  }
  return decode_base64(el.text);
}

/**
 *  Function    : encode_boolean_text
 *  Description : Computes encode boolean text from (name, value).
 *  Parameters  : name — const std::string& name; value — bool value
 *  Returns     : Element
 */
Element encode_boolean_text(const std::string& name, bool value) {
  Element el;
  el.name = name;
  el.text = value ? "true" : "false";
  return el;
}

/**
 *  Function    : decode_boolean_text
 *  Description : Returns a boolean result from el.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean_text(const Element& el) {
  const std::string t = trim(el.text);
  if (t == "true") {
    return true;
  }
  if (t == "false") {
    return false;
  }
  return bad(0, "EXTENDED-XER TEXT BOOLEAN must be true or false");
}

/**
 *  Function    : encode_enumerated_text
 *  Description : Computes encode enumerated text from (name, identifier).
 *  Parameters  : name — const std::string& name; identifier — const std::string& identifier
 *  Returns     : Element
 */
Element encode_enumerated_text(const std::string& name, const std::string& identifier) {
  Element el;
  el.name = name;
  el.text = identifier;
  return el;
}

/**
 *  Function    : decode_enumerated_text
 *  Description : Builds and returns a string for decode enumerated text.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated_text(const Element& el) {
  const std::string t = trim(el.text);
  if (t.empty()) {
    return bad(0, "EXTENDED-XER TEXT ENUMERATED empty");
  }
  return t;
}

/**
 *  Function    : encode_enumerated_number
 *  Description : Computes encode enumerated number from (name, value).
 *  Parameters  : name — const std::string& name; value — std::int64_t value
 *  Returns     : Element
 */
Element encode_enumerated_number(const std::string& name, std::int64_t value) {
  return xer::encode_integer(name, value);
}

/**
 *  Function    : decode_enumerated_number
 *  Description : Returns success or an error from decode enumerated number.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_enumerated_number(const Element& el) {
  return xer::decode_integer(el);
}

Element encode_list_of_strings(const std::string& name,
                               const std::vector<std::string>& items) {
  Element el;
  el.name = name;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i != 0) {
      el.text.push_back(' ');
    }
    el.text += items[i];
  }
  return el;
}

/**
 *  Function    : decode_list_of_strings
 *  Description : Builds and returns a string for decode list of strings.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::string>>
 */
Result<std::vector<std::string>> decode_list_of_strings(const Element& el) {
  std::vector<std::string> out;
  const std::string t = trim(el.text);
  if (t.empty()) {
    return out;
  }
  std::string cur;
  for (char c : t) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!cur.empty()) {
        out.push_back(std::move(cur));
        cur.clear();
      }
      continue;
    }
    cur.push_back(c);
  }
  if (!cur.empty()) {
    out.push_back(std::move(cur));
  }
  return out;
}

Element encode_list_of_integers(const std::string& name,
                                const std::vector<std::int64_t>& items) {
  Element el;
  el.name = name;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i != 0) {
      el.text.push_back(' ');
    }
    el.text += std::to_string(items[i]);
  }
  return el;
}

Element encode_list_of_integers(const std::string& name,
                                const std::vector<BigInteger>& items) {
  Element el;
  el.name = name;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i != 0) {
      el.text.push_back(' ');
    }
    el.text += items[i].to_decimal();
  }
  return el;
}

/**
 *  Function    : decode_list_of_integers
 *  Description : Returns success or an error from decode list of integers.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::int64_t>>
 */
Result<std::vector<std::int64_t>> decode_list_of_integers(const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<std::int64_t> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    Element tmp;
    tmp.name = "x";
    tmp.text = p;
    auto v = xer::decode_integer(tmp);
    if (!v.ok()) {
      return v.error();
    }
    out.push_back(v.value());
  }
  return out;
}

/**
 *  Function    : decode_list_of_big_integers
 *  Description : Returns success or an error from decode list of big integers.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<BigInteger>>
 */
Result<std::vector<BigInteger>> decode_list_of_big_integers(const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<BigInteger> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    auto v = BigInteger::from_decimal(p);
    if (!v.ok()) {
      return v.error();
    }
    out.push_back(std::move(v.value()));
  }
  return out;
}

Element encode_list_of_booleans(const std::string& name,
                                const std::vector<std::uint8_t>& items_as_0_1) {
  std::vector<std::string> toks;
  toks.reserve(items_as_0_1.size());
  for (std::uint8_t b : items_as_0_1) {
    toks.push_back(b ? "true" : "false");
  }
  return encode_list_of_strings(name, toks);
}

/**
 *  Function    : decode_list_of_booleans
 *  Description : Returns success or an error from decode list of booleans.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_list_of_booleans(const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<std::uint8_t> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    if (p == "true") {
      out.push_back(1);
    } else if (p == "false") {
      out.push_back(0);
    } else {
      return bad(0, "LIST BOOLEAN token must be true or false");
    }
  }
  return out;
}

Element encode_list_of_object_identifiers(
    const std::string& name, const std::vector<std::vector<std::uint64_t>>& items) {
  std::vector<std::string> toks;
  toks.reserve(items.size());
  for (const auto& arcs : items) {
    auto one = xer::encode_object_identifier("x", arcs);
    toks.push_back(std::move(one.text));
  }
  return encode_list_of_strings(name, toks);
}

Result<std::vector<std::vector<std::uint64_t>>> decode_list_of_object_identifiers(
    const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<std::vector<std::uint64_t>> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    Element tmp;
    tmp.name = "x";
    tmp.text = p;
    auto v = xer::decode_object_identifier(tmp);
    if (!v.ok()) {
      return v.error();
    }
    out.push_back(std::move(v.value()));
  }
  return out;
}

/**
 *  Function    : encode_list_of_reals
 *  Description : Computes encode list of reals from (name, items).
 *  Parameters  : name — const std::string& name; items — const std::vector<double>& items
 *  Returns     : Element
 */
Element encode_list_of_reals(const std::string& name, const std::vector<double>& items) {
  std::vector<std::string> toks;
  toks.reserve(items.size());
  for (double d : items) {
    auto one = xer::encode_real("x", d);
    if (!one.text.empty()) {
      toks.push_back(std::move(one.text));
    } else if (one.children.size() == 1) {
      toks.push_back(one.children[0].name);
    } else {
      toks.emplace_back("0");
    }
  }
  return encode_list_of_strings(name, toks);
}

/**
 *  Function    : decode_list_of_reals
 *  Description : Returns success or an error from decode list of reals.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<double>>
 */
Result<std::vector<double>> decode_list_of_reals(const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<double> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    Element tmp;
    tmp.name = "x";
    if (p == "PLUS-INFINITY" || p == "MINUS-INFINITY" || p == "NOT-A-NUMBER") {
      Element child;
      child.name = p;
      tmp.children.push_back(std::move(child));
    } else {
      tmp.text = p;
    }
    auto v = xer::decode_real(tmp);
    if (!v.ok()) {
      return v.error();
    }
    out.push_back(v.value());
  }
  return out;
}

Element encode_list_of_octet_strings(const std::string& name,
                                     const std::vector<std::vector<std::uint8_t>>& items) {
  std::vector<std::string> toks;
  toks.reserve(items.size());
  for (const auto& oct : items) {
    auto one = xer::encode_octet_string("x", oct);
    toks.push_back(std::move(one.text));
  }
  return encode_list_of_strings(name, toks);
}

Result<std::vector<std::vector<std::uint8_t>>> decode_list_of_octet_strings(
    const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<std::vector<std::uint8_t>> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    Element tmp;
    tmp.name = "x";
    tmp.text = p;
    auto v = xer::decode_octet_string(tmp);
    if (!v.ok()) {
      return v.error();
    }
    out.push_back(std::move(v.value()));
  }
  return out;
}

Element encode_list_of_bit_strings(const std::string& name,
                                   const std::vector<xer::BitStringValue>& items) {
  std::vector<std::string> toks;
  toks.reserve(items.size());
  for (const auto& bs : items) {
    auto one = xer::encode_bit_string("x", bs.bits, bs.bit_length);
    toks.push_back(std::move(one.text));
  }
  return encode_list_of_strings(name, toks);
}

/**
 *  Function    : decode_list_of_bit_strings
 *  Description : Returns success or an error from decode list of bit strings.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<xer::BitStringValue>>
 */
Result<std::vector<xer::BitStringValue>> decode_list_of_bit_strings(const Element& el) {
  auto parts = decode_list_of_strings(el);
  if (!parts.ok()) {
    return parts.error();
  }
  std::vector<xer::BitStringValue> out;
  out.reserve(parts.value().size());
  for (const auto& p : parts.value()) {
    Element tmp;
    tmp.name = "x";
    tmp.text = p;
    auto v = xer::decode_bit_string(tmp);
    if (!v.ok()) {
      return v.error();
    }
    out.push_back(std::move(v.value()));
  }
  return out;
}

/**
 *  Function    : append_untagged
 *  Description : Performs append untagged (definition).
 *  Parameters  : parent — Element& parent; fragment — Element fragment
 *  Returns     : void
 */
void append_untagged(Element& parent, Element fragment) {
  for (auto& a : fragment.attributes) {
    set_attribute(parent, a.name, a.value);
  }
  if (!fragment.text.empty()) {
    parent.text += fragment.text;
  }
  for (auto& c : fragment.children) {
    parent.children.push_back(std::move(c));
  }
}

/**
 *  Function    : set_name
 *  Description : Performs set name (definition).
 *  Parameters  : el — Element& el; name — const std::string& name
 *  Returns     : void
 */
void set_name(Element& el, const std::string& name) {
  el.name = name;
}

/**
 *  Function    : set_nil
 *  Description : Performs set nil (definition).
 *  Parameters  : el — Element& el; is_nil — bool is_nil
 *  Returns     : void
 */
void set_nil(Element& el, bool is_nil) {
  if (is_nil) {
    set_attribute(el, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    set_attribute(el, "xsi:nil", "true");
    el.text.clear();
    el.children.clear();
  } else {
    // Remove xsi:nil if present.
    el.attributes.erase(
        std::remove_if(el.attributes.begin(), el.attributes.end(),
                       [](const Attribute& a) { return a.name == "xsi:nil"; }),
        el.attributes.end());
  }
}

/**
 *  Function    : is_nil
 *  Description : Returns whether nil holds for the given inputs.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
Result<bool> is_nil(const Element& el) {
  auto v = get_attribute(el, "xsi:nil");
  if (!v.ok()) {
    return false;
  }
  return v.value() == "true" || v.value() == "1";
}

}  // namespace exer
}  // namespace asn1
