#include <asn1/frontend/parser.hpp>

#include <string>
#include <utility>

namespace asn1 {
namespace {

SourceRange merge_range(const SourceRange& a, const SourceRange& b) {
  SourceRange r;
  r.begin = a.begin;
  r.end = b.end;
  return r;
}

}  // namespace

Parser::Parser(Lexer& lexer, Diagnostics& diagnostics)
    : lexer_(lexer), diagnostics_(diagnostics), current_(lexer_.next()) {}

Token Parser::advance() {
  Token prev = current_;
  if (current_.kind != TokenKind::EndOfFile) {
    current_ = lexer_.next();
  }
  return prev;
}

bool Parser::check(TokenKind kind) const { return current_.kind == kind; }

bool Parser::match(TokenKind kind) {
  if (!check(kind)) {
    return false;
  }
  advance();
  return true;
}

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

void Parser::error_at(const Token& at, std::string message) {
  diagnostics_.error(at.range, std::move(message));
}

bool Parser::is_unsupported_construct(TokenKind kind) const {
  switch (kind) {
    case TokenKind::KwCLASS:
    case TokenKind::KwINSTANCE:
    case TokenKind::KwSYNTAX:
    case TokenKind::KwTYPE_IDENTIFIER:
    case TokenKind::KwABSTRACT_SYNTAX:
    case TokenKind::KwEMBEDDED:
    case TokenKind::KwEXTERNAL:
    case TokenKind::KwCHARACTER:
      return true;
    default:
      return false;
  }
}

void Parser::report_unsupported(const Token& tok) {
  error_at(tok, std::string("ASN.1 construct '") + std::string(tok.text) +
                    "' is not supported yet");
}

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

std::unique_ptr<ast::Module> Parser::parse_module() {
  if (!check(TokenKind::TypeReference)) {
    error_at(current_, "expected module name (typereference)");
    return nullptr;
  }
  Token name_tok = advance();
  return parse_module_body(name_tok);
}

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
  while (!check(TokenKind::KwEND) && !check(TokenKind::EndOfFile)) {
    auto assignment = parse_assignment();
    if (assignment) {
      assignments.push_back(std::move(assignment));
    } else {
      if (check(TokenKind::KwEND) || check(TokenKind::EndOfFile)) {
        break;
      }
      synchronize_assignment();
      if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
        continue;
      }
      if (!check(TokenKind::KwEND)) {
        advance();
      }
    }
  }

  Token end_tok = current_;
  expect(TokenKind::KwEND, "END");

  SourceRange range = merge_range(name_tok.range, end_tok.range);
  return std::make_unique<ast::Module>(std::move(range), std::string(name_tok.text),
                                       tag_default, extensibility_implied, std::move(exports),
                                       exports_all, std::move(imports),
                                       std::move(assignments));
}

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

void Parser::parse_imports(std::vector<ast::ImportFrom>& imports) {
  advance();  // IMPORTS
  if (match(TokenKind::Semicolon)) {
    return;
  }
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
        imp.symbols.push_back(std::move(sym));
        advance();
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

std::unique_ptr<ast::Assignment> Parser::parse_assignment() {
  if (check(TokenKind::TypeReference)) {
    Token name = advance();
    return parse_type_assignment(name);
  }
  if (check(TokenKind::Identifier)) {
    Token name = advance();
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

std::unique_ptr<ast::Type> Parser::parse_type() {
  auto tag = parse_optional_tag();
  return parse_untagged_type(std::move(tag));
}

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
  } else if (match(TokenKind::KwOBJECT)) {
    if (!expect(TokenKind::KwIDENTIFIER, "IDENTIFIER")) {
      return nullptr;
    }
    type = std::make_unique<ast::ObjectIdentifierType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwRELATIVE_OID)) {
    type = std::make_unique<ast::RelativeOidType>(start.range, std::move(tag), nullptr);
  } else if (match(TokenKind::KwSEQUENCE)) {
    if (match(TokenKind::KwOF)) {
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
    if (match(TokenKind::KwOF)) {
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
             check(TokenKind::KwUniversalString)) {
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
      default:
        break;
    }
    advance();
    type = std::make_unique<ast::StringType>(start.range, std::move(tag), nullptr, kind);
  } else if (check(TokenKind::TypeReference)) {
    std::optional<std::string> module;
    std::string name(current_.text);
    Token name_tok = advance();
    type = std::make_unique<ast::ReferencedType>(name_tok.range, std::move(tag), nullptr,
                                                 std::move(module), std::move(name));
  } else {
    error_at(current_, "expected ASN.1 type");
    return nullptr;
  }

  if (auto c = parse_optional_constraint()) {
    SourceRange range = merge_range(type->range(), c->range());
    type->set_range(range);
    type->set_constraint(std::move(c));
  }
  return type;
}

std::unique_ptr<ast::Constraint> Parser::parse_optional_constraint() {
  if (!check(TokenKind::LParen)) {
    return nullptr;
  }
  Token start = advance();  // (
  auto inner = parse_subtype_constraint();
  Token end = current_;
  expect(TokenKind::RParen, "')'");
  if (!inner) {
    return nullptr;
  }
  // If the constraint already has a range, keep it; else wrap span.
  inner->set_range(merge_range(start.range, end.range));
  return inner;
}

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
  if (match(TokenKind::LParen)) {
    auto inner = parse_subtype_constraint();
    expect(TokenKind::RParen, "')'");
    return inner;
  }
  return parse_constraint_element();
}

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

std::unique_ptr<ast::ComponentItem> Parser::parse_component_item(bool choice) {
  if (check(TokenKind::Ellipsis)) {
    Token e = advance();
    return std::make_unique<ast::ExtensionMarker>(e.range);
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
  if (check(TokenKind::Identifier)) {
    Token tok = advance();
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

}  // namespace asn1
