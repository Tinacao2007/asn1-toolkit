#include <asn1/semantic/symbol_table.hpp>

namespace asn1 {

SymbolTable::Key SymbolTable::make_key(const std::string& module, const std::string& name) {
  return module + "::" + name;
}

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

Symbol* SymbolTable::find(const std::string& module, const std::string& name) {
  auto it = entries_.find(make_key(module, name));
  return it == entries_.end() ? nullptr : &it->second;
}

const Symbol* SymbolTable::find(const std::string& module, const std::string& name) const {
  auto it = entries_.find(make_key(module, name));
  return it == entries_.end() ? nullptr : &it->second;
}

Symbol* SymbolTable::lookup(const std::string& current_module, const std::string& name) {
  return find(current_module, name);
}

const Symbol* SymbolTable::lookup(const std::string& current_module,
                                  const std::string& name) const {
  return find(current_module, name);
}

}  // namespace asn1
