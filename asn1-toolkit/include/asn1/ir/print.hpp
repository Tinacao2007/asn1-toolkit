#pragma once

#include <asn1/ir/type.hpp>

#include <ostream>
#include <string>

namespace asn1 {
namespace ir {

void print(std::ostream& out, const Model& model);
std::string to_string(const Model& model);

}  // namespace ir
}  // namespace asn1
