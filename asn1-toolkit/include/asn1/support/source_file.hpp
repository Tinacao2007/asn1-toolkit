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
  SourceFile() = default;

  /// Load from an existing UTF-8 / ASCII buffer (copied). Useful in tests.
  static SourceFile from_string(std::string path, std::string contents);

  /// Read a file from disk. Throws std::runtime_error on I/O failure.
  static SourceFile from_path(const std::string& path);

  const std::string& path() const noexcept { return path_; }
  const std::string& text() const noexcept { return text_; }
  std::string_view view() const noexcept { return text_; }
  std::size_t size() const noexcept { return text_.size(); }

  /// Map a byte offset (0-based) to a source location.
  SourceLocation location_at(std::size_t offset) const;

  /// Range covering [begin_offset, end_offset).
  SourceRange range(std::size_t begin_offset, std::size_t end_offset) const;

 private:
  SourceFile(std::string path, std::string text);

  void build_line_index();

  std::string path_;
  std::string text_;
  /// Byte offset of the first character of each 1-based line.
  /// line_starts_[0] == 0 for line 1.
  std::vector<std::size_t> line_starts_;
};

}  // namespace asn1
