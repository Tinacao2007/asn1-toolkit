#include <asn1/runtime/jer/json.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cctype>
#include <cstdio>
#include <limits>

namespace asn1 {
namespace jer {
namespace {

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

void skip_ws(const std::string& s, std::size_t& i) {
  while (i < s.size() &&
         (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
    ++i;
  }
}

Result<void> expect_char(const std::string& s, std::size_t& i, char c) {
  if (i >= s.size() || s[i] != c) {
    return bad(i, std::string("expected '") + c + "'");
  }
  ++i;
  return Result<void>::success();
}

void append_escaped_string(std::string& out, const std::string& text) {
  out.push_back('"');
  for (unsigned char b : text) {
    switch (b) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (b < 0x20u) {
          static const char* kHex = "0123456789ABCDEF";
          out += "\\u00";
          out.push_back(kHex[b >> 4]);
          out.push_back(kHex[b & 0x0F]);
        } else {
          out.push_back(static_cast<char>(b));
        }
        break;
    }
  }
  out.push_back('"');
}

Result<std::string> parse_string(const std::string& s, std::size_t& i) {
  if (auto r = expect_char(s, i, '"'); !r.ok()) {
    return r.error();
  }
  std::string out;
  while (i < s.size()) {
    const char c = s[i++];
    if (c == '"') {
      return out;
    }
    if (c == '\\') {
      if (i >= s.size()) {
        return bad(i, "truncated JSON string escape");
      }
      const char e = s[i++];
      switch (e) {
        case '"':
        case '\\':
        case '/':
          out.push_back(e);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          if (i + 4 > s.size()) {
            return bad(i, "truncated JSON \\u escape");
          }
          std::uint32_t cp = 0;
          for (int n = 0; n < 4; ++n) {
            const char h = s[i++];
            cp <<= 4;
            if (h >= '0' && h <= '9') {
              cp |= static_cast<std::uint32_t>(h - '0');
            } else if (h >= 'a' && h <= 'f') {
              cp |= static_cast<std::uint32_t>(10 + h - 'a');
            } else if (h >= 'A' && h <= 'F') {
              cp |= static_cast<std::uint32_t>(10 + h - 'A');
            } else {
              return bad(i, "invalid JSON \\u hex");
            }
          }
          if (cp < 0x80u) {
            out.push_back(static_cast<char>(cp));
          } else if (cp < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
          } else {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
          }
          break;
        }
        default:
          return bad(i, "invalid JSON string escape");
      }
      continue;
    }
    out.push_back(c);
  }
  return bad(i, "unterminated JSON string");
}

Result<Value> parse_value(const std::string& s, std::size_t& i);

Result<Value> parse_number(const std::string& s, std::size_t& i) {
  const std::size_t start = i;
  if (i < s.size() && s[i] == '-') {
    ++i;
  }
  if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
    return bad(i, "invalid JSON number");
  }
  if (s[i] == '0') {
    ++i;
  } else {
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      ++i;
    }
  }
  bool is_real = false;
  if (i < s.size() && s[i] == '.') {
    is_real = true;
    ++i;
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
      return bad(start, "invalid JSON number fraction");
    }
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      ++i;
    }
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    is_real = true;
    ++i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
      ++i;
    }
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
      return bad(start, "invalid JSON number exponent");
    }
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      ++i;
    }
  }
  const std::string text = s.substr(start, i - start);
  if (is_real) {
    try {
      std::size_t idx = 0;
      const double v = std::stod(text, &idx);
      if (idx != text.size()) {
        return bad(start, "invalid JSON number");
      }
      return Value::real_number(v);
    } catch (...) {
      return bad(start, "invalid JSON number");
    }
  }
  auto big = BigInteger::from_decimal(text);
  if (!big.ok()) {
    return bad(start, "invalid JSON number");
  }
  return Value::big_integer(std::move(big.value()));
}

Result<Value> parse_array(const std::string& s, std::size_t& i) {
  if (auto r = expect_char(s, i, '['); !r.ok()) {
    return r.error();
  }
  skip_ws(s, i);
  Value out = Value::make_array();
  if (i < s.size() && s[i] == ']') {
    ++i;
    return out;
  }
  for (;;) {
    auto item = parse_value(s, i);
    if (!item.ok()) {
      return item.error();
    }
    out.array.push_back(std::move(item.value()));
    skip_ws(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      skip_ws(s, i);
      continue;
    }
    break;
  }
  if (auto r = expect_char(s, i, ']'); !r.ok()) {
    return r.error();
  }
  return out;
}

Result<Value> parse_object(const std::string& s, std::size_t& i) {
  if (auto r = expect_char(s, i, '{'); !r.ok()) {
    return r.error();
  }
  skip_ws(s, i);
  Value out = Value::make_object();
  if (i < s.size() && s[i] == '}') {
    ++i;
    return out;
  }
  for (;;) {
    auto key = parse_string(s, i);
    if (!key.ok()) {
      return key.error();
    }
    skip_ws(s, i);
    if (auto r = expect_char(s, i, ':'); !r.ok()) {
      return r.error();
    }
    skip_ws(s, i);
    auto val = parse_value(s, i);
    if (!val.ok()) {
      return val.error();
    }
    out.object.emplace_back(std::move(key.value()), std::move(val.value()));
    skip_ws(s, i);
    if (i < s.size() && s[i] == ',') {
      ++i;
      skip_ws(s, i);
      continue;
    }
    break;
  }
  if (auto r = expect_char(s, i, '}'); !r.ok()) {
    return r.error();
  }
  return out;
}

Result<Value> parse_value(const std::string& s, std::size_t& i) {
  skip_ws(s, i);
  if (i >= s.size()) {
    return bad(i, "unexpected end of JSON");
  }
  const char c = s[i];
  if (c == 'n') {
    if (s.compare(i, 4, "null") != 0) {
      return bad(i, "expected null");
    }
    i += 4;
    return Value::null_value();
  }
  if (c == 't') {
    if (s.compare(i, 4, "true") != 0) {
      return bad(i, "expected true");
    }
    i += 4;
    return Value::boolean(true);
  }
  if (c == 'f') {
    if (s.compare(i, 5, "false") != 0) {
      return bad(i, "expected false");
    }
    i += 5;
    return Value::boolean(false);
  }
  if (c == '"') {
    auto str = parse_string(s, i);
    if (!str.ok()) {
      return str.error();
    }
    return Value::string(std::move(str.value()));
  }
  if (c == '[') {
    return parse_array(s, i);
  }
  if (c == '{') {
    return parse_object(s, i);
  }
  if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
    return parse_number(s, i);
  }
  return bad(i, "invalid JSON value");
}

}  // namespace

void write_value(std::string& out, const Value& v) {
  switch (v.kind) {
    case ValueKind::Null:
      out += "null";
      break;
    case ValueKind::Bool:
      out += v.bool_value ? "true" : "false";
      break;
    case ValueKind::Number:
      if (v.number_is_real) {
        if (std::isnan(v.real) || std::isinf(v.real)) {
          // Should be encoded as JSON strings by encode_real; defensive fallback.
          out += "null";
        } else {
          char buf[64];
          std::snprintf(buf, sizeof(buf), "%.17g", v.real);
          out += buf;
        }
      } else if (v.number_is_bigint) {
        out += v.string_value;
      } else {
        out += std::to_string(v.number);
      }
      break;
    case ValueKind::String:
      append_escaped_string(out, v.string_value);
      break;
    case ValueKind::Array: {
      out.push_back('[');
      for (std::size_t i = 0; i < v.array.size(); ++i) {
        if (i != 0) {
          out.push_back(',');
        }
        write_value(out, v.array[i]);
      }
      out.push_back(']');
      break;
    }
    case ValueKind::Object: {
      out.push_back('{');
      for (std::size_t i = 0; i < v.object.size(); ++i) {
        if (i != 0) {
          out.push_back(',');
        }
        append_escaped_string(out, v.object[i].first);
        out.push_back(':');
        write_value(out, v.object[i].second);
      }
      out.push_back('}');
      break;
    }
  }
}

std::string to_string(const Value& v) {
  std::string out;
  write_value(out, v);
  return out;
}

Result<Value> parse_document(const std::string& json) {
  std::size_t i = 0;
  auto v = parse_value(json, i);
  if (!v.ok()) {
    return v.error();
  }
  skip_ws(json, i);
  if (i != json.size()) {
    return bad(i, "trailing data after JSON value");
  }
  return v;
}

}  // namespace jer
}  // namespace asn1
