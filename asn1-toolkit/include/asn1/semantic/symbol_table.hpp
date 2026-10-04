/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/semantic/symbol_table.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Cross-module symbol resolution for types and values.
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
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_location.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace asn1 {

enum class SymbolKind { Type, Value, Module, ObjectClass, Object, ObjectSet };

struct Symbol {
  SymbolKind kind = SymbolKind::Type;
  std::string module;
  std::string name;
  SourceRange range;
  const ast::Node* ast = nullptr;  // TypeAssignment*, ValueAssignment*, or Module*
  ir::TypeId type_id = ir::kInvalidType;
  ir::ValueId value_id = ir::kInvalidValue;
  bool is_import = false;
  std::string import_from_module;
};

/// Maps (module, name) -> Symbol for types and values.
class SymbolTable {
 public:
  using Key = std::string;  // "Module::Name"

  /**
   *  Function    : make_key
   *  Description : Computes make key from (module, name).
   *  Parameters  : module — const std::string& module; name — const std::string& name
   *  Returns     : static Key
   */
  static Key make_key(const std::string& module, const std::string& name);

  /**
   *  Function    : declare
   *  Description : Returns a boolean result from symbol, diagnostics.
   *  Parameters  : symbol — Symbol symbol; diagnostics — Diagnostics& diagnostics
   *  Returns     : bool
   */
  bool declare(Symbol symbol, Diagnostics& diagnostics);
  /**
   *  Function    : find
   *  Description : Computes find from (module, name).
   *  Parameters  : module — const std::string& module; name — const std::string& name
   *  Returns     : Symbol*
   */
  Symbol* find(const std::string& module, const std::string& name);
  /**
   *  Function    : find
   *  Description : Computes find from (module, name).
   *  Parameters  : module — const std::string& module; name — const std::string& name
   *  Returns     : const Symbol*
   */
  const Symbol* find(const std::string& module, const std::string& name) const;

  /// Search local module first, then imports registered under that module scope.
  /**
   *  Function    : lookup
   *  Description : Computes lookup from (current_module, name).
   *  Parameters  : current_module — const std::string& current_module; name — const std::string& name
   *  Returns     : Symbol*
   */
  Symbol* lookup(const std::string& current_module, const std::string& name);
  /**
   *  Function    : lookup
   *  Description : Computes lookup from (current_module, name).
   *  Parameters  : current_module — const std::string& current_module; name — const std::string& name
   *  Returns     : const Symbol*
   */
  const Symbol* lookup(const std::string& current_module, const std::string& name) const;

  /**
   *  Function    : entries
   *  Description : Computes entries from (none).
   *  Parameters  : none
   *  Returns     : const std::unordered_map<Key, Symbol>&
   */
  const std::unordered_map<Key, Symbol>& entries() const noexcept { return entries_; }

 private:
  std::unordered_map<Key, Symbol> entries_;
};

}  // namespace asn1
