#pragma once

namespace asn1 {
namespace ast {

class Module;
class TypeAssignment;
class ValueAssignment;
class BooleanType;
class IntegerType;
class BitStringType;
class OctetStringType;
class NullType;
class StringType;
class SequenceType;
class ChoiceType;
class SequenceOfType;
class SetType;
class SetOfType;
class EnumeratedType;
class ObjectIdentifierType;
class RelativeOidType;
class RealType;
class ReferencedType;
class Component;
class ExtensionMarker;
class ValueRangeConstraint;
class SingleValueConstraint;
class SizeConstraint;
class UnionConstraint;
class IntersectionConstraint;
class ExtensibleConstraint;
class IntegerValue;
class BooleanValue;
class StringValue;
class BitOrOctetValue;
class ValueReference;
class NamedValue;
class NamedValueList;
class ObjectIdentifierValue;

class Visitor {
 public:
  virtual ~Visitor() = default;

  virtual void visit(const Module&) = 0;
  virtual void visit(const TypeAssignment&) = 0;
  virtual void visit(const ValueAssignment&) = 0;

  virtual void visit(const BooleanType&) = 0;
  virtual void visit(const IntegerType&) = 0;
  virtual void visit(const BitStringType&) = 0;
  virtual void visit(const OctetStringType&) = 0;
  virtual void visit(const NullType&) = 0;
  virtual void visit(const StringType&) = 0;
  virtual void visit(const SequenceType&) = 0;
  virtual void visit(const ChoiceType&) = 0;
  virtual void visit(const SequenceOfType&) = 0;
  virtual void visit(const SetType&) = 0;
  virtual void visit(const SetOfType&) = 0;
  virtual void visit(const EnumeratedType&) = 0;
  virtual void visit(const ObjectIdentifierType&) = 0;
  virtual void visit(const RelativeOidType&) = 0;
  virtual void visit(const RealType&) = 0;
  virtual void visit(const ReferencedType&) = 0;

  virtual void visit(const Component&) = 0;
  virtual void visit(const ExtensionMarker&) = 0;

  virtual void visit(const ValueRangeConstraint&) = 0;
  virtual void visit(const SingleValueConstraint&) = 0;
  virtual void visit(const SizeConstraint&) = 0;
  virtual void visit(const UnionConstraint&) = 0;
  virtual void visit(const IntersectionConstraint&) = 0;
  virtual void visit(const ExtensibleConstraint&) = 0;

  virtual void visit(const IntegerValue&) = 0;
  virtual void visit(const BooleanValue&) = 0;
  virtual void visit(const StringValue&) = 0;
  virtual void visit(const BitOrOctetValue&) = 0;
  virtual void visit(const ValueReference&) = 0;
  virtual void visit(const NamedValue&) = 0;
  virtual void visit(const NamedValueList&) = 0;
  virtual void visit(const ObjectIdentifierValue&) = 0;
};

}  // namespace ast
}  // namespace asn1
