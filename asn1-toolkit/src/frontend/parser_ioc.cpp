#include <asn1/frontend/parser.hpp>

#include <cctype>
#include <utility>

namespace asn1 {
namespace {

SourceRange merge_range(const SourceRange& a, const SourceRange& b) {
  SourceRange r;
  r.begin = a.begin;
  r.end = b.end;
  return r;
}

bool starts_uppercase(const std::string& s) {
  return !s.empty() && std::isupper(static_cast<unsigned char>(s[0]));
}

bool is_all_caps_word(const std::string& s) {
  if (s.empty()) {
    return false;
  }
  bool any_letter = false;
  for (unsigned char uc : s) {
    if (std::isalpha(uc)) {
      any_letter = true;
      if (!std::isupper(uc)) {
        return false;
      }
    } else if (uc != '-' && uc != '_') {
      return false;
    }
  }
  return any_letter;
}

}  // namespace

void Parser::remember_class_syntax(const std::string& class_name,
                                   const std::vector<ast::SyntaxToken>& syntax) {
  class_syntax_[class_name] = syntax;
  for (const auto& tok : syntax) {
    if (!tok.is_field && tok.text != "[" && tok.text != "]" && tok.text != ",") {
      known_syntax_literals_.insert(tok.text);
    }
  }
}

std::unique_ptr<ast::ObjectClassAssignment> Parser::parse_object_class_assignment(
    Token name_tok) {
  if (!expect(TokenKind::Assign, "'::='")) {
    return nullptr;
  }
  auto defn = parse_object_class_defn();
  if (!defn) {
    return nullptr;
  }
  remember_class_syntax(std::string(name_tok.text), defn->with_syntax());
  SourceRange range = merge_range(name_tok.range, defn->range());
  return std::make_unique<ast::ObjectClassAssignment>(std::move(range),
                                                      std::string(name_tok.text),
                                                      std::move(defn));
}

std::unique_ptr<ast::ObjectClassDefn> Parser::parse_object_class_defn() {
  Token start = current_;
  if (!expect(TokenKind::KwCLASS, "CLASS")) {
    return nullptr;
  }
  if (!expect(TokenKind::LBrace, "'{'")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::FieldSpec>> fields;
  if (!check(TokenKind::RBrace)) {
    do {
      if (check(TokenKind::RBrace)) {
        break;
      }
      auto f = parse_field_spec();
      if (!f) {
        break;
      }
      fields.push_back(std::move(f));
    } while (match(TokenKind::Comma));
  }
  Token end = current_;
  expect(TokenKind::RBrace, "'}'");
  std::vector<ast::SyntaxToken> syntax;
  SourceRange end_range = end.range;
  if (match(TokenKind::KwWITH)) {
    expect(TokenKind::KwSYNTAX, "SYNTAX");
    syntax = parse_with_syntax();
    if (!syntax.empty()) {
      end_range = syntax.back().range;
    }
  }
  return std::make_unique<ast::ObjectClassDefn>(merge_range(start.range, end_range),
                                                std::move(fields), std::move(syntax));
}

std::unique_ptr<ast::FieldSpec> Parser::parse_field_spec() {
  if (!expect(TokenKind::Ampersand, "'&'")) {
    return nullptr;
  }
  if (!check(TokenKind::TypeReference) && !check(TokenKind::Identifier)) {
    error_at(current_, "expected field name after '&'");
    return nullptr;
  }
  Token name = advance();
  const bool type_field = name.kind == TokenKind::TypeReference;

  if (type_field) {
    ast::Presence presence = ast::Presence::Mandatory;
    std::unique_ptr<ast::Value> def;  // unused for type fields in Phase 13 defaults
    if (match(TokenKind::KwOPTIONAL)) {
      presence = ast::Presence::Optional;
    } else if (match(TokenKind::KwDEFAULT)) {
      presence = ast::Presence::Default;
      // DEFAULT Type — skip a type for Phase 13 storage simplicity
      auto t = parse_type();
      (void)t;
    }
    SourceRange range = name.range;
    return std::make_unique<ast::FieldSpec>(std::move(range), ast::FieldSpecKind::TypeField,
                                            std::string(name.text), nullptr, false, presence,
                                            nullptr);
  }

  // Fixed-type value field: &name Type [UNIQUE] [OPTIONAL|DEFAULT value]
  auto field_type = parse_type();
  if (!field_type) {
    return nullptr;
  }
  bool unique = match(TokenKind::KwUNIQUE);
  ast::Presence presence = ast::Presence::Mandatory;
  std::unique_ptr<ast::Value> def_value;
  if (match(TokenKind::KwOPTIONAL)) {
    presence = ast::Presence::Optional;
  } else if (match(TokenKind::KwDEFAULT)) {
    presence = ast::Presence::Default;
    def_value = parse_value();
  }
  SourceRange range = merge_range(name.range, field_type->range());
  return std::make_unique<ast::FieldSpec>(std::move(range), ast::FieldSpecKind::FixedTypeValueField,
                                          std::string(name.text), std::move(field_type), unique,
                                          presence, std::move(def_value));
}

std::vector<ast::SyntaxToken> Parser::parse_with_syntax() {
  std::vector<ast::SyntaxToken> tokens;
  if (!expect(TokenKind::LBrace, "'{'")) {
    return tokens;
  }
  while (!check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
    if (match(TokenKind::LBracket)) {
      // Optional group [ ... ] — flatten contents with markers.
      tokens.push_back(ast::SyntaxToken{"[", current_.range, false});
      while (!check(TokenKind::RBracket) && !check(TokenKind::EndOfFile) &&
             !check(TokenKind::RBrace)) {
        if (match(TokenKind::Ampersand)) {
          if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
            Token n = advance();
            tokens.push_back(ast::SyntaxToken{std::string(n.text), n.range, true});
          }
        } else if (check(TokenKind::TypeReference) || check(TokenKind::Identifier) ||
                   check(TokenKind::Comma)) {
          Token t = advance();
          tokens.push_back(ast::SyntaxToken{std::string(t.text), t.range, false});
        } else {
          advance();
        }
      }
      expect(TokenKind::RBracket, "']'");
      tokens.push_back(ast::SyntaxToken{"]", current_.range, false});
      continue;
    }
    if (match(TokenKind::Ampersand)) {
      if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
        Token n = advance();
        tokens.push_back(ast::SyntaxToken{std::string(n.text), n.range, true});
      }
      continue;
    }
    if (check(TokenKind::TypeReference) || check(TokenKind::Identifier) ||
        check(TokenKind::Comma) || check(TokenKind::KwIDENTIFIER) ||
        check(TokenKind::KwBY)) {
      Token t = advance();
      tokens.push_back(ast::SyntaxToken{std::string(t.text), t.range, false});
      continue;
    }
    // Unknown token inside WITH SYNTAX — skip to avoid infinite loop.
    advance();
  }
  expect(TokenKind::RBrace, "'}'");
  return tokens;
}

std::unique_ptr<ast::ObjectAssignment> Parser::parse_object_assignment(Token name_tok,
                                                                      Token class_tok) {
  if (!expect(TokenKind::Assign, "'::='")) {
    return nullptr;
  }
  auto defn = parse_object_defn(std::string(class_tok.text));
  if (!defn) {
    return nullptr;
  }
  SourceRange range = merge_range(name_tok.range, defn->range());
  return std::make_unique<ast::ObjectAssignment>(std::move(range), std::string(name_tok.text),
                                                 std::string(class_tok.text), std::move(defn));
}

std::unique_ptr<ast::ObjectSetAssignment> Parser::parse_object_set_assignment(
    Token name_tok, Token class_tok) {
  if (!expect(TokenKind::Assign, "'::='")) {
    return nullptr;
  }
  const std::string class_name = is_useful_object_class_token(class_tok.kind)
                                     ? useful_object_class_name(class_tok.kind)
                                     : std::string(class_tok.text);
  auto defn = parse_object_set_defn(class_name);
  if (!defn) {
    return nullptr;
  }
  SourceRange range = merge_range(name_tok.range, defn->range());
  return std::make_unique<ast::ObjectSetAssignment>(
      std::move(range), std::string(name_tok.text), class_name, std::move(defn));
}

std::unique_ptr<ast::ObjectDefn> Parser::parse_object_defn(const std::string& class_name) {
  Token start = current_;
  if (!expect(TokenKind::LBrace, "'{'")) {
    return nullptr;
  }

  // Field form: { &field ... }
  if (check(TokenKind::Ampersand)) {
    std::vector<ast::FieldSetting> settings;
    if (!check(TokenKind::RBrace)) {
      do {
        if (check(TokenKind::RBrace)) {
          break;
        }
        if (!expect(TokenKind::Ampersand, "'&'")) {
          break;
        }
        if (!check(TokenKind::TypeReference) && !check(TokenKind::Identifier)) {
          error_at(current_, "expected field name in object");
          break;
        }
        Token fname = advance();
        ast::FieldSetting setting;
        setting.field_name = std::string(fname.text);
        setting.range = fname.range;
        if (fname.kind == TokenKind::TypeReference) {
          setting.type_setting = parse_type();
          if (!setting.type_setting) {
            break;
          }
          setting.range = merge_range(setting.range, setting.type_setting->range());
        } else {
          setting.value_setting = parse_value();
          if (!setting.value_setting) {
            break;
          }
          setting.range = merge_range(setting.range, setting.value_setting->range());
        }
        settings.push_back(std::move(setting));
      } while (match(TokenKind::Comma));
    }
    Token end = current_;
    expect(TokenKind::RBrace, "'}'");
    return std::make_unique<ast::ObjectDefn>(merge_range(start.range, end.range),
                                             std::move(settings));
  }

  // Defined syntax form (class known from earlier in this translation unit).
  auto it = class_syntax_.find(class_name);
  if (it != class_syntax_.end() && !it->second.empty()) {
    auto defn = parse_defined_syntax_object(class_name, it->second);
    if (!defn) {
      return nullptr;
    }
    defn->set_range(merge_range(start.range, defn->range()));
    return defn;
  }

  if (check(TokenKind::RBrace)) {
    Token end = advance();
    return std::make_unique<ast::ObjectDefn>(merge_range(start.range, end.range),
                                             std::vector<ast::FieldSetting>{});
  }

  // Heuristic defined-syntax: used when the CLASS (WITH SYNTAX) appears later in the
  // file (common in 3GPP multi-module ASN.1) or was imported from another module.
  if (check(TokenKind::TypeReference) || check(TokenKind::Identifier)) {
    auto defn = parse_heuristic_defined_syntax_object();
    if (defn) {
      defn->set_range(merge_range(start.range, defn->range()));
      return defn;
    }
  }

  error_at(current_,
           "object definition requires '&field' form or a class WITH SYNTAX definition");
  // Recover: skip to matching '}'.
  int depth = 1;
  while (!check(TokenKind::EndOfFile) && depth > 0) {
    if (check(TokenKind::LBrace)) {
      ++depth;
    } else if (check(TokenKind::RBrace)) {
      --depth;
    }
    advance();
  }
  return nullptr;
}

std::unique_ptr<ast::ObjectDefn> Parser::parse_heuristic_defined_syntax_object() {
  // Pattern used by S1AP/NGAP: ALL-CAPS literal words, then a type or value setting,
  // repeated until '}'. Example:
  //   { ID id-foo CRITICALITY reject TYPE Foo PRESENCE mandatory }
  Token start = current_;
  std::vector<ast::FieldSetting> settings;

  auto is_literal_token = [&]() -> bool {
    if (!(check(TokenKind::TypeReference) || check(TokenKind::Identifier))) {
      return false;
    }
    const std::string text(current_.text);
    if (known_syntax_literals_.count(text) != 0) {
      return true;
    }
    // ALL-CAPS typereferences are almost always WITH SYNTAX literals in 3GPP specs.
    return check(TokenKind::TypeReference) && is_all_caps_word(text);
  };

  while (!check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
    if (!is_literal_token()) {
      error_at(current_, "expected defined-syntax literal word in object");
      break;
    }
    std::string last_literal;
    while (is_literal_token()) {
      last_literal = std::string(current_.text);
      advance();
    }

    ast::FieldSetting setting;
    setting.field_name = last_literal;
    setting.range = current_.range;
    // After literals: TypeReference / built-in type => type setting; else value.
    if (is_type_start_token(current_.kind) && !check(TokenKind::Identifier)) {
      setting.type_setting = parse_type();
      if (!setting.type_setting) {
        break;
      }
      setting.range = merge_range(setting.range, setting.type_setting->range());
    } else {
      setting.value_setting = parse_value();
      if (!setting.value_setting) {
        break;
      }
      setting.range = merge_range(setting.range, setting.value_setting->range());
    }
    settings.push_back(std::move(setting));
  }

  Token end = current_;
  expect(TokenKind::RBrace, "'}'");
  (void)start;
  return std::make_unique<ast::ObjectDefn>(merge_range(start.range, end.range),
                                           std::move(settings));
}

std::unique_ptr<ast::ObjectDefn> Parser::parse_defined_syntax_object(
    const std::string& /*class_name*/, const std::vector<ast::SyntaxToken>& syntax) {
  // Current token is the first token inside '{ ... }' (opening brace already consumed).
  Token start = current_;
  std::vector<ast::FieldSetting> settings;

  auto input_matches_literal = [&](const std::string& lit) -> bool {
    if (lit == ",") {
      return check(TokenKind::Comma);
    }
    if (!(check(TokenKind::Identifier) || check(TokenKind::TypeReference) ||
          check(TokenKind::KwIDENTIFIER) || check(TokenKind::KwBY))) {
      return false;
    }
    return std::string(current_.text) == lit;
  };

  auto skip_optional_group = [&](std::size_t& i) {
    // i points at '[' ; skip until matching ']'
    ++i;
    int depth = 1;
    while (i < syntax.size() && depth > 0) {
      if (!syntax[i].is_field) {
        if (syntax[i].text == "[") {
          ++depth;
        } else if (syntax[i].text == "]") {
          --depth;
        }
      }
      ++i;
    }
  };

  auto syntax_start_matches_input = [&](std::size_t i) -> bool {
    while (i < syntax.size()) {
      const auto& tok = syntax[i];
      if (!tok.is_field && tok.text == "[") {
        // Nested optional — check inside.
        ++i;
        continue;
      }
      if (!tok.is_field && tok.text == "]") {
        return false;
      }
      if (tok.is_field) {
        if (starts_uppercase(tok.text)) {
          return is_type_start_token(current_.kind);
        }
        return check(TokenKind::Number) || check(TokenKind::Minus) ||
               check(TokenKind::KwTRUE) || check(TokenKind::KwFALSE) ||
               check(TokenKind::CharacterString) || check(TokenKind::BinaryString) ||
               check(TokenKind::HexString) || check(TokenKind::Identifier) ||
               check(TokenKind::LBrace);
      }
      return input_matches_literal(tok.text);
    }
    return false;
  };

  std::size_t i = 0;
  while (i < syntax.size() && !check(TokenKind::RBrace) && !check(TokenKind::EndOfFile)) {
    const auto& tok = syntax[i];
    if (!tok.is_field && tok.text == "[") {
      if (syntax_start_matches_input(i + 1)) {
        ++i;  // enter optional group
        continue;
      }
      skip_optional_group(i);
      continue;
    }
    if (!tok.is_field && tok.text == "]") {
      ++i;  // end optional group
      continue;
    }
    if (tok.is_field) {
      ast::FieldSetting setting;
      setting.field_name = tok.text;
      setting.range = current_.range;
      if (starts_uppercase(tok.text)) {
        setting.type_setting = parse_type();
        if (!setting.type_setting) {
          break;
        }
        setting.range = merge_range(setting.range, setting.type_setting->range());
      } else {
        setting.value_setting = parse_value();
        if (!setting.value_setting) {
          break;
        }
        setting.range = merge_range(setting.range, setting.value_setting->range());
      }
      settings.push_back(std::move(setting));
      ++i;
      continue;
    }
    // Literal word(s)
    if (!input_matches_literal(tok.text)) {
      error_at(current_, "expected WITH SYNTAX literal '" + tok.text + "'");
      break;
    }
    advance();
    ++i;
  }

  // Skip any trailing optional groups that were not taken.
  while (i < syntax.size()) {
    if (!syntax[i].is_field && syntax[i].text == "[") {
      skip_optional_group(i);
      continue;
    }
    if (!syntax[i].is_field && syntax[i].text == "]") {
      ++i;
      continue;
    }
    break;
  }

  Token end = current_;
  expect(TokenKind::RBrace, "'}'");
  (void)start;
  return std::make_unique<ast::ObjectDefn>(merge_range(start.range, end.range),
                                           std::move(settings));
}

std::unique_ptr<ast::ObjectSetDefn> Parser::parse_object_set_defn(
    const std::string& class_name) {
  Token start = current_;
  if (!expect(TokenKind::LBrace, "'{'")) {
    return nullptr;
  }
  std::vector<ast::ObjectSetElement> elements;
  bool extensible = false;
  if (!check(TokenKind::RBrace)) {
    do {
      if (check(TokenKind::RBrace)) {
        break;
      }
      if (check(TokenKind::Ellipsis)) {
        advance();
        extensible = true;
        continue;
      }
      ast::ObjectSetElement el;
      el.range = current_.range;
      if (check(TokenKind::Identifier) || check(TokenKind::TypeReference)) {
        Token id = advance();
        el.object_ref = std::string(id.text);
        el.range = id.range;
      } else if (check(TokenKind::LBrace)) {
        el.inline_object = parse_object_defn(class_name);
        if (!el.inline_object) {
          break;
        }
        el.range = el.inline_object->range();
      } else {
        error_at(current_, "expected object reference or object definition in object set");
        break;
      }
      elements.push_back(std::move(el));
    } while (match(TokenKind::VerticalBar) || match(TokenKind::Comma) || match(TokenKind::KwUNION));
  }
  Token end = current_;
  expect(TokenKind::RBrace, "'}'");
  return std::make_unique<ast::ObjectSetDefn>(merge_range(start.range, end.range),
                                              std::move(elements), extensible);
}

std::unique_ptr<ast::Constraint> Parser::parse_table_or_component_constraint() {
  // Called when current is TypeReference (object set name) or '{' inside (...).
  Token start = current_;
  std::string set_name;
  std::unique_ptr<ast::ObjectSetDefn> inline_set;
  if (check(TokenKind::TypeReference)) {
    set_name = std::string(current_.text);
    advance();
  } else if (check(TokenKind::LBrace)) {
    // Look ahead: component relation is { Set }{ @... }
    // Simple table may also be an inline object set { a | b }.
    inline_set = parse_object_set_defn();
    if (!inline_set) {
      return nullptr;
    }
  } else {
    return nullptr;
  }

  if (check(TokenKind::LBrace)) {
    // Component relation: second brace with @components
    Token brace = advance();
    (void)brace;
    std::vector<std::string> ats;
    if (!check(TokenKind::RBrace)) {
      do {
        if (!expect(TokenKind::At, "'@'")) {
          break;
        }
        // Optional leading dots for level: ignore dots for Phase 13
        while (match(TokenKind::Dot)) {
        }
        if (!check(TokenKind::Identifier)) {
          error_at(current_, "expected component identifier after '@'");
          break;
        }
        Token id = advance();
        ats.push_back(std::string(id.text));
      } while (match(TokenKind::Comma));
    }
    Token end = current_;
    expect(TokenKind::RBrace, "'}'");
    return std::make_unique<ast::ComponentRelationConstraint>(
        merge_range(start.range, end.range), std::move(set_name), std::move(inline_set),
        std::move(ats));
  }

  SourceRange range = start.range;
  if (inline_set) {
    range = inline_set->range();
  }
  return std::make_unique<ast::TableConstraint>(std::move(range), std::move(set_name),
                                                std::move(inline_set));
}

}  // namespace asn1
