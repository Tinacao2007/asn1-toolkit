/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/standard_second_tu.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   GoogleTest exercising standard second tu behaviour.
**
** Specification: Validates toolchain output against internal IR/codegen
**                 contracts (X.680 type model).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "generated.hpp"
#ifdef STANDARD_SPLIT
namespace gen = asn1_gen::Standards;
#else
namespace gen = asn1_gen;
#endif
std::vector<std::uint8_t> generated_second_tu() {
  gen::Small value{}; value.value = 3;
  asn1::BitWriter writer;
  if (!gen::encode_uper(writer,value)) return {};
  return writer.buffer();
}
