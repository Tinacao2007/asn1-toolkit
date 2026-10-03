#pragma once
#include <cstdint>
#include <vector>
namespace asn1 {
struct BitStringValue {
  std::vector<std::uint8_t> bits;
  std::size_t bit_length = 0;
};
}
