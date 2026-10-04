/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/frontend/lexer.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Hand-written lexer for ASN.1 lexical syntax (X.680 clause 12).
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (lexical and
**                 syntactic notation).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/frontend/lexer.hpp>

#include <cctype>
#include <string>
#include <unordered_map>

namespace asn1 {
namespace {

/**
 *  Function    : is_letter
 *  Description : Returns whether letter holds for the given inputs.
 *  Parameters  : c — char c
 *  Returns     : bool
 */
bool is_letter(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/**
 *  Function    : is_digit
 *  Description : Returns whether digit holds for the given inputs.
 *  Parameters  : c — char c
 *  Returns     : bool
 */
bool is_digit(char c) { return c >= '0' && c <= '9'; }

/**
 *  Function    : is_hex_digit
 *  Description : Returns whether hex digit holds for the given inputs.
 *  Parameters  : c — char c
 *  Returns     : bool
 */
bool is_hex_digit(char c) {
  return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/**
 *  Function    : is_binary_digit
 *  Description : Returns whether binary digit holds for the given inputs.
 *  Parameters  : c — char c
 *  Returns     : bool
 */
bool is_binary_digit(char c) { return c == '0' || c == '1'; }

/**
 *  Function    : is_whitespace
 *  Description : Returns whether whitespace holds for the given inputs.
 *  Parameters  : c — char c
 *  Returns     : bool
 */
bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

// ASN.1 typereference / identifier body: letters, digits, hyphen;
// hyphen not first, not last, and not consecutive (validated while scanning).
/**
 *  Function    : is_digit
 *  Description : Returns whether digit holds for the given inputs.
 *  Parameters  : is_digit(c — char c) { return is_letter(c) || is_digit(c
 *  Returns     : bool is_name_continue(char c) { return is_letter(c) ||
 */
bool is_name_continue(char c) { return is_letter(c) || is_digit(c) || c == '-'; }

/**
 *  Function    : keyword_map
 *  Description : Builds and returns a string for keyword map.
 *  Parameters  : none
 *  Returns     : const std::unordered_map<std::string_view, TokenKind>&
 */
const std::unordered_map<std::string_view, TokenKind>& keyword_map() {
  static const std::unordered_map<std::string_view, TokenKind> kMap = {
      {"ABSENT", TokenKind::KwABSENT},
      {"ABSTRACT-SYNTAX", TokenKind::KwABSTRACT_SYNTAX},
      {"ALL", TokenKind::KwALL},
      {"APPLICATION", TokenKind::KwAPPLICATION},
      {"ARRAY", TokenKind::KwARRAY},
      {"AS", TokenKind::KwAS},
      {"ATTRIBUTE", TokenKind::KwATTRIBUTE},
      {"AUTOMATIC", TokenKind::KwAUTOMATIC},
      {"BASE64", TokenKind::KwBASE64},
      {"BEGIN", TokenKind::KwBEGIN},
      {"BIT", TokenKind::KwBIT},
      {"BMPString", TokenKind::KwBMPString},
      {"BOOLEAN", TokenKind::KwBOOLEAN},
      {"BY", TokenKind::KwBY},
      {"CAPITALIZED", TokenKind::KwCAPITALIZED},
      {"CHARACTER", TokenKind::KwCHARACTER},
      {"CHOICE", TokenKind::KwCHOICE},
      {"CLASS", TokenKind::KwCLASS},
      {"COMPONENT", TokenKind::KwCOMPONENT},
      {"COMPONENTS", TokenKind::KwCOMPONENTS},
      {"CONSTRAINED", TokenKind::KwCONSTRAINED},
      {"CONTAINING", TokenKind::KwCONTAINING},
      {"DEFAULT", TokenKind::KwDEFAULT},
      {"DEFINITIONS", TokenKind::KwDEFINITIONS},
      {"EMBEDDED", TokenKind::KwEMBEDDED},
      {"ENCODED", TokenKind::KwENCODED},
      {"ENCODING-CONTROL", TokenKind::KwENCODING_CONTROL},
      {"END", TokenKind::KwEND},
      {"ENUMERATED", TokenKind::KwENUMERATED},
      {"EXCEPT", TokenKind::KwEXCEPT},
      {"EXPLICIT", TokenKind::KwEXPLICIT},
      {"EXPORTS", TokenKind::KwEXPORTS},
      {"EXTENDED-XER", TokenKind::KwEXTENDED_XER},
      {"EXTENSIBILITY", TokenKind::KwEXTENSIBILITY},
      {"EXTERNAL", TokenKind::KwEXTERNAL},
      {"FALSE", TokenKind::KwFALSE},
      {"FROM", TokenKind::KwFROM},
      {"GeneralizedTime", TokenKind::KwGeneralizedTime},
      {"GeneralString", TokenKind::KwGeneralString},
      {"GraphicString", TokenKind::KwGraphicString},
      {"IA5String", TokenKind::KwIA5String},
      {"IDENTIFIER", TokenKind::KwIDENTIFIER},
      {"IMPLICIT", TokenKind::KwIMPLICIT},
      {"IMPLIED", TokenKind::KwIMPLIED},
      {"IMPORTS", TokenKind::KwIMPORTS},
      {"INCLUDES", TokenKind::KwINCLUDES},
      {"INSTANCE", TokenKind::KwINSTANCE},
      {"INSTRUCTIONS", TokenKind::KwINSTRUCTIONS},
      {"INTEGER", TokenKind::KwINTEGER},
      {"INTERSECTION", TokenKind::KwINTERSECTION},
      {"ISO646String", TokenKind::KwISO646String},
      {"JER", TokenKind::KwJER},
      {"LIST", TokenKind::KwLIST},
      {"LOWERCASED", TokenKind::KwLOWERCASED},
      {"MAX", TokenKind::KwMAX},
      {"MIN", TokenKind::KwMIN},
      {"MINUS-INFINITY", TokenKind::KwMINUS_INFINITY},
      {"NAME", TokenKind::KwNAME},
      {"NULL", TokenKind::KwNULL},
      {"NumericString", TokenKind::KwNumericString},
      {"OBJECT", TokenKind::KwOBJECT},
      {"ObjectDescriptor", TokenKind::KwObjectDescriptor},
      {"OCTET", TokenKind::KwOCTET},
      {"OF", TokenKind::KwOF},
      {"OPTIONAL", TokenKind::KwOPTIONAL},
      {"PATTERN", TokenKind::KwPATTERN},
      {"PDV", TokenKind::KwPDV},
      {"PLUS-INFINITY", TokenKind::KwPLUS_INFINITY},
      {"PRESENT", TokenKind::KwPRESENT},
      {"PrintableString", TokenKind::KwPrintableString},
      {"PRIVATE", TokenKind::KwPRIVATE},
      {"REAL", TokenKind::KwREAL},
      {"RELATIVE-OID", TokenKind::KwRELATIVE_OID},
      {"SEQUENCE", TokenKind::KwSEQUENCE},
      {"SET", TokenKind::KwSET},
      {"SIZE", TokenKind::KwSIZE},
      {"STRING", TokenKind::KwSTRING},
      {"SYNTAX", TokenKind::KwSYNTAX},
      {"T61String", TokenKind::KwT61String},
      {"TAGS", TokenKind::KwTAGS},
      {"TeletexString", TokenKind::KwTeletexString},
      {"TEXT", TokenKind::KwTEXT},
      {"TRUE", TokenKind::KwTRUE},
      {"TYPE-IDENTIFIER", TokenKind::KwTYPE_IDENTIFIER},
      {"UNION", TokenKind::KwUNION},
      {"UNIQUE", TokenKind::KwUNIQUE},
      {"UNIVERSAL", TokenKind::KwUNIVERSAL},
      {"UniversalString", TokenKind::KwUniversalString},
      {"UNTAGGED", TokenKind::KwUNTAGGED},
      {"UNWRAPPED", TokenKind::KwUNWRAPPED},
      {"UPPERCASED", TokenKind::KwUPPERCASED},
      {"USE-NIL", TokenKind::KwUSE_NIL},
      {"USE-NUMBER", TokenKind::KwUSE_NUMBER},
      {"UTCTime", TokenKind::KwUTCTime},
      {"UTF8String", TokenKind::KwUTF8String},
      {"VideotexString", TokenKind::KwVideotexString},
      {"VisibleString", TokenKind::KwVisibleString},
      {"WITH", TokenKind::KwWITH},
      {"XER", TokenKind::KwXER},
  };
  return kMap;
}

}  // namespace

Lexer::Lexer(const SourceFile& file, Diagnostics& diagnostics)
    : file_(file), diagnostics_(diagnostics) {}

/**
 *  Function    : peek
 *  Description : Computes peek from (ahead).
 *  Parameters  : ahead — std::size_t ahead
 *  Returns     : char Lexer::
 */
char Lexer::peek(std::size_t ahead) const {
  const std::size_t i = pos_ + ahead;
  if (i >= file_.size()) {
    return '\0';
  }
  return file_.text()[i];
}

/**
 *  Function    : get
 *  Description : Computes get from (none).
 *  Parameters  : none
 *  Returns     : char Lexer::
 */
char Lexer::get() {
  if (eof()) {
    return '\0';
  }
  return file_.text()[pos_++];
}

/**
 *  Function    : size
 *  Description : Returns a boolean result from >.
 *  Parameters  : > — ) const { return pos_ >
 *  Returns     : bool Lexer::eof() const { return pos_ >= file_.
 */
bool Lexer::eof() const { return pos_ >= file_.size(); }

Token Lexer::make(TokenKind kind, std::size_t begin, std::size_t end) const {
  Token tok;
  tok.kind = kind;
  tok.text = std::string_view(file_.text()).substr(begin, end - begin);
  tok.range = file_.range(begin, end);
  return tok;
}

/**
 *  Function    : try_skip_comment
 *  Description : Returns a boolean result from none.
 *  Parameters  : none
 *  Returns     : bool Lexer::
 */
bool Lexer::try_skip_comment() {
  // Comment starts with "--" and ends at the next "--" or end of line.
  if (peek() != '-' || peek(1) != '-') {
    return false;
  }
  pos_ += 2;
  while (!eof()) {
    if (peek() == '\n') {
      get();
      return true;
    }
    if (peek() == '-' && peek(1) == '-') {
      pos_ += 2;
      return true;
    }
    get();
  }
  return true;
}

/**
 *  Function    : skip_whitespace_and_comments
 *  Description : Performs skip whitespace and comments (definition).
 *  Parameters  : none
 *  Returns     : void Lexer::
 */
void Lexer::skip_whitespace_and_comments() {
  while (!eof()) {
    if (is_whitespace(peek())) {
      get();
      continue;
    }
    if (try_skip_comment()) {
      continue;
    }
    break;
  }
}

/**
 *  Function    : lex_identifier_or_keyword
 *  Description : Computes lex identifier or keyword from (none).
 *  Parameters  : none
 *  Returns     : Token Lexer::
 */
Token Lexer::lex_identifier_or_keyword() {
  const std::size_t begin = pos_;
  const char first = get();
  bool last_was_hyphen = false;

  while (!eof()) {
    const char c = peek();
    if (!is_name_continue(c)) {
      break;
    }
    if (c == '-') {
      if (last_was_hyphen) {
        // Consecutive hyphens start a comment; leave them for the next pass.
        break;
      }
      // Look ahead: if next is also '-', this hyphen begins a comment.
      if (peek(1) == '-') {
        break;
      }
      last_was_hyphen = true;
      get();
      continue;
    }
    last_was_hyphen = false;
    get();
  }

  // A name must not end with a hyphen.
  if (last_was_hyphen) {
    --pos_;
  }

  const std::size_t end = pos_;
  const std::string_view text = std::string_view(file_.text()).substr(begin, end - begin);

  if (const auto it = keyword_map().find(text); it != keyword_map().end()) {
    return make(it->second, begin, end);
  }

  if (is_letter(first) && first >= 'A' && first <= 'Z') {
    return make(TokenKind::TypeReference, begin, end);
  }
  return make(TokenKind::Identifier, begin, end);
}

/**
 *  Function    : lex_number
 *  Description : Computes lex number from (none).
 *  Parameters  : none
 *  Returns     : Token Lexer::
 */
Token Lexer::lex_number() {
  const std::size_t begin = pos_;
  while (is_digit(peek())) {
    get();
  }

  // Real: digits '.' digits [exponent]  or  digits exponent.
  // "1..10" must stay as Number then Range, so require a digit after '.'.
  bool is_real = false;
  if (peek() == '.' && is_digit(peek(1))) {
    is_real = true;
    get();  // '.'
    while (is_digit(peek())) {
      get();
    }
  }

  if (peek() == 'e' || peek() == 'E') {
    const char after = peek(1);
    std::size_t exp_digits_at = 1;
    if (after == '+' || after == '-') {
      exp_digits_at = 2;
    }
    if (is_digit(peek(exp_digits_at))) {
      is_real = true;
      get();  // e/E
      if (peek() == '+' || peek() == '-') {
        get();
      }
      while (is_digit(peek())) {
        get();
      }
    }
  }

  return make(is_real ? TokenKind::RealNumber : TokenKind::Number, begin, pos_);
}

/**
 *  Function    : lex_quoted_string
 *  Description : Computes lex quoted string from (none).
 *  Parameters  : none
 *  Returns     : Token Lexer::
 */
Token Lexer::lex_quoted_string() {
  const std::size_t begin = pos_;
  get();  // opening '

  while (!eof()) {
    if (peek() == '\'') {
      get();
      const char suffix = peek();
      if (suffix == 'B' || suffix == 'b') {
        get();
        // Soft-validate binary digits (allow whitespace inside).
        const std::string_view body =
            std::string_view(file_.text()).substr(begin + 1, pos_ - begin - 3);
        for (char c : body) {
          if (!is_binary_digit(c) && !is_whitespace(c)) {
            diagnostics_.error(
                file_.range(begin, pos_),
                "invalid character in binary string (expected 0, 1, or whitespace)");
            break;
          }
        }
        return make(TokenKind::BinaryString, begin, pos_);
      }
      if (suffix == 'H' || suffix == 'h') {
        get();
        const std::string_view body =
            std::string_view(file_.text()).substr(begin + 1, pos_ - begin - 3);
        for (char c : body) {
          if (!is_hex_digit(c) && !is_whitespace(c)) {
            diagnostics_.error(
                file_.range(begin, pos_),
                "invalid character in hexadecimal string");
            break;
          }
        }
        return make(TokenKind::HexString, begin, pos_);
      }
      // Adjacent '' inside a cstring is an escaped quote; keep scanning.
      if (peek() == '\'') {
        get();
        continue;
      }
      // Bare '...' without B/H - treat as incomplete / invalid for Phase 2.
      diagnostics_.error(file_.range(begin, pos_),
                         "quoted string must end with 'B or 'H (use \"...\" for character strings)");
      return make(TokenKind::Invalid, begin, pos_);
    }
    if (peek() == '\n') {
      diagnostics_.error(file_.range(begin, pos_), "unterminated binary/hexadecimal string");
      return make(TokenKind::Invalid, begin, pos_);
    }
    get();
  }

  diagnostics_.error(file_.range(begin, pos_), "unterminated binary/hexadecimal string");
  return make(TokenKind::Invalid, begin, pos_);
}

/**
 *  Function    : lex_punctuation
 *  Description : Computes lex punctuation from (none).
 *  Parameters  : none
 *  Returns     : Token Lexer::
 */
Token Lexer::lex_punctuation() {
  const std::size_t begin = pos_;
  const char c = peek();

  // Multi-character first.
  if (c == ':' && peek(1) == ':' && peek(2) == '=') {
    pos_ += 3;
    return make(TokenKind::Assign, begin, pos_);
  }
  if (c == '.' && peek(1) == '.' && peek(2) == '.') {
    pos_ += 3;
    return make(TokenKind::Ellipsis, begin, pos_);
  }
  if (c == '.' && peek(1) == '.') {
    pos_ += 2;
    return make(TokenKind::Range, begin, pos_);
  }
  if (c == '.' ) {
    get();
    return make(TokenKind::Dot, begin, pos_);
  }
  if (c == '[' && peek(1) == '[') {
    pos_ += 2;
    return make(TokenKind::VersionLBracket, begin, pos_);
  }
  if (c == ']' && peek(1) == ']') {
    pos_ += 2;
    return make(TokenKind::VersionRBracket, begin, pos_);
  }

  get();
  switch (c) {
    case '{':
      return make(TokenKind::LBrace, begin, pos_);
    case '}':
      return make(TokenKind::RBrace, begin, pos_);
    case '[':
      return make(TokenKind::LBracket, begin, pos_);
    case ']':
      return make(TokenKind::RBracket, begin, pos_);
    case '(':
      return make(TokenKind::LParen, begin, pos_);
    case ')':
      return make(TokenKind::RParen, begin, pos_);
    case ',':
      return make(TokenKind::Comma, begin, pos_);
    case ';':
      return make(TokenKind::Semicolon, begin, pos_);
    case '|':
      return make(TokenKind::VerticalBar, begin, pos_);
    case '^':
      return make(TokenKind::Caret, begin, pos_);
    case ':':
      return make(TokenKind::Colon, begin, pos_);
    case '&':
      return make(TokenKind::Ampersand, begin, pos_);
    case '@':
      return make(TokenKind::At, begin, pos_);
    case '!':
      return make(TokenKind::Exclamation, begin, pos_);
    case '<':
      return make(TokenKind::Less, begin, pos_);
    case '>':
      return make(TokenKind::Greater, begin, pos_);
    case '-':
      return make(TokenKind::Minus, begin, pos_);
    default:
      break;
  }

  diagnostics_.error(file_.range(begin, pos_),
                     std::string("unexpected character '") + c + "'");
  return make(TokenKind::Invalid, begin, pos_);
}

/**
 *  Function    : next
 *  Description : Computes next from (none).
 *  Parameters  : none
 *  Returns     : Token Lexer::
 */
Token Lexer::next() {
  skip_whitespace_and_comments();
  if (eof()) {
    return make(TokenKind::EndOfFile, pos_, pos_);
  }

  const char c = peek();

  if (is_letter(c)) {
    return lex_identifier_or_keyword();
  }
  if (is_digit(c)) {
    return lex_number();
  }
  if (c == '"') {
    const std::size_t begin = pos_;
    get();
    while (!eof()) {
      if (peek() == '"') {
        get();
        // ASN.1 escapes a quote as "".
        if (peek() == '"') {
          get();
          continue;
        }
        return make(TokenKind::CharacterString, begin, pos_);
      }
      if (peek() == '\n') {
        diagnostics_.error(file_.range(begin, pos_), "unterminated character string");
        return make(TokenKind::Invalid, begin, pos_);
      }
      get();
    }
    diagnostics_.error(file_.range(begin, pos_), "unterminated character string");
    return make(TokenKind::Invalid, begin, pos_);
  }
  if (c == '\'') {
    return lex_quoted_string();
  }

  return lex_punctuation();
}

}  // namespace asn1
