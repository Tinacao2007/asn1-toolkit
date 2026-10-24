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
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace asn1 {
struct BitStringValue {
  std::vector<std::uint8_t> bits;
  std::size_t bit_length = 0;
};
// Equality concerns significant bits, independently of spare storage/padding.
inline bool operator==(const BitStringValue& a, const BitStringValue& b) {
  if(a.bit_length!=b.bit_length) return false;
  const auto whole=a.bit_length/8, tail=a.bit_length%8;
  const auto count=whole+(tail!=0);
  if(a.bits.size()<count || b.bits.size()<count) return false;
  for(std::size_t i=0;i<whole;++i) if(a.bits[i]!=b.bits[i]) return false;
  return tail==0 || ((a.bits[whole]^b.bits[whole]) & (0xffu<<(8-tail)))==0;
}
inline bool operator!=(const BitStringValue& a, const BitStringValue& b) { return !(a==b); }
}
