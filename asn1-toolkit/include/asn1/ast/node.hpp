#pragma once

#include <asn1/support/source_location.hpp>

#include <memory>

namespace asn1 {
namespace ast {

class Visitor;

class Node {
 public:
  explicit Node(SourceRange range) : range_(std::move(range)) {}
  virtual ~Node() = default;

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  const SourceRange& range() const noexcept { return range_; }
  void set_range(SourceRange range) { range_ = std::move(range); }

  virtual void accept(Visitor& v) const = 0;

 private:
  SourceRange range_;
};

template <typename T>
using NodePtr = std::unique_ptr<T>;

}  // namespace ast
}  // namespace asn1
