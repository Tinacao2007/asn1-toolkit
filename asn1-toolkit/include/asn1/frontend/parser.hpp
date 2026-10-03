#pragma once

#include <asn1/ast/module.hpp>
#include <asn1/ast/ioc.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/support/diagnostics.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace asn1 {

/// Recursive-descent ASN.1 parser. Builds an AST; does not resolve symbols or tags.
class Parser {
 public:
  Parser(Lexer& lexer, Diagnostics& diagnostics);

  /// Parse a single ModuleDefinition. Returns nullptr on hard failure.
  std::unique_ptr<ast::Module> parse_module();

  /// Parse zero or more modules from the current input (multi-module files).
  std::vector<std::unique_ptr<ast::Module>> parse_modules();

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

  std::vector<ast::FormalParameter> parse_formal_parameter_list();
  std::vector<ast::ActualParameter> parse_actual_parameter_list();
  bool is_type_start_token(TokenKind kind) const;
  bool is_governor_type_token(TokenKind kind) const;
  std::string consume_governor_name();

  std::unique_ptr<ast::ObjectClassAssignment> parse_object_class_assignment(Token name_tok);
  std::unique_ptr<ast::ObjectClassDefn> parse_object_class_defn();
  std::unique_ptr<ast::FieldSpec> parse_field_spec();
  std::vector<ast::SyntaxToken> parse_with_syntax();
  std::unique_ptr<ast::ObjectAssignment> parse_object_assignment(Token name_tok,
                                                                Token class_tok);
  std::unique_ptr<ast::ObjectSetAssignment> parse_object_set_assignment(Token name_tok,
                                                                       Token class_tok);
  std::unique_ptr<ast::ObjectDefn> parse_object_defn(const std::string& class_name = {});
  std::unique_ptr<ast::ObjectDefn> parse_defined_syntax_object(
      const std::string& class_name, const std::vector<ast::SyntaxToken>& syntax);
  /// Fallback when CLASS WITH SYNTAX is not yet known (forward / cross-module).
  std::unique_ptr<ast::ObjectDefn> parse_heuristic_defined_syntax_object();
  std::unique_ptr<ast::ObjectSetDefn> parse_object_set_defn(
      const std::string& class_name = {});
  std::unique_ptr<ast::Constraint> parse_table_or_component_constraint();

  void remember_class_syntax(const std::string& class_name,
                             const std::vector<ast::SyntaxToken>& syntax);

  std::unique_ptr<ast::Type> parse_type();
  std::vector<ast::EncodingInstruction> parse_encoding_instructions();
  std::optional<ast::EncodingInstruction> parse_one_encoding_instruction();
  std::optional<ast::NameTransform> parse_name_transform(std::string* literal_out);
  std::vector<ast::EncodingControlClause> parse_encoding_control();
  std::optional<ast::Tag> parse_optional_tag();
  std::unique_ptr<ast::Type> parse_untagged_type(std::optional<ast::Tag> tag);
  std::unique_ptr<ast::Constraint> parse_optional_constraint();
  std::unique_ptr<ast::Constraint> parse_constraint_atom();
  std::unique_ptr<ast::Constraint> parse_constraint_intersection();
  std::unique_ptr<ast::Constraint> parse_constraint_element();
  std::unique_ptr<ast::Constraint> parse_subtype_constraint();
  std::unique_ptr<ast::Constraint> parse_contents_constraint();
  std::unique_ptr<ast::Constraint> parse_with_components_constraint();

  std::vector<ast::NamedNumber> parse_named_number_list();
  /// ENUMERATED { items } — numbers optional; ellipsis allowed.
  void parse_enumeration_list(std::vector<ast::NamedNumber>& root, bool& extensible,
                              std::vector<ast::NamedNumber>& extensions);
  std::vector<std::unique_ptr<ast::ComponentItem>> parse_component_type_list(bool choice);
  std::unique_ptr<ast::ComponentItem> parse_component_item(bool choice);
  std::unique_ptr<ast::ComponentItem> parse_version_addition_group(bool choice);

  std::unique_ptr<ast::Value> parse_value();
  std::unique_ptr<ast::Value> parse_integer_value();
  std::unique_ptr<ast::Value> parse_object_identifier_value();

  bool is_unsupported_construct(TokenKind kind) const;
  void report_unsupported(const Token& tok);
  /// TYPE-IDENTIFIER / ABSTRACT-SYNTAX (X.681 UsefulObjectClassReference).
  bool is_useful_object_class_token(TokenKind kind) const;
  std::string useful_object_class_name(TokenKind kind) const;

  Lexer& lexer_;
  Diagnostics& diagnostics_;
  Token current_;
  /// Object class name -> WITH SYNTAX tokens (for defined-syntax object parsing).
  std::unordered_map<std::string, std::vector<ast::SyntaxToken>> class_syntax_;
  /// Literal words collected from all WITH SYNTAX (helps heuristic parsing).
  std::unordered_set<std::string> known_syntax_literals_;
};

}  // namespace asn1
