#pragma once

#include <asn1/common/result.hpp>
#include <asn1/common/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace asn1 {

/// MSB-first bit stream writer. Final incomplete octet is zero-padded on take().
class BitWriter {
 public:
  void put_bit(bool bit);
  /// Write the low `nbits` of `value` as an MSB-first field (nbits <= 64).
  void put_bits(std::uint64_t value, std::size_t nbits);
  /// Write `nbits` from `bytes` (MSB of bytes[0] first).
  void put_bits(Span<const std::uint8_t> bytes, std::size_t nbits);
  void put_octet(std::uint8_t octet);
  void put_octets(Span<const std::uint8_t> bytes);

  /// Pad with zero bits to the next octet boundary (no-op if already aligned).
  void align_to_octet();

  std::size_t bit_size() const noexcept { return bit_count_; }
  std::size_t byte_size() const noexcept { return (bit_count_ + 7) / 8; }
  bool octet_aligned() const noexcept { return (bit_count_ % 8) == 0; }

  /// Returns bytes with trailing zero-bit padding in the last octet if needed.
  std::vector<std::uint8_t> take();
  const std::vector<std::uint8_t>& buffer() const noexcept { return buf_; }
  void clear();

 private:
  void ensure_byte();

  std::vector<std::uint8_t> buf_;
  std::size_t bit_count_ = 0;  // total bits written
};

/// MSB-first bit stream reader over a byte buffer.
class BitReader {
 public:
  explicit BitReader(Span<const std::uint8_t> data);

  Result<bool> get_bit();
  Result<std::uint64_t> get_bits(std::size_t nbits);
  Result<std::vector<std::uint8_t>> get_bits_as_bytes(std::size_t nbits);
  Result<std::uint8_t> get_octet();
  Result<std::vector<std::uint8_t>> get_octets(std::size_t n);

  /// Skip zero padding to next octet boundary. Does not require padding bits to be zero.
  Result<void> align_to_octet();

  std::size_t bit_offset() const noexcept { return bit_pos_; }
  std::size_t bit_remaining() const noexcept;
  bool octet_aligned() const noexcept { return (bit_pos_ % 8) == 0; }
  bool eof() const noexcept { return bit_remaining() == 0; }

 private:
  Span<const std::uint8_t> data_;
  std::size_t bit_pos_ = 0;
};

}  // namespace asn1
