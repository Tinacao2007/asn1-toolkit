/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/runtime/bit_string.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   BitStringValue storage (bits + bit length) for codecs.
**
** Specification: Internal I/O and safety limits serving ITU encoding
**                 implementations in this codebase.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once
#include <cstdint>
#include <vector>
namespace asn1 {
struct BitStringValue {
  std::vector<std::uint8_t> bits;
  std::size_t bit_length = 0;
};
}
