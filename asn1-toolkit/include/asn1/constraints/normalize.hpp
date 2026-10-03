#pragma once

#include <asn1/ast/type.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/support/diagnostics.hpp>
#include <functional>

namespace asn1 {
namespace constraints {

using ValueResolver = std::function<std::optional<ir::BigInt>(const std::string&)>;

/// Normalize an AST constraint tree into an IR ConstraintDesc suitable for PER.
///
/// Handles: value range, single value, SIZE(...), union (|), intersection (^),
/// extension marker (, ...). Non-static endpoints are reported and marked
/// not statically foldable.
ir::ConstraintDesc normalize(const ast::Constraint* constraint, Diagnostics& diagnostics,
                             ValueResolver resolver = nullptr);

/// Like normalize(), then force is_size=true (BIT/OCTET STRING, character strings, * OF).
ir::ConstraintDesc normalize_size(const ast::Constraint* constraint, Diagnostics& diagnostics,
                                  ValueResolver resolver = nullptr);

/// Build host_bits / is_signed suggestions from a normalized integer constraint.
void suggest_host_integer(ir::IntegerDesc& desc);

}  // namespace constraints
}  // namespace asn1
