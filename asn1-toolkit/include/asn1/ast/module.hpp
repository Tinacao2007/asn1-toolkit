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

  const std::string& name() const noexcept { return name_; }
  const Type& type() const { return *type_; }
  const std::vector<FormalParameter>& parameters() const noexcept { return parameters_; }
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

  const std::string& name() const noexcept { return name_; }
  const Type& type() const { return *type_; }
  const Value& value() const { return *value_; }
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

  const std::string& name() const noexcept { return name_; }
  TagDefault tag_default() const noexcept { return tag_default_; }
  bool extensibility_implied() const noexcept { return extensibility_implied_; }
  bool jer_instructions() const noexcept { return jer_instructions_; }
  bool xer_instructions() const noexcept { return xer_instructions_; }
  const std::vector<std::string>& exports() const noexcept { return exports_; }
  bool exports_all() const noexcept { return exports_all_; }
  const std::vector<ImportFrom>& imports() const noexcept { return imports_; }
  const std::vector<NodePtr<Assignment>>& assignments() const noexcept {
    return assignments_;
  }
  const std::vector<EncodingControlClause>& jer_encoding_control() const noexcept {
    return jer_encoding_control_;
  }
  const std::vector<EncodingControlClause>& xer_encoding_control() const noexcept {
    return xer_encoding_control_;
  }

  void set_jer_instructions(bool v) { jer_instructions_ = v; }
  void set_xer_instructions(bool v) { xer_instructions_ = v; }
  void set_jer_encoding_control(std::vector<EncodingControlClause> clauses) {
    jer_encoding_control_ = std::move(clauses);
  }
  void set_xer_encoding_control(std::vector<EncodingControlClause> clauses) {
    xer_encoding_control_ = std::move(clauses);
  }

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
