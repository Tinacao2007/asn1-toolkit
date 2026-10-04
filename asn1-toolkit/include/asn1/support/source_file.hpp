/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/support/source_file.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Read-only source text buffer indexed by lexer tokens.
**
** Specification: No external protocol; C++ infrastructure shared by
**                 compiler and runtime.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/support/source_location.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace asn1 {

/// Owns ASN.1 source text. Lexer Token::text views point into this buffer.
class SourceFile {
 public:
  /**
   *  Function    : SourceFile
   *  Description : Constructs or initializes SourceFile.
   *  Parameters  : none
   *  Returns     : void
   */
  SourceFile() = default;

  /// Load from an existing UTF-8 / ASCII buffer (copied). Useful in tests.
  /**
   *  Function    : from_string
   *  Description : Computes from string from (path, contents).
   *  Parameters  : path — std::string path; contents — std::string contents
   *  Returns     : static SourceFile
   */
  static SourceFile from_string(std::string path, std::string contents);

  /// Read a file from disk. Throws std::runtime_error on I/O failure.
  /**
   *  Function    : from_path
   *  Description : Computes from path from (path).
   *  Parameters  : path — const std::string& path
   *  Returns     : static SourceFile
   */
  static SourceFile from_path(const std::string& path);

  /**
   *  Function    : path
   *  Description : Builds and returns a string for path.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& path() const noexcept { return path_; }
  /**
   *  Function    : text
   *  Description : Builds and returns a string for text.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& text() const noexcept { return text_; }
  /**
   *  Function    : view
   *  Description : Builds and returns a string for view.
   *  Parameters  : none
   *  Returns     : std::string_view
   */
  std::string_view view() const noexcept { return text_; }
  /**
   *  Function    : size
   *  Description : Computes size from (text_.size().
   *  Parameters  : text_.size( — ) const noexcept { return text_.size(
   *  Returns     : std::size_t size() const noexcept { return text_.
   */
  std::size_t size() const noexcept { return text_.size(); }

  /// Map a byte offset (0-based) to a source location.
  /**
   *  Function    : location_at
   *  Description : Computes location at from (offset).
   *  Parameters  : offset — std::size_t offset
   *  Returns     : SourceLocation
   */
  SourceLocation location_at(std::size_t offset) const;

  /// Range covering [begin_offset, end_offset).
  /**
   *  Function    : range
   *  Description : Computes range from (begin_offset, end_offset).
   *  Parameters  : begin_offset — std::size_t begin_offset; end_offset — std::size_t end_offset
   *  Returns     : SourceRange
   */
  SourceRange range(std::size_t begin_offset, std::size_t end_offset) const;

 private:
  /**
   *  Function    : SourceFile
   *  Description : Performs SourceFile (declaration).
   *  Parameters  : path — std::string path; text — std::string text
   *  Returns     : void
   */
  SourceFile(std::string path, std::string text);

  /**
   *  Function    : build_line_index
   *  Description : Performs build line index (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void build_line_index();

  std::string path_;
  std::string text_;
  /// Byte offset of the first character of each 1-based line.
  /// line_starts_[0] == 0 for line 1.
  std::vector<std::size_t> line_starts_;
};

}  // namespace asn1
