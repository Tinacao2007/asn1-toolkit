/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/cxer/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   CXER canonicalization rules on top of XER.
**
** Specification: ITU-T X.693 — Canonical XML Encoding Rules (CXER).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/cxer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <algorithm>
#include <cctype>

namespace asn1 {
namespace cxer {
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
 *  Function    : non_canonical
 *  Description : Computes non canonical from (offset, message).
 *  Parameters  : offset — std::size_t offset; message — std::string message
 *  Returns     : Error
 */
Error non_canonical(std::size_t offset, std::string message) {
  return make_error(Error::Code::NonCanonical, offset, std::move(message));
}

/**
 *  Function    : append_escaped_text_cxer
 *  Description : Performs append escaped text cxer (definition).
 *  Parameters  : out — std::string& out; text — const std::string& text
 *  Returns     : void
 */
void append_escaped_text_cxer(std::string& out, const std::string& text) {
  for (unsigned char b : text) {
    if (b == '&') {
      out += "&amp;";
    } else if (b == '<') {
      out += "&lt;";
    } else {
      // CXER forbids character-reference escapes for non-ASCII (X.693 9.1.3);
      // emit raw UTF-8 octets.
      out.push_back(static_cast<char>(b));
    }
  }
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
  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }
  return -1;
}

/**
 *  Function    : is_canonical_integer_text
 *  Description : Returns whether canonical integer text holds for the given inputs.
 *  Parameters  : t — const std::string& t
 *  Returns     : bool
 */
bool is_canonical_integer_text(const std::string& t) {
  if (t.empty()) {
    return false;
  }
  std::size_t i = 0;
  if (t[0] == '-') {
    i = 1;
    if (i >= t.size()) {
      return false;
    }
  }
  // No '+', no leading zeros (except a lone "0" / "-0" → we forbid "-0", allow "0").
  if (t[i] == '0') {
    return t.size() == i + 1;  // exactly "0"
  }
  for (; i < t.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(t[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

/**
 *  Function    : write_element
 *  Description : Performs write element (definition).
 *  Parameters  : out — std::string& out; el — const Element& el
 *  Returns     : void
 */
void write_element(std::string& out, const Element& el) {
  out.push_back('<');
  out += el.name;
  for (const auto& a : el.attributes) {
    out.push_back(' ');
    out += a.name;
    out += "=\"";
    // CXER attribute escaping: & < "
    for (unsigned char b : a.value) {
      if (b == '&') {
        out += "&amp;";
      } else if (b == '<') {
        out += "&lt;";
      } else if (b == '"') {
        out += "&quot;";
      } else {
        out.push_back(static_cast<char>(b));
      }
    }
    out.push_back('"');
  }
  if (el.children.empty() && el.text.empty()) {
    out += "/>";
    return;
  }
  out.push_back('>');
  if (!el.text.empty()) {
    append_escaped_text_cxer(out, el.text);
  }
  for (const auto& child : el.children) {
    cxer::write_element(out, child);
  }
  out += "</";
  out += el.name;
  out.push_back('>');
}

/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : el — const Element& el
 *  Returns     : std::string
 */
std::string to_string(const Element& el) {
  std::string out;
  cxer::write_element(out, el);
  return out;
}

/**
 *  Function    : parse_document
 *  Description : Returns success or an error from parse document.
 *  Parameters  : xml — const std::string& xml
 *  Returns     : Result<Element>
 */
Result<Element> parse_document(const std::string& xml) {
  auto el = xer::parse_document(xml);
  if (!el.ok()) {
    return el.error();
  }
  if (cxer::to_string(el.value()) != xml) {
    return non_canonical(0, "not a CANONICAL-XER encoding");
  }
  return el;
}

/**
 *  Function    : encode_boolean
 *  Description : Computes encode boolean from (name, value).
 *  Parameters  : name — const std::string& name; value — bool value
 *  Returns     : Element
 */
Element encode_boolean(const std::string& name, bool value) {
  return xer::encode_boolean(name, value);
}

/**
 *  Function    : decode_boolean
 *  Description : Returns a boolean result from el.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<bool>
 */
Result<bool> decode_boolean(const Element& el) {
  return xer::decode_boolean(el);
}

/**
 *  Function    : encode_null
 *  Description : Computes encode null from (name).
 *  Parameters  : name — const std::string& name
 *  Returns     : Element
 */
Element encode_null(const std::string& name) {
  return xer::encode_null(name);
}

/**
 *  Function    : decode_null
 *  Description : Returns success or an error from decode null.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<void>
 */
Result<void> decode_null(const Element& el) {
  if (!el.text.empty() || !el.children.empty()) {
    return non_canonical(0, "CXER NULL must be an empty element");
  }
  return Result<void>::success();
}

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (name, value).
 *  Parameters  : name — const std::string& name; value — std::int64_t value
 *  Returns     : Element
 */
Element encode_integer(const std::string& name, std::int64_t value) {
  return xer::encode_integer(name, value);
}

/**
 *  Function    : encode_integer
 *  Description : Computes encode integer from (name, value).
 *  Parameters  : name — const std::string& name; value — const BigInteger& value
 *  Returns     : Element
 */
Element encode_integer(const std::string& name, const BigInteger& value) {
  return xer::encode_integer(name, value);
}

/**
 *  Function    : decode_integer
 *  Description : Returns success or an error from decode integer.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::int64_t>
 */
Result<std::int64_t> decode_integer(const Element& el) {
  // No surrounding whitespace; text must already be canonical SignedNumber.
  if (!is_canonical_integer_text(el.text)) {
    return non_canonical(0, "CXER INTEGER must be a canonical SignedNumber");
  }
  return xer::decode_integer(el);
}

/**
 *  Function    : decode_big_integer
 *  Description : Returns success or an error from decode big integer.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<BigInteger>
 */
Result<BigInteger> decode_big_integer(const Element& el) {
  if (!is_canonical_integer_text(el.text)) {
    return non_canonical(0, "CXER INTEGER must be a canonical SignedNumber");
  }
  return xer::decode_big_integer(el);
}

/**
 *  Function    : encode_real
 *  Description : Computes encode real from (name, value).
 *  Parameters  : name — const std::string& name; value — double value
 *  Returns     : Element
 */
Element encode_real(const std::string& name, double value) {
  return xer::encode_real(name, value);
}

/**
 *  Function    : decode_real
 *  Description : Returns success or an error from decode real.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<double>
 */
Result<double> decode_real(const Element& el) {
  return xer::decode_real(el);
}

/**
 *  Function    : encode_octet_string
 *  Description : Computes encode octet string from (name, value).
 *  Parameters  : name — const std::string& name; value — Span<const std::uint8_t> value
 *  Returns     : Element
 */
Element encode_octet_string(const std::string& name, Span<const std::uint8_t> value) {
  return xer::encode_octet_string(name, value);
}

/**
 *  Function    : decode_octet_string
 *  Description : Returns success or an error from decode octet string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string(const Element& el) {
  const std::string& t = el.text;
  if (t.empty()) {
    if (!el.children.empty()) {
      return non_canonical(0, "CXER OCTET STRING empty form must have no children");
    }
    return std::vector<std::uint8_t>{};
  }
  if (t.size() % 2 != 0) {
    return non_canonical(0, "CXER OCTET STRING hex must have even length");
  }
  for (char c : t) {
    if (hex_nibble(c) < 0) {
      // Reject lowercase and non-hex.
      return non_canonical(0, "CXER OCTET STRING hex must be uppercase [0-9A-F]");
    }
  }
  std::vector<std::uint8_t> out;
  out.reserve(t.size() / 2);
  for (std::size_t i = 0; i < t.size(); i += 2) {
    out.push_back(static_cast<std::uint8_t>((hex_nibble(t[i]) << 4) | hex_nibble(t[i + 1])));
  }
  return out;
}

Element encode_bit_string(const std::string& name, Span<const std::uint8_t> bits,
                          std::size_t bit_length) {
  return xer::encode_bit_string(name, bits, bit_length);
}

/**
 *  Function    : decode_bit_string
 *  Description : Returns success or an error from decode bit string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<BitStringValue>
 */
Result<BitStringValue> decode_bit_string(const Element& el) {
  for (char c : el.text) {
    if (c != '0' && c != '1') {
      return non_canonical(0, "CXER BIT STRING must be an xmlbstring of 0/1 only");
    }
  }
  return xer::decode_bit_string(el);
}

/**
 *  Function    : encode_utf8_string
 *  Description : Computes encode utf8 string from (name, value).
 *  Parameters  : name — const std::string& name; value — const std::string& value
 *  Returns     : Element
 */
Element encode_utf8_string(const std::string& name, const std::string& value) {
  return xer::encode_utf8_string(name, value);
}

/**
 *  Function    : decode_utf8_string
 *  Description : Builds and returns a string for decode utf8 string.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_utf8_string(const Element& el) {
  return xer::decode_utf8_string(el);
}

/**
 *  Function    : encode_enumerated
 *  Description : Computes encode enumerated from (name, identifier).
 *  Parameters  : name — const std::string& name; identifier — const std::string& identifier
 *  Returns     : Element
 */
Element encode_enumerated(const std::string& name, const std::string& identifier) {
  return xer::encode_enumerated(name, identifier);
}

/**
 *  Function    : decode_enumerated
 *  Description : Builds and returns a string for decode enumerated.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::string>
 */
Result<std::string> decode_enumerated(const Element& el) {
  return xer::decode_enumerated(el);
}

/**
 *  Function    : encode_object_identifier
 *  Description : Computes encode object identifier from (name, arcs).
 *  Parameters  : name — const std::string& name; arcs — Span<const std::uint64_t> arcs
 *  Returns     : Element
 */
Element encode_object_identifier(const std::string& name, Span<const std::uint64_t> arcs) {
  return xer::encode_object_identifier(name, arcs);
}

/**
 *  Function    : decode_object_identifier
 *  Description : Returns success or an error from decode object identifier.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<std::vector<std::uint64_t>>
 */
Result<std::vector<std::uint64_t>> decode_object_identifier(const Element& el) {
  // NumberForm only — already what xer encodes; reject empty.
  if (el.text.empty()) {
    return bad(0, "Expected an OBJECT IDENTIFIER, but got ''.");
  }
  for (char c : el.text) {
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) {
      return non_canonical(0, "CXER OBJECT IDENTIFIER must use NumberForm");
    }
  }
  return xer::decode_object_identifier(el);
}

/**
 *  Function    : make_sequence
 *  Description : Computes make sequence from (name, members).
 *  Parameters  : name — const std::string& name; members — std::vector<Element> members
 *  Returns     : Element
 */
Element make_sequence(const std::string& name, std::vector<Element> members) {
  return xer::make_sequence(name, std::move(members));
}

/**
 *  Function    : find_child
 *  Description : Returns success or an error from find child.
 *  Parameters  : parent — const Element& parent; name — const std::string& name
 *  Returns     : Result<const Element*>
 */
Result<const Element*> find_child(const Element& parent, const std::string& name) {
  return xer::find_child(parent, name);
}

/**
 *  Function    : encode_choice
 *  Description : Computes encode choice from (name, alternative).
 *  Parameters  : name — const std::string& name; alternative — Element alternative
 *  Returns     : Element
 */
Element encode_choice(const std::string& name, Element alternative) {
  return xer::encode_choice(name, std::move(alternative));
}

/**
 *  Function    : decode_choice_alternative
 *  Description : Returns success or an error from decode choice alternative.
 *  Parameters  : el — const Element& el
 *  Returns     : Result<const Element*>
 */
Result<const Element*> decode_choice_alternative(const Element& el) {
  return xer::decode_choice_alternative(el);
}

/**
 *  Function    : encode_sequence_of
 *  Description : Computes encode sequence of from (name, items).
 *  Parameters  : name — const std::string& name; items — std::vector<Element> items
 *  Returns     : Element
 */
Element encode_sequence_of(const std::string& name, std::vector<Element> items) {
  return xer::encode_sequence_of(name, std::move(items));
}

/**
 *  Function    : encode_set_of
 *  Description : Computes encode set of from (name, items).
 *  Parameters  : name — const std::string& name; items — std::vector<Element> items
 *  Returns     : Element
 */
Element encode_set_of(const std::string& name, std::vector<Element> items) {
  std::sort(items.begin(), items.end(), [](const Element& a, const Element& b) {
    return cxer::to_string(a) < cxer::to_string(b);
  });
  return cxer::make_sequence(name, std::move(items));
}

/**
 *  Function    : require_set_of_order
 *  Description : Returns success or an error from require set of order.
 *  Parameters  : items — Span<const Element> items
 *  Returns     : Result<void>
 */
Result<void> require_set_of_order(Span<const Element> items) {
  for (std::size_t i = 1; i < items.size(); ++i) {
    if (cxer::to_string(items[i]) < cxer::to_string(items[i - 1])) {
      return non_canonical(0, "CXER SET OF components must be in ascending order");
    }
  }
  return Result<void>::success();
}

/**
 *  Function    : encode_boolean_item
 *  Description : Computes encode boolean item from (value).
 *  Parameters  : value — bool value
 *  Returns     : Element
 */
Element encode_boolean_item(bool value) {
  return xer::encode_boolean_item(value);
}

/**
 *  Function    : encode_integer_item
 *  Description : Computes encode integer item from (value).
 *  Parameters  : value — std::int64_t value
 *  Returns     : Element
 */
Element encode_integer_item(std::int64_t value) {
  return xer::encode_integer_item(value);
}

/**
 *  Function    : encode_integer_item
 *  Description : Computes encode integer item from (value).
 *  Parameters  : value — const BigInteger& value
 *  Returns     : Element
 */
Element encode_integer_item(const BigInteger& value) {
  return xer::encode_integer_item(value);
}

/**
 *  Function    : encode_null_item
 *  Description : Computes encode null item from (none).
 *  Parameters  : none
 *  Returns     : Element
 */
Element encode_null_item() {
  return xer::encode_null_item();
}

}  // namespace cxer
}  // namespace asn1
