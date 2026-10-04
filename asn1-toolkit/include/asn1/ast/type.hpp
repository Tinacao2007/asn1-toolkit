/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ast/type.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   AST nodes for ASN.1 type constructors and sequence/choice components.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (parse tree
**                 produced/consumed here).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/ast/encoding.hpp>
#include <asn1/ast/node.hpp>
#include <asn1/ast/visitor.hpp>

#include <cstdint>
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
  /**
   *  Function    : negative_
   *  Description : Returns a boolean result from range, text, negative_(negative.
   *  Parameters  : range — SourceRange range; text — std::string text; negative_(negative — bool negative) : Value(std::move(range)), text_(std::move(text)), negative_(negative
   *  Returns     : IntegerValue(SourceRange range, std::string text, bool negative) : Value(std::move(range)), text_(std::move(text)),
   */
  IntegerValue(SourceRange range, std::string text, bool negative)
      : Value(std::move(range)), text_(std::move(text)), negative_(negative) {}

  /**
   *  Function    : text
   *  Description : Builds and returns a string for text.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& text() const noexcept { return text_; }
  /**
   *  Function    : negative
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool negative() const noexcept { return negative_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string text_;
  bool negative_ = false;
};

class BooleanValue final : public Value {
 public:
  /**
   *  Function    : value_
   *  Description : Returns a boolean result from range, value_(value.
   *  Parameters  : range — SourceRange range; value_(value — bool value) : Value(std::move(range)), value_(value
   *  Returns     : BooleanValue(SourceRange range, bool value) : Value(std::move(range)),
   */
  BooleanValue(SourceRange range, bool value)
      : Value(std::move(range)), value_(value) {}

  /**
   *  Function    : value
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool value() const noexcept { return value_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  bool value_;
};

class StringValue final : public Value {
 public:
  StringValue(SourceRange range, std::string text)
      : Value(std::move(range)), text_(std::move(text)) {}

  /**
   *  Function    : text
   *  Description : Builds and returns a string for text.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& text() const noexcept { return text_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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
  /**
   *  Function    : text
   *  Description : Builds and returns a string for text.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& text() const noexcept { return text_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  BitOrOctetKind kind_;
  std::string text_;
};

class ValueReference final : public Value {
 public:
  ValueReference(SourceRange range, std::string name)
      : Value(std::move(range)), name_(std::move(name)) {}

  /**
   *  Function    : name
   *  Description : Builds and returns a string for name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& name() const noexcept { return name_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string name_;
};

class NamedValue final : public Value {
 public:
  NamedValue(SourceRange range, std::string name, NodePtr<Value> value)
      : Value(std::move(range)), name_(std::move(name)), value_(std::move(value)) {}

  /**
   *  Function    : name
   *  Description : Builds and returns a string for name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& name() const noexcept { return name_; }
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
  NodePtr<Value> value_;
};

class NamedValueList final : public Value {
 public:
  NamedValueList(SourceRange range, std::vector<NodePtr<NamedValue>> values)
      : Value(std::move(range)), values_(std::move(values)) {}

  /**
   *  Function    : values
   *  Description : Computes values from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<NamedValue>>&
   */
  const std::vector<NodePtr<NamedValue>>& values() const noexcept { return values_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : get
   *  Description : Computes get from (lower_.get().
   *  Parameters  : lower_.get( — ) const noexcept { return lower_.get(
   *  Returns     : const Value* lower() const noexcept { return lower_.
   */
  const Value* lower() const noexcept { return lower_.get(); }
  /**
   *  Function    : get
   *  Description : Computes get from (upper_.get().
   *  Parameters  : upper_.get( — ) const noexcept { return upper_.get(
   *  Returns     : const Value* upper() const noexcept { return upper_.
   */
  const Value* upper() const noexcept { return upper_.get(); }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Value> lower_;
  NodePtr<Value> upper_;
};

class SingleValueConstraint final : public Constraint {
 public:
  SingleValueConstraint(SourceRange range, NodePtr<Value> value)
      : Constraint(std::move(range)), value_(std::move(value)) {}

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
  NodePtr<Value> value_;
};

class SizeConstraint final : public Constraint {
 public:
  SizeConstraint(SourceRange range, NodePtr<Constraint> inner)
      : Constraint(std::move(range)), inner_(std::move(inner)) {}

  /**
   *  Function    : inner
   *  Description : Computes inner from (none).
   *  Parameters  : none
   *  Returns     : const Constraint&
   */
  const Constraint& inner() const { return *inner_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Constraint> inner_;
};

class UnionConstraint final : public Constraint {
 public:
  UnionConstraint(SourceRange range, std::vector<NodePtr<Constraint>> alts)
      : Constraint(std::move(range)), alternatives_(std::move(alts)) {}

  /**
   *  Function    : alternatives
   *  Description : Computes alternatives from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<Constraint>>&
   */
  const std::vector<NodePtr<Constraint>>& alternatives() const noexcept {
    return alternatives_;
  }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<Constraint>> alternatives_;
};

class IntersectionConstraint final : public Constraint {
 public:
  /**
   *  Function    : move
   *  Description : Computes move from (range, parts_(std::move(parts)).
   *  Parameters  : range — SourceRange range; parts_(std::move(parts) — std::vector<NodePtr<Constraint>> parts) : Constraint(std::move(range)), parts_(std::move(parts)
   *  Returns     : IntersectionConstraint(SourceRange range, std::vector<NodePtr<Constraint>> parts) : Constraint(std::move(range)), parts_(std::
   */
  IntersectionConstraint(SourceRange range, std::vector<NodePtr<Constraint>> parts)
      : Constraint(std::move(range)), parts_(std::move(parts)) {}

  /**
   *  Function    : parts
   *  Description : Computes parts from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<Constraint>>&
   */
  const std::vector<NodePtr<Constraint>>& parts() const noexcept { return parts_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<Constraint>> parts_;
};

class ExtensibleConstraint final : public Constraint {
 public:
  ExtensibleConstraint(SourceRange range, NodePtr<Constraint> root)
      : Constraint(std::move(range)), root_(std::move(root)) {}

  /**
   *  Function    : get
   *  Description : Computes get from (root_.get().
   *  Parameters  : root_.get( — ) const noexcept { return root_.get(
   *  Returns     : const Constraint* root() const noexcept { return root_.
   */
  const Constraint* root() const noexcept { return root_.get(); }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : tag
   *  Description : Computes tag from (none).
   *  Parameters  : none
   *  Returns     : const std::optional<Tag>&
   */
  const std::optional<Tag>& tag() const noexcept { return tag_; }
  /**
   *  Function    : get
   *  Description : Computes get from (constraint_.get().
   *  Parameters  : constraint_.get( — ) const noexcept { return constraint_.get(
   *  Returns     : const Constraint* constraint() const noexcept { return constraint_.
   */
  const Constraint* constraint() const noexcept { return constraint_.get(); }
  /**
   *  Function    : encoding_instructions
   *  Description : Computes encoding instructions from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<EncodingInstruction>&
   */
  const std::vector<EncodingInstruction>& encoding_instructions() const noexcept {
    return encoding_instructions_;
  }

  /**
   *  Function    : move
   *  Description : Performs move (definition).
   *  Parameters  : tag_ — std::optional<Tag> tag) { tag_
   *  Returns     : void set_tag(std::optional<Tag> tag) { tag_ = std::
   */
  void set_tag(std::optional<Tag> tag) { tag_ = std::move(tag); }
  /**
   *  Function    : move
   *  Description : Performs move (definition).
   *  Parameters  : constraint_ — NodePtr<Constraint> c) { constraint_
   *  Returns     : void set_constraint(NodePtr<Constraint> c) { constraint_ = std::
   */
  void set_constraint(NodePtr<Constraint> c) { constraint_ = std::move(c); }
  /**
   *  Function    : set_encoding_instructions
   *  Description : Performs set encoding instructions (definition).
   *  Parameters  : eis — std::vector<EncodingInstruction> eis
   *  Returns     : void
   */
  void set_encoding_instructions(std::vector<EncodingInstruction> eis) {
    /**
     *  Function    : move
     *  Description : Computes move from (eis).
     *  Parameters  : eis — eis
     *  Returns     : encoding_instructions_ = std::
     */
    encoding_instructions_ = std::move(eis);
  }
  /**
   *  Function    : add_encoding_instruction
   *  Description : Performs add encoding instruction (definition).
   *  Parameters  : ei — EncodingInstruction ei
   *  Returns     : void
   */
  void add_encoding_instruction(EncodingInstruction ei) {
    /**
     *  Function    : move
     *  Description : Computes move from (std::move(ei)).
     *  Parameters  : std::move(ei) — std::move(ei)
     *  Returns     : encoding_instructions_.push_back(std::
     */
    encoding_instructions_.push_back(std::move(ei));
  }

 private:
  std::optional<Tag> tag_;
  NodePtr<Constraint> constraint_;
  std::vector<EncodingInstruction> encoding_instructions_;
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
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

class IntegerType final : public Type {
 public:
  IntegerType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
              std::vector<NamedNumber> named)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        named_numbers_(std::move(named)) {}

  /**
   *  Function    : named_numbers
   *  Description : Computes named numbers from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NamedNumber>&
   */
  const std::vector<NamedNumber>& named_numbers() const noexcept { return named_numbers_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : named_bits
   *  Description : Computes named bits from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NamedNumber>&
   */
  const std::vector<NamedNumber>& named_bits() const noexcept { return named_bits_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NamedNumber> named_bits_;
};

class OctetStringType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

class NullType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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
  UTCTime,
  GeneralizedTime,
};

class StringType final : public Type {
 public:
  StringType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
             StringKind kind)
      : Type(std::move(range), std::move(tag), std::move(constraint)), kind_(kind) {}

  StringKind kind() const noexcept { return kind_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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
  Presence presence() const noexcept { return presence_; }
  /**
   *  Function    : get
   *  Description : Computes get from (default_value_.get().
   *  Parameters  : default_value_.get( — ) const noexcept { return default_value_.get(
   *  Returns     : const Value* default_value() const noexcept { return default_value_.
   */
  const Value* default_value() const noexcept { return default_value_.get(); }
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
  Presence presence_ = Presence::Mandatory;
  NodePtr<Value> default_value_;
};

class ExtensionMarker final : public ComponentItem {
 public:
  using ComponentItem::ComponentItem;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

/// Version brackets [[ ... ]] — nested component list (X.680 extension addition group).
class VersionAdditionGroup final : public ComponentItem {
 public:
  VersionAdditionGroup(SourceRange range, std::vector<NodePtr<ComponentItem>> items)
      : ComponentItem(std::move(range)), items_(std::move(items)) {}

  /**
   *  Function    : items
   *  Description : Computes items from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<ComponentItem>>&
   */
  const std::vector<NodePtr<ComponentItem>>& items() const noexcept { return items_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NodePtr<ComponentItem>> items_;
};

class SequenceType final : public Type {
 public:
  SequenceType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
               std::vector<NodePtr<ComponentItem>> items)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        items_(std::move(items)) {}

  /**
   *  Function    : items
   *  Description : Computes items from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<ComponentItem>>&
   */
  const std::vector<NodePtr<ComponentItem>>& items() const noexcept { return items_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : alternatives
   *  Description : Computes alternatives from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<ComponentItem>>&
   */
  const std::vector<NodePtr<ComponentItem>>& alternatives() const noexcept {
    return alternatives_;
  }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : element
   *  Description : Computes element from (none).
   *  Parameters  : none
   *  Returns     : const Type&
   */
  const Type& element() const { return *element_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : items
   *  Description : Computes items from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NodePtr<ComponentItem>>&
   */
  const std::vector<NodePtr<ComponentItem>>& items() const noexcept { return items_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : element
   *  Description : Computes element from (none).
   *  Parameters  : none
   *  Returns     : const Type&
   */
  const Type& element() const { return *element_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
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

  /**
   *  Function    : root
   *  Description : Computes root from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NamedNumber>&
   */
  const std::vector<NamedNumber>& root() const noexcept { return root_; }
  /**
   *  Function    : extensible
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
  bool extensible() const noexcept { return extensible_; }
  /**
   *  Function    : extensions
   *  Description : Computes extensions from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NamedNumber>&
   */
  const std::vector<NamedNumber>& extensions() const noexcept { return extensions_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NamedNumber> root_;
  bool extensible_ = false;
  std::vector<NamedNumber> extensions_;
};

class ObjectIdentifierType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

class RelativeOidType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

class RealType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

/// X.680 EXTERNAL (associated SEQUENCE, UNIVERSAL 8).
class ExternalType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

/// X.680 EMBEDDED PDV (associated SEQUENCE, UNIVERSAL 11).
class EmbeddedPdvType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

/// X.680 unrestricted CHARACTER STRING (associated SEQUENCE, UNIVERSAL 29).
class CharacterStringType final : public Type {
 public:
  using Type::Type;
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }
};

/// X.681 InstanceOfType: INSTANCE OF DefinedObjectClass
class InstanceOfType final : public Type {
 public:
  InstanceOfType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                 std::string class_name)
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        class_name_(std::move(class_name)) {}

  /**
   *  Function    : class_name
   *  Description : Builds and returns a string for class name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& class_name() const noexcept { return class_name_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::string class_name_;
};

class ObjectIdentifierValue final : public Value {
 public:
  struct Arc {
    std::optional<std::string> name;
    std::optional<std::uint64_t> number;
  };

  ObjectIdentifierValue(SourceRange range, std::vector<Arc> arcs)
      : Value(std::move(range)), arcs_(std::move(arcs)) {}

  /**
   *  Function    : arcs
   *  Description : Computes arcs from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<Arc>&
   */
  const std::vector<Arc>& arcs() const noexcept { return arcs_; }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<Arc> arcs_;
};

/// Actual parameter in a parameterized type reference: Type / Value / ObjectSet.
struct ActualParameter {
  NodePtr<Type> type;
  NodePtr<Value> value;
  std::optional<std::string> object_set_name;
  /// True when the actual was an inline object-set form `{...}` (name may still be set).
  bool inline_object_set = false;
};

class ReferencedType final : public Type {
 public:
  ReferencedType(SourceRange range, std::optional<Tag> tag, NodePtr<Constraint> constraint,
                 std::optional<std::string> module, std::string name,
                 std::vector<ActualParameter> actuals = {})
      : Type(std::move(range), std::move(tag), std::move(constraint)),
        module_(std::move(module)),
        name_(std::move(name)),
        actuals_(std::move(actuals)) {}

  /**
   *  Function    : module
   *  Description : Builds and returns a string for module.
   *  Parameters  : none
   *  Returns     : const std::optional<std::string>&
   */
  const std::optional<std::string>& module() const noexcept { return module_; }
  /**
   *  Function    : name
   *  Description : Builds and returns a string for name.
   *  Parameters  : none
   *  Returns     : const std::string&
   */
  const std::string& name() const noexcept { return name_; }
  /**
   *  Function    : actuals
   *  Description : Computes actuals from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<ActualParameter>&
   */
  const std::vector<ActualParameter>& actuals() const noexcept { return actuals_; }
  /**
   *  Function    : move
   *  Description : Performs move (definition).
   *  Parameters  : actuals_ — std::vector<ActualParameter> actuals) { actuals_
   *  Returns     : void set_actuals(std::vector<ActualParameter> actuals) { actuals_ = std::
   */
  void set_actuals(std::vector<ActualParameter> actuals) { actuals_ = std::move(actuals); }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::optional<std::string> module_;
  std::string name_;
  std::vector<ActualParameter> actuals_;
};

/// X.680 ContentsConstraint: CONTAINING Type [ENCODED BY Value] | ENCODED BY Value.
/// Placed after Type so NodePtr<Type> is a complete type in this header.
class ContentsConstraint final : public Constraint {
 public:
  ContentsConstraint(SourceRange range, NodePtr<Type> contained, NodePtr<Value> encoded_by)
      : Constraint(std::move(range)),
        contained_(std::move(contained)),
        encoded_by_(std::move(encoded_by)) {}

  /**
   *  Function    : get
   *  Description : Computes get from (contained_.get().
   *  Parameters  : contained_.get( — ) const noexcept { return contained_.get(
   *  Returns     : const Type* contained() const noexcept { return contained_.
   */
  const Type* contained() const noexcept { return contained_.get(); }
  /**
   *  Function    : get
   *  Description : Computes get from (encoded_by_.get().
   *  Parameters  : encoded_by_.get( — ) const noexcept { return encoded_by_.get(
   *  Returns     : const Value* encoded_by() const noexcept { return encoded_by_.
   */
  const Value* encoded_by() const noexcept { return encoded_by_.get(); }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  NodePtr<Type> contained_;
  NodePtr<Value> encoded_by_;
};

/// Presence constraint on a WITH COMPONENTS named component (X.680).
enum class ComponentPresence { Unspecified, Present, Absent, Optional };

/// One named component inside WITH COMPONENTS { ... }.
struct NamedComponentConstraint {
  std::string name;
  NodePtr<Constraint> value_constraint;  // may be null
  ComponentPresence presence = ComponentPresence::Unspecified;
  SourceRange range;
};

/// X.680 InnerTypeConstraints: WITH COMPONENTS { NamedConstraint, ... }
class WithComponentsConstraint final : public Constraint {
 public:
  WithComponentsConstraint(SourceRange range, std::vector<NamedComponentConstraint> components)
      : Constraint(std::move(range)), components_(std::move(components)) {}

  /**
   *  Function    : components
   *  Description : Computes components from (none).
   *  Parameters  : none
   *  Returns     : const std::vector<NamedComponentConstraint>&
   */
  const std::vector<NamedComponentConstraint>& components() const noexcept {
    return components_;
  }
  /**
   *  Function    : visit
   *  Description : Performs visit (definition).
   *  Parameters  : this — Visitor& v) const override { v.visit(*this
   *  Returns     : void accept(Visitor& v) const override { v.
   */
  void accept(Visitor& v) const override { v.visit(*this); }

 private:
  std::vector<NamedComponentConstraint> components_;
};

}  // namespace ast
}  // namespace asn1
