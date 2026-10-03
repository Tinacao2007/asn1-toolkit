#include <asn1/runtime/xer/xml.hpp>
#include <asn1/runtime/byte_io.hpp>

#include <cctype>

namespace asn1 {
namespace xer {
namespace {

Error bad(std::size_t offset, std::string message) {
  return make_error(Error::Code::InvalidArgument, offset, std::move(message));
}

bool is_name_start(unsigned char c) {
  return std::isalpha(c) || c == '_' || c == ':';
}

bool is_name_char(unsigned char c) {
  return std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == ':';
}

void skip_ws(const std::string& xml, std::size_t& i) {
  while (i < xml.size() &&
         (xml[i] == ' ' || xml[i] == '\t' || xml[i] == '\n' || xml[i] == '\r')) {
    ++i;
  }
}

Result<std::string> parse_name(const std::string& xml, std::size_t& i) {
  if (i >= xml.size() || !is_name_start(static_cast<unsigned char>(xml[i]))) {
    return bad(i, "expected XML name");
  }
  const std::size_t start = i;
  ++i;
  while (i < xml.size() && is_name_char(static_cast<unsigned char>(xml[i]))) {
    ++i;
  }
  return xml.substr(start, i - start);
}

Result<void> expect_char(const std::string& xml, std::size_t& i, char c) {
  if (i >= xml.size() || xml[i] != c) {
    return bad(i, std::string("expected '") + c + "'");
  }
  ++i;
  return Result<void>::success();
}

/// Decode one UTF-8 code point starting at `i`; advances `i`.
Result<std::uint32_t> decode_utf8(const std::string& s, std::size_t& i) {
  if (i >= s.size()) {
    return bad(i, "truncated UTF-8");
  }
  const auto b0 = static_cast<unsigned char>(s[i++]);
  if (b0 < 0x80u) {
    return static_cast<std::uint32_t>(b0);
  }
  int need = 0;
  std::uint32_t cp = 0;
  if ((b0 & 0xE0u) == 0xC0u) {
    need = 1;
    cp = b0 & 0x1Fu;
  } else if ((b0 & 0xF0u) == 0xE0u) {
    need = 2;
    cp = b0 & 0x0Fu;
  } else if ((b0 & 0xF8u) == 0xF0u) {
    need = 3;
    cp = b0 & 0x07u;
  } else {
    return bad(i - 1, "invalid UTF-8 lead byte");
  }
  for (int n = 0; n < need; ++n) {
    if (i >= s.size()) {
      return bad(i, "truncated UTF-8");
    }
    const auto b = static_cast<unsigned char>(s[i++]);
    if ((b & 0xC0u) != 0x80u) {
      return bad(i - 1, "invalid UTF-8 continuation");
    }
    cp = (cp << 6) | (b & 0x3Fu);
  }
  return cp;
}

void append_utf8(std::string& out, std::uint32_t cp) {
  if (cp < 0x80u) {
    out.push_back(static_cast<char>(cp));
  } else if (cp < 0x800u) {
    out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  } else if (cp < 0x10000u) {
    out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
    out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  } else {
    out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
    out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  }
}

constexpr std::size_t kMaxXmlNestingDepth = 256;

Result<Element> parse_element(const std::string& xml, std::size_t& i, std::size_t depth);

Result<void> parse_content(const std::string& xml, std::size_t& i, Element& el, std::size_t depth) {
  std::string text_acc;
  while (i < xml.size()) {
    if (xml[i] == '<') {
      if (i + 1 < xml.size() && xml[i + 1] == '/') {
        break;  // closing tag
      }
      if (!text_acc.empty()) {
        auto un = unescape_text(text_acc, i);
        if (!un.ok()) {
          return un.error();
        }
        text_acc.clear();
        // Insignificant whitespace-only text between children is dropped.
        bool only_ws = true;
        for (char c : un.value()) {
          if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            only_ws = false;
            break;
          }
        }
        if (!only_ws) {
          el.text += un.value();
        }
      }
      auto child = parse_element(xml, i, depth + 1);
      if (!child.ok()) {
        return child.error();
      }
      el.children.push_back(std::move(child.value()));
      continue;
    }
    text_acc.push_back(xml[i++]);
  }
  if (!text_acc.empty()) {
    auto un = unescape_text(text_acc, i);
    if (!un.ok()) {
      return un.error();
    }
    // Trim? Keep as-is for primitive text; whitespace-only kept if no children.
    if (el.children.empty()) {
      el.text = std::move(un.value());
    } else {
      bool only_ws = true;
      for (char c : un.value()) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
          only_ws = false;
          break;
        }
      }
      if (!only_ws) {
        el.text += un.value();
      }
    }
  }
  return Result<void>::success();
}

Result<Element> parse_element(const std::string& xml, std::size_t& i, std::size_t depth) {
  if (depth > kMaxXmlNestingDepth) {
    return bad(i, "XML maximum nesting depth exceeded");
  }
  if (auto r = expect_char(xml, i, '<'); !r.ok()) {
    return r.error();
  }
  auto name = parse_name(xml, i);
  if (!name.ok()) {
    return name.error();
  }
  Element el;
  el.name = std::move(name.value());
  skip_ws(xml, i);
  while (i < xml.size() && xml[i] != '>' && xml[i] != '/') {
    auto an = parse_name(xml, i);
    if (!an.ok()) {
      return an.error();
    }
    skip_ws(xml, i);
    if (auto r = expect_char(xml, i, '='); !r.ok()) {
      return r.error();
    }
    skip_ws(xml, i);
    if (i >= xml.size() || (xml[i] != '"' && xml[i] != '\'')) {
      return bad(i, "expected attribute value");
    }
    const char q = xml[i++];
    const std::size_t vstart = i;
    while (i < xml.size() && xml[i] != q) {
      ++i;
    }
    if (i >= xml.size()) {
      return bad(i, "unterminated attribute value");
    }
    auto un = unescape_text(xml.substr(vstart, i - vstart), vstart);
    if (!un.ok()) {
      return un.error();
    }
    el.attributes.push_back(Attribute{std::move(an.value()), std::move(un.value())});
    ++i;
    skip_ws(xml, i);
  }
  if (i < xml.size() && xml[i] == '/') {
    ++i;
    if (auto r = expect_char(xml, i, '>'); !r.ok()) {
      return r.error();
    }
    return el;
  }
  if (auto r = expect_char(xml, i, '>'); !r.ok()) {
    return r.error();
  }
  if (auto r = parse_content(xml, i, el, depth); !r.ok()) {
    return r.error();
  }
  if (auto r = expect_char(xml, i, '<'); !r.ok()) {
    return r.error();
  }
  if (auto r = expect_char(xml, i, '/'); !r.ok()) {
    return r.error();
  }
  auto end_name = parse_name(xml, i);
  if (!end_name.ok()) {
    return end_name.error();
  }
  if (end_name.value() != el.name) {
    return bad(i, "mismatched closing tag");
  }
  skip_ws(xml, i);
  if (auto r = expect_char(xml, i, '>'); !r.ok()) {
    return r.error();
  }
  return el;
}

}  // namespace

void append_escaped_text(std::string& out, const std::string& text) {
  std::size_t i = 0;
  while (i < text.size()) {
    // Fast path ASCII
    const auto b = static_cast<unsigned char>(text[i]);
    if (b < 0x80u) {
      ++i;
      if (b == '&') {
        out += "&amp;";
      } else if (b == '<') {
        out += "&lt;";
      } else if (b == '>') {
        out += "&gt;";
      } else if (b < 0x20u && b != '\t' && b != '\n' && b != '\r') {
        out += "&#";
        out += std::to_string(b);
        out += ';';
      } else {
        out.push_back(static_cast<char>(b));
      }
      continue;
    }
    std::size_t j = i;
    auto cp = decode_utf8(text, j);
    if (!cp.ok()) {
      // Fallback: emit byte as NCR
      out += "&#";
      out += std::to_string(b);
      out += ';';
      ++i;
      continue;
    }
    i = j;
    out += "&#";
    out += std::to_string(cp.value());
    out += ';';
  }
}

Result<std::string> unescape_text(const std::string& escaped, std::size_t offset) {
  std::string out;
  out.reserve(escaped.size());
  for (std::size_t i = 0; i < escaped.size();) {
    if (escaped[i] != '&') {
      out.push_back(escaped[i++]);
      continue;
    }
    const std::size_t start = i;
    ++i;
    if (i < escaped.size() && escaped[i] == '#') {
      ++i;
      bool hex = false;
      if (i < escaped.size() && (escaped[i] == 'x' || escaped[i] == 'X')) {
        hex = true;
        ++i;
      }
      std::uint32_t cp = 0;
      if (i >= escaped.size()) {
        return bad(offset + start, "unterminated character reference");
      }
      if (hex) {
        while (i < escaped.size() && std::isxdigit(static_cast<unsigned char>(escaped[i]))) {
          const char c = escaped[i++];
          cp *= 16;
          if (c >= '0' && c <= '9') {
            cp += static_cast<std::uint32_t>(c - '0');
          } else if (c >= 'a' && c <= 'f') {
            cp += static_cast<std::uint32_t>(10 + c - 'a');
          } else {
            cp += static_cast<std::uint32_t>(10 + c - 'A');
          }
        }
      } else {
        while (i < escaped.size() && std::isdigit(static_cast<unsigned char>(escaped[i]))) {
          cp = cp * 10 + static_cast<std::uint32_t>(escaped[i++] - '0');
        }
      }
      if (i >= escaped.size() || escaped[i] != ';') {
        return bad(offset + start, "unterminated character reference");
      }
      ++i;
      append_utf8(out, cp);
      continue;
    }
    // Named entities
    const std::size_t name_start = i;
    while (i < escaped.size() && std::isalpha(static_cast<unsigned char>(escaped[i]))) {
      ++i;
    }
    if (i >= escaped.size() || escaped[i] != ';') {
      return bad(offset + start, "unterminated entity reference");
    }
    const std::string name = escaped.substr(name_start, i - name_start);
    ++i;
    if (name == "amp") {
      out.push_back('&');
    } else if (name == "lt") {
      out.push_back('<');
    } else if (name == "gt") {
      out.push_back('>');
    } else if (name == "quot") {
      out.push_back('"');
    } else if (name == "apos") {
      out.push_back('\'');
    } else {
      return bad(offset + start, "unknown entity reference");
    }
  }
  return out;
}

void append_escaped_attr(std::string& out, const std::string& text) {
  for (unsigned char b : text) {
    if (b == '&') {
      out += "&amp;";
    } else if (b == '<') {
      out += "&lt;";
    } else if (b == '"') {
      out += "&quot;";
    } else if (b == '\'') {
      out += "&apos;";
    } else {
      out.push_back(static_cast<char>(b));
    }
  }
}

void write_element(std::string& out, const Element& el) {
  out.push_back('<');
  out += el.name;
  for (const auto& a : el.attributes) {
    out.push_back(' ');
    out += a.name;
    out += "=\"";
    append_escaped_attr(out, a.value);
    out.push_back('"');
  }
  if (el.children.empty() && el.text.empty()) {
    out += " />";
    return;
  }
  out.push_back('>');
  if (!el.text.empty()) {
    append_escaped_text(out, el.text);
  }
  for (const auto& child : el.children) {
    write_element(out, child);
  }
  out += "</";
  out += el.name;
  out.push_back('>');
}

std::string to_string(const Element& el) {
  std::string out;
  write_element(out, el);
  return out;
}

Result<Element> parse_document(const std::string& xml) {
  std::size_t i = 0;
  skip_ws(xml, i);
  // Optional XML declaration / processing instructions — skip <? ... ?>
  while (i + 1 < xml.size() && xml[i] == '<' && xml[i + 1] == '?') {
    i += 2;
    while (i + 1 < xml.size() && !(xml[i] == '?' && xml[i + 1] == '>')) {
      ++i;
    }
    if (i + 1 >= xml.size()) {
      return bad(i, "unterminated processing instruction");
    }
    i += 2;
    skip_ws(xml, i);
  }
  auto el = parse_element(xml, i, 0);
  if (!el.ok()) {
    return el.error();
  }
  skip_ws(xml, i);
  if (i != xml.size()) {
    return bad(i, "trailing data after XML root");
  }
  return el;
}

}  // namespace xer
}  // namespace asn1
