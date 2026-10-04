/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/jer/json.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   JSON text lexer/parser for JER.
**
** Specification: ITU-T X.697 — JSON-related helpers for JER/JERI codecs.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/jer/json.hpp>
#include <asn1/common/float_conv.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cmath>
#include <cctype>
#include <cstdio>
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
 *  Function    : skip_ws
 *  Description : Performs skip ws (definition).
 *  Parameters  : s — const std::string& s; i — std::size_t& i
 *  Returns     : void
 */
void skip_ws(const std::string& s, std::size_t& i) {
  while (i < s.size() &&
         (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
    ++i;
  }
}

/**
 *  Function    : expect_char
 *  Description : Returns success or an error from expect char.
 *  Parameters  : s — const std::string& s; i — std::size_t& i; c — char c
 *  Returns     : Result<void>
 */
Result<void> expect_char(const std::string& s, std::size_t& i, char c) {
  if (i >= s.size() || s[i] != c) {
    return bad(i, std::string("expected '") + c + "'");
  }
  ++i;
  return Result<void>::success();
}

/**
 *  Function    : append_escaped_string
 *  Description : Performs append escaped string (definition).
 *  Parameters  : out — std::string& out; text — const std::string& text
 *  Returns     : void
 */
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

/**
 *  Function    : parse_string
 *  Description : Builds and returns a string for parse string.
 *  Parameters  : s — const std::string& s; i — std::size_t& i
 *  Returns     : Result<std::string>
 */
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

/**
 *  Function    : parse_number
 *  Description : Returns success or an error from parse number.
 *  Parameters  : s — const std::string& s; i — std::size_t& i
 *  Returns     : Result<Value>
 */
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
    double v = 0.0;
    if (!parse_double_c_locale(text, v)) {
      return bad(start, "invalid JSON number");
    }
    return Value::real_number(v);
  }
  auto big = BigInteger::from_decimal(text);
  if (!big.ok()) {
    return bad(start, "invalid JSON number");
  }
  return Value::big_integer(std::move(big.value()));
}

constexpr std::size_t kMaxJsonNestingDepth = 256;

Result<Value> parse_value(const std::string& s, std::size_t& i, std::size_t depth);

/**
 *  Function    : parse_array
 *  Description : Returns success or an error from parse array.
 *  Parameters  : s — const std::string& s; i — std::size_t& i; depth — std::size_t depth
 *  Returns     : Result<Value>
 */
Result<Value> parse_array(const std::string& s, std::size_t& i, std::size_t depth) {
  if (depth > kMaxJsonNestingDepth) {
    return bad(i, "JSON maximum nesting depth exceeded");
  }
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
    auto item = parse_value(s, i, depth + 1);
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

/**
 *  Function    : parse_object
 *  Description : Returns success or an error from parse object.
 *  Parameters  : s — const std::string& s; i — std::size_t& i; depth — std::size_t depth
 *  Returns     : Result<Value>
 */
Result<Value> parse_object(const std::string& s, std::size_t& i, std::size_t depth) {
  if (depth > kMaxJsonNestingDepth) {
    return bad(i, "JSON maximum nesting depth exceeded");
  }
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
    auto val = parse_value(s, i, depth + 1);
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

/**
 *  Function    : parse_value
 *  Description : Returns success or an error from parse value.
 *  Parameters  : s — const std::string& s; i — std::size_t& i; depth — std::size_t depth
 *  Returns     : Result<Value>
 */
Result<Value> parse_value(const std::string& s, std::size_t& i, std::size_t depth) {
  if (depth > kMaxJsonNestingDepth) {
    return bad(i, "JSON maximum nesting depth exceeded");
  }
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
    return parse_array(s, i, depth);
  }
  if (c == '{') {
    return parse_object(s, i, depth);
  }
  if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
    return parse_number(s, i);
  }
  return bad(i, "invalid JSON value");
}

}  // namespace

/**
 *  Function    : write_value
 *  Description : Performs write value (definition).
 *  Parameters  : out — std::string& out; v — const Value& v
 *  Returns     : void
 */
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

/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : v — const Value& v
 *  Returns     : std::string
 */
std::string to_string(const Value& v) {
  std::string out;
  write_value(out, v);
  return out;
}

/**
 *  Function    : parse_document
 *  Description : Returns success or an error from parse document.
 *  Parameters  : json — const std::string& json
 *  Returns     : Result<Value>
 */
Result<Value> parse_document(const std::string& json) {
  std::size_t i = 0;
  auto v = parse_value(json, i, 0);
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
