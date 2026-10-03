#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace asn1 {
namespace ir {

using TypeId = std::uint32_t;
using ValueId = std::uint32_t;

constexpr TypeId kInvalidType = static_cast<TypeId>(-1);
constexpr ValueId kInvalidValue = static_cast<ValueId>(-1);

/// Sign-magnitude integer for unconstrained ASN.1 INTEGER bounds.
/// Phase 4 stores decimal text and optional host int64 when it fits.
struct BigInt {
  bool negative = false;
  std::string digits;  // absolute value, decimal, no leading zeros (except "0")
  std::optional<std::int64_t> as_i64;

  static BigInt from_decimal(std::string text, bool negative);
  static BigInt from_i64(std::int64_t v);

  std::string to_string() const;
  bool operator==(const BigInt& o) const;
  bool operator!=(const BigInt& o) const { return !(*this == o); }
  /// Three-way compare: -1 if *this < o, 0 if equal, +1 if *this > o.
  int compare(const BigInt& o) const;
  bool operator<(const BigInt& o) const { return compare(o) < 0; }
  bool operator<=(const BigInt& o) const { return compare(o) <= 0; }
  bool operator>(const BigInt& o) const { return compare(o) > 0; }
  bool operator>=(const BigInt& o) const { return compare(o) >= 0; }
};

enum class TagClass { Universal, Application, Context, Private };

struct Tag {
  TagClass cls = TagClass::Universal;
  std::uint64_t number = 0;
  bool is_explicit = false;
};

enum class TypeKind {
  Boolean,
  Integer,
  BitString,
  OctetString,
  Null,
  String,
  Sequence,
  Choice,
  SequenceOf,
  Set,
  SetOf,
  Enumerated,
  ObjectIdentifier,
  RelativeOid,
  Real,
  ObjectClassField,  // Class.&field — open type or fixed type
  InstanceOf,        // INSTANCE OF DefinedObjectClass
  Referenced,  // should be rare after resolution; kept if unresolved
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

enum class Presence { Mandatory, Optional, Default };

/// How PER should encode an integer / size determinant (X.691).
enum class PerBoundKind {
  Unconstrained,     // no lower or upper
  SemiConstrained,   // lower bound only
  Constrained,       // both bounds (possibly after alphabet compaction)
};

struct IntegerInterval {
  std::optional<BigInt> lower;  // nullopt = MIN
  std::optional<BigInt> upper;  // nullopt = MAX

  bool contains(const BigInt& v) const;
  bool overlaps_or_adjacent(const IntegerInterval& o) const;
  IntegerInterval merged_with(const IntegerInterval& o) const;
  std::optional<IntegerInterval> intersected_with(const IntegerInterval& o) const;
};

struct ConstraintDesc {
  /// Normalized root intervals (sorted, disjoint, merged).
  std::vector<IntegerInterval> root_ranges;
  bool extensible = false;
  bool is_size = false;
  bool statically_foldable = true;
  /// True when the empty set results (e.g. contradictory intersection).
  bool empty = false;

  /// Envelope bounds (min lower / max upper across root_ranges). Convenient for
  /// host_bits and simple PER; prefer root_ranges when multiple intervals exist.
  std::optional<BigInt> lower;
  std::optional<BigInt> upper;

  PerBoundKind per_kind() const;
  /// ub - lb as unsigned when both bounds fit in int64 and lb <= ub.
  std::optional<std::uint64_t> constrained_span() const;
  bool is_single_interval() const { return root_ranges.size() == 1; }
  void recompute_envelope();
};

struct NamedNumber {
  std::string name;
  BigInt value;
};

struct IntegerDesc {
  ConstraintDesc constraint;
  std::vector<NamedNumber> named_numbers;
  std::optional<int> host_bits;  // 8/16/32/64 when span fits
  bool is_signed = true;
};

struct BitStringDesc {
  ConstraintDesc size;
  std::vector<NamedNumber> named_bits;
  /// Contained type from BIT STRING (CONTAINING Type), or kInvalidType.
  TypeId containing = kInvalidType;
};

struct OctetStringDesc {
  ConstraintDesc size;
  /// Contained type from OCTET STRING (CONTAINING Type), or kInvalidType.
  TypeId containing = kInvalidType;
};

struct StringDesc {
  StringKind kind = StringKind::UTF8String;
  ConstraintDesc size;
};

struct BooleanDesc {};
struct NullDesc {};

/// X.697 JER encoding-instruction effects recorded on a type.
struct JerEncoding {
  bool array = false;      // [ARRAY] SEQUENCE
  bool base64 = false;     // [BASE64] OCTET STRING
  bool object = false;     // [OBJECT] SET OF
  bool unwrapped = false;  // [UNWRAPPED] CHOICE
  enum class TextForm { AsIs, Capitalized, Uppercased, Lowercased, Literal };
  TextForm text_form = TextForm::AsIs;  // [TEXT ...] ENUMERATED
  std::string text_literal;
};

/// X.693 EXTENDED-XER encoding-instruction effects on a type / field.
struct ExerEncoding {
  bool attribute = false;   // [ATTRIBUTE]
  bool base64 = false;      // [BASE64]
  bool text = false;        // [TEXT]
  bool use_number = false;  // [USE-NUMBER]
  bool list = false;        // [LIST]
  bool untagged = false;    // [UNTAGGED]
  bool use_nil = false;     // [USE-NIL]
};

struct InstanceOfDesc {
  std::string class_name;  // e.g. TYPE-IDENTIFIER
};

struct Field {
  std::string name;
  TypeId type = kInvalidType;
  Presence presence = Presence::Mandatory;
  Tag tag{};
  std::optional<ValueId> default_value;
  /// JER/XER [NAME ...] applied to this component / alternative.
  enum class JerNameForm { AsIs, Capitalized, Uppercased, Lowercased, Literal };
  JerNameForm jer_name_form = JerNameForm::AsIs;
  std::string jer_name_literal;
  ExerEncoding exer;
};

struct SequenceDesc {
  std::vector<Field> root;
  bool extensible = false;
  std::vector<std::vector<Field>> extension_groups;
  std::vector<Field> trailing_root;
};

struct ChoiceDesc {
  std::vector<Field> alternatives;  // root alternatives; presence always Mandatory
  bool extensible = false;
  std::vector<Field> extensions;  // extension alternatives after "..."
};

struct OfDesc {
  TypeId element = kInvalidType;
  ConstraintDesc size;
};

struct EnumeratedDesc {
  /// Root enumerations in declaration order; values are complete (auto-assigned).
  std::vector<NamedNumber> root;
  bool extensible = false;
  std::vector<NamedNumber> extensions;
};

struct OidDesc {
  ConstraintDesc size;  // SIZE on encoded length is uncommon; kept for symmetry
};

/// OER/COER IEEE fixed-length REAL forms (X.696) derived from WITH COMPONENTS.
enum class RealIeeeForm {
  Unconstrained,  // length determinant + BER/DER content
  Binary32,       // IEEE-754 binary32, 4 octets
  Binary64,       // IEEE-754 binary64, 8 octets
};

struct RealDesc {
  RealIeeeForm ieee_form = RealIeeeForm::Unconstrained;
};

struct ObjectClassFieldDesc {
  std::string class_name;
  std::string field_name;
  /// True when the field is a type field (&Type) → PER/BER open type.
  bool open_type = true;
  /// When !open_type, the fixed ASN.1 type of the value field.
  TypeId fixed_type = kInvalidType;
};

struct ReferencedDesc {
  std::string module;
  std::string name;
  TypeId resolved = kInvalidType;
};

enum class ClassFieldKind { TypeField, FixedTypeValueField };

struct ClassField {
  ClassFieldKind kind = ClassFieldKind::TypeField;
  std::string name;
  TypeId fixed_type = kInvalidType;
  bool unique = false;
  Presence presence = Presence::Mandatory;
};

struct ObjectClassInfo {
  std::string module;
  std::string name;
  std::vector<ClassField> fields;
};

struct ObjectFieldSetting {
  std::string field_name;
  TypeId type_setting = kInvalidType;  // for type fields
  ValueId value_setting = kInvalidValue;
};

struct ObjectInfo {
  std::string module;
  std::string name;
  std::string class_name;
  std::vector<ObjectFieldSetting> settings;
};

struct ObjectSetInfo {
  std::string module;
  std::string name;
  std::string class_name;
  std::vector<std::string> object_refs;
  bool extensible = false;
};

struct Type {
  TypeKind kind = TypeKind::Null;
  std::string module;
  std::string name;  // empty => anonymous
  Tag tag{};         // outermost effective tag when applicable
  bool recursive = false;
  JerEncoding jer;
  ExerEncoding exer;

  IntegerDesc integer;
  BitStringDesc bit_string;
  OctetStringDesc octet_string;
  StringDesc string;
  BooleanDesc boolean;
  NullDesc null;
  SequenceDesc sequence;
  ChoiceDesc choice;
  OfDesc sequence_of;
  SequenceDesc set;  // SET reuses SEQUENCE field layout
  OfDesc set_of;
  EnumeratedDesc enumerated;
  OidDesc object_identifier;
  OidDesc relative_oid;
  RealDesc real;
  ObjectClassFieldDesc object_class_field;
  InstanceOfDesc instance_of;
  ReferencedDesc referenced;
};

enum class ValueKind { Integer, Boolean, String, BitOrOctet, Reference, NamedList };

struct Value {
  ValueKind kind = ValueKind::Integer;
  std::string module;
  std::string name;
  BigInt integer;
  bool boolean = false;
  std::string text;
  TypeId type = kInvalidType;
};

struct ModuleInfo {
  std::string name;
  enum class TagDefault { Explicit, Implicit, Automatic } tag_default =
      TagDefault::Explicit;
  bool extensibility_implied = false;
  bool jer_instructions = false;
  bool xer_instructions = false;
  std::vector<TypeId> types;
  std::vector<ValueId> values;
};

class TypeArena {
 public:
  TypeId add(Type type);
  ValueId add_value(Value value);

  Type& get(TypeId id);
  const Type& get(TypeId id) const;
  Value& get_value(ValueId id);
  const Value& get_value(ValueId id) const;

  std::size_t type_count() const noexcept { return types_.size(); }
  std::size_t value_count() const noexcept { return values_.size(); }

  const std::vector<Type>& types() const noexcept { return types_; }
  const std::vector<Value>& values() const noexcept { return values_; }

 private:
  std::vector<Type> types_;
  std::vector<Value> values_;
};

struct Model {
  TypeArena arena;
  std::vector<ModuleInfo> modules;
  std::vector<ObjectClassInfo> object_classes;
  std::vector<ObjectInfo> objects;
  std::vector<ObjectSetInfo> object_sets;

  /// Named types in declaration order across modules.
  std::vector<TypeId> exported_types;
};

}  // namespace ir
}  // namespace asn1
