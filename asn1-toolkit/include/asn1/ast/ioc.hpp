#pragma once

#include <asn1/ast/module.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace asn1 {
namespace ast {

/// X.681 field kind (Phase 13 subset).
enum class FieldSpecKind {
  TypeField,            // &Type
  FixedTypeValueField,  // &value Type [UNIQUE] [OPTIONAL|DEFAULT]
};

class FieldSpec final : public Node {
 public:
  FieldSpec(SourceRange range, FieldSpecKind kind, std::string name, NodePtr<Type> field_type,
            bool unique, Presence presence, NodePtr<Value> default_value)
      : Node(std::move(range)),
        kind_(kind),
        name_(std::move(name)),
        field_type_(std::move(field_type)),
        unique_(unique),
        presence_(presence),
        default_value_(std::move(default_value)) {}

  FieldSpecKind kind() const noexcept { return kind_; }
  const std::string& name() const noexcept { return name_; }
  /// Null for TypeField; set for FixedTypeValueField.
  const Type* field_type() const { return field_type_.get(); }
  bool unique() const noexcept { return unique_; }
  Presence presence() const noexcept { return presence_; }
  const Value* default_value() const { return default_value_.get(); }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  FieldSpecKind kind_;
  std::string name_;
  NodePtr<Type> field_type_;
  bool unique_ = false;
  Presence presence_ = Presence::Mandatory;
  NodePtr<Value> default_value_;
};

/// WITH SYNTAX { ... } kept as a flat token-text sequence for Phase 13.
struct SyntaxToken {
  std::string text;
  SourceRange range;
  bool is_field = false;  // true when text is a field name (&foo without ampersand stored in text)
};

class ObjectClassDefn final : public Node {
 public:
  ObjectClassDefn(SourceRange range, std::vector<NodePtr<FieldSpec>> fields,
                 std::vector<SyntaxToken> with_syntax)
      : Node(std::move(range)),
        fields_(std::move(fields)),
        with_syntax_(std::move(with_syntax)) {}

  const std::vector<NodePtr<FieldSpec>>& fields() const noexcept { return fields_; }
  const std::vector<SyntaxToken>& with_syntax() const noexcept { return with_syntax_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<FieldSpec>> fields_;
  std::vector<SyntaxToken> with_syntax_;
};

class ObjectClassAssignment final : public Assignment {
 public:
  ObjectClassAssignment(SourceRange range, std::string name, NodePtr<ObjectClassDefn> defn)
      : Assignment(std::move(range)), name_(std::move(name)), defn_(std::move(defn)) {}

  const std::string& name() const noexcept { return name_; }
  const ObjectClassDefn& defn() const { return *defn_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  NodePtr<ObjectClassDefn> defn_;
};

struct FieldSetting {
  std::string field_name;
  /// Either a type setting or a value setting (exactly one set).
  NodePtr<Type> type_setting;
  NodePtr<Value> value_setting;
  SourceRange range;
};

class ObjectDefn final : public Node {
 public:
  ObjectDefn(SourceRange range, std::vector<FieldSetting> settings)
      : Node(std::move(range)), settings_(std::move(settings)) {}

  const std::vector<FieldSetting>& settings() const noexcept { return settings_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<FieldSetting> settings_;
};

class ObjectAssignment final : public Assignment {
 public:
  ObjectAssignment(SourceRange range, std::string name, std::string class_name,
                    NodePtr<ObjectDefn> defn)
      : Assignment(std::move(range)),
        name_(std::move(name)),
        class_name_(std::move(class_name)),
        defn_(std::move(defn)) {}

  const std::string& name() const noexcept { return name_; }
  const std::string& class_name() const noexcept { return class_name_; }
  const ObjectDefn& defn() const { return *defn_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  std::string class_name_;
  NodePtr<ObjectDefn> defn_;
};

/// Object set element: reference to an object (identifier) or inline object defn.
struct ObjectSetElement {
  std::optional<std::string> object_ref;
  NodePtr<ObjectDefn> inline_object;
  SourceRange range;
};

class ObjectSetDefn final : public Node {
 public:
  ObjectSetDefn(SourceRange range, std::vector<ObjectSetElement> elements, bool extensible)
      : Node(std::move(range)), elements_(std::move(elements)), extensible_(extensible) {}

  const std::vector<ObjectSetElement>& elements() const noexcept { return elements_; }
  bool extensible() const noexcept { return extensible_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<ObjectSetElement> elements_;
  bool extensible_ = false;
};

class ObjectSetAssignment final : public Assignment {
 public:
  ObjectSetAssignment(SourceRange range, std::string name, std::string class_name,
                       NodePtr<ObjectSetDefn> defn)
      : Assignment(std::move(range)),
        name_(std::move(name)),
        class_name_(std::move(class_name)),
        defn_(std::move(defn)) {}

  const std::string& name() const noexcept { return name_; }
  const std::string& class_name() const noexcept { return class_name_; }
  const ObjectSetDefn& defn() const { return *defn_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  std::string class_name_;
  NodePtr<ObjectSetDefn> defn_;
};

/// DefinedObjectClass . &FieldName
class ObjectClassFieldType final : public Type {
 public:
  ObjectClassFieldType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                       std::string class_name, std::string field_name)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        class_name_(std::move(class_name)),
        field_name_(std::move(field_name)) {}

  const std::string& class_name() const noexcept { return class_name_; }
  const std::string& field_name() const noexcept { return field_name_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string class_name_;
  std::string field_name_;
};

/// Simple table constraint: ( ObjectSet ) where ObjectSet is { ... } or a set reference.
class TableConstraint final : public Constraint {
 public:
  TableConstraint(SourceRange range, std::string object_set_name,
                  NodePtr<ObjectSetDefn> inline_set)
      : Constraint(std::move(range)),
        object_set_name_(std::move(object_set_name)),
        inline_set_(std::move(inline_set)) {}

  const std::string& object_set_name() const noexcept { return object_set_name_; }
  const ObjectSetDefn* inline_set() const { return inline_set_.get(); }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string object_set_name_;  // empty when inline_set_ is set
  NodePtr<ObjectSetDefn> inline_set_;
};

/// Component relation: { ObjectSet }{ @component, ... }
class ComponentRelationConstraint final : public Constraint {
 public:
  ComponentRelationConstraint(SourceRange range, std::string object_set_name,
                              NodePtr<ObjectSetDefn> inline_set,
                              std::vector<std::string> at_components)
      : Constraint(std::move(range)),
        object_set_name_(std::move(object_set_name)),
        inline_set_(std::move(inline_set)),
        at_components_(std::move(at_components)) {}

  const std::string& object_set_name() const noexcept { return object_set_name_; }
  const ObjectSetDefn* inline_set() const { return inline_set_.get(); }
  const std::vector<std::string>& at_components() const noexcept { return at_components_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string object_set_name_;
  NodePtr<ObjectSetDefn> inline_set_;
  std::vector<std::string> at_components_;
};

}  // namespace ast
}  // namespace asn1
