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
  void note(SourceRange where, std::string message);
  void warning(SourceRange where, std::string message);
  void error(SourceRange where, std::string message);

  bool ok() const noexcept { return error_count_ == 0; }
  std::size_t error_count() const noexcept { return error_count_; }
  const std::vector<Diagnostic>& items() const noexcept { return items_; }

  void print(std::ostream& out) const;
  std::string format(const Diagnostic& d) const;

 private:
  void add(Severity severity, SourceRange where, std::string message);

  std::vector<Diagnostic> items_;
  std::size_t error_count_ = 0;
};

}  // namespace asn1
