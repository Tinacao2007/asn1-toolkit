#include <asn1/runtime/byte_io.hpp>

namespace asn1 {

void ByteWriter::put(std::uint8_t b) { buf_.push_back(b); }

void ByteWriter::write(Span<const std::uint8_t> bytes) {
  write(bytes.data(), bytes.size());
}

void ByteWriter::write(const std::uint8_t* data, std::size_t n) {
  if (n == 0) {
    return;
  }
  buf_.insert(buf_.end(), data, data + n);
}

std::vector<std::uint8_t> ByteWriter::take() {
  std::vector<std::uint8_t> out;
  out.swap(buf_);
  return out;
}

ByteReader::ByteReader(Span<const std::uint8_t> data) : data_(data) {}

std::size_t ByteReader::remaining() const noexcept {
  return pos_ < data_.size() ? data_.size() - pos_ : 0;
}

Result<std::uint8_t> ByteReader::get() {
  if (pos_ >= data_.size()) {
    return make_error(Error::Code::Truncated, pos_, "unexpected end of input");
  }
  return data_[pos_++];
}

Result<std::uint8_t> ByteReader::peek(std::size_t ahead) const {
  if (pos_ + ahead >= data_.size()) {
    return make_error(Error::Code::Truncated, pos_, "unexpected end of input");
  }
  return data_[pos_ + ahead];
}

Result<Span<const std::uint8_t>> ByteReader::read(std::size_t n) {
  if (remaining() < n) {
    return make_error(Error::Code::Truncated, pos_, "unexpected end of input");
  }
  Span<const std::uint8_t> view(data_.data() + pos_, n);
  pos_ += n;
  return view;
}

Result<void> ByteReader::expect(std::uint8_t b) {
  auto got = get();
  if (!got) {
    return got.error();
  }
  if (got.value() != b) {
    return make_error(Error::Code::InvalidArgument, pos_ - 1,
                      "unexpected byte value");
  }
  return Result<void>::success();
}

}  // namespace asn1
