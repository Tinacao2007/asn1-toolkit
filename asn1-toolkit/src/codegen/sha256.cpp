/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/sha256.cpp
**   $Version: 0.1
**   $Date:   2026-10-16
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Self-contained SHA-256 implementation per FIPS 180-4. Compact (~150
**   lines) and used only by the emit content-hash cache; not installed
**   as a public utility.
**
** Specification: FIPS 180-4 (Secure Hash Standard).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/codegen/sha256.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace asn1 {
namespace codegen {

namespace {

constexpr std::uint32_t kRoundConstants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
  return (x >> n) | (x << (32u - n));
}

class Sha256 {
 public:
  void update(std::string_view data) {
    for (unsigned char c : data) {
      buffer_[buffer_len_++] = c;
      if (buffer_len_ == 64) {
        process_block(buffer_.data());
        bit_count_ += 512;
        buffer_len_ = 0;
      }
    }
  }

  std::array<std::uint8_t, 32> finalize() {
    // Total bit length = bit_count_ + buffer_len_*8.
    bit_count_ += static_cast<std::uint64_t>(buffer_len_) * 8u;
    // Append 0x80, pad with 0x00 until length ≡ 56 mod 64.
    buffer_[buffer_len_++] = 0x80;
    if (buffer_len_ > 56) {
      while (buffer_len_ < 64) {
        buffer_[buffer_len_++] = 0x00;
      }
      process_block(buffer_.data());
      buffer_len_ = 0;
    }
    while (buffer_len_ < 56) {
      buffer_[buffer_len_++] = 0x00;
    }
    // 64-bit big-endian bit length.
    for (int i = 7; i >= 0; --i) {
      buffer_[buffer_len_++] = static_cast<unsigned char>((bit_count_ >> (i * 8)) & 0xffu);
    }
    process_block(buffer_.data());

    std::array<std::uint8_t, 32> out{};
    for (std::uint32_t i = 0; i < 8; ++i) {
      out[i * 4 + 0] = static_cast<unsigned char>((state_[i] >> 24) & 0xffu);
      out[i * 4 + 1] = static_cast<unsigned char>((state_[i] >> 16) & 0xffu);
      out[i * 4 + 2] = static_cast<unsigned char>((state_[i] >> 8) & 0xffu);
      out[i * 4 + 3] = static_cast<unsigned char>(state_[i] & 0xffu);
    }
    return out;
  }

 private:
  void process_block(const unsigned char* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(block[i * 4 + 0]) << 24) |
             (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
             (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
             (static_cast<std::uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const std::uint32_t ch = (e & f) ^ ((~e) & g);
      const std::uint32_t t1 = h + S1 + ch + kRoundConstants[i] + w[i];
      const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const std::uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = S0 + mj;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::uint32_t state_[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                             0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  std::array<unsigned char, 64> buffer_{};
  unsigned buffer_len_ = 0;
  std::uint64_t bit_count_ = 0;
};

inline std::string to_hex(const std::array<std::uint8_t, 32>& bytes) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.resize(64);
  for (std::size_t i = 0; i < 32; ++i) {
    out[i * 2] = hex[(bytes[i] >> 4) & 0x0fu];
    out[i * 2 + 1] = hex[bytes[i] & 0x0fu];
  }
  return out;
}

}  // namespace

std::string sha256_hex(std::string_view bytes) {
  Sha256 hasher;
  hasher.update(bytes);
  return to_hex(hasher.finalize());
}

std::string sha256_file_hex(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return {};
  }
  Sha256 hasher;
  std::vector<char> chunk(64 * 1024);
  while (in) {
    in.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const std::streamsize n = in.gcount();
    if (n <= 0) {
      break;
    }
    hasher.update(std::string_view(chunk.data(), static_cast<std::size_t>(n)));
  }
  return to_hex(hasher.finalize());
}

}  // namespace codegen
}  // namespace asn1
