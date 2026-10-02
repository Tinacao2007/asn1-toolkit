#pragma once

#include <string>

namespace asn1 {

/// 1-based line and column, matching typical compiler diagnostics.
struct SourceLocation {
  std::string file;
  int line = 1;
  int column = 1;
};

struct SourceRange {
  SourceLocation begin;
  SourceLocation end;
};

}  // namespace asn1
