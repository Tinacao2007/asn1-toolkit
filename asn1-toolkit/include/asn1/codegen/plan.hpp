/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/codegen/plan.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Public interface declarations for codegen / plan.hpp.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once
#include <asn1/ir/type.hpp>
#include <cstddef>
#include <optional>
#include <vector>
#include <algorithm>
#include <limits>
#include <utility>

namespace asn1 { namespace codegen { namespace planning {
// Borrowed views: model/descriptor must outlive the plan and remain immutable.
// No C/C++ spelling, runtime types, buffer access, or policy global state here.
inline bool has_presence_bit(const ir::Field& field) {
  return field.presence == ir::Presence::Optional || field.presence == ir::Presence::Default;
}
struct FieldPlan {
  const ir::Field* field;
  ir::Tag effective_tag;
  std::optional<std::size_t> presence_index;
  const ir::TableConstraintInfo* selector;
};
struct ExtensionPlan {
  const ir::ExtensionGroup* group;
  // Version bracket groups carry a nested SEQUENCE preamble; singleton additions do not.
  bool nested_sequence;
};
struct SequencePlan {
  std::vector<FieldPlan> root;
  std::vector<const ir::Field*> presence_fields;
  std::vector<ExtensionPlan> extensions;
  bool extensible = false;
};
inline SequencePlan sequence(const ir::SequenceDesc& descriptor) {
  SequencePlan plan;
  plan.extensible = descriptor.extensible;
  auto append = [&](const std::vector<ir::Field>& fields) {
    for (const auto& field : fields) {
      std::optional<std::size_t> index;
      if (has_presence_bit(field)) {
        index = plan.presence_fields.size();
        plan.presence_fields.push_back(&field);
      }
      plan.root.push_back({&field, field.tag, index,
                           field.table_constraint ? &*field.table_constraint : nullptr});
    }
  };
  append(descriptor.root);
  append(descriptor.trailing_root);
  for (const auto& group : descriptor.extension_groups)
    plan.extensions.push_back({&group, group.version_bracket});
  return plan;
}
struct AlternativePlan {
  const ir::Field* field;
  ir::Tag effective_tag;
  const ir::TableConstraintInfo* selector;
  std::size_t ordinal; // zero-based within root or extension region
};
struct ChoicePlan {
  std::vector<AlternativePlan> root;
  std::vector<AlternativePlan> extensions;
  bool extensible = false;
};
inline ChoicePlan choice(const ir::ChoiceDesc& descriptor) {
  ChoicePlan plan;
  plan.extensible = descriptor.extensible;
  const auto append = [](const std::vector<ir::Field>& fields,
                         std::vector<AlternativePlan>& out) {
    for (const auto& field : fields)
      out.push_back({&field, field.tag,
                     field.table_constraint ? &*field.table_constraint : nullptr,
                     out.size()});
  };
  append(descriptor.alternatives, plan.root);
  append(descriptor.extensions, plan.extensions);
  return plan;
}
// Literal normalization shared by language backends. Legacy IR strings retain
// lexer quotes; binary values retain their original B/H token spelling.
inline std::optional<std::string> string_default(const ir::Value& value) {
  if (value.kind!=ir::ValueKind::String) return std::nullopt;
  std::string text=value.text;
  if (text.size()>=2 && text.front()=='"' && text.back()=='"') {
    text=text.substr(1,text.size()-2);
    std::string normalized;
    for (std::size_t i=0;i<text.size();++i) {
      normalized+=text[i];
      if(text[i]=='"' && i+1<text.size() && text[i+1]=='"') ++i;
    }
    return normalized;
  }
  return text;
}
struct BinaryDefault { std::vector<std::uint8_t> bytes; std::size_t bits=0; };
inline std::optional<BinaryDefault> binary_default(const ir::Value& value) {
  if (value.kind!=ir::ValueKind::BitOrOctet) return std::nullopt;
  const auto& token=value.text;
  if (token.size()<3 || token.front()!='\'' || token[token.size()-2]!='\'') return std::nullopt;
  const auto radix=token.back();
  if (radix!='B' && radix!='b' && radix!='H' && radix!='h') return std::nullopt;
  BinaryDefault out;
  for(std::size_t i=1;i+2<token.size();++i) {
    const char c=token[i]; if(c==' ' || c=='\r' || c=='\n' || c=='\t') continue;
    unsigned number;
    if(c>='0' && c<='9') number=unsigned(c-'0');
    else if(c>='a' && c<='f') number=unsigned(c-'a'+10);
    else if(c>='A' && c<='F') number=unsigned(c-'A'+10);
    else return std::nullopt;
    const unsigned width=radix=='B'||radix=='b'?1:4;
    if(number >= (1u<<width)) return std::nullopt;
    for(unsigned j=width;j>0;--j) {
      if(out.bits%8==0) out.bytes.push_back(0);
      if(number & (1u<<(j-1))) out.bytes.back()|=static_cast<std::uint8_t>(0x80u>>(out.bits%8));
      ++out.bits;
    }
  }
  return out;
}

// Target-neutral flattened storage/dispatch view. Indices preserve root,
// trailing root, CHOICE regions, and extension-group identity.
struct EmissionFieldPlan {
  const ir::Field* field;
  ir::Tag effective_tag;
  std::optional<std::size_t> presence_index;
  const ir::TableConstraintInfo* selector;
  std::optional<std::size_t> extension_group;
  bool nested_sequence;
  std::size_t ordinal;
};
inline std::vector<EmissionFieldPlan> fields(const ir::Type& type) {
  std::vector<EmissionFieldPlan> out;
  if (type.kind == ir::TypeKind::Sequence || type.kind == ir::TypeKind::Set) {
    const auto plan = sequence(type.kind == ir::TypeKind::Set ? type.set : type.sequence);
    for (const auto& f : plan.root)
      out.push_back({f.field,f.effective_tag,f.presence_index,f.selector,std::nullopt,false,out.size()});
    for (std::size_t i=0; i<plan.extensions.size(); ++i) {
      const auto& group=plan.extensions[i];
      std::size_t presence=0;
      for (const auto& field: group.group->fields) {
        std::optional<std::size_t> index;
        if(group.nested_sequence && has_presence_bit(field)) index=presence++;
        out.push_back({&field,field.tag,index,
                       field.table_constraint ? &*field.table_constraint : nullptr,
                       i,group.nested_sequence,out.size()});
      }
    }
  } else if (type.kind == ir::TypeKind::Choice) {
    const auto plan=choice(type.choice);
    for (const auto& f: plan.root)
      out.push_back({f.field,f.effective_tag,std::nullopt,f.selector,std::nullopt,false,f.ordinal});
    for (const auto& f: plan.extensions)
      out.push_back({f.field,f.effective_tag,std::nullopt,f.selector,f.ordinal,false,f.ordinal});
  }
  return out;
}

// Retain exact normalized ranges, not just their envelope; C and C++ adapters
// must use the same descriptor and cannot turn a disjoint constraint into a range.
struct ConstraintPlan {
  const ir::ConstraintDesc* descriptor;
  ir::PerBoundKind per_kind;
  bool requires_membership;
};
inline ConstraintPlan constraint(const ir::ConstraintDesc& descriptor) {
  return {&descriptor, descriptor.per_kind(), !descriptor.extensible && !descriptor.root_ranges.empty()};
}
// Length is measured in octets for OCTET STRING and elements for OF. Character
// string lengths need repertoire-aware counting and must not use byte counts.
inline const ir::ConstraintDesc* storage_size(const ir::Type& type) {
  switch (type.kind) {
    case ir::TypeKind::BitString: return &type.bit_string.size;
    case ir::TypeKind::OctetString: return &type.octet_string.size;
    case ir::TypeKind::SequenceOf: return &type.sequence_of.size;
    case ir::TypeKind::SetOf: return &type.set_of.size;
    default: return nullptr;
  }
}
// OER SET ordering uses the minimum tag in root CHOICE alternatives only.
// Keep this wire decision shared by the C and C++ backends.
inline std::pair<unsigned,std::uint64_t> component_tag(const ir::Model& model,
                                                     const ir::Field& field,
                                                     std::size_t depth=0) {
  const auto key=[](const ir::Tag& tag){return std::make_pair(static_cast<unsigned>(tag.cls),tag.number);};
  if(field.tag.cls!=ir::TagClass::Universal||field.tag.number!=0)return key(field.tag);
  auto id=field.type;
  for(std::size_t i=depth;i<256;++i){const auto& type=model.arena.get(id);
    if(type.tag.cls!=ir::TagClass::Universal||type.tag.number!=0)return key(type.tag);
    if(type.kind==ir::TypeKind::Referenced){id=type.referenced.resolved;continue;}
    if(type.kind==ir::TypeKind::ObjectClassField&&!type.object_class_field.open_type){id=type.object_class_field.fixed_type;continue;}
    if(type.kind==ir::TypeKind::Choice){auto best=std::make_pair(255u,std::uint64_t(0));
      for(const auto& alternative:type.choice.alternatives)
        best=std::min(best,component_tag(model,alternative,i+1));
      return best;
    }
    unsigned natural=0;
    switch(type.kind){
      case ir::TypeKind::Boolean:natural=1;break;case ir::TypeKind::Integer:natural=2;break;
      case ir::TypeKind::BitString:natural=3;break;case ir::TypeKind::OctetString:natural=4;break;
      case ir::TypeKind::Null:natural=5;break;case ir::TypeKind::ObjectIdentifier:natural=6;break;
      case ir::TypeKind::Real:natural=9;break;case ir::TypeKind::Enumerated:natural=10;break;
      case ir::TypeKind::RelativeOid:natural=13;break;
      case ir::TypeKind::Sequence:case ir::TypeKind::SequenceOf:natural=16;break;
      case ir::TypeKind::Set:case ir::TypeKind::SetOf:natural=17;break;
      case ir::TypeKind::String:
        switch(type.string.kind){
          case ir::StringKind::UTF8String:natural=12;break;case ir::StringKind::NumericString:natural=18;break;
          case ir::StringKind::PrintableString:natural=19;break;case ir::StringKind::TeletexString:natural=20;break;
          case ir::StringKind::VideotexString:natural=21;break;case ir::StringKind::IA5String:natural=22;break;
          case ir::StringKind::UTCTime:natural=23;break;case ir::StringKind::GeneralizedTime:natural=24;break;
          case ir::StringKind::GraphicString:natural=25;break;case ir::StringKind::VisibleString:natural=26;break;
          case ir::StringKind::GeneralString:natural=27;break;case ir::StringKind::UniversalString:natural=28;break;
          case ir::StringKind::BMPString:natural=30;break;
        }break;
      default:break;
    }
    if(natural)return std::make_pair(0u,std::uint64_t(natural));
    break;
  }
  return std::make_pair(255u,std::uint64_t(0));
}
inline std::vector<const ir::Field*> oer_root_fields(const ir::Model& model,
                                                    const ir::SequenceDesc& descriptor,bool is_set){
  std::vector<const ir::Field*> fields;
  for(const auto& f:sequence(descriptor).root)fields.push_back(f.field);
  if(is_set)std::stable_sort(fields.begin(),fields.end(),[&](const ir::Field* a,const ir::Field* b){return component_tag(model,*a)<component_tag(model,*b);});
  return fields;
}
}}}
