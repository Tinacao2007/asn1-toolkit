/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/support/source_location.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   File/line/column tuple attached to AST and diagnostics.
**
** Specification: No external protocol; C++ infrastructure shared by
**                 compiler and runtime.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <string>

namespace asn1 {

/// 1-based line and column, matching typical compiler diagnostics.
struct SourceLocation {
  std::string file;
  int line = 1;
  int column = 1;
};

struct SourceRange {
  SourceLocation begin;
  SourceLocation end;
};

}  // namespace asn1
