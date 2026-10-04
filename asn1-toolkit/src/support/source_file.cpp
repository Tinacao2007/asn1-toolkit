/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/support/source_file.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   SourceFile load and line/column offset mapping.
**
** Specification: No external protocol; compiler infrastructure.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/support/source_file.hpp>

#include <fstream>
#include <stdexcept>
#include <utility>

namespace asn1 {

/**
 *  Function    : move
 *  Description : Builds and returns a string for move.
 *  Parameters  : path — std::string path; text_(std::move(text) — std::string text) : path_(std::move(path)), text_(std::move(text)
 *  Returns     : SourceFile::SourceFile(std::string path, std::string text) : path_(std::move(path)), text_(std::
 */
SourceFile::SourceFile(std::string path, std::string text)
    : path_(std::move(path)), text_(std::move(text)) {
  build_line_index();
}

/**
 *  Function    : from_string
 *  Description : Computes from string from (path, contents).
 *  Parameters  : path — std::string path; contents — std::string contents
 *  Returns     : SourceFile SourceFile::
 */
SourceFile SourceFile::from_string(std::string path, std::string contents) {
  return SourceFile(std::move(path), std::move(contents));
}

/**
 *  Function    : from_path
 *  Description : Computes from path from (path).
 *  Parameters  : path — const std::string& path
 *  Returns     : SourceFile SourceFile::
 */
SourceFile SourceFile::from_path(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open ASN.1 source file: " + path);
  }
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  return SourceFile(path, std::move(contents));
}

/**
 *  Function    : build_line_index
 *  Description : Performs build line index (definition).
 *  Parameters  : none
 *  Returns     : void SourceFile::
 */
void SourceFile::build_line_index() {
  line_starts_.clear();
  line_starts_.push_back(0);
  for (std::size_t i = 0; i < text_.size(); ++i) {
    if (text_[i] == '\n') {
      line_starts_.push_back(i + 1);
    }
  }
}

SourceLocation SourceFile::location_at(std::size_t offset) const {
  SourceLocation loc;
  loc.file = path_;
  if (line_starts_.empty()) {
    loc.line = 1;
    loc.column = 1;
    return loc;
  }

  if (offset > text_.size()) {
    offset = text_.size();
  }

  // Binary search: last line_start <= offset.
  std::size_t lo = 0;
  std::size_t hi = line_starts_.size();
  while (lo + 1 < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    if (line_starts_[mid] <= offset) {
      lo = mid;
    } else {
      hi = mid;
    }
  }

  loc.line = static_cast<int>(lo + 1);
  loc.column = static_cast<int>(offset - line_starts_[lo] + 1);
  return loc;
}

SourceRange SourceFile::range(std::size_t begin_offset, std::size_t end_offset) const {
  if (end_offset < begin_offset) {
    end_offset = begin_offset;
  }
  return SourceRange{location_at(begin_offset), location_at(end_offset)};
}

}  // namespace asn1
