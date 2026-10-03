#include <asn1/runtime/bit_io.hpp>
#include <asn1/runtime/byte_io.hpp>
#include <asn1/runtime/limits.hpp>

namespace asn1 {

void BitWriter::ensure_byte() {
  // Need room for the bit about to be written at bit_count_.
  const std::size_t need = (bit_count_ / 8) + 1;
  if (buf_.size() < need) {
    buf_.resize(need, 0);
  }
}

void BitWriter::put_bit(bool bit) {
  ensure_byte();
  const std::size_t byte_index = bit_count_ / 8;
  const std::size_t bit_index = 7 - (bit_count_ % 8);
  if (bit) {
    buf_[byte_index] =
        static_cast<std::uint8_t>(buf_[byte_index] | (1u << bit_index));
  }
  ++bit_count_;
}

void BitWriter::put_bits(std::uint64_t value, std::size_t nbits) {
  if (nbits > 64) {
    nbits = 64;
  }
  for (std::size_t i = 0; i < nbits; ++i) {
    const std::size_t shift = nbits - 1 - i;
    put_bit(((value >> shift) & 1u) != 0);
  }
}

void BitWriter::put_bits(Span<const std::uint8_t> bytes, std::size_t nbits) {
  std::size_t written = 0;
  for (std::size_t i = 0; i < bytes.size() && written < nbits; ++i) {
    for (int b = 7; b >= 0 && written < nbits; --b) {
      put_bit(((bytes[i] >> b) & 1u) != 0);
      ++written;
    }
  }
}

void BitWriter::put_octet(std::uint8_t octet) { put_bits(octet, 8); }

void BitWriter::put_octets(Span<const std::uint8_t> bytes) {
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    put_octet(bytes[i]);
  }
}

void BitWriter::align_to_octet() {
  const std::size_t rem = bit_count_ % 8;
  if (rem == 0) {
    return;
  }
  put_bits(0, 8 - rem);
}

std::vector<std::uint8_t> BitWriter::take() {
  align_to_octet();
  // After align, bit_count_ is multiple of 8; drop extra empty growth.
  buf_.resize(bit_count_ / 8);
  std::vector<std::uint8_t> out;
  out.swap(buf_);
  bit_count_ = 0;
  return out;
}

void BitWriter::clear() {
  buf_.clear();
  bit_count_ = 0;
}

BitReader::BitReader(Span<const std::uint8_t> data) : data_(data) {}

std::size_t BitReader::bit_remaining() const noexcept {
  const std::size_t total = data_.size() * 8;
  return bit_pos_ < total ? total - bit_pos_ : 0;
}

Result<bool> BitReader::get_bit() {
  if (bit_remaining() == 0) {
    return make_error(Error::Code::Truncated, bit_pos_,
                      "unexpected end of bit input");
  }
  const std::size_t byte_index = bit_pos_ / 8;
  const std::size_t bit_index = 7 - (bit_pos_ % 8);
  const bool bit = ((data_[byte_index] >> bit_index) & 1u) != 0;
  ++bit_pos_;
  return bit;
}

Result<std::uint64_t> BitReader::get_bits(std::size_t nbits) {
  if (nbits > 64) {
    return make_error(Error::Code::InvalidArgument, bit_pos_,
                      "get_bits nbits must be <= 64");
  }
  if (bit_remaining() < nbits) {
    return make_error(Error::Code::Truncated, bit_pos_,
                      "unexpected end of bit input");
  }
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < nbits; ++i) {
    auto b = get_bit();
    if (!b) {
      return b.error();
    }
    value = (value << 1) | (b.value() ? 1u : 0u);
  }
  return value;
}

Result<std::vector<std::uint8_t>> BitReader::get_bits_as_bytes(std::size_t nbits) {
  if (bit_remaining() < nbits) {
    return make_error(Error::Code::Truncated, bit_pos_,
                      "unexpected end of bit input");
  }
  std::vector<std::uint8_t> out((nbits + 7) / 8, 0);
  for (std::size_t i = 0; i < nbits; ++i) {
    auto b = get_bit();
    if (!b) {
      return b.error();
    }
    if (b.value()) {
      const std::size_t byte_index = i / 8;
      const std::size_t bit_index = 7 - (i % 8);
      out[byte_index] =
          static_cast<std::uint8_t>(out[byte_index] | (1u << bit_index));
    }
  }
  return out;
}

Result<std::uint8_t> BitReader::get_octet() {
  auto v = get_bits(8);
  if (!v) {
    return v.error();
  }
  return static_cast<std::uint8_t>(v.value());
}

Result<std::vector<std::uint8_t>> BitReader::get_octets(std::size_t n) {
  if (n > kMaxCodecBytes) {
    return make_error(Error::Code::LengthOverflow, bit_offset(),
                      "octet string length exceeds codec limit");
  }
  std::vector<std::uint8_t> out;
  out.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    auto b = get_octet();
    if (!b) {
      return b.error();
    }
    out.push_back(b.value());
  }
  return out;
}

Result<void> BitReader::align_to_octet() {
  const std::size_t rem = bit_pos_ % 8;
  if (rem == 0) {
    return Result<void>::success();
  }
  auto skip = get_bits(8 - rem);
  if (!skip) {
    return skip.error();
  }
  return Result<void>::success();
}

}  // namespace asn1
