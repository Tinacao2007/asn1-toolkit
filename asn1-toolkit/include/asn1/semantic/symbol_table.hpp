#pragma once

#include <asn1/ast/module.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_location.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace asn1 {

enum class SymbolKind { Type, Value, Module };

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

  static Key make_key(const std::string& module, const std::string& name);

  bool declare(Symbol symbol, Diagnostics& diagnostics);
  Symbol* find(const std::string& module, const std::string& name);
  const Symbol* find(const std::string& module, const std::string& name) const;

  /// Search local module first, then imports registered under that module scope.
  Symbol* lookup(const std::string& current_module, const std::string& name);
  const Symbol* lookup(const std::string& current_module, const std::string& name) const;

  const std::unordered_map<Key, Symbol>& entries() const noexcept { return entries_; }

 private:
  std::unordered_map<Key, Symbol> entries_;
};

}  // namespace asn1
