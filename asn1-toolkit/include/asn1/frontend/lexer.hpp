/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/frontend/lexer.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Lexer interface: yields ASN.1 tokens from a SourceFile.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (lexical and
**                 syntactic notation).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/frontend/token.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

namespace asn1 {

/// Hand-written ASN.1 lexer. Does not allocate for token text (views into SourceFile).
class Lexer {
 public:
  /**
   *  Function    : Lexer
   *  Description : Constructs or initializes Lexer.
   *  Parameters  : file — const SourceFile& file; diagnostics — Diagnostics& diagnostics
   *  Returns     : void
   */
  Lexer(const SourceFile& file, Diagnostics& diagnostics);

  /// Return the next token. After EndOfFile, further calls keep returning EndOfFile.
  /**
   *  Function    : next
   *  Description : Computes next from (none).
   *  Parameters  : none
   *  Returns     : Token
   */
  Token next();

  /**
   *  Function    : file
   *  Description : Computes file from (none).
   *  Parameters  : none
   *  Returns     : const SourceFile&
   */
  const SourceFile& file() const noexcept { return file_; }

 private:
  /**
   *  Function    : peek
   *  Description : Computes peek from (ahead).
   *  Parameters  : ahead — std::size_t ahead
   *  Returns     : char
   */
  char peek(std::size_t ahead = 0) const;
  /**
   *  Function    : get
   *  Description : Computes get from (none).
   *  Parameters  : none
   *  Returns     : char
   */
  char get();
  /**
   *  Function    : eof
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool eof() const;
  /**
   *  Function    : skip_whitespace_and_comments
   *  Description : Performs skip whitespace and comments (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void skip_whitespace_and_comments();
  /**
   *  Function    : try_skip_comment
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool try_skip_comment();

  /**
   *  Function    : make
   *  Description : Computes make from (kind, begin, end).
   *  Parameters  : kind — TokenKind kind; begin — std::size_t begin; end — std::size_t end
   *  Returns     : Token
   */
  Token make(TokenKind kind, std::size_t begin, std::size_t end) const;
  /**
   *  Function    : lex_identifier_or_keyword
   *  Description : Computes lex identifier or keyword from (none).
   *  Parameters  : none
   *  Returns     : Token
   */
  Token lex_identifier_or_keyword();
  /**
   *  Function    : lex_number
   *  Description : Computes lex number from (none).
   *  Parameters  : none
   *  Returns     : Token
   */
  Token lex_number();
  Token lex_quoted_string();  // binary / hex / character
  /**
   *  Function    : lex_punctuation
   *  Description : Computes lex punctuation from (none).
   *  Parameters  : none
   *  Returns     : Token
   */
  Token lex_punctuation();

  const SourceFile& file_;
  Diagnostics& diagnostics_;
  std::size_t pos_ = 0;
};

}  // namespace asn1
