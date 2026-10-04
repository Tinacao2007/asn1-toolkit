/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/jeri/codec.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   JERI canonical JSON encoding on top of JER.
**
** Specification: ITU-T X.697 — Canonical JSON Encoding Rules (JERI).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/jeri/codec.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cctype>

namespace asn1 {
namespace jeri {
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

}  // namespace

std::string transform_name(const std::string& identifier, NameForm form,
                           const std::string& literal) {
  switch (form) {
    case NameForm::AsIs:
      return identifier;
    case NameForm::Literal:
      return literal;
    case NameForm::Capitalized: {
      if (identifier.empty()) {
        return identifier;
      }
      std::string out = identifier;
      out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
      return out;
    }
    case NameForm::Uppercased: {
      std::string out = identifier;
      for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      }
      return out;
    }
    case NameForm::Lowercased: {
      std::string out = identifier;
      for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      return out;
    }
  }
  return identifier;
}

std::pair<std::string, Value> named_member(const std::string& identifier, Value encoding,
                                           NameForm form, const std::string& literal) {
  return {transform_name(identifier, form, literal), std::move(encoding)};
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
 *  Description : Computes encode octet string base64 from (value).
 *  Parameters  : value — Span<const std::uint8_t> value
 *  Returns     : Value
 */
Value encode_octet_string_base64(Span<const std::uint8_t> value) {
  return Value::string(encode_base64(value));
}

/**
 *  Function    : decode_octet_string_base64
 *  Description : Returns success or an error from decode octet string base64.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<std::vector<std::uint8_t>>
 */
Result<std::vector<std::uint8_t>> decode_octet_string_base64(const Value& v) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER BASE64 OCTET STRING expects JSON string");
  }
  if (v.string_value.empty()) {
    return std::vector<std::uint8_t>{};
  }
  return decode_base64(v.string_value);
}

/**
 *  Function    : encode_sequence_array
 *  Description : Computes encode sequence array from (components, omit_trailing_nulls).
 *  Parameters  : components — std::vector<Value> components; omit_trailing_nulls — bool omit_trailing_nulls
 *  Returns     : Value
 */
Value encode_sequence_array(std::vector<Value> components, bool omit_trailing_nulls) {
  if (omit_trailing_nulls) {
    while (!components.empty() && components.back().kind == ValueKind::Null) {
      components.pop_back();
    }
  }
  return Value::make_array(std::move(components));
}

/**
 *  Function    : decode_sequence_array
 *  Description : Returns success or an error from decode sequence array.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<const std::vector<Value>*>
 */
Result<const std::vector<Value>*> decode_sequence_array(const Value& v) {
  if (v.kind != ValueKind::Array) {
    return bad(0, "JER ARRAY SEQUENCE expects JSON array");
  }
  return &v.array;
}

/**
 *  Function    : encode_set_of_object
 *  Description : Computes encode set of object from (std::vector<std::pair<std::string, entries).
 *  Parameters  : std::vector<std::pair<std::string — std::vector<std::pair<std::string; entries — Value>> entries
 *  Returns     : Value
 */
Value encode_set_of_object(std::vector<std::pair<std::string, Value>> entries) {
  return Value::make_object(std::move(entries));
}

Result<std::vector<std::pair<std::string, const Value*>>> decode_set_of_object(
    const Value& v) {
  if (v.kind != ValueKind::Object) {
    return bad(0, "JER OBJECT SET OF expects JSON object");
  }
  std::vector<std::pair<std::string, const Value*>> out;
  out.reserve(v.object.size());
  for (const auto& m : v.object) {
    out.emplace_back(m.first, &m.second);
  }
  return out;
}

Value encode_enumerated_text(const std::string& identifier, NameForm form,
                             const std::string& literal) {
  return Value::string(transform_name(identifier, form, literal));
}

Result<std::string> decode_enumerated_text(const Value& v,
                                           const std::vector<std::string>& identifiers,
                                           NameForm form,
                                           const std::vector<std::string>& literals) {
  if (v.kind != ValueKind::String) {
    return bad(0, "JER TEXT ENUMERATED expects JSON string");
  }
  const std::string& got = v.string_value;
  for (std::size_t i = 0; i < identifiers.size(); ++i) {
    const std::string& lit =
        (form == NameForm::Literal && i < literals.size()) ? literals[i] : std::string{};
    if (transform_name(identifiers[i], form, lit) == got) {
      return identifiers[i];
    }
  }
  return bad(0, "JER TEXT ENUMERATED value not in enumeration: " + got);
}

/**
 *  Function    : encode_choice_unwrapped
 *  Description : Computes encode choice unwrapped from (alternative_encoding).
 *  Parameters  : alternative_encoding — Value alternative_encoding
 *  Returns     : Value
 */
Value encode_choice_unwrapped(Value alternative_encoding) {
  return alternative_encoding;
}

/**
 *  Function    : decode_choice_unwrapped
 *  Description : Returns success or an error from decode choice unwrapped.
 *  Parameters  : v — const Value& v
 *  Returns     : Result<const Value*>
 */
Result<const Value*> decode_choice_unwrapped(const Value& v) {
  return &v;
}

}  // namespace jeri
}  // namespace asn1
