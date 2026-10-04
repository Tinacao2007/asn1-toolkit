/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/include/asn1/common/float_conv.hpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Helpers converting ASN.1 REAL (binary base-2) and IEEE-754 double.
**
** Specification: No external protocol; C++ infrastructure shared by
**                 compiler and runtime.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#pragma once

#include <charconv>
#include <cstddef>
#include <locale>
#include <sstream>
#include <string_view>

namespace asn1 {

/// Locale-independent, strict floating-point parser (period decimal point, no trailing garbage).
/**
 *  Function    : parse_double_c_locale
 *  Description : Returns a boolean result from first, last, out.
 *  Parameters  : first — const char* first; last — const char* last; out — double& out
 *  Returns     : inline bool
 */
inline bool parse_double_c_locale(const char* first, const char* last, double& out) {
  if (first >= last) {
    return false;
  }
#if defined(_MSC_VER) || (defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)
  /**
   *  Function    : from_chars
   *  Description : Computes from chars from (first, last, out).
   *  Parameters  : first — first; last — last; out — out
   *  Returns     : auto res = std::
   */
  auto res = std::from_chars(first, last, out);
  return res.ec == std::errc{} && res.ptr == last;
#else
  /**
   *  Function    : s
   *  Description : Builds and returns a string for s.
   *  Parameters  : first — first; last — last
   *  Returns     : std::string
   */
  std::string s(first, last);
  /**
   *  Function    : iss
   *  Description : Computes iss from (s).
   *  Parameters  : s — s
   *  Returns     : std::istringstream
   */
  std::istringstream iss(s);
  /**
   *  Function    : classic
   *  Description : Computes classic from (std::locale::classic()).
   *  Parameters  : std::locale::classic() — std::locale::classic()
   *  Returns     : iss.imbue(std::locale::
   */
  iss.imbue(std::locale::classic());
  iss >> out;
  /**
   *  Function    : eof
   *  Description : Computes eof from (iss.eof().
   *  Parameters  : iss.eof( — ) && iss.eof(
   *  Returns     : return !iss.fail() && iss.
   */
  return !iss.fail() && iss.eof();
#endif
}

/**
 *  Function    : parse_double_c_locale
 *  Description : Returns a boolean result from sv, out.
 *  Parameters  : sv — std::string_view sv; out — double& out
 *  Returns     : inline bool
 */
inline bool parse_double_c_locale(std::string_view sv, double& out) {
  /**
   *  Function    : size
   *  Description : Computes size from (sv.data(), sv.size(), out).
   *  Parameters  : sv.data() — sv.data(); sv.size() — sv.data() + sv.size(); out — out
   *  Returns     : return parse_double_c_locale(sv.data(), sv.data() + sv.
   */
  return parse_double_c_locale(sv.data(), sv.data() + sv.size(), out);
}

}  // namespace asn1
