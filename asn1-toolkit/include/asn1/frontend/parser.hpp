/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/frontend/parser.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Parser interface building ast::Module trees from tokens.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (lexical and
**                 syntactic notation).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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
  /**
   *  Function    : Parser
   *  Description : Constructs or initializes Parser.
   *  Parameters  : lexer — Lexer& lexer; diagnostics — Diagnostics& diagnostics
   *  Returns     : void
   */
  Parser(Lexer& lexer, Diagnostics& diagnostics);

  /// Parse a single ModuleDefinition. Returns nullptr on hard failure.
  /**
   *  Function    : parse_module
   *  Description : Computes parse module from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Module>
   */
  std::unique_ptr<ast::Module> parse_module();

  /// Parse zero or more modules from the current input (multi-module files).
  /**
   *  Function    : parse_modules
   *  Description : Computes parse modules from (none).
   *  Parameters  : none
   *  Returns     : std::vector<std::unique_ptr<ast::Module>>
   */
  std::vector<std::unique_ptr<ast::Module>> parse_modules();

 private:
  Token peek() const { return current_; }
  /**
   *  Function    : advance
   *  Description : Computes advance from (none).
   *  Parameters  : none
   *  Returns     : Token
   */
  Token advance();
  /**
   *  Function    : check
   *  Description : Returns a boolean result from kind.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : bool
   */
  bool check(TokenKind kind) const;
  /**
   *  Function    : match
   *  Description : Returns a boolean result from kind.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : bool
   */
  bool match(TokenKind kind);
  /**
   *  Function    : expect
   *  Description : Returns a boolean result from kind, what.
   *  Parameters  : kind — TokenKind kind; what — const char* what
   *  Returns     : bool
   */
  bool expect(TokenKind kind, const char* what);

  /**
   *  Function    : error_at
   *  Description : Performs error at (declaration).
   *  Parameters  : at — const Token& at; message — std::string message
   *  Returns     : void
   */
  void error_at(const Token& at, std::string message);
  /**
   *  Function    : synchronize_assignment
   *  Description : Performs synchronize assignment (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void synchronize_assignment();

  /**
   *  Function    : parse_module_body
   *  Description : Computes parse module body from (name_tok).
   *  Parameters  : name_tok — Token name_tok
   *  Returns     : std::unique_ptr<ast::Module>
   */
  std::unique_ptr<ast::Module> parse_module_body(Token name_tok);
  /**
   *  Function    : parse_exports
   *  Description : Performs parse exports (declaration).
   *  Parameters  : exports — std::vector<std::string>& exports; exports_all — bool& exports_all
   *  Returns     : void
   */
  void parse_exports(std::vector<std::string>& exports, bool& exports_all);
  /**
   *  Function    : parse_imports
   *  Description : Performs parse imports (declaration).
   *  Parameters  : imports — std::vector<ast::ImportFrom>& imports
   *  Returns     : void
   */
  void parse_imports(std::vector<ast::ImportFrom>& imports);
  /**
   *  Function    : parse_assignment
   *  Description : Computes parse assignment from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Assignment>
   */
  std::unique_ptr<ast::Assignment> parse_assignment();
  /**
   *  Function    : parse_type_assignment
   *  Description : Computes parse type assignment from (name_tok).
   *  Parameters  : name_tok — Token name_tok
   *  Returns     : std::unique_ptr<ast::TypeAssignment>
   */
  std::unique_ptr<ast::TypeAssignment> parse_type_assignment(Token name_tok);
  /**
   *  Function    : parse_value_assignment
   *  Description : Computes parse value assignment from (name_tok).
   *  Parameters  : name_tok — Token name_tok
   *  Returns     : std::unique_ptr<ast::ValueAssignment>
   */
  std::unique_ptr<ast::ValueAssignment> parse_value_assignment(Token name_tok);

  /**
   *  Function    : parse_formal_parameter_list
   *  Description : Computes parse formal parameter list from (none).
   *  Parameters  : none
   *  Returns     : std::vector<ast::FormalParameter>
   */
  std::vector<ast::FormalParameter> parse_formal_parameter_list();
  /**
   *  Function    : parse_actual_parameter_list
   *  Description : Computes parse actual parameter list from (none).
   *  Parameters  : none
   *  Returns     : std::vector<ast::ActualParameter>
   */
  std::vector<ast::ActualParameter> parse_actual_parameter_list();
  /**
   *  Function    : is_type_start_token
   *  Description : Returns whether type start token holds for the given inputs.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : bool
   */
  bool is_type_start_token(TokenKind kind) const;
  /**
   *  Function    : is_governor_type_token
   *  Description : Returns whether governor type token holds for the given inputs.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : bool
   */
  bool is_governor_type_token(TokenKind kind) const;
  /**
   *  Function    : consume_governor_name
   *  Description : Builds and returns a string for consume governor name.
   *  Parameters  : none
   *  Returns     : std::string
   */
  std::string consume_governor_name();

  /**
   *  Function    : parse_object_class_assignment
   *  Description : Computes parse object class assignment from (name_tok).
   *  Parameters  : name_tok — Token name_tok
   *  Returns     : std::unique_ptr<ast::ObjectClassAssignment>
   */
  std::unique_ptr<ast::ObjectClassAssignment> parse_object_class_assignment(Token name_tok);
  /**
   *  Function    : parse_object_class_defn
   *  Description : Computes parse object class defn from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::ObjectClassDefn>
   */
  std::unique_ptr<ast::ObjectClassDefn> parse_object_class_defn();
  /**
   *  Function    : parse_field_spec
   *  Description : Computes parse field spec from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::FieldSpec>
   */
  std::unique_ptr<ast::FieldSpec> parse_field_spec();
  /**
   *  Function    : parse_with_syntax
   *  Description : Computes parse with syntax from (none).
   *  Parameters  : none
   *  Returns     : std::vector<ast::SyntaxToken>
   */
  std::vector<ast::SyntaxToken> parse_with_syntax();
  std::unique_ptr<ast::ObjectAssignment> parse_object_assignment(Token name_tok,
                                                                Token class_tok);
  std::unique_ptr<ast::ObjectSetAssignment> parse_object_set_assignment(Token name_tok,
                                                                       Token class_tok);
  std::unique_ptr<ast::ObjectDefn> parse_object_defn(const std::string& class_name = {});
  std::unique_ptr<ast::ObjectDefn> parse_defined_syntax_object(
      const std::string& class_name, const std::vector<ast::SyntaxToken>& syntax);
  /// Fallback when CLASS WITH SYNTAX is not yet known (forward / cross-module).
  /**
   *  Function    : parse_heuristic_defined_syntax_object
   *  Description : Computes parse heuristic defined syntax object from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::ObjectDefn>
   */
  std::unique_ptr<ast::ObjectDefn> parse_heuristic_defined_syntax_object();
  std::unique_ptr<ast::ObjectSetDefn> parse_object_set_defn(
      const std::string& class_name = {});
  /**
   *  Function    : parse_table_or_component_constraint
   *  Description : Computes parse table or component constraint from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_table_or_component_constraint();

  void remember_class_syntax(const std::string& class_name,
                             const std::vector<ast::SyntaxToken>& syntax);

  /**
   *  Function    : parse_type
   *  Description : Computes parse type from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Type>
   */
  std::unique_ptr<ast::Type> parse_type();
  /**
   *  Function    : parse_encoding_instructions
   *  Description : Computes parse encoding instructions from (none).
   *  Parameters  : none
   *  Returns     : std::vector<ast::EncodingInstruction>
   */
  std::vector<ast::EncodingInstruction> parse_encoding_instructions();
  /**
   *  Function    : parse_one_encoding_instruction
   *  Description : Computes parse one encoding instruction from (none).
   *  Parameters  : none
   *  Returns     : std::optional<ast::EncodingInstruction>
   */
  std::optional<ast::EncodingInstruction> parse_one_encoding_instruction();
  /**
   *  Function    : parse_name_transform
   *  Description : Computes parse name transform from (literal_out).
   *  Parameters  : literal_out — std::string* literal_out
   *  Returns     : std::optional<ast::NameTransform>
   */
  std::optional<ast::NameTransform> parse_name_transform(std::string* literal_out);
  /**
   *  Function    : parse_encoding_control
   *  Description : Computes parse encoding control from (none).
   *  Parameters  : none
   *  Returns     : std::vector<ast::EncodingControlClause>
   */
  std::vector<ast::EncodingControlClause> parse_encoding_control();
  /**
   *  Function    : parse_optional_tag
   *  Description : Computes parse optional tag from (none).
   *  Parameters  : none
   *  Returns     : std::optional<ast::Tag>
   */
  std::optional<ast::Tag> parse_optional_tag();
  /**
   *  Function    : parse_untagged_type
   *  Description : Computes parse untagged type from (tag).
   *  Parameters  : tag — std::optional<ast::Tag> tag
   *  Returns     : std::unique_ptr<ast::Type>
   */
  std::unique_ptr<ast::Type> parse_untagged_type(std::optional<ast::Tag> tag);
  /**
   *  Function    : parse_optional_constraint
   *  Description : Computes parse optional constraint from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_optional_constraint();
  /**
   *  Function    : parse_constraint_atom
   *  Description : Computes parse constraint atom from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_constraint_atom();
  /**
   *  Function    : parse_constraint_intersection
   *  Description : Computes parse constraint intersection from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_constraint_intersection();
  /**
   *  Function    : parse_constraint_element
   *  Description : Computes parse constraint element from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_constraint_element();
  /**
   *  Function    : parse_subtype_constraint
   *  Description : Computes parse subtype constraint from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_subtype_constraint();
  /**
   *  Function    : parse_contents_constraint
   *  Description : Computes parse contents constraint from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_contents_constraint();
  /**
   *  Function    : parse_with_components_constraint
   *  Description : Computes parse with components constraint from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Constraint>
   */
  std::unique_ptr<ast::Constraint> parse_with_components_constraint();

  /**
   *  Function    : parse_named_number_list
   *  Description : Computes parse named number list from (none).
   *  Parameters  : none
   *  Returns     : std::vector<ast::NamedNumber>
   */
  std::vector<ast::NamedNumber> parse_named_number_list();
  /// ENUMERATED { items } — numbers optional; ellipsis allowed.
  void parse_enumeration_list(std::vector<ast::NamedNumber>& root, bool& extensible,
                              std::vector<ast::NamedNumber>& extensions);
  /**
   *  Function    : parse_component_type_list
   *  Description : Computes parse component type list from (choice).
   *  Parameters  : choice — bool choice
   *  Returns     : std::vector<std::unique_ptr<ast::ComponentItem>>
   */
  std::vector<std::unique_ptr<ast::ComponentItem>> parse_component_type_list(bool choice);
  /**
   *  Function    : parse_component_item
   *  Description : Computes parse component item from (choice).
   *  Parameters  : choice — bool choice
   *  Returns     : std::unique_ptr<ast::ComponentItem>
   */
  std::unique_ptr<ast::ComponentItem> parse_component_item(bool choice);
  /**
   *  Function    : parse_version_addition_group
   *  Description : Computes parse version addition group from (choice).
   *  Parameters  : choice — bool choice
   *  Returns     : std::unique_ptr<ast::ComponentItem>
   */
  std::unique_ptr<ast::ComponentItem> parse_version_addition_group(bool choice);

  /**
   *  Function    : parse_value
   *  Description : Computes parse value from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Value>
   */
  std::unique_ptr<ast::Value> parse_value();
  /**
   *  Function    : parse_integer_value
   *  Description : Computes parse integer value from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Value>
   */
  std::unique_ptr<ast::Value> parse_integer_value();
  /**
   *  Function    : parse_object_identifier_value
   *  Description : Computes parse object identifier value from (none).
   *  Parameters  : none
   *  Returns     : std::unique_ptr<ast::Value>
   */
  std::unique_ptr<ast::Value> parse_object_identifier_value();

  /**
   *  Function    : is_unsupported_construct
   *  Description : Returns whether unsupported construct holds for the given inputs.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : bool
   */
  bool is_unsupported_construct(TokenKind kind) const;
  /**
   *  Function    : report_unsupported
   *  Description : Performs report unsupported (declaration).
   *  Parameters  : tok — const Token& tok
   *  Returns     : void
   */
  void report_unsupported(const Token& tok);
  /// TYPE-IDENTIFIER / ABSTRACT-SYNTAX (X.681 UsefulObjectClassReference).
  /**
   *  Function    : is_useful_object_class_token
   *  Description : Returns whether useful object class token holds for the given inputs.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : bool
   */
  bool is_useful_object_class_token(TokenKind kind) const;
  /**
   *  Function    : useful_object_class_name
   *  Description : Builds and returns a string for useful object class name.
   *  Parameters  : kind — TokenKind kind
   *  Returns     : std::string
   */
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
