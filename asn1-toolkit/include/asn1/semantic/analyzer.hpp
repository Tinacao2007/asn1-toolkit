/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/semantic/analyzer.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Semantic passes: resolve names, tags, constraints, emit IR.
**
** Specification: ITU-T X.680 — name and module semantics;
**                 ITU-T X.681 / X.683 — object and parameterization
**                 semantics where implemented.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/ast/module.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/semantic/symbol_table.hpp>
#include <asn1/support/diagnostics.hpp>

#include <memory>
#include <vector>

namespace asn1 {

/// Semantic analyzer: declare - resolve - tag - lower to IR.
class Analyzer {
 public:
  /**
   *  Function    : Analyzer
   *  Description : Constructs or initializes Analyzer.
   *  Parameters  : diagnostics — Diagnostics& diagnostics
   *  Returns     : explicit
   */
  explicit Analyzer(Diagnostics& diagnostics);

  /**
   *  Function    : analyze
   *  Description : Computes analyze from (modules).
   *  Parameters  : modules — std::vector<std::unique_ptr<ast::Module>> modules
   *  Returns     : ir::Model
   */
  ir::Model analyze(std::vector<std::unique_ptr<ast::Module>> modules);

  /**
   *  Function    : symbols
   *  Description : Computes symbols from (none).
   *  Parameters  : none
   *  Returns     : const SymbolTable&
   */
  const SymbolTable& symbols() const noexcept { return symbols_; }

 private:
  /**
   *  Function    : declare_pass
   *  Description : Performs declare pass (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void declare_pass();
  /**
   *  Function    : resolve_pass
   *  Description : Performs resolve pass (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void resolve_pass();
  /**
   *  Function    : tag_pass
   *  Description : Performs tag pass (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void tag_pass();
  /**
   *  Function    : lower_pass
   *  Description : Performs lower pass (declaration).
   *  Parameters  : none
   *  Returns     : void
   */
  void lower_pass();

  Diagnostics& diagnostics_;
  SymbolTable symbols_;
  std::vector<std::unique_ptr<ast::Module>> modules_;
  ir::Model model_;
};

}  // namespace asn1
