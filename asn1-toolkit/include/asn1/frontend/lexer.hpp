#pragma once

#include <asn1/frontend/token.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

namespace asn1 {

/// Hand-written ASN.1 lexer. Does not allocate for token text (views into SourceFile).
class Lexer {
 public:
  Lexer(const SourceFile& file, Diagnostics& diagnostics);

  /// Return the next token. After EndOfFile, further calls keep returning EndOfFile.
  Token next();

  const SourceFile& file() const noexcept { return file_; }

 private:
  char peek(std::size_t ahead = 0) const;
  char get();
  bool eof() const;
  void skip_whitespace_and_comments();
  bool try_skip_comment();

  Token make(TokenKind kind, std::size_t begin, std::size_t end) const;
  Token lex_identifier_or_keyword();
  Token lex_number();
  Token lex_quoted_string();  // binary / hex / character
  Token lex_punctuation();

  const SourceFile& file_;
  Diagnostics& diagnostics_;
  std::size_t pos_ = 0;
};

}  // namespace asn1
