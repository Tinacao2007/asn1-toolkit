#pragma once

#include <asn1/ast/module.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/support/diagnostics.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {

/// Recursive-descent ASN.1 parser. Builds an AST; does not resolve symbols or tags.
class Parser {
 public:
  Parser(Lexer& lexer, Diagnostics& diagnostics);

  /// Parse a single ModuleDefinition. Returns nullptr on hard failure.
  std::unique_ptr<ast::Module> parse_module();

 private:
  Token peek() const { return current_; }
  Token advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  bool expect(TokenKind kind, const char* what);

  void error_at(const Token& at, std::string message);
  void synchronize_assignment();

  std::unique_ptr<ast::Module> parse_module_body(Token name_tok);
  void parse_exports(std::vector<std::string>& exports, bool& exports_all);
  void parse_imports(std::vector<ast::ImportFrom>& imports);
  std::unique_ptr<ast::Assignment> parse_assignment();
  std::unique_ptr<ast::TypeAssignment> parse_type_assignment(Token name_tok);
  std::unique_ptr<ast::ValueAssignment> parse_value_assignment(Token name_tok);

  std::unique_ptr<ast::Type> parse_type();
  std::optional<ast::Tag> parse_optional_tag();
  std::unique_ptr<ast::Type> parse_untagged_type(std::optional<ast::Tag> tag);
  std::unique_ptr<ast::Constraint> parse_optional_constraint();
  std::unique_ptr<ast::Constraint> parse_constraint_atom();
  std::unique_ptr<ast::Constraint> parse_constraint_intersection();
  std::unique_ptr<ast::Constraint> parse_constraint_element();
  std::unique_ptr<ast::Constraint> parse_subtype_constraint();

  std::vector<ast::NamedNumber> parse_named_number_list();
  /// ENUMERATED { items } — numbers optional; ellipsis allowed.
  void parse_enumeration_list(std::vector<ast::NamedNumber>& root, bool& extensible,
                              std::vector<ast::NamedNumber>& extensions);
  std::vector<std::unique_ptr<ast::ComponentItem>> parse_component_type_list(bool choice);
  std::unique_ptr<ast::ComponentItem> parse_component_item(bool choice);

  std::unique_ptr<ast::Value> parse_value();
  std::unique_ptr<ast::Value> parse_integer_value();
  std::unique_ptr<ast::Value> parse_object_identifier_value();

  bool is_unsupported_construct(TokenKind kind) const;
  void report_unsupported(const Token& tok);

  Lexer& lexer_;
  Diagnostics& diagnostics_;
  Token current_;
};

}  // namespace asn1
