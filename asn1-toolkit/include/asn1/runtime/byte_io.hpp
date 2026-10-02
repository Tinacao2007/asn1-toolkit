#pragma once

#include <asn1/common/result.hpp>
#include <asn1/common/span.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace asn1 {

class ByteWriter {
 public:
  void put(std::uint8_t b);
  void write(Span<const std::uint8_t> bytes);
  void write(const std::uint8_t* data, std::size_t n);

  std::vector<std::uint8_t>& buffer() noexcept { return buf_; }
  const std::vector<std::uint8_t>& buffer() const noexcept { return buf_; }
  std::vector<std::uint8_t> take();
  void clear() { buf_.clear(); }
  std::size_t size() const noexcept { return buf_.size(); }

 private:
  std::vector<std::uint8_t> buf_;
};

class ByteReader {
 public:
  explicit ByteReader(Span<const std::uint8_t> data);

  Result<std::uint8_t> get();
  Result<Span<const std::uint8_t>> read(std::size_t n);
  Result<void> expect(std::uint8_t b);

  std::size_t offset() const noexcept { return pos_; }
  std::size_t remaining() const noexcept;
  bool eof() const noexcept { return remaining() == 0; }

  /// Peek without consuming. Fails if not enough bytes.
  Result<std::uint8_t> peek(std::size_t ahead = 0) const;

 private:
  Span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
};

inline Error make_error(Error::Code code, std::size_t offset, std::string message) {
  Error e;
  e.code = code;
  e.offset = offset;
  e.message = std::move(message);
  return e;
}

}  // namespace asn1
