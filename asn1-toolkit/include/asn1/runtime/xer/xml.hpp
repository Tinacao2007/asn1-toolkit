/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/xer/xml.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Lightweight XML tree for XER-family codecs.
**
** Specification: ITU-T X.693 — XML Encoding Rules (support types in this
**                 file).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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
/**
 *  Function    : write_element
 *  Description : Performs write element (declaration).
 *  Parameters  : out — std::string& out; el — const Element& el
 *  Returns     : void
 */
void write_element(std::string& out, const Element& el);
/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : el — const Element& el
 *  Returns     : std::string
 */
std::string to_string(const Element& el);

/// Parse a single-root XML document. Skips insignificant whitespace between tags.
/**
 *  Function    : parse_document
 *  Description : Returns success or an error from parse document.
 *  Parameters  : xml — const std::string& xml
 *  Returns     : Result<Element>
 */
Result<Element> parse_document(const std::string& xml);

/// Escape text for element character data (XML 1.0 + decimal NCRs for non-ASCII).
/**
 *  Function    : append_escaped_text
 *  Description : Performs append escaped text (declaration).
 *  Parameters  : out — std::string& out; text — const std::string& text
 *  Returns     : void
 */
void append_escaped_text(std::string& out, const std::string& text);

/// Escape text for attribute values (also escapes quotes).
/**
 *  Function    : append_escaped_attr
 *  Description : Performs append escaped attr (declaration).
 *  Parameters  : out — std::string& out; text — const std::string& text
 *  Returns     : void
 */
void append_escaped_attr(std::string& out, const std::string& text);

/// Unescape character / entity references in element text.
/**
 *  Function    : unescape_text
 *  Description : Builds and returns a string for unescape text.
 *  Parameters  : escaped — const std::string& escaped; offset — std::size_t offset
 *  Returns     : Result<std::string>
 */
Result<std::string> unescape_text(const std::string& escaped, std::size_t offset = 0);

}  // namespace xer
}  // namespace asn1
