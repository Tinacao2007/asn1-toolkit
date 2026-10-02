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
  explicit Analyzer(Diagnostics& diagnostics);

  ir::Model analyze(std::vector<std::unique_ptr<ast::Module>> modules);

  const SymbolTable& symbols() const noexcept { return symbols_; }

 private:
  void declare_pass();
  void resolve_pass();
  void tag_pass();
  void lower_pass();

  Diagnostics& diagnostics_;
  SymbolTable symbols_;
  std::vector<std::unique_ptr<ast::Module>> modules_;
  ir::Model model_;
};

}  // namespace asn1
