/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/frontend/parser.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Recursive-descent parser for ASN.1 modules, types, values, constraints.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (lexical and
**                 syntactic notation).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/frontend/parser.hpp>

#include <string>
#include <utility>

namespace asn1 {
namespace {

/**
 *  Function    : merge_range
 *  Description : Computes merge range from (a, b).
 *  Parameters  : a — const SourceRange& a; b — const SourceRange& b
 *  Returns     : SourceRange
 */
SourceRange merge_range(const SourceRange& a, const SourceRange& b) {
  SourceRange r;
  r.begin = a.begin;
  r.end = b.end;
  return r;
}

/**
 *  Function    : is_encoding_instruction_keyword
 *  Description : Returns whether encoding instruction keyword holds for the given inputs.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : bool
 */
bool is_encoding_instruction_keyword(TokenKind kind) {
  switch (kind) {
    case TokenKind::KwARRAY:
    case TokenKind::KwBASE64:
    case TokenKind::KwOBJECT:
    case TokenKind::KwUNWRAPPED:
    case TokenKind::KwNAME:
    case TokenKind::KwTEXT:
    case TokenKind::KwATTRIBUTE:
    case TokenKind::KwUSE_NUMBER:
    case TokenKind::KwLIST:
    case TokenKind::KwUNTAGGED:
    case TokenKind::KwUSE_NIL:
      return true;
    default:
      return false;
  }
}

}  // namespace

/**
 *  Function    : next
 *  Description : Computes next from (lexer, current_(lexer_.next()).
 *  Parameters  : lexer — Lexer& lexer; current_(lexer_.next() — Diagnostics& diagnostics) : lexer_(lexer), diagnostics_(diagnostics), current_(lexer_.next()
 *  Returns     : Parser::Parser(Lexer& lexer, Diagnostics& diagnostics) : lexer_(lexer), diagnostics_(diagnostics), current_(lexer_.
 */
Parser::Parser(Lexer& lexer, Diagnostics& diagnostics)
    : lexer_(lexer), diagnostics_(diagnostics), current_(lexer_.next()) {
  // X.681 useful object classes — builtin WITH SYNTAX for defined-syntax objects.
  remember_class_syntax("TYPE-IDENTIFIER", {
      {"Type", {}, true},
      {"IDENTIFIED", {}, false},
      {"BY", {}, false},
      {"id", {}, true},
  });
  remember_class_syntax("ABSTRACT-SYNTAX", {
      {"Type", {}, true},
      {"IDENTIFIED", {}, false},
      {"BY", {}, false},
      {"id", {}, true},
      {"[", {}, false},
      {"HAS", {}, false},
      {"PROPERTY", {}, false},
      {"property", {}, true},
      {"]", {}, false},
  });
}

/**
 *  Function    : advance
 *  Description : Computes advance from (none).
 *  Parameters  : none
 *  Returns     : Token Parser::
 */
Token Parser::advance() {
  Token prev = current_;
  if (current_.kind != TokenKind::EndOfFile) {
    current_ = lexer_.next();
  }
  return prev;
}

/**
 *  Function    : check
 *  Description : Returns a boolean result from kind.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : bool Parser::
 */
bool Parser::check(TokenKind kind) const { return current_.kind == kind; }

/**
 *  Function    : match
 *  Description : Returns a boolean result from kind.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : bool Parser::
 */
bool Parser::match(TokenKind kind) {
  if (!check(kind)) {
    return false;
  }
  advance();
  return true;
}

/**
 *  Function    : expect
 *  Description : Returns a boolean result from kind, what.
 *  Parameters  : kind — TokenKind kind; what — const char* what
 *  Returns     : bool Parser::
 */
bool Parser::expect(TokenKind kind, const char* what) {
  if (match(kind)) {
    return true;
  }
  error_at(current_, std::string("expected ") + what + ", found '" +
                         std::string(current_.text.empty() ? to_string(current_.kind)
                                                           : current_.text) +
                         "'");
  return false;
}

/**
 *  Function    : error_at
 *  Description : Performs error at (definition).
 *  Parameters  : at — const Token& at; message — std::string message
 *  Returns     : void Parser::
 */
void Parser::error_at(const Token& at, std::string message) {
  diagnostics_.error(at.range, std::move(message));
}

/**
 *  Function    : is_unsupported_construct
 *  Description : Returns whether unsupported construct holds for the given inputs.
 *  Parameters  : / — TokenKind /*kind*/
 *  Returns     : bool Parser::
 */
bool Parser::is_unsupported_construct(TokenKind /*kind*/) const {
  return false;
}

/**
 *  Function    : is_useful_object_class_token
 *  Description : Returns whether useful object class token holds for the given inputs.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : bool Parser::
 */
bool Parser::is_useful_object_class_token(TokenKind kind) const {
  return kind == TokenKind::KwTYPE_IDENTIFIER || kind == TokenKind::KwABSTRACT_SYNTAX;
}

/**
 *  Function    : useful_object_class_name
 *  Description : Builds and returns a string for useful object class name.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : std::string Parser::
 */
std::string Parser::useful_object_class_name(TokenKind kind) const {
  if (kind == TokenKind::KwTYPE_IDENTIFIER) {
    return "TYPE-IDENTIFIER";
  }
  if (kind == TokenKind::KwABSTRACT_SYNTAX) {
    return "ABSTRACT-SYNTAX";
  }
  return {};
}

/**
 *  Function    : report_unsupported
 *  Description : Performs report unsupported (definition).
 *  Parameters  : tok — const Token& tok
 *  Returns     : void Parser::
 */
void Parser::report_unsupported(const Token& tok) {
  error_at(tok, std::string("ASN.1 construct '") + std::string(tok.text) +
                    "' is not supported yet");
}

/**
 *  Function    : synchronize_assignment
 *  Description : Performs synchronize assignment (definition).
 *  Parameters  : none
 *  Returns     : void Parser::
 */
void Parser::synchronize_assignment() {
  while (current_.kind != TokenKind::EndOfFile && current_.kind != TokenKind::KwEND) {
    if (current_.kind == TokenKind::TypeReference ||
        current_.kind == TokenKind::Identifier) {
      return;
    }
    if (current_.kind == TokenKind::Semicolon) {
      advance();
      return;
    }
    advance();
  }
}

/**
 *  Function    : parse_module
 *  Description : Computes parse module from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Module> Parser::
 */
std::unique_ptr<ast::Module> Parser::parse_module() {
  if (!check(TokenKind::TypeReference)) {
    error_at(current_, "expected module name (typereference)");
    return nullptr;
  }
  Token name_tok = advance();
  return parse_module_body(name_tok);
}

/**
 *  Function    : parse_modules
 *  Description : Computes parse modules from (none).
 *  Parameters  : none
 *  Returns     : std::vector<std::unique_ptr<ast::Module>> Parser::
 */
std::vector<std::unique_ptr<ast::Module>> Parser::parse_modules() {
  std::vector<std::unique_ptr<ast::Module>> modules;
  while (check(TokenKind::TypeReference) && !check(TokenKind::EndOfFile)) {
    auto module = parse_module();
    if (!module) {
      break;
    }
    modules.push_back(std::move(module));
  }
  return modules;
}

/**
 *  Function    : parse_module_body
 *  Description : Computes parse module body from (name_tok).
 *  Parameters  : name_tok — Token name_tok
 *  Returns     : std::unique_ptr<ast::Module> Parser::
 */
std::unique_ptr<ast::Module> Parser::parse_module_body(Token name_tok) {
  // Optional object identifier value after module name is skipped as unsupported form
  // if '{' appears before DEFINITIONS - Phase 3: report and skip balanced braces.
  if (check(TokenKind::LBrace)) {
    Token brace = advance();
    int depth = 1;
    while (!check(TokenKind::EndOfFile) && depth > 0) {
      if (check(TokenKind::LBrace)) {
        ++depth;
      } else if (check(TokenKind::RBrace)) {
        --depth;
      }
      advance();
    }
    diagnostics_.warning(brace.range,
                         "module object identifier ignored in Phase 3 (kept for later)");
  }

  if (!expect(TokenKind::KwDEFINITIONS, "DEFINITIONS")) {
    return nullptr;
  }

  bool jer_instructions = false;
  bool xer_instructions = false;
  if (match(TokenKind::KwJER)) {
    if (!expect(TokenKind::KwINSTRUCTIONS, "INSTRUCTIONS")) {
      return nullptr;
    }
    jer_instructions = true;
  } else if (match(TokenKind::KwXER) || match(TokenKind::KwEXTENDED_XER)) {
    if (!expect(TokenKind::KwINSTRUCTIONS, "INSTRUCTIONS")) {
      return nullptr;
    }
    xer_instructions = true;
  }

  ast::TagDefault tag_default = ast::TagDefault::Explicit;
  if (match(TokenKind::KwEXPLICIT)) {
    expect(TokenKind::KwTAGS, "TAGS");
    tag_default = ast::TagDefault::Explicit;
  } else if (match(TokenKind::KwIMPLICIT)) {
    expect(TokenKind::KwTAGS, "TAGS");
    tag_default = ast::TagDefault::Implicit;
  } else if (match(TokenKind::KwAUTOMATIC)) {
    expect(TokenKind::KwTAGS, "TAGS");
    tag_default = ast::TagDefault::Automatic;
  }

  bool extensibility_implied = false;
  if (match(TokenKind::KwEXTENSIBILITY)) {
    expect(TokenKind::KwIMPLIED, "IMPLIED");
    extensibility_implied = true;
  }

  if (!expect(TokenKind::Assign, "'::='")) {
    return nullptr;
  }
  if (!expect(TokenKind::KwBEGIN, "BEGIN")) {
    return nullptr;
  }

  std::vector<std::string> exports;
  bool exports_all = false;
  if (check(TokenKind::KwEXPORTS)) {
    parse_exports(exports, exports_all);
  }

  std::vector<ast::ImportFrom> imports;
  if (check(TokenKind::KwIMPORTS)) {
    parse_imports(imports);
  }

  std::vector<std::unique_ptr<ast::Assignment>> assignments;
  /**
   *  Function    : check
   *  Description : Computes check from (!check(TokenKind::EndOfFile)).
   *  Parameters  : !check(TokenKind::EndOfFile) — !check(TokenKind::KwEND) && !check(TokenKind::KwENCODING_CONTROL) && !check(TokenKind::EndOfFile)
   *  Returns     : while (!check(TokenKind::KwEND) && !check(TokenKind::KwENCODING_CONTROL) && !
   */
  while (!check(TokenKind::KwEND) && !check(TokenKind::KwENCODING_CONTROL) &&
         !check(TokenKind::EndOfFile)) {
    auto assignment = parse_assignment();
    if (assignment) {
      assignments.push_back(std::move(assignment));
    } else {
      if (check(TokenKind::KwEND) || check(TokenKind::KwENCODING_CONTROL) ||
          /**
           *  Function    : check
           *  Description : Performs check (definition).
           *  Parameters  : none
           *  Returns     : void
           */
          check(TokenKind::EndOfFile)) {
        break;
      }
      synchronize_assignment();
      if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
        continue;
      }
      if (!check(TokenKind::KwEND) && !check(TokenKind::KwENCODING_CONTROL)) {
        advance();
      }
    }
  }

  std::vector<ast::EncodingControlClause> jer_control;
  std::vector<ast::EncodingControlClause> xer_control;
  while (check(TokenKind::KwENCODING_CONTROL)) {
    for (auto clause : parse_encoding_control()) {
      if (clause.family == ast::EncodingControlFamily::Jer) {
        jer_control.push_back(std::move(clause));
      } else {
        xer_control.push_back(std::move(clause));
      }
    }
  }

  Token end_tok = current_;
  expect(TokenKind::KwEND, "END");

  SourceRange range = merge_range(name_tok.range, end_tok.range);
  auto module = std::make_unique<ast::Module>(
      std::move(range), std::string(name_tok.text), tag_default, extensibility_implied,
      std::move(exports), exports_all, std::move(imports), std::move(assignments));
  module->set_jer_instructions(jer_instructions);
  module->set_xer_instructions(xer_instructions);
  module->set_jer_encoding_control(std::move(jer_control));
  module->set_xer_encoding_control(std::move(xer_control));
  return module;
}

/**
 *  Function    : parse_exports
 *  Description : Performs parse exports (definition).
 *  Parameters  : exports — std::vector<std::string>& exports; exports_all — bool& exports_all
 *  Returns     : void Parser::
 */
void Parser::parse_exports(std::vector<std::string>& exports, bool& exports_all) {
  advance();  // EXPORTS
  if (match(TokenKind::KwALL)) {
    exports_all = true;
    expect(TokenKind::Semicolon, "';'");
    return;
  }
  if (match(TokenKind::Semicolon)) {
    // Empty exports list means export nothing.
    return;
  }
  do {
    if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
      exports.emplace_back(current_.text);
      advance();
    } else {
      error_at(current_, "expected symbol name in EXPORTS");
      break;
    }
  } while (match(TokenKind::Comma));
  expect(TokenKind::Semicolon, "';'");
}

/**
 *  Function    : parse_imports
 *  Description : Performs parse imports (definition).
 *  Parameters  : imports — std::vector<ast::ImportFrom>& imports
 *  Returns     : void Parser::
 */
void Parser::parse_imports(std::vector<ast::ImportFrom>& imports) {
  advance();  // IMPORTS
  if (match(TokenKind::Semicolon)) {
    return;
  }
  /**
   *  Function    : check
   *  Description : Computes check from (!check(TokenKind::KwEND)).
   *  Parameters  : !check(TokenKind::KwEND) — !check(TokenKind::Semicolon) && !check(TokenKind::EndOfFile) && !check(TokenKind::KwEND)
   *  Returns     : while (!check(TokenKind::Semicolon) && !check(TokenKind::EndOfFile) && !
   */
  while (!check(TokenKind::Semicolon) && !check(TokenKind::EndOfFile) &&
         !check(TokenKind::KwEND)) {
    ast::ImportFrom imp;
    imp.range = current_.range;
    // Symbols: TypeReference | Identifier (comma-separated) FROM Module
    if (!(check(TokenKind::TypeReference) || check(TokenKind::Identifier))) {
      error_at(current_, "expected imported symbol name");
      break;
    }
    do {
      if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
        ast::ImportedSymbol sym;
        sym.name = std::string(current_.text);
        sym.range = current_.range;
        advance();
        // IMPORTS Name{} marks a parameterized reference.
        if (match(TokenKind::LBrace)) {
          expect(TokenKind::RBrace, "'}'");
          sym.parameterized = true;
        }
        imp.symbols.push_back(std::move(sym));
      } else {
        break;
      }
    } while (match(TokenKind::Comma));

    if (!expect(TokenKind::KwFROM, "FROM")) {
      break;
    }
    if (!check(TokenKind::TypeReference)) {
      error_at(current_, "expected module name after FROM");
      break;
    }
    imp.module = std::string(current_.text);
    advance();
    if (check(TokenKind::LBrace)) {
      // Skip OID value.
      int depth = 0;
      do {
        if (check(TokenKind::LBrace)) {
          ++depth;
        }
        if (check(TokenKind::RBrace)) {
          --depth;
        }
        advance();
      } while (depth > 0 && !check(TokenKind::EndOfFile));
    }
    imports.push_back(std::move(imp));
  }
  expect(TokenKind::Semicolon, "';'");
}

/**
 *  Function    : parse_assignment
 *  Description : Computes parse assignment from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Assignment> Parser::
 */
std::unique_ptr<ast::Assignment> Parser::parse_assignment() {
  if (check(TokenKind::TypeReference)) {
    Token name = advance();
    // ObjectSetAssignment: Name ClassName ::= { ... }
    if (check(TokenKind::TypeReference) || is_useful_object_class_token(current_.kind)) {
      Token class_tok = advance();
      return parse_object_set_assignment(name, class_tok);
    }
    // Parameterized type assignment: Name { formals } ::= Type
    if (check(TokenKind::LBrace)) {
      auto formals = parse_formal_parameter_list();
      if (!expect(TokenKind::Assign, "'::='")) {
        return nullptr;
      }
      if (check(TokenKind::KwCLASS)) {
        error_at(current_, "parameterized object class assignments are not supported");
        return nullptr;
      }
      auto type = parse_type();
      if (!type) {
        return nullptr;
      }
      SourceRange range = merge_range(name.range, type->range());
      return std::make_unique<ast::TypeAssignment>(std::move(range), std::string(name.text),
                                                   std::move(type), std::move(formals));
    }
    if (check(TokenKind::Assign)) {
      // Peek: ObjectClassAssignment if next after ::= is CLASS.
      // We only have one-token lookahead; consume Assign then branch.
      advance();  // ::=
      if (check(TokenKind::KwCLASS)) {
        // Re-enter class assignment without re-reading Assign.
        auto defn = parse_object_class_defn();
        if (!defn) {
          return nullptr;
        }
        remember_class_syntax(std::string(name.text), defn->with_syntax());
        SourceRange range = merge_range(name.range, defn->range());
        return std::make_unique<ast::ObjectClassAssignment>(
            std::move(range), std::string(name.text), std::move(defn));
      }
      // Type assignment: already consumed ::=, parse type.
      auto type = parse_type();
      if (!type) {
        return nullptr;
      }
      SourceRange range = merge_range(name.range, type->range());
      return std::make_unique<ast::TypeAssignment>(std::move(range), std::string(name.text),
                                                   std::move(type));
    }
    error_at(current_, "expected '::=' or object class name in assignment");
    return nullptr;
  }
  if (check(TokenKind::Identifier)) {
    Token name = advance();
    // ObjectAssignment: name ClassName ::= { ... }
    if (check(TokenKind::TypeReference) || is_useful_object_class_token(current_.kind)) {
      Token class_tok = current_;
      const std::string class_name = is_useful_object_class_token(class_tok.kind)
                                         ? useful_object_class_name(class_tok.kind)
                                         : std::string(class_tok.text);
      // Ambiguity with ValueAssignment: name Type ::= value
      // Object assignments always use '{' after ::=; value assignments use a value.
      advance();  // class / type name
      if (check(TokenKind::Assign)) {
        Token assign = advance();
        if (check(TokenKind::LBrace)) {
          auto defn = parse_object_defn(class_name);
          if (!defn) {
            return nullptr;
          }
          SourceRange range = merge_range(name.range, defn->range());
          return std::make_unique<ast::ObjectAssignment>(
              std::move(range), std::string(name.text), class_name, std::move(defn));
        }
        // Value assignment: name Type ::= value (Assign already consumed).
        // Useful object class names are not ASN.1 types.
        if (is_useful_object_class_token(class_tok.kind)) {
          error_at(class_tok, "expected object definition '{' after " + class_name);
          return nullptr;
        }
        auto type = std::make_unique<ast::ReferencedType>(
            class_tok.range, std::nullopt, nullptr, std::nullopt, std::string(class_tok.text));
        if (auto c = parse_optional_constraint()) {
          type->set_constraint(std::move(c));
        }
        auto value = parse_value();
        if (!value) {
          return nullptr;
        }
        SourceRange range = merge_range(name.range, value->range());
        (void)assign;
        return std::make_unique<ast::ValueAssignment>(std::move(range), std::string(name.text),
                                                      std::move(type), std::move(value));
      }
      if (is_useful_object_class_token(class_tok.kind)) {
        error_at(class_tok, "expected '::=' after object class " + class_name);
        return nullptr;
      }
      auto type = std::make_unique<ast::ReferencedType>(
          class_tok.range, std::nullopt, nullptr, std::nullopt, std::string(class_tok.text));
      if (auto c = parse_optional_constraint()) {
        type->set_constraint(std::move(c));
      }
      if (!expect(TokenKind::Assign, "'::='")) {
        return nullptr;
      }
      auto value = parse_value();
      if (!value) {
        return nullptr;
      }
      SourceRange range = merge_range(name.range, value->range());
      return std::make_unique<ast::ValueAssignment>(std::move(range), std::string(name.text),
                                                    std::move(type), std::move(value));
    }
    return parse_value_assignment(name);
  }
  if (is_unsupported_construct(current_.kind)) {
    report_unsupported(current_);
    advance();
    return nullptr;
  }
  error_at(current_, "expected type or value assignment");
  return nullptr;
}

/**
 *  Function    : parse_type_assignment
 *  Description : Computes parse type assignment from (name_tok).
 *  Parameters  : name_tok — Token name_tok
 *  Returns     : std::unique_ptr<ast::TypeAssignment> Parser::
 */
std::unique_ptr<ast::TypeAssignment> Parser::parse_type_assignment(Token name_tok) {
  if (!expect(TokenKind::Assign, "'::='")) {
    return nullptr;
  }
  auto type = parse_type();
  if (!type) {
    return nullptr;
  }
  SourceRange range = merge_range(name_tok.range, type->range());
  return std::make_unique<ast::TypeAssignment>(std::move(range), std::string(name_tok.text),
                                               std::move(type));
}

/**
 *  Function    : parse_value_assignment
 *  Description : Computes parse value assignment from (name_tok).
 *  Parameters  : name_tok — Token name_tok
 *  Returns     : std::unique_ptr<ast::ValueAssignment> Parser::
 */
std::unique_ptr<ast::ValueAssignment> Parser::parse_value_assignment(Token name_tok) {
  auto type = parse_type();
  if (!type) {
    return nullptr;
  }
  if (!expect(TokenKind::Assign, "'::='")) {
    return nullptr;
  }
  auto value = parse_value();
  if (!value) {
    return nullptr;
  }
  SourceRange range = merge_range(name_tok.range, value->range());
  return std::make_unique<ast::ValueAssignment>(std::move(range), std::string(name_tok.text),
                                                std::move(type), std::move(value));
}

/**
 *  Function    : parse_type
 *  Description : Computes parse type from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Type> Parser::
 */
std::unique_ptr<ast::Type> Parser::parse_type() {
  std::vector<ast::EncodingInstruction> eis;
  std::optional<ast::Tag> tag;

  while (check(TokenKind::LBracket)) {
    Token start = advance();  // [
    const bool is_ei = is_encoding_instruction_keyword(current_.kind);
    if (is_ei) {
      // Finish encoding instruction ( `[` already consumed ).
      ast::EncodingInstruction ei;
      ei.range = start.range;
      if (match(TokenKind::KwARRAY)) {
        ei.kind = ast::EncodingInstructionKind::Array;
      } else if (match(TokenKind::KwBASE64)) {
        ei.kind = ast::EncodingInstructionKind::Base64;
      } else if (match(TokenKind::KwOBJECT)) {
        ei.kind = ast::EncodingInstructionKind::Object;
      } else if (match(TokenKind::KwUNWRAPPED)) {
        ei.kind = ast::EncodingInstructionKind::Unwrapped;
      } else if (match(TokenKind::KwNAME)) {
        ei.kind = ast::EncodingInstructionKind::Name;
        if (!expect(TokenKind::KwAS, "AS")) {
          return nullptr;
        }
        std::string lit;
        auto tr = parse_name_transform(&lit);
        if (!tr) {
          return nullptr;
        }
        ei.transform = *tr;
        ei.literal = std::move(lit);
      } else if (match(TokenKind::KwTEXT)) {
        ei.kind = ast::EncodingInstructionKind::Text;
        if (match(TokenKind::KwALL)) {
          ei.text_all = true;
          if (!expect(TokenKind::KwAS, "AS")) {
            return nullptr;
          }
          std::string lit;
          auto tr = parse_name_transform(&lit);
          if (!tr) {
            return nullptr;
          }
          ei.transform = *tr;
          ei.literal = std::move(lit);
        } else if (check(TokenKind::Identifier)) {
          ei.text_item = std::string(current_.text);
          advance();
          if (!expect(TokenKind::KwAS, "AS")) {
            return nullptr;
          }
          std::string lit;
          auto tr = parse_name_transform(&lit);
          if (!tr) {
            return nullptr;
          }
          ei.transform = *tr;
          ei.literal = std::move(lit);
        }
      } else if (match(TokenKind::KwATTRIBUTE)) {
        ei.kind = ast::EncodingInstructionKind::Attribute;
      } else if (match(TokenKind::KwUSE_NUMBER)) {
        ei.kind = ast::EncodingInstructionKind::UseNumber;
      } else if (match(TokenKind::KwLIST)) {
        ei.kind = ast::EncodingInstructionKind::List;
      } else if (match(TokenKind::KwUNTAGGED)) {
        ei.kind = ast::EncodingInstructionKind::Untagged;
      } else if (match(TokenKind::KwUSE_NIL)) {
        ei.kind = ast::EncodingInstructionKind::UseNil;
      }
      Token end = current_;
      if (!expect(TokenKind::RBracket, "']'")) {
        return nullptr;
      }
      ei.range = merge_range(start.range, end.range);
      eis.push_back(std::move(ei));
    } else {
      // Classic ASN.1 tag ( `[` already consumed ).
      ast::Tag t;
      t.range = start.range;
      t.cls = ast::TagClass::Context;
      if (match(TokenKind::KwUNIVERSAL)) {
        t.cls = ast::TagClass::Universal;
      } else if (match(TokenKind::KwAPPLICATION)) {
        t.cls = ast::TagClass::Application;
      } else if (match(TokenKind::KwPRIVATE)) {
        t.cls = ast::TagClass::Private;
      }
      if (!check(TokenKind::Number)) {
        error_at(current_, "expected tag number or encoding instruction");
        return nullptr;
      }
      t.number_text = std::string(current_.text);
      Token num = advance();
      if (!expect(TokenKind::RBracket, "']'")) {
        return nullptr;
      }
      if (match(TokenKind::KwIMPLICIT)) {
        t.mode = ast::TagMode::Implicit;
      } else if (match(TokenKind::KwEXPLICIT)) {
        t.mode = ast::TagMode::Explicit;
      }
      t.range = merge_range(start.range, num.range);
      tag = std::move(t);
      // Only one ASN.1 tag is meaningful; stop prefix scan after it.
      break;
    }
  }

  auto type = parse_untagged_type(std::move(tag));
  if (type && !eis.empty()) {
    type->set_encoding_instructions(std::move(eis));
  }
  return type;
}

/**
 *  Function    : parse_name_transform
 *  Description : Computes parse name transform from (literal_out).
 *  Parameters  : literal_out — std::string* literal_out
 *  Returns     : std::optional<ast::NameTransform> Parser::
 */
std::optional<ast::NameTransform> Parser::parse_name_transform(std::string* literal_out) {
  if (match(TokenKind::KwCAPITALIZED)) {
    return ast::NameTransform::Capitalized;
  }
  if (match(TokenKind::KwUPPERCASED)) {
    return ast::NameTransform::Uppercased;
  }
  if (match(TokenKind::KwLOWERCASED)) {
    return ast::NameTransform::Lowercased;
  }
  if (check(TokenKind::CharacterString)) {
    if (literal_out) {
      std::string t(current_.text);
      if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
        t = t.substr(1, t.size() - 2);
      }
      *literal_out = std::move(t);
    }
    advance();
    return ast::NameTransform::Literal;
  }
  error_at(current_, "expected CAPITALIZED, UPPERCASED, LOWERCASED, or character string");
  return std::nullopt;
}

/**
 *  Function    : parse_one_encoding_instruction
 *  Description : Computes parse one encoding instruction from (none).
 *  Parameters  : none
 *  Returns     : std::optional<ast::EncodingInstruction> Parser::
 */
std::optional<ast::EncodingInstruction> Parser::parse_one_encoding_instruction() {
  if (!check(TokenKind::LBracket)) {
    return std::nullopt;
  }
  Token start = advance();  // [
  if (!is_encoding_instruction_keyword(current_.kind)) {
    error_at(current_, "expected encoding instruction after '['");
    return std::nullopt;
  }
  ast::EncodingInstruction ei;
  ei.range = start.range;
  if (match(TokenKind::KwARRAY)) {
    ei.kind = ast::EncodingInstructionKind::Array;
  } else if (match(TokenKind::KwBASE64)) {
    ei.kind = ast::EncodingInstructionKind::Base64;
  } else if (match(TokenKind::KwOBJECT)) {
    ei.kind = ast::EncodingInstructionKind::Object;
  } else if (match(TokenKind::KwUNWRAPPED)) {
    ei.kind = ast::EncodingInstructionKind::Unwrapped;
  } else if (match(TokenKind::KwNAME)) {
    ei.kind = ast::EncodingInstructionKind::Name;
    if (!expect(TokenKind::KwAS, "AS")) {
      return std::nullopt;
    }
    std::string lit;
    auto tr = parse_name_transform(&lit);
    if (!tr) {
      return std::nullopt;
    }
    ei.transform = *tr;
    ei.literal = std::move(lit);
  } else if (match(TokenKind::KwTEXT)) {
    ei.kind = ast::EncodingInstructionKind::Text;
    if (match(TokenKind::KwALL)) {
      ei.text_all = true;
      if (!expect(TokenKind::KwAS, "AS")) {
        return std::nullopt;
      }
      std::string lit;
      auto tr = parse_name_transform(&lit);
      if (!tr) {
        return std::nullopt;
      }
      ei.transform = *tr;
      ei.literal = std::move(lit);
    } else if (check(TokenKind::Identifier)) {
      ei.text_item = std::string(current_.text);
      advance();
      if (!expect(TokenKind::KwAS, "AS")) {
        return std::nullopt;
      }
      std::string lit;
      auto tr = parse_name_transform(&lit);
      if (!tr) {
        return std::nullopt;
      }
      ei.transform = *tr;
      ei.literal = std::move(lit);
    }
  } else if (match(TokenKind::KwATTRIBUTE)) {
    ei.kind = ast::EncodingInstructionKind::Attribute;
  } else if (match(TokenKind::KwUSE_NUMBER)) {
    ei.kind = ast::EncodingInstructionKind::UseNumber;
  } else if (match(TokenKind::KwLIST)) {
    ei.kind = ast::EncodingInstructionKind::List;
  } else if (match(TokenKind::KwUNTAGGED)) {
    ei.kind = ast::EncodingInstructionKind::Untagged;
  } else if (match(TokenKind::KwUSE_NIL)) {
    ei.kind = ast::EncodingInstructionKind::UseNil;
  }
  Token end = current_;
  if (!expect(TokenKind::RBracket, "']'")) {
    return std::nullopt;
  }
  ei.range = merge_range(start.range, end.range);
  return ei;
}

/**
 *  Function    : parse_encoding_instructions
 *  Description : Computes parse encoding instructions from (none).
 *  Parameters  : none
 *  Returns     : std::vector<ast::EncodingInstruction> Parser::
 */
std::vector<ast::EncodingInstruction> Parser::parse_encoding_instructions() {
  std::vector<ast::EncodingInstruction> out;
  while (check(TokenKind::LBracket)) {
    auto ei = parse_one_encoding_instruction();
    if (!ei) {
      break;
    }
    out.push_back(std::move(*ei));
  }
  return out;
}

/**
 *  Function    : parse_encoding_control
 *  Description : Computes parse encoding control from (none).
 *  Parameters  : none
 *  Returns     : std::vector<ast::EncodingControlClause> Parser::
 */
std::vector<ast::EncodingControlClause> Parser::parse_encoding_control() {
  std::vector<ast::EncodingControlClause> clauses;
  advance();  // ENCODING-CONTROL
  ast::EncodingControlFamily family = ast::EncodingControlFamily::Jer;
  if (match(TokenKind::KwJER)) {
    family = ast::EncodingControlFamily::Jer;
  } else if (match(TokenKind::KwXER) || match(TokenKind::KwEXTENDED_XER)) {
    family = ast::EncodingControlFamily::Xer;
  } else {
    error_at(current_, "expected JER, XER, or EXTENDED-XER after ENCODING-CONTROL");
    return clauses;
  }
  while (check(TokenKind::LBracket)) {
    auto ei = parse_one_encoding_instruction();
    if (!ei) {
      break;
    }
    ast::EncodingControlClause clause;
    clause.instruction = std::move(*ei);
    clause.range = clause.instruction.range;
    clause.family = family;
    if (match(TokenKind::KwOCTET)) {
      expect(TokenKind::KwSTRING, "STRING");
      clause.target = ast::EncodingControlTarget::OctetString;
    } else if (match(TokenKind::KwBIT)) {
      expect(TokenKind::KwSTRING, "STRING");
      clause.target = ast::EncodingControlTarget::BitString;
    } else if (match(TokenKind::KwBOOLEAN)) {
      clause.target = ast::EncodingControlTarget::Boolean;
    } else if (match(TokenKind::KwSEQUENCE)) {
      if (match(TokenKind::KwOF)) {
        clause.target = ast::EncodingControlTarget::SequenceOf;
      } else {
        clause.target = ast::EncodingControlTarget::Sequence;
      }
    } else if (match(TokenKind::KwCHOICE)) {
      clause.target = ast::EncodingControlTarget::Choice;
    } else if (match(TokenKind::KwSET)) {
      if (match(TokenKind::KwOF)) {
        clause.target = ast::EncodingControlTarget::SetOf;
      } else {
        clause.target = ast::EncodingControlTarget::Unknown;
      }
    } else if (match(TokenKind::KwENUMERATED)) {
      clause.target = ast::EncodingControlTarget::Enumerated;
    } else {
      error_at(current_, "expected encoding-control target type");
      break;
    }
    clauses.push_back(std::move(clause));
  }
  return clauses;
}

/**
 *  Function    : parse_optional_tag
 *  Description : Computes parse optional tag from (none).
 *  Parameters  : none
 *  Returns     : std::optional<ast::Tag> Parser::
 */
std::optional<ast::Tag> Parser::parse_optional_tag() {
  if (!check(TokenKind::LBracket)) {
    return std::nullopt;
  }
  Token start = advance();  // [
  ast::Tag tag;
  tag.range = start.range;
  tag.cls = ast::TagClass::Context;

  if (match(TokenKind::KwUNIVERSAL)) {
    tag.cls = ast::TagClass::Universal;
  } else if (match(TokenKind::KwAPPLICATION)) {
    tag.cls = ast::TagClass::Application;
  } else if (match(TokenKind::KwPRIVATE)) {
    tag.cls = ast::TagClass::Private;
  }

  if (!check(TokenKind::Number)) {
    error_at(current_, "expected tag number");
    return std::nullopt;
  }
  tag.number_text = std::string(current_.text);
  Token num = advance();

  if (!expect(TokenKind::RBracket, "']'")) {
    return std::nullopt;
  }
  tag.range = merge_range(start.range, current_.range);

  if (match(TokenKind::KwIMPLICIT)) {
    tag.mode = ast::TagMode::Implicit;
  } else if (match(TokenKind::KwEXPLICIT)) {
    tag.mode = ast::TagMode::Explicit;
  }

  tag.range = merge_range(start.range, num.range);
  return tag;
}

/**
 *  Function    : parse_untagged_type
 *  Description : Computes parse untagged type from (tag).
 *  Parameters  : tag — std::optional<ast::Tag> tag
 *  Returns     : std::unique_ptr<ast::Type> Parser::
 */
std::unique_ptr<ast::Type> Parser::parse_untagged_type(std::optional<ast::Tag> tag) {
  Token start = current_;

  if (is_unsupported_construct(current_.kind)) {
    report_unsupported(current_);
    advance();
    return nullptr;
  }

  std::unique_ptr<ast::Type> type;

  if (match(TokenKind::KwBOOLEAN)) {
    type = std::make_unique<ast::BooleanType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwINTEGER)) {
    std::vector<ast::NamedNumber> named;
    if (check(TokenKind::LBrace)) {
      named = parse_named_number_list();
    }
    type = std::make_unique<ast::IntegerType>(start.range, std::move(tag), nullptr,
                                              std::move(named));
  } else if (match(TokenKind::KwENUMERATED)) {
    if (!expect(TokenKind::LBrace, "'{'")) {
      return nullptr;
    }
    std::vector<ast::NamedNumber> root;
    std::vector<ast::NamedNumber> extensions;
    bool extensible = false;
    parse_enumeration_list(root, extensible, extensions);
    Token end = current_;
    expect(TokenKind::RBrace, "'}'");
    SourceRange range = merge_range(start.range, end.range);
    type = std::make_unique<ast::EnumeratedType>(std::move(range), std::move(tag), nullptr,
                                                 std::move(root), extensible,
                                                 std::move(extensions));
  } else if (match(TokenKind::KwBIT)) {
    if (!expect(TokenKind::KwSTRING, "STRING")) {
      return nullptr;
    }
    std::vector<ast::NamedNumber> bits;
    if (check(TokenKind::LBrace)) {
      bits = parse_named_number_list();
    }
    type = std::make_unique<ast::BitStringType>(start.range, std::move(tag), nullptr,
                                                std::move(bits));
  } else if (match(TokenKind::KwOCTET)) {
    if (!expect(TokenKind::KwSTRING, "STRING")) {
      return nullptr;
    }
    type = std::make_unique<ast::OctetStringType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwNULL)) {
    type = std::make_unique<ast::NullType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwREAL)) {
    type = std::make_unique<ast::RealType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwEXTERNAL)) {
    type = std::make_unique<ast::ExternalType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwEMBEDDED)) {
    if (!expect(TokenKind::KwPDV, "PDV")) {
      return nullptr;
    }
    type = std::make_unique<ast::EmbeddedPdvType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwCHARACTER)) {
    if (!expect(TokenKind::KwSTRING, "STRING")) {
      return nullptr;
    }
    type = std::make_unique<ast::CharacterStringType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwOBJECT)) {
    if (!expect(TokenKind::KwIDENTIFIER, "IDENTIFIER")) {
      return nullptr;
    }
    type = std::make_unique<ast::ObjectIdentifierType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwRELATIVE_OID)) {
    type = std::make_unique<ast::RelativeOidType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwINSTANCE)) {
    if (!expect(TokenKind::KwOF, "OF")) {
      return nullptr;
    }
    std::string class_name;
    if (is_useful_object_class_token(current_.kind)) {
      class_name = useful_object_class_name(current_.kind);
      advance();
    } else if (check(TokenKind::TypeReference)) {
      class_name = std::string(current_.text);
      advance();
    } else {
      error_at(current_,
               "expected DefinedObjectClass (TypeReference, TYPE-IDENTIFIER, or ABSTRACT-SYNTAX)");
      return nullptr;
    }
    type = std::make_unique<ast::InstanceOfType>(start.range, std::move(tag), nullptr,
                                                 std::move(class_name));
  } else if (match(TokenKind::KwSEQUENCE)) {
    if (check(TokenKind::LParen)) {
      auto constraint = parse_optional_constraint();
      if (!expect(TokenKind::KwOF, "OF")) {
        return nullptr;
      }
      auto element = parse_type();
      if (!element) {
        return nullptr;
      }
      SourceRange range = merge_range(start.range, element->range());
      type = std::make_unique<ast::SequenceOfType>(std::move(range), std::move(tag),
                                                   std::move(constraint), std::move(element));
    } else if (match(TokenKind::KwOF)) {
      auto element = parse_type();
      if (!element) {
        return nullptr;
      }
      SourceRange range = merge_range(start.range, element->range());
      type = std::make_unique<ast::SequenceOfType>(std::move(range), std::move(tag), nullptr,
                                                   std::move(element));
    } else {
      if (!expect(TokenKind::LBrace, "'{'")) {
        return nullptr;
      }
      auto items = parse_component_type_list(/*choice=*/false);
      Token end = current_;
      expect(TokenKind::RBrace, "'}'");
      SourceRange range = merge_range(start.range, end.range);
      type = std::make_unique<ast::SequenceType>(std::move(range), std::move(tag), nullptr,
                                                 std::move(items));
    }
  } else if (match(TokenKind::KwSET)) {
    if (check(TokenKind::LParen)) {
      auto constraint = parse_optional_constraint();
      if (!expect(TokenKind::KwOF, "OF")) {
        return nullptr;
      }
      auto element = parse_type();
      if (!element) {
        return nullptr;
      }
      SourceRange range = merge_range(start.range, element->range());
      type = std::make_unique<ast::SetOfType>(std::move(range), std::move(tag),
                                              std::move(constraint), std::move(element));
    } else if (match(TokenKind::KwOF)) {
      auto element = parse_type();
      if (!element) {
        return nullptr;
      }
      SourceRange range = merge_range(start.range, element->range());
      type = std::make_unique<ast::SetOfType>(std::move(range), std::move(tag), nullptr,
                                              std::move(element));
    } else {
      if (!expect(TokenKind::LBrace, "'{'")) {
        return nullptr;
      }
      auto items = parse_component_type_list(/*choice=*/false);
      Token end = current_;
      expect(TokenKind::RBrace, "'}'");
      SourceRange range = merge_range(start.range, end.range);
      type = std::make_unique<ast::SetType>(std::move(range), std::move(tag), nullptr,
                                            std::move(items));
    }
  } else if (match(TokenKind::KwCHOICE)) {
    if (!expect(TokenKind::LBrace, "'{'")) {
      return nullptr;
    }
    auto alts = parse_component_type_list(/*choice=*/true);
    Token end = current_;
    expect(TokenKind::RBrace, "'}'");
    SourceRange range = merge_range(start.range, end.range);
    type = std::make_unique<ast::ChoiceType>(std::move(range), std::move(tag), nullptr,
                                             std::move(alts));
  } else if (check(TokenKind::KwUTF8String) || check(TokenKind::KwIA5String) ||
             check(TokenKind::KwPrintableString) || check(TokenKind::KwVisibleString) ||
             check(TokenKind::KwNumericString) || check(TokenKind::KwTeletexString) ||
             check(TokenKind::KwVideotexString) || check(TokenKind::KwGraphicString) ||
             check(TokenKind::KwGeneralString) || check(TokenKind::KwBMPString) ||
             check(TokenKind::KwUniversalString) || check(TokenKind::KwUTCTime) ||
             /**
              *  Function    : check
              *  Description : Performs check (definition).
              *  Parameters  : none
              *  Returns     : void
              */
             check(TokenKind::KwGeneralizedTime)) {
    ast::StringKind kind = ast::StringKind::UTF8String;
    switch (current_.kind) {
      case TokenKind::KwUTF8String:
        kind = ast::StringKind::UTF8String;
        break;
      case TokenKind::KwIA5String:
        kind = ast::StringKind::IA5String;
        break;
      case TokenKind::KwPrintableString:
        kind = ast::StringKind::PrintableString;
        break;
      case TokenKind::KwVisibleString:
        kind = ast::StringKind::VisibleString;
        break;
      case TokenKind::KwNumericString:
        kind = ast::StringKind::NumericString;
        break;
      case TokenKind::KwTeletexString:
        kind = ast::StringKind::TeletexString;
        break;
      case TokenKind::KwVideotexString:
        kind = ast::StringKind::VideotexString;
        break;
      case TokenKind::KwGraphicString:
        kind = ast::StringKind::GraphicString;
        break;
      case TokenKind::KwGeneralString:
        kind = ast::StringKind::GeneralString;
        break;
      case TokenKind::KwBMPString:
        kind = ast::StringKind::BMPString;
        break;
      case TokenKind::KwUniversalString:
        kind = ast::StringKind::UniversalString;
        break;
      case TokenKind::KwUTCTime:
        kind = ast::StringKind::UTCTime;
        break;
      case TokenKind::KwGeneralizedTime:
        kind = ast::StringKind::GeneralizedTime;
        break;
      default:
        break;
    }
    advance();
    type = std::make_unique<ast::StringType>(start.range, std::move(tag), nullptr, kind);
  } else if (check(TokenKind::TypeReference) || is_useful_object_class_token(current_.kind)) {
    std::optional<std::string> module;
    std::string name = is_useful_object_class_token(current_.kind)
                           ? useful_object_class_name(current_.kind)
                           : std::string(current_.text);
    Token name_tok = advance();
    if (match(TokenKind::Dot)) {
      if (match(TokenKind::Ampersand)) {
        if (!check(TokenKind::TypeReference) && !check(TokenKind::Identifier)) {
          error_at(current_, "expected field name after '.&'");
          return nullptr;
        }
        Token field = advance();
        SourceRange range = merge_range(name_tok.range, field.range);
        type = std::make_unique<ast::ObjectClassFieldType>(
            std::move(range), std::move(tag), nullptr, std::move(name), std::string(field.text));
      } else if (check(TokenKind::TypeReference)) {
        // Module.Type external reference
        Token type_tok = advance();
        module = std::move(name);
        name = std::string(type_tok.text);
        SourceRange range = merge_range(name_tok.range, type_tok.range);
        std::vector<ast::ActualParameter> actuals;
        if (check(TokenKind::LBrace)) {
          actuals = parse_actual_parameter_list();
        }
        type = std::make_unique<ast::ReferencedType>(range, std::move(tag), nullptr,
                                                     std::move(module), std::move(name),
                                                     std::move(actuals));
      } else {
        error_at(current_, "expected '&' for object class field or TypeReference after '.'");
        return nullptr;
      }
    } else {
      std::vector<ast::ActualParameter> actuals;
      if (check(TokenKind::LBrace)) {
        actuals = parse_actual_parameter_list();
      }
      type = std::make_unique<ast::ReferencedType>(name_tok.range, std::move(tag), nullptr,
                                                   std::move(module), std::move(name),
                                                   std::move(actuals));
    }
  } else {
    error_at(current_, "expected ASN.1 type");
    return nullptr;
  }

  // Zero or more successive constraints (X.680); combine with intersection.
  std::vector<std::unique_ptr<ast::Constraint>> constraints;
  while (auto c = parse_optional_constraint()) {
    constraints.push_back(std::move(c));
  }
  if (!constraints.empty()) {
    std::unique_ptr<ast::Constraint> combined;
    if (constraints.size() == 1) {
      combined = std::move(constraints[0]);
    } else {
      SourceRange range =
          merge_range(constraints.front()->range(), constraints.back()->range());
      combined = std::make_unique<ast::IntersectionConstraint>(std::move(range),
                                                               std::move(constraints));
    }
    SourceRange range = merge_range(type->range(), combined->range());
    type->set_range(range);
    type->set_constraint(std::move(combined));
  }
  return type;
}

/**
 *  Function    : parse_optional_constraint
 *  Description : Computes parse optional constraint from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_optional_constraint() {
  if (!check(TokenKind::LParen)) {
    return nullptr;
  }
  Token start = advance();  // (
  // ContentsConstraint: (CONTAINING Type ...) or (ENCODED BY ...)
  if (check(TokenKind::KwCONTAINING) || check(TokenKind::KwENCODED)) {
    auto contents = parse_contents_constraint();
    Token end = current_;
    expect(TokenKind::RParen, "')'");
    if (contents) {
      contents->set_range(merge_range(start.range, end.range));
    }
    return contents;
  }
  // Table / component-relation constraints (X.682).
  if (check(TokenKind::TypeReference) || check(TokenKind::LBrace)) {
    // Ambiguity: ({1}) could be weird; prefer table when '{' looks like object set
    // (identifier or nested '{' or ellipsis) or TypeReference as set name.
    bool try_table = check(TokenKind::TypeReference);
    if (check(TokenKind::LBrace)) {
      // Peek one token after '{' without a real peek API: if Identifier / '{' / Ellipsis
      // treat as object set; if Number treat as OID value in single-value — rare in
      // constraint position. Use Identifier/Ampersand/Ellipsis/LBrace as object-set cues.
      // We can't peek; consume via table parser which expects object set form.
      try_table = true;
    }
    if (try_table) {
      auto table = parse_table_or_component_constraint();
      Token end = current_;
      expect(TokenKind::RParen, "')'");
      if (table) {
        table->set_range(merge_range(start.range, end.range));
      }
      return table;
    }
  }
  auto inner = parse_subtype_constraint();
  Token end = current_;
  expect(TokenKind::RParen, "')'");
  if (!inner) {
    return nullptr;
  }
  inner->set_range(merge_range(start.range, end.range));
  return inner;
}

/**
 *  Function    : parse_contents_constraint
 *  Description : Computes parse contents constraint from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_contents_constraint() {
  Token start = current_;
  std::unique_ptr<ast::Type> contained;
  std::unique_ptr<ast::Value> encoded_by;

  if (match(TokenKind::KwCONTAINING)) {
    contained = parse_type();
    if (!contained) {
      return nullptr;
    }
  }
  if (match(TokenKind::KwENCODED)) {
    if (!expect(TokenKind::KwBY, "BY")) {
      return nullptr;
    }
    encoded_by = parse_value();
    if (!encoded_by) {
      return nullptr;
    }
  }
  if (!contained && !encoded_by) {
    error_at(start, "expected CONTAINING Type or ENCODED BY Value");
    return nullptr;
  }
  SourceRange range = start.range;
  if (encoded_by) {
    range = merge_range(range, encoded_by->range());
  } else if (contained) {
    range = merge_range(range, contained->range());
  }
  return std::make_unique<ast::ContentsConstraint>(std::move(range), std::move(contained),
                                                   std::move(encoded_by));
}

/**
 *  Function    : parse_subtype_constraint
 *  Description : Computes parse subtype constraint from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_subtype_constraint() {
  // Union of intersections: elem (| elem)* [, ...]
  std::vector<std::unique_ptr<ast::Constraint>> alts;
  auto first = parse_constraint_intersection();
  if (!first) {
    return nullptr;
  }
  alts.push_back(std::move(first));
  while (match(TokenKind::VerticalBar)) {
    auto next = parse_constraint_intersection();
    if (!next) {
      break;
    }
    alts.push_back(std::move(next));
  }

  std::unique_ptr<ast::Constraint> root;
  if (alts.size() == 1) {
    root = std::move(alts[0]);
  } else {
    SourceRange range = merge_range(alts.front()->range(), alts.back()->range());
    root = std::make_unique<ast::UnionConstraint>(std::move(range), std::move(alts));
  }

  if (match(TokenKind::Comma) && check(TokenKind::Ellipsis)) {
    Token ell = advance();
    SourceRange range = merge_range(root->range(), ell.range);
    return std::make_unique<ast::ExtensibleConstraint>(std::move(range), std::move(root));
  }
  return root;
}

/**
 *  Function    : parse_constraint_intersection
 *  Description : Computes parse constraint intersection from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_constraint_intersection() {
  // atom (^ atom)*   also accept keyword INTERSECTION
  auto first = parse_constraint_atom();
  if (!first) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Constraint>> parts;
  parts.push_back(std::move(first));
  while (match(TokenKind::Caret) || match(TokenKind::KwINTERSECTION)) {
    auto next = parse_constraint_atom();
    if (!next) {
      break;
    }
    parts.push_back(std::move(next));
  }
  if (parts.size() == 1) {
    return std::move(parts[0]);
  }
  SourceRange range = merge_range(parts.front()->range(), parts.back()->range());
  return std::make_unique<ast::IntersectionConstraint>(std::move(range), std::move(parts));
}

/**
 *  Function    : parse_constraint_atom
 *  Description : Computes parse constraint atom from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_constraint_atom() {
  if (check(TokenKind::KwSIZE)) {
    Token size_tok = advance();
    if (!expect(TokenKind::LParen, "'(' after SIZE")) {
      return nullptr;
    }
    auto inner = parse_subtype_constraint();
    Token end = current_;
    expect(TokenKind::RParen, "')'");
    if (!inner) {
      return nullptr;
    }
    SourceRange range = merge_range(size_tok.range, end.range);
    return std::make_unique<ast::SizeConstraint>(std::move(range), std::move(inner));
  }
  if (check(TokenKind::KwWITH)) {
    return parse_with_components_constraint();
  }
  if (check(TokenKind::KwCONTAINING) || check(TokenKind::KwENCODED)) {
    return parse_contents_constraint();
  }
  if (match(TokenKind::LParen)) {
    auto inner = parse_subtype_constraint();
    expect(TokenKind::RParen, "')'");
    return inner;
  }
  return parse_constraint_element();
}

/**
 *  Function    : parse_with_components_constraint
 *  Description : Computes parse with components constraint from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_with_components_constraint() {
  Token start = current_;
  if (!expect(TokenKind::KwWITH, "WITH")) {
    return nullptr;
  }
  if (!expect(TokenKind::KwCOMPONENTS, "COMPONENTS")) {
    return nullptr;
  }
  if (!expect(TokenKind::LBrace, "'{' after WITH COMPONENTS")) {
    return nullptr;
  }
  std::vector<ast::NamedComponentConstraint> components;
  if (!check(TokenKind::RBrace)) {
    do {
      if (check(TokenKind::RBrace)) {
        break;
      }
      if (!check(TokenKind::Identifier)) {
        error_at(current_, "expected component identifier in WITH COMPONENTS");
        return nullptr;
      }
      ast::NamedComponentConstraint nc;
      nc.name = std::string(current_.text);
      nc.range = current_.range;
      advance();
      if (match(TokenKind::LParen)) {
        nc.value_constraint = parse_subtype_constraint();
        if (!nc.value_constraint) {
          return nullptr;
        }
        expect(TokenKind::RParen, "')' after component constraint");
      }
      if (match(TokenKind::KwPRESENT)) {
        nc.presence = ast::ComponentPresence::Present;
      } else if (match(TokenKind::KwABSENT)) {
        nc.presence = ast::ComponentPresence::Absent;
      } else if (match(TokenKind::KwOPTIONAL)) {
        nc.presence = ast::ComponentPresence::Optional;
      }
      if (nc.value_constraint) {
        nc.range = merge_range(nc.range, nc.value_constraint->range());
      }
      components.push_back(std::move(nc));
    } while (match(TokenKind::Comma));
  }
  Token end = current_;
  if (!expect(TokenKind::RBrace, "'}' after WITH COMPONENTS")) {
    return nullptr;
  }
  return std::make_unique<ast::WithComponentsConstraint>(merge_range(start.range, end.range),
                                                         std::move(components));
}

/**
 *  Function    : parse_constraint_element
 *  Description : Computes parse constraint element from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Constraint> Parser::
 */
std::unique_ptr<ast::Constraint> Parser::parse_constraint_element() {
  if (check(TokenKind::Ellipsis)) {
    Token e = advance();
    return std::make_unique<ast::ExtensibleConstraint>(e.range, nullptr);
  }

  bool lower_min = false;
  std::unique_ptr<ast::Value> lower;
  Token start = current_;

  if (match(TokenKind::KwMIN)) {
    lower_min = true;
  } else {
    lower = parse_value();
    if (!lower) {
      return nullptr;
    }
  }

  if (match(TokenKind::Range)) {
    bool upper_max = false;
    std::unique_ptr<ast::Value> upper;
    Token end = current_;
    if (match(TokenKind::KwMAX)) {
      upper_max = true;
      end = Token{TokenKind::KwMAX, "MAX", end.range};
    } else {
      upper = parse_value();
      if (!upper) {
        error_at(current_, "expected upper bound of value range");
        return nullptr;
      }
      end.range = upper->range();
    }
    SourceRange range = merge_range(start.range, end.range);
    return std::make_unique<ast::ValueRangeConstraint>(
        std::move(range), lower_min ? nullptr : std::move(lower),
        upper_max ? nullptr : std::move(upper));
  }

  if (lower_min) {
    error_at(start, "MIN must be used in a value range (MIN .. ...)");
    return nullptr;
  }
  SourceRange range = lower->range();
  return std::make_unique<ast::SingleValueConstraint>(std::move(range), std::move(lower));
}


void Parser::parse_enumeration_list(std::vector<ast::NamedNumber>& root, bool& extensible,
                                    std::vector<ast::NamedNumber>& extensions) {
  extensible = false;
  std::vector<ast::NamedNumber>* dest = &root;
  if (check(TokenKind::RBrace)) {
    return;
  }
  do {
    if (check(TokenKind::RBrace)) {
      break;
    }
    if (check(TokenKind::Ellipsis)) {
      advance();
      extensible = true;
      dest = &extensions;
      continue;
    }
    if (!check(TokenKind::Identifier)) {
      error_at(current_, "expected enumeration identifier");
      break;
    }
    ast::NamedNumber nn;
    nn.name = std::string(current_.text);
    nn.range = current_.range;
    advance();
    if (match(TokenKind::LParen)) {
      if (match(TokenKind::Minus)) {
        nn.negative = true;
      }
      if (!check(TokenKind::Number)) {
        error_at(current_, "expected enumeration number");
        break;
      }
      nn.number_text = std::string(current_.text);
      nn.range = merge_range(nn.range, current_.range);
      advance();
      expect(TokenKind::RParen, "')'");
    }
    dest->push_back(std::move(nn));
  } while (match(TokenKind::Comma));
}

/**
 *  Function    : parse_object_identifier_value
 *  Description : Computes parse object identifier value from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Value> Parser::
 */
std::unique_ptr<ast::Value> Parser::parse_object_identifier_value() {
  Token start = advance();  // {
  std::vector<ast::ObjectIdentifierValue::Arc> arcs;
  while (!check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
    ast::ObjectIdentifierValue::Arc arc;
    if (check(TokenKind::Number)) {
      try {
        arc.number = std::stoull(std::string(current_.text));
      } catch (...) {
        error_at(current_, "invalid OID arc number");
      }
      advance();
    } else if (check(TokenKind::Identifier)) {
      arc.name = std::string(current_.text);
      advance();
      if (match(TokenKind::LParen)) {
        if (!check(TokenKind::Number)) {
          error_at(current_, "expected OID arc number");
          break;
        }
        try {
          arc.number = std::stoull(std::string(current_.text));
        } catch (...) {
          error_at(current_, "invalid OID arc number");
        }
        advance();
        expect(TokenKind::RParen, "')'");
      }
    } else {
      error_at(current_, "expected OID arc");
      break;
    }
    arcs.push_back(std::move(arc));
  }
  Token end = current_;
  expect(TokenKind::RBrace, "'}'");
  return std::make_unique<ast::ObjectIdentifierValue>(merge_range(start.range, end.range),
                                                      std::move(arcs));
}

/**
 *  Function    : parse_named_number_list
 *  Description : Computes parse named number list from (none).
 *  Parameters  : none
 *  Returns     : std::vector<ast::NamedNumber> Parser::
 */
std::vector<ast::NamedNumber> Parser::parse_named_number_list() {
  std::vector<ast::NamedNumber> list;
  expect(TokenKind::LBrace, "'{'");
  if (check(TokenKind::RBrace)) {
    advance();
    return list;
  }
  do {
    if (!check(TokenKind::Identifier)) {
      error_at(current_, "expected identifier in named number list");
      break;
    }
    ast::NamedNumber nn;
    nn.name = std::string(current_.text);
    nn.range = current_.range;
    advance();
    expect(TokenKind::LParen, "'('");
    if (match(TokenKind::Minus)) {
      nn.negative = true;
    }
    if (!check(TokenKind::Number)) {
      error_at(current_, "expected number in named number");
      break;
    }
    nn.number_text = std::string(current_.text);
    nn.range = merge_range(nn.range, current_.range);
    advance();
    expect(TokenKind::RParen, "')'");
    list.push_back(std::move(nn));
  } while (match(TokenKind::Comma));
  expect(TokenKind::RBrace, "'}'");
  return list;
}

std::vector<std::unique_ptr<ast::ComponentItem>> Parser::parse_component_type_list(
    bool choice) {
  std::vector<std::unique_ptr<ast::ComponentItem>> items;
  if (check(TokenKind::RBrace)) {
    return items;
  }
  do {
    // Trailing comma before } is not standard; allow extension marker alone.
    if (check(TokenKind::RBrace)) {
      break;
    }
    auto item = parse_component_item(choice);
    if (item) {
      items.push_back(std::move(item));
    } else {
      break;
    }
  } while (match(TokenKind::Comma));
  return items;
}

/**
 *  Function    : parse_component_item
 *  Description : Computes parse component item from (choice).
 *  Parameters  : choice — bool choice
 *  Returns     : std::unique_ptr<ast::ComponentItem> Parser::
 */
std::unique_ptr<ast::ComponentItem> Parser::parse_component_item(bool choice) {
  if (check(TokenKind::Ellipsis)) {
    Token e = advance();
    return std::make_unique<ast::ExtensionMarker>(e.range);
  }
  if (check(TokenKind::VersionLBracket)) {
    return parse_version_addition_group(choice);
  }

  if (!check(TokenKind::Identifier)) {
    error_at(current_, choice ? "expected CHOICE alternative name"
                              : "expected SEQUENCE/SET component name");
    return nullptr;
  }
  Token name = advance();
  auto type = parse_type();
  if (!type) {
    return nullptr;
  }

  ast::Presence presence = ast::Presence::Mandatory;
  std::unique_ptr<ast::Value> default_value;
  if (!choice) {
    if (match(TokenKind::KwOPTIONAL)) {
      presence = ast::Presence::Optional;
    } else if (match(TokenKind::KwDEFAULT)) {
      presence = ast::Presence::Default;
      default_value = parse_value();
      if (!default_value) {
        return nullptr;
      }
    }
  }

  SourceRange range = merge_range(name.range, type->range());
  if (default_value) {
    range = merge_range(range, default_value->range());
  }
  return std::make_unique<ast::Component>(std::move(range), std::string(name.text),
                                          std::move(type), presence,
                                          std::move(default_value));
}

/**
 *  Function    : parse_version_addition_group
 *  Description : Computes parse version addition group from (choice).
 *  Parameters  : choice — bool choice
 *  Returns     : std::unique_ptr<ast::ComponentItem> Parser::
 */
std::unique_ptr<ast::ComponentItem> Parser::parse_version_addition_group(bool choice) {
  Token start = current_;
  if (!expect(TokenKind::VersionLBracket, "'[['")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::ComponentItem>> items;
  if (!check(TokenKind::VersionRBracket)) {
    do {
      if (check(TokenKind::VersionRBracket)) {
        break;
      }
      // Nested version groups / ellipsis are unusual inside [[ ]]; allow components only.
      if (check(TokenKind::Ellipsis) || check(TokenKind::VersionLBracket)) {
        error_at(current_, "unexpected token inside version brackets");
        break;
      }
      auto item = parse_component_item(choice);
      if (!item) {
        break;
      }
      items.push_back(std::move(item));
    } while (match(TokenKind::Comma));
  }
  Token end = current_;
  expect(TokenKind::VersionRBracket, "']]'");
  return std::make_unique<ast::VersionAdditionGroup>(merge_range(start.range, end.range),
                                                     std::move(items));
}

/**
 *  Function    : parse_integer_value
 *  Description : Computes parse integer value from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Value> Parser::
 */
std::unique_ptr<ast::Value> Parser::parse_integer_value() {
  Token start = current_;
  bool negative = match(TokenKind::Minus);
  if (!check(TokenKind::Number)) {
    error_at(current_, "expected integer value");
    return nullptr;
  }
  Token num = advance();
  SourceRange range = negative ? merge_range(start.range, num.range) : num.range;
  return std::make_unique<ast::IntegerValue>(std::move(range), std::string(num.text),
                                             negative);
}

/**
 *  Function    : parse_value
 *  Description : Computes parse value from (none).
 *  Parameters  : none
 *  Returns     : std::unique_ptr<ast::Value> Parser::
 */
std::unique_ptr<ast::Value> Parser::parse_value() {
  if (check(TokenKind::KwTRUE) || check(TokenKind::KwFALSE)) {
    Token tok = advance();
    return std::make_unique<ast::BooleanValue>(tok.range, tok.kind == TokenKind::KwTRUE);
  }
  if (check(TokenKind::Minus) || check(TokenKind::Number)) {
    return parse_integer_value();
  }
  if (check(TokenKind::CharacterString)) {
    Token tok = advance();
    return std::make_unique<ast::StringValue>(tok.range, std::string(tok.text));
  }
  if (check(TokenKind::BinaryString)) {
    Token tok = advance();
    return std::make_unique<ast::BitOrOctetValue>(tok.range, ast::BitOrOctetKind::Binary,
                                                  std::string(tok.text));
  }
  if (check(TokenKind::HexString)) {
    Token tok = advance();
    return std::make_unique<ast::BitOrOctetValue>(tok.range, ast::BitOrOctetKind::Hex,
                                                  std::string(tok.text));
  }
  if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
    Token tok = advance();
    if (match(TokenKind::Dot)) {
      if (!check(TokenKind::Identifier) && !check(TokenKind::TypeReference)) {
        error_at(current_, "expected identifier after '.' in value reference");
        return nullptr;
      }
      Token member = advance();
      return std::make_unique<ast::ValueReference>(merge_range(tok.range, member.range),
                                                   std::string(member.text));
    }
    return std::make_unique<ast::ValueReference>(tok.range, std::string(tok.text));
  }
  if (check(TokenKind::LBrace)) {
    Token start = advance();  // {
    if (check(TokenKind::RBrace)) {
      Token end = current_;
      advance();
      return std::make_unique<ast::NamedValueList>(
          merge_range(start.range, end.range),
          std::vector<std::unique_ptr<ast::NamedValue>>{});
    }

    auto finish_oid = [&](std::vector<ast::ObjectIdentifierValue::Arc> arcs) {
      Token end = current_;
      expect(TokenKind::RBrace, "'}'");
      return std::make_unique<ast::ObjectIdentifierValue>(merge_range(start.range, end.range),
                                                          std::move(arcs));
    };
    auto parse_remaining_oid_arcs =
        [&](std::vector<ast::ObjectIdentifierValue::Arc>& arcs) {
          while (!check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
            ast::ObjectIdentifierValue::Arc arc;
            if (check(TokenKind::Number)) {
              try {
                arc.number = std::stoull(std::string(current_.text));
              } catch (...) {
                error_at(current_, "invalid OID arc number");
              }
              advance();
            } else if (check(TokenKind::Identifier)) {
              arc.name = std::string(current_.text);
              advance();
              if (match(TokenKind::LParen)) {
                if (!check(TokenKind::Number)) {
                  error_at(current_, "expected OID arc number");
                  return false;
                }
                try {
                  arc.number = std::stoull(std::string(current_.text));
                } catch (...) {
                  error_at(current_, "invalid OID arc number");
                }
                advance();
                expect(TokenKind::RParen, "')'");
              }
            } else {
              error_at(current_, "expected OID arc");
              return false;
            }
            arcs.push_back(std::move(arc));
          }
          return true;
        };

    // OBJECT IDENTIFIER: number-first or identifier(number).
    // NamedValueList: identifier followed by a value (not '(').
    if (check(TokenKind::Number)) {
      std::vector<ast::ObjectIdentifierValue::Arc> arcs;
      if (!parse_remaining_oid_arcs(arcs)) {
        return nullptr;
      }
      return finish_oid(std::move(arcs));
    }
    if (check(TokenKind::Identifier)) {
      Token n = advance();
      if (check(TokenKind::LParen)) {
        std::vector<ast::ObjectIdentifierValue::Arc> arcs;
        ast::ObjectIdentifierValue::Arc first;
        first.name = std::string(n.text);
        advance();  // (
        if (!check(TokenKind::Number)) {
          error_at(current_, "expected OID arc number");
          return nullptr;
        }
        try {
          first.number = std::stoull(std::string(current_.text));
        } catch (...) {
          error_at(current_, "invalid OID arc number");
        }
        advance();
        expect(TokenKind::RParen, "')'");
        arcs.push_back(std::move(first));
        if (!parse_remaining_oid_arcs(arcs)) {
          return nullptr;
        }
        return finish_oid(std::move(arcs));
      }
      std::vector<std::unique_ptr<ast::NamedValue>> values;
      auto v = parse_value();
      if (!v) {
        return nullptr;
      }
      SourceRange r = merge_range(n.range, v->range());
      values.push_back(std::make_unique<ast::NamedValue>(std::move(r), std::string(n.text),
                                                         std::move(v)));
      while (match(TokenKind::Comma)) {
        if (!check(TokenKind::Identifier)) {
          error_at(current_, "expected named value or '}'");
          break;
        }
        Token n2 = advance();
        auto v2 = parse_value();
        if (!v2) {
          break;
        }
        SourceRange r2 = merge_range(n2.range, v2->range());
        values.push_back(std::make_unique<ast::NamedValue>(
            std::move(r2), std::string(n2.text), std::move(v2)));
      }
      Token end = current_;
      expect(TokenKind::RBrace, "'}'");
      return std::make_unique<ast::NamedValueList>(merge_range(start.range, end.range),
                                                   std::move(values));
    }
    error_at(current_, "expected named value, OID arc, or '}'");
    return nullptr;
  }
  error_at(current_, "expected ASN.1 value");
  return nullptr;
}

/**
 *  Function    : is_governor_type_token
 *  Description : Returns whether governor type token holds for the given inputs.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : bool Parser::
 */
bool Parser::is_governor_type_token(TokenKind kind) const {
  switch (kind) {
    case TokenKind::TypeReference:
    case TokenKind::KwBOOLEAN:
    case TokenKind::KwINTEGER:
    case TokenKind::KwBIT:
    case TokenKind::KwOCTET:
    case TokenKind::KwNULL:
    case TokenKind::KwREAL:
    case TokenKind::KwEXTERNAL:
    case TokenKind::KwEMBEDDED:
    case TokenKind::KwCHARACTER:
    case TokenKind::KwENUMERATED:
    case TokenKind::KwSEQUENCE:
    case TokenKind::KwSET:
    case TokenKind::KwCHOICE:
    case TokenKind::KwOBJECT:
    case TokenKind::KwRELATIVE_OID:
    case TokenKind::KwUTF8String:
    case TokenKind::KwIA5String:
    case TokenKind::KwPrintableString:
    case TokenKind::KwVisibleString:
    case TokenKind::KwNumericString:
    case TokenKind::KwTeletexString:
    case TokenKind::KwVideotexString:
    case TokenKind::KwGraphicString:
    case TokenKind::KwGeneralString:
    case TokenKind::KwBMPString:
    case TokenKind::KwUniversalString:
    case TokenKind::KwUTCTime:
    case TokenKind::KwGeneralizedTime:
    case TokenKind::KwTYPE_IDENTIFIER:
    case TokenKind::KwABSTRACT_SYNTAX:
      return true;
    default:
      return false;
  }
}

/**
 *  Function    : is_type_start_token
 *  Description : Returns whether type start token holds for the given inputs.
 *  Parameters  : kind — TokenKind kind
 *  Returns     : bool Parser::
 */
bool Parser::is_type_start_token(TokenKind kind) const {
  return is_governor_type_token(kind) || kind == TokenKind::LBracket ||
         kind == TokenKind::KwINSTANCE;
}

/**
 *  Function    : consume_governor_name
 *  Description : Builds and returns a string for consume governor name.
 *  Parameters  : none
 *  Returns     : std::string Parser::
 */
std::string Parser::consume_governor_name() {
  if (check(TokenKind::KwBIT)) {
    advance();
    expect(TokenKind::KwSTRING, "STRING");
    return "BIT STRING";
  }
  if (check(TokenKind::KwOCTET)) {
    advance();
    expect(TokenKind::KwSTRING, "STRING");
    return "OCTET STRING";
  }
  if (check(TokenKind::KwOBJECT)) {
    advance();
    expect(TokenKind::KwIDENTIFIER, "IDENTIFIER");
    return "OBJECT IDENTIFIER";
  }
  std::string name = std::string(current_.text);
  advance();
  return name;
}

/**
 *  Function    : parse_formal_parameter_list
 *  Description : Computes parse formal parameter list from (none).
 *  Parameters  : none
 *  Returns     : std::vector<ast::FormalParameter> Parser::
 */
std::vector<ast::FormalParameter> Parser::parse_formal_parameter_list() {
  std::vector<ast::FormalParameter> formals;
  if (!expect(TokenKind::LBrace, "'{'")) {
    return formals;
  }
  if (!check(TokenKind::RBrace)) {
    do {
      if (check(TokenKind::RBrace)) {
        break;
      }
      ast::FormalParameter fp;
      if (is_governor_type_token(current_.kind)) {
        // Governor : Dummy  OR  Dummy alone (TypeReference type parameter)
        Token first = current_;
        std::string first_name = consume_governor_name();
        if (match(TokenKind::Colon)) {
          fp.governor = std::move(first_name);
          if (!check(TokenKind::TypeReference) && !check(TokenKind::Identifier)) {
            error_at(current_, "expected dummy reference after ':'");
            break;
          }
          fp.type_ref_name = check(TokenKind::TypeReference);
          fp.name = std::string(current_.text);
          advance();
        } else {
          // DummyReference alone — must be a type parameter (TypeReference).
          if (first.kind != TokenKind::TypeReference) {
            error_at(first, "expected TypeReference dummy parameter or Governor : Dummy");
            break;
          }
          fp.name = std::move(first_name);
          fp.type_ref_name = true;
        }
      } else if (check(TokenKind::Identifier)) {
        fp.name = std::string(current_.text);
        fp.type_ref_name = false;
        advance();
      } else {
        error_at(current_, "expected formal parameter");
        break;
      }
      formals.push_back(std::move(fp));
    } while (match(TokenKind::Comma));
  }
  expect(TokenKind::RBrace, "'}'");
  return formals;
}

/**
 *  Function    : parse_actual_parameter_list
 *  Description : Computes parse actual parameter list from (none).
 *  Parameters  : none
 *  Returns     : std::vector<ast::ActualParameter> Parser::
 */
std::vector<ast::ActualParameter> Parser::parse_actual_parameter_list() {
  std::vector<ast::ActualParameter> actuals;
  if (!expect(TokenKind::LBrace, "'{'")) {
    return actuals;
  }
  if (!check(TokenKind::RBrace)) {
    do {
      if (check(TokenKind::RBrace)) {
        break;
      }
      ast::ActualParameter ap;
      if (check(TokenKind::LBrace)) {
        // Object set actual — may be {{Name}} nested.
        auto set = parse_object_set_defn();
        if (!set) {
          break;
        }
        ap.inline_object_set = true;
        if (set->elements().size() == 1 && set->elements()[0].object_ref) {
          ap.object_set_name = *set->elements()[0].object_ref;
        }
      } else if (check(TokenKind::Number) || check(TokenKind::Minus) ||
                 check(TokenKind::KwTRUE) || check(TokenKind::KwFALSE) ||
                 check(TokenKind::CharacterString) || check(TokenKind::BinaryString) ||
                 /**
                  *  Function    : check
                  *  Description : Computes check from (none).
                  *  Parameters  : none
                  *  Returns     : check(TokenKind::HexString) ||
                  */
                 check(TokenKind::HexString) || check(TokenKind::Identifier)) {
        ap.value = parse_value();
        if (!ap.value) {
          break;
        }
      } else if (is_type_start_token(current_.kind)) {
        ap.type = parse_type();
        if (!ap.type) {
          break;
        }
      } else {
        error_at(current_, "expected actual parameter (type, value, or object set)");
        break;
      }
      actuals.push_back(std::move(ap));
    } while (match(TokenKind::Comma));
  }
  expect(TokenKind::RBrace, "'}'");
  return actuals;
}

}  // namespace asn1
