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
}}}
