/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/constraints/normalize.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Constraint normalization algorithms.
**
** Specification: ITU-T X.680 — subtyping and constraint notation (X.682
**                 constraint application).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/constraints/normalize.hpp>

#include <algorithm>
#include <climits>
#include <utility>
#include <vector>

namespace asn1 {
namespace constraints {
namespace {

using ir::BigInt;
using ir::ConstraintDesc;
using ir::IntegerInterval;

/**
 *  Function    : from_ast_value
 *  Description : Computes from ast value from (v, resolver).
 *  Parameters  : v — const ast::Value* v; resolver — const ValueResolver& resolver
 *  Returns     : std::optional<BigInt>
 */
std::optional<BigInt> from_ast_value(const ast::Value* v, const ValueResolver& resolver) {
  if (const auto* iv = dynamic_cast<const ast::IntegerValue*>(v)) {
    return BigInt::from_decimal(iv->text(), iv->negative());
  }
  if (const auto* vr = dynamic_cast<const ast::ValueReference*>(v)) {
    if (resolver) {
      return resolver(vr->name());
    }
  }
  return std::nullopt;
}

ConstraintDesc from_intervals(std::vector<IntegerInterval> intervals, bool extensible,
                              bool is_size) {
  ConstraintDesc out;
  out.extensible = extensible;
  out.is_size = is_size;
  out.root_ranges = std::move(intervals);
  out.empty = out.root_ranges.empty();
  out.recompute_envelope();
  return out;
}

/**
 *  Function    : sort_and_merge
 *  Description : Performs sort and merge (definition).
 *  Parameters  : intervals — std::vector<IntegerInterval>& intervals
 *  Returns     : void
 */
void sort_and_merge(std::vector<IntegerInterval>& intervals) {
  if (intervals.empty()) {
    return;
  }
  std::sort(intervals.begin(), intervals.end(),
            [](const IntegerInterval& a, const IntegerInterval& b) {
              if (!a.lower && b.lower) {
                return true;
              }
              if (a.lower && !b.lower) {
                return false;
              }
              if (a.lower && b.lower && *a.lower != *b.lower) {
                return *a.lower < *b.lower;
              }
              // Same lower: prefer unbounded upper first
              if (!a.upper && b.upper) {
                return true;
              }
              if (a.upper && !b.upper) {
                return false;
              }
              if (a.upper && b.upper) {
                return *a.upper < *b.upper;
              }
              return false;
            });

  std::vector<IntegerInterval> merged;
  merged.push_back(intervals.front());
  for (std::size_t i = 1; i < intervals.size(); ++i) {
    if (merged.back().overlaps_or_adjacent(intervals[i])) {
      merged.back() = merged.back().merged_with(intervals[i]);
    } else {
      merged.push_back(intervals[i]);
    }
  }
  intervals = std::move(merged);
}

/**
 *  Function    : normalize_union
 *  Description : Computes normalize union from (parts).
 *  Parameters  : parts — std::vector<ConstraintDesc> parts
 *  Returns     : ConstraintDesc
 */
ConstraintDesc normalize_union(std::vector<ConstraintDesc> parts) {
  ConstraintDesc out;
  out.statically_foldable = true;
  bool any_size = false;
  bool any_ext = false;
  std::vector<IntegerInterval> all;
  for (auto& p : parts) {
    if (!p.statically_foldable) {
      out.statically_foldable = false;
    }
    any_size = any_size || p.is_size;
    any_ext = any_ext || p.extensible;
    if (p.empty) {
      continue;
    }
    if (p.root_ranges.empty() && !p.lower && !p.upper && p.statically_foldable) {
      // Fully unconstrained part => union is unconstrained.
      out.root_ranges.clear();
      out.lower.reset();
      out.upper.reset();
      out.extensible = any_ext;
      out.is_size = any_size;
      out.recompute_envelope();
      return out;
    }
    for (auto& iv : p.root_ranges) {
      all.push_back(std::move(iv));
    }
  }
  sort_and_merge(all);
  out = from_intervals(std::move(all), any_ext, any_size);
  out.statically_foldable = true;
  for (const auto& p : parts) {
    if (!p.statically_foldable) {
      out.statically_foldable = false;
    }
  }
  return out;
}

ConstraintDesc normalize_intersection(std::vector<ConstraintDesc> parts,
                                      Diagnostics& diagnostics, SourceRange where) {
  if (parts.empty()) {
    ConstraintDesc empty;
    empty.empty = true;
    empty.statically_foldable = true;
    return empty;
  }
  ConstraintDesc acc = std::move(parts.front());
  for (std::size_t i = 1; i < parts.size(); ++i) {
    ConstraintDesc& other = parts[i];
    if (!other.statically_foldable || !acc.statically_foldable) {
      acc.statically_foldable = false;
      diagnostics.warning(where, "intersection involves non-static constraint");
      return acc;
    }
    acc.extensible = acc.extensible || other.extensible;
    acc.is_size = acc.is_size || other.is_size;

    const bool acc_unconstrained = acc.root_ranges.empty() && !acc.empty;
    const bool other_unconstrained = other.root_ranges.empty() && !other.empty;
    if (acc_unconstrained) {
      other.extensible = acc.extensible;
      other.is_size = acc.is_size;
      acc = std::move(other);
      continue;
    }
    if (other_unconstrained) {
      continue;
    }

    std::vector<IntegerInterval> next;
    for (const auto& a : acc.root_ranges) {
      for (const auto& b : other.root_ranges) {
        if (auto iv = a.intersected_with(b)) {
          next.push_back(*iv);
        }
      }
    }
    sort_and_merge(next);
    acc.root_ranges = std::move(next);
    acc.empty = acc.root_ranges.empty();
    acc.recompute_envelope();
  }
  if (acc.empty) {
    diagnostics.warning(where, "constraint intersection is empty");
  }
  return acc;
}

ConstraintDesc normalize_ast(const ast::Constraint* c, Diagnostics& diag, const ValueResolver& resolver);

/**
 *  Function    : normalize_ast
 *  Description : Computes normalize ast from (c, diag, resolver).
 *  Parameters  : c — const ast::Constraint* c; diag — Diagnostics& diag; resolver — const ValueResolver& resolver
 *  Returns     : ConstraintDesc
 */
ConstraintDesc normalize_ast(const ast::Constraint* c, Diagnostics& diag, const ValueResolver& resolver) {
  ConstraintDesc out;
  if (!c) {
    return out;
  }

  if (const auto* ext = dynamic_cast<const ast::ExtensibleConstraint*>(c)) {
    if (ext->root()) {
      out = normalize_ast(ext->root(), diag, resolver);
    }
    out.extensible = true;
    return out;
  }

  if (const auto* size = dynamic_cast<const ast::SizeConstraint*>(c)) {
    out = normalize_ast(&size->inner(), diag, resolver);
    out.is_size = true;
    return out;
  }

  if (const auto* uni = dynamic_cast<const ast::UnionConstraint*>(c)) {
    std::vector<ConstraintDesc> parts;
    parts.reserve(uni->alternatives().size());
    for (const auto& alt : uni->alternatives()) {
      parts.push_back(normalize_ast(alt.get(), diag, resolver));
    }
    return normalize_union(std::move(parts));
  }

  if (const auto* inter = dynamic_cast<const ast::IntersectionConstraint*>(c)) {
    std::vector<ConstraintDesc> parts;
    parts.reserve(inter->parts().size());
    for (const auto& p : inter->parts()) {
      parts.push_back(normalize_ast(p.get(), diag, resolver));
    }
    return normalize_intersection(std::move(parts), diag, c->range());
  }

  if (const auto* range = dynamic_cast<const ast::ValueRangeConstraint*>(c)) {
    IntegerInterval iv;
    if (range->lower()) {
      iv.lower = from_ast_value(range->lower(), resolver);
      if (!iv.lower) {
        out.statically_foldable = false;
        diag.warning(c->range(), "lower bound is not a static integer");
        return out;
      }
    }
    if (range->upper()) {
      iv.upper = from_ast_value(range->upper(), resolver);
      if (!iv.upper) {
        out.statically_foldable = false;
        diag.warning(c->range(), "upper bound is not a static integer");
        return out;
      }
    }
    if (iv.lower && iv.upper && *iv.lower > *iv.upper) {
      diag.error(c->range(), "invalid value range: lower bound exceeds upper bound");
      out.empty = true;
      out.statically_foldable = true;
      return out;
    }
    out.root_ranges.push_back(std::move(iv));
    out.recompute_envelope();
    return out;
  }

  if (const auto* single = dynamic_cast<const ast::SingleValueConstraint*>(c)) {
    auto v = from_ast_value(&single->value(), resolver);
    if (!v) {
      out.statically_foldable = false;
      diag.warning(c->range(), "single-value constraint is not a static integer");
      return out;
    }
    IntegerInterval iv;
    iv.lower = v;
    iv.upper = v;
    out.root_ranges.push_back(std::move(iv));
    out.recompute_envelope();
    return out;
  }

  // ContentsConstraint (CONTAINING / ENCODED BY) does not constrain SIZE or value
  // ranges; treat as unconstrained so it can intersect with SIZE.
  if (dynamic_cast<const ast::ContentsConstraint*>(c)) {
    out.statically_foldable = true;
    return out;
  }

  out.statically_foldable = false;
  diag.warning(c->range(), "unsupported constraint form in Phase 5 normalizer");
  return out;
}

}  // namespace

ir::ConstraintDesc normalize(const ast::Constraint* constraint, Diagnostics& diagnostics,
                             ValueResolver resolver) {
  return normalize_ast(constraint, diagnostics, resolver);
}

ir::ConstraintDesc normalize_size(const ast::Constraint* constraint, Diagnostics& diagnostics,
                                  ValueResolver resolver) {
  auto c = normalize_ast(constraint, diagnostics, resolver);
  c.is_size = true;
  return c;
}

/**
 *  Function    : suggest_host_integer
 *  Description : Performs suggest host integer (definition).
 *  Parameters  : desc — ir::IntegerDesc& desc
 *  Returns     : void
 */
void suggest_host_integer(ir::IntegerDesc& desc) {
  desc.host_bits.reset();
  if (desc.constraint.extensible) {
    // Extensible integers can hold arbitrary extension additions beyond root bounds;
    // promote to int64 or BigInteger so decoded values aren't truncated by narrow casts.
    desc.host_bits = 64;
    desc.is_signed = true;
    return;
  }
  if (!desc.constraint.lower || !desc.constraint.upper || !desc.constraint.lower->as_i64 ||
      !desc.constraint.upper->as_i64) {
    return;
  }
  // Multiple disjoint root ranges: still use envelope for host storage width.
  const std::int64_t lo = *desc.constraint.lower->as_i64;
  const std::int64_t hi = *desc.constraint.upper->as_i64;
  if (lo > hi) {
    return;
  }
  if (lo >= 0) {
    desc.is_signed = false;
    const std::uint64_t top = static_cast<std::uint64_t>(hi);
    if (top <= 0xFFu) {
      desc.host_bits = 8;
    } else if (top <= 0xFFFFu) {
      desc.host_bits = 16;
    } else if (top <= 0xFFFFFFFFu) {
      desc.host_bits = 32;
    } else {
      desc.host_bits = 64;
    }
  } else {
    desc.is_signed = true;
    if (lo >= INT8_MIN && hi <= INT8_MAX) {
      desc.host_bits = 8;
    } else if (lo >= INT16_MIN && hi <= INT16_MAX) {
      desc.host_bits = 16;
    } else if (lo >= INT32_MIN && hi <= INT32_MAX) {
      desc.host_bits = 32;
    } else {
      desc.host_bits = 64;
    }
  }
}

}  // namespace constraints
}  // namespace asn1
