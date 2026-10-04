/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/frontend/token.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   TokenKind enumeration and Token span into SourceFile.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (lexical and
**                 syntactic notation).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/support/source_location.hpp>

#include <string_view>

namespace asn1 {

/// Lexical token kinds for ASN.1 (X.680). Multi-word constructs such as
/// "OCTET STRING" and "SEQUENCE OF" are separate tokens; the parser combines them.
enum class TokenKind {
  EndOfFile,

  // Identifiers (case of first letter is significant)
  TypeReference,  // Initial capital: MyType, INTEGER is a keyword instead
  Identifier,     // Initial lowercase: myValue

  // Literals
  Number,           // Decimal digit string
  RealNumber,       // e.g. 1.5 or 1E10 (basic forms)
  BinaryString,     // '0101'B
  HexString,        // 'ABCD'H
  CharacterString,  // "..."

  // Punctuation / operators
  Assign,          // ::=
  Range,           // ..
  Ellipsis,        // ...
  VersionLBracket, // [[
  VersionRBracket, // ]]
  LBrace,          // {
  RBrace,          // }
  LBracket,        // [
  RBracket,        // ]
  LParen,          // (
  RParen,          // )
  Comma,           // ,
  Semicolon,       // ;
  VerticalBar,     // |
  Caret,           // ^
  Colon,           // :
  Dot,             // .
  Ampersand,       // &
  At,              // @
  Exclamation,     // !
  Less,            // <
  Greater,         // >
  Minus,           // -

  // Reserved words (single lexical items)
  KwABSENT,
  KwABSTRACT_SYNTAX,
  KwALL,
  KwAPPLICATION,
  KwARRAY,
  KwAS,
  KwATTRIBUTE,
  KwAUTOMATIC,
  KwBASE64,
  KwBEGIN,
  KwBIT,
  KwBMPString,
  KwBOOLEAN,
  KwBY,
  KwCHARACTER,
  KwCHOICE,
  KwCLASS,
  KwCOMPONENT,
  KwCOMPONENTS,
  KwCONSTRAINED,
  KwCONTAINING,
  KwDEFAULT,
  KwDEFINITIONS,
  KwEMBEDDED,
  KwENCODED,
  KwENCODING_CONTROL,
  KwEND,
  KwENUMERATED,
  KwEXCEPT,
  KwEXPLICIT,
  KwEXPORTS,
  KwEXTENDED_XER,
  KwEXTENSIBILITY,
  KwEXTERNAL,
  KwFALSE,
  KwFROM,
  KwGeneralizedTime,
  KwGeneralString,
  KwGraphicString,
  KwIA5String,
  KwIDENTIFIER,
  KwIMPLICIT,
  KwIMPLIED,
  KwIMPORTS,
  KwINCLUDES,
  KwINSTANCE,
  KwINSTRUCTIONS,
  KwINTEGER,
  KwINTERSECTION,
  KwISO646String,
  KwJER,
  KwLIST,
  KwMAX,
  KwMIN,
  KwMINUS_INFINITY,
  KwNAME,
  KwNULL,
  KwNumericString,
  KwOBJECT,
  KwObjectDescriptor,
  KwOCTET,
  KwOF,
  KwOPTIONAL,
  KwPATTERN,
  KwPDV,
  KwPLUS_INFINITY,
  KwPRESENT,
  KwPrintableString,
  KwPRIVATE,
  KwREAL,
  KwRELATIVE_OID,
  KwSEQUENCE,
  KwSET,
  KwSIZE,
  KwSTRING,
  KwSYNTAX,
  KwT61String,
  KwTAGS,
  KwTeletexString,
  KwTRUE,
  KwTYPE_IDENTIFIER,
  KwUNION,
  KwUNIQUE,
  KwUNIVERSAL,
  KwUniversalString,
  KwUNTAGGED,
  KwUNWRAPPED,
  KwUPPERCASED,
  KwUSE_NIL,
  KwUSE_NUMBER,
  KwUTCTime,
  KwUTF8String,
  KwVideotexString,
  KwVisibleString,
  KwWITH,
  KwXER,
  KwCAPITALIZED,
  KwLOWERCASED,
  KwTEXT,

  // Emitted when a character cannot form a valid token (lexer continues).
  Invalid,
};

struct Token {
  TokenKind kind = TokenKind::EndOfFile;
  std::string_view text;  // Points into SourceFile::text()
  SourceRange range;
};

/**
 *  Function    : to_string
 *  Description : Computes to string from (kind).
 *  Parameters  : kind — TokenKind kind
 *  Returns     : const char*
 */
const char* to_string(TokenKind kind);

}  // namespace asn1
