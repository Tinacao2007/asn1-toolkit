#include <asn1/runtime/jer/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cctype>
#include <limits>

namespace asn1 {
namespace jer {
namespace {

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
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

Value encode_boolean(bool value) {
  return Value::boolean(value);
}

Result<bool> decode_boolean(const Value& v) {
  if (v.kind != ValueKind::Bool) {
    return bad(0, "JER BOOLEAN expects JSON true/false");
  }
  return v.bool_value;
}

Value encode_null() {
  return Value::null_value();
}

Result<void> decode_null(const Value& v) {
  if (v.kind != ValueKind::Null) {
    return bad(0, "JER NULL expects JSON null");
  }
  return Result<void>::success();
}

Value encode_integer(std::int64_t value) {
  return Value::integer(value);
}

Value encode_integer(const BigInteger& value) {
  return Value::big_integer(value);
}

Result<BigInteger> decode_big_integer(const Value& v) {
  if (v.kind != ValueKind::Number || v.number_is_real) {
    return bad(0, "JER INTEGER expects JSON integer number");
  }
  if (v.number_is_bigint) {
    return BigInteger::from_decimal(v.string_value);
  }
  return BigInteger::from_i64(v.number);
}

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

Value encode_octet_string(Span<const std::uint8_t> value) {
  return Value::string(to_hex_upper(value));
}

Result<std::vector<std::uint8_t>> decode_octet_string(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER OCTET STRING expects JSON string");
  }
  return from_hex(v.string_value);
}

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
  out.bit_length = static_cast<std::size_t>(len_v->number);
  return out;
}

Value encode_utf8_string(const std::string& value) {
  return Value::string(value);
}

Result<std::string> decode_utf8_string(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER UTF8String expects JSON string");
  }
  return v.string_value;
}

Value encode_enumerated(const std::string& identifier) {
  return Value::string(identifier);
}

Result<std::string> decode_enumerated(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER ENUMERATED expects JSON string");
  }
  return v.string_value;
}

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

Value encode_real(double value) {
  if (std::isnan(value)) {
    return Value::string("NaN");
  }
  if (std::isinf(value)) {
    return Value::string(value > 0.0 ? "INF" : "-INF");
  }
  return Value::real_number(value);
}

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

Value make_sequence(std::vector<std::pair<std::string, Value>> members) {
  return Value::make_object(std::move(members));
}

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

Value encode_choice(const std::string& alternative, Value encoding) {
  Value obj = Value::make_object();
  obj.object.emplace_back(alternative, std::move(encoding));
  return obj;
}

Result<std::pair<std::string, const Value*>> decode_choice(const Value& v) {
  if (v.kind != ValueKind::Object || v.object.size() != 1) {
    return bad(0, "JER CHOICE expects a single-property JSON object");
  }
  return std::make_pair(v.object[0].first, &v.object[0].second);
}

Value encode_sequence_of(std::vector<Value> items) {
  return Value::make_array(std::move(items));
}

Result<const std::vector<Value>*> decode_sequence_of(const Value& v) {
  if (v.kind != ValueKind::Array) {
    return bad(0, "JER SEQUENCE OF expects JSON array");
  }
  return &v.array;
}

}  // namespace jer
}  // namespace asn1
