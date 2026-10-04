/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ast/node.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Common AST node bases, source locations, and forward declarations.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (parse tree
**                 produced/consumed here).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/support/source_location.hpp>

#include <memory>

namespace asn1 {
namespace ast {

class Visitor;

class Node {
 public:
  /**
   *  Function    : move
   *  Description : Computes move from (range_(std::move(range)).
   *  Parameters  : range_(std::move(range) — SourceRange range) : range_(std::move(range)
   *  Returns     : explicit Node(SourceRange range) : range_(std::
   */
  explicit Node(SourceRange range) : range_(std::move(range)) {}
  /**
   *  Function    : ~Node
   *  Description : Destroys the Node instance.
   *  Parameters  : none
   *  Returns     : virtual
   */
  virtual ~Node() = default;

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  /**
   *  Function    : range
   *  Description : Computes range from (none).
   *  Parameters  : none
   *  Returns     : const SourceRange&
   */
  const SourceRange& range() const noexcept { return range_; }
  /**
   *  Function    : move
   *  Description : Performs move (definition).
   *  Parameters  : range_ — SourceRange range) { range_
   *  Returns     : void set_range(SourceRange range) { range_ = std::
   */
  void set_range(SourceRange range) { range_ = std::move(range); }

  /**
   *  Function    : accept
   *  Description : Computes accept from (v).
   *  Parameters  : v — Visitor& v
   *  Returns     : virtual void
   */
  virtual void accept(Visitor& v) const = 0;

 private:
  SourceRange range_;
};

template <typename T>
using NodePtr = std::unique_ptr<T>;

}  // namespace ast
}  // namespace asn1
