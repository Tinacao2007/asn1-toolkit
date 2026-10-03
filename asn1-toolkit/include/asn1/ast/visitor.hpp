#pragma once

namespace asn1 {
namespace ast {

class Module;
class TypeAssignment;
class ValueAssignment;
class ObjectClassAssignment;
class ObjectAssignment;
class ObjectSetAssignment;
class FieldSpec;
class ObjectClassDefn;
class ObjectDefn;
class ObjectSetDefn;
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
class ExternalType;
class EmbeddedPdvType;
class CharacterStringType;
class InstanceOfType;
class ReferencedType;
class ObjectClassFieldType;
class Component;
class ExtensionMarker;
class VersionAdditionGroup;
class ValueRangeConstraint;
class SingleValueConstraint;
class SizeConstraint;
class UnionConstraint;
class IntersectionConstraint;
class ExtensibleConstraint;
class ContentsConstraint;
class WithComponentsConstraint;
class TableConstraint;
class ComponentRelationConstraint;
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
  virtual void visit(const ObjectClassAssignment&) = 0;
  virtual void visit(const ObjectAssignment&) = 0;
  virtual void visit(const ObjectSetAssignment&) = 0;

  virtual void visit(const FieldSpec&) = 0;
  virtual void visit(const ObjectClassDefn&) = 0;
  virtual void visit(const ObjectDefn&) = 0;
  virtual void visit(const ObjectSetDefn&) = 0;

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
  virtual void visit(const ExternalType&) = 0;
  virtual void visit(const EmbeddedPdvType&) = 0;
  virtual void visit(const CharacterStringType&) = 0;
  virtual void visit(const InstanceOfType&) = 0;
  virtual void visit(const ReferencedType&) = 0;
  virtual void visit(const ObjectClassFieldType&) = 0;

  virtual void visit(const Component&) = 0;
  virtual void visit(const ExtensionMarker&) = 0;
  virtual void visit(const VersionAdditionGroup&) = 0;

  virtual void visit(const ValueRangeConstraint&) = 0;
  virtual void visit(const SingleValueConstraint&) = 0;
  virtual void visit(const SizeConstraint&) = 0;
  virtual void visit(const UnionConstraint&) = 0;
  virtual void visit(const IntersectionConstraint&) = 0;
  virtual void visit(const ExtensibleConstraint&) = 0;
  virtual void visit(const ContentsConstraint&) = 0;
  virtual void visit(const WithComponentsConstraint&) = 0;
  virtual void visit(const TableConstraint&) = 0;
  virtual void visit(const ComponentRelationConstraint&) = 0;

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
