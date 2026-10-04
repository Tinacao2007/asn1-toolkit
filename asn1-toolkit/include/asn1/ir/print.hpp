/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/ir/print.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Declares type IR debug printer API.
**
** Specification: Internal type IR (lowering target for X.680/X.681
**                 constructs; not a wire standard).
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <asn1/ir/type.hpp>

#include <ostream>
#include <string>

namespace asn1 {
namespace ir {

/**
 *  Function    : print
 *  Description : Performs print (declaration).
 *  Parameters  : out — std::ostream& out; model — const Model& model
 *  Returns     : void
 */
void print(std::ostream& out, const Model& model);
/**
 *  Function    : to_string
 *  Description : Builds and returns a string for to string.
 *  Parameters  : model — const Model& model
 *  Returns     : std::string
 */
std::string to_string(const Model& model);

}  // namespace ir
}  // namespace asn1
