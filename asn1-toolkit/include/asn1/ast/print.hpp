/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ast/print.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Declares AST pretty-printer entry points used by asn1cxx and unit tests.
**
** Specification: ITU-T X.680 — ASN.1 abstract syntax (parse tree
**                 produced/consumed here).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/ast/module.hpp>

#include <ostream>
#include <string>

namespace asn1 {
namespace ast {

/// Pretty-print an AST module for debugging (--dump-ast).
/**
 *  Function    : print
 *  Description : Performs print (declaration).
 *  Parameters  : out — std::ostream& out; module — const Module& module
 *  Returns     : void
 */
void print(std::ostream& out, const Module& module);
/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : module — const Module& module
 *  Returns     : std::string
 */
std::string to_string(const Module& module);

}  // namespace ast
}  // namespace asn1
