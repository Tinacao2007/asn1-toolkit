/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/support/diagnostics.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Diagnostic message storage and formatting.
**
** Specification: No external protocol; compiler infrastructure.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/support/diagnostics.hpp>

#include <sstream>

namespace asn1 {
namespace {

/**
 *  Function    : severity_label
 *  Description : Computes severity label from (s).
 *  Parameters  : s — Severity s
 *  Returns     : const char*
 */
const char* severity_label(Severity s) {
  switch (s) {
    case Severity::Note:
      return "note";
    case Severity::Warning:
      return "warning";
    case Severity::Error:
      return "error";
  }
  return "error";
}

}  // namespace

/**
 *  Function    : add
 *  Description : Performs add (definition).
 *  Parameters  : severity — Severity severity; where — SourceRange where; message — std::string message
 *  Returns     : void Diagnostics::
 */
void Diagnostics::add(Severity severity, SourceRange where, std::string message) {
  if (severity == Severity::Error) {
    ++error_count_;
  }
  items_.push_back(Diagnostic{severity, std::move(where), std::move(message)});
}

/**
 *  Function    : note
 *  Description : Performs note (definition).
 *  Parameters  : where — SourceRange where; message — std::string message
 *  Returns     : void Diagnostics::
 */
void Diagnostics::note(SourceRange where, std::string message) {
  add(Severity::Note, std::move(where), std::move(message));
}

/**
 *  Function    : warning
 *  Description : Performs warning (definition).
 *  Parameters  : where — SourceRange where; message — std::string message
 *  Returns     : void Diagnostics::
 */
void Diagnostics::warning(SourceRange where, std::string message) {
  add(Severity::Warning, std::move(where), std::move(message));
}

/**
 *  Function    : error
 *  Description : Performs error (definition).
 *  Parameters  : where — SourceRange where; message — std::string message
 *  Returns     : void Diagnostics::
 */
void Diagnostics::error(SourceRange where, std::string message) {
  add(Severity::Error, std::move(where), std::move(message));
}

/**
 *  Function    : format
 *  Description : Builds and returns a string for format.
 *  Parameters  : d — const Diagnostic& d
 *  Returns     : std::string Diagnostics::
 */
std::string Diagnostics::format(const Diagnostic& d) const {
  std::ostringstream oss;
  const SourceLocation& loc = d.where.begin;
  if (!loc.file.empty()) {
    oss << loc.file << ':' << loc.line << ':' << loc.column << ":\n";
  }
  oss << severity_label(d.severity) << ": " << d.message;
  return oss.str();
}

/**
 *  Function    : print
 *  Description : Performs print (definition).
 *  Parameters  : out — std::ostream& out
 *  Returns     : void Diagnostics::
 */
void Diagnostics::print(std::ostream& out) const {
  for (const Diagnostic& d : items_) {
    out << format(d) << '\n';
  }
}

/**
 *  Function    : merge
 *  Description : Move-append another Diagnostics. Items end up in merge-call
 *                order so the caller controls the final byte-level sequence
 *                (used by the parallel analyzer orchestrator in P3-1).
 *  Parameters  : other — Diagnostics&& other
 *  Returns     : void Diagnostics::
 */
void Diagnostics::merge(Diagnostics&& other) noexcept {
  // error_count_ aggregates: parallel workers never share this Diagnostics so
  // the counter is always the local worker's count. Add it so the merged
  // total equals the sum of workers' counts (matches serial aggregation).
  error_count_ += other.error_count_;
  if (!other.items_.empty()) {
    if (items_.empty()) {
      items_ = std::move(other.items_);
    } else {
      items_.reserve(items_.size() + other.items_.size());
      for (auto& d : other.items_) {
        items_.push_back(std::move(d));
      }
      other.items_.clear();
    }
  }
}

}  // namespace asn1
