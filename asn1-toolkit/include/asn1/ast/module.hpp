/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ast/module.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   AST for MODULE headers, imports/exports, and top-level assignments.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (parse tree
**                 produced/consumed here).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/ast/encoding.hpp>
#include <asn1/ast/type.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace ast {

enum class TagDefault { Explicit, Implicit, Automatic };

class Assignment : public Node {
 public:
  using Node::Node;
};

/// Formal parameter of a parameterized type assignment (X.683).
struct FormalParameter {
  std::string governor;  // empty if DummyReference alone; else Type / ObjectClass name
  std::string name;
  bool type_ref_name = false;  // DummyReference was TypeReference
};

class TypeAssignment final : public Assignment {
 public:
  TypeAssignment(SourceRange range, std::string name, NodePtr<Type> type,
                 std::vector<FormalParameter> parameters = {})
      : Assignment(std::move(range)),
        name_(std::move(name)),
        type_(std::move(type)),
        parameters_(std::move(parameters)) {}

  /**
   *  Function    : name
   *  Description : Builds and returns a string for name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& name() const noexcept { return name_; }
  /**
   *  Function    : type
   *  Description : Computes type from (none).
   *  Parameters  : none
   *  Returns     : const Type&
   */
  const Type& type() const { return *type_; }
  /**
   *  Function    : parameters
   *  Description : Computes parameters from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<FormalParameter>&
   */
  const std::vector<FormalParameter>& parameters() const noexcept { return parameters_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  NodePtr<Type> type_;
  std::vector<FormalParameter> parameters_;
};

class ValueAssignment final : public Assignment {
 public:
  ValueAssignment(SourceRange range, std::string name, NodePtr<Type> type, NodePtr<Value> value)
      : Assignment(std::move(range)),
        name_(std::move(name)),
        type_(std::move(type)),
        value_(std::move(value)) {}

  /**
   *  Function    : name
   *  Description : Builds and returns a string for name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& name() const noexcept { return name_; }
  /**
   *  Function    : type
   *  Description : Computes type from (none).
   *  Parameters  : none
   *  Returns     : const Type&
   */
  const Type& type() const { return *type_; }
  /**
   *  Function    : value
   *  Description : Computes value from (none).
   *  Parameters  : none
   *  Returns     : const Value&
   */
  const Value& value() const { return *value_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  NodePtr<Type> type_;
  NodePtr<Value> value_;
};

struct ImportedSymbol {
  std::string name;
  SourceRange range;
  bool parameterized = false;  // IMPORTS Name{}
};

struct ImportFrom {
  std::string module;
  std::optional<std::string> oid_text;  // raw text if present; Phase 3 keeps simple
  std::vector<ImportedSymbol> symbols;
  SourceRange range;
};

class Module final : public Node {
 public:
  Module(SourceRange range, std::string name, TagDefault tag_default,
         bool extensibility_implied, std::vector<std::string> exports,
         bool exports_all, std::vector<ImportFrom> imports,
         std::vector<NodePtr<Assignment>> assignments)
      : Node(std::move(range)),
        name_(std::move(name)),
        tag_default_(tag_default),
        extensibility_implied_(extensibility_implied),
        exports_(std::move(exports)),
        exports_all_(exports_all),
        imports_(std::move(imports)),
        assignments_(std::move(assignments)) {}

  /**
   *  Function    : name
   *  Description : Builds and returns a string for name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& name() const noexcept { return name_; }
  TagDefault tag_default() const noexcept { return tag_default_; }
  /**
   *  Function    : extensibility_implied
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool extensibility_implied() const noexcept { return extensibility_implied_; }
  /**
   *  Function    : jer_instructions
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool jer_instructions() const noexcept { return jer_instructions_; }
  /**
   *  Function    : xer_instructions
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool xer_instructions() const noexcept { return xer_instructions_; }
  /**
   *  Function    : exports
   *  Description : Builds and returns a string for exports.
   *  Parameters  : none
   *  Returns     : const std::vector<std::string>&
   */
  const std::vector<std::string>& exports() const noexcept { return exports_; }
  /**
   *  Function    : exports_all
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool exports_all() const noexcept { return exports_all_; }
  /**
   *  Function    : imports
   *  Description : Computes imports from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<ImportFrom>&
   */
  const std::vector<ImportFrom>& imports() const noexcept { return imports_; }
  /**
   *  Function    : assignments
   *  Description : Computes assignments from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<Assignment>>&
   */
  const std::vector<NodePtr<Assignment>>& assignments() const noexcept {
    return assignments_;
  }
  /**
   *  Function    : jer_encoding_control
   *  Description : Computes jer encoding control from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<EncodingControlClause>&
   */
  const std::vector<EncodingControlClause>& jer_encoding_control() const noexcept {
    return jer_encoding_control_;
  }
  /**
   *  Function    : xer_encoding_control
   *  Description : Computes xer encoding control from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<EncodingControlClause>&
   */
  const std::vector<EncodingControlClause>& xer_encoding_control() const noexcept {
    return xer_encoding_control_;
  }

  /**
   *  Function    : set_jer_instructions
   *  Description : Performs set jer instructions (definition).
   *  Parameters  : v — bool v
   *  Returns     : void
   */
  void set_jer_instructions(bool v) { jer_instructions_ = v; }
  /**
   *  Function    : set_xer_instructions
   *  Description : Performs set xer instructions (definition).
   *  Parameters  : v — bool v
   *  Returns     : void
   */
  void set_xer_instructions(bool v) { xer_instructions_ = v; }
  /**
   *  Function    : set_jer_encoding_control
   *  Description : Performs set jer encoding control (definition).
   *  Parameters  : clauses — std::vector<EncodingControlClause> clauses
   *  Returns     : void
   */
  void set_jer_encoding_control(std::vector<EncodingControlClause> clauses) {
    /**
     *  Function    : move
     *  Description : Computes move from (clauses).
     *  Parameters  : clauses — clauses
     *  Returns     : jer_encoding_control_ = std::
     */
    jer_encoding_control_ = std::move(clauses);
  }
  /**
   *  Function    : set_xer_encoding_control
   *  Description : Performs set xer encoding control (definition).
   *  Parameters  : clauses — std::vector<EncodingControlClause> clauses
   *  Returns     : void
   */
  void set_xer_encoding_control(std::vector<EncodingControlClause> clauses) {
    /**
     *  Function    : move
     *  Description : Computes move from (clauses).
     *  Parameters  : clauses — clauses
     *  Returns     : xer_encoding_control_ = std::
     */
    xer_encoding_control_ = std::move(clauses);
  }

  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  TagDefault tag_default_ = TagDefault::Explicit;
  bool extensibility_implied_ = false;
  bool jer_instructions_ = false;
  bool xer_instructions_ = false;
  std::vector<std::string> exports_;
  bool exports_all_ = false;
  std::vector<ImportFrom> imports_;
  std::vector<NodePtr<Assignment>> assignments_;
  std::vector<EncodingControlClause> jer_encoding_control_;
  std::vector<EncodingControlClause> xer_encoding_control_;
};

}  // namespace ast
}  // namespace asn1
