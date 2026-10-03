#include <asn1/runtime/cxer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <algorithm>
#include <cctype>

namespace asn1 {
namespace cxer {
namespace {

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

Error non_canonical(std::size_t offset, std::string message) {
  return make_error(Error::Code::NonCanonical, offset, std::move(message));
}

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

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }
  return -1;
}

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

std::string to_string(const Element& el) {
  std::string out;
  cxer::write_element(out, el);
  return out;
}

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

Element encode_boolean(const std::string& name, bool value) {
  return xer::encode_boolean(name, value);
}

Result<bool> decode_boolean(const Element& el) {
  return xer::decode_boolean(el);
}

Element encode_null(const std::string& name) {
  return xer::encode_null(name);
}

Result<void> decode_null(const Element& el) {
  if (!el.text.empty() || !el.children.empty()) {
    return non_canonical(0, "CXER NULL must be an empty element");
  }
  return Result<void>::success();
}

Element encode_integer(const std::string& name, std::int64_t value) {
  return xer::encode_integer(name, value);
}

Element encode_integer(const std::string& name, const BigInteger& value) {
  return xer::encode_integer(name, value);
}

Result<std::int64_t> decode_integer(const Element& el) {
  // No surrounding whitespace; text must already be canonical SignedNumber.
  if (!is_canonical_integer_text(el.text)) {
    return non_canonical(0, "CXER INTEGER must be a canonical SignedNumber");
  }
  return xer::decode_integer(el);
}

Result<BigInteger> decode_big_integer(const Element& el) {
  if (!is_canonical_integer_text(el.text)) {
    return non_canonical(0, "CXER INTEGER must be a canonical SignedNumber");
  }
  return xer::decode_big_integer(el);
}

Element encode_real(const std::string& name, double value) {
  return xer::encode_real(name, value);
}

Result<double> decode_real(const Element& el) {
  return xer::decode_real(el);
}

Element encode_octet_string(const std::string& name, Span<const std::uint8_t> value) {
  return xer::encode_octet_string(name, value);
}

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

Result<BitStringValue> decode_bit_string(const Element& el) {
  for (char c : el.text) {
    if (c != '0' && c != '1') {
      return non_canonical(0, "CXER BIT STRING must be an xmlbstring of 0/1 only");
    }
  }
  return xer::decode_bit_string(el);
}

Element encode_utf8_string(const std::string& name, const std::string& value) {
  return xer::encode_utf8_string(name, value);
}

Result<std::string> decode_utf8_string(const Element& el) {
  return xer::decode_utf8_string(el);
}

Element encode_enumerated(const std::string& name, const std::string& identifier) {
  return xer::encode_enumerated(name, identifier);
}

Result<std::string> decode_enumerated(const Element& el) {
  return xer::decode_enumerated(el);
}

Element encode_object_identifier(const std::string& name, Span<const std::uint64_t> arcs) {
  return xer::encode_object_identifier(name, arcs);
}

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

Element make_sequence(const std::string& name, std::vector<Element> members) {
  return xer::make_sequence(name, std::move(members));
}

Result<const Element*> find_child(const Element& parent, const std::string& name) {
  return xer::find_child(parent, name);
}

Element encode_choice(const std::string& name, Element alternative) {
  return xer::encode_choice(name, std::move(alternative));
}

Result<const Element*> decode_choice_alternative(const Element& el) {
  return xer::decode_choice_alternative(el);
}

Element encode_sequence_of(const std::string& name, std::vector<Element> items) {
  return xer::encode_sequence_of(name, std::move(items));
}

Element encode_set_of(const std::string& name, std::vector<Element> items) {
  std::sort(items.begin(), items.end(), [](const Element& a, const Element& b) {
    return cxer::to_string(a) < cxer::to_string(b);
  });
  return cxer::make_sequence(name, std::move(items));
}

Result<void> require_set_of_order(Span<const Element> items) {
  for (std::size_t i = 1; i < items.size(); ++i) {
    if (cxer::to_string(items[i]) < cxer::to_string(items[i - 1])) {
      return non_canonical(0, "CXER SET OF components must be in ascending order");
    }
  }
  return Result<void>::success();
}

Element encode_boolean_item(bool value) {
  return xer::encode_boolean_item(value);
}

Element encode_integer_item(std::int64_t value) {
  return xer::encode_integer_item(value);
}

Element encode_integer_item(const BigInteger& value) {
  return xer::encode_integer_item(value);
}

Element encode_null_item() {
  return xer::encode_null_item();
}

}  // namespace cxer
}  // namespace asn1
