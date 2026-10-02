#include <asn1/support/diagnostics.hpp>

#include <sstream>

namespace asn1 {
namespace {

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

void Diagnostics::add(Severity severity, SourceRange where, std::string message) {
  if (severity == Severity::Error) {
    ++error_count_;
  }
  items_.push_back(Diagnostic{severity, std::move(where), std::move(message)});
}

void Diagnostics::note(SourceRange where, std::string message) {
  add(Severity::Note, std::move(where), std::move(message));
}

void Diagnostics::warning(SourceRange where, std::string message) {
  add(Severity::Warning, std::move(where), std::move(message));
}

void Diagnostics::error(SourceRange where, std::string message) {
  add(Severity::Error, std::move(where), std::move(message));
}

std::string Diagnostics::format(const Diagnostic& d) const {
  std::ostringstream oss;
  const SourceLocation& loc = d.where.begin;
  if (!loc.file.empty()) {
    oss << loc.file << ':' << loc.line << ':' << loc.column << ":\n";
  }
  oss << severity_label(d.severity) << ": " << d.message;
  return oss.str();
}

void Diagnostics::print(std::ostream& out) const {
  for (const Diagnostic& d : items_) {
    out << format(d) << '\n';
  }
}

}  // namespace asn1
