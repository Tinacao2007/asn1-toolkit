#pragma once

#include <asn1/common/result.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace asn1 {
namespace xer {

struct Attribute {
  std::string name;
  std::string value;
};

/// Minimal XML element tree for XER / EXTENDED-XER.
struct Element {
  std::string name;
  std::string text;  // decoded character data (empty if none)
  std::vector<Attribute> attributes;
  std::vector<Element> children;
};

/// Serialize `el` as UTF-8 XML (no XML declaration). Empty elements use `<name />`.
void write_element(std::string& out, const Element& el);
std::string to_string(const Element& el);

/// Parse a single-root XML document. Skips insignificant whitespace between tags.
Result<Element> parse_document(const std::string& xml);

/// Escape text for element character data (XML 1.0 + decimal NCRs for non-ASCII).
void append_escaped_text(std::string& out, const std::string& text);

/// Escape text for attribute values (also escapes quotes).
void append_escaped_attr(std::string& out, const std::string& text);

/// Unescape character / entity references in element text.
Result<std::string> unescape_text(const std::string& escaped, std::size_t offset = 0);

}  // namespace xer
}  // namespace asn1
