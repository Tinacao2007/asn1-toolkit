#pragma once

#include <asn1/ast/module.hpp>

#include <ostream>
#include <string>

namespace asn1 {
namespace ast {

/// Pretty-print an AST module for debugging (--dump-ast).
void print(std::ostream& out, const Module& module);
std::string to_string(const Module& module);

}  // namespace ast
}  // namespace asn1
