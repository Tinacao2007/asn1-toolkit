#pragma once

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

class TypeAssignment final : public Assignment {
 public:
  TypeAssignment(SourceRange range, std::string name, NodePtr<Type> type)
      : Assignment(std::move(range)), name_(std::move(name)), type_(std::move(type)) {}

  const std::string& name() const noexcept { return name_; }
  const Type& type() const { return *type_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  NodePtr<Type> type_;
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
  const std::vector<std::string>& exports() const noexcept { return exports_; }
  bool exports_all() const noexcept { return exports_all_; }
  const std::vector<ImportFrom>& imports() const noexcept { return imports_; }
  const std::vector<NodePtr<Assignment>>& assignments() const noexcept {
    return assignments_;
  }

  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  TagDefault tag_default_ = TagDefault::Explicit;
  bool extensibility_implied_ = false;
  std::vector<std::string> exports_;
  bool exports_all_ = false;
  std::vector<ImportFrom> imports_;
  std::vector<NodePtr<Assignment>> assignments_;
};

}  // namespace ast
}  // namespace asn1
