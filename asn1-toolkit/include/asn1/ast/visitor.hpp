/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ast/visitor.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Visitor interface over concrete AST node types.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (parse tree
**                 produced/consumed here).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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
  /**
   *  Function    : ~Visitor
   *  Description : Destroys the Visitor instance.
   *  Parameters  : none
   *  Returns     : virtual
   */
  virtual ~Visitor() = default;

  /**
   *  Function    : visit
   *  Description : Computes visit from (Module).
   *  Parameters  : Module — const Module&
   *  Returns     : virtual void
   */
  virtual void visit(const Module&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (TypeAssignment).
   *  Parameters  : TypeAssignment — const TypeAssignment&
   *  Returns     : virtual void
   */
  virtual void visit(const TypeAssignment&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ValueAssignment).
   *  Parameters  : ValueAssignment — const ValueAssignment&
   *  Returns     : virtual void
   */
  virtual void visit(const ValueAssignment&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectClassAssignment).
   *  Parameters  : ObjectClassAssignment — const ObjectClassAssignment&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectClassAssignment&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectAssignment).
   *  Parameters  : ObjectAssignment — const ObjectAssignment&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectAssignment&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectSetAssignment).
   *  Parameters  : ObjectSetAssignment — const ObjectSetAssignment&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectSetAssignment&) = 0;

  /**
   *  Function    : visit
   *  Description : Computes visit from (FieldSpec).
   *  Parameters  : FieldSpec — const FieldSpec&
   *  Returns     : virtual void
   */
  virtual void visit(const FieldSpec&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectClassDefn).
   *  Parameters  : ObjectClassDefn — const ObjectClassDefn&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectClassDefn&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectDefn).
   *  Parameters  : ObjectDefn — const ObjectDefn&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectDefn&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectSetDefn).
   *  Parameters  : ObjectSetDefn — const ObjectSetDefn&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectSetDefn&) = 0;

  /**
   *  Function    : visit
   *  Description : Computes visit from (BooleanType).
   *  Parameters  : BooleanType — const BooleanType&
   *  Returns     : virtual void
   */
  virtual void visit(const BooleanType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (IntegerType).
   *  Parameters  : IntegerType — const IntegerType&
   *  Returns     : virtual void
   */
  virtual void visit(const IntegerType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (BitStringType).
   *  Parameters  : BitStringType — const BitStringType&
   *  Returns     : virtual void
   */
  virtual void visit(const BitStringType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (OctetStringType).
   *  Parameters  : OctetStringType — const OctetStringType&
   *  Returns     : virtual void
   */
  virtual void visit(const OctetStringType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (NullType).
   *  Parameters  : NullType — const NullType&
   *  Returns     : virtual void
   */
  virtual void visit(const NullType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (StringType).
   *  Parameters  : StringType — const StringType&
   *  Returns     : virtual void
   */
  virtual void visit(const StringType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (SequenceType).
   *  Parameters  : SequenceType — const SequenceType&
   *  Returns     : virtual void
   */
  virtual void visit(const SequenceType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ChoiceType).
   *  Parameters  : ChoiceType — const ChoiceType&
   *  Returns     : virtual void
   */
  virtual void visit(const ChoiceType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (SequenceOfType).
   *  Parameters  : SequenceOfType — const SequenceOfType&
   *  Returns     : virtual void
   */
  virtual void visit(const SequenceOfType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (SetType).
   *  Parameters  : SetType — const SetType&
   *  Returns     : virtual void
   */
  virtual void visit(const SetType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (SetOfType).
   *  Parameters  : SetOfType — const SetOfType&
   *  Returns     : virtual void
   */
  virtual void visit(const SetOfType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (EnumeratedType).
   *  Parameters  : EnumeratedType — const EnumeratedType&
   *  Returns     : virtual void
   */
  virtual void visit(const EnumeratedType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectIdentifierType).
   *  Parameters  : ObjectIdentifierType — const ObjectIdentifierType&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectIdentifierType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (RelativeOidType).
   *  Parameters  : RelativeOidType — const RelativeOidType&
   *  Returns     : virtual void
   */
  virtual void visit(const RelativeOidType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (RealType).
   *  Parameters  : RealType — const RealType&
   *  Returns     : virtual void
   */
  virtual void visit(const RealType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ExternalType).
   *  Parameters  : ExternalType — const ExternalType&
   *  Returns     : virtual void
   */
  virtual void visit(const ExternalType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (EmbeddedPdvType).
   *  Parameters  : EmbeddedPdvType — const EmbeddedPdvType&
   *  Returns     : virtual void
   */
  virtual void visit(const EmbeddedPdvType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (CharacterStringType).
   *  Parameters  : CharacterStringType — const CharacterStringType&
   *  Returns     : virtual void
   */
  virtual void visit(const CharacterStringType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (InstanceOfType).
   *  Parameters  : InstanceOfType — const InstanceOfType&
   *  Returns     : virtual void
   */
  virtual void visit(const InstanceOfType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ReferencedType).
   *  Parameters  : ReferencedType — const ReferencedType&
   *  Returns     : virtual void
   */
  virtual void visit(const ReferencedType&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectClassFieldType).
   *  Parameters  : ObjectClassFieldType — const ObjectClassFieldType&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectClassFieldType&) = 0;

  /**
   *  Function    : visit
   *  Description : Computes visit from (Component).
   *  Parameters  : Component — const Component&
   *  Returns     : virtual void
   */
  virtual void visit(const Component&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ExtensionMarker).
   *  Parameters  : ExtensionMarker — const ExtensionMarker&
   *  Returns     : virtual void
   */
  virtual void visit(const ExtensionMarker&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (VersionAdditionGroup).
   *  Parameters  : VersionAdditionGroup — const VersionAdditionGroup&
   *  Returns     : virtual void
   */
  virtual void visit(const VersionAdditionGroup&) = 0;

  /**
   *  Function    : visit
   *  Description : Computes visit from (ValueRangeConstraint).
   *  Parameters  : ValueRangeConstraint — const ValueRangeConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const ValueRangeConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (SingleValueConstraint).
   *  Parameters  : SingleValueConstraint — const SingleValueConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const SingleValueConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (SizeConstraint).
   *  Parameters  : SizeConstraint — const SizeConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const SizeConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (UnionConstraint).
   *  Parameters  : UnionConstraint — const UnionConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const UnionConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (IntersectionConstraint).
   *  Parameters  : IntersectionConstraint — const IntersectionConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const IntersectionConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ExtensibleConstraint).
   *  Parameters  : ExtensibleConstraint — const ExtensibleConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const ExtensibleConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ContentsConstraint).
   *  Parameters  : ContentsConstraint — const ContentsConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const ContentsConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (WithComponentsConstraint).
   *  Parameters  : WithComponentsConstraint — const WithComponentsConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const WithComponentsConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (TableConstraint).
   *  Parameters  : TableConstraint — const TableConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const TableConstraint&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ComponentRelationConstraint).
   *  Parameters  : ComponentRelationConstraint — const ComponentRelationConstraint&
   *  Returns     : virtual void
   */
  virtual void visit(const ComponentRelationConstraint&) = 0;

  /**
   *  Function    : visit
   *  Description : Computes visit from (IntegerValue).
   *  Parameters  : IntegerValue — const IntegerValue&
   *  Returns     : virtual void
   */
  virtual void visit(const IntegerValue&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (BooleanValue).
   *  Parameters  : BooleanValue — const BooleanValue&
   *  Returns     : virtual void
   */
  virtual void visit(const BooleanValue&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (StringValue).
   *  Parameters  : StringValue — const StringValue&
   *  Returns     : virtual void
   */
  virtual void visit(const StringValue&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (BitOrOctetValue).
   *  Parameters  : BitOrOctetValue — const BitOrOctetValue&
   *  Returns     : virtual void
   */
  virtual void visit(const BitOrOctetValue&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ValueReference).
   *  Parameters  : ValueReference — const ValueReference&
   *  Returns     : virtual void
   */
  virtual void visit(const ValueReference&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (NamedValue).
   *  Parameters  : NamedValue — const NamedValue&
   *  Returns     : virtual void
   */
  virtual void visit(const NamedValue&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (NamedValueList).
   *  Parameters  : NamedValueList — const NamedValueList&
   *  Returns     : virtual void
   */
  virtual void visit(const NamedValueList&) = 0;
  /**
   *  Function    : visit
   *  Description : Computes visit from (ObjectIdentifierValue).
   *  Parameters  : ObjectIdentifierValue — const ObjectIdentifierValue&
   *  Returns     : virtual void
   */
  virtual void visit(const ObjectIdentifierValue&) = 0;
};

}  // namespace ast
}  // namespace asn1
