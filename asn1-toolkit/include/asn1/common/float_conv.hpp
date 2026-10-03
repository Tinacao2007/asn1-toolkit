#pragma once

#include <charconv>
#include <cstddef>
#include <locale>
#include <sstream>
#include <string_view>

namespace asn1 {

/// Locale-independent, strict floating-point parser (period decimal point, no trailing garbage).
inline bool parse_double_c_locale(const char* first, const char* last, double& out) {
  if (first >= last) {
    return false;
  }
#if defined(_MSC_VER) || (defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)
  auto res = std::from_chars(first, last, out);
  return res.ec == std::errc{} && res.ptr == last;
#else
  std::string s(first, last);
  std::istringstream iss(s);
  iss.imbue(std::locale::classic());
  iss >> out;
  return !iss.fail() && iss.eof();
#endif
}

inline bool parse_double_c_locale(std::string_view sv, double& out) {
  return parse_double_c_locale(sv.data(), sv.data() + sv.size(), out);
}

}  // namespace asn1
