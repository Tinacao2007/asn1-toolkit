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

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

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

Element encode_boolean(const std::string& name, bool value) {
  Element el;
  el.name = name;
  el.children.push_back(encode_boolean_item(value));
  return el;
}

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

Element encode_null(const std::string& name) {
  Element el;
  el.name = name;
  return el;
}

Result<void> decode_null(const Element& /*el*/) {
  return Result<void>::success();
}

Element encode_integer(const std::string& name, std::int64_t value) {
  return encode_integer(name, BigInteger::from_i64(value));
}

Element encode_integer(const std::string& name, const BigInteger& value) {
  Element el;
  el.name = name;
  el.text = value.to_decimal();
  return el;
}

Result<BigInteger> decode_big_integer(const Element& el) {
  const std::string t = trim(el.text);
  if (t.empty()) {
    return bad(0, "XER INTEGER text empty");
  }
  return BigInteger::from_decimal(t);
}

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

Element encode_utf8_string(const std::string& name, const std::string& value) {
  Element el;
  el.name = name;
  el.text = value;
  return el;
}

Result<std::string> decode_utf8_string(const Element& el) {
  return el.text;
}

Element encode_enumerated(const std::string& name, const std::string& identifier) {
  Element el;
  el.name = name;
  Element child;
  child.name = identifier;
  el.children.push_back(std::move(child));
  return el;
}

Result<std::string> decode_enumerated(const Element& el) {
  if (el.children.size() != 1) {
    return bad(0, "XER ENUMERATED expects a single identifier child");
  }
  return el.children[0].name;
}

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

Element make_sequence(const std::string& name, std::vector<Element> members) {
  Element el;
  el.name = name;
  el.children = std::move(members);
  return el;
}

Result<const Element*> find_child(const Element& parent, const std::string& name) {
  for (const auto& c : parent.children) {
    if (c.name == name) {
      return &c;
    }
  }
  return bad(0, "XER SEQUENCE member not found: " + name);
}

Element encode_choice(const std::string& name, Element alternative) {
  Element el;
  el.name = name;
  el.children.push_back(std::move(alternative));
  return el;
}

Result<const Element*> decode_choice_alternative(const Element& el) {
  if (el.children.size() != 1) {
    return bad(0, "XER CHOICE expects exactly one alternative child");
  }
  return &el.children[0];
}

Element encode_sequence_of(const std::string& name, std::vector<Element> items) {
  return make_sequence(name, std::move(items));
}

Element encode_boolean_item(bool value) {
  Element el;
  el.name = value ? "true" : "false";
  return el;
}

Element encode_integer_item(std::int64_t value) {
  return encode_integer("INTEGER", value);
}

Element encode_integer_item(const BigInteger& value) {
  return encode_integer("INTEGER", value);
}

Element encode_null_item() {
  return encode_null("NULL");
}

}  // namespace xer
}  // namespace asn1
