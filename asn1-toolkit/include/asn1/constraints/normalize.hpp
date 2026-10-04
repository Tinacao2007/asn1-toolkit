/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/constraints/normalize.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Normalizes ASN.1 constraint notation for IR and codecs.
**
** Specification: ITU-T X.680 — subtyping and constraint notation (X.682
**                 constraint application).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
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
/**
 *  Function    : suggest_host_integer
 *  Description : Performs suggest host integer (declaration).
 *  Parameters  : desc — ir::IntegerDesc& desc
 *  Returns     : void
 */
void suggest_host_integer(ir::IntegerDesc& desc);

}  // namespace constraints
}  // namespace asn1
