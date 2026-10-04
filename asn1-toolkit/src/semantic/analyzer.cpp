/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/semantic/analyzer.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Analyzer implementation lowering AST to ir::Model.
**
** Specification: ITU-T X.680 — name and module semantics;
**                 ITU-T X.681 / X.683 — object and parameterization
**                 semantics where implemented.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/semantic/analyzer.hpp>

#include <asn1/ast/encoding.hpp>
#include <asn1/ast/ioc.hpp>
#include <asn1/ast/type.hpp>
#include <asn1/constraints/normalize.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace asn1 {
namespace {

const ast::ComponentRelationConstraint* extract_component_relation_constraint(
    const ast::Constraint* c) {
  if (!c) {
    return nullptr;
  }
  if (const auto* crc = dynamic_cast<const ast::ComponentRelationConstraint*>(c)) {
    return crc;
  }
  return nullptr;
}

/// Extract a closed integer interval from a simple value / range constraint.
/**
 *  Function    : extract_i64_bounds
 *  Description : Returns a boolean result from c, lo, hi.
 *  Parameters  : c — const ast::Constraint* c; lo — std::int64_t& lo; hi — std::int64_t& hi
 *  Returns     : bool
 */
bool extract_i64_bounds(const ast::Constraint* c, std::int64_t& lo, std::int64_t& hi) {
  if (!c) {
    return false;
  }
  if (const auto* single = dynamic_cast<const ast::SingleValueConstraint*>(c)) {
    const auto* iv = dynamic_cast<const ast::IntegerValue*>(&single->value());
    if (!iv) {
      return false;
    }
    auto v = ir::BigInt::from_decimal(iv->text(), iv->negative());
    if (!v.as_i64) {
      return false;
    }
    lo = hi = *v.as_i64;
    return true;
  }
  if (const auto* range = dynamic_cast<const ast::ValueRangeConstraint*>(c)) {
    if (!range->lower() || !range->upper()) {
      return false;
    }
    const auto* lo_v = dynamic_cast<const ast::IntegerValue*>(range->lower());
    const auto* hi_v = dynamic_cast<const ast::IntegerValue*>(range->upper());
    if (!lo_v || !hi_v) {
      return false;
    }
    auto lo_b = ir::BigInt::from_decimal(lo_v->text(), lo_v->negative());
    auto hi_b = ir::BigInt::from_decimal(hi_v->text(), hi_v->negative());
    if (!lo_b.as_i64 || !hi_b.as_i64) {
      return false;
    }
    lo = *lo_b.as_i64;
    hi = *hi_b.as_i64;
    return true;
  }
  if (const auto* u = dynamic_cast<const ast::UnionConstraint*>(c)) {
    if (u->alternatives().size() == 1) {
      return extract_i64_bounds(u->alternatives()[0].get(), lo, hi);
    }
  }
  if (const auto* inter = dynamic_cast<const ast::IntersectionConstraint*>(c)) {
    if (inter->parts().size() == 1) {
      return extract_i64_bounds(inter->parts()[0].get(), lo, hi);
    }
  }
  return false;
}

/**
 *  Function    : universal_tag_number_for
 *  Description : Computes universal tag number for from (kind).
 *  Parameters  : kind — ir::TypeKind kind
 *  Returns     : static std::uint64_t
 */
static std::uint64_t universal_tag_number_for(ir::TypeKind kind) {
  switch (kind) {
    case ir::TypeKind::Boolean: return 1;
    case ir::TypeKind::Integer: return 2;
    case ir::TypeKind::BitString: return 3;
    case ir::TypeKind::OctetString: return 4;
    case ir::TypeKind::Null: return 5;
    case ir::TypeKind::ObjectIdentifier: return 6;
    case ir::TypeKind::InstanceOf: return 8;
    case ir::TypeKind::Real: return 9;
    case ir::TypeKind::Enumerated: return 10;
    case ir::TypeKind::RelativeOid: return 13;
    case ir::TypeKind::Sequence:
    case ir::TypeKind::SequenceOf: return 16;
    case ir::TypeKind::Set:
    case ir::TypeKind::SetOf: return 17;
    case ir::TypeKind::String: return 12;
    default: return 0;
  }
}

/**
 *  Function    : classify_real_with_components
 *  Description : Computes classify real with components from (wc).
 *  Parameters  : wc — const ast::WithComponentsConstraint& wc
 *  Returns     : ir::RealIeeeForm
 */
ir::RealIeeeForm classify_real_with_components(const ast::WithComponentsConstraint& wc) {
  std::optional<std::int64_t> mant_lo, mant_hi, base, exp_lo, exp_hi;
  for (const auto& c : wc.components()) {
    std::int64_t lo = 0;
    std::int64_t hi = 0;
    if (!c.value_constraint || !extract_i64_bounds(c.value_constraint.get(), lo, hi)) {
      continue;
    }
    if (c.name == "mantissa") {
      mant_lo = lo;
      mant_hi = hi;
    } else if (c.name == "base") {
      if (lo == hi) {
        base = lo;
      }
    } else if (c.name == "exponent") {
      exp_lo = lo;
      exp_hi = hi;
    }
  }
  if (!mant_lo || !mant_hi || !base || !exp_lo || !exp_hi) {
    return ir::RealIeeeForm::Unconstrained;
  }
  if (*base == 2 && *mant_lo >= -16777215 && *mant_hi <= 16777215 && *exp_lo >= -149 &&
      *exp_hi <= 104) {
    return ir::RealIeeeForm::Binary32;
  }
  if (*base == 2 && *mant_lo >= -9007199254740991LL && *mant_hi <= 9007199254740991LL &&
      *exp_lo >= -1074 && *exp_hi <= 971) {
    return ir::RealIeeeForm::Binary64;
  }
  return ir::RealIeeeForm::Unconstrained;
}

/**
 *  Function    : real_ieee_form_from_constraint
 *  Description : Computes real ieee form from constraint from (c).
 *  Parameters  : c — const ast::Constraint* c
 *  Returns     : ir::RealIeeeForm
 */
ir::RealIeeeForm real_ieee_form_from_constraint(const ast::Constraint* c) {
  if (!c) {
    return ir::RealIeeeForm::Unconstrained;
  }
  if (const auto* wc = dynamic_cast<const ast::WithComponentsConstraint*>(c)) {
    return classify_real_with_components(*wc);
  }
  if (const auto* inter = dynamic_cast<const ast::IntersectionConstraint*>(c)) {
    for (const auto& p : inter->parts()) {
      auto f = real_ieee_form_from_constraint(p.get());
      if (f != ir::RealIeeeForm::Unconstrained) {
        return f;
      }
    }
  }
  if (const auto* u = dynamic_cast<const ast::UnionConstraint*>(c)) {
    if (u->alternatives().size() == 1) {
      return real_ieee_form_from_constraint(u->alternatives()[0].get());
    }
  }
  return ir::RealIeeeForm::Unconstrained;
}

/**
 *  Function    : universal
 *  Description : Computes universal from (number).
 *  Parameters  : number — std::uint64_t number
 *  Returns     : ir::Tag
 */
ir::Tag universal(std::uint64_t number) {
  return ir::Tag{ir::TagClass::Universal, number, false};
}

/**
 *  Function    : builtin_tag_for
 *  Description : Computes builtin tag for from (type).
 *  Parameters  : type — const ast::Type& type
 *  Returns     : ir::Tag
 */
ir::Tag builtin_tag_for(const ast::Type& type) {
  if (dynamic_cast<const ast::BooleanType*>(&type)) {
    return universal(1);
  }
  if (dynamic_cast<const ast::IntegerType*>(&type)) {
    return universal(2);
  }
  if (dynamic_cast<const ast::BitStringType*>(&type)) {
    return universal(3);
  }
  if (dynamic_cast<const ast::OctetStringType*>(&type)) {
    return universal(4);
  }
  if (dynamic_cast<const ast::NullType*>(&type)) {
    return universal(5);
  }
  if (dynamic_cast<const ast::ObjectIdentifierType*>(&type)) {
    return universal(6);
  }
  if (dynamic_cast<const ast::RealType*>(&type)) {
    return universal(9);
  }
  if (dynamic_cast<const ast::EnumeratedType*>(&type)) {
    return universal(10);
  }
  if (dynamic_cast<const ast::EmbeddedPdvType*>(&type)) {
    return universal(11);
  }
  if (dynamic_cast<const ast::RelativeOidType*>(&type)) {
    return universal(13);
  }
  if (dynamic_cast<const ast::CharacterStringType*>(&type)) {
    return universal(29);
  }
  if (dynamic_cast<const ast::ExternalType*>(&type) ||
      /**
       *  Function    : InstanceOfType*>
       *  Description : Constructs or initializes InstanceOfType*>.
       *  Parameters  : none
       *  Returns     : dynamic_cast<const ast::
       */
      dynamic_cast<const ast::InstanceOfType*>(&type)) {
    return universal(8);
  }
  if (dynamic_cast<const ast::SequenceOfType*>(&type) ||
      /**
       *  Function    : SequenceType*>
       *  Description : Constructs or initializes SequenceType*>.
       *  Parameters  : none
       *  Returns     : dynamic_cast<const ast::
       */
      dynamic_cast<const ast::SequenceType*>(&type)) {
    return universal(16);
  }
  if (dynamic_cast<const ast::SetOfType*>(&type) ||
      /**
       *  Function    : SetType*>
       *  Description : Constructs or initializes SetType*>.
       *  Parameters  : none
       *  Returns     : dynamic_cast<const ast::
       */
      dynamic_cast<const ast::SetType*>(&type)) {
    return universal(17);
  }
  // Object class field types are open / untagged at the universal level by default.
  if (dynamic_cast<const ast::ObjectClassFieldType*>(&type)) {
    return ir::Tag{};
  }
  if (const auto* s = dynamic_cast<const ast::StringType*>(&type)) {
    switch (s->kind()) {
      case ast::StringKind::UTF8String:
        return universal(12);
      case ast::StringKind::NumericString:
        return universal(18);
      case ast::StringKind::PrintableString:
        return universal(19);
      case ast::StringKind::TeletexString:
        return universal(20);
      case ast::StringKind::VideotexString:
        return universal(21);
      case ast::StringKind::IA5String:
        return universal(22);
      case ast::StringKind::GraphicString:
        return universal(25);
      case ast::StringKind::VisibleString:
        return universal(26);
      case ast::StringKind::GeneralString:
        return universal(27);
      case ast::StringKind::UniversalString:
        return universal(28);
      case ast::StringKind::BMPString:
        return universal(30);
      case ast::StringKind::UTCTime:
        return universal(23);
      case ast::StringKind::GeneralizedTime:
        return universal(24);
    }
  }
  return universal(0);
}

/**
 *  Function    : parse_u64
 *  Description : Returns a boolean result from text, out.
 *  Parameters  : text — const std::string& text; out — std::uint64_t& out
 *  Returns     : bool
 */
bool parse_u64(const std::string& text, std::uint64_t& out) {
  if (text.empty()) {
    return false;
  }
  out = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return false;
    }
    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
    if (out > (UINT64_MAX - digit) / 10u) {
      return false;
    }
    out = out * 10u + digit;
  }
  return true;
}

/**
 *  Function    : map_string_kind
 *  Description : Computes map string kind from (k).
 *  Parameters  : k — ast::StringKind k
 *  Returns     : ir::StringKind
 */
ir::StringKind map_string_kind(ast::StringKind k) {
  switch (k) {
    case ast::StringKind::UTF8String:
      return ir::StringKind::UTF8String;
    case ast::StringKind::IA5String:
      return ir::StringKind::IA5String;
    case ast::StringKind::PrintableString:
      return ir::StringKind::PrintableString;
    case ast::StringKind::VisibleString:
      return ir::StringKind::VisibleString;
    case ast::StringKind::NumericString:
      return ir::StringKind::NumericString;
    case ast::StringKind::TeletexString:
      return ir::StringKind::TeletexString;
    case ast::StringKind::VideotexString:
      return ir::StringKind::VideotexString;
    case ast::StringKind::GraphicString:
      return ir::StringKind::GraphicString;
    case ast::StringKind::GeneralString:
      return ir::StringKind::GeneralString;
    case ast::StringKind::BMPString:
      return ir::StringKind::BMPString;
    case ast::StringKind::UniversalString:
      return ir::StringKind::UniversalString;
    case ast::StringKind::UTCTime:
      return ir::StringKind::UTCTime;
    case ast::StringKind::GeneralizedTime:
      return ir::StringKind::GeneralizedTime;
  }
  return ir::StringKind::UTF8String;
}

/**
 *  Function    : make_ast_tag
 *  Description : Computes make ast tag from (t, module_default, force_explicit).
 *  Parameters  : t — const ast::Tag& t; module_default — ast::TagDefault module_default; force_explicit — bool force_explicit
 *  Returns     : ir::Tag
 */
ir::Tag make_ast_tag(const ast::Tag& t, ast::TagDefault module_default, bool force_explicit) {
  ir::Tag out;
  switch (t.cls) {
    case ast::TagClass::Universal:
      out.cls = ir::TagClass::Universal;
      break;
    case ast::TagClass::Application:
      out.cls = ir::TagClass::Application;
      break;
    case ast::TagClass::Context:
      out.cls = ir::TagClass::Context;
      break;
    case ast::TagClass::Private:
      out.cls = ir::TagClass::Private;
      break;
  }
  parse_u64(t.number_text, out.number);
  if (force_explicit) {
    out.is_explicit = true;
  } else if (t.mode) {
    out.is_explicit = (*t.mode == ast::TagMode::Explicit);
  } else {
    out.is_explicit = (module_default == ast::TagDefault::Explicit);
  }
  return out;
}

ir::Tag component_tag(const ast::Type& field_type, ast::TagDefault module_default,
                      std::uint64_t auto_index) {
  const bool is_choice = dynamic_cast<const ast::ChoiceType*>(&field_type) != nullptr;
  if (field_type.tag()) {
    return make_ast_tag(*field_type.tag(), module_default, is_choice);
  }
  if (module_default == ast::TagDefault::Automatic) {
    return ir::Tag{ir::TagClass::Context, auto_index, is_choice};
  }
  return builtin_tag_for(field_type);
}

struct LowerCtx {
  ir::Model& model;
  SymbolTable& symbols;
  Diagnostics& diagnostics;
  const ast::Module& mod;
  std::unordered_set<std::string> resolving;
  std::unordered_map<std::string, ir::TypeId> named_ids;
  std::uint32_t synth_serial = 0;
};

/**
 *  Function    : synth_type_name
 *  Description : Builds and returns a string for synth type name.
 *  Parameters  : ctx — LowerCtx& ctx; assigned — const std::string& assigned; kind — const char* kind
 *  Returns     : std::string
 */
std::string synth_type_name(LowerCtx& ctx, const std::string& assigned, const char* kind) {
  if (!assigned.empty()) {
    return assigned;
  }
  return std::string("_") + kind + std::to_string(++ctx.synth_serial);
}

/// Nested anonymous SEQUENCE/SET/CHOICE need stable IR names for codegen.
/**
 *  Function    : needs_synthetic_nested_name
 *  Description : Returns whether synthetic nested name holds for the given inputs.
 *  Parameters  : type — const ast::Type& type
 *  Returns     : bool
 */
bool needs_synthetic_nested_name(const ast::Type& type) {
  return dynamic_cast<const ast::SequenceType*>(&type) != nullptr ||
         dynamic_cast<const ast::SetType*>(&type) != nullptr ||
         dynamic_cast<const ast::ChoiceType*>(&type) != nullptr;
}

/**
 *  Function    : uniquify_type_name
 *  Description : Builds and returns a string for uniquify type name.
 *  Parameters  : ctx — LowerCtx& ctx; name — std::string name
 *  Returns     : std::string
 */
std::string uniquify_type_name(LowerCtx& ctx, std::string name) {
  if (name.empty()) {
    return name;
  }
  const std::string key0 = ctx.mod.name() + "::" + name;
  if (!ctx.named_ids.count(key0)) {
    return name;
  }
  for (;;) {
    std::string candidate = name + "_" + std::to_string(++ctx.synth_serial);
    const std::string key = ctx.mod.name() + "::" + candidate;
    if (!ctx.named_ids.count(key)) {
      return candidate;
    }
  }
}

std::string nested_type_name(LowerCtx& ctx, const std::string& parent,
                             const std::string& field, const char* fallback_kind) {
  std::string name;
  if (!parent.empty() && !field.empty()) {
    name = parent + "-" + field;
  } else if (!parent.empty()) {
    name = parent + "-" + fallback_kind;
  } else {
    name = synth_type_name(ctx, "", fallback_kind);
  }
  return uniquify_type_name(ctx, std::move(name));
}

std::string child_lower_name(LowerCtx& ctx, const std::string& parent,
                             const std::string& field, const ast::Type& child_type,
                             const char* fallback_kind) {
  if (!needs_synthetic_nested_name(child_type)) {
    return "";
  }
  return nested_type_name(ctx, parent, field, fallback_kind);
}

struct SubstEnv {
  std::unordered_map<std::string, const ast::ActualParameter*> by_name;
};

std::string actual_param_label(const ast::ActualParameter& a, const SubstEnv* subst,
                               std::size_t index) {
  if (a.type) {
    if (const auto* r = dynamic_cast<const ast::ReferencedType*>(a.type.get())) {
      return r->name();
    }
    return "T" + std::to_string(index);
  }
  if (a.object_set_name) {
    std::string os = *a.object_set_name;
    if (subst) {
      auto it = subst->by_name.find(os);
      if (it != subst->by_name.end() && it->second) {
        if (it->second->object_set_name) {
          return *it->second->object_set_name;
        }
        if (it->second->type) {
          if (const auto* r =
                  /**
                   *  Function    : get
                   *  Description : Computes get from (none).
                   *  Parameters  : none
                   *  Returns     : dynamic_cast<const ast::ReferencedType*>(it->second->type.
                   */
                  dynamic_cast<const ast::ReferencedType*>(it->second->type.get())) {
            return r->name();
          }
        }
      }
    }
    return os;
  }
  if (a.value) {
    return "V" + std::to_string(index);
  }
  return "X" + std::to_string(index);
}

std::string mangle_instantiation(const std::string& template_name,
                                 const std::vector<ast::ActualParameter>& actuals,
                                 const SubstEnv* subst) {
  std::string m = template_name;
  for (std::size_t i = 0; i < actuals.size(); ++i) {
    m += "-";
    m += actual_param_label(actuals[i], subst, i);
  }
  return m;
}

const ast::ActualParameter* resolve_actual(const ast::ActualParameter& actual,
                                           const SubstEnv* subst) {
  if (!subst) {
    return &actual;
  }
  if (actual.object_set_name) {
    auto it = subst->by_name.find(*actual.object_set_name);
    if (it != subst->by_name.end() && it->second) {
      return it->second;
    }
  }
  if (actual.type) {
    if (const auto* r = dynamic_cast<const ast::ReferencedType*>(actual.type.get())) {
      if (r->actuals().empty()) {
        auto it = subst->by_name.find(r->name());
        if (it != subst->by_name.end() && it->second) {
          return it->second;
        }
      }
    }
  }
  return &actual;
}

/**
 *  Function    : lower_simple_value
 *  Description : Computes lower simple value from (ctx, v).
 *  Parameters  : ctx — LowerCtx& ctx; v — const ast::Value& v
 *  Returns     : std::optional<ir::ValueId>
 */
std::optional<ir::ValueId> lower_simple_value(LowerCtx& ctx, const ast::Value& v) {
  ir::Value out;
  out.module = ctx.mod.name();
  if (const auto* iv = dynamic_cast<const ast::IntegerValue*>(&v)) {
    out.kind = ir::ValueKind::Integer;
    out.integer = ir::BigInt::from_decimal(iv->text(), iv->negative());
  } else if (const auto* bv = dynamic_cast<const ast::BooleanValue*>(&v)) {
    out.kind = ir::ValueKind::Boolean;
    out.boolean = bv->value();
  } else if (const auto* sv = dynamic_cast<const ast::StringValue*>(&v)) {
    out.kind = ir::ValueKind::String;
    out.text = sv->text();
  } else if (const auto* vr = dynamic_cast<const ast::ValueReference*>(&v)) {
    out.kind = ir::ValueKind::Reference;
    out.name = vr->name();
  } else {
    return std::nullopt;
  }
  return ctx.model.arena.add_value(std::move(out));
}

/**
 *  Function    : resolve_object_class_field
 *  Description : Performs resolve object class field (definition).
 *  Parameters  : t — ir::Type& t; model — const ir::Model& model
 *  Returns     : void
 */
void resolve_object_class_field(ir::Type& t, const ir::Model& model) {
  if (t.kind != ir::TypeKind::ObjectClassField) {
    return;
  }
  const ir::ObjectClassInfo* best_oc = nullptr;
  for (const auto& oc : model.object_classes) {
    if (oc.name != t.object_class_field.class_name) {
      continue;
    }
    // Prefer matching the defining module if possible
    if (oc.module == t.module) {
      best_oc = &oc;
      break;
    }
    if (!best_oc) {
      best_oc = &oc;
    }
  }
  if (!best_oc) {
    return;
  }
  for (const auto& f : best_oc->fields) {
    if (f.name != t.object_class_field.field_name) {
      continue;
    }
    if (f.kind == ir::ClassFieldKind::TypeField) {
      t.object_class_field.open_type = true;
      t.object_class_field.fixed_type = ir::kInvalidType;
    } else {
      t.object_class_field.open_type = false;
      t.object_class_field.fixed_type = f.fixed_type;
    }
    return;
  }
}

/**
 *  Function    : re_resolve_all_object_class_fields
 *  Description : Performs re resolve all object class fields (definition).
 *  Parameters  : model — ir::Model& model
 *  Returns     : void
 */
void re_resolve_all_object_class_fields(ir::Model& model) {
  for (std::size_t i = 0; i < model.arena.type_count(); ++i) {
    resolve_object_class_field(model.arena.get(static_cast<ir::TypeId>(i)), model);
  }
}

/// Walk a constraint tree for the first ContentsConstraint contained type.
/**
 *  Function    : find_containing_type
 *  Description : Computes find containing type from (c).
 *  Parameters  : c — const ast::Constraint* c
 *  Returns     : const ast::Type*
 */
const ast::Type* find_containing_type(const ast::Constraint* c) {
  if (!c) {
    return nullptr;
  }
  if (const auto* contents = dynamic_cast<const ast::ContentsConstraint*>(c)) {
    return contents->contained();
  }
  if (const auto* size = dynamic_cast<const ast::SizeConstraint*>(c)) {
    return find_containing_type(&size->inner());
  }
  if (const auto* ext = dynamic_cast<const ast::ExtensibleConstraint*>(c)) {
    return find_containing_type(ext->root());
  }
  if (const auto* uni = dynamic_cast<const ast::UnionConstraint*>(c)) {
    for (const auto& alt : uni->alternatives()) {
      if (const ast::Type* t = find_containing_type(alt.get())) {
        return t;
      }
    }
  }
  if (const auto* inter = dynamic_cast<const ast::IntersectionConstraint*>(c)) {
    for (const auto& part : inter->parts()) {
      if (const ast::Type* t = find_containing_type(part.get())) {
        return t;
      }
    }
  }
  return nullptr;
}

ir::TypeId lower_type(LowerCtx& ctx, const ast::Type& type, const std::string& assigned_name,
                      const SubstEnv* subst = nullptr);

/**
 *  Function    : map_jer_name_form
 *  Description : Computes map jer name form from (t).
 *  Parameters  : t — ast::NameTransform t
 *  Returns     : ir::Field::JerNameForm
 */
ir::Field::JerNameForm map_jer_name_form(ast::NameTransform t) {
  switch (t) {
    case ast::NameTransform::AsIs:
      return ir::Field::JerNameForm::AsIs;
    case ast::NameTransform::Capitalized:
      return ir::Field::JerNameForm::Capitalized;
    case ast::NameTransform::Uppercased:
      return ir::Field::JerNameForm::Uppercased;
    case ast::NameTransform::Lowercased:
      return ir::Field::JerNameForm::Lowercased;
    case ast::NameTransform::Literal:
      return ir::Field::JerNameForm::Literal;
  }
  return ir::Field::JerNameForm::AsIs;
}

/**
 *  Function    : map_jer_text_form
 *  Description : Computes map jer text form from (t).
 *  Parameters  : t — ast::NameTransform t
 *  Returns     : ir::JerEncoding::TextForm
 */
ir::JerEncoding::TextForm map_jer_text_form(ast::NameTransform t) {
  switch (t) {
    case ast::NameTransform::AsIs:
      return ir::JerEncoding::TextForm::AsIs;
    case ast::NameTransform::Capitalized:
      return ir::JerEncoding::TextForm::Capitalized;
    case ast::NameTransform::Uppercased:
      return ir::JerEncoding::TextForm::Uppercased;
    case ast::NameTransform::Lowercased:
      return ir::JerEncoding::TextForm::Lowercased;
    case ast::NameTransform::Literal:
      return ir::JerEncoding::TextForm::Literal;
  }
  return ir::JerEncoding::TextForm::AsIs;
}

void apply_jer_instructions(ir::Type& out,
                            const std::vector<ast::EncodingInstruction>& eis) {
  for (const auto& ei : eis) {
    switch (ei.kind) {
      case ast::EncodingInstructionKind::Array:
        out.jer.array = true;
        break;
      case ast::EncodingInstructionKind::Base64:
        out.jer.base64 = true;
        break;
      case ast::EncodingInstructionKind::Object:
        out.jer.object = true;
        break;
      case ast::EncodingInstructionKind::Unwrapped:
        out.jer.unwrapped = true;
        break;
      case ast::EncodingInstructionKind::Name:
        break;
      case ast::EncodingInstructionKind::Text:
        out.jer.text_form = map_jer_text_form(ei.transform);
        out.jer.text_literal = ei.literal;
        break;
      case ast::EncodingInstructionKind::Attribute:
      case ast::EncodingInstructionKind::UseNumber:
      case ast::EncodingInstructionKind::List:
      case ast::EncodingInstructionKind::Untagged:
      case ast::EncodingInstructionKind::UseNil:
        break;
    }
  }
}

void apply_exer_instructions(ir::Type& out,
                             const std::vector<ast::EncodingInstruction>& eis) {
  for (const auto& ei : eis) {
    switch (ei.kind) {
      case ast::EncodingInstructionKind::Base64:
        out.exer.base64 = true;
        break;
      case ast::EncodingInstructionKind::Text:
        out.exer.text = true;
        break;
      case ast::EncodingInstructionKind::UseNumber:
        out.exer.use_number = true;
        break;
      case ast::EncodingInstructionKind::List:
        out.exer.list = true;
        break;
      case ast::EncodingInstructionKind::Attribute:
      case ast::EncodingInstructionKind::Untagged:
      case ast::EncodingInstructionKind::UseNil:
      case ast::EncodingInstructionKind::Name:
      case ast::EncodingInstructionKind::Array:
      case ast::EncodingInstructionKind::Object:
      case ast::EncodingInstructionKind::Unwrapped:
        break;
    }
  }
}

/**
 *  Function    : apply_field_jer_name_from_ast
 *  Description : Performs apply field jer name from ast (definition).
 *  Parameters  : field — ir::Field& field; ast_type — const ast::Type& ast_type
 *  Returns     : void
 */
void apply_field_jer_name_from_ast(ir::Field& field, const ast::Type& ast_type) {
  for (const auto& ei : ast_type.encoding_instructions()) {
    if (ei.kind == ast::EncodingInstructionKind::Name) {
      field.jer_name_form = map_jer_name_form(ei.transform);
      field.jer_name_literal = ei.literal;
    }
  }
}

/**
 *  Function    : apply_field_exer_from_ast
 *  Description : Performs apply field exer from ast (definition).
 *  Parameters  : field — ir::Field& field; ast_type — const ast::Type& ast_type
 *  Returns     : void
 */
void apply_field_exer_from_ast(ir::Field& field, const ast::Type& ast_type) {
  for (const auto& ei : ast_type.encoding_instructions()) {
    switch (ei.kind) {
      case ast::EncodingInstructionKind::Attribute:
        field.exer.attribute = true;
        break;
      case ast::EncodingInstructionKind::Untagged:
        field.exer.untagged = true;
        break;
      case ast::EncodingInstructionKind::UseNil:
        field.exer.use_nil = true;
        break;
      case ast::EncodingInstructionKind::Name:
        field.jer_name_form = map_jer_name_form(ei.transform);
        field.jer_name_literal = ei.literal;
        break;
      default:
        break;
    }
  }
}

/**
 *  Function    : map_presence
 *  Description : Computes map presence from (p).
 *  Parameters  : p — ast::Presence p
 *  Returns     : ir::Presence
 */
ir::Presence map_presence(ast::Presence p) {
  switch (p) {
    case ast::Presence::Mandatory:
      return ir::Presence::Mandatory;
    case ast::Presence::Optional:
      return ir::Presence::Optional;
    case ast::Presence::Default:
      return ir::Presence::Default;
  }
  return ir::Presence::Mandatory;
}

/**
 *  Function    : apply_outer_tag
 *  Description : Performs apply outer tag (definition).
 *  Parameters  : out — ir::Type& out; type — const ast::Type& type; tag_default — ast::TagDefault tag_default
 *  Returns     : void
 */
void apply_outer_tag(ir::Type& out, const ast::Type& type, ast::TagDefault tag_default) {
  out.tag = builtin_tag_for(type);
  if (type.tag()) {
    const bool is_choice = dynamic_cast<const ast::ChoiceType*>(&type) != nullptr;
    out.tag = make_ast_tag(*type.tag(), tag_default, is_choice);
  }
}

ir::TypeId make_builtin_scalar(LowerCtx& ctx, ir::TypeKind kind, ir::Tag tag,
                               ir::StringKind string_kind = ir::StringKind::UTF8String) {
  ir::Type t;
  t.kind = kind;
  t.tag = tag;
  if (kind == ir::TypeKind::String) {
    t.string.kind = string_kind;
  }
  return ctx.model.arena.add(std::move(t));
}

/**
 *  Function    : make_field
 *  Description : Computes make field from (name, type, presence, tag).
 *  Parameters  : name — std::string name; type — ir::TypeId type; presence — ir::Presence presence; tag — ir::Tag tag
 *  Returns     : ir::Field
 */
ir::Field make_field(std::string name, ir::TypeId type, ir::Presence presence, ir::Tag tag) {
  ir::Field f;
  f.name = std::move(name);
  f.type = type;
  f.presence = presence;
  f.tag = tag;
  return f;
}

/**
 *  Function    : lower_identification_choice
 *  Description : Computes lower identification choice from (ctx, base).
 *  Parameters  : ctx — LowerCtx& ctx; base — const std::string& base
 *  Returns     : ir::TypeId
 */
ir::TypeId lower_identification_choice(LowerCtx& ctx, const std::string& base) {
  const ir::TypeId oid_ty = make_builtin_scalar(ctx, ir::TypeKind::ObjectIdentifier, universal(6));
  const ir::TypeId int_ty = make_builtin_scalar(ctx, ir::TypeKind::Integer, universal(2));
  const ir::TypeId null_ty = make_builtin_scalar(ctx, ir::TypeKind::Null, universal(5));

  ir::Type syntaxes;
  syntaxes.kind = ir::TypeKind::Sequence;
  syntaxes.module = ctx.mod.name();
  syntaxes.name = base + "-syntaxes";
  syntaxes.tag = universal(16);
  syntaxes.sequence.root.push_back(make_field(
      "abstract", oid_ty, ir::Presence::Mandatory, ir::Tag{ir::TagClass::Context, 0, false}));
  syntaxes.sequence.root.push_back(make_field(
      "transfer", oid_ty, ir::Presence::Mandatory, ir::Tag{ir::TagClass::Context, 1, false}));
  const ir::TypeId syntaxes_id = ctx.model.arena.add(std::move(syntaxes));

  ir::Type ctx_neg;
  ctx_neg.kind = ir::TypeKind::Sequence;
  ctx_neg.module = ctx.mod.name();
  ctx_neg.name = base + "-context-negotiation";
  ctx_neg.tag = universal(16);
  ctx_neg.sequence.root.push_back(
      make_field("presentation-context-id", int_ty, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 0, false}));
  ctx_neg.sequence.root.push_back(
      make_field("transfer-syntax", oid_ty, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 1, false}));
  const ir::TypeId ctx_neg_id = ctx.model.arena.add(std::move(ctx_neg));

  ir::Type ident;
  ident.kind = ir::TypeKind::Choice;
  ident.module = ctx.mod.name();
  ident.name = base + "-identification";
  ident.choice.alternatives.push_back(make_field(
      "syntaxes", syntaxes_id, ir::Presence::Mandatory, ir::Tag{ir::TagClass::Context, 0, true}));
  ident.choice.alternatives.push_back(make_field(
      "syntax", oid_ty, ir::Presence::Mandatory, ir::Tag{ir::TagClass::Context, 1, false}));
  ident.choice.alternatives.push_back(
      make_field("presentation-context-id", int_ty, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 2, false}));
  ident.choice.alternatives.push_back(
      make_field("context-negotiation", ctx_neg_id, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 3, true}));
  ident.choice.alternatives.push_back(
      make_field("transfer-syntax", oid_ty, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 4, false}));
  ident.choice.alternatives.push_back(make_field(
      "fixed", null_ty, ir::Presence::Mandatory, ir::Tag{ir::TagClass::Context, 5, false}));
  return ctx.model.arena.add(std::move(ident));
}

ir::TypeId lower_embedded_or_charstring(LowerCtx& ctx, const std::string& assigned_name,
                                        const ast::Type& ast_type, std::uint64_t univ_tag,
                                        const char* kind, const char* value_field_name) {
  const std::string base = synth_type_name(ctx, assigned_name, kind);
  const ir::TypeId ident_id = lower_identification_choice(ctx, base);
  const ir::TypeId desc_ty =
      make_builtin_scalar(ctx, ir::TypeKind::String, universal(7), ir::StringKind::GraphicString);
  const ir::TypeId oct_ty = make_builtin_scalar(ctx, ir::TypeKind::OctetString, universal(4));

  // X.680 associated SEQUENCE with AUTOMATIC-style tags:
  // identification [0] EXPLICIT, data-value-descriptor [1] OPTIONAL, value [2].
  ir::Type seq;
  seq.kind = ir::TypeKind::Sequence;
  seq.module = ctx.mod.name();
  seq.name = base;
  seq.tag = universal(univ_tag);
  seq.sequence.root.push_back(
      make_field("identification", ident_id, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 0, true}));
  seq.sequence.root.push_back(
      make_field("data-value-descriptor", desc_ty, ir::Presence::Optional,
                 ir::Tag{ir::TagClass::Context, 1, false}));
  seq.sequence.root.push_back(
      make_field(value_field_name, oct_ty, ir::Presence::Mandatory,
                 ir::Tag{ir::TagClass::Context, 2, false}));
  apply_jer_instructions(seq, ast_type.encoding_instructions());
  apply_exer_instructions(seq, ast_type.encoding_instructions());
  apply_outer_tag(seq, ast_type, ctx.mod.tag_default());
  if (!ast_type.tag()) {
    seq.tag = universal(univ_tag);
  }
  return ctx.model.arena.add(std::move(seq));
}

ir::TypeId lower_type(LowerCtx& ctx, const ast::Type& type, const std::string& assigned_name,
                      const SubstEnv* subst) {
  if (const auto* ref = dynamic_cast<const ast::ReferencedType*>(&type)) {
    if (subst) {
      auto sit = subst->by_name.find(ref->name());
      if (sit != subst->by_name.end() && sit->second && sit->second->type) {
        return lower_type(ctx, *sit->second->type, assigned_name, subst);
      }
    }

    std::string owner = ref->module() ? *ref->module() : ctx.mod.name();
    std::string name = ref->name();
    Symbol* sym = ctx.symbols.lookup(owner, name);
    if (sym && sym->is_import) {
      owner = sym->import_from_module;
      sym = ctx.symbols.find(owner, name);
    }

    // Parameterized instantiation (X.683). Lower in the template's defining
    // module so nested refs (e.g. ProtocolIE-Field inside ProtocolIE-Container)
    // resolve; rebind object-set formals through the outer SubstEnv.
    if (!ref->actuals().empty() && sym && sym->kind == SymbolKind::Type && !sym->is_import) {
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(sym->ast);
      if (ta && !ta->parameters().empty()) {
        const auto& formals = ta->parameters();
        if (ref->actuals().size() != formals.size()) {
          ctx.diagnostics.error(ref->range(),
                                "parameterized type '" + name + "' expects " +
                                    std::to_string(formals.size()) + " actual parameter(s), got " +
                                    std::to_string(ref->actuals().size()));
          return ctx.model.arena.add(ir::Type{});
        }
        SubstEnv local;
        for (std::size_t i = 0; i < formals.size(); ++i) {
          local.by_name[formals[i].name] = resolve_actual(ref->actuals()[i], subst);
        }
        if (subst) {
          for (const auto& kv : subst->by_name) {
            if (!local.by_name.count(kv.first)) {
              local.by_name[kv.first] = kv.second;
            }
          }
        }
        const std::string mangled = mangle_instantiation(name, ref->actuals(), subst);
        const std::string inst_key = owner + "::" + mangled;
        if (auto it = ctx.named_ids.find(inst_key); it != ctx.named_ids.end()) {
          return it->second;
        }
        const std::string body_name = assigned_name.empty() ? mangled : assigned_name;

        ir::TypeId id = ir::kInvalidType;
        if (owner != ctx.mod.name()) {
          const Symbol* mod_sym = ctx.symbols.find(owner, owner);
          const ast::Module* other =
              mod_sym ? dynamic_cast<const ast::Module*>(mod_sym->ast) : nullptr;
          if (!other) {
            ctx.diagnostics.error(ref->range(),
                                  "cannot instantiate parameterized type '" + name +
                                      "' from module '" + owner + "'");
            return ctx.model.arena.add(ir::Type{});
          }
          LowerCtx other_ctx{ctx.model, ctx.symbols, ctx.diagnostics, *other,
                             ctx.resolving, ctx.named_ids, ctx.synth_serial};
          id = lower_type(other_ctx, ta->type(), body_name, &local);
          ctx.resolving = std::move(other_ctx.resolving);
          ctx.named_ids = std::move(other_ctx.named_ids);
          ctx.synth_serial = other_ctx.synth_serial;
        } else {
          id = lower_type(ctx, ta->type(), body_name, &local);
        }
        if (id != ir::kInvalidType) {
          ir::Type& built = ctx.model.arena.get(id);
          if (built.name.empty()) {
            built.name = body_name;
          }
          built.module = owner;
          ctx.named_ids[inst_key] = id;
        }
        return id;
      }
    }

    const std::string key = owner + "::" + name;

    if (auto it = ctx.named_ids.find(key); it != ctx.named_ids.end()) {
      return it->second;
    }

    if (sym && sym->kind == SymbolKind::Type && sym->type_id != ir::kInvalidType) {
      return sym->type_id;
    }

    if (sym && sym->kind == SymbolKind::Type && !sym->is_import) {
      if (ctx.resolving.count(key)) {
        ir::Type placeholder;
        placeholder.kind = ir::TypeKind::Referenced;
        placeholder.module = owner;
        placeholder.name = name;
        placeholder.recursive = true;
        placeholder.referenced.module = owner;
        placeholder.referenced.name = name;
        const ir::TypeId id = ctx.model.arena.add(std::move(placeholder));
        ctx.named_ids[key] = id;
        return id;
      }
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(sym->ast);
      if (ta) {
        if (!ta->parameters().empty()) {
          ir::Type t;
          t.kind = ir::TypeKind::Referenced;
          t.module = owner;
          t.name = assigned_name.empty() ? name : assigned_name;
          t.referenced.module = owner;
          t.referenced.name = name;
          return ctx.model.arena.add(std::move(t));
        }
        ctx.resolving.insert(key);
        ir::TypeId id;
        if (owner == ctx.mod.name()) {
          id = lower_type(ctx, ta->type(), ta->name(), subst);
        } else {
          const Symbol* mod_sym = ctx.symbols.find(owner, owner);
          const ast::Module* other =
              mod_sym ? dynamic_cast<const ast::Module*>(mod_sym->ast) : nullptr;
          if (!other) {
            ctx.diagnostics.error(ref->range(),
                                  "cannot lower imported type '" + name + "'");
            ctx.resolving.erase(key);
            return ctx.model.arena.add(ir::Type{});
          }
          LowerCtx other_ctx{ctx.model, ctx.symbols, ctx.diagnostics, *other,
                             ctx.resolving, ctx.named_ids};
          id = lower_type(other_ctx, ta->type(), ta->name(), subst);
          ctx.resolving = std::move(other_ctx.resolving);
          ctx.named_ids = std::move(other_ctx.named_ids);
        }
        ctx.resolving.erase(key);
        ctx.model.arena.get(id).module = owner;
        ctx.model.arena.get(id).name = ta->name();
        sym->type_id = id;
        ctx.named_ids[key] = id;
        for (std::size_t i = 0; i < ctx.model.arena.type_count(); ++i) {
          ir::Type& t = ctx.model.arena.get(static_cast<ir::TypeId>(i));
          if (t.kind == ir::TypeKind::Referenced && t.referenced.module == owner &&
              t.referenced.name == name) {
            t.referenced.resolved = id;
          }
        }
        return id;
      }
    }

    ir::Type t;
    t.kind = ir::TypeKind::Referenced;
    t.module = owner;
    t.name = assigned_name;
    t.referenced.module = owner;
    t.referenced.name = name;
    return ctx.model.arena.add(std::move(t));
  }

  // Expand associated types to SEQUENCE/CHOICE IR (X.680).
  // EXTERNAL uses the modern associated form (identification CHOICE), matching
  // EMBEDDED PDV / CHARACTER STRING. Classic EXTERNAL remains available in the
  // BER runtime via encode_external / decode_external.
  if (dynamic_cast<const ast::ExternalType*>(&type)) {
    return lower_embedded_or_charstring(ctx, assigned_name, type, 8, "External", "data-value");
  }
  if (dynamic_cast<const ast::EmbeddedPdvType*>(&type)) {
    return lower_embedded_or_charstring(ctx, assigned_name, type, 11, "EmbeddedPdv",
                                        "data-value");
  }
  if (dynamic_cast<const ast::CharacterStringType*>(&type)) {
    return lower_embedded_or_charstring(ctx, assigned_name, type, 29, "CharacterString",
                                        "string-value");
  }

  ir::Type out;
  out.module = ctx.mod.name();
  out.name = assigned_name;
  apply_outer_tag(out, type, ctx.mod.tag_default());

  auto val_resolver = [&](const std::string& name) -> std::optional<ir::BigInt> {
    const Symbol* sym = ctx.symbols.lookup(ctx.mod.name(), name);
    if (!sym) sym = ctx.symbols.find(ctx.mod.name(), name);
    if (!sym) {
      // Search all modules
      for (const auto& pair : ctx.symbols.entries()) {
        if (pair.second.kind == SymbolKind::Value && pair.second.name == name) {
          sym = &pair.second;
          break;
        }
      }
    }
    if (!sym) return std::nullopt;
    if (sym->kind == SymbolKind::Value && sym->ast) {
      if (const auto* va = dynamic_cast<const ast::ValueAssignment*>(sym->ast)) {
        if (const auto* iv = dynamic_cast<const ast::IntegerValue*>(&va->value())) {
          return ir::BigInt::from_decimal(iv->text(), iv->negative());
        }
      }
    }
    return std::nullopt;
  };

  if (dynamic_cast<const ast::BooleanType*>(&type)) {
    out.kind = ir::TypeKind::Boolean;
  } else if (const auto* integer = dynamic_cast<const ast::IntegerType*>(&type)) {
    out.kind = ir::TypeKind::Integer;
    out.integer.constraint =
        constraints::normalize(integer->constraint(), ctx.diagnostics, val_resolver);
    for (const auto& nn : integer->named_numbers()) {
      out.integer.named_numbers.push_back(
          ir::NamedNumber{nn.name, ir::BigInt::from_decimal(nn.number_text, nn.negative)});
    }
    constraints::suggest_host_integer(out.integer);
  } else if (const auto* bits = dynamic_cast<const ast::BitStringType*>(&type)) {
    out.kind = ir::TypeKind::BitString;
    out.bit_string.size =
        constraints::normalize_size(bits->constraint(), ctx.diagnostics, val_resolver);
    for (const auto& nn : bits->named_bits()) {
      out.bit_string.named_bits.push_back(
          ir::NamedNumber{nn.name, ir::BigInt::from_decimal(nn.number_text, false)});
    }
    if (const ast::Type* contained = find_containing_type(bits->constraint())) {
      out.bit_string.containing = lower_type(ctx, *contained, "", subst);
    }
  } else if (const auto* octets = dynamic_cast<const ast::OctetStringType*>(&type)) {
    out.kind = ir::TypeKind::OctetString;
    out.octet_string.size =
        constraints::normalize_size(octets->constraint(), ctx.diagnostics, val_resolver);
    if (const ast::Type* contained = find_containing_type(octets->constraint())) {
      out.octet_string.containing = lower_type(ctx, *contained, "", subst);
    }
  } else if (dynamic_cast<const ast::NullType*>(&type)) {
    out.kind = ir::TypeKind::Null;
  } else if (const auto* str = dynamic_cast<const ast::StringType*>(&type)) {
    out.kind = ir::TypeKind::String;
    out.string.kind = map_string_kind(str->kind());
    out.string.size = constraints::normalize_size(str->constraint(), ctx.diagnostics, val_resolver);
  } else if (const auto* seq = dynamic_cast<const ast::SequenceType*>(&type)) {
    out.kind = ir::TypeKind::Sequence;
    std::uint64_t auto_index = 0;
    bool in_extensions = false;
    bool in_trailing_root = false;
    auto make_field = [&](const ast::Component& comp) {
      ir::Field field;
      field.name = comp.name();
      field.presence = map_presence(comp.presence());
      field.tag = component_tag(comp.type(), ctx.mod.tag_default(), auto_index);
      ++auto_index;
      field.type = lower_type(
          ctx, comp.type(),
          child_lower_name(ctx, assigned_name, comp.name(), comp.type(), "Seq"), subst);
      if (comp.default_value()) {
        field.default_value = lower_simple_value(ctx, *comp.default_value());
      }
      apply_field_jer_name_from_ast(field, comp.type());
      apply_field_exer_from_ast(field, comp.type());
      if (const auto* crc = extract_component_relation_constraint(comp.type().constraint())) {
        std::string set_name = crc->object_set_name();
        if (subst) {
          auto sit = subst->by_name.find(set_name);
          if (sit != subst->by_name.end() && sit->second) {
            if (sit->second->object_set_name) {
              set_name = *sit->second->object_set_name;
            } else if (sit->second->type) {
              if (const auto* r = dynamic_cast<const ast::ReferencedType*>(sit->second->type.get())) {
                set_name = r->name();
              }
            }
          }
        }
        if (!crc->at_components().empty()) {
          ir::TableConstraintInfo tc;
          tc.object_set_name = set_name;
          tc.governor_field_name = crc->at_components()[0];
          field.table_constraint = std::move(tc);
        }
      }
      return field;
    };
    std::function<void(const ast::ComponentItem&)> process =
        [&](const ast::ComponentItem& item) {
          if (dynamic_cast<const ast::ExtensionMarker*>(&item)) {
            if (!out.sequence.extensible) {
              out.sequence.extensible = true;
              in_extensions = true;
              in_trailing_root = false;
            } else if (in_extensions) {
              // Second ellipsis: trailing root component list.
              in_extensions = false;
              in_trailing_root = true;
            }
            return;
          }
          if (const auto* grp = dynamic_cast<const ast::VersionAdditionGroup*>(&item)) {
            if (in_extensions) {
              out.sequence.extension_groups.emplace_back();
              auto& dest = out.sequence.extension_groups.back();
              for (const auto& child : grp->items()) {
                const auto* comp = dynamic_cast<const ast::Component*>(child.get());
                if (comp) {
                  dest.push_back(make_field(*comp));
                }
              }
            }
            return;
          }
          const auto* comp = dynamic_cast<const ast::Component*>(&item);
          if (!comp) {
            return;
          }
          ir::Field field = make_field(*comp);
          if (in_extensions) {
            // Each ComponentType after "..." is its own extension addition.
            out.sequence.extension_groups.push_back({std::move(field)});
          } else if (in_trailing_root) {
            out.sequence.trailing_root.push_back(std::move(field));
          } else {
            out.sequence.root.push_back(std::move(field));
          }
        };
    for (const auto& item : seq->items()) {
      process(*item);
    }
  } else if (const auto* ch = dynamic_cast<const ast::ChoiceType*>(&type)) {
    out.kind = ir::TypeKind::Choice;
    std::uint64_t auto_index = 0;
    bool in_extensions = false;
    std::function<void(const ast::ComponentItem&)> process =
        [&](const ast::ComponentItem& item) {
          if (dynamic_cast<const ast::ExtensionMarker*>(&item)) {
            out.choice.extensible = true;
            in_extensions = true;
            return;
          }
          if (const auto* grp = dynamic_cast<const ast::VersionAdditionGroup*>(&item)) {
            for (const auto& child : grp->items()) {
              process(*child);
            }
            return;
          }
          const auto* comp = dynamic_cast<const ast::Component*>(&item);
          if (!comp) {
            return;
          }
          ir::Field field;
          field.name = comp->name();
          field.presence = ir::Presence::Mandatory;
          field.tag = component_tag(comp->type(), ctx.mod.tag_default(), auto_index);
          ++auto_index;
          field.type = lower_type(
              ctx, comp->type(),
              child_lower_name(ctx, assigned_name, comp->name(), comp->type(), "Choice"),
              subst);
          apply_field_jer_name_from_ast(field, comp->type());
          apply_field_exer_from_ast(field, comp->type());
          if (in_extensions) {
            out.choice.extensions.push_back(std::move(field));
          } else {
            out.choice.alternatives.push_back(std::move(field));
          }
        };
    for (const auto& item : ch->alternatives()) {
      process(*item);
    }
  } else if (const auto* of = dynamic_cast<const ast::SequenceOfType*>(&type)) {
    out.kind = ir::TypeKind::SequenceOf;
    out.sequence_of.element = lower_type(
        ctx, of->element(),
        child_lower_name(ctx, assigned_name, "Item", of->element(), "Item"), subst);
    out.sequence_of.size =
        constraints::normalize_size(of->constraint(), ctx.diagnostics, val_resolver);
  } else if (const auto* set = dynamic_cast<const ast::SetType*>(&type)) {
    out.kind = ir::TypeKind::Set;
    std::uint64_t auto_index = 0;
    bool in_extensions = false;
    bool in_trailing_root = false;
    auto make_field = [&](const ast::Component& comp) {
      ir::Field field;
      field.name = comp.name();
      field.presence = map_presence(comp.presence());
      field.tag = component_tag(comp.type(), ctx.mod.tag_default(), auto_index);
      ++auto_index;
      field.type = lower_type(
          ctx, comp.type(),
          child_lower_name(ctx, assigned_name, comp.name(), comp.type(), "Set"), subst);
      if (comp.default_value()) {
        field.default_value = lower_simple_value(ctx, *comp.default_value());
      }
      apply_field_jer_name_from_ast(field, comp.type());
      apply_field_exer_from_ast(field, comp.type());
      if (const auto* crc = extract_component_relation_constraint(comp.type().constraint())) {
        std::string set_name = crc->object_set_name();
        if (subst) {
          auto sit = subst->by_name.find(set_name);
          if (sit != subst->by_name.end() && sit->second) {
            if (sit->second->object_set_name) {
              set_name = *sit->second->object_set_name;
            } else if (sit->second->type) {
              if (const auto* r = dynamic_cast<const ast::ReferencedType*>(sit->second->type.get())) {
                set_name = r->name();
              }
            }
          }
        }
        if (!crc->at_components().empty()) {
          ir::TableConstraintInfo tc;
          tc.object_set_name = set_name;
          tc.governor_field_name = crc->at_components()[0];
          field.table_constraint = std::move(tc);
        }
      }
      return field;
    };
    std::function<void(const ast::ComponentItem&)> process =
        [&](const ast::ComponentItem& item) {
          if (dynamic_cast<const ast::ExtensionMarker*>(&item)) {
            if (!out.set.extensible) {
              out.set.extensible = true;
              in_extensions = true;
              in_trailing_root = false;
            } else if (in_extensions) {
              in_extensions = false;
              in_trailing_root = true;
            }
            return;
          }
          if (const auto* grp = dynamic_cast<const ast::VersionAdditionGroup*>(&item)) {
            if (in_extensions) {
              out.set.extension_groups.emplace_back();
              auto& dest = out.set.extension_groups.back();
              for (const auto& child : grp->items()) {
                const auto* comp = dynamic_cast<const ast::Component*>(child.get());
                if (comp) {
                  dest.push_back(make_field(*comp));
                }
              }
            }
            return;
          }
          const auto* comp = dynamic_cast<const ast::Component*>(&item);
          if (!comp) {
            return;
          }
          ir::Field field = make_field(*comp);
          if (in_extensions) {
            out.set.extension_groups.push_back({std::move(field)});
          } else if (in_trailing_root) {
            out.set.trailing_root.push_back(std::move(field));
          } else {
            out.set.root.push_back(std::move(field));
          }
        };
    for (const auto& item : set->items()) {
      process(*item);
    }
  } else if (const auto* setof = dynamic_cast<const ast::SetOfType*>(&type)) {
    out.kind = ir::TypeKind::SetOf;
    out.set_of.element = lower_type(
        ctx, setof->element(),
        child_lower_name(ctx, assigned_name, "Item", setof->element(), "Item"), subst);
    out.set_of.size =
        constraints::normalize_size(setof->constraint(), ctx.diagnostics, val_resolver);
  } else if (const auto* en = dynamic_cast<const ast::EnumeratedType*>(&type)) {
    out.kind = ir::TypeKind::Enumerated;
    auto assign_enum = [](const std::vector<ast::NamedNumber>& in,
                          std::vector<ir::NamedNumber>& dest) {
      std::int64_t next = 0;
      bool have_next = true;
      for (const auto& nn : in) {
        ir::NamedNumber out_nn;
        out_nn.name = nn.name;
        if (!nn.number_text.empty()) {
          out_nn.value = ir::BigInt::from_decimal(nn.number_text, nn.negative);
          if (out_nn.value.as_i64) {
            next = *out_nn.value.as_i64 + 1;
            have_next = true;
          } else {
            have_next = false;
          }
        } else if (have_next) {
          out_nn.value = ir::BigInt::from_i64(next);
          ++next;
        } else {
          out_nn.value = ir::BigInt::from_i64(0);
        }
        dest.push_back(std::move(out_nn));
      }
    };
    assign_enum(en->root(), out.enumerated.root);
    out.enumerated.extensible = en->extensible();
    assign_enum(en->extensions(), out.enumerated.extensions);
  } else if (dynamic_cast<const ast::ObjectIdentifierType*>(&type)) {
    out.kind = ir::TypeKind::ObjectIdentifier;
    out.object_identifier.size =
        constraints::normalize_size(type.constraint(), ctx.diagnostics, val_resolver);
  } else if (dynamic_cast<const ast::RelativeOidType*>(&type)) {
    out.kind = ir::TypeKind::RelativeOid;
    out.relative_oid.size =
        constraints::normalize_size(type.constraint(), ctx.diagnostics, val_resolver);
  } else if (dynamic_cast<const ast::RealType*>(&type)) {
    out.kind = ir::TypeKind::Real;
    out.real.ieee_form = real_ieee_form_from_constraint(type.constraint());
  } else if (const auto* ocf = dynamic_cast<const ast::ObjectClassFieldType*>(&type)) {
    out.kind = ir::TypeKind::ObjectClassField;
    out.object_class_field.class_name = ocf->class_name();
    out.object_class_field.field_name = ocf->field_name();
    out.object_class_field.open_type = true;
    resolve_object_class_field(out, ctx.model);
  } else if (const auto* inst = dynamic_cast<const ast::InstanceOfType*>(&type)) {
    out.kind = ir::TypeKind::InstanceOf;
    out.instance_of.class_name = inst->class_name();
    const Symbol* cls = ctx.symbols.lookup(ctx.mod.name(), inst->class_name());
    if (!cls || cls->kind != SymbolKind::ObjectClass) {
      if (inst->class_name() != "TYPE-IDENTIFIER" &&
          /**
           *  Function    : class_name
           *  Description : Computes class name from (none).
           *  Parameters  : none
           *  Returns     : inst->
           */
          inst->class_name() != "ABSTRACT-SYNTAX") {
        ctx.diagnostics.error(inst->range(),
                              "undefined object class '" + inst->class_name() + "'");
      }
    }
  } else {
    ctx.diagnostics.error(type.range(), "internal error: unsupported AST type in lower");
    out.kind = ir::TypeKind::Null;
  }

  apply_jer_instructions(out, type.encoding_instructions());
  apply_exer_instructions(out, type.encoding_instructions());
  const ir::TypeId id = ctx.model.arena.add(std::move(out));
  if (!assigned_name.empty()) {
    const ir::Type& built = ctx.model.arena.get(id);
    if (built.kind == ir::TypeKind::Sequence || built.kind == ir::TypeKind::Set ||
        built.kind == ir::TypeKind::Choice) {
      ctx.named_ids[ctx.mod.name() + "::" + assigned_name] = id;
    }
  }
  return id;
}

void check_type_refs(const ast::Type& type, const std::string& module, SymbolTable& symbols,
                     Diagnostics& diagnostics,
                     const std::unordered_set<std::string>* formals = nullptr) {
  if (const auto* ref = dynamic_cast<const ast::ReferencedType*>(&type)) {
    if (formals && formals->count(ref->name())) {
      // Formal type parameter — ok.
    } else {
      const std::string owner = ref->module() ? *ref->module() : module;
      const Symbol* sym = symbols.lookup(owner, ref->name());
      if (!sym || sym->kind != SymbolKind::Type) {
        diagnostics.error(ref->range(), "undefined ASN.1 type '" + ref->name() + "'");
      }
    }
    for (const auto& a : ref->actuals()) {
      if (a.type) {
        check_type_refs(*a.type, module, symbols, diagnostics, formals);
      }
    }
  } else if (const auto* seq = dynamic_cast<const ast::SequenceType*>(&type)) {
    for (const auto& item : seq->items()) {
      if (const auto* comp = dynamic_cast<const ast::Component*>(item.get())) {
        check_type_refs(comp->type(), module, symbols, diagnostics, formals);
      } else if (const auto* grp =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::VersionAdditionGroup*>(item.
                      */
                     dynamic_cast<const ast::VersionAdditionGroup*>(item.get())) {
        for (const auto& child : grp->items()) {
          if (const auto* comp = dynamic_cast<const ast::Component*>(child.get())) {
            check_type_refs(comp->type(), module, symbols, diagnostics, formals);
          }
        }
      }
    }
  } else if (const auto* ch = dynamic_cast<const ast::ChoiceType*>(&type)) {
    for (const auto& item : ch->alternatives()) {
      if (const auto* comp = dynamic_cast<const ast::Component*>(item.get())) {
        check_type_refs(comp->type(), module, symbols, diagnostics, formals);
      } else if (const auto* grp =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::VersionAdditionGroup*>(item.
                      */
                     dynamic_cast<const ast::VersionAdditionGroup*>(item.get())) {
        for (const auto& child : grp->items()) {
          if (const auto* comp = dynamic_cast<const ast::Component*>(child.get())) {
            check_type_refs(comp->type(), module, symbols, diagnostics, formals);
          }
        }
      }
    }
  } else if (const auto* of = dynamic_cast<const ast::SequenceOfType*>(&type)) {
    check_type_refs(of->element(), module, symbols, diagnostics, formals);
  } else if (const auto* set = dynamic_cast<const ast::SetType*>(&type)) {
    for (const auto& item : set->items()) {
      if (const auto* comp = dynamic_cast<const ast::Component*>(item.get())) {
        check_type_refs(comp->type(), module, symbols, diagnostics, formals);
      } else if (const auto* grp =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::VersionAdditionGroup*>(item.
                      */
                     dynamic_cast<const ast::VersionAdditionGroup*>(item.get())) {
        for (const auto& child : grp->items()) {
          if (const auto* comp = dynamic_cast<const ast::Component*>(child.get())) {
            check_type_refs(comp->type(), module, symbols, diagnostics, formals);
          }
        }
      }
    }
  } else if (const auto* setof = dynamic_cast<const ast::SetOfType*>(&type)) {
    check_type_refs(setof->element(), module, symbols, diagnostics, formals);
  } else if (const auto* ocf = dynamic_cast<const ast::ObjectClassFieldType*>(&type)) {
    const Symbol* cls = symbols.lookup(module, ocf->class_name());
    if (!cls || cls->kind != SymbolKind::ObjectClass) {
      // Useful object classes are injected later; allow unresolved for now.
      if (ocf->class_name() != "TYPE-IDENTIFIER" &&
          /**
           *  Function    : class_name
           *  Description : Computes class name from (none).
           *  Parameters  : none
           *  Returns     : ocf->
           */
          ocf->class_name() != "ABSTRACT-SYNTAX") {
        // Governor object-class formals (e.g. PROTOCOL-IES : IEsSetParam) — class name
        // may be a formal governor type; skip if it looks like a known class formal.
        if (!(formals && formals->count(ocf->class_name()))) {
          diagnostics.error(ocf->range(),
                            "undefined object class '" + ocf->class_name() + "'");
        }
      }
    }
  } else if (const auto* inst = dynamic_cast<const ast::InstanceOfType*>(&type)) {
    const Symbol* cls = symbols.lookup(module, inst->class_name());
    if (!cls || cls->kind != SymbolKind::ObjectClass) {
      if (inst->class_name() != "TYPE-IDENTIFIER" &&
          /**
           *  Function    : class_name
           *  Description : Computes class name from (none).
           *  Parameters  : none
           *  Returns     : inst->
           */
          inst->class_name() != "ABSTRACT-SYNTAX") {
        if (!(formals && formals->count(inst->class_name()))) {
          diagnostics.error(inst->range(),
                            "undefined object class '" + inst->class_name() + "'");
        }
      }
    }
  }
}

}  // namespace

Analyzer::Analyzer(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}

/**
 *  Function    : analyze
 *  Description : Computes analyze from (modules).
 *  Parameters  : modules — std::vector<std::unique_ptr<ast::Module>> modules
 *  Returns     : ir::Model Analyzer::
 */
ir::Model Analyzer::analyze(std::vector<std::unique_ptr<ast::Module>> modules) {
  modules_ = std::move(modules);
  model_ = ir::Model{};
  symbols_ = SymbolTable{};

  declare_pass();
  resolve_pass();
  lower_pass();
  tag_pass();

  return std::move(model_);
}

/**
 *  Function    : declare_pass
 *  Description : Performs declare pass (definition).
 *  Parameters  : none
 *  Returns     : void Analyzer::
 */
void Analyzer::declare_pass() {
  for (auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    ast::Module& mod = *mod_ptr;

    Symbol mod_sym;
    mod_sym.kind = SymbolKind::Module;
    mod_sym.module = mod.name();
    mod_sym.name = mod.name();
    mod_sym.range = mod.range();
    mod_sym.ast = &mod;
    symbols_.declare(std::move(mod_sym), diagnostics_);

    for (const auto& imp : mod.imports()) {
      for (const auto& sym : imp.symbols) {
        Symbol s;
        const bool is_type = !sym.name.empty() && sym.name[0] >= 'A' && sym.name[0] <= 'Z';
        s.kind = is_type ? SymbolKind::Type : SymbolKind::Value;
        s.module = mod.name();
        s.name = sym.name;
        s.range = sym.range;
        s.is_import = true;
        s.import_from_module = imp.module;
        symbols_.declare(std::move(s), diagnostics_);
      }
    }

    for (const auto& assignment : mod.assignments()) {
      if (const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::Type;
        s.module = mod.name();
        s.name = ta->name();
        s.range = ta->range();
        s.ast = ta;
        symbols_.declare(std::move(s), diagnostics_);
      } else if (const auto* va =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::ValueAssignment*>(assignment.
                      */
                     dynamic_cast<const ast::ValueAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::Value;
        s.module = mod.name();
        s.name = va->name();
        s.range = va->range();
        s.ast = va;
        symbols_.declare(std::move(s), diagnostics_);
      } else if (const auto* oc =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::ObjectClassAssignment*>(assignment.
                      */
                     dynamic_cast<const ast::ObjectClassAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::ObjectClass;
        s.module = mod.name();
        s.name = oc->name();
        s.range = oc->range();
        s.ast = oc;
        symbols_.declare(std::move(s), diagnostics_);
      } else if (const auto* obj =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::ObjectAssignment*>(assignment.
                      */
                     dynamic_cast<const ast::ObjectAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::Object;
        s.module = mod.name();
        s.name = obj->name();
        s.range = obj->range();
        s.ast = obj;
        symbols_.declare(std::move(s), diagnostics_);
      } else if (const auto* oset =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::ObjectSetAssignment*>(assignment.
                      */
                     dynamic_cast<const ast::ObjectSetAssignment*>(assignment.get())) {
        Symbol s;
        s.kind = SymbolKind::ObjectSet;
        s.module = mod.name();
        s.name = oset->name();
        s.range = oset->range();
        s.ast = oset;
        symbols_.declare(std::move(s), diagnostics_);
      }
    }
  }

  // Builtin useful object classes (X.681) available in every module scope.
  for (auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    for (const char* useful : {"TYPE-IDENTIFIER", "ABSTRACT-SYNTAX"}) {
      if (symbols_.find(mod_ptr->name(), useful)) {
        continue;
      }
      Symbol s;
      s.kind = SymbolKind::ObjectClass;
      s.module = mod_ptr->name();
      s.name = useful;
      s.range = mod_ptr->range();
      s.ast = nullptr;
      symbols_.declare(std::move(s), diagnostics_);
    }
  }
}

/**
 *  Function    : resolve_pass
 *  Description : Performs resolve pass (definition).
 *  Parameters  : none
 *  Returns     : void Analyzer::
 */
void Analyzer::resolve_pass() {
  for (const auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    const ast::Module& mod = *mod_ptr;
    for (const auto& imp : mod.imports()) {
      bool found_module = false;
      for (const auto& other : modules_) {
        if (other && other->name() == imp.module) {
          found_module = true;
          for (const auto& sym : imp.symbols) {
            const Symbol* exported = symbols_.find(imp.module, sym.name);
            if (!exported || exported->is_import) {
              diagnostics_.error(sym.range, "undefined ASN.1 type '" + sym.name +
                                                "' imported from module '" + imp.module + "'");
            }
          }
          break;
        }
      }
      if (!found_module) {
        diagnostics_.warning(imp.range, "imported module '" + imp.module +
                                            "' is not among the analyzed modules");
      }
    }

    for (const auto& assignment : mod.assignments()) {
      if (const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get())) {
        std::unordered_set<std::string> formals;
        for (const auto& p : ta->parameters()) {
          formals.insert(p.name);
        }
        check_type_refs(ta->type(), mod.name(), symbols_, diagnostics_,
                        formals.empty() ? nullptr : &formals);
      } else if (const auto* va =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::ValueAssignment*>(assignment.
                      */
                     dynamic_cast<const ast::ValueAssignment*>(assignment.get())) {
        check_type_refs(va->type(), mod.name(), symbols_, diagnostics_);
      }
    }
  }
}

/**
 *  Function    : lower_pass
 *  Description : Performs lower pass (definition).
 *  Parameters  : none
 *  Returns     : void Analyzer::
 */
void Analyzer::lower_pass() {
  for (auto& mod_ptr : modules_) {
    if (!mod_ptr) {
      continue;
    }
    ast::Module& mod = *mod_ptr;
    ir::ModuleInfo info;
    info.name = mod.name();
    switch (mod.tag_default()) {
      case ast::TagDefault::Explicit:
        info.tag_default = ir::ModuleInfo::TagDefault::Explicit;
        break;
      case ast::TagDefault::Implicit:
        info.tag_default = ir::ModuleInfo::TagDefault::Implicit;
        break;
      case ast::TagDefault::Automatic:
        info.tag_default = ir::ModuleInfo::TagDefault::Automatic;
        break;
    }
    info.extensibility_implied = mod.extensibility_implied();

    LowerCtx ctx{model_, symbols_, diagnostics_, mod, {}, {}};

    // Lower object classes first so ObjectClassFieldType can resolve fields.
    for (const auto& assignment : mod.assignments()) {
      const auto* oc = dynamic_cast<const ast::ObjectClassAssignment*>(assignment.get());
      if (!oc) {
        continue;
      }
      ir::ObjectClassInfo info;
      info.module = mod.name();
      info.name = oc->name();
      for (const auto& fptr : oc->defn().fields()) {
        const ast::FieldSpec& f = *fptr;
        ir::ClassField cf;
        cf.name = f.name();
        cf.unique = f.unique();
        cf.presence = map_presence(f.presence());
        if (f.kind() == ast::FieldSpecKind::TypeField) {
          cf.kind = ir::ClassFieldKind::TypeField;
        } else {
          cf.kind = ir::ClassFieldKind::FixedTypeValueField;
          if (f.field_type()) {
            cf.fixed_type = lower_type(ctx, *f.field_type(), "");
          }
        }
        info.fields.push_back(std::move(cf));
      }
      model_.object_classes.push_back(std::move(info));
    }

    // Builtin TYPE-IDENTIFIER / ABSTRACT-SYNTAX (X.681 Annex A / B).
    auto ensure_useful_class = [&](const char* name, bool with_property) {
      for (const auto& c : model_.object_classes) {
        if (c.name == name && c.module == mod.name()) {
          return;
        }
      }
      ir::ObjectClassInfo cls;
      cls.module = mod.name();
      cls.name = name;
      ir::ClassField id;
      id.kind = ir::ClassFieldKind::FixedTypeValueField;
      id.name = "id";
      id.unique = true;
      {
        ir::Type oid;
        oid.kind = ir::TypeKind::ObjectIdentifier;
        oid.module = mod.name();
        oid.tag = universal(6);
        id.fixed_type = model_.arena.add(std::move(oid));
      }
      ir::ClassField ty;
      ty.kind = ir::ClassFieldKind::TypeField;
      ty.name = "Type";
      cls.fields.push_back(std::move(id));
      cls.fields.push_back(std::move(ty));
      if (with_property) {
        ir::ClassField prop;
        prop.kind = ir::ClassFieldKind::FixedTypeValueField;
        prop.name = "property";
        prop.presence = ir::Presence::Default;
        {
          ir::Type bits;
          bits.kind = ir::TypeKind::BitString;
          bits.module = mod.name();
          bits.tag = universal(3);
          prop.fixed_type = model_.arena.add(std::move(bits));
        }
        cls.fields.push_back(std::move(prop));
      }
      model_.object_classes.push_back(std::move(cls));
    };
    ensure_useful_class("TYPE-IDENTIFIER", false);
    ensure_useful_class("ABSTRACT-SYNTAX", true);

    for (const auto& assignment : mod.assignments()) {
      if (const auto* obj = dynamic_cast<const ast::ObjectAssignment*>(assignment.get())) {
        ir::ObjectInfo info;
        info.module = mod.name();
        info.name = obj->name();
        info.class_name = obj->class_name();
        for (const auto& s : obj->defn().settings()) {
          ir::ObjectFieldSetting fs;
          fs.field_name = s.field_name;
          if (s.type_setting) {
            fs.type_setting = lower_type(ctx, *s.type_setting, "");
          }
          if (s.value_setting) {
            auto vid = lower_simple_value(ctx, *s.value_setting);
            if (vid) {
              fs.value_setting = *vid;
            }
          }
          info.settings.push_back(std::move(fs));
        }
        model_.objects.push_back(std::move(info));
      } else if (const auto* oset =
                     /**
                      *  Function    : get
                      *  Description : Computes get from (none).
                      *  Parameters  : none
                      *  Returns     : dynamic_cast<const ast::ObjectSetAssignment*>(assignment.
                      */
                     dynamic_cast<const ast::ObjectSetAssignment*>(assignment.get())) {
        ir::ObjectSetInfo info;
        info.module = mod.name();
        info.name = oset->name();
        info.class_name = oset->class_name();
        info.extensible = oset->defn().extensible();
        std::size_t inline_obj_idx = 0;
        for (const auto& e : oset->defn().elements()) {
          if (e.object_ref) {
            info.object_refs.push_back(*e.object_ref);
          } else if (e.inline_object) {
            ir::ObjectInfo inline_obj;
            inline_obj.module = mod.name();
            inline_obj.name = oset->name() + "_item_" + std::to_string(inline_obj_idx++);
            inline_obj.class_name = oset->class_name();
            for (const auto& s : e.inline_object->settings()) {
              ir::ObjectFieldSetting fs;
              fs.field_name = s.field_name;
              if (s.type_setting) {
                fs.type_setting = lower_type(ctx, *s.type_setting, "");
              }
              if (s.value_setting) {
                auto vid = lower_simple_value(ctx, *s.value_setting);
                if (vid) {
                  fs.value_setting = *vid;
                }
              }
              inline_obj.settings.push_back(std::move(fs));
            }
            info.object_refs.push_back(inline_obj.name);
            model_.objects.push_back(std::move(inline_obj));
          }
        }
        model_.object_sets.push_back(std::move(info));
      }
    }

    // Reserve stable TypeIds for all named types so recursion can point at them.
    // Skip parameterized templates — they are lowered only via instantiation.
    for (const auto& assignment : mod.assignments()) {
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get());
      if (!ta || !ta->parameters().empty()) {
        continue;
      }
      const std::string key = mod.name() + "::" + ta->name();
      ir::Type stub;
      stub.module = mod.name();
      stub.name = ta->name();
      stub.kind = ir::TypeKind::Null;
      const ir::TypeId id = model_.arena.add(std::move(stub));
      ctx.named_ids[key] = id;
      if (Symbol* sym = symbols_.find(mod.name(), ta->name())) {
        sym->type_id = id;
      }
    }

    for (const auto& assignment : mod.assignments()) {
      const auto* ta = dynamic_cast<const ast::TypeAssignment*>(assignment.get());
      if (!ta || !ta->parameters().empty()) {
        continue;
      }
      const std::string key = mod.name() + "::" + ta->name();
      const ir::TypeId reserved = ctx.named_ids[key];
      ctx.resolving.insert(key);
      const ir::TypeId built = lower_type(ctx, ta->type(), ta->name());
      ctx.resolving.erase(key);

      // Move built content into the reserved slot (keep stable id for recursion).
      // Type aliases (`A ::= B`) where B is not yet lowered still have a Null stub;
      // keep A as Referenced→B instead of copying the stub (Phase 38).
      if (built != reserved) {
        const ir::Type& built_t = model_.arena.get(built);
        const bool alias_to_unready_stub =
            built_t.kind == ir::TypeKind::Null && !built_t.name.empty() &&
            built_t.name != ta->name();
        if (alias_to_unready_stub) {
          ir::Type alias;
          alias.kind = ir::TypeKind::Referenced;
          alias.module = mod.name();
          alias.name = ta->name();
          alias.referenced.module = mod.name();
          alias.referenced.name = built_t.name;
          alias.referenced.resolved = built;
          model_.arena.get(reserved) = std::move(alias);
        } else {
          model_.arena.get(reserved) = model_.arena.get(built);
          model_.arena.get(reserved).module = mod.name();
          model_.arena.get(reserved).name = ta->name();
          // Redirect any field still pointing at `built` toward `reserved`.
          for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
            ir::Type& t = model_.arena.get(static_cast<ir::TypeId>(i));
            auto rew = [&](ir::TypeId& id) {
              if (id == built) {
                id = reserved;
              }
            };
            if (t.kind == ir::TypeKind::Sequence) {
              for (auto& f : t.sequence.root) {
                rew(f.type);
              }
              for (auto& g : t.sequence.extension_groups) {
                for (auto& f : g) {
                  rew(f.type);
                }
              }
              for (auto& f : t.sequence.trailing_root) {
                rew(f.type);
              }
            } else if (t.kind == ir::TypeKind::Set) {
              for (auto& f : t.set.root) {
                rew(f.type);
              }
              for (auto& g : t.set.extension_groups) {
                for (auto& f : g) {
                  rew(f.type);
                }
              }
              for (auto& f : t.set.trailing_root) {
                rew(f.type);
              }
            } else if (t.kind == ir::TypeKind::Choice) {
              for (auto& f : t.choice.alternatives) {
                rew(f.type);
              }
              for (auto& f : t.choice.extensions) {
                rew(f.type);
              }
            } else if (t.kind == ir::TypeKind::SequenceOf) {
              rew(t.sequence_of.element);
            } else if (t.kind == ir::TypeKind::SetOf) {
              rew(t.set_of.element);
            } else if (t.kind == ir::TypeKind::Referenced) {
              if (t.referenced.resolved == built) {
                t.referenced.resolved = reserved;
              }
            }
          }
        }
      }

      apply_jer_instructions(model_.arena.get(reserved), ta->type().encoding_instructions());
      apply_exer_instructions(model_.arena.get(reserved), ta->type().encoding_instructions());

      info.types.push_back(reserved);
      model_.exported_types.push_back(reserved);
    }

    info.jer_instructions = mod.jer_instructions();
    info.xer_instructions = mod.xer_instructions();
    for (const auto& clause : mod.jer_encoding_control()) {
      if (clause.target == ast::EncodingControlTarget::OctetString &&
          clause.instruction.kind == ast::EncodingInstructionKind::Base64) {
        for (std::size_t ti = 0; ti < model_.arena.type_count(); ++ti) {
          ir::Type& ot = model_.arena.get(static_cast<ir::TypeId>(ti));
          if (ot.module == mod.name() && ot.kind == ir::TypeKind::OctetString &&
              !ot.jer.base64) {
            ot.jer.base64 = true;
          }
        }
      }
    }
    for (const auto& clause : mod.xer_encoding_control()) {
      for (std::size_t ti = 0; ti < model_.arena.type_count(); ++ti) {
        ir::Type& ty = model_.arena.get(static_cast<ir::TypeId>(ti));
        if (ty.module != mod.name()) {
          continue;
        }
        if (clause.target == ast::EncodingControlTarget::OctetString &&
            clause.instruction.kind == ast::EncodingInstructionKind::Base64 &&
            ty.kind == ir::TypeKind::OctetString) {
          ty.exer.base64 = true;
        }
        if (clause.target == ast::EncodingControlTarget::Boolean &&
            clause.instruction.kind == ast::EncodingInstructionKind::Text &&
            ty.kind == ir::TypeKind::Boolean) {
          ty.exer.text = true;
        }
        if (clause.target == ast::EncodingControlTarget::Enumerated &&
            clause.instruction.kind == ast::EncodingInstructionKind::UseNumber &&
            ty.kind == ir::TypeKind::Enumerated) {
          ty.exer.use_number = true;
        }
        if ((clause.target == ast::EncodingControlTarget::SequenceOf ||
             clause.target == ast::EncodingControlTarget::SetOf) &&
            /**
             *  Function    : &&
             *  Description : Computes && from (none).
             *  Parameters  : none
             *  Returns     : clause.instruction.kind == ast::EncodingInstructionKind::List
             */
            clause.instruction.kind == ast::EncodingInstructionKind::List &&
            (ty.kind == ir::TypeKind::SequenceOf || ty.kind == ir::TypeKind::SetOf)) {
          ty.exer.list = true;
        }
      }
    }

    model_.modules.push_back(std::move(info));
  }

  // Object classes from later modules (e.g. S1AP-Containers) may register after
  // earlier modules already instantiated ProtocolIE-Field; re-bind fixed fields.
  re_resolve_all_object_class_fields(model_);
}

void resolve_table_constraints(ir::Model& model, const SymbolTable& symbols) {
  std::unordered_map<std::string, const ir::ObjectSetInfo*> os_by_name;
  for (const auto& os : model.object_sets) {
    os_by_name[os.name] = &os;
  }

  std::unordered_map<std::string, const ir::ObjectInfo*> obj_by_name;
  for (const auto& obj : model.objects) {
    obj_by_name[obj.name] = &obj;
  }

  auto resolve_integer_constant = [&](const std::string& name) -> std::optional<ir::BigInt> {
    for (const auto& pair : symbols.entries()) {
      if (pair.second.kind == SymbolKind::Value && pair.second.name == name) {
        if (const auto* va = dynamic_cast<const ast::ValueAssignment*>(pair.second.ast)) {
          if (const auto* iv = dynamic_cast<const ast::IntegerValue*>(&va->value())) {
            return ir::BigInt::from_decimal(iv->text(), iv->negative());
          }
        }
      }
    }
    return std::nullopt;
  };

  auto resolve_field_constraint = [&](ir::Field& f, const std::string& /*parent_module*/) {
    if (!f.table_constraint || f.table_constraint->object_set_name.empty()) {
      return;
    }
    const auto os_it = os_by_name.find(f.table_constraint->object_set_name);
    if (os_it == os_by_name.end() || !os_it->second) {
      return;
    }
    const ir::ObjectSetInfo& os = *os_it->second;
    const std::string& gov_name = f.table_constraint->governor_field_name;

    for (const auto& obj_ref : os.object_refs) {
      const auto obj_it = obj_by_name.find(obj_ref);
      if (obj_it == obj_by_name.end() || !obj_it->second) {
        continue;
      }
      const ir::ObjectInfo& obj = *obj_it->second;

      std::string id_symbol;
      ir::BigInt id_val;
      bool has_id_val = false;
      ir::TypeId target_type = ir::kInvalidType;
      std::string type_name;

      for (const auto& s : obj.settings) {
        std::string lower_sname = s.field_name;
        for (char& c : lower_sname) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        std::string lower_gov = gov_name;
        for (char& c : lower_gov) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (lower_sname == lower_gov || lower_sname == "&" + lower_gov) {
          if (s.value_setting != ir::kInvalidValue) {
            const auto& val = model.arena.get_value(s.value_setting);
            if (val.kind == ir::ValueKind::Integer) {
              id_val = val.integer;
              has_id_val = true;
            } else if (val.kind == ir::ValueKind::Reference) {
              id_symbol = val.name;
              auto resolved = resolve_integer_constant(val.name);
              if (resolved) {
                id_val = *resolved;
                has_id_val = true;
              }
            }
          }
        }

        if (s.type_setting != ir::kInvalidType) {
          if (lower_sname.find("type") != std::string::npos ||
              lower_sname.find("val") != std::string::npos ||
              target_type == ir::kInvalidType) {
            target_type = s.type_setting;
          }
        }
      }

      if (target_type != ir::kInvalidType) {
        ir::TypeId cur = target_type;
        while (cur != ir::kInvalidType) {
          const auto& t = model.arena.get(cur);
          if (t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType) {
            cur = t.referenced.resolved;
          } else {
            type_name = t.name;
            break;
          }
        }

        ir::TableConstraintEntry entry;
        entry.id_symbol = std::move(id_symbol);
        entry.id_value = std::move(id_val);
        entry.has_id_value = has_id_val;
        entry.type = cur != ir::kInvalidType ? cur : target_type;
        entry.type_name = std::move(type_name);
        f.table_constraint->entries.push_back(std::move(entry));
      }
    }
  };

  for (std::size_t i = 0; i < model.arena.type_count(); ++i) {
    ir::Type& t = model.arena.get(static_cast<ir::TypeId>(i));
    if (t.kind == ir::TypeKind::Sequence) {
      for (auto& f : t.sequence.root) {
        resolve_field_constraint(f, t.module);
      }
      for (auto& g : t.sequence.extension_groups) {
        for (auto& f : g) {
          resolve_field_constraint(f, t.module);
        }
      }
      for (auto& f : t.sequence.trailing_root) {
        resolve_field_constraint(f, t.module);
      }
    } else if (t.kind == ir::TypeKind::Set) {
      for (auto& f : t.set.root) {
        resolve_field_constraint(f, t.module);
      }
      for (auto& g : t.set.extension_groups) {
        for (auto& f : g) {
          resolve_field_constraint(f, t.module);
        }
      }
      for (auto& f : t.set.trailing_root) {
        resolve_field_constraint(f, t.module);
      }
    }
  }
}

/**
 *  Function    : tag_pass
 *  Description : Performs tag pass (definition).
 *  Parameters  : none
 *  Returns     : void Analyzer::
 */
void Analyzer::tag_pass() {
  // Resolve Referenced placeholders and rewrite field type ids to the real type.
  for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
    ir::Type& t = model_.arena.get(static_cast<ir::TypeId>(i));
    if (t.kind == ir::TypeKind::Referenced && t.referenced.resolved == ir::kInvalidType) {
      if (Symbol* sym = symbols_.find(t.referenced.module, t.referenced.name)) {
        t.referenced.resolved = sym->type_id;
      }
    }
  }

  auto rewrite = [&](ir::TypeId& id) {
    if (id == ir::kInvalidType) {
      return;
    }
    const ir::Type& t = model_.arena.get(id);
    if (t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType) {
      id = t.referenced.resolved;
    }
  };

  auto rewrite_field = [&](ir::Field& f) {
    rewrite(f.type);
    if (f.tag.cls == ir::TagClass::Universal && f.tag.number == 0 && !f.tag.is_explicit &&
        f.type != ir::kInvalidType) {
      const ir::Type& ft = model_.arena.get(f.type);
      if (ft.tag.cls != ir::TagClass::Universal || ft.tag.number != 0) {
        f.tag = ft.tag;
      } else if (ft.kind != ir::TypeKind::Choice) {
        // Fall back to the natural universal tag of the resolved type
        f.tag.cls = ir::TagClass::Universal;
        f.tag.number = universal_tag_number_for(ft.kind);
      }
    }
  };

  for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
    ir::Type& t = model_.arena.get(static_cast<ir::TypeId>(i));
    if (t.kind == ir::TypeKind::Sequence) {
      for (auto& f : t.sequence.root) {
        rewrite_field(f);
      }
      for (auto& group : t.sequence.extension_groups) {
        for (auto& f : group) {
          rewrite_field(f);
        }
      }
      for (auto& f : t.sequence.trailing_root) {
        rewrite_field(f);
      }
    } else if (t.kind == ir::TypeKind::Set) {
      for (auto& f : t.set.root) {
        rewrite_field(f);
      }
      for (auto& group : t.set.extension_groups) {
        for (auto& f : group) {
          rewrite_field(f);
        }
      }
      for (auto& f : t.set.trailing_root) {
        rewrite_field(f);
      }
    } else if (t.kind == ir::TypeKind::Choice) {
      for (auto& f : t.choice.alternatives) {
        rewrite_field(f);
      }
      for (auto& f : t.choice.extensions) {
        rewrite_field(f);
      }
    } else if (t.kind == ir::TypeKind::SequenceOf) {
      rewrite(t.sequence_of.element);
    } else if (t.kind == ir::TypeKind::SetOf) {
      rewrite(t.set_of.element);
    }
  }

  for (auto& obj : model_.objects) {
    for (auto& s : obj.settings) {
      rewrite(s.type_setting);
    }
  }

  resolve_table_constraints(model_, symbols_);
}

}  // namespace asn1
