/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/runtime/byte_io.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Byte-oriented I/O helpers.
**
** Specification: Internal I/O and safety limits serving ITU encoding
**                 implementations in this codebase.
** Design Spec:   asn1-toolkit/docs/ARCHITECTURE.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/runtime/byte_io.hpp>

namespace asn1 {

/**
 *  Function    : push_back
 *  Description : Performs push back (definition).
 *  Parameters  : buf_.push_back(b — std::uint8_t b) { buf_.push_back(b
 *  Returns     : void ByteWriter::put(std::uint8_t b) { buf_.
 */
void ByteWriter::put(std::uint8_t b) { buf_.push_back(b); }

/**
 *  Function    : write
 *  Description : Performs write (definition).
 *  Parameters  : bytes — Span<const std::uint8_t> bytes
 *  Returns     : void ByteWriter::
 */
void ByteWriter::write(Span<const std::uint8_t> bytes) {
  write(bytes.data(), bytes.size());
}

/**
 *  Function    : write
 *  Description : Performs write (definition).
 *  Parameters  : data — const std::uint8_t* data; n — std::size_t n
 *  Returns     : void ByteWriter::
 */
void ByteWriter::write(const std::uint8_t* data, std::size_t n) {
  if (n == 0) {
    return;
  }
  buf_.insert(buf_.end(), data, data + n);
}

/**
 *  Function    : take
 *  Description : Computes take from (none).
 *  Parameters  : none
 *  Returns     : std::vector<std::uint8_t> ByteWriter::
 */
std::vector<std::uint8_t> ByteWriter::take() {
  std::vector<std::uint8_t> out;
  out.swap(buf_);
  return out;
}

ByteReader::ByteReader(Span<const std::uint8_t> data) : data_(data) {}

/**
 *  Function    : remaining
 *  Description : Computes remaining from (none).
 *  Parameters  : none
 *  Returns     : std::size_t ByteReader::
 */
std::size_t ByteReader::remaining() const noexcept {
  return pos_ < data_.size() ? data_.size() - pos_ : 0;
}

Span<const std::uint8_t> ByteReader::remaining_span() const noexcept {
  return Span<const std::uint8_t>(data_.data() + pos_, remaining());
}

/**
 *  Function    : get
 *  Description : Returns success or an error from get.
 *  Parameters  : none
 *  Returns     : Result<std::uint8_t> ByteReader::
 */
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

/**
 *  Function    : read
 *  Description : Returns success or an error from read.
 *  Parameters  : n — std::size_t n
 *  Returns     : Result<Span<const std::uint8_t>> ByteReader::
 */
Result<Span<const std::uint8_t>> ByteReader::read(std::size_t n) {
  if (remaining() < n) {
    return make_error(Error::Code::Truncated, pos_, "unexpected end of input");
  }
  Span<const std::uint8_t> view(data_.data() + pos_, n);
  pos_ += n;
  return view;
}

/**
 *  Function    : expect
 *  Description : Returns success or an error from expect.
 *  Parameters  : b — std::uint8_t b
 *  Returns     : Result<void> ByteReader::
 */
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
