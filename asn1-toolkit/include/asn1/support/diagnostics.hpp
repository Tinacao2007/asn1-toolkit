/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/support/diagnostics.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Collects lexer/parser/semantic errors with SourceLocation.
**
** Specification: No external protocol; C++ infrastructure shared by
**                 compiler and runtime.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/support/source_location.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace asn1 {

enum class Severity { Note, Warning, Error };

struct Diagnostic {
  Severity severity = Severity::Error;
  SourceRange where;
  std::string message;
};

/// Collects frontend / semantic diagnostics. Does not throw.
class Diagnostics {
 public:
  /**
   *  Function    : note
   *  Description : Performs note (declaration).
   *  Parameters  : where — SourceRange where; message — std::string message
   *  Returns     : void
   */
  void note(SourceRange where, std::string message);
  /**
   *  Function    : warning
   *  Description : Performs warning (declaration).
   *  Parameters  : where — SourceRange where; message — std::string message
   *  Returns     : void
   */
  void warning(SourceRange where, std::string message);
  /**
   *  Function    : error
   *  Description : Performs error (declaration).
   *  Parameters  : where — SourceRange where; message — std::string message
   *  Returns     : void
   */
  void error(SourceRange where, std::string message);

  /**
   *  Function    : ok
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool ok() const noexcept { return error_count_ == 0; }
  /**
   *  Function    : error_count
   *  Description : Computes error count from (none).
   *  Parameters  : none
   *  Returns     : std::size_t
   */
  std::size_t error_count() const noexcept { return error_count_; }
  /**
   *  Function    : items
   *  Description : Computes items from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<Diagnostic>&
   */
  const std::vector<Diagnostic>& items() const noexcept { return items_; }

  /**
   *  Function    : print
   *  Description : Performs print (declaration).
   *  Parameters  : out — std::ostream& out
   *  Returns     : void
   */
  void print(std::ostream& out) const;
  /**
   *  Function    : format
   *  Description : Builds and returns a string for format.
   *  Parameters  : d — const Diagnostic& d
   *  Returns     : std::string
   */
  std::string format(const Diagnostic& d) const;

 private:
  /**
   *  Function    : add
   *  Description : Performs add (declaration).
   *  Parameters  : severity — Severity severity; where — SourceRange where; message — std::string message
   *  Returns     : void
   */
  void add(Severity severity, SourceRange where, std::string message);

  std::vector<Diagnostic> items_;
  std::size_t error_count_ = 0;
};

}  // namespace asn1
