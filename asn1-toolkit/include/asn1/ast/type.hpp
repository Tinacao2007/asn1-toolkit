#pragma once

#include <asn1/ast/node.hpp>
#include <asn1/ast/visitor.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace asn1 {
namespace ast {

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

enum class TagClass { Universal, Application, Context, Private };

enum class TagMode { Explicit, Implicit };

struct Tag {
  TagClass cls = TagClass::Context;
  std::string number_text;  // decimal text from source
  std::optional<TagMode> mode;  // absent => module default
  SourceRange range;
};

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------

class Value : public Node {
 public:
  using Node::Node;
};

class IntegerValue final : public Value {
 public:
  IntegerValue(SourceRange range, std::string text, bool negative)
      : Value(std::move(range)), text_(std::move(text)), negative_(negative) {}

  const std::string& text() const noexcept { return text_; }
  bool negative() const noexcept { return negative_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string text_;
  bool negative_ = false;
};

class BooleanValue final : public Value {
 public:
  BooleanValue(SourceRange range, bool value)
      : Value(std::move(range)), value_(value) {}

  bool value() const noexcept { return value_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  bool value_;
};

class StringValue final : public Value {
 public:
  StringValue(SourceRange range, std::string text)
      : Value(std::move(range)), text_(std::move(text)) {}

  const std::string& text() const noexcept { return text_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string text_;
};

enum class BitOrOctetKind { Binary, Hex };

class BitOrOctetValue final : public Value {
 public:
  BitOrOctetValue(SourceRange range, BitOrOctetKind kind, std::string text)
      : Value(std::move(range)), kind_(kind), text_(std::move(text)) {}

  BitOrOctetKind kind() const noexcept { return kind_; }
  const std::string& text() const noexcept { return text_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  BitOrOctetKind kind_;
  std::string text_;
};

class ValueReference final : public Value {
 public:
  ValueReference(SourceRange range, std::string name)
      : Value(std::move(range)), name_(std::move(name)) {}

  const std::string& name() const noexcept { return name_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
};

class NamedValue final : public Value {
 public:
  NamedValue(SourceRange range, std::string name, NodePtr<Value> value)
      : Value(std::move(range)), name_(std::move(name)), value_(std::move(value)) {}

  const std::string& name() const noexcept { return name_; }
  const Value& value() const { return *value_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  NodePtr<Value> value_;
};

class NamedValueList final : public Value {
 public:
  NamedValueList(SourceRange range, std::vector<NodePtr<NamedValue>> values)
      : Value(std::move(range)), values_(std::move(values)) {}

  const std::vector<NodePtr<NamedValue>>& values() const noexcept { return values_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<NamedValue>> values_;
};

// ---------------------------------------------------------------------------
// Constraints
// ---------------------------------------------------------------------------

class Constraint : public Node {
 public:
  using Node::Node;
};

class ValueRangeConstraint final : public Constraint {
 public:
  ValueRangeConstraint(SourceRange range, NodePtr<Value> lower, NodePtr<Value> upper)
      : Constraint(std::move(range)),
        lower_(std::move(lower)),
        upper_(std::move(upper)) {}

  const Value* lower() const noexcept { return lower_.get(); }
  const Value* upper() const noexcept { return upper_.get(); }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Value> lower_;
  NodePtr<Value> upper_;
};

class SingleValueConstraint final : public Constraint {
 public:
  SingleValueConstraint(SourceRange range, NodePtr<Value> value)
      : Constraint(std::move(range)), value_(std::move(value)) {}

  const Value& value() const { return *value_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Value> value_;
};

class SizeConstraint final : public Constraint {
 public:
  SizeConstraint(SourceRange range, NodePtr<Constraint> inner)
      : Constraint(std::move(range)), inner_(std::move(inner)) {}

  const Constraint& inner() const { return *inner_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Constraint> inner_;
};

class UnionConstraint final : public Constraint {
 public:
  UnionConstraint(SourceRange range, std::vector<NodePtr<Constraint>> alts)
      : Constraint(std::move(range)), alternatives_(std::move(alts)) {}

  const std::vector<NodePtr<Constraint>>& alternatives() const noexcept {
    return alternatives_;
  }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<Constraint>> alternatives_;
};

class IntersectionConstraint final : public Constraint {
 public:
  IntersectionConstraint(SourceRange range, std::vector<NodePtr<Constraint>> parts)
      : Constraint(std::move(range)), parts_(std::move(parts)) {}

  const std::vector<NodePtr<Constraint>>& parts() const noexcept { return parts_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<Constraint>> parts_;
};

class ExtensibleConstraint final : public Constraint {
 public:
  ExtensibleConstraint(SourceRange range, NodePtr<Constraint> root)
      : Constraint(std::move(range)), root_(std::move(root)) {}

  const Constraint* root() const noexcept { return root_.get(); }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Constraint> root_;  // may be null if only "..."
};

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

class Type : public Node {
 public:
  Type(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint)
      : Node(std::move(range)),
        tag_(std::move(tag)),
        constraint_(std::move(constraint)) {}

  const std::optional<Tag>& tag() const noexcept { return tag_; }
  const Constraint* constraint() const noexcept { return constraint_.get(); }

  void set_tag(std::optional<Tag> tag) { tag_ = std::move(tag); }
  void set_constraint(NodePtr<Constraint> c) { constraint_ = std::move(c); }

 private:
  std::optional<Tag> tag_;
  NodePtr<Constraint> constraint_;
};

struct NamedNumber {
  std::string name;
  std::string number_text;
  bool negative = false;
  SourceRange range;
};

class BooleanType final : public Type {
 public:
  using Type::Type;
  void accept(Visitor& v) const override { v.visit(*this); }
};

class IntegerType final : public Type {
 public:
  IntegerType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
              std::vector<NamedNumber> named)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        named_numbers_(std::move(named)) {}

  const std::vector<NamedNumber>& named_numbers() const noexcept { return named_numbers_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NamedNumber> named_numbers_;
};

class BitStringType final : public Type {
 public:
  BitStringType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                std::vector<NamedNumber> named_bits)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        named_bits_(std::move(named_bits)) {}

  const std::vector<NamedNumber>& named_bits() const noexcept { return named_bits_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NamedNumber> named_bits_;
};

class OctetStringType final : public Type {
 public:
  using Type::Type;
  void accept(Visitor& v) const override { v.visit(*this); }
};

class NullType final : public Type {
 public:
  using Type::Type;
  void accept(Visitor& v) const override { v.visit(*this); }
};

enum class StringKind {
  UTF8String,
  IA5String,
  PrintableString,
  VisibleString,
  NumericString,
  TeletexString,
  VideotexString,
  GraphicString,
  GeneralString,
  BMPString,
  UniversalString,
};

class StringType final : public Type {
 public:
  StringType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
             StringKind kind)
      : Type(std::move(range), std::move(tag), std::move(constraint)), kind_(kind) {}

  StringKind kind() const noexcept { return kind_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  StringKind kind_;
};

class ComponentItem : public Node {
 public:
  using Node::Node;
};

enum class Presence { Mandatory, Optional, Default };

class Component final : public ComponentItem {
 public:
  Component(SourceRange range, std::string name, NodePtr<Type> type, Presence presence,
            NodePtr<Value> default_value)
      : ComponentItem(std::move(range)),
        name_(std::move(name)),
        type_(std::move(type)),
        presence_(presence),
        default_value_(std::move(default_value)) {}

  const std::string& name() const noexcept { return name_; }
  const Type& type() const { return *type_; }
  Presence presence() const noexcept { return presence_; }
  const Value* default_value() const noexcept { return default_value_.get(); }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
  NodePtr<Type> type_;
  Presence presence_ = Presence::Mandatory;
  NodePtr<Value> default_value_;
};

class ExtensionMarker final : public ComponentItem {
 public:
  using ComponentItem::ComponentItem;
  void accept(Visitor& v) const override { v.visit(*this); }
};

class SequenceType final : public Type {
 public:
  SequenceType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
               std::vector<NodePtr<ComponentItem>> items)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        items_(std::move(items)) {}

  const std::vector<NodePtr<ComponentItem>>& items() const noexcept { return items_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<ComponentItem>> items_;
};

class ChoiceType final : public Type {
 public:
  ChoiceType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
             std::vector<NodePtr<ComponentItem>> alternatives)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        alternatives_(std::move(alternatives)) {}

  const std::vector<NodePtr<ComponentItem>>& alternatives() const noexcept {
    return alternatives_;
  }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<ComponentItem>> alternatives_;
};

class SequenceOfType final : public Type {
 public:
  SequenceOfType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                 NodePtr<Type> element)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        element_(std::move(element)) {}

  const Type& element() const { return *element_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Type> element_;
};

class SetType final : public Type {
 public:
  SetType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
          std::vector<NodePtr<ComponentItem>> items)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        items_(std::move(items)) {}

  const std::vector<NodePtr<ComponentItem>>& items() const noexcept { return items_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<ComponentItem>> items_;
};

class SetOfType final : public Type {
 public:
  SetOfType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
            NodePtr<Type> element)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        element_(std::move(element)) {}

  const Type& element() const { return *element_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Type> element_;
};

class EnumeratedType final : public Type {
 public:
  EnumeratedType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                 std::vector<NamedNumber> root, bool extensible,
                 std::vector<NamedNumber> extensions)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        root_(std::move(root)),
        extensible_(extensible),
        extensions_(std::move(extensions)) {}

  const std::vector<NamedNumber>& root() const noexcept { return root_; }
  bool extensible() const noexcept { return extensible_; }
  const std::vector<NamedNumber>& extensions() const noexcept { return extensions_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NamedNumber> root_;
  bool extensible_ = false;
  std::vector<NamedNumber> extensions_;
};

class ObjectIdentifierType final : public Type {
 public:
  using Type::Type;
  void accept(Visitor& v) const override { v.visit(*this); }
};

class RelativeOidType final : public Type {
 public:
  using Type::Type;
  void accept(Visitor& v) const override { v.visit(*this); }
};

class RealType final : public Type {
 public:
  using Type::Type;
  void accept(Visitor& v) const override { v.visit(*this); }
};

class ObjectIdentifierValue final : public Value {
 public:
  struct Arc {
    std::optional<std::string> name;
    std::optional<std::uint64_t> number;
  };

  ObjectIdentifierValue(SourceRange range, std::vector<Arc> arcs)
      : Value(std::move(range)), arcs_(std::move(arcs)) {}

  const std::vector<Arc>& arcs() const noexcept { return arcs_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<Arc> arcs_;
};

class ReferencedType final : public Type {
 public:
  ReferencedType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                 std::optional<std::string> module, std::string name)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        module_(std::move(module)),
        name_(std::move(name)) {}

  const std::optional<std::string>& module() const noexcept { return module_; }
  const std::string& name() const noexcept { return name_; }
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::optional<std::string> module_;
  std::string name_;
};

}  // namespace ast
}  // namespace asn1
