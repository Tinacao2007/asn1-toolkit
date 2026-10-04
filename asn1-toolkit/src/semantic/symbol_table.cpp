/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/semantic/symbol_table.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Symbol table insert, lookup, and import wiring.
**
** Specification: ITU-T X.680 — name and module semantics;
**                 ITU-T X.681 / X.683 — object and parameterization
**                 semantics where implemented.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/semantic/symbol_table.hpp>

namespace asn1 {

/**
 *  Function    : make_key
 *  Description : Computes make key from (module, name).
 *  Parameters  : module — const std::string& module; name — const std::string& name
 *  Returns     : SymbolTable::Key SymbolTable::
 */
SymbolTable::Key SymbolTable::make_key(const std::string& module, const std::string& name) {
  return module + "::" + name;
}

/**
 *  Function    : declare
 *  Description : Returns a boolean result from symbol, diagnostics.
 *  Parameters  : symbol — Symbol symbol; diagnostics — Diagnostics& diagnostics
 *  Returns     : bool SymbolTable::
 */
bool SymbolTable::declare(Symbol symbol, Diagnostics& diagnostics) {
  const Key key = make_key(symbol.module, symbol.name);
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    diagnostics.error(symbol.range, "duplicate symbol '" + symbol.name + "' in module '" +
                                        symbol.module + "'");
    diagnostics.note(it->second.range, "previous declaration is here");
    return false;
  }
  entries_.emplace(key, std::move(symbol));
  return true;
}

/**
 *  Function    : find
 *  Description : Computes find from (module, name).
 *  Parameters  : module — const std::string& module; name — const std::string& name
 *  Returns     : Symbol* SymbolTable::
 */
Symbol* SymbolTable::find(const std::string& module, const std::string& name) {
  auto it = entries_.find(make_key(module, name));
  return it == entries_.end() ? nullptr : &it->second;
}

/**
 *  Function    : find
 *  Description : Computes find from (module, name).
 *  Parameters  : module — const std::string& module; name — const std::string& name
 *  Returns     : const Symbol* SymbolTable::
 */
const Symbol* SymbolTable::find(const std::string& module, const std::string& name) const {
  auto it = entries_.find(make_key(module, name));
  return it == entries_.end() ? nullptr : &it->second;
}

/**
 *  Function    : lookup
 *  Description : Computes lookup from (current_module, name).
 *  Parameters  : current_module — const std::string& current_module; name — const std::string& name
 *  Returns     : Symbol* SymbolTable::
 */
Symbol* SymbolTable::lookup(const std::string& current_module, const std::string& name) {
  return find(current_module, name);
}

const Symbol* SymbolTable::lookup(const std::string& current_module,
                                  const std::string& name) const {
  return find(current_module, name);
}

}  // namespace asn1
